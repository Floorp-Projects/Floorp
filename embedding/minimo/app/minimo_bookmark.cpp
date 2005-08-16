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

#include "minimo_bookmark.h"

/* Bookmark global variables */
FILE *gBookmarkFilePointer;
gchar *gBookmarkFilePath;
GtkMozEmbed *gBookmarkMozEmbed;

/* initialize the embed variable */
void bookmark_moz_embed_initialize(GtkWidget *embed)
{
	gBookmarkMozEmbed= GTK_MOZ_EMBED(embed);
}

/* Create bookmarks manager window */
void bookmark_create_dialog ()
{
	BookmarkWindow *bwin;
	GtkTreeViewColumn *column1, *column2;
	GtkCellRenderer *renderer1,*renderer2;

	bookmark_open_file ();

	bwin = g_new0(BookmarkWindow,1);

	bwin->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        g_object_set_data(G_OBJECT(bwin->window),"window",bwin->window);
	gtk_window_set_title(GTK_WINDOW(bwin->window),"Bookmarks");
	gtk_widget_set_usize(bwin->window,230,350);
	gtk_window_set_resizable(GTK_WINDOW(bwin->window),FALSE);
	gtk_window_set_position (GTK_WINDOW(bwin->window),GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal (GTK_WINDOW (bwin->window), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW (bwin->window), TRUE);

	bwin->vbox1 = gtk_vbox_new(FALSE,0);
	gtk_widget_show(bwin->vbox1);
	gtk_container_add(GTK_CONTAINER(bwin->window),bwin->vbox1);

	bwin->hbox4 = gtk_hbox_new(FALSE,0);
	gtk_widget_show(bwin->hbox4);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->hbox4,FALSE,FALSE,0);

	bwin->menubar = gtk_menu_bar_new();

	bwin->menu_item_tools = gtk_menu_item_new_with_label("Tools");
	gtk_menu_bar_append(GTK_MENU_BAR(bwin->menubar), bwin->menu_item_tools);
	gtk_box_pack_start(GTK_BOX(bwin->hbox4),bwin->menubar,FALSE,FALSE,0);
	gtk_widget_show (bwin->menu_item_tools);
	gtk_widget_show (bwin->menubar);

	bwin->menu_tools = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(bwin->menu_item_tools),bwin->menu_tools);
	gtk_widget_show (bwin->menu_tools);

		bwin->menu_import = gtk_menu_item_new_with_label("Import bookmarks");
		gtk_menu_bar_append(GTK_MENU(bwin->menu_tools), bwin->menu_import);
		gtk_signal_connect (GTK_OBJECT (bwin->menu_import), "activate", GTK_SIGNAL_FUNC(bookmark_import_cb), bwin);
		gtk_widget_show (bwin->menu_import);

		bwin->menu_export = gtk_menu_item_new_with_label("Export bookmarks");
		gtk_menu_bar_append(GTK_MENU(bwin->menu_tools), bwin->menu_export);
		gtk_signal_connect (GTK_OBJECT (bwin->menu_export), "activate", GTK_SIGNAL_FUNC(bookmark_export_cb), bwin);
		gtk_widget_show (bwin->menu_export);

	bwin->scrolled_window = gtk_scrolled_window_new(NULL,NULL);
	gtk_widget_show(bwin->scrolled_window);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->scrolled_window,TRUE,TRUE,0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bwin->scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

	/* mount the bookmark treeview */
	bwin->treev_data = g_new0(BookmarkTreeVData,1);
	bwin->treev_data->treeStore = GTK_TREE_MODEL(gtk_tree_store_new(2,G_TYPE_STRING,G_TYPE_STRING));
	bwin->treev = gtk_tree_view_new_with_model(GTK_TREE_MODEL(bwin->treev_data->treeStore));

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (bwin->treev), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (bwin->treev), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW (bwin->treev), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW (bwin->treev), TRUE);
	gtk_widget_show(bwin->treev);

	gtk_container_add(GTK_CONTAINER(bwin->scrolled_window),bwin->treev);

	renderer1 = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_fixed_size (renderer1,100,-1);
	column1 = gtk_tree_view_column_new_with_attributes("Name", renderer1, "text", 0, NULL);
	gtk_tree_view_column_set_resizable (column1, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(bwin->treev),
				column1);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(bwin->treev),column1);
	gtk_tree_view_set_search_column (GTK_TREE_VIEW(bwin->treev),0);

	renderer2 = gtk_cell_renderer_text_new();
	column2 = gtk_tree_view_column_new_with_attributes("URL", renderer2, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(bwin->treev),
				column2);

	bwin->treev_data->parentIter = NULL;

	bookmark_load_from_file(bwin->treev_data);

	/* expand the main tree */
	gtk_tree_view_expand_row(GTK_TREE_VIEW (bwin->treev),gtk_tree_path_new_from_string("0"),FALSE);

	/* set selection mode */
	bwin->selection= gtk_tree_view_get_selection(GTK_TREE_VIEW (bwin->treev));
	gtk_tree_selection_set_mode (bwin->selection, GTK_SELECTION_SINGLE);

	/* set find function */
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW(bwin->treev),bookmark_search_function,bwin,NULL);

	/* set selection signal */
	g_signal_connect(bwin->treev,"cursor-changed",G_CALLBACK(bookmark_on_treev_select_row_cb),bwin);

	/* hbox1: name label, name entry, url label, url entry and add button */
	bwin->hbox1 = gtk_hbox_new(FALSE,0);
	gtk_widget_show(bwin->hbox1);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->hbox1,FALSE,FALSE,0);

	/* name label */
	bwin->text_label = gtk_label_new("Name ");
	gtk_widget_show(bwin->text_label);
	gtk_box_pack_start(GTK_BOX(bwin->hbox1),bwin->text_label,FALSE,FALSE,0);

	/* name entry */
	bwin->text_entry = gtk_entry_new();
	gtk_widget_show(bwin->text_entry);
	gtk_box_pack_start(GTK_BOX(bwin->hbox1),bwin->text_entry,TRUE,TRUE,0);

	/* url label */
	bwin->url_label = gtk_label_new(" URL ");
	gtk_widget_show(bwin->url_label);
	gtk_box_pack_start(GTK_BOX(bwin->hbox1),bwin->url_label,FALSE,FALSE,2);

	/* url entry */
	bwin->url_entry = gtk_entry_new();
	gtk_widget_show(bwin->url_entry);
	gtk_box_pack_start(GTK_BOX(bwin->hbox1),bwin->url_entry,TRUE,TRUE,0);
	gtk_entry_set_text(GTK_ENTRY(bwin->url_entry),"");

	/* hbox 2: add folder button and folder name entry */
	bwin->hbox2 = gtk_hbox_new(FALSE,0);
	gtk_widget_show(bwin->hbox2);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->hbox2,FALSE,FALSE,0);

	/* folder name label */
	bwin->text_label = gtk_label_new("Folder ");
	gtk_widget_show(bwin->text_label);
	gtk_box_pack_start(GTK_BOX(bwin->hbox2),bwin->text_label,FALSE,FALSE,0);

	/* folder name entry */
	bwin->folder_entry = gtk_entry_new();
	gtk_widget_show(bwin->folder_entry);
	gtk_box_pack_start(GTK_BOX(bwin->hbox2),bwin->folder_entry,TRUE,TRUE,0);

	/* add button */
	bwin->add_button = gtk_button_new_with_label("Add");
	gtk_widget_show(bwin->add_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox2),bwin->add_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->add_button),"clicked",G_CALLBACK(bookmark_add_button_cb),bwin);
	
	/* edit button */
	bwin->edit_button = gtk_button_new_with_label("Edit");
	gtk_widget_show(bwin->edit_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox2),bwin->edit_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->edit_button),"clicked",G_CALLBACK(bookmark_edit_button_cb),bwin);

	/* hbox 3: go, ok, remove buttons */
	bwin->hbox3 = gtk_hbox_new(FALSE,0);
	gtk_widget_show(bwin->hbox3);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->hbox3,FALSE,FALSE,0);

	/* go button */
	bwin->go_button = gtk_button_new_with_label("Go");
	gtk_widget_show(bwin->go_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->go_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->go_button),"clicked",G_CALLBACK(bookmark_go_button_cb),bwin);

	/* ok button */
	bwin->ok_button = gtk_button_new_with_label("Ok");
	gtk_widget_show(bwin->ok_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->ok_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->ok_button),"clicked",G_CALLBACK(bookmark_ok_button_cb),bwin);

	/* remove button */
	bwin->remove_button = gtk_button_new_with_label("Remove");
	gtk_widget_show(bwin->remove_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->remove_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->remove_button),"clicked",G_CALLBACK(bookmark_remove_button_cb),bwin);
	
	/* cancel button*/
	bwin->cancel_button = gtk_button_new_with_label("Cancel");
	gtk_widget_show(bwin->cancel_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->cancel_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->cancel_button),"clicked",G_CALLBACK(bookmark_cancel_button_cb),bwin);
	
	gtk_widget_show(bwin->window);
}

