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
   SubscribeView.cpp -- 4.x subscribe view.
   Created: Chris Toshok <toshok@netscape.com>, 16-Oct-1996.
   */



#include "MozillaApp.h"
#include "SubscribeView.h"
#include "SubTabView.h"
#include "SubAllView.h"
#include "SubSearchView.h"
#include "SubNewView.h"
#include "ViewGlue.h"

#include "xfe.h"
#include "msgcom.h"
#include "xp_mem.h"

#include <XmL/Folder.h>

XFE_SubscribeView::XFE_SubscribeView(XFE_Component *toplevel_component, 
									 Widget parent,
									 XFE_View *parent_view,
									 MWContext *context,
									 MSG_Host *host,
									 MSG_Pane *p)
	: XFE_MNView(toplevel_component, parent_view, context, p)
{
	Widget folder;
	XFE_SubAllView *suballview;
	XFE_SubSearchView *subsearchview;
	XFE_SubNewView *subnewview;
	int all_buttons_maxwidth, search_buttons_maxwidth, new_buttons_maxwidth;
	int max_button_width;

	// Clone the context
	m_cloneContext = XP_NewContext();
	if (!m_cloneContext)
		return;
	
#if defined(GLUE_COMPO_CONTEXT)
	fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
	XP_ASSERT(fec);
	CONTEXT_DATA(m_cloneContext) = fec;
	m_cloneContext->funcs = fe_BuildDisplayFunctionTable();

	ViewGlue_addMappingForCompo(this, (void *)m_cloneContext);
	CONTEXT_WIDGET(m_cloneContext) = getBaseWidget();
#else
	XFE_Frame *f = ViewGlue_getFrame(context);
	XP_ASSERT(f);
	fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
	XP_ASSERT(fec);
	CONTEXT_DATA(m_cloneContext) = fec;
	ViewGlue_addMapping(f, (void *)m_cloneContext);
	m_cloneContext->funcs = fe_BuildDisplayFunctionTable();
	CONTEXT_WIDGET(m_cloneContext) = CONTEXT_WIDGET(context);
#endif
	// Set the type of the context
	m_cloneContext->type = MWContextNews;

	if (!p) {
		setPane(MSG_CreateSubscribePaneForHost(m_cloneContext,
											   XFE_MNView::m_master,
											   host));
		XP_ASSERT(m_pane != 0);
	}

	m_interrupting = FALSE;

	getToplevel()->registerInterest(XFE_View::chromeNeedsUpdating,
									this,
									(XFE_FunctionNotification)updateButtons_cb);

	folder = XtVaCreateWidget("subscribeFolder",
							  xmlFolderWidgetClass,
							  parent,
							  NULL);

	setBaseWidget(folder);

	XtAddCallback(folder, XmNactivateCallback, tab_activate_callback, this);

	suballview = new XFE_SubAllView(toplevel_component, folder, this, m_contextData, m_pane);
	subsearchview = new XFE_SubSearchView(toplevel_component, folder, this, m_contextData, m_pane);  
	subnewview = new XFE_SubNewView(toplevel_component, folder, this, m_contextData, m_pane);

	addView(suballview);
	addView(subsearchview);
	addView(subnewview);

	all_buttons_maxwidth = suballview->getButtonsMaxWidth();
	search_buttons_maxwidth = subsearchview->getButtonsMaxWidth();
	new_buttons_maxwidth = subnewview->getButtonsMaxWidth();

	max_button_width = all_buttons_maxwidth;
	if (search_buttons_maxwidth > max_button_width)
	  max_button_width = search_buttons_maxwidth;
	if (new_buttons_maxwidth > max_button_width)
	  max_button_width = new_buttons_maxwidth;

	suballview->setButtonsWidth(max_button_width);
	subsearchview->setButtonsWidth(max_button_width);
	subnewview->setButtonsWidth(max_button_width);

	suballview->show();
	subsearchview->show();
	subnewview->show();

	// make the subtabview generate the list of of servers.
	XFE_SubTabView::syncServerList();

	static MSG_SubscribeCallbacks subscribe_callbacks = {
	  XFE_SubscribeView::do_fetch_group,
	  XFE_SubscribeView::fetch_completed
	};

	// since the all tab is the visible one when we start.
	tab_activate(0);

	MSG_SubscribeSetCallbacks(m_pane, &subscribe_callbacks, this);
}

