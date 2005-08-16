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

#ifndef minimo_types_h
#define minimo_types_h

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>

#include "gtkmozembed.h"

/* MINIMO TOOLBAR STRUCT */
typedef struct _MinimoToolBar {
	GtkWidget  *toolbar;

	GtkWidget *OpenButton;
	GtkWidget *BackButton;
	GtkWidget *ReloadButton;
	GtkWidget *ForwardButton;
	GtkWidget *StopButton;
	GtkWidget *PrefsButton;
	GtkWidget *InfoButton;
	GtkWidget *QuitButton;  
} MinimoToolbar;

/* MINIMO STRUCT */
typedef struct _MinimoBrowser {
  
	GtkWidget  *topLevelWindow;
	GtkWidget  *topLevelVBox;
	GtkWidget  *mozEmbed;
	
	MinimoToolbar toolbar;
	
	gboolean    show_tabs;
	GtkWidget  *notebook;
	GtkWidget  *label_notebook;
	GtkWidget  *active_page;
	
	GtkWidget  *progressPopup;
	GtkWidget  *progressBar;
	gint        totalBytes;
	
	GtkWidget  *link_menu;
	GtkWidget  *image_menu;
	GtkWidget  *doc_menu;
	GtkWidget  *image_link_menu;
	GtkWidget  *reload_menu;

	gboolean didFind;
  
} MinimoBrowser;

typedef struct _PrefWindow 		/* configuration data struct */
{
	GtkWidget *dialog;
	GtkWidget *okbutton;
	GtkWidget *cancelbutton;
	GtkWidget *applybutton;
	GtkWidget *block_popup;
	GtkWidget *vbox_popup;
	GtkWidget *fr_connection;
	GtkWidget *vbox_connection;
	GtkWidget *hbox_connection;
	GtkWidget *hbox_direct_connection;
	GtkWidget *fr_popup;
	GtkWidget *lb_popup;

	GtkWidget *vbox_manual;
	GtkWidget *rb_manual_connection;
	GtkWidget *hbox_http;
	GtkWidget *lb_http;
	GtkWidget *en_http_proxy;
	GtkWidget *box_popup;

	GtkWidget *lb_port_http;
	GtkWidget *en_http_port;
	GtkWidget *hbox_ssl;
	GtkWidget *lb_ssl;
	GtkWidget *en_ssl;
	GtkWidget *lb_ssl_port;
	GtkWidget *en_ssl_port;
	GtkWidget *hbox_ftp;
	
	GtkWidget *lb_ftp;
	GtkWidget *en_ftp_proxy;
	GtkWidget *lb_ftp_port;
	GtkWidget *en_ftp_port;
	GtkWidget *hbox_noproxy;
	GtkWidget *lb_connection;

	GSList *rb_direct_connection_group;
	GSList *rb_manual_connection_group;
	GtkWidget *hbox_manual_connection;
	GtkWidget *rb_direct_connection;

} PrefWindow;

/* SPECIFIC STRUCT FOR SPECIFY SOME PARAMETERS OF '_open_dialog_params' METHOD */
typedef struct _open_dialog_params
{
	GtkWidget* dialog_combo;
	GtkWidget* main_combo;
} OpenDialogParams;

/* SPECIFIC STRUCT FOR THE RIGHT CLICK BUTTON EMULATION */
typedef struct _right_button_click
{
	GTimer *pressing_timer;
	gulong sig_handler;
} RightButtonClick;


/****************/
/* PREFERENCE.H */
/****************/

#define LANG_FONT_NUM 15
#define DEFAULT_FONT_SIZE 10
#define DEFAULT_MIN_FONT_SIZE 5
#define DEFAULT_SERIF_FONT     "-adobe-times-medium-r-normal-*-14-*-*-*-p-*-iso8859-1"
#define DEFAULT_SANSSERIF_FONT "-adobe-times-medium-r-normal-*-14-*-*-*-p-*-iso8859-1"
#define DEFAULT_CURSIVE_FONT   "-adobe-times-medium-r-normal-*-14-*-*-*-p-*-iso8859-1"
#define DEFAULT_FANTASY_FONT   "-adobe-times-medium-r-normal-*-14-*-*-*-p-*-iso8859-1"
#define DEFAULT_MONOSPACE_FONT "-adobe-times-medium-r-normal-*-14-*-*-*-p-*-iso8859-1"

static const char *lang_font_item [LANG_FONT_NUM] =
{
        "x-western",
        "x-central-euro",
        "ja",
        "zh-TW",
        "zh-CN",
        "ko",
        "x-cyrillic",
        "x-baltic",
        "el",
        "tr",
        "x-unicode",
        "x-user-def",
        "th",
        "he",
        "ar"
};

/**
* configuration data struct
*/
typedef struct _ConfigData 		/* configuration data struct */
{
	/* minimo prefs */
     	gchar *home;
	gchar *mailer;
     	gint xsize;
	gint ysize;
	gint layout;
	gint maxpopupitems;
	gint max_go;     
     	/* mozilla prefs */
     	gchar *http_proxy;
     	gchar *http_proxy_port;
	gchar *ftp_proxy;
	gchar *ftp_proxy_port;
     	gchar *ssl_proxy;
     	gchar *ssl_proxy_port;
     	gint font_size[LANG_FONT_NUM];
     	gint min_font_size[LANG_FONT_NUM];
	gint current_font_size;
     	gboolean java;
     	gboolean javascript;
     	gboolean underline_links;
     	gchar *no_proxy_for;
	gint direct_connection;
     	gint popup_in_new_window;
     	gint disable_popups;
     	gint tab_text_length;

} ConfigData;

/****************/
/* HISTORY.H    */
/****************/

/* History Window Structure */
typedef struct _HistoryWindow{
        GtkWidget *window;
	GtkWidget *scrolled_window;
	GtkWidget *clist;
	GtkWidget *vbox;
	GtkWidget *remove;
	GtkWidget *close;
	GtkWidget *btnbox;
	GtkWidget *clear;
     	GtkWidget *search_label;
     	GtkWidget *search_entry;
     	GtkWidget *search_box;
     	GtkWidget *search_button;
     	GtkWidget *go_button;
     	GtkWidget *embed;
     	gchar *title;
}HistoryWindow;

/****************/
/*  BOOKMARK.H  */
/****************/


/* represent a bookmark item */
typedef struct _BookmarkData {
	gchar *label;
	gchar *url;
} BookmarkData;

/* Represents Bookmarks Tree View*/
typedef struct _BookmarkTreeVData {
	GtkTreeModel *treeStore;
	GtkTreeIter *parentIter;
} BookmarkTreeVData;

/* Bookmarks Window Structure */
typedef struct _BookmarkWindow {
	FILE *bookmark_file;
	GtkWidget *window;
	GtkWidget *menubar;
	GtkWidget *menu_item_tools;
	GtkWidget *menu_tools;
	GtkWidget *menu_import;
	GtkWidget *menu_export;
	GtkWidget *scrolled_window;
	GtkWidget *vbox1;
	GtkWidget *hbox4;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *hbox3;
	GtkWidget *text_label;
	GtkWidget *text_entry;
	GtkWidget *url_label;
	GtkWidget *url_entry;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *folder_entry;
	GtkWidget *remove_button;
	GtkWidget *ok_button;
	GtkWidget *go_button;
	GtkWidget *cancel_button;
	GtkWidget *treev;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	BookmarkTreeVData *treev_data;

} BookmarkWindow;

#endif
