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
   MailFilterRulesDlg.cpp -- 
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */



#include "MailFilterRulesDlg.h"
#include "MailFilterDlg.h"
#include "Outliner.h"
#include "Outlinable.h"

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>
#include <Xm/DialogS.h>

extern "C" {
#include "xfe.h"
};

#include "xpgetstr.h"

XFE_MailFilterRulesDlg::XFE_MailFilterRulesDlg(XFE_Component *subscriber,
											   Widget     parent,
											   char      *name,
											   Boolean    modal,
											   MWContext *context,
											   XP_Bool    isNew):
	XFE_ViewDialog((XFE_View *) 0, parent, name,
				   context,
				   True, /* ok */
				   True, /* cancel */
				   False, /* help */
				   False, /* apply; remove */
				   False, /* separator */
				   modal),
	m_subscriber(subscriber)
{

  Arg av [20];
  int ac = 0;

  //
  // we don't want a default value, since return does other stuff for
  // us.
  XtVaSetValues(m_chrome, /* the dialog */
				XmNallowShellResize, True,
				XmNdefaultButton, NULL,
				NULL);

  /* Form: m_chrome is the dialog 
   */
  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  Widget form = XmCreateForm (m_chrome, "editContainer", av, ac);
  XtManageChild (form);

  /* Search UI
   */
  XFE_MailFilterRulesView *view = 
	  new XFE_MailFilterRulesView(this,     /* toplevel_component */
								  form,     /* parent */
								  NULL,     /* parent_view */
								  m_context /* context */,
								  isNew);
  setView(view);

  /* Notification stuff
   */
  registerInterest(XFE_MailFilterRulesView::listChanged,
				   (XFE_NotificationCenter *) subscriber,
				   (XFE_FunctionNotification) XFE_MailFilterDlg::listChanged_cb);
}

XFE_MailFilterRulesDlg::~XFE_MailFilterRulesDlg()
{
	/* need to delete resources here; 
	 * check if this destructor ever gets called
	 */
  /* Notification stuff
   */
  unregisterInterest(XFE_MailFilterRulesView::listChanged,
				   (XFE_NotificationCenter *) m_subscriber,
				   (XFE_FunctionNotification) XFE_MailFilterDlg::listChanged_cb);
	
}

void XFE_MailFilterRulesDlg::cancel()
{
	((XFE_MailFilterRulesView *) m_view)->cancel();
}

void XFE_MailFilterRulesDlg::ok()
{
	((XFE_MailFilterRulesView *) m_view)->apply();
	cancel();
}

/* C API
 */
extern "C"  void
fe_showMailFilterRulesDlg(XFE_Component *subscriber,
						  Widget toplevel, 
						  MWContext *context,
						  MSG_FilterList *list, 
						  MSG_FilterIndex at,
						  XP_Bool isNew)
{
	/* recreate every time -> delete for eaech cancel
	 */
	XFE_MailFilterRulesDlg *dlg =  
		new XFE_MailFilterRulesDlg(subscriber, 
								   toplevel, 
								   "editFilterDialog",
								   True,
								   context,
								   isNew);
	dlg->setDlgValues(list, at);
	dlg->show();
}

