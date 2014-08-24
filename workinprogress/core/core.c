#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>


typedef struct Word 
{
	gint 	occurence;
	gchar  *w;
}Word; 

static gint word_list_inspect(gconstpointer data, gconstpointer user_data)
{
	gchar *c1 = ((Word *)data)->w;
	gchar *c2 = ((Word *)user_data)->w;
	
	if(c1 != NULL && c2 != NULL)
		return g_strcmp0(c1, c2);
	
	return -1;
}

static gint get_num_line(FILE *fp)
{
	gchar *line 		= NULL;
	size_t len 			= 0;
	gchar **keycommand 	= NULL;
	int nbr				= 0;
	
	while(getline(&line,&len, fp ) != -1)
	{
		keycommand = g_strsplit_set(line, "=\n", -1);
		
		if(keycommand == NULL) break;
		
		if(keycommand[0] != NULL && keycommand[1] != NULL) nbr++;
		g_strfreev(keycommand);
	}	
	
	//rembobiner
	fseek(fp, 0, SEEK_SET);
	
	return nbr;
}

static void dictionnary_table_print(gpointer key, gpointer data, gpointer user_data)
{
	gchar *k 	= (gchar *)key;
	GList *list = (GList *)data;
	GList *iter = NULL;
	 
	g_print("\n\ncommand: %s\n",k); 
	for(iter = list;iter;iter = iter->next)
	{
		Word *word = (Word *)iter->data;
		g_print("%s %d\n", word->w, word->occurence);
	}
}

void dictionnary_display(Dictionnary *dico)
{
	//show dictionnary
	GList *keys 	= g_hash_table_get_keys(dico->projections);
	GList *values 	= g_hash_table_get_values(dico->projections);
	gint i, j;
	
	for(i=0; i<g_list_length(keys);i++)
	{
		gchar *key = g_list_nth_data(keys, i);
		gdouble *tab = g_list_nth_data(values, i);
		
		g_print("[%s]\n", key);
		for(j=0; j<g_list_length(dico->allwords); j++)
		{
			Word *ww = g_list_nth_data(dico->allwords, j);
			g_print("%s:\t%.20f\n", ww->w, tab[j]);
		}
		g_print("\n");
	}
}


void dictionnary_project(Dictionnary* dico, GHashTable* table) 
{
	gint i;
	GList *keys = NULL;
	GList *values = NULL;
	gint N = g_list_length(dico->allwords);
	g_return_if_fail(NULL != dico);
	
	//get the key and the value from the table
	keys   = g_hash_table_get_keys(table);
	values = g_hash_table_get_values(table);
	
	for( i = 0; i< g_list_length(keys); i++)
	{
		GList *iter = NULL;
		GList *list = g_list_nth_data(values, i);
		gchar *key	= g_list_nth_data(keys, i);
		gdouble ntot = 0.0f;

		//nombre de mots dans le doc		
		for(iter = list; iter; iter = iter->next)
		{
			ntot += (gdouble)((Word *)(iter->data))->occurence;
		}

		//on alloue un tableau qui contiendra les fréquences normalisées
		gdouble *tab=(gdouble *)g_malloc0(N*sizeof(gdouble));
		
		//pour chaque mot dans le dico on cherche pour chaque document si il est présent
		gint j = 0;
		for(iter = dico->allwords; iter; iter=iter->next)
		{
			Word  *w 	= (Word *)(iter->data);
			GList *res 	= g_list_find_custom(list, w, word_list_inspect);
			
			if(NULL != res)
			{
				tab[j] = (gdouble)(((Word *)res->data)->occurence ) / ntot * log((gdouble)N/(gdouble)w->occurence);
			}
			
			j++;
		}

		g_hash_table_insert(dico->projections, g_strdup(key), tab);
	}
}

static void table_destroy(gpointer key, gpointer data, gpointer user_data)
{
	g_free((gchar *)key);
	GList *list = (GList *)data; 
	GList *iter = NULL;
	
	for(iter = list; iter; iter = iter->next)
	{
		g_free(((Word *)iter->data)->w);
		g_free((Word *)iter->data);
	}
	
	g_list_free(list);
}

static void projection_destroy(gpointer key, gpointer data, gpointer user_data)
{
	g_free((gchar *)key);
	g_free((gdouble *)data);
}

