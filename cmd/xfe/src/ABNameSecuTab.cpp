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
   ABNameSecuTabView.cpp -- class definition for ABNameSecuTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "ABNameSecuTab.h"

#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/LabelG.h> 

#include "xfe.h"
#include "xpgetstr.h"

extern int XFE_AB_NAME_SECURITY_TAB;
extern int XFE_AB_SECUR_YES;
extern int XFE_AB_SECUR_NO;
extern int XFE_AB_SECUR_EXPIRED;
extern int XFE_AB_SECUR_REVOKED;
extern int XFE_AB_SECUR_YOU_YES;
extern int XFE_AB_SECUR_YOU_NO;
extern int XFE_AB_SECUR_YOU_EXPIRED;
extern int XFE_AB_SECUR_YOU_REVOKED;
extern int XFE_AB_SECUR_SHOW;
extern int XFE_AB_SECUR_GET;

XFE_ABNameSecuTabView::XFE_ABNameSecuTabView(XFE_Component *top,
					     XFE_View *view):
  XFE_PropertyTabView(top, view, XFE_AB_NAME_SECURITY_TAB)
{
  Widget topForm = getBaseWidget();

  /* m_securityLabel
   */
  m_securLabel = XtVaCreateManagedWidget("securLabel",
										 xmLabelGadgetClass, topForm,
										 XmNalignment, XmALIGNMENT_BEGINNING, 
										 NULL);
  XmString info = XmStringCreateLtoR(XP_GetString(XFE_AB_SECUR_YES),
									 XmFONTLIST_DEFAULT_TAG);

  XtVaSetValues(m_securLabel,
				XmNlabelString, info,
				XmNleftAttachment, XmATTACH_FORM,
				XmNtopAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM,
				XmNbottomAttachment, XmATTACH_NONE,
				XmNtopOffset, 10,
				XmNrightOffset, 10,
				XmNbottomOffset, 10,
				XmNleftOffset, 10,
				0);

  /* Show Certificate button
   */
  Arg    av[20];
  int    ac = 0;

  XtSetArg (av[ac], XmNwidth, 120); ac++;
  //XtSetArg (av[ac], XmNrecomputeSize, False); ac++;
  m_showCertiBtn = XmCreatePushButton(topForm, 
				      "showSecurBtn", 
				      av, ac);
  fe_SetString(m_showCertiBtn, 
			   XmNlabelString, XP_GetString(XFE_AB_SECUR_SHOW));

  Dimension widthF;
  XtVaGetValues(m_securLabel, 
				XmNwidth, &widthF,
				0);
  Dimension widthp;
  XtVaGetValues(m_showCertiBtn, 
				XmNwidth, &widthp,
				0);

  int leftoffset = (widthF-widthp)/2;
  XtVaSetValues(m_showCertiBtn,
				XmNleftAttachment, XmATTACH_FORM,
				XmNleftOffset, leftoffset, 
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, m_securLabel,
				XmNtopOffset, 50, 
				XmNrightAttachment, XmATTACH_NONE,
				XmNbottomAttachment, XmATTACH_NONE,
				NULL);
  XtManageChild(m_showCertiBtn);

  XtAddCallback(m_showCertiBtn,
				XmNactivateCallback, 
				XFE_ABNameSecuTabView::showCallback, 
				this);
}

XFE_ABNameSecuTabView::~XFE_ABNameSecuTabView()
{
}

void XFE_ABNameSecuTabView::setDlgValues()
{
}

void XFE_ABNameSecuTabView::apply()
{
}

void XFE_ABNameSecuTabView::getDlgValues()
{
}

/*
 * Callbacks for outside world
 */
void
XFE_ABNameSecuTabView::showCallback(Widget w, 
				   XtPointer clientData, 
				   XtPointer callData)
{
  XFE_ABNameSecuTabView *obj = (XFE_ABNameSecuTabView *) clientData;
  obj->showCB(w, callData);
}

void XFE_ABNameSecuTabView::showCB(Widget, XtPointer)
{
}
