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
   MailFolderPropDialog.h -- property dialogs for mail folders.
   Created: Chris Toshok <toshok@netscape.com>, 08-Apr-97
 */



#ifndef _xfe_mailfolderpropdialog_h
#define _xfe_mailfolderpropdialog_h

#include "PropertySheetDialog.h"
#include "PropertyTabView.h"
#include "msgcom.h"

class XFE_MailFolderPropGeneralTab : public XFE_PropertyTabView
{
public:
	XFE_MailFolderPropGeneralTab(XFE_Component *top,
								 XFE_View *view,
								 MSG_Pane *pane,
								 MSG_FolderInfo *folder);
	virtual ~XFE_MailFolderPropGeneralTab();

	void apply();

private:
	MSG_FolderInfo *m_folder;
	MSG_Pane *m_pane;
	Widget m_namevalue;
};

class XFE_MailFolderPropDialog : public XFE_PropertySheetDialog
{
public:
	XFE_MailFolderPropDialog(Widget parent, char *name, MWContext *context,
							 MSG_Pane *pane, MSG_FolderInfo *folder);
	virtual ~XFE_MailFolderPropDialog();
};

extern "C" void fe_showMailFolderProperties(Widget parent, MWContext *context,
											MSG_Pane *pane, MSG_FolderInfo *folder);

#endif /* _xfe_mailfolderpropdialog_h */
