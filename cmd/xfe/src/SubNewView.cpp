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
   SubNewView.cpp -- 4.x subscribe view, new newsgroup tab.
   Created: Chris Toshok <toshok@netscape.com>, 20-Oct-1996.
   */



#include "SubNewView.h"
#include "Outliner.h"
#include "Command.h"

#include "xfe.h"
#include "msgcom.h"
#include "xp_mem.h"

#include "xpgetstr.h"

#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>
#include "DtWidgets/ComboBox.h"
#include <Xfe/Xfe.h>

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern int XFE_NEW_NEWSGROUP_TAB;
extern int XFE_NEW_NEWSGROUP_TAB_INFO_MSG;

#define OUTLINER_GEOMETRY_PREF "mail.subscribepane.new_groups.outliner_geometry"

XFE_SubNewView::XFE_SubNewView(XFE_Component *toplevel_component,
							   Widget parent, XFE_View *parent_view,
							   MWContext *context, MSG_Pane *p)
	: XFE_SubTabView(toplevel_component, parent, parent_view, context, XFE_NEW_NEWSGROUP_TAB, p)
{
	int num_columns = 3;
	static int column_widths[] = {40, 3, 9};
	XmString info_label_str;

	m_outliner = new XFE_Outliner("subscribeNewList",
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

	m_outliner->setHideColumnsAllowed( True );
	m_outliner->setMultiSelectAllowed( True );
	m_outliner->setColumnWidth(OUTLINER_COLUMN_SUBSCRIBE, subscribedIcon.width + 2 /* for the outliner's shadow */);
	m_outliner->setColumnResizable(OUTLINER_COLUMN_SUBSCRIBE, False);
  
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

	m_getnewButton = XtVaCreateManagedWidget(xfeCmdGetNewGroups,
											 xmPushButtonWidgetClass,
											 m_buttonForm,
											 XmNleftAttachment, XmATTACH_FORM,
											 XmNrightAttachment, XmATTACH_FORM,
											 XmNtopAttachment, XmATTACH_WIDGET,
											 XmNtopWidget, m_sep1,
											 XmNbottomAttachment, XmATTACH_NONE,
											 NULL);

	m_clearnewButton = XtVaCreateManagedWidget(xfeCmdClearNewGroups,
											   xmPushButtonWidgetClass,
											   m_buttonForm,
											   XmNleftAttachment, XmATTACH_FORM,
											   XmNrightAttachment, XmATTACH_FORM,
											   XmNtopAttachment, XmATTACH_WIDGET,
											   XmNtopWidget, m_getnewButton,
											   XmNbottomAttachment, XmATTACH_NONE,
											   NULL);

	m_sep2 = XtVaCreateManagedWidget("sep2",
									 xmSeparatorWidgetClass,
									 m_buttonForm,
									 XmNleftAttachment, XmATTACH_FORM,
									 XmNrightAttachment, XmATTACH_FORM,
									 XmNtopAttachment, XmATTACH_WIDGET,
									 XmNtopWidget, m_clearnewButton,
									 XmNbottomAttachment, XmATTACH_NONE,
									 NULL);

	m_stopButton = XtVaCreateManagedWidget(xfeCmdStopLoading,
										   xmPushButtonWidgetClass,
										   m_buttonForm,
										   XmNleftAttachment, XmATTACH_FORM,
										   XmNrightAttachment, XmATTACH_FORM,
										   XmNtopAttachment, XmATTACH_WIDGET,
										   XmNtopWidget, m_sep2,
										   XmNbottomAttachment, XmATTACH_NONE,
										   NULL);

	info_label_str = XmStringCreateLtoR(XP_GetString( XFE_NEW_NEWSGROUP_TAB_INFO_MSG ),
										XmFONTLIST_DEFAULT_TAG);

	m_infoLabel = XtVaCreateManagedWidget("subNewInfoLabel",
										  xmLabelWidgetClass,
										  m_form,
										  XmNlabelString, info_label_str,
										  NULL);
  
	XmStringFree(info_label_str);

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

	XtVaSetValues(m_serverForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_infoLabel,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_serverForm,
				  NULL);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_infoLabel,
				  NULL);

	XtAddCallback(m_subscribeButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_getnewButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_clearnewButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);
	XtAddCallback(m_stopButton, XmNactivateCallback, XFE_SubTabView::button_callback, this);

	m_outliner->show();  

	// setBaseWidget is done in SubTabView.
}

XFE_SubNewView::~XFE_SubNewView()
{
}

Boolean
XFE_SubNewView::handlesCommand(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdClearNewGroups
		|| command == xfeCmdGetNewGroups)
		{
			return True;
		}
	else
		{
			return XFE_SubTabView::handlesCommand(command, calldata);
		}
}

void
XFE_SubNewView::updateButtons()
{
#define S(x) XtSetSensitive((x), isCommandEnabled(Command::intern(XtName((x)))))
	S(m_subscribeButton);
	S(m_getnewButton);
	S(m_clearnewButton);
	S(m_stopButton);
#undef S
}

void
XFE_SubNewView::serverSelected()
{
	doCommand(xfeCmdGetNewGroups);

	notifyInterested(XFE_View::chromeNeedsUpdating);
}

int
XFE_SubNewView::getButtonsMaxWidth()
{
  return XfeVaGetWidestWidget(m_subscribeButton, 
							  m_getnewButton,
							  m_clearnewButton,
							  m_stopButton,
							  NULL);
}

void
XFE_SubNewView::setButtonsWidth(int width)
{
  Arg args[1];
  
  XtSetArg(args[0], XmNwidth, width);

  XtSetValues(m_subscribeButton, args, 1);
  XtSetValues(m_getnewButton, args, 1);
  XtSetValues(m_clearnewButton, args, 1);
  XtSetValues(m_stopButton, args, 1);
}
