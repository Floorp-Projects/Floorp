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
// srchobj.cpp : implementation file
//


#include "stdafx.h"
#include "srchobj.h"
#include "dirprefs.h"
#include "xp_time.h"
#include "xplocale.h"
#include "template.h"
#include "dateedit.h"
#include "intl_csi.h"
#include "msg_srch.h"
#include "advopdlg.h"
#include "wfemsg.h"
#include "edhdrdlg.h"
#include "numedit.h"


static int ChoicesTable[][4] =
	{{IDC_COMBO_ATTRIB1, IDC_COMBO_OP1, IDC_EDIT_VALUE1, IDC_STATIC_THE1},
	 {IDC_COMBO_ATTRIB2, IDC_COMBO_OP2, IDC_EDIT_VALUE2, IDC_STATIC_THE2},
	 {IDC_COMBO_ATTRIB3, IDC_COMBO_OP3, IDC_EDIT_VALUE3, IDC_STATIC_THE3},
	 {IDC_COMBO_ATTRIB4, IDC_COMBO_OP4, IDC_EDIT_VALUE4, IDC_STATIC_THE4},
	 {IDC_COMBO_ATTRIB5, IDC_COMBO_OP5, IDC_EDIT_VALUE5, IDC_STATIC_THE5}};

#define COL_ATTRIB              0
#define COL_OP                  1
#define COL_VALUE               2
#define COL_COUNT               4

static _TCHAR szResultText[64];



MSG_SearchError MSG_GetValuesForAttribute( MSG_ScopeAttribute scope, 
										   MSG_SearchAttribute attrib, 
										   MSG_SearchMenuItem *items, 
										   uint16 *maxItems)
{
	const int32 aiStatiValues[] = { MSG_FLAG_READ,
									MSG_FLAG_REPLIED,
									MSG_FLAG_FORWARDED,
									MSG_FLAG_NEW,
									MSG_FLAG_MARKED };

	const MSG_PRIORITY aiPriorityValues[] = {
									   MSG_LowestPriority,
									   MSG_LowPriority,
									   MSG_NormalPriority,
									   MSG_HighPriority,
									   MSG_HighestPriority };

	uint16 nStati = sizeof(aiStatiValues) / sizeof(int32);
	uint16 nPriorities = sizeof(aiPriorityValues) / sizeof(MSG_PRIORITY);
	uint16 i;

	switch (attrib) {
	case attribPriority:
		for ( i = 0; i < nPriorities && i < *maxItems; i++ ) {
			items[i].attrib = (int16) aiPriorityValues[i];
			MSG_GetPriorityName( (MSG_PRIORITY) items[i].attrib, 
							     items[i].name, 
							     sizeof( items[i].name ) / sizeof(char) );
			items[i].isEnabled = TRUE;
		}
		*maxItems = i;
		if ( i == nPriorities ) {
			return SearchError_Success;
		} else {
			return SearchError_ListTooSmall;
		}

	case attribMsgStatus:
		for ( i = 0; i < nStati && i < *maxItems; i++ ) {

			// XXX WHS Watch them int sizes!
			// if a message status flag > 16 bits comes out,
			// we'll need to resize MSG_SearchMenuItem.attrib

			items[i].attrib = (int16) aiStatiValues[i];
			MSG_GetStatusName( (int32) items[i].attrib, 
							   items[i].name, 
							   sizeof( items[i].name ) / sizeof(char) );
			items[i].isEnabled = TRUE;
		}
		*maxItems = i;
		if ( i == nStati ) {
			return SearchError_Success;
		} else {
			return SearchError_ListTooSmall;
		}
	
	default:
		*maxItems = 0;
		return SearchError_InvalidAttribute;
	}
}


static void SlideWindow( CWnd *pWnd, int dx, int dy )
{
	CRect rect;
	CWnd *parent;

	if (pWnd)
	{
		pWnd->GetWindowRect(&rect);
		if (parent = pWnd->GetParent())
			parent->ScreenToClient(&rect);

		rect.top += dy;
		rect.left += dx;
		rect.bottom += dy;
		rect.right += dx;

		pWnd->MoveWindow(&rect, TRUE);
	}
}

static void GrowWindow( CWnd *pWnd, int dx, int dy )
{
	CRect rect;
	CWnd *parent;

	if (pWnd)
	{
		pWnd->GetWindowRect(&rect);
		if (parent = pWnd->GetParent())
			parent->ScreenToClient(&rect);

		rect.bottom += dy;
		rect.right += dx;

		pWnd->MoveWindow(&rect, TRUE);
	}
}

