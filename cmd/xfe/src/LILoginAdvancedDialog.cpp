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
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/**********************************************************************
 LILoginAdvancedDialog.cpp
 By Daniel Malmer
 5/4/98

**********************************************************************/

#include "LILoginAdvancedDialog.h"


XFE_LILoginAdvancedDialog::XFE_LILoginAdvancedDialog(Widget parent) : XFE_Dialog(parent, "liLoginOptions", TRUE, TRUE, FALSE, FALSE, FALSE)
{
	Widget form;

	form = XmCreateForm(m_chrome, "prefs", NULL, 0);
	XtManageChild(form);

	m_serverFrame = new XFE_PrefsPageLIServer(form);
	m_filesFrame = new XFE_PrefsPageLIFiles(form);

	m_serverFrame->create();
	m_serverFrame->init();

	m_filesFrame->create();
	m_filesFrame->init();

	XtVaSetValues(m_serverFrame->get_frame(), 
					XmNbottomAttachment,  XmATTACH_NONE, 
					NULL);
	XtVaSetValues(m_filesFrame->get_frame(), 
					XmNtopAttachment,  XmATTACH_WIDGET,
					XmNtopWidget, m_serverFrame->get_frame(), 
					NULL);

	XtAddCallback(m_chrome, XmNokCallback, ok_callback, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cancel_callback, this);
}


XFE_LILoginAdvancedDialog::~XFE_LILoginAdvancedDialog()
{
}


void
XFE_LILoginAdvancedDialog::ok_callback(Widget w, XtPointer closure, XtPointer data)
{
	((XFE_LILoginAdvancedDialog*) closure)->okCallback(w, data);
}


void
XFE_LILoginAdvancedDialog::cancel_callback(Widget w, XtPointer closure, XtPointer data)
{
	((XFE_LILoginAdvancedDialog*) closure)->cancelCallback(w, data);
}


void
XFE_LILoginAdvancedDialog::okCallback(Widget w, XtPointer data)
{
	m_serverFrame->save();
	m_filesFrame->save();
	cancelCallback(w, data);
}


void
XFE_LILoginAdvancedDialog::cancelCallback(Widget w, XtPointer data)
{
	hide();
}


