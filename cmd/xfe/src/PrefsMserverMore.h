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
   PrefsMserverMore.h -- Mail server "more" preference dialog 
   Created: Linda Wei <lwei@netscape.com>, 23-Oct-96.
 */



#ifndef _xfe_prefsmservermore_h
#define _xfe_prefsmservermore_h

#include "Dialog.h"
#include "PrefsDialog.h"

struct PrefsDataMserverMore
{
	MWContext *context;

	Widget     browse_button;
	Widget     mail_dir_text;
	Widget     imap_mail_dir_text;
	Widget     imap_mail_local_dir_text;
	Widget     imap_mail_local_dir_browse_button;
	Widget     check_for_mail_toggle;
	Widget     check_mail_interval_text;
	//	Widget     enable_biff_toggle;
	Widget     remember_email_passwd_toggle;
	//	Widget     encrypted_passwd_toggle;
	//	Widget     always_toggle;
	//	Widget     never_toggle;
};

class XFE_PrefsMserverMoreDialog : public XFE_Dialog

{
public:

	// Constructors, Destructors

	XFE_PrefsMserverMoreDialog(XFE_PrefsDialog *prefsDialog,
							   Widget           parent,    
							   char            *name,  
							   Boolean          modal = TRUE);

	virtual ~XFE_PrefsMserverMoreDialog();

	virtual void show();                // pop up dialog
	void initPage();                    // initialize dialog
	Boolean verifyPage();               // verify page
	void installChanges();              // install changes
	MWContext *getContext();            // return the context

	// Callbacks

	static void cb_ok(Widget, XtPointer, XtPointer);
	static void cb_cancel(Widget, XtPointer, XtPointer);
	static void cb_browseMailDir(Widget, XtPointer, XtPointer);
	//	static void cb_toggleMapiServer(Widget, XtPointer, XtPointer);

private:

	// User data

	XFE_PrefsDialog              *m_prefsDialog;
	PrefsDataMserverMore         *m_prefsDataMserverMore;
};

#endif /* _xfe_prefsmservermore_h */

