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
   MailDownloadFrame.cpp -- class definition for the mail download window.
   Created: Chris Toshok <toshok@netscape.com>, 22-Jan-97
 */



#include "MailDownloadFrame.h"
#include "MNView.h"
#include "MozillaApp.h"
#include "Dashboard.h"
#include "Logo.h"

#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>

#include "xpgetstr.h"

extern int XFE_DOWNLOADING_NEW_MESSAGES;
extern int XFE_PURGING_NEWS_MESSAGES;

extern "C" {
  void fe_set_scrolled_default_size(MWContext *context);
}

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

const char *XFE_MailDownloadView::pastPasswordCheck = "XFE_MailDownloadView::pastPasswordCheck";
const char *XFE_MailDownloadView::progressDone = "XFE_MailDownloadView::progressDone";

XFE_MailDownloadView::XFE_MailDownloadView(XFE_Component *toplevel_component, 
										   Widget parent, 
										   XFE_View *parent_view, 
										   MWContext *context, 
										   MSG_Pane *parent_pane,
										   MSG_Pane *p)
	: XFE_MNView(toplevel_component, parent_view, context, p)
{
	Widget form;
	XmString xmstr = XmStringCreate(XP_GetString(XFE_DOWNLOADING_NEW_MESSAGES), XmFONTLIST_DEFAULT_TAG);
	XmFontList fontList;

	form = XtCreateWidget("topArea",
						  xmFormWidgetClass,
						  parent,
						  NULL, 0);

	m_label = XtVaCreateManagedWidget("label",
									xmLabelWidgetClass,
									form,
									XmNlabelString, xmstr,
									NULL);

	m_logo = new XFE_Logo((XFE_Frame*)toplevel_component, form, "logo");

	XtVaGetValues(m_label,
				  XmNfontList, &fontList,
				  NULL);

	// we need more space, or else the window is *tiny*
	XtVaSetValues(m_label,
				  XmNwidth, XmStringWidth(fontList, xmstr) * 2,
				  NULL);

	XmStringFree(xmstr);

	/* now do the attachments. */

	XtVaSetValues(m_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_logo->getBaseWidget(),
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_logo->getBaseWidget(),
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, m_logo->getBaseWidget(),
				  NULL);

	XtVaSetValues(m_logo->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);

	m_logo->show();

	if (!p)
		setPane(MSG_CreateProgressPane(m_contextData, XFE_MNView::getMaster(), parent_pane));

	setBaseWidget(form);
}

XFE_MailDownloadView::~XFE_MailDownloadView()
{
	destroyPane();

	XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);
}

void
XFE_MailDownloadView::startDownload()
{
	XmString xmstr = XmStringCreate(XP_GetString(XFE_DOWNLOADING_NEW_MESSAGES), XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(m_label, XmNlabelString, xmstr, 0);
	XmStringFree(xmstr);

	XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);
	
	MSG_Command (m_pane, MSG_GetNewMail, NULL, 0);
}

void
XFE_MailDownloadView::startNewsDownload()
{
	XmString xmstr = XmStringCreate(XP_GetString(XFE_DOWNLOADING_NEW_MESSAGES), XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(m_label, XmNlabelString, xmstr, 0);
	XmStringFree(xmstr);

	XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);
	
	MSG_Command (m_pane, MSG_GetNextChunkMessages, NULL, 0);
}

void
XFE_MailDownloadView::cleanUpNews()
{
    // Change the label string, since we're not actually downloading:
	XmString xmstr = XmStringCreate(XP_GetString(XFE_PURGING_NEWS_MESSAGES),
                                    XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(m_label, XmNlabelString, xmstr, 0);
	XmStringFree(xmstr);

    XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);
	
    MSG_CleanupFolders(m_pane);
}

void
XFE_MailDownloadView::paneChanged(XP_Bool,
								  MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
								  int32)
{
	if (notify_code == MSG_PanePastPasswordCheck)
		{
			D( printf ("Showing maildownload frame.\n");)
			notifyInterested(pastPasswordCheck);
		}
	else if (notify_code == MSG_PaneProgressDone)
		{
			notifyInterested(progressDone);
		}
}

