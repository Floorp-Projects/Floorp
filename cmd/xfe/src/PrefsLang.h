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
   PrefsLang.h -- Language preference dialog 
   Created: Linda Wei <lwei@netscape.com>, 11-Nov-96.
 */

#ifndef _xfe_prefslang_h
#define _xfe_prefslang_h

#include "Dialog.h"
#include "PrefsDialog.h"

struct PrefsDataLang
{
	MWContext *context;

	Widget   lang_list;
	Widget   other_text;
	Widget   temp_default_button;

	char   **avail_lang_regs;
	int      avail_cnt;
};

class XFE_PrefsLangDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

	XFE_PrefsLangDialog(XFE_PrefsDialog *prefsDialog,
						XFE_PrefsPageBrowserLang *langPage,
						Widget           parent,    
						char            *name,  
						Boolean          modal = TRUE);

	virtual ~XFE_PrefsLangDialog();

	virtual void show();                // pop up dialog
	void initPage();                    // initialize dialog
	Boolean verifyPage();               // verify page
	MWContext *getContext();            // return the context
	XFE_PrefsPageBrowserLang *getLangPage(); // return the language page
    XFE_PrefsDialog *getPrefsDialog();  // return the pref dialog											   
	// Callbacks

	static void cb_ok(Widget, XtPointer, XtPointer);
	static void cb_cancel(Widget, XtPointer, XtPointer);
	static void cb_focusOther(Widget, XtPointer, XtPointer);
	static void cb_loseFocusOther(Widget, XtPointer, XtPointer);

private:

	// User data

	XFE_PrefsDialog          *m_prefsDialog;
	XFE_PrefsPageBrowserLang *m_langPage;
	PrefsDataLang            *m_prefsDataLang;
};

#endif /* _xfe_prefslang_h */

