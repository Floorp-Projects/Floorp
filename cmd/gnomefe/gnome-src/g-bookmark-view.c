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
  g-bookmark-view.c -- bookmark views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#include "xp_mem.h"
#include "g-bookmark-view.h"

/* XXX I18N */
static char *column_titles[] = {
  "Name",
  "Location",
  "Last Visited",
  "Created On"
};
static int num_column_titles = sizeof(column_titles) / sizeof(column_titles[0]);

void
moz_bookmark_view_init(MozBookmarkView *view,
		       MozFrame *parent_frame,
		       MWContext *context)
{
  /* call our superclass's init */
  moz_view_init(MOZ_VIEW(view), parent_frame, context);

  /* then do our stuff */
  moz_tagged_set_type(MOZ_TAGGED(view),
		      MOZ_TAG_BOOKMARK_VIEW);

#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 1)
  view->ctree = gtk_ctree_new_with_titles(num_column_titles, 0, column_titles);
#else
  view->ctree = gtk_clist_new_with_titles(num_column_titles, column_titles);
#endif

  moz_component_set_basewidget(MOZ_COMPONENT(view), view->ctree);
}

void
moz_bookmark_view_deinit(MozBookmarkView *view)
{
  /* do our stuff. */

  /* then call our superclass's deinit */
  moz_view_deinit(MOZ_VIEW(view));
}

MozBookmarkView*
moz_bookmark_view_create(MozFrame *parent_frame,
			 MWContext *context)
{
  MozBookmarkView* view;

  view = XP_NEW_ZAP(MozBookmarkView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  moz_bookmark_view_init(view, parent_frame, context);
  return view;
}

void
moz_bookmark_view_refresh_cells(MozBookmarkView *view,
				int32 first,
				int32 last,
				XP_Bool now)
{
  printf("moz_bookmark_view_refresh_cells (empty)\n");
}

void
moz_bookmark_view_scroll_into_view(MozBookmarkView *view,
				   BM_Entry *entry)
{
  printf("moz_bookmark_view_scroll_into_view (empty)\n");
}