CSearchObject::CSearchObject()
{
	m_wnd = NULL;
}

CSearchObject::~CSearchObject()
{
}

CComboBox * CSearchObject::GetColumnOneAttributeWidget(int iRow)
{
	if (m_wnd)
		return (CComboBox *)m_wnd->GetDlgItem( ChoicesTable[iRow][COL_ATTRIB] );
	else 
		return NULL;
}

int CSearchObject::InitializeLDAPSearchWindow (MSG_Pane* searchPane, DIR_Server* curServer, int* iMoreCount, BOOL bLogicType)
{
	MSG_SearchAttribute attrib;
	MSG_SearchOperator op;
	MSG_SearchValue value;
	int numTerms = 0;
	int numItems;
	CComboBox *combo;
	int i, j, k = 0;
	DWORD dwItemData = (DWORD) -1L;
	BOOL found = FALSE;
	int dy = 0;

	MSG_CountSearchTerms (searchPane, &numTerms);

	// set the window to the right number of rows
	if (numTerms) {
		if ((*iMoreCount) <= numTerms - 1) {
			for (j = (*iMoreCount); j < numTerms - 1; j++)
				dy += More (iMoreCount, bLogicType);
		}
		else {
			for (j = (*iMoreCount); j > numTerms - 1; j--)
				dy += Fewer (iMoreCount, bLogicType);
		}
	}
	
	// reset values back to null string
	for (i = 0; i <= (*iMoreCount); i++)
	{
		(m_wnd->GetDlgItem( ChoicesTable[i][COL_VALUE]))->SetWindowText("");
	}

	for (i = 0; i < numTerms; i++)
	{
		MSG_GetNthSearchTerm (searchPane, i, &attrib, &op, &value);
		// set the attribute
		j = 0;
		found = FALSE;
		combo = (CComboBox *) m_wnd->GetDlgItem( ChoicesTable[i][COL_ATTRIB] );
		numItems = combo->GetCount();
		while (j < numItems && !found) {
			dwItemData = combo->GetItemData(j);
			if (dwItemData != (DWORD)attrib)
				j++;
			else 
				found = TRUE;
		}

		combo->SetCurSel (j);

		if (found) {
			// if the attribute is found make sure that 
			// it hasn't been remaped from customization of a different server
			int numScopes = 0;
			DIR_Server* server = NULL;
			uint16 maxItems = 16;
			MSG_SearchMenuItem items[16];
			MSG_ScopeAttribute scope = scopeLdapDirectory;
			MSG_CountSearchScopes (searchPane, &numScopes);
			BOOL found2 = FALSE;
			
			if (numScopes) {
				ASSERT (numScopes == 1);
				MSG_GetNthSearchScope (searchPane, 0, &scope, (void**) &server);
				if (server && !DIR_AreServersSame (server, curServer)) {					
					MSG_GetAttributesForSearchScopes (WFE_MSGGetMaster(), scopeLdapDirectory, 
						(void**) &server, 1, items, &maxItems);
				
					while (k < maxItems && !found2) {
						if (items [k].attrib != attrib)
							k++;
						else 
							found2 = TRUE;
					}
					if (found2) {
						char label [kSearchMenuLength]; 
						combo->GetLBText(j, label );
						if (strcmp (items [k].name, label ) != 0) {
							found = FALSE;
							combo->SetCurSel (0);
						}
					}
				}
			}
		}
		// set the operator
		if (found)
		{
			j = 0;
			found = FALSE;
			combo = (CComboBox *) m_wnd->GetDlgItem( ChoicesTable[i][COL_OP] );
			numItems = combo->GetCount();
			while (j < numItems && !found) {
				dwItemData = combo->GetItemData(j);
				if (dwItemData != (DWORD)op)
					j++;
				else 
					found = TRUE;
			}
			combo->SetCurSel (j);	
			// set the value
			(m_wnd->GetDlgItem( ChoicesTable[i][COL_VALUE]))->SetWindowText(value.u.string);
		}
	}
	return dy;
}

