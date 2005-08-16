/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Doug Turner <dougt@meer.net>  Branched from TestGtkEmbed.cpp
 *
 *   The 10LE Team (in alphabetical order)
 *   -------------------------------------
 *
 *    Ilias Biris       <ext-ilias.biris@indt.org.br> - Coordinator
 *    Afonso Costa      <afonso.costa@indt.org.br>
 *    Antonio Gomes     <antonio.gomes@indt.org.br>
 *    Diego Gonzalez    <diego.gonzalez@indt.org.br>
 *    Raoni Novellino   <raoni.novellino@indt.org.br>
 *    Andre Pedralho    <andre.pedralho@indt.org.br>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "minimo_support.h"
#include "Minimo.h"
#include "minimo_callbacks.h"

extern GList *gMinimoBrowserList;	/* the list of browser windows
 currently open */
extern gchar *gMinimoProfilePath;

/* Variable to enable pango settings */
static PangoFontDescription* gDefaultMinimoFont = NULL;

/* Data Struct for make possible the real autocomplete */
GList *gAutoCompleteList = g_list_alloc(); 	/* Auto Complete List */
GtkEntryCompletion *gMinimoEntryCompletion;
GtkListStore 	*gMinimoAutoCompleteListStore;
GtkTreeIter gMinimoAutoCompleteIter;

/* body */
void support_handle_remote(int sig) {
	
	FILE *fp;
	char url[256];
	
	sprintf(url, "/tmp/Mosaic.%d", getpid());
	if ((fp = fopen(url, "r"))) {
	
		if (fgets(url, sizeof(url) - 1, fp)) {
			MinimoBrowser *bw = NULL;
			if (strncmp(url, "goto", 4) == 0 && 
				fgets(url, sizeof(url) - 1, fp)) {
					GList *tmp_list = gMinimoBrowserList;
					bw =(MinimoBrowser *)tmp_list->data;
					if(!bw) return;
		
			} else if (strncmp(url, "newwin", 6) == 0 &&
				fgets(url, sizeof(url) - 1, fp)) { 
		
				bw = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);
				gtk_widget_set_usize(bw->mozEmbed, 240, 320);
				set_browser_visibility(bw, TRUE);
			}
	
			if (bw)
				gtk_moz_embed_load_url(GTK_MOZ_EMBED(bw->mozEmbed), url);
				fclose(fp);
		}
	}
	return;
}

void support_init_remote() {
	gchar *file;
	FILE *fp;
	
	signal(SIGUSR1, SIG_IGN);
	
	/* Write the pidfile : would be useful for automation process*/
	file = g_strconcat(g_get_home_dir(), "/", ".mosaicpid", NULL);
	if((fp = fopen(file, "w"))) {
		fprintf (fp, "%d\n", getpid());
		fclose (fp);
		signal(SIGUSR1, support_handle_remote);
	}
	g_free(file);
}

void support_cleanup_remote() {

	gchar *file;

	signal(SIGUSR1, SIG_IGN);

	file = g_strconcat(g_get_home_dir(), "/", ".mosaicpid", NULL);
	unlink(file);
	g_free(file);
}

void support_autocomplete_populate(gpointer data, gpointer combobox) {
  
	if (!data || !combobox)
		return;
	
	gtk_combo_set_popdown_strings (GTK_COMBO (combobox), gAutoCompleteList);
}

void support_populate_autocomplete(GtkCombo *comboBox) {
  
	g_list_foreach(gAutoCompleteList, support_autocomplete_populate, comboBox);
}

gint support_autocomplete_list_cmp(gconstpointer a, gconstpointer b) {
  
	if (!a)
		return -1;
	if (!b)
		return 1;
	
	return strcmp((char*)a, (char*)b);
}

