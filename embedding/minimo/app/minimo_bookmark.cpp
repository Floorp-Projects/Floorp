/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this gBookmakeFile are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this gBookmakeFile except in compliance with
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
 * Alternatively, the contents of this gBookmakeFile may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this gBookmakeFile only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this gBookmakeFile under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this gBookmakeFile under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "minimo_bookmark.h"

/* Global variables */
FILE *gBookmakeFilePath;
gchar *gBookmakeFile;
GNode *gBookmarkNodes;
GtkMozEmbed *gBookmarkMozEmbed;

/* Bookmarks Functions*/
void initialize_bookmark(GtkWidget *embed)
{
  gBookmarkMozEmbed= GTK_MOZ_EMBED(embed);
}

/* verify if there is a bookmark gBookmakeFile */
void open_bookmark()
{
  gBookmakeFile = g_strconcat(g_get_home_dir(),"/.Minimo/bookmark",NULL);
  
  if (!(g_file_test(gBookmakeFile,G_FILE_TEST_EXISTS)))
  {
    gBookmakeFilePath = fopen(gBookmakeFile,"w+");
    fprintf(gBookmakeFilePath, "Bookmarks\n");
    fclose(gBookmakeFilePath);
    open_bookmark();
  }
  else
  {
    gBookmakeFilePath = fopen(gBookmakeFile,"r");
    show_bookmark();
  }
}