void CSearchObject::UpdateAttribList(MSG_ScopeAttribute scope, DIR_Server* pServer)
{
	CComboBox *combo;
	int i, j, iCurSel = -1;
	DWORD dwItemData = (DWORD) -1L;

	combo = (CComboBox *) m_wnd->GetDlgItem( IDC_COMBO_SCOPE );
	if (combo) {
		iCurSel = combo->GetCurSel();
		dwItemData = combo->GetItemData(iCurSel); 
	}

	//defines required for custom hearder vars
	MSG_FolderInfo *folderInfos[1];
	BOOL bUsesCustomHeaders;
	folderInfos[0]= (MSG_FolderInfo *) dwItemData;
	uint16 numItems = 0;
	MSG_SearchMenuItem * HeaderItems = NULL;
	CString strEditCustom;
	//ldap searching define
	uint16 maxItems = 16;
	MSG_SearchMenuItem items[16];
	
	if (scope != scopeLdapDirectory) {
		MSG_GetNumAttributesForSearchScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)folderInfos, 1, &numItems); 
		bUsesCustomHeaders = MSG_ScopeUsesCustomHeaders(WFE_MSGGetMaster(), scopeMailFolder, folderInfos[0], FALSE);
		HeaderItems = new MSG_SearchMenuItem [numItems+1];
		if (!HeaderItems ) 
			return;  //something bad happened here!!
		MSG_GetAttributesForSearchScopes (WFE_MSGGetMaster(), scope, (void **)folderInfos, 1, HeaderItems, &numItems);
		//setup custom header combo selection text
		strEditCustom.LoadString(IDS_EDIT_CUSTOM);
		strcpy( HeaderItems[numItems].name,  strEditCustom);
		HeaderItems[numItems].attrib = -1;  //identifies a command to launch the Edit headers dialog
		HeaderItems[numItems].isEnabled = FALSE;
	}
	else {
		DIR_Server *server = pServer;
		if (iCurSel != -1) {
			XP_List *ldapDirectories = XP_ListNew();
			if (ldapDirectories) {
				DIR_GetLdapServers (theApp.m_directories, ldapDirectories);
				if (XP_ListCount(ldapDirectories))
					server = (DIR_Server*) XP_ListGetObjectNum (ldapDirectories, iCurSel + 1);
				XP_ListDestroy (ldapDirectories);
			}
		}
		if (server)
			MSG_GetAttributesForSearchScopes (WFE_MSGGetMaster(), scope, (void**)&server, 1, items, &maxItems);
	}


	for (i = 0; i < 5; i++) {
		DWORD dwOldAttrib = (DWORD) -1L;

		combo = (CComboBox *) m_wnd->GetDlgItem( ChoicesTable[i][COL_ATTRIB] );
		int idx = combo->GetCurSel();
		if (combo->GetCurSel() != -1)
			dwOldAttrib = combo->GetItemData(idx);
		combo->ResetContent();

		if ( scope == scopeLdapDirectory)
		{
			for (j = 0; j < maxItems; j++) {
				combo->AddString(items[j].name);
				combo->SetItemData(j, items[j].attrib);
				if (items[j].attrib == (MSG_SearchAttribute) dwOldAttrib)
					combo->SetCurSel(j);
			}
		}
		else
		{   //do extra work to handle custom headers.  We only do this for searching messages.

			for (j = 0; j < numItems; j++) {
				combo->AddString(HeaderItems[j].name);
				combo->SetItemData(j, HeaderItems[j].attrib);
				if (HeaderItems[j].attrib == (MSG_SearchAttribute) dwOldAttrib)
					combo->SetCurSel(j);
			}
			if (j == numItems && bUsesCustomHeaders)
			{   //place the edit text in the last position of the combo box
				combo->AddString(HeaderItems[j].name);
				combo->SetItemData(j, HeaderItems[j].attrib);
			}
		}
		if (combo->GetCurSel() == -1)
			combo->SetCurSel(i % combo->GetCount());
	}
	if (HeaderItems) 
		delete HeaderItems; //free the memory to the header array since it's now in the combo box
}

void CSearchObject::UpdateOpList(MSG_ScopeAttribute scope, DIR_Server* pServer)
{
	for (int i = 0; i < 5; i++) {
		UpdateOpList(i, scope, pServer);
	}
}

