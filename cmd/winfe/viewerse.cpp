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

#include "stdafx.h"
#include "viewerse.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CViewerSecurity dialog


CViewerSecurity::CViewerSecurity(CWnd* pParent /*=NULL*/)
	: CDialog(CViewerSecurity::IDD, pParent)
{
	//{{AFX_DATA_INIT(CViewerSecurity)
	m_csMessage = _T("");
	m_bAskNoMore = FALSE;
	m_csDontAskText = _T("");
	//}}AFX_DATA_INIT
}


void CViewerSecurity::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CViewerSecurity)
	DDX_Text(pDX, IDC_MESSAGE, m_csMessage);
	DDX_Check(pDX, IDC_DONTASKAGAIN, m_bAskNoMore);
	DDX_Text(pDX, IDC_DONTASKTEXT, m_csDontAskText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CViewerSecurity, CDialog)
	//{{AFX_MSG_MAP(CViewerSecurity)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CViewerSecurity message handlers

void CViewerSecurity::OnCancel() 
{
	// TODO: Add extra cleanup here
	m_bCanceled = TRUE;

    //  Call the OnOK handler to save the items therein.
	CDialog::OnOK();
}

void CViewerSecurity::OnOK() 
{
	// TODO: Add extra validation here
    m_bCanceled = FALSE;
	
	CDialog::OnOK();
}

/////////////////////////////////////////////////////////////////////////////
// CLaunchHelper dialog

// XXX - this should become part of our general part utility routines...
static LPSTR
PathFindFileName(LPCSTR lpszPath)
{
    for (LPCSTR lpszFilename = lpszPath; *lpszPath; lpszPath = AnsiNext(lpszPath)) {
        if ((lpszPath[0] == '\\' || lpszPath[0] == ':') && lpszPath[1] && (lpszPath[1] != '\\'))
            lpszFilename = lpszPath + 1;
    }

    return (LPSTR)lpszFilename;
}

CLaunchHelper::CLaunchHelper(LPCSTR lpszFilename, LPCSTR lpszHelperApp, BOOL canDoOLE, CWnd* pParent)
	: CDialog(CLaunchHelper::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLaunchHelper)
	m_strMessage = _T("");
	m_bAlwaysAsk = FALSE;
	m_nAction = HELPER_OPEN_IT;
	m_bHandleByOLE = FALSE;
	//}}AFX_DATA_INIT

	m_doc = lpszFilename;
	m_canDoOLE = canDoOLE;
    // Build the message string that's based on the name of the application. Just use
    // the filename and extension part of the helper app
    m_strMessage.Format(szLoadString(IDS_OPENING_FILE), lpszFilename, PathFindFileName(lpszHelperApp));


}

BOOL
CLaunchHelper::OnInitDialog()
{
	// Set the icon
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ICON1), STM_SETICON, (WPARAM)::LoadIcon(NULL, IDI_QUESTION), 0L);

	if (!m_canDoOLE) {
		EnableOLECheck(FALSE);
		m_bHandleByOLE = FALSE;
		::ShowWindow(::GetDlgItem(m_hWnd, IDC_HANDLEBYOLE), SW_HIDE);

	}
	else {
		if (m_nAction == HELPER_OPEN_IT) {
			EnableOLECheck(TRUE);
		}
		else {
			EnableOLECheck(FALSE);
		}
	}
	return CDialog::OnInitDialog();
}

void CLaunchHelper::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CViewerSecurity)
	DDX_Text(pDX, IDC_MESSAGE, m_strMessage);
	DDX_Check(pDX, IDC_CHECK1, m_bAlwaysAsk);
	DDX_Check(pDX, IDC_HANDLEBYOLE, m_bHandleByOLE);
	DDX_Radio(pDX, IDC_RADIO1, m_nAction);
	//}}AFX_DATA_MAP
}
void CLaunchHelper::EnableOLECheck(BOOL enable)
{
	CWnd * pcWin = GetDlgItem(IDC_HANDLEBYOLE);
	if (enable) {
		pcWin->EnableWindow(TRUE);
	}
	else  // if this app does not have an ole inplace server, gray out the check box.
		pcWin->EnableWindow(FALSE);
}

void CLaunchHelper::OnOpenit()
{
	if (!m_canDoOLE) {
		EnableOLECheck(FALSE);
		m_bHandleByOLE = FALSE;
		::ShowWindow(::GetDlgItem(m_hWnd, IDC_HANDLEBYOLE), SW_HIDE);
	}
	else {
		EnableOLECheck(TRUE);
	}
}
void CLaunchHelper::OnSaveit()
{
	EnableOLECheck(FALSE);
}

BEGIN_MESSAGE_MAP(CLaunchHelper, CDialog)
	//{{AFX_MSG_MAP(CLaunchHelper)
	ON_BN_CLICKED( IDC_RADIO1, OnOpenit)
	ON_BN_CLICKED( IDC_RADIO2, OnSaveit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