Dictionnary* dictionnary_new(const gchar *path)
{
	Dictionnary *dico	= NULL;
	FILE *fp 			= NULL;
	gchar *line 		= NULL;
	size_t len 			= 0;
	gint nbr			= 0;
	gint t				= 0;
	gchar **keycommand 	= NULL;
	GList **list 		= NULL;
	GList *iter 		= NULL;
	GHashTable *table	= NULL;
	
	
	g_return_if_fail(NULL != path);

	dico 			 	= (Dictionnary *)g_malloc0(sizeof(Dictionnary));
	table 	 		 	= g_hash_table_new(g_str_hash, g_str_equal);
	dico->projections	= g_hash_table_new(g_str_hash, g_str_equal);
	dico->allwords 	 	= NULL;
		
	fp 	 				= fopen(path, "r");
	nbr  				= get_num_line(fp);
	list 				= (GList **)g_malloc0((nbr+1)*sizeof(GList *));
	nbr  				= 0;
	
	while(getline(&line, &len, fp) != -1)
	{
		int i=0,j=0;
		
		//couoer la ligne en deux pour obtenir la commande et la clé		
		keycommand = g_strsplit_set(line, "=\n", -1);
	
		if(keycommand ==  NULL) break;
	
		if ( keycommand[0] != NULL
		||   keycommand[1] != NULL)
		{
			gchar **commands = g_strsplit_set(keycommand[1], "\n", -1);
			gchar **keys	 = g_strsplit_set(keycommand[0], " \n\0", -1);
			//tant que l'on a des mots dans la clé		
			j=0;
			
			if(commands[0] != NULL)
			{
				while(keys[j] != NULL)
				{
					//si la clé n'est pas tronquée
					if(*keys[j] !='\0')
					{
						Word *word 		= (Word *)g_malloc0(sizeof(Word));
						word->w	  		= g_strdup(keys[j]);
						word->occurence = 0;
										
						//si le mot n'est pas dans la liste de mots on l'ajoute à la liste globale et là la liste //locale
						if(g_list_find_custom(dico->allwords, word, word_list_inspect) == NULL)
						{
							Word *wl = (Word *)g_malloc0(sizeof(Word));

							word->occurence = 1;							
							wl->w = g_strdup(word->w);
							wl->occurence = 1;
							
							list[nbr] 		= g_list_append(list[nbr], wl);
							dico->allwords 	= g_list_append(dico->allwords, word);
						}
						else
						{													
							//le mot a déjà été trouvé, on incrémente les listes
							GList *locale  = g_list_find_custom(list[nbr], word, word_list_inspect);
							GList *globale = g_list_find_custom(dico->allwords, word, word_list_inspect );
						
						
							if(locale != NULL)
							{
								((Word *)locale->data)->occurence++;
								
								g_free(word->w);
								g_free(word);
							}
							else
							{
								Word *wl = (Word *)g_malloc0(sizeof(Word));
								wl->w = g_strdup(word->w);
								wl->occurence = 1;

								
								list[nbr] 		= g_list_append(list[nbr], wl);
								((Word *)globale->data)->occurence++;
							}
						}
					}
					j++;
				}
				
				g_hash_table_insert(table, g_strdup(g_strstrip(commands[0])), list[nbr]);
			}
						
			if(commands != NULL) g_strfreev(commands);
			if(keys 	!= NULL) g_strfreev(keys);
		}
		
		nbr++;
	}
		
	if(line != NULL)
		g_free(line);
		
	//get normalized frequency	
	dictionnary_project(dico, table);		
	
	//Destroy the table
	g_hash_table_foreach(table, table_destroy, NULL);
	g_hash_table_destroy(table);
	return dico;
}


void dictionnary_free(Dictionnary *dico)
{
	GList *iter = NULL;
	
	for(iter = dico->allwords; iter; iter = iter->next)
	{
		g_free(((Word *)iter->data)->w);
		g_free(((Word *)iter->data));
	}
	
	g_list_free(dico->allwords);
	
	//libération de la hashtable
	if (dico->projections != NULL)
	{
		g_hash_table_foreach(dico->projections, projection_destroy, NULL);
		g_hash_table_destroy(dico->projections);
		dico->projections = NULL;
	}
}

gchar* dictionnary_process_request(Dictionnary *dico, gchar *input)
{
	g_return_val_if_fail(NULL != input, NULL);
	
	//découpe la chaine
	gchar **keys		= g_strsplit_set(input, " \n\0", -1);
	gint j 				= 0;
	gint k				= 0;
	gint i				= 0;
	GList *list 		= NULL;
	GList *iter 		= NULL;
	gint N 				= g_list_length(dico->allwords);
	gdouble *tab		= (gdouble *)g_malloc0(N*sizeof(gdouble));
	gchar   *cmd		= NULL;

	while(keys[j] != NULL)
	{
		Word* word 		= (Word *)g_malloc0(sizeof(Word));
		word->w	 		= g_strdup(keys[j]);
		word->occurence = 1;
		
		//si on a bien un mot du dico
		if( g_list_find_custom(dico->allwords, word, word_list_inspect ) != NULL)
		{
			GList* res 	= g_list_find_custom(list, word, word_list_inspect);
			
			if(res == NULL)
			{
				list 	= g_list_append(list, word);
			}
			else
			{
				((Word *)res->data)->occurence++;
			}
		}
		
		j++;
	}
	
	if(keys 	!= NULL) g_strfreev(keys);	
	
	//projection
	for(iter = dico->allwords; iter; iter = iter->next)
	{
		Word  *w 	= (Word *)(iter->data);
		GList *res 	= g_list_find_custom(list, w, word_list_inspect);
		
		if(NULL != res)
		{
			tab[k] = (gdouble)(((Word *)res->data)->occurence ) / (gdouble) j * log((gdouble)N/(gdouble)w->occurence);			
		}
		
		k++;
	}	
	

	GList	*values		= g_hash_table_get_values(dico->projections);
	GList   *commands	= g_hash_table_get_keys(dico->projections);
	//find nearest in all vectors
	gdouble mdist= 0.0f;	
	for(i = 0; i< g_list_length(values); ++i)
	{
		gdouble* t = g_list_nth_data(values, i);
		gdouble dist = 0.0f;
		
		for(j = 0; j < g_list_length(dico->allwords); ++j)
		{
			dist += pow(tab[j] - t[j],2);
		}
		
		if(i == 0 || mdist > dist)
		{
			mdist = dist;
			
			cmd = g_strdup(g_list_nth_data(commands, i));
		}
	}
	

	g_free(tab);
	return cmd;
}


Dictionnary* dictionnary_new_from_file(const gchar *file)
{
	g_return_val_if_fail(NULL != file, NULL);
	g_return_val_if_fail(FALSE != g_file_test(file, G_FILE_TEST_EXISTS ), NULL);

	GKeyFile* keyfile;
	GKeyFileFlags flags;
	GError *error = NULL;
	keyfile = g_key_file_new();
	
	g_key_file_load_from_file (keyfile, file, flags, &error );
	
	flags = G_KEY_FILE_KEEP_COMMENTS;
	gint i = 0;
	
	Dictionnary *dico	= (Dictionnary *)g_malloc0(sizeof(Dictionnary));
	dico->projections	= g_hash_table_new(g_str_hash, g_str_equal);
	dico->allwords 	 	= NULL;
	
	
	//get the key in the Dictionnary group
	gchar **words = g_key_file_get_keys (keyfile,
										"Dictionnary",
										NULL,
										NULL);
										
	gchar **projections = g_key_file_get_keys(keyfile,
										  "Projections",
										  NULL,
										  NULL);
	
	g_return_val_if_fail(NULL != words, NULL);
	g_return_val_if_fail(NULL != projections, NULL);
	
	do
	{
		Word* ww = (Word *)g_malloc0(sizeof(Word));
		ww->w = g_strdup(words[i]);
		ww->occurence = g_key_file_get_integer(keyfile, "Dictionnary", words[i], NULL);
		
		dico->allwords = g_list_append(dico->allwords, ww);
		
		i++;
	}while(words[i] != NULL);
	i=0;
	
	do
	{
		gdouble* tab = g_key_file_get_double_list(keyfile,
												  "Projections",
												  projections[i],
												  NULL,
												  NULL);
		
		g_hash_table_insert(dico->projections, g_strdup(projections[i]), tab); 
		i++;
	}while(projections[i] != NULL);
	
	g_strfreev(words);
	g_strfreev(projections);
	
	g_key_file_free(keyfile);
	return dico;
}


void dictionnary_to_file(Dictionnary *dico, gchar *dicFile)
{
	g_return_if_fail(NULL != dico);
	g_return_if_fail(NULL != dicFile);
	
	//create a g_key_file to store the dictionnary
	GKeyFile* keyfile;
    GError *error = NULL;
    gsize size;
    keyfile = g_key_file_new();


	
	//write all words of the dictionnary to the file in a string_list
	gint words_size = g_list_length(dico->allwords);
	gint i;

	GList *commands	= NULL;
	GList *values 	= NULL;
	
	for(i = 0; i < words_size; i++) 
	{
		Word *w1 = (Word *)g_list_nth_data(dico->allwords, i);
	 
		g_key_file_set_integer(keyfile, "Dictionnary", w1->w, w1->occurence);
	}
	
	//write projections in the dictionnary
	values	 = g_hash_table_get_values(dico->projections);
	commands = g_hash_table_get_keys(dico->projections);
	
	for(i = 0; i < g_list_length(values); i++ )
	{
		const gchar* w = g_strdup((gchar *) g_list_nth_data(commands, i));
		gdouble* tab = (gdouble *)g_list_nth_data(values, i);
		
		g_key_file_set_double_list(keyfile, "Projections", w, tab, words_size);
	}
		
	g_key_file_save_to_file (keyfile, dicFile, NULL);
	
	g_list_free(values);
	g_list_free(commands);
	g_key_file_free(keyfile);
}
