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

// srchobj.h : header file
//
#ifndef srchobj_H
#define srchobj_H
#include "msg_srch.h"
#ifndef _WIN32
#include "ctl3d.h"
#endif

#define COL_ATTRIB              0
#define COL_OP                  1
#define COL_VALUE               2
#define COL_COUNT               4

/////////////////////////////////////////////////////////////////////////////
// CSearchObject

class CSearchObject
{
	int m_iOrigFrameHeight;//Height of dialog's parent CFrameWnd
public:
    CSearchObject ( );
    ~CSearchObject ( );

	// Attributes
	MSG_SearchAttribute AttribWas[5];
	MSG_SearchValueWidget WidgetWas[5];
	CWnd* m_wnd;
public:
	void UpdateAttribList(MSG_ScopeAttribute scope, DIR_Server* pServer = NULL);
	void UpdateOpList(MSG_ScopeAttribute scope, DIR_Server* pServer = NULL);
	void UpdateOpList(int iRow, MSG_ScopeAttribute scope, DIR_Server* pServer = NULL);
	int More(int* iMoreCount, BOOL bLogicType);
	int Fewer(int* iMoreCount, BOOL bLogicType);
	void OnAndOr (int iMoreCount, BOOL* logicType);
	int ChangeLogicText(int moreCount, BOOL bLogicType);
	void SetOrigFrameHeight(int iHeight) {m_iOrigFrameHeight = iHeight;};
	int GetOrigFrameHeight() const {return m_iOrigFrameHeight;};
	int ClearSearch(int* iMoreCount, BOOL bIsLDAPSearch);
	void BuildQuery (MSG_Pane* searchPane, int iMoreCount, Bool bLogicType);
	int New (CWnd* window);
	void ReInitializeWidgets ();
	void OnSize( UINT nType, int cx, int cy, int dx);
	void InitializeAttributes (MSG_SearchValueWidget widgetValue, MSG_SearchAttribute attribValue);
	void UpdateColumn1Attributes();
	CComboBox * GetColumnOneAttributeWidget(int iRow);
	int InitializeLDAPSearchWindow (MSG_Pane* searchPane, DIR_Server* curServer, int* iMoreCount, BOOL bLogicType);

	MSG_ScopeAttribute DetermineScope( DWORD dwItemData );

protected:

};
#endif
