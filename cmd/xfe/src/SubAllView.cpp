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

    // create other gadgets

    Widget actionForm = XmCreateRowColumn(m_form, "actionForm", NULL, 0);
    m_addserverButton =
        XmCreatePushButtonGadget(actionForm,xfeCmdAddNewsServer, NULL,0);
    XtManageChild(m_addserverButton);

    // this holds the data at the top of the form -
    // server, newsgroup
    Widget dataForm = XmCreateForm(m_form, "dataForm", NULL, 0);

	Widget serverLabel, newsgroupLabel;
    
    int i=0;
    Widget dataWidgets[6];
    // Server & dropdown
    dataWidgets[i++] = serverLabel =
        XmCreateLabelGadget(dataForm, "serverLabel", NULL, 0);
	initializeServerCombo(dataForm);
    dataWidgets[i++] = m_serverCombo;    

    int server_height = XfeVaGetTallestWidget(serverLabel,
                                              m_serverCombo,
                                              m_addserverButton,
                                              NULL);

    XtVaSetValues(serverLabel,
                  XmNheight, server_height,
                  XmNalignment, XmALIGNMENT_END,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(m_serverCombo,
                  XmNheight, server_height,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, serverLabel,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, serverLabel,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(m_addserverButton,
                  XmNheight, server_height,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, serverLabel,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    
    
    // newsgroup, text field, and button
    dataWidgets[i++] = newsgroupLabel =
        XmCreateLabelGadget(dataForm, "newsgroupLabel", NULL, 0);
	dataWidgets[i++] = m_newsgroupText =
        fe_CreateTextField(dataForm, "newsgroupText",NULL, 0);
    
    
    int newsgroup_height = XfeVaGetTallestWidget(newsgroupLabel,
                                                 m_newsgroupText,
                                                 NULL);
                                                 
    XtVaSetValues(newsgroupLabel,
                  XmNheight, newsgroup_height,
                  XmNalignment, XmALIGNMENT_END,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, serverLabel,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
                                                 
    XtVaSetValues(m_newsgroupText,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, newsgroupLabel,
                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget, m_serverCombo,
                  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNrightWidget, m_serverCombo,
                  NULL);

    int label_width = XfeVaGetWidestWidget(serverLabel,
                                           newsgroupLabel,
                                           NULL);
    XtVaSetValues(serverLabel,    XmNwidth, label_width, NULL);
    XtVaSetValues(newsgroupLabel, XmNwidth, label_width, NULL);
    XtManageChildren(dataWidgets,i);
    
    i=0;
    Widget buttons[6];
    // buttons
	Widget buttonForm = XtCreateManagedWidget("buttonForm",
										 xmRowColumnWidgetClass,
										 m_form,
										 NULL, 0);

    buttons[i++] = 
	m_subscribeButton = XtVaCreateManagedWidget(xfeCmdToggleSubscribe,
												xmPushButtonWidgetClass,
												buttonForm,
												NULL);
    buttons[i++] =
    m_unsubscribeButton = XtVaCreateManagedWidget(xfeCmdUnsubscribe,
                                                  xmPushButtonWidgetClass,
                                                  buttonForm,
                                                  NULL);
    
    buttons[i++] = XmCreateSeparatorGadget(buttonForm, "sep1", NULL, 0);
    
    buttons[i++] =
	m_getdeletionsButton = XtVaCreateManagedWidget(xfeCmdFetchGroupList,
												   xmPushButtonWidgetClass,
												   buttonForm,
												   NULL);
    buttons[i++] = 
	m_expandallButton = XtVaCreateManagedWidget(xfeCmdExpandAll,
												xmPushButtonWidgetClass,
												buttonForm,
												NULL);
    buttons[i++] =
	m_collapseallButton = XtVaCreateManagedWidget(xfeCmdCollapseAll,
												  xmPushButtonWidgetClass,
												  buttonForm,
												  NULL);
    
    buttons [i++] = XmCreateSeparatorGadget(buttonForm, "sep2", NULL, 0);

    buttons[i++] =
	m_stopButton = XtVaCreateManagedWidget(xfeCmdStopLoading,
										   xmPushButtonWidgetClass,
										   buttonForm,
										   NULL);

    XtManageChildren(buttons,i);

    // now arrange the forms
    int button_width = XfeVaGetWidestWidget(actionForm, dataForm, NULL);

    XtVaSetValues(actionForm,
                  XmNwidth, button_width,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    
	XtVaSetValues(buttonForm,
                  XmNwidth, button_width,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, dataForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
    
    XtVaSetValues(dataForm,
                  XmNwidth, button_width,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_WIDGET,
                  XmNrightWidget, actionForm,
                  NULL);
    
    
	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, dataForm,
                  XmNrightAttachment, XmATTACH_WIDGET,
                  XmNrightWidget, buttonForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);


    
	XtAddCallback(m_newsgroupText, XmNvalueChangedCallback, newsgroup_typedown_callback, this);
	XtAddCallback(m_newsgroupText, XmNactivateCallback, newsgroup_selected_callback, this);

	XtAddCallback(m_subscribeButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
    XtAddCallback(m_unsubscribeButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_expandallButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_collapseallButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_getdeletionsButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_stopButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_addserverButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);

    XtManageChild(actionForm);
    XtManageChild(buttonForm);
    XtManageChild(dataForm);
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
															   /* Tao: we might need to check if this returns a 
																* non-NULL frame
																*/
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
XFE_SubAllView::newsgroup_selected()
{
	const int *selected;
	int count;

	// Get the selection
  	m_outliner->getSelection(&selected, &count);

	// Subscribe to the newsgroup
	MSG_Command(m_pane, MSG_ToggleSubscribed, (MSG_ViewIndex*)selected,
				count); 
}

void
XFE_SubAllView::newsgroup_typedown_callback(Widget /*w*/,
											XtPointer clientData,
											XtPointer /*callData*/)
{
	XFE_SubAllView *obj = (XFE_SubAllView*)clientData;
  
	obj->newsgroup_typedown();
}

void
XFE_SubAllView::newsgroup_selected_callback(Widget /*w*/,
										  XtPointer clientData,
										  XtPointer /*callData*/)
{
	XFE_SubAllView *obj = (XFE_SubAllView*)clientData;
  
	obj->newsgroup_selected();
}


