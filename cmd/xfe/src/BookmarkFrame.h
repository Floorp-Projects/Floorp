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
   BookmarkFrame.h -- class definitions for bookmark frames
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_bookmarkframe_h
#define _xfe_bookmarkframe_h

#include "Frame.h"
#include "xp_core.h"
#include <Xm/Xm.h>

class XFE_BookmarkFrame : public XFE_Frame
{
public:
  XFE_BookmarkFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
  virtual ~XFE_BookmarkFrame();

  static MWContext *main_bm_context;

  // To allow window close (we need a more graceful way of doing this)
  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);


  static void createBookmarkFrame(Widget toplevel, 
								  XFE_Frame *parent_frame, 
								  Chrome *chromespec);

  static XFE_BookmarkFrame * getBookmarkFrame();

private:
  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];

  static XFE_BookmarkFrame * m_bookmarkFrame;
};

extern "C" MWContext* fe_showBookmarks(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern "C" MWContext* fe_getBookmarkContext();
extern "C" void fe_createBookmarks(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);

#endif /* _xfe_bookmarkframe_h */
