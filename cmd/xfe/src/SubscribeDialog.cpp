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
   SubscribeDialog.cpp -- 4.x Subscribe UI dialog.
   Created: Chris Toshok <toshok@netscape.com>, 16-Oct-96.
 */



#include "SubscribeDialog.h"
#include "SubscribeView.h"
#include "ViewGlue.h"
#include "msgcom.h"

static Widget create_chrome_widget(Widget   parent, char    *name,
								   Boolean  ok, Boolean  cancel,
								   Boolean  help, Boolean  apply,
								   Boolean  separator, Boolean  modal);

static XFE_SubscribeDialog *theDialog;

XFE_SubscribeDialog::XFE_SubscribeDialog(char *name, XFE_NotificationCenter *toplevel,
										 Widget parent, MWContext *context, MSG_Host *host)
	: XFE_ViewDialog(NULL, parent, name,
					 context,
					 FALSE, // ok
					 FALSE, // cancel
					 FALSE, // help
					 FALSE, // apply
					 FALSE, // separator
					 FALSE, // modal
					 create_chrome_widget(parent,
										  name, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE))
{
	XFE_SubscribeView *subscribeview;
	Widget button_area;

    m_okToDestroy = FALSE;

	m_toplevelNotifier = toplevel;

	m_dashboard = new XFE_Dashboard(this,
									m_chrome,
                                    NULL,   // XFE_Frame
									False 	/* taskbar */);

	m_dashboard->setShowStatusBar(True);
	m_dashboard->setShowProgressBar(True);

	subscribeview = new XFE_SubscribeView(this, m_chrome, NULL, m_context, host);

	button_area = createButtonArea(m_chrome, TRUE, TRUE, FALSE, FALSE);

	XtVaSetValues(m_dashboard->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(button_area,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_dashboard->getBaseWidget(),
				  XmNmarginWidth, 0,
				  XmNmarginHeight, 0,
				  NULL);
				  
	XtVaSetValues(subscribeview->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 3,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNrightOffset, 3,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNtopOffset, 3,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, button_area,
				  NULL);

	XtManageChild(button_area);
	m_dashboard->show();
	subscribeview->show();

	setView(subscribeview);
}

XFE_SubscribeDialog::~XFE_SubscribeDialog()
{
    unregisterInterest(XFE_Frame::frameNotBusyCallback,
					   m_toplevelNotifier,
					   (XFE_FunctionNotification)XFE_Frame::updateBusyState_cb,
					   (void*)False);

	// Resotore the original notifier
	m_toplevelNotifier->setForwarder(m_oldForwarder);

	theDialog = NULL;
}

void
XFE_SubscribeDialog::show()
{
	XFE_ViewDialog::show();

	// Save the old notifier
	m_oldForwarder = m_toplevelNotifier->getForwarder();

	// Set up the forwarder
	m_toplevelNotifier->setForwarder(this);

	registerInterest(XFE_Frame::frameNotBusyCallback,
					 m_toplevelNotifier,
					 (XFE_FunctionNotification)XFE_Frame::updateBusyState_cb,
					 (void*)False);
}

Widget
XFE_SubscribeDialog::createButtonArea(Widget parent,
									  Boolean ok, Boolean cancel,
									  Boolean help, Boolean /*apply*/)
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
	  button = XmMessageBoxGetChild(msgb, XmDIALOG_OK_BUTTON);

	  if (button)
		{
		  XtManageChild(button);
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

static Widget
create_chrome_widget(Widget   parent,    
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

void
XFE_SubscribeDialog::ok()
{
    registerInterest(XFE_Frame::allConnectionsCompleteCallback,
                     this,
                     (XFE_FunctionNotification)allConnectionsComplete_cb);
    XFE_ViewDialog::ok();
}

void
fe_showSubscribeDialog(XFE_NotificationCenter *toplevel,
					   Widget parent,
					   MWContext *context,
					   MSG_Host *host)
{
    // The widget tree gets destroyed by ViewDialog's ok_cb and cancel_cb,
    // so we'd better start over if we've shown the dialog before:
    if (theDialog)
        delete theDialog;

    theDialog = new XFE_SubscribeDialog("SubscribeDialog", 
                                        toplevel, parent,
                                        context,
                                        host);
	theDialog->show();
}

// Tells the FEs to bring up the subscribe UI on the given news or imap host.
XP_Bool FE_CreateSubscribePaneOnHost(MSG_Master* /*master*/,
                                     MWContext* parentContext,
                                     MSG_Host* host)
{
    XFE_Component* toplevel = ViewGlue_getFrame(parentContext)->getToplevel();
    fe_showSubscribeDialog(toplevel, toplevel->getBaseWidget(),
                           parentContext, host);
    return True;
}

XFE_CALLBACK_DEFN(XFE_SubscribeDialog, allConnectionsComplete)(XFE_NotificationCenter *,
														  void *,
														  void *)
{
    hide();
}