/* verify if there is a bookmark file */
void bookmark_open_file ()
{
	gBookmarkFilePath = g_strconcat(g_get_home_dir(),"/.Minimo/bookmarks",NULL);

	if (!(g_file_test(gBookmarkFilePath,G_FILE_TEST_EXISTS)))
	{
		gBookmarkFilePointer = fopen(gBookmarkFilePath,"w+");
		fprintf(gBookmarkFilePointer, "folder Bookmarks\n");
		fclose(gBookmarkFilePointer);
		bookmark_open_file ();
	}
	else
	{
		gBookmarkFilePointer = fopen(gBookmarkFilePath,"r");
	}
}

/* load the treeview from the file */
void bookmark_load_from_file(BookmarkTreeVData *treev_data)
{
	gchar *line = (gchar *)g_malloc(1024);
	BookmarkData *data= g_new0(BookmarkData,1);
	GtkTreeIter *parentIter= g_new0(GtkTreeIter,1);

	while(fgets(line,1023,gBookmarkFilePointer)!= NULL)
	{	
		line = g_strstrip(line);

		if (g_strncasecmp(line,"folder",6) == 0)
		{
			data->label = g_strdup(line+7);
			data->url = "";
			bookmark_insert_folder_on_treeview(data,treev_data);

			continue;
		}

		if (g_strncasecmp(line,"url",3) == 0)
		{
			gchar **temp;

			temp = g_strsplit(line+4," ",2);
			data->url = g_strdup(temp[0]);

			if (temp[1] != NULL)
				data->label = g_strdup(temp[1]);
			else
				data->label = g_strdup(temp[0]);
			g_strfreev(temp);

			bookmark_insert_item_on_treeview(data,treev_data);

			continue;
		}

		if (g_strncasecmp(line,"/folder",7) == 0)
		{
			if (gtk_tree_model_iter_parent(treev_data->treeStore, parentIter, treev_data->parentIter))
			{
				treev_data->parentIter= gtk_tree_iter_copy(parentIter);
			}
		}
	}

	g_free(line);
	g_free(data);
	g_free(parentIter);
}

