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
   NewsServerPropDialog.h -- property dialogs for news servers.
   Created: Chris Toshok <toshok@netscape.com>, 08-Apr-97
 */



#ifndef _xfe_newsserverpropdialog_h
#define _xfe_newsserverpropdialog_h

#include "PropertySheetDialog.h"
#include "PropertyTabView.h"
#include "msgcom.h"

class XFE_NewsServerPropGeneralTab : public XFE_PropertyTabView
{
public:
	XFE_NewsServerPropGeneralTab(XFE_Component *top,
								 XFE_View *view,
								 MSG_NewsHost *host);
	virtual ~XFE_NewsServerPropGeneralTab();

private:
	MSG_NewsHost *m_newshost;
    Widget m_prompt_toggle;
    Widget m_anonymous_toggle;
    void setPushAuth(Widget);
    static void promptForPasswd_cb(Widget, XtPointer, XtPointer);
};

class XFE_NewsServerPropDialog : public XFE_PropertySheetDialog
{
public:
	XFE_NewsServerPropDialog(Widget parent, char *name, MWContext *context, MSG_NewsHost *host);
	virtual ~XFE_NewsServerPropDialog();
};

extern "C" void fe_showNewsServerProperties(Widget parent, MWContext *context, MSG_NewsHost *host);

#endif /* _xfe_newsserverpropdialog_h */
