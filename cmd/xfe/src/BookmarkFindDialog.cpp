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
   BookmarkFindDialog.cpp -- dialog for finding bookmarks (duh).
   Created: Chris Toshok <toshok@netscape.com>, 12-Mar-97.
 */



#include "structs.h"
#include "xp_core.h"

#include "BookmarkFrame.h"
#include "BookmarkFindDialog.h"
#include <Xm/XmP.h>
#include <Xm/LabelG.h>
#include <Xm/ToggleBG.h>

#include <Xfe/Xfe.h>

#include "xfe.h"
#include "felocale.h"

/* we only have one instance of the bookmark find dialog up at at any given time. */
static XFE_BookmarkFindDialog *theDialog = NULL;

XFE_BookmarkFindDialog::XFE_BookmarkFindDialog(Widget parent,
											   BM_FindInfo *info)
	: XFE_Dialog(parent,
				 "findDialog",
				 TRUE, 	// ok
				 TRUE, 	// cancel
				 FALSE,	// help
				 TRUE, 	//apply
				 TRUE, 	//separator
				 FALSE 	// modal
				 )
{
	Widget form;
	Arg av[10];
	int ac;
	Widget findlabel, lookinlabel, helplabel;

	m_findInfo = info;

	form = XmCreateForm(m_chrome, "bmFindForm", NULL, 0);

	XtManageChild(form);

	XtAddCallback(m_chrome, XmNokCallback, find_cb, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);
	XtAddCallback(m_chrome, XmNapplyCallback, clear_cb, this);

	findlabel = XtCreateManagedWidget("findLabel",
									  xmLabelGadgetClass,
									  form,
									  NULL, 0);

	m_findText = fe_CreateTextField(form, "findText", NULL, 0);
	XtManageChild(m_findText);

	lookinlabel = XtCreateManagedWidget("lookinLabel",
										xmLabelGadgetClass,
										form,
										NULL, 0);

	if (XfeWidth(findlabel) < XfeWidth(lookinlabel))
		XtVaSetValues (findlabel, XmNwidth, XfeWidth(lookinlabel), 0);
	else
		XtVaSetValues (lookinlabel, XmNwidth, XfeWidth(findlabel), 0);

	ac = 0;
	XtSetArg(av[ac], XmNindicatorType, XmN_OF_MANY); ac++;
	m_nameToggle = XtCreateManagedWidget("nameToggle",
										 xmToggleButtonGadgetClass, form, av, ac);
	m_locationToggle = XtCreateManagedWidget("locationToggle",
											 xmToggleButtonGadgetClass, form, av, ac);
	m_descriptionToggle = XtCreateManagedWidget("descriptionToggle",
												xmToggleButtonGadgetClass, form, av, ac);
	m_caseToggle = XtCreateManagedWidget("caseSensitive",
										 xmToggleButtonGadgetClass, form, av, ac);
	m_wordToggle = XtCreateManagedWidget("wordToggle",
										 xmToggleButtonGadgetClass, form, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNentryAlignment, XmALIGNMENT_CENTER); ac++;

	helplabel = XtCreateManagedWidget("helptext",
									  xmLabelGadgetClass, form, av, ac);

	/* Attachments */
	XtVaSetValues(findlabel, XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM, 0);

	XtVaSetValues(m_findText, XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, findlabel,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, findlabel,
				  XmNrightAttachment, XmATTACH_FORM, 0);
	
	XtVaSetValues(lookinlabel, XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_findText, 0);
	
	XtVaSetValues(m_nameToggle, XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, lookinlabel,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_findText, 0);

	XtVaSetValues(m_locationToggle, XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_nameToggle,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_nameToggle, 0);

	XtVaSetValues(m_descriptionToggle, XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_locationToggle,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_nameToggle, 0);

	XtVaSetValues(m_caseToggle, XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, lookinlabel,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_nameToggle, 0);

	XtVaSetValues(m_wordToggle, XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, lookinlabel,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_caseToggle, 0);
	
	XtVaSetValues(helplabel, XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_wordToggle, 0);

	XtVaSetValues (m_chrome, XmNinitialFocus, m_findText, 0);

	setValues();
}

XFE_BookmarkFindDialog::~XFE_BookmarkFindDialog()
{
	theDialog = NULL;
}

void
XFE_BookmarkFindDialog::getValues()
{
  /* Should we free findInfo->textToFind */
  XP_FREE(m_findInfo->textToFind);
  m_findInfo->textToFind = fe_GetTextField(m_findText);

  XtVaGetValues(m_nameToggle, XmNset, &m_findInfo->checkName, 0);
  XtVaGetValues(m_locationToggle, XmNset, &m_findInfo->checkLocation, 0);
  XtVaGetValues(m_descriptionToggle, XmNset, &m_findInfo->checkDescription, 0);
  XtVaGetValues(m_caseToggle, XmNset, &m_findInfo->matchCase, 0);
  XtVaGetValues(m_wordToggle, XmNset, &m_findInfo->matchWholeWord, 0);
}

void
XFE_BookmarkFindDialog::setValues()
{
	fe_SetTextField(m_findText, m_findInfo->textToFind);

	XtVaSetValues(m_nameToggle, XmNset, m_findInfo->checkName, 0);
	XtVaSetValues(m_locationToggle, XmNset, m_findInfo->checkLocation, 0);
	XtVaSetValues(m_descriptionToggle, XmNset, m_findInfo->checkDescription, 0);
	XtVaSetValues(m_caseToggle, XmNset, m_findInfo->matchCase, 0);
	XtVaSetValues(m_wordToggle, XmNset, m_findInfo->matchWholeWord, 0);
}

void
XFE_BookmarkFindDialog::find()
{
	getValues();
	hide();

    BM_DoFindBookmark(XFE_BookmarkFrame::main_bm_context, m_findInfo);
}

void
XFE_BookmarkFindDialog::clear()
{
    if (m_findInfo->textToFind)
		m_findInfo->textToFind[0] = '\0';
    fe_SetTextField(m_findText, m_findInfo->textToFind);
    XmProcessTraversal (m_findText, XmTRAVERSE_CURRENT);
}

void
XFE_BookmarkFindDialog::cancel()
{
	hide();
}

void
XFE_BookmarkFindDialog::find_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_BookmarkFindDialog *obj = (XFE_BookmarkFindDialog*)clientData;

	obj->find();
}

void
XFE_BookmarkFindDialog::clear_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_BookmarkFindDialog *obj = (XFE_BookmarkFindDialog*)clientData;

	obj->clear();
}

void
XFE_BookmarkFindDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_BookmarkFindDialog *obj = (XFE_BookmarkFindDialog*)clientData;

	obj->cancel();
}

extern "C" void
fe_showBMFindDialog(Widget parent, BM_FindInfo *info)
{
	if (!theDialog)
		{
			theDialog = new XFE_BookmarkFindDialog(parent, info);
		}

	theDialog->show();
}