/* the treeview internal search function */
gboolean bookmark_search_function(GtkTreeModel *tree_view, gint column, const gchar *key, GtkTreeIter *iter,void *bwin)
{
	BookmarkData *bmark = g_new0(BookmarkData,1);

	gtk_tree_model_get(GTK_TREE_MODEL(tree_view), iter, 0, &bmark->label, 1, &bmark->url, -1);

	if (g_str_has_prefix(g_ascii_strdown ((char *)(bmark->label),-1),g_ascii_strdown ((char *)key,-1)))
	{
		gtk_tree_selection_select_iter (((BookmarkWindow *)bwin)->selection, iter);
		bookmark_on_treev_select_row_cb (NULL, (BookmarkWindow *)bwin);
	}
	
	return TRUE;
}

/* insert a new folder on treeview */
void bookmark_insert_folder_on_treeview(BookmarkData *data, BookmarkTreeVData *treev_data)
{
	GtkTreeIter *newIter= g_new0(GtkTreeIter,1);

	gtk_tree_store_append(GTK_TREE_STORE(treev_data->treeStore),newIter,treev_data->parentIter);
	gtk_tree_store_set(GTK_TREE_STORE(treev_data->treeStore), newIter, 0, data->label, 1, data->url, -1);

	treev_data->parentIter= newIter;
}

