/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* 
   XmLTabView.cpp -- class definition for XmLTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "TabView.h"
#include <XmL/Folder.h>
#include "xpgetstr.h"


XFE_XmLTabView::XFE_XmLTabView(XFE_Component *top,
							   XFE_View *view, /* the parent view */
							   int       tab_string_id): XFE_View(top, view)
{
	Widget 		folder;
	WidgetList 	tabs;
	int 		ntabs;

	m_labelWidth = 0;

	// Create tab
	XmString xmstr;
	xmstr = XmStringCreate(XP_GetString(tab_string_id ), 
						   XmFONTLIST_DEFAULT_TAG);

	// Get the folder
	folder = view->getBaseWidget();

	// Add the tab 
	Widget form = XmLFolderAddTabForm(folder, xmstr);

	XmStringFree(xmstr);

	// Get the tab list
	XtVaGetValues(folder, XmNtabCount, &ntabs, NULL);
	XtVaGetValues(folder, XmNtabWidgetList, &tabs, NULL);

	// Save the tab
	m_tab = tabs[ntabs - 1];

	setBaseWidget(form);
}

XFE_XmLTabView::~XFE_XmLTabView()
{
}

void
XFE_XmLTabView::hide()
{
	XtUnmanageChild(getBaseWidget());
	XtUnmanageChild(m_tab);
}

void
XFE_XmLTabView::show()
{
	XtManageChild(m_tab);
	XtManageChild(getBaseWidget());
}
