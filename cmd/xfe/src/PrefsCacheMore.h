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
/* */
/*
   PrefsCacheMore.h -- Cache "more" preference dialog 
   Created: Linda Wei <lwei@netscape.com>, 04-Dec-96.
 */


#ifndef _xfe_prefscachemore_h
#define _xfe_prefscachemore_h

#include "Dialog.h"
#include "PrefsDialog.h"

struct PrefsDataCacheMore
{
	MWContext *context;
};

class XFE_PrefsCacheMoreDialog : public XFE_Dialog

{
public:

	// Constructors, Destructors

	XFE_PrefsCacheMoreDialog(XFE_PrefsDialog *prefsDialog,
							 Widget           parent,    
							 char            *name,  
							 Boolean          modal = TRUE);

	virtual ~XFE_PrefsCacheMoreDialog();

	virtual void show();                // pop up dialog
	void initPage();                    // initialize dialog
	Boolean verifyPage();               // verify page
	void installChanges();              // install changes
	MWContext *getContext();            // return the context

	// Callbacks

	friend void prefsCacheMoreCb_ok(Widget, XtPointer, XtPointer);
	friend void prefsCacheMoreCb_cancel(Widget, XtPointer, XtPointer);
	friend void prefsCacheMoreCb_clearDisk(Widget, XtPointer, XtPointer);
	friend void prefsCacheMoreCb_clearMem(Widget, XtPointer, XtPointer);

private:

	// User data

	XFE_PrefsDialog              *m_prefsDialog;
	PrefsDataCacheMore           *m_prefsDataCacheMore;
};

#endif /* _xfe_prefssslconfig_h */

