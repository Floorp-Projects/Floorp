/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*
  g-commands.c -- toolbar callbacks.
  Created: Chris Toshok <toshok@hungry.com>, 13-Apr-98.
*/

#include "g-commands.h"

void
moz_open_bookmarks(GtkWidget *widget, gpointer client_data)
{
  MozFrame *frame = (MozFrame*)moz_bookmark_frame_create();

  moz_component_show(MOZ_COMPONENT(frame));

  moz_frame_raise(frame);
}

void
moz_open_history(GtkWidget *widget, gpointer client_data)
{
  MozFrame *frame = (MozFrame*)moz_history_frame_create();

  moz_component_show(MOZ_COMPONENT(frame));

  moz_frame_raise(frame);
}

void
moz_open_browser(GtkWidget *widget, gpointer client_data)
{
  MozFrame *frame = (MozFrame*)moz_browser_frame_create();

  moz_component_show(MOZ_COMPONENT(frame));
}

void
moz_open_editor(GtkWidget *widget, gpointer client_data)
{
  MozFrame *frame = (MozFrame*)moz_editor_frame_create();

  moz_component_show(MOZ_COMPONENT(frame));
}

void
moz_exit(GtkWidget *widget, gpointer client_data)
{
  gtk_main_quit();
}