/* insert a new url on treeview */
void bookmark_insert_item_on_treeview(BookmarkData *data, BookmarkTreeVData *treev_data)
{
	GtkTreeIter *newIter= g_new0(GtkTreeIter,1);

	gtk_tree_store_append(GTK_TREE_STORE(treev_data->treeStore),newIter,treev_data->parentIter);
	gtk_tree_store_set(GTK_TREE_STORE(treev_data->treeStore), newIter, 0, data->label, 1, data->url, -1);
}

/* get selected data */
void bookmark_on_treev_select_row_cb (GtkTreeView *view, BookmarkWindow *bwin)
{
	BookmarkData *bmark;

	bookmark_clear_all_entries(bwin);

	bmark = g_new0(BookmarkData,1);

	gtk_tree_selection_get_selected(bwin->selection, &bwin->treev_data->treeStore, &bwin->iter);
	gtk_tree_model_get(bwin->treev_data->treeStore, &bwin->iter, 0, &bmark->label, 1, &bmark->url, -1);

	if (g_ascii_strcasecmp(bmark->url,"") == 0)
	{
		gtk_entry_set_text(GTK_ENTRY(bwin->folder_entry), g_strstrip(bmark->label));
		gtk_entry_set_text(GTK_ENTRY(bwin->url_entry), "");
	}
	else
	{
        	gtk_entry_set_text(GTK_ENTRY(bwin->text_entry), g_strstrip(bmark->label));
	        gtk_entry_set_text(GTK_ENTRY(bwin->url_entry), g_strstrip(bmark->url));
	} 

	bwin->treev_data->parentIter= &bwin->iter;
	g_free(bmark);
}

/* add a bookmark */
void bookmark_add_button_cb(GtkWidget *button,BookmarkWindow *bwin)
{
	BookmarkData *data;
	GtkTreeIter *iter;
	
	iter= g_new0(GtkTreeIter,1);
	data = g_new0(BookmarkData,1);

	data->url = g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry)));

	/* it's a url */
	if (g_ascii_strcasecmp(data->url,"")!=0)
	{
		data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
		gtk_tree_store_append(GTK_TREE_STORE(bwin->treev_data->treeStore),iter,bwin->treev_data->parentIter);
		gtk_tree_store_set(GTK_TREE_STORE(bwin->treev_data->treeStore), iter, 0, data->label, 1, data->url, -1);
	}
	else /* it's a folder */
	{
		data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->folder_entry))));
		if (g_ascii_strcasecmp(data->label,"")==0)
			data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
		gtk_tree_store_append(GTK_TREE_STORE(bwin->treev_data->treeStore),iter,bwin->treev_data->parentIter);
		gtk_tree_store_set(GTK_TREE_STORE(bwin->treev_data->treeStore), iter, 0, data->label, 1, "", -1);
	}

	g_free(iter);
	bookmark_clear_all_entries(bwin);
}

/* edit a url or a folder */
void bookmark_edit_button_cb (GtkWidget *button,BookmarkWindow *bwin)
{
	BookmarkData *data;

	data = g_new0(BookmarkData,1);

	data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
	data->url = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry))));

	/* it's a folder */
	if (g_ascii_strcasecmp(data->url,"")==0)
	{
		data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->folder_entry))));
		if (g_ascii_strcasecmp(data->label,"")==0)
			data->label= g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
		gtk_tree_store_set(GTK_TREE_STORE(bwin->treev_data->treeStore), &bwin->iter, 0, data->label, 1, "", -1);
	}
	else /* it's a url */
	{
		data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
		gtk_tree_store_set(GTK_TREE_STORE(bwin->treev_data->treeStore), &bwin->iter, 0, data->label, 1, data->url, -1);
	}

	bookmark_clear_all_entries(bwin);
}

