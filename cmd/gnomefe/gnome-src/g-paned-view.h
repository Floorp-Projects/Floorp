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
