/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   SubAllView.h -- 4.x subscribe view, all newsgroup tab.
   Created: Chris Toshok <toshok@netscape.com>, 18-Oct-1996.
   */



#ifndef _xfe_suballview_h
#define _xfe_suballview_h

#include "SubTabView.h"
#include "Command.h"

class XFE_SubAllView : public XFE_SubTabView
{
public:
	XFE_SubAllView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context, MSG_Pane *p = NULL);
	virtual ~XFE_SubAllView();

	virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
	virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

	void updateButtons();

	void serverSelected();

	void defaultFocus();

private:
	Widget m_newsgroupText;
	Widget m_subscribeButton, m_unsubscribeButton, m_expandallButton, 
		m_collapseallButton, m_getdeletionsButton, 
		m_stopButton, m_addserverButton;

	void newsgroup_typedown();
	void newsgroup_selected();

	static void newsgroup_typedown_callback(Widget, XtPointer, XtPointer);
	static void newsgroup_selected_callback(Widget, XtPointer, XtPointer);

};


#endif /* _xfe_suballview_h */
