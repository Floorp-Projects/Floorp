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
  g-history-view.c -- history views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#include "xp_mem.h"
#include "g-history-view.h"

/* XXX I18N */
static char *column_titles[] = {
  "Title",
  "Location",
  "Last Visited",
  "Expires",
  "Visit Count"
};
static int num_column_titles = sizeof(column_titles) / sizeof(column_titles[0]);

void
moz_history_view_init(MozHistoryView *view,
		       MozFrame *parent_frame,
		       MWContext *context)
{
  /* call our superclass's init */
  moz_view_init(MOZ_VIEW(view), parent_frame, context);

  /* then do our stuff */
  moz_tagged_set_type(MOZ_TAGGED(view),
		      MOZ_TAG_HISTORY_VIEW);

  view->clist = gtk_clist_new_with_titles(num_column_titles, column_titles);

  moz_component_set_basewidget(MOZ_COMPONENT(view), view->clist);
}

void
moz_history_view_deinit(MozHistoryView *view)
{
  /* do our stuff. */

  /* then call our superclass's deinit */
  moz_view_deinit(MOZ_VIEW(view));
}

MozHistoryView*
moz_history_view_create(MozFrame *parent_frame,
			 MWContext *context)
{
  MozHistoryView* view;

  view = XP_NEW_ZAP(MozHistoryView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  moz_history_view_init(view, parent_frame, context);
  return view;
}
