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
   SubscribeView.h -- The actual subscribe view.
   Created: Chris Toshok <toshok@netscape.com>, 16-Oct-96.
 */



#ifndef _xfe_subscribeview_h
#define _xfe_subscribeview_h

#include "MNView.h"
#include "SubTabView.h"
#include "Command.h"
#include "xp.h"

class XFE_SubscribeView : public XFE_MNView
{
public:
	XFE_SubscribeView(XFE_Component *toplevel_component, Widget parent,
					  XFE_View *parent_view, MWContext *context, 
					  MSG_Host *host, MSG_Pane *p = NULL);
	virtual ~XFE_SubscribeView();

	virtual Boolean isCommandEnabled(CommandType command, void *cd = NULL,
								   XFE_CommandInfo* i = NULL);
	virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
	virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
	XP_Bool isInterrupting() { return m_interrupting; };
	void doneInterrupting() { m_interrupting = FALSE; };

private:
	XFE_CALLBACK_DECL(updateButtons)
	XP_Bool	m_interrupting;

	XFE_SubTabView *m_activeView;
	MWContext *m_cloneContext;

	// invoked when you switch tabs
	void tab_activate(int pos);
	static void tab_activate_callback(Widget, XtPointer, XtPointer);

	void doFetchGroup();
	void fetchCompleted();

	static void do_fetch_group(MSG_Pane *pane, void *closure);
	static void fetch_completed(MSG_Pane *pane, void *closure);
};

#endif /* _xfe_subscribeview_h */
