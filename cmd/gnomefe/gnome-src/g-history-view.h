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
  g-history-view.h -- history views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#ifndef _moz_history_view_h
#define _moz_history_view_h

#include "g-view.h"

struct _MozHistoryView {
  /* our superclass */
  MozView _view;

  GtkWidget *clist;
};

extern void		moz_history_view_init(MozHistoryView *view, MozFrame *parent_frame, MWContext *context);
extern void		moz_history_view_deinit(MozHistoryView *view);

extern MozHistoryView*	moz_history_view_create(MozFrame *parent_frame, MWContext *context);

#endif _moz_history_view_h 
