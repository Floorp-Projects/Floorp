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
   NewsgroupPropDialog.h -- property dialogs for news groups.
   Created: Chris Toshok <toshok@netscape.com>, 08-Apr-97
 */



#ifndef _xfe_newsgrouppropdialog_h
#define _xfe_newsgrouppropdialog_h

#include "PropertySheetDialog.h"
#include "PropertyTabView.h"
#include "msgcom.h"

class XFE_NewsgroupPropGeneralTab : public XFE_PropertyTabView
{
public:
	XFE_NewsgroupPropGeneralTab(XFE_Component *top,
								XFE_View *view,
								MSG_FolderInfo *group);
	virtual ~XFE_NewsgroupPropGeneralTab();

private:
	MSG_FolderInfo *m_group;
	Widget m_htmltoggle;

	void html_toggled();
	static void html_toggled_cb(Widget, XtPointer, XtPointer);
};

class XFE_NewsgroupPropDialog : public XFE_PropertySheetDialog
{
public:
	XFE_NewsgroupPropDialog(Widget parent, char *name, MWContext *context, MSG_FolderInfo *group);
	virtual ~XFE_NewsgroupPropDialog();
};

extern "C" void fe_showNewsgroupProperties(Widget parent, MWContext *context, MSG_FolderInfo *group);

#endif /* _xfe_newsgrouppropdialog_h */