void support_init_autocomplete() {
	if (!gAutoCompleteList)
		return;
	
	char* full_path = g_strdup_printf("%s/%s", gMinimoProfilePath, "autocomplete.txt");
	
	FILE *fp;
	char url[255];
	
	gMinimoAutoCompleteListStore = gtk_list_store_new (1, G_TYPE_STRING);
	
	if((fp = fopen(full_path, "r+"))) {
		while(fgets(url, sizeof(url) - 1, fp)) {
			int length = strlen(url);
			if (url[length-1] == '\n')
				url[length-1] = '\0'; 
			gAutoCompleteList = g_list_append(gAutoCompleteList, g_strdup(url));
			/* Append url's to autocompletion feature */
			gtk_list_store_append (gMinimoAutoCompleteListStore, &gMinimoAutoCompleteIter);
			gtk_list_store_set (gMinimoAutoCompleteListStore, &gMinimoAutoCompleteIter, 0, url, -1);
		}
		fclose(fp);
	}
  
	gAutoCompleteList =   g_list_sort(gAutoCompleteList, support_autocomplete_list_cmp);
}

void support_autocomplete_writer(gpointer data, gpointer fp) {

	FILE* file = (FILE*) fp;
	char* url  = (char*) data;
	
	if (!url || !fp)
		return;
	
	fwrite(url, strlen(url), 1, file);
	fputc('\n', file);  
}

void support_autocomplete_destroy(gpointer data, gpointer dummy) {
  
	g_free(data);
}

void support_cleanup_autocomplete() {
	char* full_path = g_strdup_printf("%s/%s", gMinimoProfilePath, "autocomplete.txt");
	FILE *fp;
	
	if((fp = fopen(full_path, "w"))) {
		g_list_foreach(gAutoCompleteList, support_autocomplete_writer, fp);
		fclose(fp);
	}
	
	g_free(full_path);
	g_list_foreach(gAutoCompleteList, support_autocomplete_destroy, NULL);
}

void support_add_autocomplete(const char* value, OpenDialogParams* params) {
  
	GList* found = g_list_find_custom(gAutoCompleteList, value, support_autocomplete_list_cmp);
	
	if (!found) {
		gAutoCompleteList = g_list_insert_sorted(gAutoCompleteList, g_strdup(value), support_autocomplete_list_cmp);    
	/* Append url's to autocompletion*/
		gtk_list_store_append (gMinimoAutoCompleteListStore, &gMinimoAutoCompleteIter);
		gtk_list_store_set (gMinimoAutoCompleteListStore, &gMinimoAutoCompleteIter, 0, value, -1);
	}
	gtk_combo_set_popdown_strings (GTK_COMBO (params->dialog_combo), gAutoCompleteList);
}

/* Method that build the entry completion */
void support_build_entry_completion (MinimoBrowser* browser) 
{  
	/* Minimo entry completion */
	gMinimoEntryCompletion = gtk_entry_completion_new ();
	
	gtk_entry_completion_set_model (gMinimoEntryCompletion, GTK_TREE_MODEL (gMinimoAutoCompleteListStore));
	gtk_entry_completion_set_text_column (gMinimoEntryCompletion, 0);
}

/* Method to sep up the escape key handler */
void support_setup_escape_key_handler(GtkWidget *window) 
{
	  g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(support_escape_key_handler), NULL);
}

/* Method to handler the escape key */
gint support_escape_key_handler(GtkWidget *window, GdkEventKey *ev) 
{
	g_return_val_if_fail(window != NULL, FALSE);
	
	if (ev->keyval == GDK_Escape) {
		gtk_widget_destroy(window);
		return (1);
	}
	return (0);
}

PangoFontDescription* getOrCreateDefaultMinimoFont()
{
	if (gDefaultMinimoFont)
		return gDefaultMinimoFont;
  
	gDefaultMinimoFont = pango_font_description_from_string("sans 8");
	return gDefaultMinimoFont;
}

void support_destroy_dialog_params_cb(GtkWidget *widget, OpenDialogParams* params)
{
	free(params);
}
