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
 *    Antonio Gomes     <agan@ufam.edu.br>
 *    Diego Gonzalez    <diego.gonzalez@indt.org.br>
 *    Andre Pedralho    <asp@ufam.edu.br>
 *    Raoni Novellino   <raoni.novellino@indt.org.br>
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

#include "gtkmozembed.h"

/* Represents a Bookmark Item */
typedef struct _BookmarkData {
	gchar *label;
	gchar *url;
} BookmarkData;

/* Represents Bookmarks List View*/
typedef struct _BookmarkCTreeData {
	GtkWidget *ctree;
	GtkCTreeNode *parent;
} BookmarkCTreeData;

/* Bookmarks Window Structure */
typedef struct _BookmarkWindow {
	GtkWidget *window;
	GtkWidget *scrolled_window;
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *hbox3;
	GtkWidget *text_label;
	GtkWidget *text_entry;
	GtkWidget *url_label;
	GtkWidget *url_entry;
	GtkWidget *add_button;
	GtkWidget *add_folder_button;
	GtkWidget *folder_entry;
	GtkWidget *remove_button;
	GtkWidget *ok_button;
	GtkWidget *go_button;
	GtkWidget *cancel_button;
	GtkWidget *ctree;
	GtkCTreeNode *menu_node;
	BookmarkData *menu_node_data;
	BookmarkCTreeData ctree_data;
} BookmarkWindow;

/* Callbacks from the UI */
void show_bookmark(GtkWidget *embed);
void read_bookmark(void);
void generate_bookmark_ctree(GNode *node, BookmarkCTreeData *ctree_data);
void add_bookmark_cb(GtkButton *button,GtkMozEmbed *min);
void on_bookmark_add_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_add_folder_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_ok_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_go_button_clicked(GtkButton *button,BookmarkWindow *bwin);
void on_bookmark_remove_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_cancel_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void close_bookmark_window(BookmarkWindow *bwin);

/* Callbacks from widgets*/
void on_bookmark_ctree_select_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin);
void on_bookmark_ctree_unselect_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin);
void on_bookmark_ctree_move(GtkWidget *ctree,GtkCTreeNode *node,GtkCTreeNode *parent,GtkCTreeNode *sibling,BookmarkWindow *bwin);
void print_bookmarks ();
void print_node_data (GNode *node,FILE *file);
void clear_entries(BookmarkWindow *bwin);

/* Global variables */
FILE *bookmark_file;
gchar *file;
GNode *bookmarks;
GtkMozEmbed *minEmbed;

/* Bookmarks Functions*/

