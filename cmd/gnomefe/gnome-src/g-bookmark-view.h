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
  g-bookmark-view.h -- bookmark views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#ifndef _moz_bookmark_view_h
#define _moz_bookmark_view_h

#include "g-view.h"

struct _MozBookmarkView {
  /* our superclass */
  MozView _view;

  GtkWidget *ctree;
};

extern void		moz_bookmark_view_init(MozBookmarkView *view, MozFrame *parent_frame, MWContext *context);
extern void		moz_bookmark_view_deinit(MozBookmarkView *view);

extern MozBookmarkView*	moz_bookmark_view_create(MozFrame *parent_frame, MWContext *context);

extern void		moz_bookmark_view_refresh_cells(MozBookmarkView *view, int32 first, int32 last, XP_Bool now);
extern void		moz_bookmark_view_scroll_into_view(MozBookmarkView *view, BM_Entry *entry);

#endif _moz_bookmark_view_h 