void CSearchObject::UpdateOpList(int iRow, MSG_ScopeAttribute scope, DIR_Server* pServer)
{
	CComboBox *combo;
	CWnd *widget, *widgetPrior;
	CEdit *edit;
	CNSNumEdit *AgeInDays;
	CNSDateEdit *date;
	DWORD dwItemData = (DWORD) -1L;

	int j, iAttribCurSel, iScopeCurSel = -1;
	MSG_SearchAttribute attrib;

	combo = (CComboBox *) m_wnd->GetDlgItem( IDC_COMBO_SCOPE );
	if (combo) {
		iScopeCurSel = combo->GetCurSel();
		dwItemData = combo->GetItemData(iScopeCurSel);    
	}

	combo = (CComboBox *) m_wnd->GetDlgItem( ChoicesTable[iRow][COL_ATTRIB] );
	iAttribCurSel = combo->GetCurSel();
	attrib = (MSG_SearchAttribute) combo->GetItemData(iAttribCurSel);

	DWORD dwOldOp = (DWORD) -1L;
	MSG_SearchMenuItem items[16];
	uint16 maxItems = 16;

	combo = (CComboBox *) m_wnd->GetDlgItem( ChoicesTable[iRow][COL_OP] );
	int idx = combo->GetCurSel();
	if (idx != -1)
		dwOldOp = combo->GetItemData(idx);
	combo->ResetContent();

	if (scope != scopeLdapDirectory) {
		MSG_FolderInfo *folderInfos[1];
		folderInfos[0]= (MSG_FolderInfo *) dwItemData;
		MSG_GetOperatorsForSearchScopes(WFE_MSGGetMaster(), scope, (void **) folderInfos, 1, attrib, items, &maxItems);
	} else {
		DIR_Server *server = pServer;
		if (iScopeCurSel != -1) {
			XP_List *ldapDirectories = XP_ListNew();
			if (ldapDirectories) {
				DIR_GetLdapServers (theApp.m_directories, ldapDirectories);
				if (XP_ListCount(ldapDirectories))
					server = (DIR_Server*) XP_ListGetObjectNum (ldapDirectories, iScopeCurSel + 1);
				XP_ListDestroy (ldapDirectories);
			}
		}
		if (server)
			MSG_GetOperatorsForSearchScopes(WFE_MSGGetMaster(), scope, (void **) &server, 1, attrib, items, &maxItems);
	}

	for (j = 0; j < maxItems; j++) {
		combo->AddString(items[j].name);
		combo->SetItemData(j, items[j].attrib);
		if (items[j].attrib == (MSG_SearchOperator) dwOldOp)
			combo->SetCurSel(j);
	}
	if (combo->GetCurSel() == -1)
		combo->SetCurSel(0);

	// Calculate a dialog unit 
	RECT rect = {0, 0, 1, 1};
	::MapDialogRect(m_wnd->m_hWnd, &rect);
	int iDialogUnit = rect.right;

	POINT ptUs = {0,0};
	m_wnd->ClientToScreen(&ptUs);

	//
	// Substitute a happy new widget based on the search attribute
	//

	if ( attrib == AttribWas [iRow] ) {
		return;
	}

	AttribWas[iRow] = attrib;

	MSG_SearchValueWidget valueWidget;
	MSG_GetSearchWidgetForAttribute ( attrib, &valueWidget );

	if (( WidgetWas[iRow] == valueWidget ) && ( valueWidget != widgetMenu) ) {
		return;
	}
	widget = m_wnd->GetDlgItem( ChoicesTable[iRow][COL_VALUE] );
	widgetPrior = m_wnd->GetDlgItem( ChoicesTable[iRow][COL_OP] );

	RECT rectWidget;
	widget->GetWindowRect(&rectWidget);
	::OffsetRect( &rectWidget, -ptUs.x, -ptUs.y );

	DWORD dwStyle = widget->GetStyle() &
					(WS_CHILD|WS_VISIBLE|WS_DISABLED|WS_GROUP|WS_TABSTOP);

	DWORD dwExStyle = 0;
	widget->DestroyWindow();

	switch( valueWidget ) {
	case widgetMenu:
		rectWidget.bottom = rectWidget.top + 60 * iDialogUnit;
		dwStyle |= WS_VSCROLL|CBS_DROPDOWNLIST;
		combo = new CComboBox;
		maxItems = 16;
		combo->Create( dwStyle, rectWidget, m_wnd, ChoicesTable[iRow][COL_VALUE] );
		combo->SetFont(m_wnd->GetFont());
		MSG_GetValuesForAttribute( scope, attrib, items, &maxItems );
		for (j = 0; j < maxItems; j++) {
			combo->AddString( items[j].name );
			combo->SetItemData( j, items[j].attrib );
		}
		combo->SetCurSel(0);
		combo->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;

	case widgetDate:
#ifdef _WIN32
		dwExStyle |= WS_EX_CLIENTEDGE;
		if ( !sysInfo.m_bWin4 )
#endif
		{
			dwStyle |= WS_BORDER;
		}
		dwStyle |= WS_CLIPCHILDREN;

		date = new CNSDateEdit;
#ifdef _WIN32
		date->CreateEx( dwExStyle, _T("STATIC"), "", dwStyle, 
						rectWidget.left, rectWidget.top, 
						rectWidget.right - rectWidget.left, 
						rectWidget.bottom - rectWidget.top,
						m_wnd->m_hWnd, (HMENU) ChoicesTable[iRow][COL_VALUE]);
#else
		date->Create( "", dwStyle|SS_SIMPLE, rectWidget, m_wnd, ChoicesTable[iRow][COL_VALUE] );
#endif
		date->SetFont(m_wnd->GetFont() );
		date->SetDate( CTime( time( NULL ) ) );
		date->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;

	case widgetInt:
		// TODO!!! Add edit class that takes only numbers
		dwStyle |= ES_AUTOHSCROLL;
#ifdef _WIN32
		dwExStyle |= WS_EX_CLIENTEDGE;
		if ( !sysInfo.m_bWin4 )
#endif
		{
			dwStyle |= WS_BORDER;
		}
		AgeInDays = new CNSNumEdit;
#ifdef _WIN32
		AgeInDays->CreateEx( dwExStyle, _T("EDIT"), "", dwStyle, 
						rectWidget.left, rectWidget.top, 
						rectWidget.right - rectWidget.left, 
						rectWidget.bottom - rectWidget.top,
						m_wnd->m_hWnd, (HMENU) ChoicesTable[iRow][COL_VALUE]);
#else
		AgeInDays->Create( dwStyle, rectWidget, m_wnd, ChoicesTable[iRow][COL_VALUE] );
#endif
		AgeInDays->SetFont(m_wnd->GetFont());
		//AgeInDays->SetWindowText(_T(""));            
		AgeInDays->SetValue(0);
		AgeInDays->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;

	case widgetText:
	default:
		dwStyle |= ES_AUTOHSCROLL;
#ifdef _WIN32
		dwExStyle |= WS_EX_CLIENTEDGE;
		if ( !sysInfo.m_bWin4 )
#endif
		{
			dwStyle |= WS_BORDER;
		}
		edit = new CEdit;
#ifdef _WIN32
		edit->CreateEx( dwExStyle, _T("EDIT"), "", dwStyle, 
						rectWidget.left, rectWidget.top, 
						rectWidget.right - rectWidget.left, 
						rectWidget.bottom - rectWidget.top,
						m_wnd->m_hWnd, (HMENU) ChoicesTable[iRow][COL_VALUE]);
#else
		edit->Create( dwStyle, rectWidget, m_wnd, ChoicesTable[iRow][COL_VALUE] );
#endif
		edit->SetFont(m_wnd->GetFont());
		edit->SetWindowText(_T(""));            
		edit->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;
	}
	WidgetWas[iRow] = valueWidget;
}

