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
   SubTabView.h -- 4.x subscribe ui tabs.
   Created: Chris Toshok <toshok@netscape.com>, 18-Oct-1996.
   */



#ifndef _xfe_subtabview_h
#define _xfe_subtabview_h

#include "MNListView.h"
#include "Outlinable.h"
#include "Command.h"

class XFE_SubTabView : public XFE_MNListView
{
public:
	XFE_SubTabView(XFE_Component *toplevel_component, Widget parent,
				   XFE_View *parent_view, MWContext *context,
				   int tab_string_id, MSG_Pane *p = NULL);
	virtual ~XFE_SubTabView();

	virtual Boolean isCommandEnabled(CommandType command, void *cd = NULL,
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
	virtual void *acquireLineData(int line);
	virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth,
							 OutlinerAncestorInfo **ancestor);
	virtual EOutlinerTextStyle getColumnStyle(int column);
	virtual char *getColumnText(int column);
	virtual fe_icon *getColumnIcon(int column);
	virtual void releaseLineData();

	virtual void Buttonfunc(const OutlineButtonFuncData *data);
	virtual void Flippyfunc(const OutlineFlippyFuncData *data);

	virtual void updateButtons();

	virtual int getButtonsMaxWidth();
	virtual void setButtonsWidth(int width);

	// these gets called whenever a tab is activated
	void syncServerCombo(); 
	virtual void defaultFocus(); /* override this to set the focus of something when the user
									clicks on this tab. */
	// this gets called when the server list needs to be updated.
	static void syncServerList();

protected:
	Widget m_form; // the form that makes up the body of the tab.
	Widget m_serverCombo; // the combobox that lists the servers.

	static const int OUTLINER_COLUMN_NAME;
	static const int OUTLINER_COLUMN_SUBSCRIBE;
	static const int OUTLINER_COLUMN_POSTINGS;

	void init_outliner_icons(Widget outliner);

	void initializeServerCombo(Widget parent);
  
	virtual void serverSelected();

	void server_select(int pos);
	static void server_select_cb(Widget, XtPointer, XtPointer);

	static void button_callback(Widget, XtPointer, XtPointer);

	// icons for the outliner.
	static fe_icon subscribedIcon;

private:
	// for the server combo box stuff
	static int m_numhosts;
	static MSG_Host **m_hosts;

	// for the outlinable stuff
	OutlinerAncestorInfo *m_ancestorInfo;
	MSG_GroupNameLine m_groupLine;
};

#endif /* _xfe_subtabview_h */
