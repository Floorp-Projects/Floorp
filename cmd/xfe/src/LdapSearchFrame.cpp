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
   LdapSearchFrame.cpp -- Ldap Search window stuff.
   Created: Dora Hsu<dora@netscape.com>, 16-Dec-96.
 */

#include "LdapSearchFrame.h"
#include "LdapSearchView.h"
#include "xpassert.h"

static XFE_LdapSearchFrame *theFrame = NULL;

XFE_LdapSearchFrame::XFE_LdapSearchFrame(Widget toplevel,
										 XFE_Frame *parent_frame,
										 Chrome *chromespec)
		: XFE_Frame("Ldap", toplevel, parent_frame,
					FRAME_LDAP_SEARCH, chromespec, False, False, False, False, True /* destroyOnClose */)
{

  XFE_LdapSearchView *view = new XFE_LdapSearchView(this, 
													getChromeParent(), 
													this,
													NULL, 
													m_context, 
													NULL);

  view->createWidgets(view->getBaseWidget());
  setView(view);

  registerInterest(XFE_LdapSearchView::CloseLdap, this,
		(XFE_FunctionNotification)close_cb);

  view->show();
}

XFE_LdapSearchFrame::~XFE_LdapSearchFrame()
{
	theFrame = NULL;
}

XFE_CALLBACK_DEFN(XFE_LdapSearchFrame, close)(XFE_NotificationCenter */*obj*/,
			void * /*clientData*/, 
			void */*callData*/) 
{
	delete_response();
}


extern "C" MWContext*
fe_showLdapSearch(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
{
  if (theFrame == NULL)
        theFrame = new XFE_LdapSearchFrame(toplevel, parent_frame, chromespec);
  theFrame->show();

  return theFrame->getContext();
}