int CSearchObject::More(int* moreCount, BOOL bLogicType)
{
	CWnd *widget;
	int dy = 0;
	CRect rect, rect2;

	if (*moreCount < 4)
	{
		(*moreCount)++;

		CString strLogicText;
		int nID = 0;
		for (int i = 0; i < COL_COUNT; i++) {
			nID = ChoicesTable[(*moreCount)][i];
			widget = m_wnd->GetDlgItem(nID);
			if (i == 3 && bLogicType)
			{
				strLogicText.LoadString(IDS_ORTHE);
				widget->SetWindowText(strLogicText);
			}
			else if ( i==3 && !bLogicType)
			{   //jump to another set up resource id's
				strLogicText.LoadString(IDS_ANDTHE);
				widget->SetWindowText(strLogicText);
			}
			widget->ShowWindow(SW_SHOW);
		}

		widget = m_wnd->GetDlgItem(ChoicesTable[(*moreCount)][0]);
		widget->GetWindowRect(&rect);
		dy = rect.bottom;
		widget = m_wnd->GetDlgItem(ChoicesTable[(*moreCount) - 1][0]);
		widget->GetWindowRect(&rect);
		dy -= rect.bottom;
		widget = m_wnd->GetDlgItem(IDC_MORE);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_FEWER);
		SlideWindow(widget, 0, dy);

		widget = m_wnd->GetDlgItem(IDC_NEW);
		SlideWindow(widget, 0, dy);

		widget = m_wnd->GetDlgItem(IDC_CLEAR_SEARCH);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_STATIC_DESC);
		SlideWindow(widget, 0, dy);
	}

	return dy;
}

void CSearchObject::OnAndOr(int iMoreCount, BOOL* bLogicType)
{
	CComboBox *combo;
	int iCurSel = 0;

	combo = (CComboBox *) m_wnd->GetDlgItem( IDC_COMBO_AND_OR );
	if (!combo) return;
	iCurSel = combo->GetCurSel();
	*bLogicType = (iCurSel == 0 ? 0 : 1);
	ChangeLogicText(iMoreCount, *bLogicType);
}

