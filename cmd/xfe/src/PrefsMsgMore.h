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
   PrefsMsgMore.h -- Message "more" preference dialog 
   Created: Linda Wei <lwei@netscape.com>, 04-Mar-97.
 */



#ifndef _xfe_prefsmsgmore_h
#define _xfe_prefsmsgmore_h

#include "Dialog.h"
#include "PrefsDialog.h"

struct PrefsDataMsgMore
{
	MWContext *context;

	Widget     eight_bit_toggle;
	Widget     quoted_printable_toggle;
	Widget     names_and_nicknames_toggle;
	Widget     nicknames_only_toggle;
	Widget     ask_toggle;
	Widget     send_text_toggle;
	Widget     send_html_toggle;
	Widget     send_both_toggle;
};

class XFE_PrefsMsgMoreDialog : public XFE_Dialog

{
public:

	// Constructors, Destructors

	XFE_PrefsMsgMoreDialog(XFE_PrefsDialog *prefsDialog,
						   Widget           parent,    
						   char            *name,  
						   Boolean          modal = TRUE);

	virtual ~XFE_PrefsMsgMoreDialog();

	virtual void show();                // pop up dialog
	void initPage();                    // initialize dialog
	Boolean verifyPage();               // verify page
	void installChanges();              // install changes
	MWContext *getContext();            // return the context

	// Callbacks

	static void cb_ok(Widget, XtPointer, XtPointer);
	static void cb_cancel(Widget, XtPointer, XtPointer);
	static void cb_toggleEncoding(Widget, XtPointer, XtPointer);
	static void cb_toggleAddressing(Widget, XtPointer, XtPointer);
	static void cb_toggleSend(Widget, XtPointer, XtPointer);

private:

	// User data

	XFE_PrefsDialog          *m_prefsDialog;
	PrefsDataMsgMore         *m_prefsDataMsgMore;
};

#endif /* _xfe_prefsmsgmore_h */