/* Create gBookmarkNodes manager window */
void show_bookmark()
{
  BookmarkWindow *bwin;
  
  bwin = g_new0(BookmarkWindow,1);
  
  //	gBookmarkMozEmbed= GTK_MOZ_EMBED(embed);
  //	bwin->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  
  bwin->window = gtk_dialog_new();
  g_object_set_data(G_OBJECT(bwin->window),"window",bwin->window);
  gtk_window_set_title(GTK_WINDOW(bwin->window),"Bookmarks");
  gtk_widget_set_usize(bwin->window,230,350);
  gtk_window_set_resizable(GTK_WINDOW(bwin->window),FALSE);
  gtk_window_set_position (GTK_WINDOW(bwin->window),GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (bwin->window), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW (bwin->window), TRUE);
  
  bwin->vbox1 = gtk_vbox_new(FALSE,0);
  gtk_widget_show(bwin->vbox1);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(bwin->window)->vbox),bwin->vbox1, TRUE, TRUE, 0);
  
  bwin->scrolled_window = gtk_scrolled_window_new(NULL,NULL);
  gtk_widget_show(bwin->scrolled_window);
  gtk_box_pack_start(GTK_BOX(bwin->vbox1),bwin->scrolled_window,TRUE,TRUE,0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bwin->scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  
  gchar *titles[] = {("Name"),("URL")};
  gchar *menur[] = {"Bookmarks",""};
  
  bwin->menu_node_data = g_new0(BookmarkData,1);
  bwin->menu_node_data->label = g_strdup("Bookmarks");
  bwin->menu_node_data->url = NULL;
  
  gBookmarkNodes= g_node_new(bwin->menu_node_data);
  bwin->parent_node= gBookmarkNodes;
  
  read_bookmark();
  
  /* mount the bookmark ctree */
  bwin->ctree = gtk_ctree_new_with_titles(2,0,titles);
  gtk_container_add(GTK_CONTAINER(bwin->scrolled_window),bwin->ctree);
  gtk_clist_set_column_width(GTK_CLIST(bwin->ctree),0,150);
  gtk_clist_set_column_width(GTK_CLIST(bwin->ctree),1,150);
  gtk_clist_column_titles_show(GTK_CLIST(bwin->ctree));
  gtk_clist_set_reorderable(GTK_CLIST(bwin->ctree),TRUE);
  gtk_clist_set_auto_sort(GTK_CLIST(bwin->ctree),TRUE);
  
  bwin->menu_node = gtk_ctree_insert_node(GTK_CTREE(bwin->ctree),NULL,NULL,menur,0,NULL,NULL,NULL,NULL,FALSE,TRUE);
  gtk_ctree_node_set_row_data(GTK_CTREE(bwin->ctree),bwin->menu_node,bwin->menu_node_data);
  gtk_ctree_node_set_selectable(GTK_CTREE(bwin->ctree),bwin->menu_node,FALSE);
  bwin->ctree_data.ctree = bwin->ctree;
  
  /* show bookmark ctree */
  if (gBookmarkNodes != NULL)
  {
    bwin->ctree_data.parent = bwin->menu_node;
    g_node_children_foreach(gBookmarkNodes,G_TRAVERSE_ALL,(GNodeForeachFunc)generate_bookmark_ctree,&bwin->ctree_data);
  }
  
  gtk_widget_show(bwin->ctree);
  
  g_signal_connect(G_OBJECT(bwin->ctree),"tree_select_row",G_CALLBACK(on_bookmark_ctree_select_row),bwin);
  g_signal_connect(G_OBJECT(bwin->ctree),"tree_unselect_row",G_CALLBACK(on_bookmark_ctree_unselect_row),bwin);
  g_signal_connect(G_OBJECT(bwin->ctree),"tree_move",G_CALLBACK(on_bookmark_ctree_move),bwin);
  
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
  gtk_entry_set_text(GTK_ENTRY(bwin->url_entry)," ");
  
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
  g_signal_connect(G_OBJECT(bwin->add_button),"clicked",G_CALLBACK(on_bookmark_add_button_clicked),bwin);
  
  /* edit button */
  bwin->edit_button = gtk_button_new_with_label("Edit");
  gtk_widget_show(bwin->edit_button);
  gtk_box_pack_start(GTK_BOX(bwin->hbox2),bwin->edit_button,FALSE,FALSE,0);
  g_signal_connect(G_OBJECT(bwin->edit_button),"clicked",G_CALLBACK(on_bookmark_edit_button_clicked),bwin);
  
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

/* read gBookmarkNodes */
void read_bookmark()
{
  gchar *line;
  BookmarkData *data;
  GNode *parent;
  
  line = (gchar *)g_malloc(1024);
  
  parent= gBookmarkNodes;
  
  while(fgets(line,1023,gBookmakeFilePath)!= NULL)
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
  if (g_ascii_strcasecmp(data->url," ") == 0)
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
void on_bookmark_ctree_select_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin)
{
  BookmarkData *bmark;
  
  bmark = (BookmarkData*) gtk_ctree_node_get_row_data(GTK_CTREE(bwin->ctree), node);
  
  if (g_ascii_strcasecmp(bmark->url," ") == 0)
    gtk_entry_set_text(GTK_ENTRY(bwin->folder_entry), bmark->label);
  else
  {
    gtk_entry_set_text(GTK_ENTRY(bwin->text_entry), bmark->label);
    gtk_entry_set_text(GTK_ENTRY(bwin->url_entry), bmark->url);
  } 
  
  bwin->temp_node= g_node_find(gBookmarkNodes,G_IN_ORDER,G_TRAVERSE_ALL,bmark);
  if (G_NODE_IS_LEAF(bwin->temp_node))
  {
    bwin->parent_node= bwin->temp_node->parent;
    bwin->menu_node= node;
  }
  else
  {
    bwin->parent_node= bwin->temp_node;
    bwin->menu_node= node;
  }
}

/* there isn't a selected raw on ctree */
void on_bookmark_ctree_unselect_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin)
{	
  bwin->menu_node= gtk_ctree_node_nth(GTK_CTREE(bwin->ctree),0);
  gtk_ctree_unselect_recursive(GTK_CTREE(bwin->ctree),gtk_ctree_node_nth (GTK_CTREE(bwin->ctree),0));
  gtk_entry_set_text(GTK_ENTRY(bwin->text_entry),"");
  gtk_entry_set_text(GTK_ENTRY(bwin->url_entry)," ");
  gtk_entry_set_text(GTK_ENTRY(bwin->folder_entry),"");
}

