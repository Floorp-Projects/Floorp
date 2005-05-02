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

#ifndef bookmark_h
#define bookmark_h

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "minimo_types.h"

#include "gtkmozembed.h"

/* Callbacks from the UI */
void initialize_bookmark(GtkWidget *embed);
void open_bookmark(void);
void show_bookmark(void);
void read_bookmark(void);
void generate_bookmark_ctree(GNode *node, BookmarkCTreeData *ctree_data);
void add_bookmark_cb(GtkWidget *menu_item,GtkWidget *min);
void on_bookmark_add_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_edit_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_ok_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_cancel_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void on_bookmark_go_button_clicked(GtkButton *button,BookmarkWindow *bwin);
void on_bookmark_remove_button_clicked(GtkWidget *button,BookmarkWindow *bwin);
void export_bookmarks(GtkButton *button,BookmarkWindow *bwin);

/* Callbacks from widgets*/
void on_bookmark_ctree_select_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin);
void on_bookmark_ctree_unselect_row(GtkWidget *ctree,GtkCTreeNode *node,gint col,BookmarkWindow *bwin);
void on_bookmark_ctree_move(GtkWidget *ctree,GtkCTreeNode *node,GtkCTreeNode *parent,GtkCTreeNode *sibling,BookmarkWindow *bwin);
void move_folder(GNode *old_node, GNode *new_parent_node);
void print_bookmarks ();
void print_node_data (GNode *node,FILE *file);
void clear_entries(BookmarkWindow *bwin);
void close_bookmark_window(BookmarkWindow *bwin);
gboolean url_exists(BookmarkData *data);

#endif
