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
   CategoryView.h -- class definition for CategoryView.
   
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */

#ifndef _xfe_categoryview_h
#define _xfe_categoryview_h

#include "MNListView.h"
#include "Outlinable.h"

class XFE_CategoryView : public XFE_MNListView
{
public:
	XFE_CategoryView(XFE_Component *toplevel_component, Widget parent,
					 XFE_View *parent_view, MWContext *context,
					 MSG_Pane *p = NULL);
	
	virtual ~XFE_CategoryView();
	
	void loadFolder(MSG_FolderInfo *folderinfo);

	void selectCategory(MSG_FolderInfo *category);

	virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
									 XFE_CommandInfo* i = NULL);
	virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
	virtual void doCommand(CommandType command, void *calldata = NULL,
						   XFE_CommandInfo* i = NULL);
	
	/* Outlinable interface methods */
	virtual void *ConvFromIndex(int index);
	virtual int ConvToIndex(void *item);
	
	virtual char *getColumnName(int column);
	
	virtual char *getColumnHeaderText(int column);
	virtual fe_icon *getColumnHeaderIcon(int column);
	virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
	virtual void *aquireLineData(int line);
	virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth, OutlinerAncestorInfo **ancestor);
	virtual EOutlinerTextStyle getColumnStyle(int column);
	virtual char *getColumnText(int column);
	virtual fe_icon *getColumnIcon(int column);
	virtual void releaseLineData();

	virtual void Buttonfunc(const OutlineButtonFuncData *data);
	virtual void Flippyfunc(const OutlineFlippyFuncData *data);

	static const char *categorySelected;

private:
	static const int OUTLINER_COLUMN_NAME;
	
	MSG_FolderInfo *m_folderInfo;
	
	// for the outlinable stuff
	MSG_FolderLine m_categoryLine;
	OutlinerAncestorInfo *m_ancestorInfo;
};

#endif /* _xfe_categoryview_h */

