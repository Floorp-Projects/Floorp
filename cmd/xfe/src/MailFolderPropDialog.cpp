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
   MailFolderPropDialog.cpp -- property dialogs for mail folders
   Created: Chris Toshok <toshok@netscape.com>, 08-Apr-97
 */



#include "MailFolderPropDialog.h"
#include "PropertySheetView.h"
#include "MNView.h"
#include <Xm/TextF.h>
#include <Xm/LabelG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Form.h>
#include <Xm/ToggleBG.h>
#include <Xfe/Xfe.h>

#include "xfe.h"
#include "felocale.h"

#include "xpgetstr.h"
extern int XFE_GENERAL;

static XFE_MailFolderPropDialog *theDialog = NULL;

XFE_MailFolderPropGeneralTab::XFE_MailFolderPropGeneralTab(XFE_Component *top,
														   XFE_View *view,
														   MSG_Pane *pane,
														   MSG_FolderInfo *folder)
	: XFE_PropertyTabView(top, view, XFE_GENERAL)
{
	int widest_width;
	char buf[100];

	Widget top_form, bottom_form;
	Widget sep1;

	Widget name_label;
	Widget location_label, location_value;

	Widget unread_label, unread_value;
	Widget total_label, total_value;
	Widget wasted_label, wasted_value;
	Widget space_label, space_value;

	XmString str;
	MSG_FolderLine line;

	m_folder = folder;
	m_pane = pane;

	MSG_GetFolderLineById(fe_getMNMaster(), m_folder, &line);
	int32 sizeOnDisk = MSG_GetFolderSizeOnDisk (line.id);

	top_form = XtCreateManagedWidget("top_form",
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

	name_label = XtCreateManagedWidget("name_label",
									   xmLabelGadgetClass,
									   top_form,
									   NULL, 0);

	m_namevalue = fe_CreateTextField(top_form, "name_value", NULL, 0);

	fe_SetTextFieldAndCallBack(m_namevalue, (char*)line.name);

	XtSetSensitive(m_namevalue,
				   !(line.flags & (MSG_FOLDER_FLAG_TRASH
								   | MSG_FOLDER_FLAG_DRAFTS
								   | MSG_FOLDER_FLAG_QUEUE
								   | MSG_FOLDER_FLAG_INBOX)));
					
	XtManageChild(m_namevalue);

	location_label = XtCreateManagedWidget("location_label",
										   xmLabelGadgetClass,
										   top_form,
										   NULL, 0);

	str = XmStringCreate("", XmFONTLIST_DEFAULT_TAG); // XXXX
	location_value = XtVaCreateManagedWidget("location_value",
											 xmLabelGadgetClass,
											 top_form,
											 XmNlabelString, str,
											 NULL);
	XmStringFree(str);

	unread_label = XtCreateManagedWidget("unread_label",
										 xmLabelGadgetClass,
										 bottom_form,
										 NULL, 0);
	if (line.unseen >=0)
		PR_snprintf(buf, sizeof(buf), "%d", line.unseen);
	else
		strcpy(buf, "???");
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	unread_value = XtVaCreateManagedWidget("unread_value",
										   xmLabelGadgetClass,
										   bottom_form,
										   XmNlabelString, str,
										   NULL);
	XmStringFree(str);
	total_label = XtCreateManagedWidget("total_label",
										xmLabelGadgetClass,
										bottom_form,
										NULL, 0);
	PR_snprintf(buf, sizeof(buf), "%d", line.total);
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	total_value = XtVaCreateManagedWidget("total_value",
										  xmLabelGadgetClass,
										  bottom_form,
										  XmNlabelString, str,
										  NULL);
	XmStringFree(str);
	wasted_label = XtCreateManagedWidget("wasted_label",
										 xmLabelGadgetClass,
										 bottom_form,
										 NULL, 0);
	PR_snprintf(buf, sizeof(buf), "%d %%", sizeOnDisk == 0 ? 0 : line.deletedBytes / sizeOnDisk);
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	wasted_value = XtVaCreateManagedWidget("wasted_value",
										   xmLabelGadgetClass,
										   bottom_form,
										   XmNlabelString, str,
										   NULL);
	XmStringFree(str);
	space_label = XtCreateManagedWidget("space_label",
										xmLabelGadgetClass,
										bottom_form,
										NULL, 0);
	PR_snprintf(buf, sizeof(buf), "%d k", sizeOnDisk / 1024);
	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
	space_value = XtVaCreateManagedWidget("space_value",
										  xmLabelGadgetClass,
										  bottom_form,
										  XmNlabelString, str,
										  NULL);
	XmStringFree(str);

	widest_width = XfeVaGetWidestWidget(unread_label, total_label, wasted_label, space_label, NULL);

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
	XtVaSetValues(wasted_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, total_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(wasted_value,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wasted_label,
				  XmNleftOffset, widest_width - XfeWidth(wasted_label),
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, wasted_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(space_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, wasted_label,
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

	XtVaSetValues(m_namevalue,
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
				  XmNleftWidget, m_namevalue,
				  XmNleftOffset, widest_width - XfeWidth(location_label),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_namevalue,
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

	XtVaSetValues(bottom_form,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, sep1,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
}

XFE_MailFolderPropGeneralTab::~XFE_MailFolderPropGeneralTab()
{
}

void
XFE_MailFolderPropGeneralTab::apply()
{
	/* bstell: do we need to fix this ? */
	char *folder_name = fe_GetTextField(m_namevalue);
	MSG_FolderLine line;

	MSG_GetFolderLineById(fe_getMNMaster(),
						  m_folder, &line);

	if (strcmp(line.name, folder_name))
		MSG_RenameMailFolder(m_pane, m_folder, folder_name);

	XtFree(folder_name);
}

XFE_MailFolderPropDialog::XFE_MailFolderPropDialog(Widget parent,
												   char *name,
												   MWContext *context,
												   MSG_Pane *pane,
												   MSG_FolderInfo *folder)
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
	
	folderView->addTab(new XFE_MailFolderPropGeneralTab(this,
														folderView, pane, folder));
}
						  
XFE_MailFolderPropDialog::~XFE_MailFolderPropDialog()
{
	theDialog = NULL;
}

void 
fe_showMailFolderProperties(Widget parent,
							MWContext *context,
							MSG_Pane *pane,
							MSG_FolderInfo *folder)
{
	if (theDialog)
		delete theDialog;
	
	theDialog = new XFE_MailFolderPropDialog(parent, "MailFolderProps", context, pane, folder);
	
	theDialog->show();
}
