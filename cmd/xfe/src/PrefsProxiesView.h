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
   PrefsProxiesView.h -- Proxies "view" preference dialog 
   Created: Linda Wei <lwei@netscape.com>, 24-Oct-96.
 */



#ifndef _xfe_prefsproxiesview_h
#define _xfe_prefsproxiesview_h

#include "Dialog.h"
#include "PrefsDialog.h"

struct PrefsDataProxiesView
{
	MWContext *context;

	Widget     ftp_proxy_text;
	Widget     ftp_port_text;
	Widget     gopher_proxy_text;
	Widget     gopher_port_text;
	Widget     http_proxy_text;
	Widget     http_port_text;
	Widget     https_proxy_text;
	Widget     https_port_text;
	Widget     wais_proxy_text;
	Widget     wais_port_text;
	Widget     no_proxy_text;
	Widget     socks_host_text;
	Widget     socks_port_text;
};

class XFE_PrefsProxiesViewDialog : public XFE_Dialog

{
public:

	// Constructors, Destructors

	XFE_PrefsProxiesViewDialog(XFE_PrefsDialog *prefsDialog,
							   Widget           parent,    
							   char            *name,  
							   Boolean          modal = TRUE);

	virtual ~XFE_PrefsProxiesViewDialog();

	virtual void show();                // pop up dialog
	void initPage();                    // initialize page
	Boolean verifyPage();               // verify page
	void installChanges();              // install changes
	MWContext *getContext();            // return the context

	// Callbacks

	friend void prefsProxiesViewCb_ok(Widget, XtPointer, XtPointer);
	friend void prefsProxiesViewCb_cancel(Widget, XtPointer, XtPointer);

private:

	// User data

	XFE_PrefsDialog              *m_prefsDialog;
	PrefsDataProxiesView         *m_prefsDataProxiesView;
};

#endif /* _xfe_prefsproxiesview_h */