/* Create bookmarks manager window */
void show_bookmark(GtkWidget *embed)
{
	BookmarkWindow *bwin;
	
	bwin = g_new0(BookmarkWindow,1);
	
	minEmbed= (GtkMozEmbed *) embed;

	bwin->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_set_data(G_OBJECT(bwin->window),"window",bwin->window);
	gtk_window_set_title(GTK_WINDOW(bwin->window),"Bookmarks");
	gtk_widget_set_usize(bwin->window,230,300);
	gtk_window_set_resizable(GTK_WINDOW(bwin->window),FALSE);
	gtk_window_set_position (GTK_WINDOW(bwin->window),GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal (GTK_WINDOW (bwin->window), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW (bwin->window), TRUE);

	bwin->vbox1 = gtk_vbox_new(FALSE,0);
	gtk_widget_show(bwin->vbox1);
	gtk_container_add(GTK_CONTAINER(bwin->window),bwin->vbox1);

	bwin->scrolled_window = gtk_scrolled_window_new(NULL,NULL);
	gtk_widget_show(bwin->scrolled_window);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->scrolled_window,TRUE,TRUE,0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bwin->scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

	gchar *titles[] = {("Name"),("URL")};
	gchar *menur[] = {("Bookmarks"),""};

	file = g_strconcat(g_get_home_dir(),"/.Minimo/bookmarks",NULL);
	if ((bookmark_file = fopen(file,"r"))!= NULL)
	{
		fclose(bookmark_file);
		bookmark_file = fopen(file,"r+");
	}
	else
		bookmark_file = fopen(file,"w+");

	bwin->menu_node_data = g_new0(BookmarkData,1);
	bwin->menu_node_data->label = g_strdup("Bookmarks");
	bwin->menu_node_data->url = NULL;

	bookmarks= g_node_new(bwin->menu_node_data);

	read_bookmark();

	/* mount the bookmark ctree */
	bwin->ctree = gtk_ctree_new_with_titles(2,0,titles);
	gtk_container_add(GTK_CONTAINER(bwin->scrolled_window),bwin->ctree);
	gtk_clist_set_column_width(GTK_CLIST(bwin->ctree),0,150);
	gtk_clist_set_column_width(GTK_CLIST(bwin->ctree),1,150);
	gtk_clist_column_titles_show(GTK_CLIST(bwin->ctree));
	gtk_clist_set_reorderable(GTK_CLIST(bwin->ctree),TRUE);

	bwin->menu_node = gtk_ctree_insert_node(GTK_CTREE(bwin->ctree),NULL,NULL,menur,0,NULL,NULL,NULL,NULL,FALSE,TRUE);
	gtk_ctree_node_set_row_data(GTK_CTREE(bwin->ctree),bwin->menu_node,bwin->menu_node_data);
	gtk_ctree_node_set_selectable(GTK_CTREE(bwin->ctree),bwin->menu_node,FALSE);
	bwin->ctree_data.ctree = bwin->ctree;

	/* show bookmark ctree */
	if (bookmarks != NULL)
	{	bwin->ctree_data.parent = bwin->menu_node;
		g_node_children_foreach(bookmarks,G_TRAVERSE_ALL,(GNodeForeachFunc)generate_bookmark_ctree,&bwin->ctree_data);
	}

	gtk_widget_show(bwin->ctree);

	g_signal_connect(G_OBJECT(bwin->ctree),"tree_select_row",G_CALLBACK(on_bookmark_ctree_select_row),bwin);
	g_signal_connect(G_OBJECT(bwin->ctree),"tree_unselect_row",G_CALLBACK(on_bookmark_ctree_unselect_row),bwin);
	g_signal_connect(G_OBJECT(bwin->ctree),"tree_move",G_CALLBACK(on_bookmark_ctree_move),bwin);

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

	/* add button */
	bwin->add_button = gtk_button_new_with_label("Add");
	gtk_widget_show(bwin->add_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox1),bwin->add_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->add_button),"clicked",G_CALLBACK(on_bookmark_add_button_clicked),bwin);

	/* hbox 2: add folder button and folder name entry */
	bwin->hbox2 = gtk_hbox_new(FALSE,0);
	gtk_widget_show(bwin->hbox2);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->hbox2,FALSE,FALSE,0);

	/* folder name entry */
	bwin->folder_entry = gtk_entry_new();
	gtk_widget_show(bwin->folder_entry);
	gtk_box_pack_start(GTK_BOX(bwin->hbox2), bwin->folder_entry,TRUE,TRUE,0);

	/* add folder button */
	bwin->add_folder_button = gtk_button_new_with_label("Add folder");
	gtk_widget_show(bwin->add_folder_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox2),bwin->add_folder_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->add_folder_button),"clicked",G_CALLBACK(on_bookmark_add_folder_button_clicked),bwin);
	
	/* hbox 3: go, ok, remove buttons */
	bwin->hbox3 = gtk_hbox_new(FALSE,0);
	gtk_widget_show(bwin->hbox3);
	gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->hbox3,FALSE,FALSE,0);

	/* go button */
	bwin->go_button = gtk_button_new_with_label("Go");
	gtk_widget_show(bwin->go_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->go_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->go_button),"clicked",G_CALLBACK(on_bookmark_go_button_clicked),bwin);

	/* ok button */
	bwin->ok_button = gtk_button_new_with_label("Ok");
	gtk_widget_show(bwin->ok_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->ok_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->ok_button),"clicked",G_CALLBACK(on_bookmark_ok_button_clicked),bwin);

	/* remove button */
	bwin->remove_button = gtk_button_new_with_label("Remove");
	gtk_widget_show(bwin->remove_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->remove_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->remove_button),"clicked",G_CALLBACK(on_bookmark_remove_button_clicked),bwin);
	
	/* cancel button*/
	bwin->cancel_button = gtk_button_new_with_label("Cancel");
	gtk_widget_show(bwin->cancel_button);
	gtk_box_pack_start(GTK_BOX(bwin->hbox3),bwin->cancel_button,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(bwin->cancel_button),"clicked",G_CALLBACK(on_bookmark_cancel_button_clicked),bwin);
	
	gtk_widget_show(bwin->window);
	
}

/* read bookmarks from file */
void read_bookmark()
{
	gchar *line;
	BookmarkData *data;
	GNode *parent;

	line = (gchar *)g_malloc(1024);

	parent= bookmarks;

	while(fgets(line,1023,bookmark_file)!= NULL)
	{	
		line = g_strstrip(line);

		if (g_strncasecmp(line,"folder",6) == 0)
		{
			data = g_new0(BookmarkData,1);
			data->label = g_strdup(line+7);
			data->url = " ";

			parent= g_node_append_data(parent, data);

			continue;
		}

		if (g_strncasecmp(line,"url",3) == 0)
		{
			gchar **temp;

			data = g_new0(BookmarkData,1);
			temp = g_strsplit(line+4," ",2);
			data->url = g_strdup(temp[0]);

			if (temp[1] != NULL)
				data->label = g_strdup(temp[1]);
			else
				data->label = g_strdup(temp[0]);
			g_strfreev(temp);

			g_node_append_data(parent,data);

			continue;
		}

		if (g_strncasecmp(line,"/folder",7) == 0)
		{
		 	parent= parent->parent;
			continue;
		}
	}
	g_free(line);
}

