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

/* -*- Mode: C++; tab-width: 4 -*-
   ViewDashBDlg.cpp -- View dialog with a dashboard.
   Created: Tao Cheng <tao@netscape.com>, 27-apr-98
 */


#include "ViewDashBDlg.h"
#include <Xfe/Xfe.h>

XFE_ViewDashBDlg::XFE_ViewDashBDlg(Widget     parent, 
								   char      *name, 
								   MWContext *context,
								   Boolean    ok,
								   Boolean    cancel,
								   Boolean    help,
								   Boolean    apply,
								   Boolean    modal)
	: XFE_ViewDialog(NULL, parent, name,
					 context,
					 FALSE, // ok
					 FALSE, // cancel
					 FALSE, // help
					 FALSE, // apply
					 FALSE, // separator
					 FALSE, // modal
					 create_chrome_widget(parent, name, 
										  TRUE, 
										  TRUE, 
										  FALSE, 
										  FALSE, 
										  FALSE, 
										  modal))
{
	m_okBtn = 0;

	m_dashboard = new XFE_Dashboard(this,
									m_chrome,
                                    NULL,   // XFE_Frame
									False 	/* taskbar */);

	m_dashboard->setShowStatusBar(True);
	m_dashboard->setShowProgressBar(True);

	// create the button area
	m_buttonArea = createButtonArea(m_chrome, ok, cancel, help, apply);

	XtVaSetValues(m_dashboard->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_buttonArea,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_dashboard->getBaseWidget(),
				  XmNmarginWidth, 0,
				  XmNmarginHeight, 0,
				  NULL);
				  
}

void 
XFE_ViewDashBDlg::attachView()
{
	// subclass create the view area
	XP_ASSERT(m_aboveButtonArea);
	XtVaSetValues(m_aboveButtonArea,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 3,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNrightOffset, 3,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNtopOffset, 3,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_buttonArea,
				  NULL);

	XtManageChild(m_buttonArea);
	m_dashboard->show();
}

XFE_ViewDashBDlg::~XFE_ViewDashBDlg()
{
#if 0
	/* Bug 140476: core dump in LDAP search
	 * dashboard shall not be destroyed here
	 */
	if (m_dashboard) {
		Widget w = m_dashboard->getBaseWidget();
		if (w && XfeIsAlive(w))
			XtDestroyWidget(w);
		m_dashboard = 0;
	}/* if */
#endif
}

Widget
XFE_ViewDashBDlg::createButtonArea(Widget parent,
								   Boolean ok, 
								   Boolean cancel,
								   Boolean help, 
								   Boolean /*apply*/)
{
  Widget msgb;
  Widget button;

  msgb = XmCreateMessageBox(parent, "messagebox", NULL, 0);

  /* We have to do this explicitly because of AIX versions come
	 with these buttons by default */
  fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_SEPARATOR));
  if (!ok)
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_OK_BUTTON));
  else
	{
	  m_okBtn = XmMessageBoxGetChild(msgb, XmDIALOG_OK_BUTTON);

	  if (m_okBtn)
		{
		  XtManageChild(m_okBtn);
		  XtAddCallback(msgb, XmNokCallback, ok_cb, this);
		}
	}
  if (!cancel)
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_CANCEL_BUTTON));
  else
	{
	  button = XmMessageBoxGetChild(msgb, XmDIALOG_CANCEL_BUTTON);

	  if (button)
		{
		  XtManageChild(button);
		  XtAddCallback(msgb, XmNcancelCallback, cancel_cb, this);
		}
	}

  if (!help)
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_HELP_BUTTON));
  else
	{
	  button = XmMessageBoxGetChild(msgb, XmDIALOG_HELP_BUTTON);

	  if (button)
		{
		  XtManageChild(button);
		  XtAddCallback(msgb, XmNhelpCallback, help_cb, this);
		}
	}

  return msgb;
}

Widget
XFE_ViewDashBDlg::create_chrome_widget(Widget   parent,    
									   char    *name,
									   Boolean /* ok */,
									   Boolean /* cancel */,
									   Boolean /* help */,
									   Boolean /* apply */,
									   Boolean /* separator */,
									   Boolean  modal)
{
	Visual   *v = 0;
	Colormap  cmap = 0;
	Cardinal  depth = 0;
	Arg       av[20];
	int       ac;
	Widget chrome;

	XtVaGetValues (parent, 
				   XtNvisual, &v, 
				   XtNcolormap, &cmap,
				   XtNdepth, &depth,
				   0);

	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
	XtSetArg (av[ac], XmNtransientFor, parent); ac++;
	if (modal) {
		XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
	}
	XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
	XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
	
	chrome = XmCreateFormDialog(parent, name, av, ac);

	return chrome;
}

