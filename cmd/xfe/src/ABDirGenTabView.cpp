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
   ABDirGenTabView.cpp -- class definition for ABDirGenTabView
   Created: Tao Cheng <tao@netscape.com>, 10-nov-97
 */

#include "ABDirGenTabView.h"
#include "ABDirPropertyDlg.h"

#include <Xm/Form.h>
#include <Xm/TextF.h> 
#include <Xm/Text.h> 
#include <Xm/Separator.h>
#include <Xm/SeparatoG.h>

#include <Xm/ToggleB.h>
#include <Xm/LabelG.h> 

#include "felocale.h"
#include "xpgetstr.h"

extern int XFE_AB_NAME_GENERAL_TAB;

extern int XFE_ABDIR_DESCRIPT;
extern int XFE_ABDIR_SERVER;
extern int XFE_ABDIR_SERVER_ROOT;
extern int XFE_ABDIR_PORT_NUM;
extern int XFE_ABDIR_MAX_HITS;
extern int XFE_ABDIR_SECURE;
extern int XFE_ABDIR_USE_PASSWD;
extern int XFE_ABDIR_SAVE_PASSWD;

extern "C" Widget fe_CreateTextField(Widget parent, const char *name,
									 Arg *av, int ac);

XFE_ABDirGenTabView::XFE_ABDirGenTabView(XFE_Component *top,
										 XFE_View *view):
	XFE_PropertyTabView(top, view, XFE_AB_NAME_GENERAL_TAB)
{

	int    ac, 
		   numForms = ABDIR_LAST+ABDIR_SECLAST;
	Arg    av[20];
	Widget topForm = getBaseWidget(),
		   label = NULL,
           stripForm[ABDIR_LAST+ABDIR_SECLAST];
	char  *genTabLabels[ABDIR_LAST+ABDIR_SECLAST];
	Dimension width,
	          m_labelWidth = 0;

	genTabLabels[ABDIR_DESCRIPTION] = XP_GetString(XFE_ABDIR_DESCRIPT);
	genTabLabels[ABDIR_LDAPSERVER] = XP_GetString(XFE_ABDIR_SERVER);
	genTabLabels[ABDIR_SEARCHROOT] = XP_GetString(XFE_ABDIR_SERVER_ROOT);
	genTabLabels[ABDIR_PORTNUMBER] = XP_GetString(XFE_ABDIR_PORT_NUM);
	genTabLabels[ABDIR_MAXHITS] = XP_GetString(XFE_ABDIR_MAX_HITS);
	genTabLabels[ABDIR_LAST + ABDIR_SECUR] = XP_GetString(XFE_ABDIR_SECURE);
	genTabLabels[ABDIR_LAST + ABDIR_USEPASSWD] = 
		XP_GetString(XFE_ABDIR_USE_PASSWD);
	genTabLabels[ABDIR_LAST + ABDIR_SAVEPASSWD] = 
		XP_GetString(XFE_ABDIR_SAVE_PASSWD);

	int i; // for use in multiple for loops... 
	// without breaking newer ANSI C++ rules
	Widget sep = NULL;
	for (i=0; i < numForms; i++) {
		sep = NULL;
		if (i == ABDIR_PORTNUMBER ||
			i == ABDIR_LAST) {
			sep = XtVaCreateManagedWidget("sep",
										  xmSeparatorGadgetClass, topForm,
										  XmNleftAttachment, XmATTACH_FORM,
										  XmNtopAttachment, XmATTACH_WIDGET,
										  XmNtopWidget, stripForm[i-1],
										  XmNtopOffset, 3,
										  XmNrightAttachment, XmATTACH_FORM,
										  XmNbottomAttachment, XmATTACH_NONE,
										  0);
		}/* if */

		ac = 0;
		stripForm[i] = XmCreateForm(topForm, "strip", av, ac);
		if (i < ABDIR_LAST) {
			/* label/textF
			 */
			/* Create labels
			 */
			label = XtVaCreateManagedWidget(genTabLabels[i],
											xmLabelGadgetClass, stripForm[i],
											XmNalignment, XmALIGNMENT_BEGINNING, 
											NULL);
			m_labels[i] = label;
			XtVaSetValues(label, 
						  XmNleftAttachment, XmATTACH_FORM,
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_NONE,
						  XmNbottomAttachment, XmATTACH_FORM,
						  0);
			XtVaGetValues(label, 
						  XmNwidth, &width,
						  0);
			
			if (m_labelWidth < width)
				m_labelWidth = width;
			
			/* TextF
			 */
			ac = 0;
			m_textFs[i] = fe_CreateTextField(stripForm[i], 
											 (char *) genTabLabels[i],
											 av, ac);
			XtVaSetValues(m_textFs[i], 
						  XmNleftAttachment, XmATTACH_WIDGET,
						  XmNleftWidget, label,
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_FORM,
						  XmNbottomAttachment, XmATTACH_FORM,
						  0);
			XtManageChild(m_textFs[i]);
		}/* if */
		else {
			/* the toggle */
			XmString xmstr;
			xmstr = XmStringCreate(genTabLabels[i],
								   XmFONTLIST_DEFAULT_TAG);
			ac = 0;
			XtSetArg (av[ac], XmNlabelString, xmstr); ac++;
			m_toggles[i-ABDIR_LAST] = 
				XmCreateToggleButton(stripForm[i], genTabLabels[i], 
									 av, ac);
			XtVaSetValues(m_toggles[i-ABDIR_LAST], 
						  XmNleftAttachment, XmATTACH_FORM,
						  // XmNleftOffset, m_labelWidth,
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_NONE,
						  XmNbottomAttachment, XmATTACH_FORM,
						  0);
			if (i == (ABDIR_LAST+ABDIR_USEPASSWD)) {
				XtAddCallback(m_toggles[i-ABDIR_LAST], 
							  XmNvalueChangedCallback, 
							  XFE_ABDirGenTabView::usePasswdCallback, this);
				
			}/* if */
			XtManageChild(m_toggles[i-ABDIR_LAST]);
			
		}/* else */
		
		if (i == 0)
			XtVaSetValues(stripForm[i], 
						  XmNleftAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_FORM,
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNtopOffset, 10,
						  XmNleftOffset, 3,
						  XmNrightOffset, 3,
						  0);
		else if (sep)
			XtVaSetValues(stripForm[i], 
						  XmNleftAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_FORM,
						  XmNtopAttachment, XmATTACH_WIDGET,
						  XmNtopWidget, sep,
						  XmNtopOffset, 3,
						  XmNleftOffset, 3,
						  XmNrightOffset, 3,
						  0);
		else
			XtVaSetValues(stripForm[i], 
						  XmNleftAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_FORM,
						  XmNtopAttachment, XmATTACH_WIDGET,
						  XmNtopWidget, stripForm[i-1],
						  XmNtopOffset, 3,
						  XmNleftOffset, i==(ABDIR_LAST+ABDIR_SAVEPASSWD)?25:3,
						  XmNrightOffset, 3,
						  0);
		
		XtManageChild(stripForm[i]);
		
		if (i == ABDIR_SEARCHROOT) {
			for (int j=0; j <= ABDIR_SEARCHROOT;j++)
				XtVaSetValues(m_labels[j], 
							  XmNwidth, m_labelWidth,
							  0);
			m_labelWidth = 0;
		}/* if */
		else if (i == ABDIR_MAXHITS)
			for (int j=ABDIR_PORTNUMBER; j <= ABDIR_MAXHITS; j++)
				XtVaSetValues(m_labels[j], 
							  XmNwidth, m_labelWidth,
							  0);
	}/* for i */
}

