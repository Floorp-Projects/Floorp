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

#ifndef _moz_commands_h
#define _moz_commands_h

#include "g-types.h"
#include "g-bookmark-frame.h"
#include "g-editor-frame.h"
#include "g-browser-frame.h"

extern void moz_open_bookmarks(GtkWidget *widget, gpointer client_data);
extern void moz_open_history(GtkWidget *widget, gpointer client_data);
extern void moz_open_browser(GtkWidget *widget, gpointer client_data);
extern void moz_open_editor(GtkWidget *widget, gpointer client_data);
extern void moz_exit(GtkWidget *widget, gpointer client_data);
#endif /* _moz_commands_h */
