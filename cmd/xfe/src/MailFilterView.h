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
   MailFilterView.h -- class definition for MailFilterView
   Created: Tao Cheng <tao@netscape.com>, 31-jan-97
 */



#ifndef _MAILFILTERVIEW_H_
#define _MAILFILTERVIEW_H_

#include "msg_filt.h"  
#include "msg_srch.h"  
#include "FolderDropdown.h"
#include "MNListView.h"


class XFE_MailFilterView : public XFE_MNListView
{
public:
	XFE_MailFilterView(XFE_Component *toplevel_component, 
					   Widget         parent, 
					   XFE_View      *parent_view, 
					   MWContext     *context,
					   MSG_Pane      *pane);
	virtual ~XFE_MailFilterView();

	// The Outlinable interface.
	virtual void    *ConvFromIndex(int index);
	virtual int      ConvToIndex(void *item);
	
	//
	virtual char	*getColumnName(int /* column */);
	virtual char    *getColumnHeaderText(int /* column */){ return 0;};
	virtual fe_icon *getColumnHeaderIcon(int /* column */){ return 0;}
	virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
	virtual EOutlinerTextStyle getColumnStyle(int column);
	virtual char    *getColumnText(int /* column */);
	virtual fe_icon *getColumnIcon(int /* column */);

	//
	virtual void     getTreeInfo(XP_Bool *expandable, 
								 XP_Bool *is_expanded, 
								 int *depth, 
								 OutlinerAncestorInfo **ancestor);
	//
	virtual void     Buttonfunc(const OutlineButtonFuncData *data);
	
	virtual void     Flippyfunc(const OutlineFlippyFuncData *data);
	//
	virtual void     releaseLineData();
	virtual void    *acquireLineData(int line);
	
	// Get tooltipString & docString; 
	// returned string shall be freed by the callee
	// row < 0 indicates heading row; otherwise it is a content row
	// (starting from 0)
	//
	virtual char *getCellTipString(int /* row */, int /* column */);
	virtual char *getCellDocString(int /* row */, int /* column */);
	
	// columns for the Outliner
	enum {OUTLINER_COLUMN_ORDER = 0,
		  OUTLINER_COLUMN_NAME,
		  OUTLINER_COLUMN_ON,
		  OUTLINER_COLUMN_LAST
	};
	

	virtual void apply();
	virtual void cancel();

	// list changed
	void listChanged(int which);

	// callbacks
	static void upCallback(Widget, XtPointer, XtPointer);
	static void downCallback(Widget, XtPointer, XtPointer);

	static void closeCallback(Widget, XtPointer, XtPointer);
	static void okCallback(Widget, XtPointer, XtPointer);
	static void newCallback(Widget, XtPointer, XtPointer);
	static void editCallback(Widget, XtPointer, XtPointer);
	static void delCallback(Widget, XtPointer, XtPointer);
	static void logCallback(Widget, XtPointer, XtPointer);
	static void viewLogCallback(Widget, XtPointer, XtPointer);

	// icons for the outliner
	static fe_icon m_filterOnIcon;
	static fe_icon m_filterOffIcon;

protected:

	virtual void doubleClickBody(const OutlineButtonFuncData *);

	/* all my command buttons
	 */
	virtual void upCB(Widget w, XtPointer callData);
	virtual void downCB(Widget w, XtPointer callData);

	virtual void newCB(Widget w, XtPointer callData);
	virtual void editCB(Widget w, XtPointer callData);
	virtual void delCB(Widget w, XtPointer callData);
	virtual void logCB(Widget w, XtPointer callData);
	virtual void viewLogCB(Widget w, XtPointer callData);

private:
	XFE_CALLBACK_DECL(updateCommands)

	XFE_Outliner *m_outliner;
    XFE_FolderDropdown *m_server_dropdown;
    
	int m_dataRow;

	MSG_FilterList *m_filterlist;
	MSG_Filter     *m_filter;
	MSG_Filter    **m_goneFilter;
    MSG_Pane       *m_pane;
	int             m_numGone;

	Widget  m_editBtn;
	Widget  m_deleteBtn;
 
	Widget  m_upBtn;
	Widget  m_downBtn;

	Widget  m_text;
	Widget  m_logBtn;
 
	Dimension m_despwidth;
	
	int     m_curPos;
	int     m_stripCount;
	XP_Bool m_curFilterOn;
	XP_Bool m_logOn;
};

#endif /* _MAILFILTERVIEW_H_ */