int CSearchObject::ChangeLogicText(int iMoreCount, BOOL bLogicType)
{
	if (iMoreCount <= 4)
	{
		CWnd *widget;

		CString strLogicText;
		int nID = 0;
		for (int Row = 1; Row <= iMoreCount; Row++) 
		{
			widget = m_wnd->GetDlgItem(ChoicesTable[Row][3]);
			if (!widget) return 0;
			if (bLogicType)
			{   //Display OR logic text
				strLogicText.LoadString(IDS_ORTHE);
				widget->SetWindowText(strLogicText);
			}
			else 
			{   //Display AND logic text
				strLogicText.LoadString(IDS_ANDTHE);
				widget->SetWindowText(strLogicText);
			}
			widget->ShowWindow(SW_SHOW);
		}
	}

	return 1;
}


int CSearchObject::Fewer(int* iMoreCount, BOOL bLogicTyp)
{
	CWnd *widget;
	int dy = 0;

	if (*iMoreCount > 0) {
		for (int i = 0; i < COL_COUNT; i++) {
			widget = m_wnd->GetDlgItem(ChoicesTable[*iMoreCount][i]);
			widget->ShowWindow(SW_HIDE);
		}
		(*iMoreCount)--;

		CRect rect;
		widget = m_wnd->GetDlgItem(ChoicesTable[*iMoreCount][0]);
		widget->GetWindowRect(&rect);
		dy = rect.bottom;
		widget = m_wnd->GetDlgItem(ChoicesTable[*iMoreCount + 1][0]);
		widget->GetWindowRect(&rect);
		dy -= rect.bottom;

		widget = m_wnd->GetDlgItem(IDC_MORE);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_FEWER);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_NEW);
		SlideWindow(widget, 0, dy);

		widget = m_wnd->GetDlgItem(IDC_CLEAR_SEARCH);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_STATIC_DESC);
		SlideWindow(widget, 0, dy);
	}                      

		return dy;
}


int CSearchObject::ClearSearch(int* iMoreCount, BOOL bIsLDAPSearch)
{
	CWnd *widget;
	int dy = 0, res = 0;

	while (*iMoreCount > 0) {
		for (int j = 0; j < COL_COUNT; j++) {
			widget = m_wnd->GetDlgItem(ChoicesTable[*iMoreCount][j]);
			widget->ShowWindow(SW_HIDE);
		}
		(*iMoreCount)--;

		CRect rect;
		widget = m_wnd->GetDlgItem(ChoicesTable[*iMoreCount][0]);
		widget->GetWindowRect(&rect);
		dy = rect.bottom;
		widget = m_wnd->GetDlgItem(ChoicesTable[*iMoreCount + 1][0]);
		widget->GetWindowRect(&rect);
		dy -= rect.bottom;

		widget = m_wnd->GetDlgItem(IDC_MORE);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_FEWER);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_NEW);
		SlideWindow(widget, 0, dy);


		widget = m_wnd->GetDlgItem(IDC_CLEAR_SEARCH);
		SlideWindow(widget, 0, dy);
		widget = m_wnd->GetDlgItem(IDC_STATIC_DESC);
		SlideWindow(widget, 0, dy);

		//if ((bIsLDAPSearch && (*iMoreCount) >= 0) || (*iMoreCount) > 0)
		if ((*iMoreCount) >= 0)
			res += dy;
	}

	return res;
}

void CSearchObject::OnSize( UINT nType, int cx, int cy, int dx )
{
	CWnd *widget;

	for (int i = 0; i < 5; i++) {
		widget = m_wnd->GetDlgItem(ChoicesTable[i][COL_VALUE]);
		GrowWindow(widget, dx, 0);
		if (WidgetWas[i] == widgetMenu) {
			RECT rect = {0, 0, 1, 1};
			::MapDialogRect(m_wnd->GetSafeHwnd(), &rect);
			int iDialogUnit = rect.right;
			int dy = 60 * iDialogUnit;
			GrowWindow(widget, 0, dy);
		}
	}
}

