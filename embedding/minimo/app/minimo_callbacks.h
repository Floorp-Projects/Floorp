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

#ifndef minimo_callbacks_h
#define minimo_callbacks_h

#include "minimo_context.h"
#include "minimo_types.h"
#include "minimo_preference.h"
#include "minimo_bookmark.h"

/* context menu callbacks */
void browse_in_window_from_popup(GtkWidget *menuitem, MinimoBrowser *browser);
void download_link_from_popup(GtkWidget *menuitem, MinimoBrowser *browser);
void minimo_load_url(gchar *url, MinimoBrowser *browser);
void make_tab_from_popup(GtkWidget *menuitem, MinimoBrowser *browser);
void open_new_tab_cb (GtkMenuItem *button, MinimoBrowser *browser);
void close_current_tab_cb (GtkMenuItem *button, MinimoBrowser *browser);
/* menu bar callbacks */
void open_bookmark_window_cb (GtkMenuItem *button,MinimoBrowser *browser);
void back_clicked_cb(GtkButton *button, MinimoBrowser *browser);
void reload_clicked_cb(GtkButton *button, MinimoBrowser *browser);
void forward_clicked_cb(GtkButton *button, MinimoBrowser *browser);
void stop_clicked_cb(GtkButton *button, MinimoBrowser *browser);
/* callbacks from the UI */
gboolean delete_cb(GtkWidget *widget, GdkEventAny *event, MinimoBrowser *browser);
/* callbacks from the widget */
void pref_clicked_cb(GtkButton *button, MinimoBrowser *browser);
void quick_message(gchar* message);
GtkWidget* create_minimo_progress (MinimoBrowser* browser);
void net_state_change_cb(GtkMozEmbed *embed, gint flags, guint status, GtkLabel* label);
void progress_change_cb(GtkMozEmbed *embed, gint cur, gint max, MinimoBrowser* browser);
void title_changed_cb(GtkMozEmbed *embed, MinimoBrowser *browser);
void load_started_cb(GtkMozEmbed *embed, MinimoBrowser *browser);
void load_finished_cb(GtkMozEmbed *embed, MinimoBrowser *browser);
void location_changed_cb(GtkMozEmbed *embed, GtkLabel* label);
void create_save_document(GtkMenuItem *button, MinimoBrowser *browser, gchar *location);
void on_save_ok_cb(GtkWidget *button, OpenDialogParams *fs);
void save_image_from_popup(GtkWidget *menuitem, MinimoBrowser *browser);
void visibility_cb(GtkMozEmbed *embed, gboolean visibility, MinimoBrowser *browser);
void set_browser_visibility(MinimoBrowser *browser, gboolean visibility);
gint open_uri_cb(GtkMozEmbed *embed, const char *uri, MinimoBrowser *browser);
void size_to_cb(GtkMozEmbed *embed, gint width, gint height, MinimoBrowser *browser);
void link_message_cb (GtkWidget *, MinimoBrowser *);

/* callbacks from buttons (mouse or stylus) clicks */
void on_button_clicked_cb(GtkWidget *, MinimoBrowser *);
gint on_button_pressed_cb(GtkWidget *, MinimoBrowser *);
gint on_button_released_cb(GtkWidget *, gpointer *, MinimoBrowser *);

#endif