/* a button to go to a selected url */
void bookmark_go_button_cb (GtkButton *button,BookmarkWindow *bwin)
{
	gchar *url;

	/* do not open folders */
	if ((url= g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry)))))
	{	
		gtk_moz_embed_stop_load(GTK_MOZ_EMBED(gBookmarkMozEmbed));
		gtk_moz_embed_load_url(GTK_MOZ_EMBED(gBookmarkMozEmbed), url);
	}

	g_free(url);
	bookmark_clear_all_entries(bwin);
}

/* accept new edit configurations and update bookmark menu */
void bookmark_ok_button_cb (GtkWidget *button,BookmarkWindow *bwin)
{
	bookmark_write_on_file (bwin);
	bookmark_close_dialog(bwin);
}

/* cancel user's operations */
void bookmark_cancel_button_cb (GtkWidget *button,BookmarkWindow *bwin)
{	
	bookmark_close_dialog(bwin);
}

/* remove a selected bookmark */
void bookmark_remove_button_cb (GtkWidget *button,BookmarkWindow *bwin)
{
	BookmarkData *data;

	gtk_tree_selection_get_selected(bwin->selection, &bwin->treev_data->treeStore, &bwin->iter);

	data = g_new0(BookmarkData,1);
	gtk_tree_model_get(bwin->treev_data->treeStore, &bwin->iter, 0, &data->label, 1, &data->url, -1);

	gtk_tree_store_remove(GTK_TREE_STORE(bwin->treev_data->treeStore), &bwin->iter);

	bookmark_clear_all_entries(bwin);
}

/* write bookmarks on file */
void bookmark_write_on_file (BookmarkWindow *bwin)
{
	fclose(gBookmarkFilePointer);
	gBookmarkFilePointer = fopen(gBookmarkFilePath,"w");
	GtkTreeIter *iter= g_new0(GtkTreeIter,1);

	if (!gtk_tree_model_get_iter_first(bwin->treev_data->treeStore,iter)) return;

	bookmark_write_node_data_on_file(bwin->treev_data->treeStore, iter);

	g_free(iter);
}

/* print node data on file */
void bookmark_write_node_data_on_file (GtkTreeModel *treeStore, GtkTreeIter *iter)
{
     	BookmarkData *data= g_new0(BookmarkData,1);
	GtkTreeIter *newIter= g_new0(GtkTreeIter,1);

	gtk_tree_model_get(treeStore, iter, 0, &data->label, 1, &data->url, -1);

	/* it's a folder */
	if (g_ascii_strcasecmp(data->url,"") == 0)
	{
		fprintf(gBookmarkFilePointer,"folder %s\n",data->label);
		if (!gtk_tree_model_iter_children(treeStore,newIter,iter)) return;
		bookmark_write_node_data_on_file(treeStore, newIter);
		if (gtk_tree_model_iter_next(treeStore,newIter))
			bookmark_write_node_data_on_file(treeStore, newIter);
		if(g_ascii_strcasecmp(data->label,"Bookmarks") != 0)
			fprintf(gBookmarkFilePointer,"/folder\n");			
	}
	else /* it's a url */
	{
		fprintf(gBookmarkFilePointer,"url %s %s\n",data->url, data->label);
		if (gtk_tree_model_iter_next(treeStore,iter))
		{
			newIter= gtk_tree_iter_copy(iter);
			bookmark_write_node_data_on_file(treeStore, newIter);
		}
	}

	g_free(data);
	g_free(newIter);
}