void CSearchObject::BuildQuery (MSG_Pane* searchPane, int iMoreCount, BOOL bLogicType)
{
	MSG_SearchAttribute attrib;
	MSG_SearchOperator op;
	MSG_SearchValue value;
	CNSDateEdit *date = NULL;
	CNSNumEdit *AgeInDays = NULL;
	CComboBox * combo = NULL;
	int iCurSel = 0;
	CWnd * widget = NULL;
	char szArbitraryHeader[70];

	for (int i = 0; i <= iMoreCount; i++) {
		combo = (CComboBox *) m_wnd->GetDlgItem(ChoicesTable[i][COL_ATTRIB]);
		iCurSel = combo->GetCurSel();
		attrib = (MSG_SearchAttribute) combo->GetItemData(iCurSel);

		combo = (CComboBox *) m_wnd->GetDlgItem(ChoicesTable[i][COL_OP]);
		iCurSel = combo->GetCurSel();
		op = (MSG_SearchOperator) combo->GetItemData(iCurSel);

		widget = m_wnd->GetDlgItem(ChoicesTable[i][COL_VALUE]);
		widget->GetWindowText(szResultText, sizeof(szResultText));

		value.attribute = attrib;
		XP_Bool freeValueStr = FALSE;  

		switch (attrib)
		{
		case attribAgeInDays:
			{
				AgeInDays = (CNSNumEdit *) m_wnd->GetDlgItem(ChoicesTable[i][COL_VALUE]);
				value.u.age = AgeInDays->GetValue();  
			}
			break;
		case attribDate: 
			{
				CTime ctime;
				date = (CNSDateEdit *) m_wnd->GetDlgItem(ChoicesTable[i][COL_VALUE]);
				date->GetDate(ctime);
				value.u.date = ctime.GetTime();
			}
			break;
		case attribPriority:
			combo = (CComboBox *) m_wnd->GetDlgItem(ChoicesTable[i][COL_VALUE]);
			iCurSel = combo->GetCurSel();
			value.u.priority = (MSG_PRIORITY) combo->GetItemData(iCurSel);
			break;
		case attribMsgStatus:
			combo = (CComboBox *) m_wnd->GetDlgItem(ChoicesTable[i][COL_VALUE]);
			iCurSel = combo->GetCurSel();
			value.u.msgStatus = combo->GetItemData(iCurSel);
			break;

		case attribOtherHeader:
			combo = (CComboBox *) m_wnd->GetDlgItem(ChoicesTable[i][COL_ATTRIB]);
			iCurSel = combo->GetCurSel();
			combo->GetLBText(iCurSel, szArbitraryHeader);

		default:
			value.u.string = XP_STRDUP (szResultText);
			freeValueStr = TRUE;
			break;
		}

		MSG_AddSearchTerm(searchPane, attrib, op, &value, !bLogicType, 
						 (attrib != attribOtherHeader ? NULL : szArbitraryHeader) );

		if (freeValueStr)	
			XP_FREE(value.u.string);
	}
	
}

void CSearchObject::InitializeAttributes (MSG_SearchValueWidget widgetValue, MSG_SearchAttribute attribValue)
{
	for (int i = 0; i < 5; i++ ) {
		WidgetWas[i] = widgetValue;
		AttribWas[i] = attribValue;
	} 
}

int CSearchObject::New(CWnd* window)
{
	CRect rect, rect2;
	CWnd *widget;
	int dy = 0;

	m_wnd = window;
	widget = m_wnd->GetDlgItem(IDC_FEWER);
	ASSERT(widget); 
	widget->GetWindowRect(&rect);
	widget = m_wnd->GetDlgItem(ChoicesTable[0][0]);
	ASSERT(widget);
	widget->GetWindowRect(&rect2);
	dy = rect2.bottom - rect.top;
	widget = m_wnd->GetDlgItem(ChoicesTable[4][0]);
	ASSERT(widget);
	widget->GetWindowRect(&rect2);
	dy += rect.top - rect2.bottom;
                
	// Hide all the advanced lines
	for (int j = 1; j < 5; j++) {
		for (int i = 0; i < COL_COUNT; i++) {
			widget = m_wnd->GetDlgItem(ChoicesTable[j][i]);
			ASSERT(widget);
			widget->ShowWindow(SW_HIDE);
		}
	}

	CComboBox *comboAndOr = (CComboBox*) m_wnd->GetDlgItem(IDC_COMBO_AND_OR);
	if (comboAndOr)
	{	//Default to AND logic for display in the combobox
		comboAndOr->SetCurSel(0);
	}


	// Move more and fewer and disable fewer
	widget = m_wnd->GetDlgItem(IDC_FEWER);
	SlideWindow(widget, 0, dy);
	widget->EnableWindow (FALSE);
	widget = m_wnd->GetDlgItem(IDC_MORE);
	SlideWindow(widget, 0, dy);
	widget = m_wnd->GetDlgItem(IDC_NEW);
	SlideWindow(widget, 0, dy);

	widget = m_wnd->GetDlgItem(IDC_CLEAR_SEARCH);
	SlideWindow(widget, 0, dy);
	widget = m_wnd->GetDlgItem(IDC_STATIC_DESC);
	SlideWindow(widget, 0, dy);

	return dy;
}