/* generate bookmark ctree */
void generate_bookmark_ctree(GNode *node, BookmarkCTreeData *ctree_data)
{
	BookmarkData *data;
	gchar *ctree_entry[2];
	GtkCTreeNode *ctree_node;

	data= (BookmarkData*) node->data;
     
        ctree_entry[0] = data->label;
	ctree_entry[1] = data->url;

	/* it's a folder */
	if (g_strncasecmp(data->url," ",1) == 0)
	{
		BookmarkCTreeData new_ctree_data;
		ctree_node = gtk_ctree_insert_node(GTK_CTREE(ctree_data->ctree),ctree_data->parent,NULL,ctree_entry,0,NULL,NULL,NULL,NULL,FALSE,TRUE);
		new_ctree_data.ctree = ctree_data->ctree;
		new_ctree_data.parent = ctree_node;
		g_node_children_foreach(node,G_TRAVERSE_ALL,(GNodeForeachFunc)generate_bookmark_ctree,&new_ctree_data);
	}/* it's a url */
	else
		ctree_node = gtk_ctree_insert_node(GTK_CTREE(ctree_data->ctree),ctree_data->parent,NULL,ctree_entry,0,NULL,NULL,NULL,NULL,TRUE,TRUE);

	gtk_ctree_node_set_row_data(GTK_CTREE(ctree_data->ctree),ctree_node,data);
}

/* get selected data */
void on_bookmark_ctree_select_row(GtkWidget *ctree, GtkCTreeNode *node,gint col, BookmarkWindow *bwin)
{
	BookmarkData *bmark;

     	bmark = (BookmarkData*) gtk_ctree_node_get_row_data(GTK_CTREE(bwin->ctree), node);

	/* it's a url */
	if (g_strncasecmp(bmark->url," ",1) != 0)
	{
        	gtk_entry_set_text(GTK_ENTRY(bwin->text_entry), bmark->label);
	        gtk_entry_set_text(GTK_ENTRY(bwin->url_entry), bmark->url);
	}/* it's a folder */
	else
		gtk_entry_set_text(GTK_ENTRY(bwin->folder_entry), bmark->label);	
}
/* set null values to entries */
void on_bookmark_ctree_unselect_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin)
{
        gtk_entry_set_text(GTK_ENTRY(bwin->text_entry),"");
        gtk_entry_set_text(GTK_ENTRY(bwin->url_entry),"");
	gtk_entry_set_text(GTK_ENTRY(bwin->folder_entry),"");
}

/* move a node */
void on_bookmark_ctree_move(GtkWidget *ctree,GtkCTreeNode *node,GtkCTreeNode *parent,GtkCTreeNode *sibling,BookmarkWindow *bwin)
{
	BookmarkData *data, *parent_data;
	GNode *menu_node;

	data= (BookmarkData *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree),node);

	if (parent== NULL || g_strncasecmp(data->url," ",1) == 0) return;

	menu_node= g_node_find(bookmarks,G_IN_ORDER,G_TRAVERSE_ALL,data);
	g_node_destroy(menu_node);

	parent_data= (BookmarkData *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree),parent);
	menu_node= g_node_find(bookmarks, G_IN_ORDER, G_TRAVERSE_ALL, parent_data);
	g_node_append_data(menu_node,data);

}

/* add a bookmark */
void on_bookmark_add_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{
	BookmarkData *data;
	gchar *ctree_entry[2];
	GtkCTreeNode *node;
	
	data = g_new0(BookmarkData,1);

	data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
	data->url = ctree_entry[1] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry))));

	node = gtk_ctree_insert_node(GTK_CTREE(bwin->ctree),bwin->menu_node,NULL,ctree_entry,0,NULL,NULL,NULL,NULL,TRUE,FALSE);
	gtk_ctree_node_set_row_data(GTK_CTREE(bwin->ctree),node,data);
	g_node_append_data(bookmarks,data);

	clear_entries(bwin);
}