XFE_ABDirGenTabView::~XFE_ABDirGenTabView()
{
}

void 
XFE_ABDirGenTabView::usePasswdCallback(Widget w, XtPointer clientData, 
									   XtPointer callData)
{
	XFE_ABDirGenTabView *obj = (XFE_ABDirGenTabView *) clientData;
	obj->usePasswdCB(w, callData);
}

void
XFE_ABDirGenTabView::usePasswdCB(Widget, XtPointer callData)
{
	XmToggleButtonCallbackStruct *cdata = 
		(XmToggleButtonCallbackStruct *) callData;

	XtSetSensitive(m_toggles[ABDIR_SAVEPASSWD], cdata->set);
}

void
XFE_ABDirGenTabView::setDlgValues()
{
	XFE_ABDirPropertyDlg *dlg = (XFE_ABDirPropertyDlg *)getToplevel();
	DIR_Server *dir = dlg->getDir();

	if (dir) {
		/* set
		 */
		fe_SetTextField(m_textFs[ABDIR_DESCRIPTION], 
						dir->description?dir->description:"");
		if (dir->dirType == PABDirectory) {
#if 1
			int i = 0;
			for (i=ABDIR_LDAPSERVER; i < ABDIR_LAST; i++) {
				fe_SetTextField(m_textFs[i], "");
				XtSetSensitive(m_textFs[i], False);
				XtSetSensitive(m_labels[i], False);
			}/* for i */
			for (i = ABDIR_SECUR; i < ABDIR_SECLAST; i++) {
				XmToggleButtonSetState(m_toggles[i], False, FALSE);
				XtSetSensitive(m_toggles[i], False);
			}/* for i */
#else
			fe_SetTextField(m_textFs[ABDIR_LDAPSERVER], 
							"");
			fe_SetTextField(m_textFs[ABDIR_SEARCHROOT], 
							"");
			fe_SetTextField(m_textFs[ABDIR_PORTNUMBER], 
							"");
			fe_SetTextField(m_textFs[ABDIR_MAXHITS], 
							"");
			XtSetSensitive(m_textFs[ABDIR_LDAPSERVER], False);
			XtSetSensitive(m_textFs[ABDIR_SEARCHROOT], False);
			XtSetSensitive(m_textFs[ABDIR_PORTNUMBER], False);
			XtSetSensitive(m_textFs[ABDIR_MAXHITS], False);
			XmToggleButtonSetState(m_toggles[ABDIR_SECUR], False, FALSE);
			XtSetSensitive(m_toggles[ABDIR_SECUR], False);
			XmToggleButtonSetState(m_toggles[ABDIR_SAVEPASSWD], False, FALSE);
			XtSetSensitive(m_toggles[ABDIR_SAVEPASSWD], False);
	
			XmToggleButtonSetState(m_toggles[ABDIR_USEPASSWD], False, TRUE);
			XtSetSensitive(m_toggles[ABDIR_USEPASSWD], False);
#endif
		}
		else {
			fe_SetTextField(m_textFs[ABDIR_LDAPSERVER], 
							dir->serverName?dir->serverName:"");
			fe_SetTextField(m_textFs[ABDIR_SEARCHROOT], 
							dir->searchBase?dir->searchBase:"");
			char tmp[16];
			XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							"%d",
							dir->port);
			fe_SetTextField(m_textFs[ABDIR_PORTNUMBER], 
							tmp);
		
			XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							"%d",
							dir->maxHits);
			fe_SetTextField(m_textFs[ABDIR_MAXHITS], 
							tmp);
			
			XmToggleButtonSetState(m_toggles[ABDIR_SECUR], dir->isSecure, 
								   FALSE);
			XmToggleButtonSetState(m_toggles[ABDIR_SAVEPASSWD], dir->savePassword,
								   FALSE);		
			XmToggleButtonSetState(m_toggles[ABDIR_USEPASSWD], dir->enableAuth, 
								   TRUE);		
			XtSetSensitive(m_toggles[ABDIR_SAVEPASSWD], dir->enableAuth);
		}
	}/* if */
	else {
		/* clear
		 */
		fe_SetTextField(m_textFs[ABDIR_DESCRIPTION], 
						"");
		fe_SetTextField(m_textFs[ABDIR_LDAPSERVER], 
						"");
		fe_SetTextField(m_textFs[ABDIR_SEARCHROOT], 
						"");
		fe_SetTextField(m_textFs[ABDIR_PORTNUMBER], 
						"");
		fe_SetTextField(m_textFs[ABDIR_MAXHITS], 
						"");

		XmToggleButtonSetState(m_toggles[ABDIR_SECUR], False, FALSE);
		XmToggleButtonSetState(m_toggles[ABDIR_SAVEPASSWD], False, FALSE);	
		XmToggleButtonSetState(m_toggles[ABDIR_USEPASSWD], False, TRUE);
		XtSetSensitive(m_toggles[ABDIR_SAVEPASSWD], False);
	}/* else */
}

