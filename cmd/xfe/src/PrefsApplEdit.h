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
   PrefsApplEdit.h -- Applications "edit" preference dialog 

   Created: Linda Wei <lwei@netscape.com>, 01-Nov-96.
 */

#ifndef _xfe_prefsappledit_h
#define _xfe_prefsappledit_h

#include "Dialog.h"
#include "xeditor.h"
#include "PrefsData.h"
class XFE_PrefsDialog;
class XFE_PrefsPageGeneralAppl;

class XFE_PrefsApplEditDialog : public XFE_Dialog

{
public:

	// Constructors, Destructors

	XFE_PrefsApplEditDialog(XFE_PrefsDialog *prefsDialog,
							Widget           parent,    
							char            *name,  
							XFE_PrefsPageGeneralAppl *prefsPageGeneralAppl,
							Boolean          modal = TRUE);

	virtual ~XFE_PrefsApplEditDialog();

	void show();                        // pop up dialog
	void initPage(Boolean  newFlag,
				  int      pos);        // initialize dialog
	Boolean verifyPage();               // verify page
	void installChanges();              // install changes
	MWContext *getContext();            // return the context
    XFE_PrefsDialog *getPrefsDialog();  // return the pref dialog											   
    PrefsDataGeneralAppl *getPrefsDataGeneralAppl();  

	// Callbacks

	friend void prefsApplEditCb_ok(Widget, XtPointer, XtPointer);
	friend void prefsApplEditCb_cancel(Widget, XtPointer, XtPointer);
	friend void prefsApplEditCb_toggleHandledBy(Widget, XtPointer, XtPointer);
	friend void prefsApplEditCb_browseAppl(Widget, XtPointer, XtPointer);
	friend void prefsApplEditCb_setHandledBy(Widget, XtPointer, XtPointer);
	friend void prefsApplEditCb_selectPlugin(Widget, XtPointer, XtPointer);

private:

	void buildPluginList(char *mimeType);
	void pluginSelected(char *pluginName);
	Bool handleByPlugin(char *type);

	// User data

	XFE_PrefsDialog              *m_prefsDialog;
	XFE_PrefsPageGeneralAppl     *m_prefsPageGeneralAppl;
	PrefsDataApplEdit            *m_prefsDataApplEdit;
	PrefsDataGeneralAppl         *m_prefsDataGeneralAppl;
};

#endif /* _xfe_prefsappledit_h */

