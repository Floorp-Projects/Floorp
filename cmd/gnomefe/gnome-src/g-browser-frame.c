/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
  browser-frame.c -- browser windows.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#include "xp_mem.h"
#include "structs.h"
#include "ntypes.h"
#include "net.h"

#include "g-util.h"
#include "g-browser-frame.h"
#include "g-commands.h"

#include "g-paned-view.h"
#include "g-html-view.h"
#include "g-navcenter-view.h"

static void
callback(GtkWidget *widget,
         gpointer client_data)
{
  /* blah */
}

static void
toggle_navcenter(GtkWidget *widget,
                 gpointer client_data)
{
  MozBrowserFrame *browser_frame = MOZ_BROWSER_FRAME(client_data);
  MozView *view = MOZ_FRAME(browser_frame)->top_view;
  MozNavCenterView *ncview = (MozNavCenterView*)moz_get_view_of_type(view,
                                                                     MOZ_TAG_NAVCENTER_VIEW);

  if (moz_component_is_shown(MOZ_COMPONENT(ncview)))
    moz_component_hide(MOZ_COMPONENT(ncview));
  else
    moz_component_show(MOZ_COMPONENT(ncview));
}


static GnomeUIInfo file_submenu[] = {
  { GNOME_APP_UI_ITEM, "New Browser", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "Open Location", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Save...", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Save As...", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Save Frame As...", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Send Page", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Send Link", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

#ifdef EDITOR
  { GNOME_APP_UI_ITEM, "Edit Page", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Edit Frame", NULL, callback, NULL, NULL },
#endif

  { GNOME_APP_UI_ITEM, "Upload File", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Print", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Close", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Exit", NULL, moz_exit, NULL, NULL },

  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo edit_submenu[] = {
  { GNOME_APP_UI_ITEM, "Undo", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Redo", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Cut", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Copy", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Paste", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Select All", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Find In Page", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Find Again", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Search", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Preferences...", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo view_submenu[] = {
  { GNOME_APP_UI_ITEM, "Toggle Navigation Toolbar", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Toggle Location Toolbar", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Toggle Personal Toolbar", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Toggle Navigation Center", NULL, toggle_navcenter, NULL, NULL },
  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Increase Font", NULL, callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Decreasea Font", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Reload", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "Show Images", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "Refresh", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "Stop Loading", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "View Page Source", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "View Page Info", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "View Page Services", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo go_submenu[] = {
  { GNOME_APP_UI_ITEM, "Back", "Back to previous page", callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Forward", "Forward to next page", callback, NULL, NULL },
  { GNOME_APP_UI_ITEM, "Home", "Load home page", callback, NULL, NULL },
  { GNOME_APP_UI_SEPARATOR },
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo window_submenu[] = {
  { GNOME_APP_UI_ITEM, "Navigation Center", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "Navigator", NULL, moz_open_browser, NULL, NULL },

#ifdef EDITOR
  { GNOME_APP_UI_ITEM, "Composer", NULL, moz_open_editor, NULL, NULL },
#endif
  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Bookmarks", NULL, moz_open_bookmarks, NULL, NULL },

  { GNOME_APP_UI_ITEM, "History", NULL, moz_open_history, NULL, NULL },

#ifdef MOZ_SECURITY
  { GNOME_APP_UI_ITEM, "View Security", NULL, callback, NULL, NULL },
#endif

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo documents_submenu[] = {
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo help_submenu[] = {
  { GNOME_APP_UI_HELP, "HelpStuff", NULL, "GnuZilla" },
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo menubar_info[] = {
  { GNOME_APP_UI_SUBTREE, "File", NULL, file_submenu },
  { GNOME_APP_UI_SUBTREE, "Edit", NULL, edit_submenu },
  { GNOME_APP_UI_SUBTREE, "View", NULL, view_submenu },
  { GNOME_APP_UI_SUBTREE, "Go", NULL, go_submenu },
  { GNOME_APP_UI_SUBTREE, "Window", NULL, window_submenu },
  { GNOME_APP_UI_SUBTREE, "Documents", NULL, documents_submenu },
  { GNOME_APP_UI_SUBTREE, "Help", NULL, help_submenu },
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo toolbar_info[] = {
  { GNOME_APP_UI_ITEM,
    "Back", "Back to previous page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Back.xpm" },

  { GNOME_APP_UI_ITEM,
    "Forward", "Forward to next page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Forward.xpm" },

  { GNOME_APP_UI_ITEM,
    "Reload", "Reload current page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Reload.xpm" },

  { GNOME_APP_UI_ITEM,
    "Home", "Load home page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Home.xpm" },

  { GNOME_APP_UI_ITEM,
    "Search", "Search the Web",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Search.xpm" },

  { GNOME_APP_UI_SEPARATOR },

#ifdef EDITOR
  { GNOME_APP_UI_ITEM,
    "Edit", "Edit this page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_EditPage.xpm" },

  { GNOME_APP_UI_SEPARATOR },
#endif

  { GNOME_APP_UI_ITEM,
    "Images", "Show Images",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_LoadImages.xpm" },


  { GNOME_APP_UI_ITEM,
    "Print", "Print this page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Print.xpm" },

#ifdef MOZ_SECURITY
  { GNOME_APP_UI_ITEM,
    "Security", "View the Security Info for this page",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Secure.xpm" },
#endif

  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM,
    "Stop", "Stop Loading",
    callback, NULL, NULL,
    GNOME_APP_PIXMAP_FILENAME,
    "images/TB_Stop.xpm" },

  { GNOME_APP_UI_ENDOFINFO }
};

void
moz_browser_frame_init(MozBrowserFrame *frame)
{
  /* call our superclass's init method first. */
  moz_frame_init(MOZ_FRAME(frame),
                 menubar_info,
                 toolbar_info);

  /* then do our stuff */
  moz_tagged_set_type(MOZ_TAGGED(frame),
                      MOZ_TAG_BROWSER_FRAME);
}

extern void fe_url_exit (URL_Struct *url, int status, MWContext *context);

static void
combo_activate(GtkWidget *entry,
               MozBrowserFrame *frame)
{
  MWContext *context = MOZ_FRAME(frame)->context;
  gchar *value;
  URL_Struct *url;

  value = gtk_entry_get_text(GTK_ENTRY(entry));

  url = NET_CreateURLStruct(value, NET_NORMAL_RELOAD);

  NET_GetURL(url, FO_CACHE_AND_PRESENT,
             context, fe_url_exit);
}

static GtkWidget*
moz_browser_frame_create_bookmark_dropdown(MozBrowserFrame *frame)
{
  GtkWidget *hbox, *pmap, *button, *label;

  hbox = gtk_hbox_new(FALSE, 2);

  pmap = gnome_pixmap_new_from_file("images/BM_QFile.xpm");
  gtk_widget_show(pmap);
  gtk_box_pack_start(GTK_BOX(hbox), pmap, FALSE, FALSE, 0);

  label = gtk_label_new(_("Bookmarks"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

  button = gtk_button_new();

  gtk_widget_show(hbox);

  gtk_container_add(GTK_CONTAINER(button), hbox);

  return button;
}


static GtkWidget*
moz_browser_frame_create_browser_area(MozBrowserFrame *frame)
{
  GtkWidget *combobox, *vbox, *label, *quickfile;
  GtkWidget *hbox;
  GtkWidget *hb;
  GtkWidget *statusbar;
  GtkWidget *proxy;

  hb = gtk_handle_box_new();
  gtk_widget_show(hb);

  hbox = gtk_hbox_new(FALSE, 0);

  gtk_widget_show(hbox);

  vbox = gtk_vbox_new(FALSE, 0);

  quickfile = moz_browser_frame_create_bookmark_dropdown(frame);
  gtk_box_pack_start(GTK_BOX(hbox), quickfile, FALSE, FALSE, 0);
  gtk_widget_show(quickfile);

  proxy = gnome_pixmap_new_from_file("images/LocationProxy.xpm");
  gtk_box_pack_start(GTK_BOX(hbox), proxy, FALSE, FALSE, 0);
  gtk_widget_show(proxy);

  label = gtk_label_new(_("Location:"));
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  combobox = gtk_combo_new();
  gtk_combo_disable_activate(GTK_COMBO(combobox));
  gtk_box_pack_start(GTK_BOX(hbox), combobox, TRUE, TRUE, 0);
  gtk_widget_show(combobox);

  gtk_container_add(GTK_CONTAINER(hb), hbox);
  gtk_widget_show(hb);

  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combobox)->entry),
                     "activate",
                     (GtkSignalFunc)combo_activate, frame);

  gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox),
                     MOZ_COMPONENT(MOZ_FRAME(frame)->top_view)->base_widget,
                     TRUE, TRUE, 0);

  moz_component_show(MOZ_COMPONENT(MOZ_FRAME(frame)->top_view));

  gtk_widget_show(vbox);

  return vbox;
}

MozBrowserFrame*
moz_browser_frame_create()
{
  MozBrowserFrame *frame = XP_NEW_ZAP(MozBrowserFrame);
  MozPanedView *paned_view;
  MozNavCenterView *navcenter_view;
  MozHTMLView *html_view;

  moz_browser_frame_init(frame);

  MOZ_FRAME(frame)->context->type = MWContextBrowser;

  gtk_widget_realize(MOZ_COMPONENT(frame)->base_widget);

  paned_view = moz_paned_view_create_horizontal(MOZ_FRAME(frame), MOZ_FRAME(frame)->context);
  html_view = moz_html_view_create(MOZ_FRAME(frame), MOZ_FRAME(frame)->context);
#if NAVCENTER_VIEW  
  navcenter_view = moz_navcenter_view_create(MOZ_FRAME(frame), MOZ_FRAME(frame)->context);
#endif

  MOZ_FRAME(frame)->top_view = MOZ_VIEW(paned_view);
#if NAVCENTER_VIEW
  moz_paned_view_add_view1(paned_view, MOZ_VIEW(navcenter_view));
#endif
  moz_paned_view_add_view2(paned_view, MOZ_VIEW(html_view));

  moz_component_show(MOZ_COMPONENT(html_view));
  moz_component_show(MOZ_COMPONENT(paned_view));

  moz_frame_set_viewarea(MOZ_FRAME(frame),
                         moz_browser_frame_create_browser_area(frame));

  gtk_widget_set_usize(MOZ_COMPONENT(frame)->base_widget,
                       500, 700); /* XXX save off the default browser size. */

  return frame;
}
