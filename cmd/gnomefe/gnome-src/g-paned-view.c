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
  g-paned-view.c -- paned views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#include "xp_mem.h"
#include "g-paned-view.h"

void
moz_paned_view_init(MozPanedView *view,
		    MozFrame *parent_frame,
		    MWContext *context,
		    XP_Bool horizontal)
{
  /* call our superclass's init */
  moz_view_init(MOZ_VIEW(view), parent_frame, context);
  
  /* then do our stuff */
  moz_tagged_set_type(MOZ_TAGGED(view),
		      MOZ_TAG_PANED_VIEW);

  view->paned = horizontal ? gtk_hpaned_new() : gtk_vpaned_new();

  view->horizontal = horizontal;

  moz_component_set_basewidget(MOZ_COMPONENT(view), view->paned);
}

void
moz_paned_view_deinit(MozPanedView *view)
{
  /* do our stuff. */

  /* then call our superclass's deinit */
  moz_view_deinit(MOZ_VIEW(view));
}

MozPanedView*
moz_paned_view_create_horizontal(MozFrame *parent_frame,
				 MWContext *context)
{
  MozPanedView* view;

  view = XP_NEW_ZAP(MozPanedView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  moz_paned_view_init(view, parent_frame, context, TRUE);
  return view;
}

MozPanedView*
moz_paned_view_create_vertical(MozFrame *parent_frame,
			       MWContext *context)
{
  MozPanedView* view;

  view = XP_NEW_ZAP(MozPanedView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  moz_paned_view_init(view, parent_frame, context, FALSE);
  return view;
}

void
moz_paned_view_add_view1(MozPanedView *parent_view,
                         MozView *child_view)
{
  gtk_paned_add1(GTK_PANED(parent_view->paned),
                 MOZ_COMPONENT(child_view)->base_widget);
  
  /* XXX mostly copied from moz_view_add_view */
  child_view->parent_view = MOZ_VIEW(parent_view);
  MOZ_VIEW(parent_view)->subviews = g_list_prepend(MOZ_VIEW(parent_view)->subviews, child_view);
}

void
moz_paned_view_add_view2(MozPanedView *parent_view,
                         MozView *child_view)
{
  gtk_paned_add2(GTK_PANED(parent_view->paned),
                 MOZ_COMPONENT(child_view)->base_widget);
  
  /* XXX mostly copied from moz_view_add_view */
  child_view->parent_view = MOZ_VIEW(parent_view);
  MOZ_VIEW(parent_view)->subviews = g_list_prepend(MOZ_VIEW(parent_view)->subviews, child_view);
}
