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
  g-paned-view.h -- paned views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#ifndef _moz_paned_view_h
#define _moz_paned_view_h

#include "g-view.h"

struct _MozPanedView {
  /* our superclass */
  MozView _view;

  GtkWidget* paned;
  XP_Bool horizontal;
};

extern void		moz_paned_view_init(MozPanedView *view, MozFrame *parent_frame, MWContext *context, XP_Bool horizontal);
extern void		moz_paned_view_deinit(MozPanedView *view);

extern MozPanedView*	moz_paned_view_create_horizontal(MozFrame *parent_frame, MWContext *context);
extern MozPanedView*	moz_paned_view_create_vertical(MozFrame *parent_frame, MWContext *context);

extern void		moz_paned_view_add_view1(MozPanedView *parent_view, MozView *child_view);
extern void		moz_paned_view_add_view2(MozPanedView *parent_view, MozView *child_view);

extern MozView*		moz_paned_view_get_view1(MozPanedView *paned_view);
extern MozView*		moz_paned_view_get_view2(MozPanedView *paned_view);

#endif _moz_bookmark_view_h 
