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
   NewsServerPropDialog.cpp -- property dialogs for news servers.
   Created: Chris Toshok <toshok@netscape.com>, 08-Apr-97
 */



#include "NewsServerPropDialog.h"
#include "PropertySheetView.h"
#include <Xm/LabelG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xfe/Xfe.h>

#include "xfe.h"

#include "xpgetstr.h"
extern int XFE_GENERAL;
extern int XFE_HTML_NEWSGROUP_MSG;
extern int XFE_SEC_ENCRYPTED;
extern int XFE_SEC_NONE;

static XFE_NewsServerPropDialog *theDialog = NULL;

XFE_NewsServerPropGeneralTab::XFE_NewsServerPropGeneralTab(XFE_Component *top,
														   XFE_View *view,
														   MSG_NewsHost *host)
	: XFE_PropertyTabView(top, view, XFE_GENERAL)
{
	int widest_width;
	char buf[100];
	XmString str;

	Widget top_form, middle_form;
	Widget sep1;

	Widget name_label, name_value;
	Widget port_label, port_value;
	Widget security_label, security_value;
	Widget desc_label, desc_value;

	m_newshost = host;

	top_form = XtCreateManagedWidget("top_form",
									 xmFormWidgetClass,
									 getBaseWidget(),
									 NULL, 0);
	middle_form = XtCreateManagedWidget("middle_form",
										xmFormWidgetClass,
										getBaseWidget(),
										NULL, 0);

	sep1 = XtCreateManagedWidget("sep1",
								 xmSeparatorGadgetClass,
								 getBaseWidget(),
								 NULL, 0);

	name_label = XtCreateManagedWidget("name_label",
									   xmLabelGadgetClass,
									   top_form,
									   NULL, 0);

	str = XmStringCreate((char*)MSG_GetNewsHostName(m_newshost),
						 XmFONTLIST_DEFAULT_TAG);
	name_value = XtVaCreateManagedWidget("name_value",
										 xmLabelGadgetClass,
										 top_form,
										 XmNlabelString, str,
										 NULL);
	XmStringFree(str);

	port_label = XtCreateManagedWidget("port_label",
									   xmLabelGadgetClass,
									   top_form,
									   NULL, 0);

	PR_snprintf(buf, sizeof(buf), "%d", MSG_GetNewsHostPort(m_newshost));
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	port_value = XtVaCreateManagedWidget("port_value",
										 xmLabelGadgetClass,
										 top_form,
										 XmNlabelString, str,
										 NULL);
	XmStringFree(str);

	security_label = XtCreateManagedWidget("security_label",
										   xmLabelGadgetClass,
										   top_form,
										   NULL, 0);
	str = XmStringCreate(MSG_IsNewsHostSecure(m_newshost)
						 ? XP_GetString(XFE_SEC_ENCRYPTED)
						 : XP_GetString(XFE_SEC_NONE),
						 XmFONTLIST_DEFAULT_TAG);
	security_value = XtVaCreateManagedWidget("security_value",
											 xmLabelGadgetClass,
											 top_form,
											 XmNlabelString, str,
											 NULL);
	XmStringFree(str);
	desc_label = XtCreateManagedWidget("desc_label",
									   xmLabelGadgetClass,
									   top_form,
									   NULL, 0);

	str = XmStringCreate("", XmFONTLIST_DEFAULT_TAG); // XXXX 
	desc_value = XtVaCreateManagedWidget("desc_value",
										 xmLabelGadgetClass,
										 top_form,
										 XmNlabelString, str,
										 NULL);
	XmStringFree(str);

	m_prompt_toggle = XtCreateManagedWidget("prompt_toggle",
                                            xmToggleButtonWidgetClass,
                                            middle_form,
                                            NULL, 0);
	m_anonymous_toggle = XtCreateManagedWidget("anonymous_toggle",
                                               xmToggleButtonWidgetClass,
                                               middle_form,
                                               NULL, 0);
	XtVaSetValues(m_prompt_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);
	XtVaSetValues(m_anonymous_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_prompt_toggle,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	widest_width = XfeVaGetWidestWidget(name_label, port_label, security_label, desc_label, NULL);

	XtVaSetValues(name_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(port_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, name_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(security_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(desc_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, security_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(name_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, name_label,
				  XmNleftOffset, widest_width - XfeWidth(name_label),
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, name_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);
	XtVaSetValues(port_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, port_label,
				  XmNleftOffset, widest_width - XfeWidth(port_label),
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);
	XtVaSetValues(security_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, security_label,
				  XmNleftOffset, widest_width - XfeWidth(security_label),
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, security_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);
	XtVaSetValues(desc_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, desc_label,
				  XmNleftOffset, widest_width - XfeWidth(desc_label),
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, desc_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(top_form,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(sep1,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, top_form,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(middle_form,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, sep1,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

#define setPromptToggle() \
    if (MSG_GetNewsHostPushAuth(m_newshost)) { \
        XmToggleButtonSetState(m_prompt_toggle, True, False); \
        XmToggleButtonSetState(m_anonymous_toggle, False, False); \
    } else { \
        XmToggleButtonSetState(m_prompt_toggle, False, False); \
        XmToggleButtonSetState(m_anonymous_toggle, True, False); \
    }

    setPromptToggle();

    XtAddCallback(m_prompt_toggle, XmNvalueChangedCallback,
                  promptForPasswd_cb, this);
    XtAddCallback(m_anonymous_toggle, XmNvalueChangedCallback,
                  promptForPasswd_cb, this);
}

XFE_NewsServerPropGeneralTab::~XFE_NewsServerPropGeneralTab()
{
}

void
XFE_NewsServerPropGeneralTab::setPushAuth(Widget w)
{
    XP_Bool pushAuth = (w == m_prompt_toggle) ? True : False;
    MSG_SetNewsHostPushAuth (m_newshost, pushAuth);
    setPromptToggle();
}

void
XFE_NewsServerPropGeneralTab::promptForPasswd_cb(Widget w,
                                                 XtPointer clientData,
                                                 XtPointer)
{
	XFE_NewsServerPropGeneralTab* obj
        = (XFE_NewsServerPropGeneralTab*)clientData;
    if (obj)
        obj->setPushAuth(w);
}

XFE_NewsServerPropDialog::XFE_NewsServerPropDialog(Widget parent,
												   char *name,
												   MWContext *context,
												   MSG_NewsHost *host)
	: XFE_PropertySheetDialog((XFE_View*)0, parent, name,
						  context,
						  TRUE, /* ok */
						  TRUE, /* cancel */
						  TRUE, /* help */
						  FALSE, /* apply */
						  FALSE, /* separator */
						  True) /* modal */
{
	XFE_PropertySheetView* folderView = (XFE_PropertySheetView *) m_view;

	folderView->addTab(new XFE_NewsServerPropGeneralTab(this,
														folderView, host));
}
						  
XFE_NewsServerPropDialog::~XFE_NewsServerPropDialog()
{
	theDialog = NULL;
}

void 
fe_showNewsServerProperties(Widget parent,
							MWContext *context,
							MSG_NewsHost *host)
{
	if (theDialog)
		delete theDialog;

	theDialog = new XFE_NewsServerPropDialog(parent, "NewsServerProps", context, host);

	theDialog->show();
}
