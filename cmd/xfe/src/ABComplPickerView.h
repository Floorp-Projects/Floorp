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

/* -*- Mode: C++; tab-width: 4 -*-
   Created: Tao Cheng<tao@netscape.com>, 16-mar-1998
 */


#ifndef _ABCOMPLPICKERVIEW_H_
#define _ABCOMPLPICKERVIEW_H_

#include "MNListView.h"

#include "addrbk.h"

class XFE_ABComplPickerView : public XFE_MNListView
{
public:
	XFE_ABComplPickerView(XFE_Component *toplevel_component, 
						  Widget         parent,
						  XFE_View      *parent_view, 
						  MWContext     *context, 
						  MSG_Pane      *p);
	virtual ~XFE_ABComplPickerView();

	//
	// callback to be notified when allconnectionsComplete
	//
	virtual void allConnectionsComplete(MWContext  *context);
	XFE_CALLBACK_DECL(allConnectionsComplete)

	//
	virtual void listChangeFinished(XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, 
									MSG_ViewIndex where, int32 num);
	
	virtual void paneChanged(XP_Bool asynchronous, 
							 MSG_PANE_CHANGED_NOTIFY_CODE notify_code, 
							 int32 value);

	// We're nice to our subclasses :)
	virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
	virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
									 XFE_CommandInfo* i = NULL);
	virtual Boolean isCommandSelected(CommandType cmd, void *calldata = NULL,
									  XFE_CommandInfo* i = NULL);
	virtual void doCommand(CommandType cmd, void *calldata = NULL,
						   XFE_CommandInfo* i = NULL);
	virtual char *commandToString(CommandType cmd, void *calldata = NULL,
								  XFE_CommandInfo* i = NULL);
  
	// The Outlinable interface.
	virtual void    *ConvFromIndex(int /* index */){return NULL;};
	virtual int      ConvToIndex(void */* item */){return 0;};
	virtual fe_icon *getColumnHeaderIcon(int column){return NULL;};
	virtual EOutlinerTextStyle getColumnHeaderStyle(int){return OUTLINER_Default;};

	virtual char    *getColumnName(int column);
	virtual char    *getColumnHeaderText(int column);
	virtual void    *acquireLineData(int line);
	virtual char    *getColumnText(int column);
	virtual fe_icon *getColumnIcon(int column);
	
	virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, 
							 int *depth, OutlinerAncestorInfo **ancestor) {};
	EOutlinerTextStyle getColumnStyle(int){};
	void Flippyfunc(const OutlineFlippyFuncData *){};

	virtual void doubleClickBody(const OutlineButtonFuncData *data);
	virtual void Buttonfunc(const OutlineButtonFuncData *data);
	virtual void releaseLineData();

	// Get tooltipString & docString; 
	// returned string shall be freed by the callee
	// row < 0 indicates heading row; otherwise it is a content row
	// (starting from 0)
	//
	virtual char *getCellTipString(int /* row */, int /* column */);
	virtual char *getCellDocString(int /* row */, int /* column */)	{ 
		return NULL;}

	// columns for the Outliner
	enum {
		OUTLINER_COLUMN_TYPE = 0,
		OUTLINER_COLUMN_NAME,
		OUTLINER_COLUMN_TITLE,
		OUTLINER_COLUMN_DEPARTMENT,
		OUTLINER_COLUMN_LAST
	};

	static fe_icon m_pabIcon;
	static fe_icon m_ldapDirIcon;
	static fe_icon m_mListIcon;

	AB_NameCompletionCookie *getSelection();

	static const char *confirmed;

protected:
	int           m_numAttribs;
	ABID          m_entryID; /* entryID for current line */
	MSG_ViewIndex m_curIndex;
	XP_Bool       m_searchingDir;
	XP_Bool       m_frmParent;	
};
#endif /* _ABCOMPLPICKERVIEW_H_ */

