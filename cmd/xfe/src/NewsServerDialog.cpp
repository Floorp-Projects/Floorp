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
   NewsServerDialog.cpp -- dialog for letting the user add a new news server.
   Created: Chris Toshok <toshok@netscape.com>, 24-Oct-96.
 */



#include "NewsServerDialog.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>

#include "felocale.h"
#include "xpgetstr.h"
extern int XFE_MAKE_SURE_SERVER_AND_PORT_ARE_VALID;

#define SECURE_PORT_STRING "563"
#define INSECURE_PORT_STRING "119"

// we need this in the post() method.
extern "C" void fe_EventLoop();

XFE_NewsServerDialog::XFE_NewsServerDialog(Widget parent, char *name,
										   XFE_Frame *frame)
	: XFE_Dialog(parent, 
				 name, 
				 TRUE, // ok
				 TRUE, // cancel
				 FALSE, // help
				 FALSE, // apply
				 TRUE, // separator
				 TRUE // modal
				 )
{
	Widget server_label, port_label, secure_label;
	Widget form, serverForm, portForm, secureForm;

	m_frame = frame;

	form = XtCreateManagedWidget("serverDialogForm",
								 xmFormWidgetClass,
								 m_chrome,
								 NULL, 0);

	serverForm = XtCreateManagedWidget("serverForm",
									   xmFormWidgetClass,
									   form,
									   NULL, 0);

	portForm = XtCreateManagedWidget("portForm",
									 xmFormWidgetClass,
									 form,
									 NULL, 0);

	secureForm = XtCreateManagedWidget("secureForm",
									   xmFormWidgetClass,
									   form,
									   NULL, 0);

	server_label = XtCreateManagedWidget("serverLabel",
										 xmLabelGadgetClass,
										 serverForm,
										 NULL, 0);

	m_serverText = fe_CreateTextField(serverForm,
									  "serverText",
									  NULL, 0);
	XtManageChild(m_serverText);

	port_label = XtCreateManagedWidget("portLabel",
									   xmLabelGadgetClass,
									   portForm,
									   NULL, 0);

	m_portText = fe_CreateTextField(portForm,
									"portText",
									NULL, 0);
	XtManageChild(m_portText);

	secure_label = XtCreateManagedWidget("secureLabel",
										 xmLabelGadgetClass,
										 secureForm,
										 NULL, 0);

	m_secureToggle = XtCreateManagedWidget("secureToggle",
										   xmToggleButtonGadgetClass,
										   secureForm,
										   NULL, 0);

	// now we dance the body attachment
	XtVaSetValues(secure_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_secureToggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, secure_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(port_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_portText,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, port_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(server_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_serverText,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, server_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(serverForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(portForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, serverForm,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(secureForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, portForm,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);

	fe_SetTextFieldAndCallBack(m_portText, INSECURE_PORT_STRING);
	m_portTextEdited = False;

	XtAddCallback(m_secureToggle, XmNvalueChangedCallback, toggle_cb, this);
	XtAddCallback(m_portText, XmNvalueChangedCallback, port_edit_cb, this);
}

XFE_NewsServerDialog::~XFE_NewsServerDialog()
{
	// nothing needed (that I'm aware of) yet.
}

XP_Bool
XFE_NewsServerDialog::post()
{
	m_doneWithLoop = False;
  
	XtVaSetValues(m_chrome,
				  XmNdeleteResponse, XmUNMAP,
				  NULL);

	show();

	while(!m_doneWithLoop)
		{
			fe_EventLoop();
		}

	return m_retVal;
}

const char *
XFE_NewsServerDialog::getServer()
{
	char *foo = fe_GetTextField(m_serverText);

	if (strlen(foo) == 0)
		return NULL;
	else
		return foo;
}

int
XFE_NewsServerDialog::getPort()
{
	char *foo = fe_GetTextField(m_portText);

	if (strlen(foo) == 0)
		return -1;
	else
		return atoi(foo);
}

XP_Bool
XFE_NewsServerDialog::isSecure()
{
	return XmToggleButtonGadgetGetState(m_secureToggle);
}

XP_Bool
XFE_NewsServerDialog::verify()
{
	if (getServer() == NULL || getPort() <= 0)
		{
			XFE_Alert(m_frame->getContext(), XP_GetString(XFE_MAKE_SURE_SERVER_AND_PORT_ARE_VALID));
			return False;
		}
	else
		{
			return True;
		}
}

void
XFE_NewsServerDialog::ok()
{
	if (verify())
		{
			m_doneWithLoop = True;
			m_retVal = True;
			
			hide();
		}
}

void
XFE_NewsServerDialog::cancel()
{
	m_doneWithLoop = True;
	m_retVal = False;

	hide();
}

void
XFE_NewsServerDialog::toggle()
{
	char *text = fe_GetTextField(m_portText);

	if (!m_portTextEdited || !strcmp(text, ""))
		{
			XtRemoveCallback(m_portText, XmNvalueChangedCallback, port_edit_cb, this);
			if (XmToggleButtonGadgetGetState(m_secureToggle))
				{
					fe_SetTextFieldAndCallBack(m_portText, SECURE_PORT_STRING);
				}
			else
				{
					fe_SetTextFieldAndCallBack(m_portText, INSECURE_PORT_STRING);
				}
			XtAddCallback(m_portText, XmNvalueChangedCallback, port_edit_cb, this);
			m_portTextEdited = False;
		}

	XtFree(text);
}

void
XFE_NewsServerDialog::port_edit()
{
	m_portTextEdited = True;
}

void
XFE_NewsServerDialog::port_edit_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsServerDialog *obj = (XFE_NewsServerDialog*)clientData;

	obj->port_edit();
}

void
XFE_NewsServerDialog::toggle_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsServerDialog *obj = (XFE_NewsServerDialog*)clientData;

	obj->toggle();
}

void 
XFE_NewsServerDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsServerDialog *obj = (XFE_NewsServerDialog*)clientData;

	obj->ok();
}

void
XFE_NewsServerDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsServerDialog *obj = (XFE_NewsServerDialog*)clientData;

	obj->cancel();
}

