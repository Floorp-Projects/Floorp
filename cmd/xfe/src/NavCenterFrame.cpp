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
			  False) // m_destroyOnClose, always keep one Bookmark instance
{
  D(printf("XFE_NavCenterFrame SUB-SYSTEM INSTANCIATING\n");)

  // create the bookmark view
  XFE_View *view = new XFE_NavCenterView(this, getViewParent(), 
                                         NULL, m_context);
  setView(view);
  setMenubar(menu_bar_spec);

  XtVaSetValues(view->getBaseWidget(),
  		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);

  //setMenubar(menu_bar_spec);

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
XP_Bool
XFE_NavCenterFrame::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
    {
      return XFE_Frame::isCommandEnabled(cmd, calldata);
    }
}

void
XFE_NavCenterFrame::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo*info)
{
    {
      XFE_Frame::doCommand(cmd,calldata,info);
    }
}

Boolean
XFE_NavCenterFrame::handlesCommand(CommandType cmd, void *calldata,
                            XFE_CommandInfo* info)
{
    {
      return XFE_Frame::handlesCommand(cmd, calldata, info);
    }
}

char *
XFE_NavCenterFrame::commandToString(CommandType cmd, void* calldata,
                                    XFE_CommandInfo* info)
{
  return XFE_Frame::commandToString(cmd, calldata, info);
}

//////////////////////////////////////////////////////////////////////////
extern "C" MWContext *
fe_showNavCenter(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, URL_Struct *url)
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
