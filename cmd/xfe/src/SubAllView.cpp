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
   SubAllView.cpp -- 4.x subscribe view, all newsgroup tab.
   Created: Chris Toshok <toshok@netscape.com>, 18-Oct-1996.
   */



#include "SubAllView.h"
#include "NewsServerDialog.h"
#include "Outliner.h"
#include "Command.h"
#include "ViewGlue.h"

#include "xfe.h"
#include "msgcom.h"
#include "xp_mem.h"

#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>
#include "DtWidgets/ComboBox.h"
#include "felocale.h"

#include <Xfe/Xfe.h>

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern int XFE_ALL_NEWSGROUP_TAB;

#define OUTLINER_GEOMETRY_PREF "mail.subscribepane.all_groups.outliner_geometry"

XFE_SubAllView::XFE_SubAllView(XFE_Component *toplevel_component,
							   Widget parent, XFE_View *parent_view,
							   MWContext *context, MSG_Pane *p)
	: XFE_SubTabView(toplevel_component, parent, parent_view, context, XFE_ALL_NEWSGROUP_TAB, p)
{
	int num_columns = 3;
	static int column_widths[] = {40, 3, 9};

	m_outliner = new XFE_Outliner("subscribeAllList",
								  this,
								  getToplevel(),
								  m_form,
								  False, // constantSize
								  True, //hasHeadings
								  num_columns,
								  num_columns,
								  column_widths,
								  OUTLINER_GEOMETRY_PREF);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNvisibleRows, 15,
				  NULL);

	init_outliner_icons(m_outliner->getBaseWidget());

	// the all newsgroups view is hierarchical
	m_outliner->setPipeColumn(OUTLINER_COLUMN_NAME);
	m_outliner->setHideColumnsAllowed( True );
	m_outliner->setMultiSelectAllowed( True );
	m_outliner->setColumnWidth(OUTLINER_COLUMN_SUBSCRIBE, subscribedIcon.width + 2 /* for the outliner's shadow */);
	m_outliner->setColumnResizable(OUTLINER_COLUMN_SUBSCRIBE, False);
	
	m_newsgroupForm = XtCreateManagedWidget("newsgroupForm",
											xmFormWidgetClass,
											m_form,
											NULL, 0);
  
	m_newsgroupLabel = XtVaCreateManagedWidget("newsgroupLabel",
											   xmLabelWidgetClass,
											   m_newsgroupForm,
											   XmNleftAttachment, XmATTACH_FORM,
											   XmNrightAttachment, XmATTACH_NONE,
											   XmNbottomAttachment, XmATTACH_FORM,
											   XmNtopAttachment, XmATTACH_FORM,
											   NULL);
  
	m_newsgroupText = fe_CreateTextField(m_newsgroupForm,
										 "newsgroupText",
										 NULL, 0);
	XtVaSetValues(m_newsgroupText,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_newsgroupLabel,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	XtManageChild(m_newsgroupText);

	m_buttonForm = XtCreateManagedWidget("buttonForm",
										 xmFormWidgetClass,
										 m_form,
										 NULL, 0);

	m_subscribeButton = XtVaCreateManagedWidget(xfeCmdToggleSubscribe,
												xmPushButtonWidgetClass,
												m_buttonForm,
												XmNleftAttachment, XmATTACH_FORM,
												XmNrightAttachment, XmATTACH_FORM,
												XmNtopAttachment, XmATTACH_FORM,
												XmNbottomAttachment, XmATTACH_NONE,
												NULL);
	m_sep1 = XtVaCreateManagedWidget("sep1",
									 xmSeparatorWidgetClass,
									 m_buttonForm,
									 XmNleftAttachment, XmATTACH_FORM,
									 XmNrightAttachment, XmATTACH_FORM,
									 XmNtopAttachment, XmATTACH_WIDGET,
									 XmNtopWidget, m_subscribeButton,
									 XmNbottomAttachment, XmATTACH_NONE,
									 NULL);

	m_expandallButton = XtVaCreateManagedWidget(xfeCmdExpandAll,
												xmPushButtonWidgetClass,
												m_buttonForm,
												XmNleftAttachment, XmATTACH_FORM,
												XmNrightAttachment, XmATTACH_FORM,
												XmNtopAttachment, XmATTACH_WIDGET,
												XmNtopWidget, m_sep1,
												XmNbottomAttachment, XmATTACH_NONE,
												NULL);

	m_collapseallButton = XtVaCreateManagedWidget(xfeCmdCollapseAll,
												  xmPushButtonWidgetClass,
												  m_buttonForm,
												  XmNleftAttachment, XmATTACH_FORM,
												  XmNrightAttachment, XmATTACH_FORM,
												  XmNtopAttachment, XmATTACH_WIDGET,
												  XmNtopWidget, m_expandallButton,
												  XmNbottomAttachment, XmATTACH_NONE,
												  NULL);

	m_sep2 = XtVaCreateManagedWidget("sep2",
									 xmSeparatorWidgetClass,
									 m_buttonForm,
									 XmNleftAttachment, XmATTACH_FORM,
									 XmNrightAttachment, XmATTACH_FORM,
									 XmNtopAttachment, XmATTACH_WIDGET,
									 XmNtopWidget, m_collapseallButton,
									 XmNbottomAttachment, XmATTACH_NONE,
									 NULL);
				 
	m_getdeletionsButton = XtVaCreateManagedWidget(xfeCmdFetchGroupList,
												   xmPushButtonWidgetClass,
												   m_buttonForm,
												   XmNleftAttachment, XmATTACH_FORM,
												   XmNrightAttachment, XmATTACH_FORM,
												   XmNtopAttachment, XmATTACH_WIDGET,
												   XmNtopWidget, m_sep2,
												   XmNbottomAttachment, XmATTACH_NONE,
												   NULL);
  
	m_stopButton = XtVaCreateManagedWidget(xfeCmdStopLoading,
										   xmPushButtonWidgetClass,
										   m_buttonForm,
										   XmNleftAttachment, XmATTACH_FORM,
										   XmNrightAttachment, XmATTACH_FORM,
										   XmNtopAttachment, XmATTACH_WIDGET,
										   XmNtopWidget, m_getdeletionsButton,
										   XmNbottomAttachment, XmATTACH_NONE,
										   NULL);

	m_addserverButton = XtVaCreateManagedWidget(xfeCmdAddNewsServer,
												xmPushButtonWidgetClass,
												m_buttonForm,
												XmNleftAttachment, XmATTACH_FORM,
												XmNrightAttachment, XmATTACH_FORM,
												XmNtopAttachment, XmATTACH_NONE,
												XmNbottomAttachment, XmATTACH_FORM,
												NULL);

	m_serverForm = XtCreateManagedWidget("serverForm",
										 xmFormWidgetClass,
										 m_form,
										 NULL, 0);

	m_serverLabel = XtVaCreateManagedWidget("serverLabel",
											xmLabelWidgetClass,
											m_serverForm,
											XmNleftAttachment, XmATTACH_FORM,
											XmNrightAttachment, XmATTACH_NONE,
											XmNtopAttachment, XmATTACH_FORM,
											XmNbottomAttachment, XmATTACH_FORM,
											NULL);

	initializeServerCombo(m_serverForm);
	XtVaSetValues(m_serverCombo,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_serverLabel,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_buttonForm,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_newsgroupForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(m_serverForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_newsgroupForm,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_serverForm,
				  NULL);

	XtAddCallback(m_newsgroupText, XmNvalueChangedCallback, newsgroup_typedown_callback, this);
	XtAddCallback(m_subscribeButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_expandallButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_collapseallButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_getdeletionsButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_stopButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_addserverButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);

	m_outliner->show();  

	// setBaseWidget is done in SubTabView.
}

XFE_SubAllView::~XFE_SubAllView()
{
}

Boolean
XFE_SubAllView::handlesCommand(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdExpandAll
		|| command == xfeCmdCollapseAll
		|| command == xfeCmdFetchGroupList
		|| command == xfeCmdAddNewsServer)
		{
			return True;
		}
	else
		{
			return XFE_SubTabView::handlesCommand(command, calldata);
		}
}