/* clear all the bookmarks entries */
void bookmark_clear_all_entries(BookmarkWindow *bwin)
{
	gtk_entry_set_text(GTK_ENTRY(bwin->url_entry),"");
	gtk_editable_delete_text(GTK_EDITABLE(bwin->text_entry),0 ,-1);
	gtk_editable_delete_text(GTK_EDITABLE(bwin->folder_entry),0 ,-1);
}

/* close the bookmark window */
void bookmark_close_dialog(BookmarkWindow *bwin)
{	
	/* close bookmark file*/
	fclose(gBookmarkFilePointer);

	gtk_widget_destroy(bwin->window);
	g_free(bwin);
}

/* function used for importing bookmarks */
void bookmark_import_cb(GtkButton *button,BookmarkWindow *bwin)
{
	FILE *inFile;
	BookmarkData *data= g_new0(BookmarkData,1);
        gchar *parsedname;
	gchar *line = (gchar *)g_malloc(1024);
	gchar *filename= g_strconcat(g_get_home_dir(),"/bookmarks.html",NULL);
	GString *name = g_string_new (NULL);
        GString *url = g_string_new (NULL);

	fclose(gBookmarkFilePointer);

	if (!(inFile = fopen(filename,"r"))) return;
	if (!(gBookmarkFilePointer = fopen(gBookmarkFilePath,"a"))) return;

	if(fgets(line, 256, inFile) != NULL)
		if(!strstr(line, "NETSCAPE-Bookmark-file")) return;

	while (!feof (inFile))
	{
		switch (bookmark_get_ns_item (inFile, name, url)) {
                case NS_FOLDER: //While selected line is Folder
                        data->label = g_strdup (g_strstrip(name->str));
			fprintf(gBookmarkFilePointer,"folder %s\n",data->label);
                        g_free (data->label);
                        break;
                case NS_SITE://While selected line in Bookmark			
                        parsedname = bookmark_ns_parse_item (name);
			data->label = g_strdup (g_strstrip(parsedname));
			if (url->str)
				data->url = g_strdup(g_strstrip(url->str));
			else
                                data->url = NULL;
			fprintf(gBookmarkFilePointer,"url %s %s\n",data->url,data->label);
			g_free (data->label);
			g_free (data->url);
			g_free (parsedname);
			break;
		case NS_FOLDER_END://While selected line is end of Folder.
			fprintf(gBookmarkFilePointer,"/folder\n");
			break;
                default:
                        break;
		}
        }
        bookmark_close_dialog(bwin);

        g_string_free (name, TRUE);
        g_string_free (url, TRUE);
	g_free(data);
	g_free(filename);
}

NSItemType bookmark_get_ns_item (FILE *inFile, GString *name, GString *url)
{
	gchar *line = NULL;
	gchar *found;
	gchar *temp = NULL;
	gchar *bm_temp = NULL;
        GString *nick = g_string_new (NULL);

	line = bookmark_read_line_from_html_file (inFile);

	if ((found = (char *) bookmark_string_strcasestr (line, "<A HREF=")))
	{  // declare site? 
		g_string_assign (url, found+9);  // url=URL+ ADD_DATE ... 
		g_string_truncate (url, strstr(url->str, "\"") - url->str);
		found = (char *) strstr (found+9+url->len, "\">");
		if (!found)
		{
			g_free (line);
			return NS_UNKNOWN;
		}
		g_string_assign (name, found+2);
		temp = (char *) bookmark_string_strcasestr(name->str,"</A>");
		g_string_truncate (name, temp - (name->str));
		if ((found = (char *) bookmark_string_strcasestr (line,"SHORTCUTURL=")))
		{
			g_string_assign (nick, found+13);
			g_string_truncate (nick, strstr(nick->str, "\"") - nick->str);
		}
		else
			g_string_assign (nick, "");
		g_free (line);
		return NS_SITE;
	}
	else if ((found = (char *) bookmark_string_strcasestr (line, "<DT><H3")))
	{ //declare folder? 
		found = (char *) strstr(found+7, ">");
		if (!found) return NS_UNKNOWN;
		g_string_assign (name, found+1);
		bm_temp = (char *) bookmark_string_strcasestr (name->str, "</H3>");
		g_string_truncate (name, bm_temp - (name->str));
		g_free (line);
		return NS_FOLDER;
	}
	else if ((found = (char *) bookmark_string_strcasestr (line, "</DL>")))
	{    // end folder? 
		g_free (line);
		return NS_FOLDER_END;
	}
	else if ((found = (char *) bookmark_string_strcasestr (line, "<HR>")))
	{    // separator 
		g_free (line);
		return NS_SEPARATOR;
	}
	else if ((found = (char *) bookmark_string_strcasestr (line, "<DD>")))
	{    // comments 
		g_string_assign (name, found+4);
		g_free (line);
		return NS_NOTES;
	}
	else if (strchr(line, '<')==NULL && strchr(line, '>')==NULL)
	{    // continued comments (probably) 
		g_string_assign (name, line);
		g_free (line);
		return NS_NOTES;
	}
	g_free (line);
	g_string_free (nick, TRUE);
	return NS_UNKNOWN;
}

