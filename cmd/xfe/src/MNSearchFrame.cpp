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
   MNSearchFrame.cpp -- Mail/News Search window stuff.
   Created: Dora Hsu<dora@netscape.com>, 16-Dec-96.
 */



#include "MNSearchFrame.h"
#include "MNSearchView.h"
#include "xpassert.h"

static XFE_MNSearchFrame *theFrame = NULL;

XFE_MNSearchFrame::XFE_MNSearchFrame(Widget toplevel, XFE_Frame *parent_frame,
									 Chrome *chromespec,
									 XFE_MNView *mnview,
									 MSG_FolderInfo *folderInfo)
	: XFE_Frame("MNSearch",				// name
				toplevel,				// top level
				parent_frame,			// parent frame
				FRAME_MAILNEWS_SEARCH,	// frame type
				chromespec,				// chrome
				False,					// have html display
				False,					// have menu bar
				False,					// have toolbar
				False,					// have dashboard (we'll make our own)
				True)					// destroy on close

{

  XP_ASSERT(mnview != NULL);

  XFE_MNSearchView *view = new XFE_MNSearchView(this,
												getChromeParent(), 
												this,
												mnview, 
												m_context, 
												NULL, 
												folderInfo, 
												True);

  view->createWidgets(view->getBaseWidget());
  setView(view);

  registerInterest(XFE_MNSearchView::CloseSearch, this,
		(XFE_FunctionNotification)close_cb);

  view->show();
}

XFE_MNSearchFrame::~XFE_MNSearchFrame()
{
	theFrame = NULL;
}

void
XFE_MNSearchFrame::setFolderOption(MSG_FolderInfo *folderInfo)
{
   ((XFE_MNSearchView*)getView())->setFolderOption(folderInfo);
}

XFE_CALLBACK_DEFN(XFE_MNSearchFrame, close)(XFE_NotificationCenter */*obj*/,
			void * /*clientData*/, 
			void */*callData*/) 
{
	delete_response();
}

extern "C" MWContext*
fe_showMNSearch(Widget toplevel, XFE_Frame *parent_frame,
				Chrome *chromespec, XFE_MNView* mnview, 
		MSG_FolderInfo* folderInfo )
{
  if (theFrame == NULL)
        theFrame = new XFE_MNSearchFrame(toplevel, parent_frame,
										 chromespec, mnview, folderInfo);
  else theFrame->setFolderOption(folderInfo);

  theFrame->show();

  return theFrame->getContext();
}

extern "C" MWContext*
fe_showSearch(Widget toplevel, XFE_Frame *parent_frame,
			  Chrome *chromespec)
{
  if (theFrame == NULL)
        theFrame = new XFE_MNSearchFrame(toplevel, parent_frame, chromespec, NULL);
  theFrame->show();

  return theFrame->getContext();
}