void
XFE_SubAllView::updateButtons()
{
#define S(x) XtSetSensitive((x), isCommandEnabled(Command::intern(XtName((x)))))
	S(m_subscribeButton);
	S(m_expandallButton);
	S(m_collapseallButton);
	S(m_getdeletionsButton);
	S(m_stopButton);
#undef S
}

void
XFE_SubAllView::defaultFocus()
{
	XmProcessTraversal(m_newsgroupText, XmTRAVERSE_CURRENT);
}

void
XFE_SubAllView::doCommand(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdFetchGroupList)
		m_outliner->deselectAllItems();
    // Then fall through -- MNView maps this cmd to MSG_FetchGroupList.

	if (command == xfeCmdAddNewsServer)
		{
			XFE_NewsServerDialog *d = new XFE_NewsServerDialog(getToplevel()->getBaseWidget(),
															   "addServer",
															   ViewGlue_getFrame(m_contextData));
			XP_Bool ok_pressed = d->post();

			if (ok_pressed)
				{
					const char *serverName = d->getServer();
					int serverPort = d->getPort();
					XP_Bool isSecure = d->isSecure();
					MSG_NewsHost *newshost;

					newshost = MSG_CreateNewsHost(XFE_MNView::getMaster(),
                                                  serverName,
                                                  isSecure,
                                                  serverPort);
					MSG_SubscribeSetHost(m_pane,
                                         MSG_GetMSGHostFromNewsHost(newshost));
					
					syncServerList();
					syncServerCombo();
					
					serverSelected();
				}

			delete d;
		}
	else
		{
			XFE_SubTabView::doCommand(command, calldata);
		}
}

void
XFE_SubAllView::serverSelected()
{
	doCommand(xfeCmdFetchGroupList);

	notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_SubAllView::newsgroup_typedown()
{
	MSG_ViewIndex index;
	const char *str;

	/* bstell: do we need to fix this ? */
	str = fe_GetTextField(m_newsgroupText);

	index = MSG_SubscribeFindFirst(m_pane, str);

	XtFree((char*)str);

	if (index == MSG_VIEWINDEXNONE)
		{
			m_outliner->deselectAllItems();
		}
	else
		{
			m_outliner->selectItemExclusive(index);
			m_outliner->makeVisible(index);
		}
}

void
XFE_SubAllView::newsgroup_typedown_callback(Widget /*w*/,
											XtPointer clientData,
											XtPointer /*callData*/)
{
	XFE_SubAllView *obj = (XFE_SubAllView*)clientData;
  
	obj->newsgroup_typedown();
}

int
XFE_SubAllView::getButtonsMaxWidth()
{
  return XfeVaGetWidestWidget(m_subscribeButton, 
							  m_expandallButton, 
							  m_collapseallButton,
							  m_getdeletionsButton,
							  m_stopButton,
							  m_addserverButton,
							  NULL);
}

void
XFE_SubAllView::setButtonsWidth(int width)
{
  Arg args[1];
  
  XtSetArg(args[0], XmNwidth, width);

  XtSetValues(m_subscribeButton, args, 1);
  XtSetValues(m_expandallButton, args, 1);
  XtSetValues(m_collapseallButton, args, 1);
  XtSetValues(m_getdeletionsButton, args, 1);
  XtSetValues(m_stopButton, args, 1);
  XtSetValues(m_addserverButton, args, 1);
}