void 
XFE_ABDirGenTabView::getDlgValues()
{
	XFE_ABDirPropertyDlg *dlg = (XFE_ABDirPropertyDlg *)getToplevel();
	DIR_Server *dir = dlg->getDir();

	/* setting up the defaults 
	 */
	char *tmp;

	tmp = fe_GetTextField(m_textFs[ABDIR_DESCRIPTION]);
	if (tmp && strlen(tmp))
		dir->description = tmp;
	else
		dir->description = NULL;

	tmp = fe_GetTextField(m_textFs[ABDIR_LDAPSERVER]);
	if (tmp && strlen(tmp))
		dir->serverName = tmp;
	else
		dir->serverName = NULL;

	tmp = fe_GetTextField(m_textFs[ABDIR_SEARCHROOT]);
	if (tmp && strlen(tmp))
		dir->searchBase = tmp;
	else
		dir->searchBase = NULL;


	tmp = fe_GetTextField(m_textFs[ABDIR_PORTNUMBER]);
	if (tmp && strlen(tmp))
		sscanf(tmp, "%d", &(dir->port));
	else
		dir->port = 0;


	tmp = fe_GetTextField(m_textFs[ABDIR_MAXHITS]);
	if (tmp && strlen(tmp))
		sscanf(tmp, "%d", &(dir->maxHits));
	else
		dir->maxHits = 0;

	dir->isSecure = XmToggleButtonGetState(m_toggles[ABDIR_SECUR]);
	dir->enableAuth = XmToggleButtonGetState(m_toggles[ABDIR_USEPASSWD]);
	dir->savePassword = XmToggleButtonGetState(m_toggles[ABDIR_SAVEPASSWD]);
}
