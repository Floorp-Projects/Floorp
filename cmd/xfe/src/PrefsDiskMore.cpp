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
   PrefsDiskMore.cpp -- The more dialog for Offline Disk Space preference
   Created: Linda Wei <lwei@netscape.com>, 16-Dec-96.
 */

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "PrefsDiskMore.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <Xfe/Xfe.h>

extern int XFE_ERROR_SAVING_OPTIONS;

extern "C"
{
	char *XP_GetString(int i);
	void fe_installDiskMore();
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsDiskMoreDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the More dialog for Offline Disk Space Preferences

XFE_PrefsDiskMoreDialog::XFE_PrefsDiskMoreDialog(XFE_PrefsDialog *prefsDialog,
									 Widget     parent,      // dialog parent
									 char      *name,        // dialog name
									 Boolean    modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 FALSE,    // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_prefsDialog(prefsDialog),
	  m_prefsDataDiskMore(0)
{
	PrefsDataDiskMore *fep = NULL;

	fep = new PrefsDataDiskMore;
	memset(fep, 0, sizeof(PrefsDataDiskMore));
	m_prefsDataDiskMore = fep;
	
	Widget     form;
	Widget     desc_label;
	Widget     rm_msg_body_toggle;
	Widget     num_days_text;
	Widget     days_label;
	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	ac = 0;
	i = 0;

	kids[i++] = desc_label = 
		XmCreateLabelGadget(form, "descLabel", av, ac);

	kids[i++] = rm_msg_body_toggle = 
		XmCreateToggleButtonGadget (form, "rmMsgBodyToggle", av, ac);

	kids[i++] = num_days_text =
		fe_CreateTextField(form, "numDaysText", av, ac);

	kids[i++] = days_label = 
		XmCreateLabelGadget(form, "daysLabel", av, ac);

	XtVaSetValues(desc_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	int labels_height;
	labels_height = XfeVaGetTallestWidget(rm_msg_body_toggle,
										  num_days_text,
										  days_label,
										  NULL);

	XtVaSetValues(rm_msg_body_toggle,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, desc_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, desc_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(num_days_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, rm_msg_body_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, rm_msg_body_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(days_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, rm_msg_body_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, num_days_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	fep->rm_msg_body_toggle = rm_msg_body_toggle;
	fep->num_days_text = num_days_text;

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, prefsDiskMoreCb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, prefsDiskMoreCb_cancel, this);

	XtManageChildren(kids, i);
}

// Member:       ~XFE_PrefsDiskMoreDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsDiskMoreDialog::~XFE_PrefsDiskMoreDialog()
{
	// Clean up

	delete m_prefsDataDiskMore;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsDiskMoreDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsDiskMore
// Inputs:
// Side effects: 

void XFE_PrefsDiskMoreDialog::initPage()
{
	XP_ASSERT(m_prefsDataDiskMore);

	PrefsDataDiskMore  *fep = m_prefsDataDiskMore;
	XFE_GlobalPrefs    *prefs = &fe_globalPrefs;
	char                buf[1024];
	
	XtVaSetValues(fep->rm_msg_body_toggle, XmNset, prefs->news_remove_bodies_by_age, 0);
	PR_snprintf(buf, sizeof(buf), "%d", prefs->news_remove_bodies_days);
	XtVaSetValues(fep->num_days_text, XmNvalue, buf, 0);
}

// Member:       verifyPage
// Description:  verify page for MailNewsDiskMore
// Inputs:
// Side effects: 

Boolean XFE_PrefsDiskMoreDialog::verifyPage()
{
	return TRUE;
}

// Member:       installChanges
// Description:  install changes for MailNewsDiskMore
// Inputs:
// Side effects: 

void XFE_PrefsDiskMoreDialog::installChanges()
{
	fe_installDiskMore();
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsDiskMoreDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       prefsDiskMoreCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDiskMoreDialog::prefsDiskMoreCb_ok(Widget    w,
												 XtPointer closure,
												 XtPointer callData)
{
	XFE_PrefsDiskMoreDialog *theDialog = (XFE_PrefsDiskMoreDialog *)closure;
	PrefsDataDiskMore       *fep = theDialog->m_prefsDataDiskMore;

	XP_ASSERT(fep);
	if (! theDialog->verifyPage()) return;
	
	Boolean                     b;
    char                        c;
	char                       *s = 0;
    int                         n;

	XtVaGetValues(fep->rm_msg_body_toggle, XmNset, &b, 0);
	fe_globalPrefs.news_remove_bodies_by_age = b;

    XtVaGetValues(fep->num_days_text, XmNvalue, &s, 0);
    fe_globalPrefs.news_remove_bodies_days = 0;
    if (1 == sscanf (s, " %d %c", &n, &c))
      fe_globalPrefs.news_remove_bodies_days = n;

	// Simulate a cancel

	prefsDiskMoreCb_cancel(w, closure, callData);

	// Install preferences

	theDialog->installChanges();

	// Save the preferences at the end, so that if we've found some 
	// setting that crashes, it won't get saved...

	if (! fe_CheckVersionAndSavePrefs((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
		fe_perror (theDialog->getContext(), XP_GetString( XFE_ERROR_SAVING_OPTIONS));
}

// Member:       prefsDiskMoreCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDiskMoreDialog::prefsDiskMoreCb_cancel(Widget    /* w */,
													 XtPointer closure,
													 XtPointer /* callData */)
{
	XFE_PrefsDiskMoreDialog *theDialog = (XFE_PrefsDiskMoreDialog *)closure;

	// Delete the dialog

	delete theDialog;
}


