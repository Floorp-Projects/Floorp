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

typedef enum
{
        NS_SITE,
        NS_NOTES,
        NS_FOLDER,
        NS_FOLDER_END,
        NS_SEPARATOR,
        NS_UNKNOWN
} NSItemType;

void 	bookmark_moz_embed_initialize		(GtkWidget *);
void 	bookmark_create_dialog 			();
void 	bookmark_open_file 			();
void 	bookmark_load_from_file			(BookmarkTreeVData *);
gboolean bookmark_search_function(GtkTreeModel *, gint, const gchar *, GtkTreeIter *,void *);
void 	bookmark_insert_folder_on_treeview	(BookmarkData *, BookmarkTreeVData *);
void 	bookmark_insert_item_on_treeview	(BookmarkData *, BookmarkTreeVData *);
void 	bookmark_on_treev_select_row_cb 	(GtkTreeView *, BookmarkWindow *);
void 	bookmark_add_button_cb			(GtkWidget *,BookmarkWindow *);
void 	bookmark_edit_button_cb 		(GtkWidget * ,BookmarkWindow *);
void 	bookmark_go_button_cb 			(GtkButton * ,BookmarkWindow *);
void 	bookmark_ok_button_cb 			(GtkWidget * ,BookmarkWindow *);
void 	bookmark_cancel_button_cb 		(GtkWidget * ,BookmarkWindow *);
void 	bookmark_remove_button_cb 		(GtkWidget * ,BookmarkWindow *);
void 	bookmark_write_on_file 			(BookmarkWindow *);
void 	bookmark_write_node_data_on_file 	(GtkTreeModel *, GtkTreeIter *);
void 	bookmark_clear_all_entries		(BookmarkWindow *);
void 	bookmark_close_dialog			(BookmarkWindow *);
void 	bookmark_import_cb			(GtkButton *,BookmarkWindow *);
NSItemType bookmark_get_ns_item 		(FILE *, GString *, GString *);
gchar * bookmark_read_line_from_html_file 	(FILE *);
const gchar *bookmark_string_strcasestr 	(const gchar *, const gchar *);
char  * bookmark_ns_parse_item 			(GString *);
void 	bookmark_export_cb 			(GtkButton *,BookmarkWindow *);
void 	bookmark_export_items 			(FILE *, BookmarkData *, gboolean );
void 	bookmark_add_url_directly_cb 		(GtkWidget *,GtkWidget *);

#endif