/* add a folder */
void on_bookmark_add_folder_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{
	BookmarkData *data;
	gchar *ctree_entry[2];
	GtkCTreeNode *node;
	
	data = g_new0(BookmarkData,1);

	data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->folder_entry))));
	data->url = " ";
	ctree_entry[1] = NULL;

	node = gtk_ctree_insert_node(GTK_CTREE(bwin->ctree),bwin->menu_node,NULL,ctree_entry,0,NULL,NULL,NULL,NULL,FALSE,TRUE);
	gtk_ctree_node_set_row_data(GTK_CTREE(bwin->ctree),node,data);
	g_node_append_data(bookmarks,data);

	clear_entries(bwin);
}

/* a button to go to a selected url */
void on_bookmark_go_button_clicked(GtkButton *button,BookmarkWindow *bwin)
{
	GList *selection;
	gchar *url;

	/* case there isn't a selected url */
	if (!(selection = GTK_CLIST(bwin->ctree)->selection)) {
		return;
	}

	url= g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry))));
	
	/* it isn't a folder */
	if (g_strncasecmp(url," ",1) != 0)
	{
		gtk_moz_embed_stop_load(GTK_MOZ_EMBED(minEmbed));
		gtk_moz_embed_load_url(GTK_MOZ_EMBED(minEmbed), url);
	}
	clear_entries(bwin);
}

/* accept new edit configurations and update bookmark menu */
void on_bookmark_ok_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{
	print_bookmarks();
	close_bookmark_window(bwin);
}

/* remove a selected bookmark */
void on_bookmark_remove_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{
	GList *selection;
	GtkCTreeNode *node;
	BookmarkData *data;
	GNode *menu_node;

	/* case there isn't a selected folder, url or separator to be removed */
	if (!(selection = GTK_CLIST(bwin->ctree)->selection)) {
		return;
	}

	node = (GtkCTreeNode*) g_list_nth_data(selection,0);

	data = g_new0(BookmarkData,1);
	data= (BookmarkData*) gtk_ctree_node_get_row_data(GTK_CTREE(bwin->ctree),node);

	gtk_ctree_remove_node(GTK_CTREE(bwin->ctree),node);

	menu_node= g_node_find(bookmarks, G_IN_ORDER, G_TRAVERSE_ALL, data);

	g_node_destroy(menu_node);

	clear_entries(bwin);
}

/* cancel user's operations */
void on_bookmark_cancel_button_clicked(GtkWidget *button, BookmarkWindow *bwin)
{	
	gtk_widget_destroy(bwin->window);

}

/* write bookmarks on file */
void print_bookmarks ()
{
	fclose(bookmark_file);
	bookmark_file = fopen(file,"w");
	
	if (bookmarks != NULL)
		g_node_children_foreach(bookmarks,G_TRAVERSE_ALL,(GNodeForeachFunc)print_node_data,bookmark_file);
}

/* print node data on file */
void print_node_data (GNode *node,FILE *bookmark_file)
{
     	BookmarkData *data;
        
	data = (BookmarkData*) node->data;

	/* it's a url */
	if (g_strncasecmp(data->url," ",1) != 0)
		fprintf(bookmark_file,"url %s %s\n",data->url,data->label);
	else /* it's a folder */
	{
		fprintf(bookmark_file,"folder %s\n",data->label);
		g_node_children_foreach(node,G_TRAVERSE_ALL,(GNodeForeachFunc)print_node_data,bookmark_file);
		fprintf(bookmark_file,"/folder\n");
	}
}

/* clear all the bookmarks entries */
void clear_entries(BookmarkWindow *bwin)
{
	gtk_editable_delete_text(GTK_EDITABLE(bwin->url_entry),0 ,-1);
	gtk_editable_delete_text(GTK_EDITABLE(bwin->text_entry),0 ,-1);
	gtk_editable_delete_text(GTK_EDITABLE(bwin->folder_entry),0 ,-1);
}

/* close the bookmark window */
void close_bookmark_window(BookmarkWindow *bwin)
{	
	/* close bookmark file*/
	fclose(bookmark_file);
	
	gtk_widget_destroy(bwin->window);
	g_free(bwin->menu_node_data);
	g_free(bwin);
}

/* the menu button to add a bookmark */
void add_bookmark_cb(GtkButton *button,GtkMozEmbed *min)
{
	gchar *url;
	gchar *title;

	if (!(bookmark_file = fopen(file,"a+"))) return;
	
	if (!(url= gtk_moz_embed_get_location(minEmbed))) return;

	/* doesn't add an empty url or about:blank */
	if ((g_strcasecmp(url,"") != 0) && (g_strcasecmp(url,"about:blank") != 0))
	{
		title= gtk_moz_embed_get_title (GTK_MOZ_EMBED (minEmbed));
		fprintf(bookmark_file, "url %s %s\n", url, title);
	}
	fclose(bookmark_file);	
}
