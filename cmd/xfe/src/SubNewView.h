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