/**
 * bookmark_read_line_from_html_file: reads a line from an opened file and
 * returns it in a new allocated string
 */
gchar *bookmark_read_line_from_html_file (FILE *inFile)
{
	gchar *line = g_strdup ("");
	gchar *t;
	gchar *buf = g_new0 (gchar, 256);

        /* Read from the file unles endoffile has reached or newline has encountered */
	while (!(strchr (buf, '\n') || feof (inFile)))
	{
		fgets(buf, 256, inFile);
		t = line;
		line = g_strconcat (line, buf, NULL);
		g_free (t);
	}
	g_free (buf);
	return line;
}

/**
 * bookmark_string_strcasestr: test if a string b is a substring of string a,
 * independent of case.
 */
const gchar *bookmark_string_strcasestr (const gchar *a, const gchar *b)
{
	gchar *down_a;
	gchar *down_b;
	gchar *ptr;
	// copy and lower case the strings 
	down_a = g_strdup (a);
	down_b = g_strdup (b);
	//down_a = g_utf8_strdown (down_a, strlen (down_a))
	g_strdown (down_a);
	g_strdown (down_b);
	ptr = strstr (down_a, down_b);

	g_free (down_a);
	g_free (down_b);

	return ptr == NULL ? NULL : (a + (ptr - down_a));
}

/**
 * This function replaces some weird elements
 * like &amp; &le;, etc..
 * More info : http://www.w3.org/TR/html4/charset.html#h-5.3.2
 */
char *bookmark_ns_parse_item (GString *string)
{
	char *iterator, *temp;
	int cnt = 0;
	GString *result = g_string_new (NULL);


	iterator = string->str;

	for (cnt = 0, iterator = string->str;cnt <= (int)(strlen (string->str));cnt++, iterator++)
	{
		if (*iterator == '&')
		{
			int jump = 0;
			int i;
			if (g_strncasecmp (iterator, "&amp;", 5) == 0)
			{

				g_string_append_c (result, '&');
				jump = 5;
			}
			else if (g_strncasecmp (iterator, "&lt;", 4) == 0)
			{
				g_string_append_c (result, '<');
				jump = 4;
                        }
			else if (g_strncasecmp (iterator, "&gt;", 4) == 0)
			{
				g_string_append_c (result, '>');
				jump = 4;
			}
			else if (g_strncasecmp (iterator, "&quot;", 6) == 0)
			{
				g_string_append_c (result, '\"');
				jump = 6;
			}
			else
			{
				// It must be some numeric thing now 
				iterator++;
				if (iterator && *iterator == '#')
				{
					int val;
					char *num, *tmp;

					iterator++;

					val = atoi (iterator);

					tmp = g_strdup_printf ("%d", val);
					jump = strlen (tmp);
					g_free (tmp);

					num = g_strdup_printf ("%c", (char) val);
					g_string_append (result, num);
					g_free (num);
				}
			}
			for (i = jump - 1; i > 0; i--)
			{
				iterator++;
				if (iterator == NULL) break;
			}
		}
		else
			g_string_append_c (result, *iterator);
	}
	temp = result->str;
	g_string_free (result, FALSE);
	return temp;
}

