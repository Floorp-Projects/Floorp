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
   NavCenterFrame.cpp -- class definition for the NavCenter frame class
   Created: Stephen Lamm <slamm@netscape.com>, 5-Nov-97.
 */



#include "NavCenterFrame.h"
#include "NavCenterView.h"
#include "Dashboard.h"
#include "Menu.h"

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

MenuSpec XFE_NavCenterFrame::file_menu_spec[] = {
  { xfeCmdClose,		PUSHBUTTON },
  { xfeCmdExit,			PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_NavCenterFrame::menu_bar_spec[] = {
  { xfeMenuFile, 	CASCADEBUTTON, (MenuSpec*)&XFE_NavCenterFrame::file_menu_spec },
  { "bookmarksSubmenu",	CASCADEBUTTON, XFE_Frame::bookmark_submenu_spec },
  { xfeMenuWindow, 	CASCADEBUTTON, (MenuSpec*)&XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON, (MenuSpec*)&XFE_Frame::help_menu_spec },
  { NULL }
};

XFE_NavCenterFrame::XFE_NavCenterFrame(Widget toplevel,
                                       XFE_Frame *parent_frame,
                                       Chrome *chromespec) 
  : XFE_Frame("navcenter", toplevel, 
			  parent_frame,
			  FRAME_NAVCENTER, 
			  chromespec,
			  False,
			  True,
			  False,
			  True,
			  True)
{
  D(printf("XFE_NavCenterFrame SUB-SYSTEM INSTANCIATING\n");)

  // create the bookmark view
  XFE_View *view = new XFE_NavCenterView(this, getChromeParent(), 
                                         NULL, m_context);
  setView(view);
  setMenubar(menu_bar_spec);

#ifdef NOTYET
  //
  // Make the bookmark frame title more reasonable
  //
  char title[kMaxFullNameLength+64];

  PR_snprintf(title, 
			  sizeof(title),
              XP_GetString(XFE_BM_FRAME_TITLE),
    	      FE_UsersFullName());

  setTitle(title);
#endif
  //setTitle(HT_GetViewName(m_htview));

  view->show();

  m_dashboard->setShowStatusBar(True);
  //m_dashboard->setShowProgressBar(True);

  realize();
  resize(600,580); //XXX Default size for now

}

XFE_NavCenterFrame::~XFE_NavCenterFrame()
{
	D(printf("XFE_NavCenterFrame SUB-SYSTEM DESTRUCTING\n");)

      //BM_SaveBookmarks(main_bm_context, NULL);
}

//////////////////////////////////////////////////////////////////////////
/*static*/ void
XFE_NavCenterFrame::showBookmarks (Widget toplevel, XFE_Frame *parent_frame)
{
  // not a static global, since we can have multiple browsers.
	XFE_NavCenterFrame *theFrame;
	MWContext *theContext = NULL;
	
	theFrame = new XFE_NavCenterFrame(toplevel, parent_frame, NULL);
    theFrame->getNavCenterView()->newBookmarksPane();
	theFrame->show();
}
//////////////////////////////////////////////////////////////////////////
/*static*/ void
XFE_NavCenterFrame::showHistory (Widget toplevel, XFE_Frame *parent_frame)
{
  // not a static global, since we can have multiple browsers.
	XFE_NavCenterFrame *theFrame;
	MWContext *theContext = NULL;
	
	theFrame = new XFE_NavCenterFrame(toplevel, parent_frame, NULL);
    theFrame->getNavCenterView()->newHistoryPane();
	theFrame->show();
}
//////////////////////////////////////////////////////////////////////////
/*static*/ void
XFE_NavCenterFrame::editToolbars (Widget toplevel, XFE_Frame *parent_frame)
{
  // not a static global, since we can have multiple browsers.
	XFE_NavCenterFrame *theFrame;
	MWContext *theContext = NULL;
	
	theFrame = new XFE_NavCenterFrame(toplevel, parent_frame, NULL);
    theFrame->getNavCenterView()->newToolbarPane();
	theFrame->show();
}
//////////////////////////////////////////////////////////////////////////
extern "C" MWContext *
fe_showNavCenter(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, URL_Struct * /* url */)
{
  // not a static global, since we can have multiple browsers.
	XFE_NavCenterFrame *theFrame;
	MWContext *theContext = NULL;
	
	D( printf("in fe_showNavCenter()\n"); );
	
	theFrame = new XFE_NavCenterFrame(toplevel, parent_frame, chromespec);
	
	theFrame->show();
	
	theContext = theFrame->getContext();

	D( printf("leaving fe_showNavCenter()\n"); );

	return theContext;
}
extern "C" void
//////////////////////////////////////////////////////////////////////////
fe_showBookmarks(Widget toplevel)
{
    XFE_NavCenterFrame::showBookmarks(toplevel, NULL /*parent_frame*/);
}
extern "C" void
fe_showHistory(Widget toplevel)
{
    XFE_NavCenterFrame::showHistory(toplevel, NULL /*parent_frame*/);
}


