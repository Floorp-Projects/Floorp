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
   SubNewView.h -- 4.x subscribe view, all newsgroup tab.
   Created: Chris Toshok <toshok@netscape.com>, 18-Oct-1996.
   */



#ifndef _xfe_subnewview_h
#define _xfe_subnewview_h

#include "SubTabView.h"
#include "Command.h"

class XFE_SubNewView : public XFE_SubTabView
{
public:
	XFE_SubNewView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context, MSG_Pane *p = NULL);
	virtual ~XFE_SubNewView();

	virtual Boolean handlesCommand(CommandType command, void *calldata = NULL, XFE_CommandInfo *i = NULL);

	virtual void updateButtons();

	void serverSelected();

	virtual int getButtonsMaxWidth();
	virtual void setButtonsWidth(int width);

private:
	Widget m_buttonForm;
	Widget m_subscribeButton, m_getnewButton, m_clearnewButton, m_stopButton;
	Widget m_sep1, m_sep2;
	Widget m_infoLabel;
	Widget m_serverForm;
	Widget m_serverLabel;
};


#endif /* _xfe_subnewview_h */
