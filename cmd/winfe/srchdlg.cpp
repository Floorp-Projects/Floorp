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
// srchdlg.cpp : implementation file
//

#include "stdafx.h"
#include "srchdlg.h"
#include "template.h"
#include "wfemsg.h"
#include "msg_srch.h"
#include "dirprefs.h"
#include "nethelp.h"
#include "xp_help.h"
#include "prefapi.h"
#include "intlwin.h"
#include "xp_time.h"
#include "xplocale.h"
#include "dateedit.h"
#include "intl_csi.h"

#define BOTTOM_BORDER 15

////////////////////////////////////////////////////////////////////////////
// CAddrEditProperities

CSearchDialog::CSearchDialog (LPCTSTR lpszCaption, MSG_Pane* pSearchPane, DIR_Server* pServer, CWnd * parent) 
:CDialog ( CSearchDialog::IDD, parent)
{
	m_pTitle = XP_STRDUP(lpszCaption);
	m_pServer = pServer;
	m_pSearchPane = pSearchPane;
	m_iMoreCount = 0;
	m_bLogicType = 1;
	m_bChanged = FALSE;
}

CSearchDialog::CSearchDialog (UINT nIDCaption, MSG_Pane* pSearchPane, DIR_Server* pServer, CWnd * parent) 
:CDialog ( CSearchDialog::IDD, parent)
{
	m_pTitle = XP_STRDUP(::szLoadString(nIDCaption));
	m_pServer = pServer;
	m_pSearchPane = pSearchPane;
	m_iMoreCount = 0;
	m_bLogicType = 1;
	m_bChanged = FALSE;

}

CSearchDialog::~CSearchDialog ( )
{

	if(m_pTitle)
	{
		XP_FREE(m_pTitle);
	}
}

void CSearchDialog::PostNcDestroy( )
{
}

void CSearchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(CSearchDialog)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSearchDialog, CDialog)
	//{{AFX_MSG_MAP(CSearchDialog)
		// NOTE: the ClassWizard will add message map macros here
	    ON_WM_CREATE()
		ON_BN_CLICKED(IDC_MORE, OnMore)
		ON_BN_CLICKED(IDC_FEWER, OnFewer)
		ON_BN_CLICKED(IDC_CLEAR_SEARCH, OnClearSearch)
		ON_BN_CLICKED(IDC_SEARCH, OnSearch)
		ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB1, OnAttrib1)
		ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB2, OnAttrib2)
		ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB3, OnAttrib3)
		ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB4, OnAttrib4)
		ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB5, OnAttrib5)
		ON_CBN_SELCHANGE(IDC_COMBO_OP1, OnOperatorValueChanged)
		ON_CBN_SELCHANGE(IDC_COMBO_OP2, OnOperatorValueChanged)
		ON_CBN_SELCHANGE(IDC_COMBO_OP3, OnOperatorValueChanged)
		ON_CBN_SELCHANGE(IDC_COMBO_OP4, OnOperatorValueChanged)
		ON_CBN_SELCHANGE(IDC_COMBO_OP5, OnOperatorValueChanged)
		ON_BN_CLICKED(IDC_RADIO_ALL, OnAndOr)
		ON_BN_CLICKED(IDC_RADIO_ANY, OnAndOr)
		ON_EN_CHANGE( IDC_EDIT_VALUE1, OnEditValueChanged )
		ON_EN_CHANGE( IDC_EDIT_VALUE2, OnEditValueChanged )
		ON_EN_CHANGE( IDC_EDIT_VALUE3, OnEditValueChanged )
		ON_EN_CHANGE( IDC_EDIT_VALUE4, OnEditValueChanged )
#ifdef ON_UPDATE_COMMAND_UI_RANGE
		ON_UPDATE_COMMAND_UI_RANGE( IDC_COMBO_ATTRIB1, IDC_EDIT_VALUE5, OnUpdateQuery )
#endif

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#ifndef ON_UPDATE_COMMAND_UI_RANGE

BOOL CAdvancedSearch::OnCmdMsg( UINT nID, int nCode, void* pExtra, 
							 AFX_CMDHANDLERINFO* pHandlerInfo ) 
{
	if ((nID >= IDC_COMBO_ATTRIB1) && (nID <= IDC_EDIT_VALUE5) && 
		( nCode == CN_UPDATE_COMMAND_UI) ) {
		OnUpdateQuery( (CCmdUI *) pExtra );
		return TRUE;
	}
	return CDialog::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );
}

#endif

void CSearchDialog::OnUpdateQuery( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( TRUE );
}

void CSearchDialog::AdjustHeight(int dy)
{
	CRect rect;
	GetWindowRect(&rect);

	CSize size = rect.Size();
	size.cy += dy;

	SetWindowPos( NULL, 0, 0, size.cx, size.cy, SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW|SWP_NOMOVE);
	GetWindowRect(&rect);
}

