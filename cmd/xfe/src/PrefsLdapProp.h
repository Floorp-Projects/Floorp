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
   PrefsLdapProp.h -- LDAP Server Properties
   Created: Linda Wei <lwei@netscape.com>, 7-Feb-97.
 */



#ifndef _xfe_prefsldapprop_h
#define _xfe_prefsldapprop_h

#include "Dialog.h"
#include "PrefsDialog.h"
#include "dirprefs.h"

struct PrefsDataLdapProp
{
	MWContext *context;

	Widget   desc_text;
	Widget   server_text;
	Widget   root_text;
	Widget   port_number_text;
	Widget   number_of_hit_text;
	Widget   secure_toggle;
#if 0
	Widget   save_passwd_toggle;
#endif
};

class XFE_PrefsLdapPropDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

	XFE_PrefsLdapPropDialog(XFE_PrefsDialog               *prefsDialog,
							XFE_PrefsPageMailNewsAddrBook *addrBookPage,
							Widget                         parent,    
							char                          *name,  
							Boolean                        modal = TRUE);

	virtual ~XFE_PrefsLdapPropDialog();

	virtual void show();                // pop up dialog
	void initPage(DIR_Server *);        // initialize dialog
	Boolean verifyPage();               // verify page
	MWContext *getContext();            // return the context
	PrefsDataLdapProp *getData();
	DIR_Server *getEditDir();
	XFE_PrefsPageMailNewsAddrBook *getAddrBookPage(); // return the address book page
    XFE_PrefsDialog *getPrefsDialog();  // return the pref dialog			   

	// Callbacks

	static void cb_ok(Widget, XtPointer, XtPointer);
	static void cb_cancel(Widget, XtPointer, XtPointer);

private:

	// User data

	XFE_PrefsDialog               *m_prefsDialog;
	XFE_PrefsPageMailNewsAddrBook *m_addrBookPage;
	PrefsDataLdapProp             *m_prefsDataLdapProp;
	DIR_Server                    *m_server;
};

#endif /* _xfe_prefsldapprop_h */

