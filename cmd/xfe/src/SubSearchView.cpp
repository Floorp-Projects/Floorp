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
   SubSearchView.h -- 4.x subscribe view, search tab.
   Created: Chris Toshok <toshok@netscape.com>, 18-Oct-1996.
   */



#include "SubSearchView.h"
#include "Outliner.h"
#include "Command.h"

#include "xfe.h"
#include "msgcom.h"
#include "xp_mem.h"

#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>
#include "DtWidgets/ComboBox.h"
#include <Xfe/Xfe.h>
#include "felocale.h"

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern int XFE_SEARCH_FOR_NEWSGROUP_TAB;

#define OUTLINER_GEOMETRY_PREF "mail.subscribepane.search_groups.outliner_geometry"

XFE_SubSearchView::XFE_SubSearchView(XFE_Component *toplevel_component, 
									 Widget parent, XFE_View *parent_view,
									 MWContext *context, MSG_Pane *p)
	: XFE_SubTabView(toplevel_component, parent, parent_view, context, XFE_SEARCH_FOR_NEWSGROUP_TAB, p)
{
	int num_columns = 3;
	static int column_widths[] = {40, 3, 9};

	m_outliner = new XFE_Outliner("subscribeSearchList",
								  this,
								  getToplevel(),
								  m_form,
								  False, // constantSize
								  True, //hasHeadings
								  num_columns,
								  3,
								  column_widths,
								  OUTLINER_GEOMETRY_PREF);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNvisibleRows, 15,
				  NULL);

	init_outliner_icons(m_outliner->getBaseWidget());

	m_outliner->setHideColumnsAllowed( True );
	m_outliner->setMultiSelectAllowed( True );
	m_outliner->setColumnWidth(OUTLINER_COLUMN_SUBSCRIBE, subscribedIcon.width + 2 /* for the outliner's shadow */);
	m_outliner->setColumnResizable(OUTLINER_COLUMN_SUBSCRIBE, False);

    Widget dataForm = XmCreateForm(m_form, "dataForm", NULL, 0);
    
    Widget serverLabel, searchLabel;
    
    int i=0;
    Widget dataWidgets[6];

    dataWidgets[i++] = serverLabel =
        XmCreateLabelGadget(dataForm, "serverLabel", NULL, 0);
    initializeServerCombo(dataForm);
    dataWidgets[i++] = m_serverCombo;

    int server_height = XfeVaGetTallestWidget(serverLabel,
                                              m_serverCombo,
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
                  
    dataWidgets[i++] =
	searchLabel = XtCreateManagedWidget("searchLabel",
										  xmLabelWidgetClass,
										  dataForm,
										  NULL, 0);

    dataWidgets[i++] =
	m_searchText = fe_CreateTextField(dataForm, "searchText", NULL, 0);

    int search_height = XfeVaGetTallestWidget(searchLabel,
                                              m_searchText,
                                              NULL);
    XtVaSetValues(searchLabel,
                  XmNheight, search_height,
                  XmNalignment, XmALIGNMENT_END,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, serverLabel,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
                                                 
    XtVaSetValues(m_searchText,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, searchLabel,
                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget, m_serverCombo,
                  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNrightWidget, m_serverCombo,
                  NULL);

    int label_width = XfeVaGetWidestWidget(serverLabel,
                                           searchLabel,
                                           NULL);
    XtVaSetValues(serverLabel,    XmNwidth, label_width, NULL);
    XtVaSetValues(searchLabel, XmNwidth, label_width, NULL);
    XtManageChildren(dataWidgets, i);

    // upper-right - search button
    
    Widget actionForm = XmCreateRowColumn(m_form, "actionForm", NULL, 0);
    m_searchnowButton =
        XmCreatePushButtonGadget(actionForm, xfeCmdSearch, NULL, 0);
    XtManageChild(m_searchnowButton);

    // lower-right - other buttons
    i=0;
    Widget buttons[6];
	Widget buttonForm = XtCreateManagedWidget("buttonForm",
										 xmRowColumnWidgetClass,
										 m_form, NULL, 0);

    buttons[i++] =
	m_subscribeButton = XtCreateManagedWidget(xfeCmdToggleSubscribe,
											  xmPushButtonWidgetClass,
											  buttonForm,
											  NULL, 0);
    buttons[i++] =
    m_unsubscribeButton = XtCreateManagedWidget(xfeCmdUnsubscribe,
                                                xmPushButtonWidgetClass,
                                                buttonForm,
                                                NULL, 0);

    buttons[i++] = XmCreateSeparatorGadget(buttonForm, "sep1", NULL, 0);
	buttons[i++] =
	m_stopButton = XtVaCreateManagedWidget(xfeCmdStopLoading,
										   xmPushButtonWidgetClass,
										   buttonForm,
										   NULL);

    XtManageChildren(buttons, i);


    // now lay out the main forms
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

	XtAddCallback(m_subscribeButton, XmNactivateCallback, button_callback, this);
    XtAddCallback(m_unsubscribeButton, XmNactivateCallback, button_callback, this);
	XtAddCallback(m_searchnowButton, XmNactivateCallback, button_callback, this);
	XtAddCallback(m_searchText, XmNactivateCallback, search_activate_callback, this);

    XtManageChild(actionForm);
    XtManageChild(buttonForm);
    XtManageChild(dataForm);
	m_outliner->show();

	// setBaseWidget happens in SubTabView.
}

XFE_SubSearchView::~XFE_SubSearchView()
{
}

Boolean
XFE_SubSearchView::handlesCommand(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdSearch)
		{
			return True;
		}
	else
		{
			return XFE_SubTabView::handlesCommand(command, calldata);
		}
}

Boolean
XFE_SubSearchView::isCommandEnabled(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdSearch)
		{
			return !XP_IsContextBusy(MSG_GetContext(m_pane));
		}
	else
		{
			return XFE_SubTabView::isCommandEnabled(command, calldata);
		}
}

void
XFE_SubSearchView::defaultFocus()
{
	XmProcessTraversal(m_searchText, XmTRAVERSE_CURRENT);
}

void
XFE_SubSearchView::doCommand(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdSearch)
		{
			/* bstell: do we need to fix this ? */
			const char *str = fe_GetTextField(m_searchText);

			MSG_SubscribeFindAll(m_pane, str);

			XtFree((char*)str);
		}
	else
		{
			XFE_SubTabView::doCommand(command, calldata);
		}
}

void
XFE_SubSearchView::updateButtons()
{
#define S(x) XtSetSensitive((x), isCommandEnabled(Command::intern(XtName((x)))))
	S(m_searchnowButton);
	S(m_subscribeButton);
	S(m_stopButton);
#undef S
}

void
XFE_SubSearchView::serverSelected()
{
	doCommand(xfeCmdSearch);

	notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_SubSearchView::search_activate_callback(Widget /*w*/,
											XtPointer clientData,
											XtPointer /*calldata*/)
{
	XFE_SubSearchView *obj = (XFE_SubSearchView*)clientData;

	obj->doCommand(xfeCmdSearch);
}

int
XFE_SubSearchView::getButtonsMaxWidth()
{
  return XfeVaGetWidestWidget(m_subscribeButton, 
							  m_searchnowButton,
							  m_stopButton,
							  NULL);
}

void
XFE_SubSearchView::setButtonsWidth(int width)
{
  Arg args[1];
  
  XtSetArg(args[0], XmNwidth, width);

  XtSetValues(m_subscribeButton, args, 1);
  XtSetValues(m_searchnowButton, args, 1);
  XtSetValues(m_stopButton, args, 1);
}