void CSearchDialog::OnSearch()
{
	MSG_SearchFree (m_pSearchPane);
	MSG_SearchAlloc (m_pSearchPane);
	MSG_AddLdapScope(m_pSearchPane, m_pServer);
	m_searchObj.BuildQuery(m_pSearchPane, m_iMoreCount, m_bLogicType);

	CDialog::OnOK();
}

void CSearchDialog::OnAndOr()
{
	m_searchObj.OnAndOr(m_iMoreCount, &m_bLogicType);
	m_bChanged = TRUE;
}

void CSearchDialog::OnMore()
{
	int dy = 0;

	dy = m_searchObj.More(&m_iMoreCount, m_bLogicType);

	if (m_iMoreCount < 4)
		GetDlgItem(IDC_MORE)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_MORE)->EnableWindow (FALSE);
	if (m_iMoreCount > 0)
		GetDlgItem(IDC_FEWER)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_FEWER)->EnableWindow (FALSE);

	AdjustHeight (dy);

}

void CSearchDialog::OnFewer()
{
	int dy = 0;

	dy = m_searchObj.Fewer(&m_iMoreCount, m_bLogicType);

	if (m_iMoreCount < 4)
		GetDlgItem(IDC_MORE)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_MORE)->EnableWindow (FALSE);
	if (m_iMoreCount > 0)
		GetDlgItem(IDC_FEWER)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_FEWER)->EnableWindow (FALSE);

	AdjustHeight (dy);

}

void CSearchDialog::OnClearSearch()
{
	int dy = 0;

	MSG_SearchFree (m_pSearchPane);

	dy = m_searchObj.ClearSearch(&m_iMoreCount, TRUE);

	AdjustHeight (dy);
	m_bChanged = TRUE;
}

void CSearchDialog::OnEditValueChanged ()
{
	m_bChanged = TRUE;
}

void CSearchDialog::OnOperatorValueChanged ()
{
	m_bChanged = TRUE;
}

void CSearchDialog::OnAttrib1()
{

	m_searchObj.UpdateOpList(0, scopeLdapDirectory, m_pServer);
	m_bChanged = TRUE;
}

void CSearchDialog::OnAttrib2()
{

	m_searchObj.UpdateOpList(1, scopeLdapDirectory, m_pServer);
	m_bChanged = TRUE;
}

void CSearchDialog::OnAttrib3()
{

	m_searchObj.UpdateOpList(2, scopeLdapDirectory, m_pServer);
	m_bChanged = TRUE;
}

void CSearchDialog::OnAttrib4()
{
	m_searchObj.UpdateOpList(3, scopeLdapDirectory, m_pServer);
	m_bChanged = TRUE;
}

void CSearchDialog::OnAttrib5()
{
	DIR_Server * server = NULL;

	m_searchObj.UpdateOpList(4, scopeLdapDirectory, m_pServer);
	m_bChanged = TRUE;
}

int CSearchDialog::OnCreate( LPCREATESTRUCT lpCreateStruct )
{

	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	int16 guicsid = 0;
	CString cs;

	guicsid = CIntlWin::GetSystemLocaleCsid();

	HDC hDC = ::GetDC(m_hWnd);
    LOGFONT lf;  
    memset(&lf,0,sizeof(LOGFONT));

	lf.lfPitchAndFamily = FF_SWISS;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = IntlGetLfCharset(CIntlWin::GetSystemLocaleCsid());
	if (CIntlWin::GetSystemLocaleCsid() == CS_LATIN1)
 		_tcscpy(lf.lfFaceName, "MS Sans Serif");
	else
 		_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(CIntlWin::GetSystemLocaleCsid()));
	lf.lfHeight = -MulDiv(9, ::GetDeviceCaps(hDC, LOGPIXELSY), 72);
	m_pFont = theApp.CreateAppFont( lf );

    ::ReleaseDC(m_hWnd,hDC);

	return 0;
}


BOOL CSearchDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_searchObj.InitializeAttributes (widgetText, attribCommonName);

	m_searchObj.New (this);

	m_searchObj.UpdateAttribList(scopeLdapDirectory, m_pServer);
	m_searchObj.UpdateOpList(scopeLdapDirectory, m_pServer);

	SetWindowText(m_pTitle);
	GetDlgItem(IDC_STATIC1)->SetWindowText(m_pServer->description);

	CWnd* pWidget = GetDlgItem(IDC_STATIC_DESC);
	if(pWidget)
	{
		CRect widgetRect, windowRect;
		pWidget->GetWindowRect(widgetRect);

		GetWindowRect(windowRect);
		int dy = windowRect.BottomRight().y - widgetRect.BottomRight().y;

		SetWindowPos(NULL, 0, 0, windowRect.Width(), 
					windowRect.Height() - dy + BOTTOM_BORDER, SWP_NOMOVE);
	}

	return TRUE;
}


