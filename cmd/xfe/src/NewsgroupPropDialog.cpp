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
   NewsgroupPropDialog.cpp -- property dialogs for news groups.
   Created: Chris Toshok <toshok@netscape.com>, 08-Apr-97
 */



#include "NewsgroupPropDialog.h"
#include "PropertySheetView.h"
#include "MNView.h"
#include <Xm/LabelG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Form.h>
#include <Xm/ToggleBG.h>
#include <Xfe/Xfe.h>

#include "xfe.h"

#include "xpgetstr.h"
extern int XFE_GENERAL;
extern int XFE_HTML_NEWSGROUP_MSG;

static XFE_NewsgroupPropDialog *theDialog = NULL;

XFE_NewsgroupPropGeneralTab::XFE_NewsgroupPropGeneralTab(XFE_Component *top,
														 XFE_View *view,
														 MSG_FolderInfo *group)
	: XFE_PropertyTabView(top, view, XFE_GENERAL)
{
	int widest_width;
	char buf[100];
	char *name_buf;

	Widget top_form, middle_form, bottom_form;
	Widget sep1, sep2;

	Widget name_label, name_value;
	Widget location_label, location_value;

	Widget unread_label, unread_value;
	Widget total_label, total_value;
	Widget space_label, space_value;

	Widget html_desc;

	XmString str;

	MSG_FolderLine line;
	MSG_NewsHost *host;

	m_group = group;

	MSG_GetFolderLineById(fe_getMNMaster(), m_group, &line);
	int32 sizeOnDisk = MSG_GetFolderSizeOnDisk (line.id);

	top_form = XtCreateManagedWidget("top_form",
									 xmFormWidgetClass,
									 getBaseWidget(),
									 NULL, 0);
	middle_form = XtCreateManagedWidget("middle_form",
										xmFormWidgetClass,
										getBaseWidget(),
										NULL, 0);
	bottom_form = XtCreateManagedWidget("bottom_form",
										xmFormWidgetClass,
										getBaseWidget(),
										NULL, 0);

	sep1 = XtCreateManagedWidget("sep1",
								 xmSeparatorGadgetClass,
								 getBaseWidget(),
								 NULL, 0);

	sep2 = XtCreateManagedWidget("sep2",
								 xmSeparatorGadgetClass,
								 getBaseWidget(),
								 NULL, 0);

	name_label = XtCreateManagedWidget("name_label",
									   xmLabelGadgetClass,
									   top_form,
									   NULL, 0);

	if (line.prettyName)
		name_buf = PR_smprintf("%s\n(%s)", line.prettyName, line.name);
	else
		name_buf = strdup(line.name);
	str = XmStringCreateLtoR(name_buf, XmFONTLIST_DEFAULT_TAG);
	free(name_buf);
	name_value = XtVaCreateManagedWidget("name_value",
										 xmLabelGadgetClass,
										 top_form,
										 XmNlabelString, str,
										 NULL);
	XmStringFree(str);

	location_label = XtCreateManagedWidget("location_label",
										   xmLabelGadgetClass,
										   top_form,
										   NULL, 0);
	host = MSG_GetNewsHostForFolder(m_group);
	str = XmStringCreate(host ? (char*)MSG_GetNewsHostName(host) : "", XmFONTLIST_DEFAULT_TAG);
	location_value = XtVaCreateManagedWidget("location_value",
											 xmLabelGadgetClass,
											 top_form,
											 XmNlabelString, str,
											 NULL);
	XmStringFree(str);

	unread_label = XtCreateManagedWidget("unread_label",
										 xmLabelGadgetClass,
										 middle_form,
										 NULL, 0);
	if (line.unseen >=0)
		PR_snprintf(buf, sizeof(buf), "%d", line.unseen);
	else
		strcpy(buf, "???");
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	unread_value = XtVaCreateManagedWidget("unread_value",
										   xmLabelGadgetClass,
										   middle_form,
										   XmNlabelString, str,
										   NULL);
	XmStringFree(str);
	total_label = XtCreateManagedWidget("total_label",
										xmLabelGadgetClass,
										middle_form,
										NULL, 0);
	PR_snprintf(buf, sizeof(buf), "%d", line.total);
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	total_value = XtVaCreateManagedWidget("total_value",
										  xmLabelGadgetClass,
										  middle_form,
										  XmNlabelString, str,
										  NULL);
	XmStringFree(str);
	space_label = XtCreateManagedWidget("space_label",
										xmLabelGadgetClass,
										middle_form,
										NULL, 0);
	PR_snprintf(buf, sizeof(buf), "%d k", sizeOnDisk / 1024);
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	space_value = XtVaCreateManagedWidget("space_value",
										  xmLabelGadgetClass,
										  middle_form,
										  XmNlabelString, str,
										  NULL);
	XmStringFree(str);
	m_htmltoggle = XtVaCreateManagedWidget("html_toggle",
										   xmToggleButtonGadgetClass,
										   bottom_form,
										   XmNset, MSG_IsHTMLOK(fe_getMNMaster(),
																m_group),
										   NULL);

	XtAddCallback(m_htmltoggle, XmNvalueChangedCallback, html_toggled_cb, this);

	str = XmStringCreateLtoR(XP_GetString(XFE_HTML_NEWSGROUP_MSG),
							 XmFONTLIST_DEFAULT_TAG);

	html_desc = XtVaCreateManagedWidget("html_desc",
										xmLabelGadgetClass,
										bottom_form,
										XmNlabelString, str,
										NULL);

	XmStringFree(str);

	XtVaSetValues(m_htmltoggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);
	XtVaSetValues(html_desc,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 15,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_htmltoggle,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	widest_width = XfeVaGetWidestWidget(unread_label, total_label, space_label, NULL);

	XtVaSetValues(unread_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(unread_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, unread_label,
				  XmNleftOffset, widest_width - XfeWidth(unread_label),
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, unread_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(total_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, unread_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(total_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, total_label,
				  XmNleftOffset, widest_width - XfeWidth(total_label),
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, total_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(space_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, total_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(space_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, space_label,
				  XmNleftOffset, widest_width - XfeWidth(space_label),
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, space_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	widest_width = XfeVaGetWidestWidget(name_label, location_label, NULL);

	XtVaSetValues(name_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(location_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, location_value,
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
	XtVaSetValues(location_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, name_value,
				  XmNleftOffset, widest_width - XfeWidth(location_label),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, name_value,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(top_form,
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
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(sep2,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, middle_form,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(bottom_form,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, sep2,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
}

XFE_NewsgroupPropGeneralTab::~XFE_NewsgroupPropGeneralTab()
{
}

void
XFE_NewsgroupPropGeneralTab::html_toggled()
{
	MSG_SetIsHTMLOK(fe_getMNMaster(), m_group,
					m_contextData, XmToggleButtonGetState(m_htmltoggle));
}

void
XFE_NewsgroupPropGeneralTab::html_toggled_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsgroupPropGeneralTab *obj = (XFE_NewsgroupPropGeneralTab*)clientData;

	obj->html_toggled();
}

XFE_NewsgroupPropDialog::XFE_NewsgroupPropDialog(Widget parent,
												 char *name,
												 MWContext *context,
												 MSG_FolderInfo *group)
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
	
	folderView->addTab(new XFE_NewsgroupPropGeneralTab(this,
													   folderView, group));
}
						  
XFE_NewsgroupPropDialog::~XFE_NewsgroupPropDialog()
{
	theDialog = NULL;
}

void 
fe_showNewsgroupProperties(Widget parent,
						   MWContext *context,
						   MSG_FolderInfo *group)
{
	if (theDialog)
		delete theDialog;
	
	theDialog = new XFE_NewsgroupPropDialog(parent, "NewsgroupProps", context, group);
	
	theDialog->show();
}
