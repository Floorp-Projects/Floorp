/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  g-editor-view.c -- editor views.
  Created: Chris Toshok <toshok@hungry.com>, 18-Jul-98.
*/

#include "g-editor-view.h"

void 
moz_editor_view_init(MozEditorView *view, MozFrame *parent_frame, MWContext *context)
{
  /* call our superclass's init */
  moz_html_view_init(MOZ_VIEW(view), parent_frame, context);

  /* then do our stuff. */
  moz_tagged_set_type(MOZ_TAGGED(view),
		      MOZ_TAG_EDITOR_VIEW);
}

void 
moz_editor_view_deinit(MozEditorView *view)
{
  /* do our stuff. */

  /* then call our superclass's deinit */
  moz_html_view_deinit(MOZ_HTML_VIEW(view));
}

MozEditorView*
moz_editor_view_create(MozFrame *parent_frame, MWContext *context)
{
  MozEditorView *view;

  view = XP_NEW_ZAP(MozEditorView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  /* if context == NULL, then we should create a new context.
     this is used for grid cells. */
  moz_editor_view_init(view, parent_frame, context);

  return view;
}

void
moz_editor_view_display_linefeed(MozEditorView *view,
				 LO_LinefeedStruct *line_feed,
				 XP_Bool need_bg)
{
  XP_ASSERT(0);
  printf("moz_editor_view_display_linefeed (empty)\n");
}