XFE_SubscribeView::~XFE_SubscribeView()
{
    destroyPane();
#if defined(GLUE_COMPO_CONTEXT)
	// unglue; reset context 
	ViewGlue_addMappingForCompo(NULL, (void *)m_cloneContext);
	//CONTEXT_WIDGET(m_cloneContext) = getBaseWidget();
#endif 

#if notyet
/* This line is needed to fix bug #77615.  The problem is that if it's present,
   when you close the subscribe UI it hangs for anywhere from 10 seconds to
   a couple of minutes depending on the number of newsgroups the server has. */
	XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
#endif
}

Boolean
XFE_SubscribeView::isCommandEnabled(CommandType command, void *calldata, XFE_CommandInfo*)
{
	MWContext *c = MSG_GetContext(m_pane);
	XP_Bool busy = XP_IsContextBusy(c);
	XP_Bool stoppable = fe_IsContextStoppable(c);

	if (command == xfeCmdDialogOk
		|| command == xfeCmdDialogCancel)
		{
			return True;
		}
	else if (command == xfeCmdStopLoading)
		{
			return ((busy && stoppable) != 0);
		}
	else
		{
			return m_activeView->isCommandEnabled(command, calldata);
		}
}

Boolean
XFE_SubscribeView::handlesCommand(CommandType command, void *calldata, XFE_CommandInfo*)
{
	if (command == xfeCmdDialogOk
		|| command == xfeCmdDialogCancel
        || command == xfeCmdStopLoading
        )
		{
			return True;
		}
	else
		{
			return m_activeView->handlesCommand(command, calldata);
		}
}

void
XFE_SubscribeView::doCommand(CommandType command, void *calldata,
							 XFE_CommandInfo*) 
{
	if (command == xfeCmdDialogCancel) {
		MSG_SubscribeCancel(m_pane);
	} else if (command == xfeCmdStopLoading) {
		MWContext *c = MSG_GetContext(m_pane);

		m_interrupting = TRUE;
		XP_InterruptContext(c);

	} else if (command == xfeCmdDialogOk) {

		// If the context is busy, interrupt it.
		if (XP_IsContextBusy(m_cloneContext)) { 
			m_interrupting = TRUE;
			XP_InterruptContext(m_cloneContext); 
		} 

		// Commit the subscription info
		MSG_SubscribeCommit(m_pane);

	} else {
		m_activeView->doCommand(command, calldata);
	}
}

void
XFE_SubscribeView::tab_activate(int pos)
{
	XP_ASSERT(pos >=0 && pos <=2);

	m_activeView = (XFE_SubTabView*)m_subviews[pos];

	// associate the active view with the pane, so we have a way
	// to get to the right outliner.
	m_activeView->setPane(m_pane);

	m_activeView->defaultFocus();

	m_activeView->syncServerCombo();

	switch(pos)
		{
		case 0: // All tab
			MSG_SubscribeSetMode(m_pane, MSG_SubscribeAll);
			break;
		case 1: // Search tab
			MSG_SubscribeSetMode(m_pane, MSG_SubscribeSearch);
			break;
		case 2: // New tab
			MSG_SubscribeSetMode(m_pane, MSG_SubscribeNew);
			m_activeView->doCommand(xfeCmdGetNewGroups);
			break;
		default:
			XP_ASSERT(0);
			break;
		}

	m_activeView->updateButtons();
}

void
XFE_SubscribeView::tab_activate_callback(Widget /*w*/,
										 XtPointer clientData,
										 XtPointer callData)
{
	XFE_SubscribeView *obj = (XFE_SubscribeView*)clientData;
	XmLFolderCallbackStruct *cbs = (XmLFolderCallbackStruct*)callData;

	cbs->layoutNeeded = True;
	obj->tab_activate(cbs->pos);
}

XFE_CALLBACK_DEFN(XFE_SubscribeView, updateButtons)(XFE_NotificationCenter *,
													void *, void *)
{
	m_activeView->updateButtons();
}

void
XFE_SubscribeView::fetchCompleted()
{
  m_activeView->updateButtons();
}

void
XFE_SubscribeView::doFetchGroup()
{
  m_activeView->doCommand(xfeCmdFetchGroupList);
}

void
XFE_SubscribeView::fetch_completed(MSG_Pane *,
								   void *closure)
{
  XFE_SubscribeView *obj = (XFE_SubscribeView*)closure;

  obj->fetchCompleted();
}

void
XFE_SubscribeView::do_fetch_group(MSG_Pane *,
								  void *closure)
{
  XFE_SubscribeView *obj = (XFE_SubscribeView*)closure;

  obj->doFetchGroup();
}