XFE_Logo *
XFE_MailDownloadView::getLogo()
{
	return m_logo;
}

XFE_MailDownloadFrame::XFE_MailDownloadFrame(Widget			toplevel,
											 XFE_Frame *	parent_frame,
											 MSG_Pane *		parent_pane)
	: XFE_Frame("MessageDownload",			// name
				toplevel,					// top level
				parent_frame,				// parent frame
				FRAME_MAILNEWS_DOWNLOAD,	// frame type
				NULL,						// chrome
				False,						// have html display
				False,						// have menu bar
				False,						// have toolbar
				False,						// have dashboard (we'll do it)
				True)						// destroy on close
{
	Widget form;

	XFE_MailDownloadView *v = new XFE_MailDownloadView(this, getChromeParent(),
													   NULL, m_context, parent_pane);
	XFE_Component *belowView;

	m_being_deleted = False;

	form = XtVaCreateManagedWidget("stopButtonForm",
								   xmFormWidgetClass,
								   getChromeParent(),
								   XmNfractionBase, 8,
								   NULL);

	m_stopButton = XtVaCreateManagedWidget(xfeCmdStopLoading,
										   xmPushButtonWidgetClass,
										   form,
										   XmNleftAttachment, XmATTACH_POSITION,
										   XmNleftPosition, 3,
										   XmNrightAttachment, XmATTACH_POSITION,
										   XmNrightPosition, 5,
										   XmNtopAttachment, XmATTACH_NONE,
										   XmNbottomAttachment, XmATTACH_FORM,
										   NULL);

	XtAddCallback(m_stopButton, XmNactivateCallback, activate_cb, this);

	belowView = new XFE_Component(form);

	setView(v);
	setBelowViewArea(belowView);

	m_dashboard = new XFE_Dashboard(this,				// top level
									getChromeParent(),	// parent
									this,				// parent frame
									False);				// have task bar

	m_dashboard->setShowStatusBar(True);
	m_dashboard->setShowProgressBar(True);

	v->registerInterest(XFE_MailDownloadView::pastPasswordCheck,
						this,
						(XFE_FunctionNotification)pastPassword_cb);
	v->registerInterest(XFE_MailDownloadView::progressDone,
						this,
						(XFE_FunctionNotification)progressDone_cb);

	fe_set_scrolled_default_size(m_context);
	v->show();
	belowView->show();
	m_dashboard->show();

	// Register the logo animation notifications with ourselves
	// We need to do this, cause the XFE_Frame class only does so
	// if the have_toolbars ctor parameter is true and in our case
	// it is not.
	registerInterest(XFE_Frame::logoStartAnimation,
					 this,
					 &XFE_Frame::logoAnimationStartNotice_cb);
	
	registerInterest(XFE_Frame::logoStopAnimation,
					 this,
					 &XFE_Frame::logoAnimationStopNotice_cb);
}

XFE_MailDownloadFrame::~XFE_MailDownloadFrame()
{
	// Unregister the logo animation notifications with ourselves
	// We need to do this, cause the XFE_Frame class only does so
	// if the have_toolbars ctor parameter is true and in our case
	// it is not.
	unregisterInterest(XFE_Frame::logoStartAnimation,
					   this,
					   &XFE_Frame::logoAnimationStartNotice_cb);
	
	unregisterInterest(XFE_Frame::logoStopAnimation,
					   this,
					   &XFE_Frame::logoAnimationStopNotice_cb);
}

