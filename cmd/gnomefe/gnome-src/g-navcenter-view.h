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
  g-navcenter-view.h -- navcenter views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#ifndef _moz_navcenter_view_h
#define _moz_navcenter_view_h

#include "g-view.h"
#include "htrdf.h"

struct _MozNavCenterView {
  /* our superclass */
  MozView _view;

  GtkWidget *ctree;
  HT_View ht_view;
  HT_Pane ht_pane;
};

extern void		moz_navcenter_view_init(MozNavCenterView *view, MozFrame *parent_frame, MWContext *context);
extern void		moz_navcenter_view_deinit(MozNavCenterView *view);

extern MozNavCenterView*	moz_navcenter_view_create(MozFrame *parent_frame, MWContext *context);

#endif _moz_bookmark_view_h 
