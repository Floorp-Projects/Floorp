/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   BookmarkFindDialog.h -- dialog for finding bookmarks (duh).
   Created: Chris Toshok <toshok@netscape.com>, 12-Mar-97.
 */



#ifndef _xfe_bookmarkfinddialog_h
#define _xfe_bookmarkfinddialog_h

#include "Dialog.h"
#include "bkmks.h"

class XFE_BookmarkFindDialog : public XFE_Dialog
{
public:
	XFE_BookmarkFindDialog(Widget parent,
						   BM_FindInfo *info);

	virtual ~XFE_BookmarkFindDialog();

private:
	Widget m_findText;
	Widget m_nameToggle, m_locationToggle, m_descriptionToggle;
	Widget m_caseToggle, m_wordToggle;

	BM_FindInfo *m_findInfo;

	void find();
	void clear();
	void cancel();

	void getValues(); // fills in m_findInfo
	void setValues(); // alters UI to match m_findInfo

	static void find_cb(Widget, XtPointer, XtPointer);
	static void clear_cb(Widget, XtPointer, XtPointer);
	static void cancel_cb(Widget, XtPointer, XtPointer);
};

extern "C" void fe_showBMFindDialog(Widget parent, BM_FindInfo *info);

#endif /* xfe_bookmarkfinddialog_h */
