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
  g-history-frame.c -- history windows.
  Created: Chris Toshok <toshok@hungry.com>, 13-Apr-98.
*/

#include "xp_mem.h"
#include "structs.h"
#include "ntypes.h"
#include "g-commands.h"
#include "g-history-frame.h"
#include "g-history-view.h"

static void
callback(GtkWidget *widget,
	 gpointer client_data)
{
  /* blah */
}

static GnomeUIInfo file_submenu[] = {
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo edit_submenu[] = {
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo view_submenu[] = {
  { GNOME_APP_UI_ENDOFINFO }
};

static GnomeUIInfo window_submenu[] = {
  { GNOME_APP_UI_ITEM, "Navigation Center", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_ITEM, "Navigator", NULL, moz_open_browser, NULL, NULL },

#ifdef EDITOR
  { GNOME_APP_UI_ITEM, "Composer", NULL, callback, NULL, NULL },
#endif
  { GNOME_APP_UI_SEPARATOR },

  { GNOME_APP_UI_ITEM, "Bookmarks", NULL, moz_open_bookmarks, NULL, NULL },

  { GNOME_APP_UI_ITEM, "History", NULL, moz_open_history, NULL, NULL },

  { GNOME_APP_UI_ITEM, "View Security", NULL, callback, NULL, NULL },

  { GNOME_APP_UI_SEPARATOR },
  
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
  { GNOME_APP_UI_SUBTREE, "Window", NULL, window_submenu },
  { GNOME_APP_UI_SUBTREE, "Help", NULL, help_submenu },
  { GNOME_APP_UI_ENDOFINFO }
};

void
moz_history_frame_init(MozHistoryFrame *frame)
{
  /* call our superclass's init method first. */
  moz_frame_init(MOZ_FRAME(frame),
                 menubar_info,
                 NULL);

  /* then do our stuff */
  moz_tagged_set_type(MOZ_TAGGED(frame),
                      MOZ_TAG_HISTORY_FRAME);
}

void
moz_history_frame_deinit(MozHistoryFrame *frame)
{
  printf("moz_history_frame_deinit (empty)\n");
}

/* our one history frame. */
static MozHistoryFrame* singleton = NULL;

MozHistoryFrame*
moz_history_frame_create()
{
  if (!singleton)
    {
      MozHistoryView *view;
      
      singleton = XP_NEW_ZAP(MozHistoryFrame);

      moz_history_frame_init(singleton);
      
      MOZ_FRAME(singleton)->context->type = MWContextHistory;
      
      gtk_widget_realize(MOZ_COMPONENT(singleton)->base_widget);
      
      view = moz_history_view_create(MOZ_FRAME(singleton), MOZ_FRAME(singleton)->context);
      
      MOZ_FRAME(singleton)->top_view = MOZ_VIEW(view);
      
      gtk_widget_show(MOZ_COMPONENT(view)->base_widget);
      
      moz_frame_set_viewarea(MOZ_FRAME(singleton),
                             MOZ_COMPONENT(view)->base_widget);
      
      gtk_widget_set_usize(MOZ_COMPONENT(singleton)->base_widget,
                           300, 400); /* XXX save off the default history window size. */
    }

  return singleton;
}
  
