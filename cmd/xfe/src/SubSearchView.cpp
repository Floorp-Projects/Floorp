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

	m_topForm = XtCreateManagedWidget("topForm",
									  xmFormWidgetClass,
									  m_form,
									  NULL, 0);

	m_searchForm = XtCreateManagedWidget("searchForm",
										 xmFormWidgetClass,
										 m_topForm,
										 NULL, 0);
  
	m_searchLabel = XtCreateManagedWidget("searchLabel",
										  xmLabelWidgetClass,
										  m_searchForm,
										  NULL, 0);

	m_searchText = fe_CreateTextField(m_searchForm, "searchText", NULL, 0);
	XtManageChild(m_searchText);

	m_onserverForm = XtCreateManagedWidget("onserverForm",
										   xmFormWidgetClass,
										   m_topForm,
										   NULL, 0);

	m_onserverLabel = XtCreateManagedWidget("onserverLabel",
											xmLabelWidgetClass,
											m_onserverForm,
											NULL, 0);

	initializeServerCombo(m_onserverForm);

	m_buttonForm = XtCreateManagedWidget("buttonForm",
										 xmFormWidgetClass,
										 m_form, NULL, 0);

	m_searchnowButton = XtCreateManagedWidget(xfeCmdSearch,
											  xmPushButtonWidgetClass,
											  m_buttonForm,
											  NULL, 0);

	m_subscribeButton = XtCreateManagedWidget(xfeCmdToggleSubscribe,
											  xmPushButtonWidgetClass,
											  m_buttonForm,
											  NULL, 0);

	m_sep1 = XtVaCreateManagedWidget("sep1",
									 xmSeparatorWidgetClass,
									 m_buttonForm,
									 XmNleftAttachment, XmATTACH_FORM,
									 XmNrightAttachment, XmATTACH_FORM,
									 XmNtopAttachment, XmATTACH_WIDGET,
									 XmNtopWidget, m_subscribeButton,
									 XmNbottomAttachment, XmATTACH_NONE,
									 NULL);
	
	m_stopButton = XtVaCreateManagedWidget(xfeCmdStopLoading,
										   xmPushButtonWidgetClass,
										   m_buttonForm,
										   XmNleftAttachment, XmATTACH_FORM,
										   XmNrightAttachment, XmATTACH_FORM,
										   XmNtopAttachment, XmATTACH_WIDGET,
										   XmNtopWidget, m_sep1,
										   XmNbottomAttachment, XmATTACH_NONE,
										   NULL);

	/* search text stuff. */
	XtVaSetValues(m_searchLabel,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_searchText,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_searchLabel,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);

	/* server selection stuff. */
	XtVaSetValues(m_onserverLabel,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_serverCombo,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_onserverLabel,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);
		
	/* button attachments. */
	XtVaSetValues(m_searchnowButton,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(m_subscribeButton,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_searchnowButton,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	/* attachments for the forms and outliner */
	XtVaSetValues(m_searchForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(m_onserverForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_searchForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_topForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_topForm,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, m_buttonForm,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(m_buttonForm,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);



	XtAddCallback(m_subscribeButton, XmNactivateCallback, button_callback, this);
	XtAddCallback(m_searchnowButton, XmNactivateCallback, button_callback, this);
	XtAddCallback(m_searchText, XmNactivateCallback, search_activate_callback, this);

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