/* move a node */
void on_bookmark_ctree_move(GtkWidget *ctree,GtkCTreeNode *node,GtkCTreeNode *parent,GtkCTreeNode *sibling,BookmarkWindow *bwin)
{
  BookmarkData *data, *parent_data;
  GNode *menu_node, *old_node;
  
  data= (BookmarkData *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree),node);
  
  if (parent== NULL) return;
  
  old_node= g_node_find(gBookmarkNodes,G_IN_ORDER,G_TRAVERSE_ALL,data);
  
  parent_data= (BookmarkData *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree),parent);
  menu_node= g_node_find(gBookmarkNodes, G_IN_ORDER, G_TRAVERSE_ALL, parent_data);
  
  if (old_node->children!= NULL)
    move_folder(old_node,menu_node);
  else
    g_node_append_data(menu_node,old_node->data);
  
  g_node_destroy(old_node);
  
}

/* move a folder and its contents */
void move_folder(GNode *old_node, GNode *new_parent_node)
{
  
  if (old_node== NULL) return;
  else if  (old_node->children!= NULL) 
  {
    new_parent_node= g_node_append_data(new_parent_node,old_node->data);
    move_folder(old_node->children,new_parent_node);
  }
  else if (old_node->children== NULL)
  {
    g_node_append_data(new_parent_node,old_node->data);
    move_folder(old_node->next,new_parent_node);
  }	
}

/* add a bookmark */
void on_bookmark_add_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{
  BookmarkData *data;
  gchar *ctree_entry[2];
  GtkCTreeNode *node;
  
  data = g_new0(BookmarkData,1);
  node= NULL;
  
  data->url = ctree_entry[1] = g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry)));
  
  if (((GtkCTreeRow *)(((GList *)(bwin->menu_node))->data))->is_leaf)
    bwin->menu_node= ((GtkCTreeRow *)(((GList *)(bwin->menu_node))->data))->parent;
  
  /* it's a url */
  if (g_ascii_strcasecmp(data->url," ")!=0)
  {
    data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
    node = gtk_ctree_insert_node(GTK_CTREE(bwin->ctree),bwin->menu_node,NULL,ctree_entry,0,NULL,NULL,NULL,NULL,TRUE,FALSE);
  }
  else /* it's a folder */
  {
    data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->folder_entry))));
    if (g_ascii_strcasecmp(data->label,"")==0)
      data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
    node = gtk_ctree_insert_node(GTK_CTREE(bwin->ctree),bwin->menu_node,NULL,ctree_entry,0,NULL,NULL,NULL,NULL,FALSE,TRUE);
  }
  
  gtk_ctree_node_set_row_data(GTK_CTREE(bwin->ctree),node,data);
  g_node_append_data(bwin->parent_node,data);
  
  clear_entries(bwin);
}

/* add a folder */
void on_bookmark_edit_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{
  BookmarkData *data;
  gchar *ctree_entry[2];
  GNode *parent;
  data = g_new0(BookmarkData,1);
  
  data->label = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
  data->url = ctree_entry[1]= g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry)));
  
  /* it's a folder */
  if (g_ascii_strcasecmp(data->url," ")==0)
  {
    data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->folder_entry))));
    if (g_ascii_strcasecmp(data->label,"")==0)
      data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
    gtk_ctree_set_node_info(GTK_CTREE(bwin->ctree),bwin->menu_node,ctree_entry[0],0,NULL,NULL,NULL,NULL,FALSE,TRUE);
  }
  else /* it's a url */
  {
    data->label = ctree_entry[0] = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->text_entry))));
    gtk_ctree_set_node_info(GTK_CTREE(bwin->ctree),bwin->menu_node,ctree_entry[0],0,NULL,NULL,NULL,NULL,TRUE,FALSE);
  }
  
  gtk_ctree_node_set_row_data(GTK_CTREE(bwin->ctree),bwin->menu_node,data);
  
  parent= g_node_append_data(bwin->temp_node->parent,data);
  
  if (bwin->temp_node->children!= NULL)
    move_folder(bwin->temp_node->children,parent);
  
  g_node_destroy(bwin->temp_node);
  
  clear_entries(bwin);
}