/* function used for exporting bookmarks */
void bookmark_export_cb (GtkButton *button,BookmarkWindow *bwin)
{
	gchar *filename= g_strconcat(g_get_home_dir(),"/bookmarks.html",NULL);
	BookmarkData *data= g_new0(BookmarkData,1);
	gchar *line = (gchar *)g_malloc(1024);
	gboolean isFolder= TRUE;
	FILE *outFile;
       
	if (!(outFile = fopen (filename, "w"))) return;
                          
	fputs ("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n", outFile);
	fputs ("<!-- This file was automatically generated by Minimo\n", outFile);
	fputs ("It will be read and overwritten.\n", outFile);
	fputs ("Do Not Edit! -->\n", outFile);
	fputs ("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;"
                          " charset=UTF-8\">\n", outFile);
	fputs ("<TITLE>Bookmarks</TITLE>\n", outFile);
	fputs ("<H1>Minimo Bookmarks</H1>\n", outFile);
	fputs ("\n", outFile);
	fputs ("<DL><p>\n", outFile);	 

	rewind(gBookmarkFilePointer);

	while(fgets(line,1023,gBookmarkFilePointer)!= NULL)
	{	
		line = g_strstrip(line);

		if (g_strncasecmp(line,"folder",6) == 0)
		{
			data->label = g_strdup(line+7);
			data->url = "";
			isFolder = TRUE;
		}

		if (g_strncasecmp(line,"url",3) == 0)
		{
			gchar **temp;

			temp = g_strsplit(line+4," ",2);
			data->url = g_strdup(temp[0]);

			if (temp[1] != NULL)
				data->label = g_strdup(temp[1]);
			else
				data->label = g_strdup(temp[0]);
			g_strfreev(temp);
			isFolder = FALSE;

		}

		if (g_strncasecmp(line,"/folder",7) == 0)
		{
			fputs ("</DL><p>\n", outFile);
		}

		bookmark_export_items (outFile, data, isFolder);
	}
	fputs ("</DL><p>\n", outFile);
	fputs ("</DL><p>\n", outFile);

	fclose (outFile);
	g_free(data);
	g_free(line);
	g_free(filename);
}

void bookmark_export_items (FILE *file, BookmarkData *data, gboolean isFolder)
{

	gchar *str;

	/* it is a url */
	if (isFolder == FALSE) {
		fputs ("\t<DT><A HREF=\"", file);
		fputs (data->url, file);
		fputs ("\"", file);
		fputs (">", file);
		str = g_strdup(data->label);
		fputs (str, file);
		fputs ("</A>\n", file);
	} /* it is a folder */
	else
	{
		fputs ("<DT><H3 ADD_DATE=\"0\">", file);
		str = g_strdup(data->label);
		fputs (str, file);
		fputs ("</H3>\n", file);
		fputs ("<DL><p>\n", file);
	}
}

void bookmark_add_url_directly_cb (GtkWidget *menu_item,GtkWidget *embed)
{
	gchar *url;
	gchar *title;

	bookmark_open_file ();
	fclose(gBookmarkFilePointer);

	if (!(gBookmarkFilePointer = fopen(gBookmarkFilePath,"a+"))) return;

	if (!(url= gtk_moz_embed_get_location(gBookmarkMozEmbed))) return;

	/* doesn't add an empty url or about:blank */
	if ((g_ascii_strcasecmp(url,"") != 0) && (g_ascii_strcasecmp(url,"about:blank") != 0))
	{
		title= gtk_moz_embed_get_title (gBookmarkMozEmbed);
		fprintf(gBookmarkFilePointer, "url %s %s\n", url, title);
		g_free(title);
	}

	fclose(gBookmarkFilePointer);
	g_free(url);
	g_free(gBookmarkFilePath);
}
