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
  g-frame.h -- components.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_frame_h
#define _moz_frame_h

#include "structs.h"
#include "ntypes.h"

#include "g-component.h"

struct _MozFrame {
  MozComponent _component;

  /* MozFrame specific stuff here. */

  MWContext *context;

  MozView *top_view;
  MozView *focus_view;

  GtkWidget *vbox;
  GtkWidget *viewarea;
  GtkWidget *statusbar;
};

extern void			moz_frame_init(MozFrame *frame, GnomeUIInfo *menubar_info, GnomeUIInfo *toolbar_info);
extern void			moz_frame_deinit(MozFrame *frame);

extern void	  		moz_frame_show(MozFrame *frame);
extern void			moz_frame_raise(MozFrame *frame);

extern MozView*		moz_frame_get_top_view(MozFrame *frame);
extern MozView*		moz_frame_get_focus_view(MozFrame *frame);

extern MWContext*	moz_frame_get_context(MozFrame *frame);

extern void 		moz_frame_set_title(MozFrame *frame, char *title);
extern void 		moz_frame_set_status(MozFrame *frame, char *status);
extern void 		moz_frame_set_percent(MozFrame *frame, int32 percent);

extern void			moz_frame_set_viewarea(MozFrame *frame, GtkWidget *viewarea);

#endif /* _moz_frame_h */