/* a button to go to a selected url */
void on_bookmark_go_button_clicked(GtkButton *button,BookmarkWindow *bwin)
{
  GList *selection;
  gchar *url;
  
  /* case there isn't a selected url */
  if (!(selection = GTK_CLIST(bwin->ctree)->selection)) return;
  
  url= g_strdup(gtk_entry_get_text(GTK_ENTRY(bwin->url_entry)));
  
  /* it isn't a folder */
  if (g_ascii_strcasecmp(url," ") != 0)
  {
    gtk_moz_embed_stop_load(GTK_MOZ_EMBED(gBookmarkMozEmbed));
    gtk_moz_embed_load_url(GTK_MOZ_EMBED(gBookmarkMozEmbed), url);
  }
  clear_entries(bwin);
}

/* accept new edit configurations and update bookmark menu */
void on_bookmark_ok_button_clicked(GtkWidget *button, BookmarkWindow *bwin)
{
  print_bookmarks ();
  close_bookmark_window(bwin);
}

/* cancel user's operations */
void on_bookmark_cancel_button_clicked(GtkWidget *button,BookmarkWindow *bwin)
{	
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
  
  menu_node= g_node_find(gBookmarkNodes, G_IN_ORDER, G_TRAVERSE_ALL, data);
  
  g_node_destroy(menu_node);
  
  clear_entries(bwin);
}

/* write bookmarks on file */
void print_bookmarks ()
{
  fclose(gBookmakeFilePath);
  gBookmakeFilePath = fopen(gBookmakeFile,"w");
  
  if (gBookmarkNodes != NULL)
    g_node_children_foreach(gBookmarkNodes,G_TRAVERSE_ALL,(GNodeForeachFunc)print_node_data,gBookmakeFilePath);
}

/* print node data on gBookmakeFile */
void print_node_data (GNode *node,FILE *gBookmakeFilePath)
{
  BookmarkData *data;
  
  data = (BookmarkData*) node->data;
  
  /* it's a url */
  if (g_ascii_strcasecmp(data->url," ") != 0)
    fprintf(gBookmakeFilePath,"url %s %s\n",data->url,data->label);
  else /* it's a folder */
  {
    fprintf(gBookmakeFilePath,"folder %s\n",data->label);
    g_node_children_foreach(node,G_TRAVERSE_ALL,(GNodeForeachFunc)print_node_data,gBookmakeFilePath);
    fprintf(gBookmakeFilePath,"/folder\n");
  }
}

/* clear all the gBookmarkNodes entries */
void clear_entries(BookmarkWindow *bwin)
{
  bwin->menu_node= gtk_ctree_node_nth(GTK_CTREE(bwin->ctree),0);
  bwin->parent_node= g_node_get_root(gBookmarkNodes);
  gtk_ctree_unselect_recursive(GTK_CTREE(bwin->ctree),gtk_ctree_node_nth (GTK_CTREE(bwin->ctree),0));
  gtk_entry_set_text(GTK_ENTRY(bwin->url_entry)," ");
  gtk_editable_delete_text(GTK_EDITABLE(bwin->text_entry),0 ,-1);
  gtk_editable_delete_text(GTK_EDITABLE(bwin->folder_entry),0 ,-1);
}

/* close the bookmark window */
void close_bookmark_window(BookmarkWindow *bwin)
{	
  /* close bookmark gBookmakeFile*/
  fclose(gBookmakeFilePath);
  
  gtk_widget_destroy(bwin->window);
  g_free(bwin);
}

/* export gBookmarkNodes callback */
void export_gBookmarkNodes(GtkButton *button,BookmarkWindow *bwin)
{
  
}

/* the menu button to add a bookmark */
void add_bookmark_cb(GtkWidget *menu_item,GtkWidget *embed)
{
  gchar *url;
  gchar *title;
  
  gBookmakeFile = g_strconcat(g_get_home_dir(),"/.Minimo/bookmark",NULL);
  if (!(gBookmakeFilePath = fopen(gBookmakeFile,"a+"))) return;
  
  if (!(url= gtk_moz_embed_get_location(gBookmarkMozEmbed))) return;
  
  /* doesn't add an empty url or about:blank */
  if ((g_ascii_strcasecmp(url," ") != 0) && (g_ascii_strcasecmp(url,"about:blank") != 0))
  {
    title= gtk_moz_embed_get_title (gBookmarkMozEmbed);
    fprintf(gBookmakeFilePath, "url %s %s\n", url, title);
  }
  
  fclose(gBookmakeFilePath);	
}
