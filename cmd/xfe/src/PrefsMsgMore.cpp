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
   PrefsMsgMore.cpp -- The more dialog for Mail messages preference
   Created: Linda Wei <lwei@netscape.com>, 04-Mar-97.
 */



#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "prefapi.h"
#include "PrefsMsgMore.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 

extern int XFE_ERROR_SAVING_OPTIONS;

extern "C"
{
	char *XP_GetString(int i);
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsMsgMoreDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the More dialog for Mail Messages Preferences

XFE_PrefsMsgMoreDialog::XFE_PrefsMsgMoreDialog(XFE_PrefsDialog *prefsDialog,
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
	  m_prefsDataMsgMore(0)
{
	PrefsDataMsgMore *fep = NULL;

	fep = new PrefsDataMsgMore;
	memset(fep, 0, sizeof(PrefsDataMsgMore));
	m_prefsDataMsgMore = fep;
	
	Widget     form;
	Widget     names_and_nicknames_toggle;
	Widget     nicknames_only_toggle;
	Widget     eight_bit_toggle;
	Widget     quoted_printable_toggle;
	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNmarginWidth, 8,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	// Addressing messages

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "addressFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "addressBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "addressingLabel", av, ac);

	ac = 0;
	i = 0;

	kids [i++] = names_and_nicknames_toggle = 
		XmCreateToggleButtonGadget(form1, "nameAndNicknamesToggle", av, ac);

	kids [i++] = nicknames_only_toggle = 
		XmCreateToggleButtonGadget(form1, "nicknamesOnlyToggle", av, ac);

	fep->names_and_nicknames_toggle = names_and_nicknames_toggle;
	fep->nicknames_only_toggle = nicknames_only_toggle;

	XtVaSetValues(names_and_nicknames_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(nicknames_only_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, names_and_nicknames_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, names_and_nicknames_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(label1);
	XtManageChild(form1);
	XtManageChild(frame1);

	// Send messages that use 8-bit characters

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "frame2", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "frame2Box", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "composeLabel", av, ac);

	ac = 0;
	i = 0;

	kids [i++] = eight_bit_toggle = 
		XmCreateToggleButtonGadget(form2, "8bitToggle", av, ac);

	kids[i++] = quoted_printable_toggle = 
		XmCreateToggleButtonGadget(form2, "qpToggle", av, ac);

	fep->eight_bit_toggle = eight_bit_toggle;
	fep->quoted_printable_toggle = quoted_printable_toggle;

	XtVaSetValues(eight_bit_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(quoted_printable_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, eight_bit_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, eight_bit_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	fep->eight_bit_toggle = eight_bit_toggle;
	fep->quoted_printable_toggle = quoted_printable_toggle;

	XtManageChildren(kids, i);
	XtManageChild(label2);
	XtManageChild(form2);
	XtManageChild(frame2);

	// Sending messages

	Widget frame3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame2); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame3 = XmCreateFrame (form, "frame3", av, ac);

	Widget form3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form3 = XmCreateForm (frame3, "frame3Box", av, ac);

	Widget label3;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	XtSetArg (av [ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
	label3 = XmCreateLabelGadget (frame3, "sendLabel", av, ac);

	Widget ask_toggle;
	Widget send_text_toggle;
	Widget send_html_toggle;
	Widget send_both_toggle;

	ac = 0;
	i = 0;

	kids [i++] = ask_toggle = 
		XmCreateToggleButtonGadget(form3, "askToggle", av, ac);

	kids[i++] = send_text_toggle = 
		XmCreateToggleButtonGadget(form3, "sendTextToggle", av, ac);

	kids[i++] = send_html_toggle = 
		XmCreateToggleButtonGadget(form3, "sendHtmlToggle", av, ac);

	kids[i++] = send_both_toggle = 
		XmCreateToggleButtonGadget(form3, "sendBothToggle", av, ac);

	fep->ask_toggle = ask_toggle;
	fep->send_text_toggle = send_text_toggle;
	fep->send_html_toggle = send_html_toggle;
	fep->send_both_toggle = send_both_toggle;

	XtVaSetValues(ask_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(send_text_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, ask_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, ask_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(send_html_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, send_text_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, ask_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(send_both_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, send_html_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, ask_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(label3);
	XtManageChild(form3);
	XtManageChild(frame3);

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);

	XtAddCallback(names_and_nicknames_toggle, XmNvalueChangedCallback, cb_toggleAddressing, fep);
	XtAddCallback(nicknames_only_toggle, XmNvalueChangedCallback, cb_toggleAddressing, fep);

	XtAddCallback(eight_bit_toggle, XmNvalueChangedCallback, cb_toggleEncoding, fep);
	XtAddCallback(quoted_printable_toggle, XmNvalueChangedCallback, cb_toggleEncoding, fep);

	XtAddCallback(ask_toggle, XmNvalueChangedCallback, cb_toggleSend, fep);
	XtAddCallback(send_text_toggle, XmNvalueChangedCallback, cb_toggleSend, fep);
	XtAddCallback(send_html_toggle, XmNvalueChangedCallback, cb_toggleSend, fep);
	XtAddCallback(send_both_toggle, XmNvalueChangedCallback, cb_toggleSend, fep);
}

// Member:       ~XFE_PrefsMsgMoreDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsMsgMoreDialog::~XFE_PrefsMsgMoreDialog()
{
	// Clean up

	delete m_prefsDataMsgMore;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsMsgMoreDialog::show()
{
	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsMsgMore
// Inputs:
// Side effects: 

void XFE_PrefsMsgMoreDialog::initPage()
{
	XP_ASSERT(m_prefsDataMsgMore);

	PrefsDataMsgMore  *fep = m_prefsDataMsgMore;
	XFE_GlobalPrefs   *prefs = &fe_globalPrefs;
	XP_Bool            b;
    Boolean            sensitive;

    sensitive = !PREF_PrefIsLocked("mailnews.nicknames_only");
	b = prefs->expand_addr_nicknames_only;
	XtVaSetValues(fep->nicknames_only_toggle,
				  XmNset, b, 
                  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->names_and_nicknames_toggle,
				  XmNset, !b, 
                  XmNsensitive, sensitive,
				  0);

	// 8-bit or quoted printable

    sensitive = !PREF_PrefIsLocked("mail.strictly_mime");
	XtVaSetValues(fep->eight_bit_toggle,
				  XmNset, !prefs->qp_p,
                  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->quoted_printable_toggle,
				  XmNset, prefs->qp_p, 
                  XmNsensitive, sensitive,
				  0);

	// Sending HTML messages

    sensitive = !PREF_PrefIsLocked("mail.default_html_action");
	XtVaSetValues(fep->ask_toggle, 
				  XmNset, (prefs->html_def_action == HTML_ACTION_ASK),
                  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->send_text_toggle, 
				  XmNset, (prefs->html_def_action == HTML_ACTION_TEXT),
                  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->send_html_toggle, 
				  XmNset, (prefs->html_def_action == HTML_ACTION_HTML),
                  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->send_both_toggle, 
				  XmNset, (prefs->html_def_action == HTML_ACTION_BOTH),
                  XmNsensitive, sensitive,
				  0);
}

// Member:       verifyPage
// Description:  verify page for MailNewsMsgMore
// Inputs:
// Side effects: 

Boolean XFE_PrefsMsgMoreDialog::verifyPage()
{
	return TRUE;
}

// Member:       installChanges
// Description:  install changes for MailNewsMsgMore
// Inputs:
// Side effects: 

void XFE_PrefsMsgMoreDialog::installChanges()
{
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsMsgMoreDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       prefsMsgMoreCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsMsgMoreDialog::cb_ok(Widget    w,
									   XtPointer closure,
									   XtPointer callData)
{
	XFE_PrefsMsgMoreDialog *theDialog = (XFE_PrefsMsgMoreDialog *)closure;
	PrefsDataMsgMore       *fep = theDialog->m_prefsDataMsgMore;
	Boolean                 b;

	XP_ASSERT(fep);
	if (! theDialog->verifyPage()) return;

	// addressing messages

	XtVaGetValues(fep->nicknames_only_toggle, XmNset, &b, 0);
	fe_globalPrefs.expand_addr_nicknames_only = b;

	// 8bit, quoted printable

	XtVaGetValues(fep->quoted_printable_toggle, XmNset, &b, 0);
	fe_globalPrefs.qp_p = b;

	// Default html action

    XtVaGetValues(fep->ask_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.html_def_action = HTML_ACTION_ASK;

    XtVaGetValues(fep->send_text_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.html_def_action = HTML_ACTION_TEXT;

    XtVaGetValues(fep->send_html_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.html_def_action = HTML_ACTION_HTML;

    XtVaGetValues(fep->send_both_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.html_def_action = HTML_ACTION_BOTH;

	// Simulate a cancel

	theDialog->cb_cancel(w, closure, callData);

	// Install preferences

	theDialog->installChanges();

	// Save the preferences at the end, so that if we've found some setting that
	// crashes, it won't get saved...

	if (! fe_CheckVersionAndSavePrefs((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
		fe_perror (theDialog->getContext(), XP_GetString( XFE_ERROR_SAVING_OPTIONS));
}

// Member:       prefsMsgMoreCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsMsgMoreDialog::cb_cancel(Widget    /* w */,
									   XtPointer closure,
									   XtPointer /* callData */)
{
	XFE_PrefsMsgMoreDialog *theDialog = (XFE_PrefsMsgMoreDialog *)closure;

	// Delete the dialog

	delete theDialog;
}

void XFE_PrefsMsgMoreDialog::cb_toggleEncoding(Widget    w,
											   XtPointer closure,
											   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMsgMore             *fep = (PrefsDataMsgMore *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->eight_bit_toggle) {
		XtVaSetValues(fep->quoted_printable_toggle, XmNset, False, 0);
	}
	else if (w == fep->quoted_printable_toggle) {
		XtVaSetValues(fep->eight_bit_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

void XFE_PrefsMsgMoreDialog::cb_toggleAddressing(Widget    w,
												 XtPointer closure,
												 XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMsgMore             *fep = (PrefsDataMsgMore *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->names_and_nicknames_toggle) {
		XtVaSetValues(fep->nicknames_only_toggle, XmNset, False, 0);
	}
	else if (w == fep->nicknames_only_toggle) {
		XtVaSetValues(fep->names_and_nicknames_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

void XFE_PrefsMsgMoreDialog::cb_toggleSend(Widget    w,
										   XtPointer closure,
										   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMsgMore             *fep = (PrefsDataMsgMore *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->ask_toggle) {
		XtVaSetValues(fep->send_text_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_html_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_both_toggle, XmNset, False, 0);
	}
	else if (w == fep->send_text_toggle) {
		XtVaSetValues(fep->ask_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_html_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_both_toggle, XmNset, False, 0);
	}
	else if (w == fep->send_html_toggle) {
		XtVaSetValues(fep->ask_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_text_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_both_toggle, XmNset, False, 0);
	}
	else if (w == fep->send_both_toggle) {
		XtVaSetValues(fep->ask_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_text_toggle, XmNset, False, 0);
		XtVaSetValues(fep->send_html_toggle, XmNset, False, 0);
	}
	else
		abort();	
}


