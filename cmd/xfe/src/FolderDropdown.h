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
   FolderDropdown.h --- a combo box for selecting folders/newsgroups
   Created: Chris Toshok <toshok@netscape.com>, 6-Mar-97
 */



#ifndef _xfe_folderdropdown_h
#define _xfe_folderdropdown_h

#include "Component.h"
#include "MNView.h"
#include "DtWidgets/ComboBox.h"

class XFE_FolderDropdown : public XFE_Component
{
public:
	XFE_FolderDropdown(XFE_Component *toplevel_component,
					   Widget parent,
					   XP_Bool allowServerSelection = TRUE,
					   XP_Bool showNewsgroups = TRUE,
					   XP_Bool boldWithNew = FALSE);

	virtual ~XFE_FolderDropdown();

	MSG_FolderInfo *getSelectedFolder();
	void selectFolder(MSG_FolderInfo *, Boolean notify = False);
	void selectFolder(char *name);
	void setPopupServer(XP_Bool p);

	void resyncDropdown();

	static const char *folderSelected;

protected:
private:
	XmString makeListItemFromLine(MSG_FolderLine* line);

	void buildList(MSG_FolderInfo *info, int *position);

	void syncFolderList();
	void syncCombo();

	static void folderSelect_cb(Widget, XtPointer, XtPointer);
	static void folderMenuPost_cb(Widget, XtPointer, XtPointer);
	
	XFE_CALLBACK_DECL(rebuildFolderDropdown)
	XFE_CALLBACK_DECL(boldFolderInfo)

	void folderMenuPost();
	void folderSelect(int row);

	XP_Bool m_foldersHaveChanged;
	XP_Bool m_allowServerSelection;
	XP_Bool m_popupServer;
	XP_Bool m_showNewsgroups;
	XP_Bool m_boldWithNew;

	int m_lastSelectedPosition;

	int m_numinfos;
	MSG_FolderInfo **m_infos;

	int m_numNewsHosts;
};

#endif /* _xfe_folderdropdown_h */
