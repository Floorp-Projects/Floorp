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
   AB2PaneView.h -- class definition for AB2PaneView
   Created: Tao Cheng <tao@netscape.com>, 14-oct-97
 */

#ifndef _XFE_AB2PANEVIEW_H
#define _XFE_AB2PANEVIEW_H

#include "ABListSearchView.h"
#include "ABDirListView.h"

class XFE_AB2PaneView : public XFE_View
{
public:
	XFE_AB2PaneView(XFE_Component *toplevel_component, 
					Widget parent, 
					XFE_View *parent_view, 
					MWContext *context,
					eABViewMode mode);

	virtual ~XFE_AB2PaneView();

    // 
	void                  expandCollapse(XP_Bool expand);
	void                  expandCollapse();
	//
	void                  selectLine(int line);
	void                  selectDir(DIR_Server* dir);

	XFE_ABListSearchView* getEntriesListView(){return m_entriesListView;}
	XFE_ABDirListView*    getDirListView(){return m_dirListView;}
#if defined(USE_ABCOM)
	const AB_ContainerInfo **getRootContainers(uint32 &count) const;
#endif

	//
	virtual Boolean isCommandEnabled(CommandType command, 
									 void *calldata = NULL,
									 XFE_CommandInfo* i = NULL);

	virtual Boolean isCommandSelected(CommandType command, 
									  void *calldata = NULL,
									  XFE_CommandInfo* i = NULL);

	virtual Boolean handlesCommand(CommandType command, 
								   void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

	virtual void doCommand(CommandType command, 
						   void *calldata = NULL,
						   XFE_CommandInfo* i = NULL);

	// callbacks
	static void propertiesCallback(Widget, XtPointer, XtPointer);

	//
	XFE_CALLBACK_DECL(dirCollapse)
	XFE_CALLBACK_DECL(dirExpand)
	XFE_CALLBACK_DECL(dirSelect)
	XFE_CALLBACK_DECL(dirsChanged)
	XFE_CALLBACK_DECL(changeFocus)


protected:
	// callbacks
	void propertiesCB(Widget w, XtPointer callData);

private:
	// subviews
	XFE_ABDirListView    *m_dirListView;
	XFE_ABListSearchView *m_entriesListView;

	XP_List              *m_directories;
	int                   m_nDirs;
	DIR_Server           *m_dir;
	XP_Bool               m_expanded;
#if defined(USE_ABCOM)
	AB_ContainerInfo    **m_rootContainers;
#endif
	// focus
	XFE_MNListView       *m_focusedView;
	void setFocusView(XFE_MNListView *listView);
};
#endif /* _XFE_AB2PANEVIEW_H */