void CSearchObject::ReInitializeWidgets()
{

	CWnd *widget;

	for (int i = 0; i < 5; i++) {
		widget = m_wnd->GetDlgItem( ChoicesTable[i][COL_VALUE] );

		switch (WidgetWas[i]) {
		case widgetMenu:
			((CComboBox *) widget)->SetCurSel(0);
			break;

		case widgetDate:
			((CNSDateEdit *) widget)->SetDate(CTime(time(NULL)));
			break;
		
		case widgetInt:
			((CNSNumEdit *) widget)->SetValue(0);
			break;

		case widgetText:
		default:
			widget->SetWindowText("");
			break;
		}
	}

	widget = m_wnd->GetDlgItem( ChoicesTable[0][COL_VALUE] );
	widget->SetFocus();

}

void CSearchObject::UpdateColumn1Attributes()
{
	int i, j;
	CWnd *widget = NULL;
	uint16 numItems;
	CComboBox *combo = NULL;

	MSG_FolderInfo *pInbox = NULL;
	MSG_GetFoldersWithFlag (WFE_MSGGetMaster(), MSG_FOLDER_FLAG_INBOX, &pInbox, 1);
	MSG_GetNumAttributesForSearchScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, &numItems); 
    MSG_SearchMenuItem * HeaderItems = new MSG_SearchMenuItem [numItems+1];
	
	if (!HeaderItems) 
		return;  //something bad happened here!!

	//we are getting the list of Headers and updating controls in column 1 
	MSG_GetAttributesForSearchScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, HeaderItems, &numItems);

	CString strEditCustom;
	strEditCustom.LoadString(IDS_EDIT_CUSTOM);
	strcpy( HeaderItems[numItems].name,  strEditCustom);
	HeaderItems[numItems].attrib = -1;  //identifies a command to launch the Edit headers dialog
	HeaderItems[numItems].isEnabled = FALSE;

	CComboBox* comboScope;
	int iScopeCurSel;

	comboScope = (CComboBox *) m_wnd->GetDlgItem( IDC_COMBO_SCOPE );
	iScopeCurSel = comboScope->GetCurSel();
	DWORD dwItemData = comboScope->GetItemData(iScopeCurSel);
	MSG_ScopeAttribute scope = DetermineScope( dwItemData );
	BOOL bUsesCustomHeaders = MSG_ScopeUsesCustomHeaders(WFE_MSGGetMaster(), scope, pInbox, FALSE);

	for (i = 0; i < 5; i++) {

		combo = (CComboBox *) m_wnd->GetDlgItem( ChoicesTable[i][COL_ATTRIB] );
		if (!combo)
		{
			delete HeaderItems; //free the memory since it's now in the combo box
			return;
		}
		combo->ResetContent();
		for (j = 0; j < numItems; j++) {
			combo->AddString(HeaderItems[j].name);
			combo->SetItemData(j, HeaderItems[j].attrib);
		}
		if (j == numItems && bUsesCustomHeaders)
		{   //place the edit text in the last position of the combo box
			combo->AddString(HeaderItems[j].name);
			combo->SetItemData(j, HeaderItems[j].attrib);
		}

		combo->SetCurSel(0);
		UpdateOpList(i, scope);
	}

	delete HeaderItems; //free the memory since it's now in the combo box
}


MSG_ScopeAttribute CSearchObject::DetermineScope( DWORD dwItemData )
{
	MSG_Pane *pPane = NULL;
	MSG_ScopeAttribute scope = scopeMailFolder;

	
	MSG_FolderLine folderLine;
	if (MSG_GetFolderLineById(WFE_MSGGetMaster(), (MSG_FolderInfo *) dwItemData, &folderLine)) 
	{
		if (folderLine.flags & MSG_FOLDER_FLAG_MAIL) 
		{
			scope = scopeMailFolder;	// Yeah, it's redundant
		}
		else if (folderLine.flags & (MSG_FOLDER_FLAG_NEWS_HOST|MSG_FOLDER_FLAG_NEWSGROUP)) 
		{
			scope = scopeNewsgroup;
		}
	}
	
	return scope;
}

