/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/**********************************************************************
 LIConflictDialog.cpp
 By Daniel Malmer
 5/18/98

**********************************************************************/

#include "LIConflictDialog.h"
#include "li_public.h"
#include "xfe.h"

#include <Xm/ToggleB.h>
#include <Xm/Label.h>


extern "C" void
fe_makeConflictDialog(LIConflictDialogCallback f, void* closure, const char* title, const char* message, const char* left_button, const char* right_button)
{
	XFE_LIConflictDialog* dialog = new XFE_LIConflictDialog(FE_GetToplevelWidget(), title, message, left_button, right_button);

	dialog->show();

	while ( dialog->selection_made() == 0 ) {
		FEU_StayingAlive();
	}

	f(closure, dialog->state());

	delete(dialog);
}


XFE_LIConflictDialog::XFE_LIConflictDialog(Widget parent, const char* title, const char* message, const char* left_button, const char* right_button) : XFE_Dialog(parent, "liConflict", TRUE, TRUE, FALSE, FALSE, TRUE, TRUE)
{
	int ac;
	Arg av[16];
	Widget form;
	XmString xm_str;

	m_selection_made = 0;

	ac = 0;
	form = XmCreateForm(m_chrome, "form", av, ac);
	XtManageChild(form);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	m_message_label = XmCreateLabel(form, "messageLabel", av, ac);
	XtManageChild(m_message_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_message_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	m_query_label = XmCreateLabel(form, "queryLabel", av, ac);
	XtManageChild(m_query_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_query_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	m_always_toggle = XmCreateToggleButtonGadget(form, "alwaysToggle", av, ac);
	XtManageChild(m_always_toggle);

	setTitle(title);

	xm_str = XmStringCreate((char*) message, XmFONTLIST_DEFAULT_TAG);
	XtVaSetValues(m_message_label, XmNlabelString, xm_str, NULL);
	XmStringFree(xm_str);

	xm_str = XmStringCreate((char*) left_button, XmFONTLIST_DEFAULT_TAG);
	XtVaSetValues(m_okButton, XmNlabelString, xm_str, NULL);
	XmStringFree(xm_str);

	xm_str = XmStringCreate((char*) right_button, XmFONTLIST_DEFAULT_TAG);
	XtVaSetValues(m_cancelButton, XmNlabelString, xm_str, NULL);
	XmStringFree(xm_str);

	XtAddCallback(m_chrome, XmNokCallback, ok_callback, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cancel_callback, this);
}


XFE_LIConflictDialog::~XFE_LIConflictDialog()
{
}


void
XFE_LIConflictDialog::ok_callback(Widget w, XtPointer closure, XtPointer data)
{
	((XFE_LIConflictDialog*) closure)->useLocalCallback(w, data);
}


void
XFE_LIConflictDialog::cancel_callback(Widget w, XtPointer closure, XtPointer data)
{
	((XFE_LIConflictDialog*) closure)->useServerCallback(w, data);
}


void
XFE_LIConflictDialog::useLocalCallback(Widget w, XtPointer data)
{
	m_state = KEEP_LOCAL_FILE;
	hide();
	m_selection_made = 1;
}


void
XFE_LIConflictDialog::useServerCallback(Widget w, XtPointer data)
{
	m_state = DOWNLOAD_SERVER_FILE;
	hide();
	m_selection_made = 1;
}


int
XFE_LIConflictDialog::state()
{
	return (m_state | 
			(XmToggleButtonGetState(m_always_toggle) ? APPLY_TO_ALL : 0));
}


void
XFE_LIConflictDialog::setTitle(const char* title)
{
	Widget shell = getBaseWidget();

	while ( !XtIsShell(shell) && XtParent(shell) ) {
		shell = XtParent(shell);
	}
	XtVaSetValues(shell, XmNtitle, title, NULL);
}


