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
   Dialog.cpp -- class for toplevel XFE dialogs.
   Created: Linda Wei <lwei@netscape.com>, 16-Sep-96.
 */



#include "Dialog.h"
#include "xpassert.h"
#include "xfe2_extern.h"

#include <Xm/SelectioB.h>

// ==================== Public Member Functions ====================

// Member:       XFE_Dialog
// Description:  Constructor
// Inputs:
// Side effects: Creates a prompt dialog

XFE_Dialog::XFE_Dialog(Widget   parent,      // dialog parent
					   char    *name,        // dialog name
					   Boolean  ok,          // show OK button?
					   Boolean  cancel,      // show cancel button?
					   Boolean  help,        // show help button?
					   Boolean  apply,       // show apply button?
					   Boolean  separator,   // show separator?
					   Boolean  modal,       // modal dialog?
					   Widget chrome_widget)
	: XFE_Component()
{
#if defined(GLUE_COMPO_CONTEXT)
    Widget s = parent;
    while (s && !XtIsShell(s)) {
        s = XtParent(s);
    }
	m_wParent = s;

	// Create the chrome

	if (chrome_widget == NULL)
	  m_chrome = createDialogChromeWidget(s, name, ok, cancel,
										  help, apply, separator, modal);
	else
	  m_chrome = chrome_widget;
#else
	m_wParent = parent;

	// Create the chrome

	if (chrome_widget == NULL)
	  m_chrome = createDialogChromeWidget(parent, name, ok, cancel,
										  help, apply, separator, modal);
	else
	  m_chrome = chrome_widget;
#endif /* GLUE_COMPO_CONTEXT */

	// Register base widget and parent widget

	setBaseWidget(XtParent(m_chrome));
	installDestroyHandler();
}

// Member:       ~XFE_Dialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_Dialog::~XFE_Dialog()
{
}

// Member:       show
// Description:  
// Inputs:
// Side effects:

void XFE_Dialog::show()
{
	XP_ASSERT(m_chrome);
	fe_NukeBackingStore(m_chrome);
	XtManageChild(m_chrome);
}

// Member:       hide
// Description:  
// Inputs:
// Side effects:

void XFE_Dialog::hide()
{
	XP_ASSERT(m_chrome);
	XtUnmanageChild(m_chrome);
}

// ==================== Protected Member Functions ====================

// ==================== Private Member Functions ====================

Widget
XFE_Dialog::createDialogChromeWidget(Widget   parent,    
									 char    *name,
									 Boolean  ok,
									 Boolean  cancel,
									 Boolean  help,
									 Boolean  apply,
									 Boolean  separator,
									 Boolean  modal)
{
	Visual   *v = 0;
	Colormap  cmap = 0;
	Cardinal  depth = 0;
	Arg       av[20];
	int       ac;
	Widget chrome;

	XtVaGetValues (parent, 
				   XtNvisual, &v, 
				   XtNcolormap, &cmap,
				   XtNdepth, &depth,
				   0);

	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
	XtSetArg (av[ac], XmNtransientFor, parent); ac++;
	if (modal) {
		XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
	}
	XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
	XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
	XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
	XtSetArg (av[ac], XmNnoResize, True); ac++;

	chrome = XmCreatePromptDialog (parent, name, av, ac);

	if (!separator)
		fe_UnmanageChild_safe(XmSelectionBoxGetChild(chrome,
													 XmDIALOG_SEPARATOR));

	fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome, 
												   XmDIALOG_TEXT));

	fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome,
												   XmDIALOG_SELECTION_LABEL));

	if (!ok)
		fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome, 
													   XmDIALOG_OK_BUTTON));
	else
		m_okButton = XmSelectionBoxGetChild(chrome, XmDIALOG_OK_BUTTON);

	if (!cancel)
		fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome,
													   XmDIALOG_CANCEL_BUTTON));
	else
		m_cancelButton = XmSelectionBoxGetChild(chrome, XmDIALOG_CANCEL_BUTTON);

	fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome, XmDIALOG_HELP_BUTTON));

	if (help) {
		// TODO: Enable help later
		fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome, XmDIALOG_HELP_BUTTON));
		// m_helpButton = XmSelectionBoxGetChild(chrome, XmDIALOG_HELP_BUTTON);
	}
	else
		fe_UnmanageChild_safe (XmSelectionBoxGetChild (chrome, XmDIALOG_HELP_BUTTON));

	if (apply) {
		m_applyButton = XmSelectionBoxGetChild(chrome, XmDIALOG_APPLY_BUTTON);
		XtManageChild(m_applyButton);
	}

	return chrome;
}

