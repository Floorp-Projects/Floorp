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

	virtual int getButtonsMaxWidth();
	virtual void setButtonsWidth(int width);

private:
	Widget m_newsgroupForm;
	Widget m_newsgroupLabel, m_newsgroupText;
	Widget m_subscribeButton, m_expandallButton, 
		m_collapseallButton, m_getdeletionsButton, 
		m_stopButton, m_addserverButton;
	Widget m_serverForm;
	Widget m_serverLabel;
	Widget m_sep1, m_sep2;
	Widget m_buttonForm;

	void newsgroup_typedown();
	static void newsgroup_typedown_callback(Widget, XtPointer, XtPointer);
};


#endif /* _xfe_suballview_h */