XFE_Logo *
XFE_MailDownloadFrame::getLogo()
{
	XFE_Logo *				logo = NULL;
	XFE_MailDownloadView *	view = (XFE_MailDownloadView *) getView();
	
	if (view)
	{
		logo = view->getLogo();
	}

	return logo;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_MailDownloadFrame::configureLogo()
{
}

XP_Bool
XFE_MailDownloadFrame::isCommandEnabled(CommandType cmd,
										void */*calldata*/, XFE_CommandInfo*)
{
	if (cmd == xfeCmdStopLoading)
		return fe_IsContextStoppable(m_context);
	else
		return False;
}

void
XFE_MailDownloadFrame::doCommand(CommandType cmd,
								 void */*calldata*/, XFE_CommandInfo*)
{
	if (cmd == xfeCmdStopLoading)
		{
			D( printf ("Canceling.\n"); )
			XP_InterruptContext(m_context);
			// allConnectionsComplete will destroy this frame.
		}
}

XP_Bool
XFE_MailDownloadFrame::handlesCommand(CommandType cmd,
									  void */*calldata*/, XFE_CommandInfo*)
{
	if (cmd == xfeCmdStopLoading)
		return True;
	else
		return False;
}

void
XFE_MailDownloadFrame::app_delete_response()
{
	D(printf ("In XFE_MailDownloadFrame::app_delete_response()\n");)
	if (!m_being_deleted)
		m_being_deleted = TRUE;
	XFE_Frame::app_delete_response();
}

void
XFE_MailDownloadFrame::allConnectionsComplete()
{
	D(printf ("All Connections Complete in mail download frame.\n");)

	if (!m_being_deleted)
		app_delete_response();
}

void
XFE_MailDownloadFrame::show()
{
	XFE_Frame *active_frame = XFE_Frame::getActiveFrame();

	if (active_frame)
		{
			Position x, y;
			Widget parent = active_frame->getBaseWidget();
			Screen *screen = XtScreen(m_widget);
			Dimension screen_width = WidthOfScreen (screen);
			Dimension screen_height = HeightOfScreen (screen);
			Dimension parent_width = 0;
			Dimension parent_height = 0;
			Dimension child_width = 0;
			Dimension child_height = 0;
			
			/* we were already realized in the startDownload function below.
			   Being realized is a requirement for some of the stuff we
			   have to do here. */

			XtVaGetValues(m_widget,
						  XtNwidth, &child_width, XtNheight, &child_height, 0);
			XtVaGetValues (parent,
						   XtNwidth, &parent_width, XtNheight, &parent_height, 0);

			x = (((Position)parent_width) - ((Position)child_width)) / 2;
			y = (((Position)parent_height) - ((Position)child_height)) / 2;
			XtTranslateCoords (parent, x, y, &x, &y);

			if ((Dimension) (x + child_width) > screen_width)
				x = screen_width - child_width;
			if (x < 0)
				x = 0;
			
			if ((Dimension) (y + child_height) > screen_height)
				y = screen_height - child_height;
			if (y < 0)
				y = 0;
			
			position(x, y);
		}

	XFE_Frame::show();
}

void
XFE_MailDownloadFrame::startDownload()
{
	XFE_MailDownloadView *v = (XFE_MailDownloadView*)m_view;

	realize();

	v->startDownload();
}

void
XFE_MailDownloadFrame::startNewsDownload()
{
	XFE_MailDownloadView *v = (XFE_MailDownloadView*)m_view;

	realize();

	v->startNewsDownload();
}

void
XFE_MailDownloadFrame::cleanUpNews()
{
	XFE_MailDownloadView *v = (XFE_MailDownloadView*)m_view;

	realize();
    show();

	v->cleanUpNews();
}

void
XFE_MailDownloadFrame::activate_cb(Widget w, XtPointer clientData, XtPointer)
{
	CommandType cmd = Command::intern(XtName(w));
	XFE_MailDownloadFrame *obj = (XFE_MailDownloadFrame*)clientData;

	if (obj->handlesCommand(cmd) && obj->isCommandEnabled(cmd))
		obj->doCommand(cmd);
}

XFE_CALLBACK_DEFN(XFE_MailDownloadFrame, pastPassword)(XFE_NotificationCenter*,
													   void *,
													   void *)
{
	D(printf ("Showing MailDownloadFrame()\n");)
	show();
}

XFE_CALLBACK_DEFN(XFE_MailDownloadFrame, progressDone)(XFE_NotificationCenter*,
													   void *,
													   void *)
{
	D(printf ("Deleting MailDownloadFrame()\n");)
	if (!m_being_deleted)
		app_delete_response();
}
