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
// AdvOpDlg.cpp : implementation file
// Called from srchfrm.cpp and used to customize search for the search messages dialog.

#include "stdafx.h"
#include "AdvOpDlg.h"
#include "prefapi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAdvSearchOptionsDlg dialog


CAdvSearchOptionsDlg::CAdvSearchOptionsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAdvSearchOptionsDlg::IDD, pParent)
{
	m_pParent = pParent;
	if (!CDialog::Create(CAdvSearchOptionsDlg::IDD, pParent))
	{
        TRACE0("Warning: creation of CAdvSearchOptionsDlg dialog failed\n");
		return;
	}
	if( m_pParent && ::IsWindow(m_pParent->m_hWnd) ){
		m_pParent->EnableWindow(FALSE);
	}

	this->EnableWindow(TRUE);
	//{{AFX_DATA_INIT(CAdvSearchOptionsDlg)
	m_bIncludeSubfolders = FALSE;
	m_iSearchArea = 0;  
	//}}AFX_DATA_INIT
	m_bChanges= FALSE;
}


void CAdvSearchOptionsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAdvSearchOptionsDlg)
	DDX_Check(pDX, IDC_CHECK_SUBFOLDERS, m_bIncludeSubfolders);
	DDX_Radio(pDX, IDC_RADIO_SEARCH_LOCAL, m_iSearchArea);
	//}}AFX_DATA_MAP
}

BOOL CAdvSearchOptionsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	XP_Bool bSubFolders   = FALSE;
	XP_Bool bSearchServer = FALSE;
	PREF_GetBoolPref("mailnews.searchSubFolders",&bSubFolders);
	m_bIncludeSubfolders = bSubFolders;
	PREF_GetBoolPref("mailnews.searchServer",&bSearchServer);
	m_iSearchArea = !bSearchServer ? 0 : 1; 
	UpdateData(FALSE);


	return TRUE;
}

void CAdvSearchOptionsDlg::OnOK()
{
	UpdateData();
	PREF_SetBoolPref("mailnews.searchSubFolders",(XP_Bool)m_bIncludeSubfolders);
	XP_Bool bSearchServer = m_iSearchArea ? 1 : 0;
	PREF_SetBoolPref("mailnews.searchServer", bSearchServer);
	m_bChanges = TRUE;
	GetParent()->PostMessage(WM_ADVANCED_OPTIONS_DONE);
	OnClose();
}

void CAdvSearchOptionsDlg::OnHelp()
{
	//!!TODO  add NetHelp(..) call here.
}

void CAdvSearchOptionsDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	if( m_pParent && ::IsWindow(m_pParent->m_hWnd) ){
		m_pParent->EnableWindow(TRUE);
		// Return focus to parent window
		m_pParent->SetActiveWindow();
		m_pParent->SetFocus();
	}
	delete this;
}


void CAdvSearchOptionsDlg::OnClose()
{
    DestroyWindow();
}


BEGIN_MESSAGE_MAP(CAdvSearchOptionsDlg, CDialog)
	//{{AFX_MSG_MAP(CAdvSearchOptionsDlg)
	ON_BN_CLICKED( IDOK, OnOK)
	ON_BN_CLICKED( IDC_ADVANCED_SEARCH_HELP, OnHelp)
	ON_COMMAND(IDCANCEL, OnClose)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAdvSearchOptionsDlg message handlers
