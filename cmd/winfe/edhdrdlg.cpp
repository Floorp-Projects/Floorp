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

// edhdrdlg.cpp : implementation file
//

#include "stdafx.h"
#include "netscape.h"
#include "edhdrdlg.h"
#include "wfemsg.h"
#include "prefapi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//Edit Header Dialog called from CCustomHeadersDlg

CEditHeadersDlg::CEditHeadersDlg(CString strHeader, CWnd* pParent /*=NULL*/ )
	: CDialog(CEditHeadersDlg::IDD, pParent)
{
	m_pParent = pParent;

	//{{AFX_DATA_INIT(CEditHeadersDlg)
	m_strHeader = strHeader;
	//}}AFX_DATA_INIT
}


void CEditHeadersDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditHeadersDlg)
	DDX_Text(pDX, IDC_EDIT_HEADERS, m_strHeader);
	DDV_MaxChars(pDX, m_strHeader, 65);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditHeadersDlg, CDialog)
	//{{AFX_MSG_MAP(CEditHeadersDlg)
	ON_EN_CHANGE(IDC_EDIT_HEADERS, OnChangeEditHeader)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CEditHeadersDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_MNResourceSwitcher.Reset();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}



void CEditHeadersDlg::OnChangeEditHeader() 
{

	CWnd *pWnd = (CWnd*) GetDlgItem(IDOK);

	CEdit *pEdit = (CEdit*) GetDlgItem(IDC_EDIT_HEADERS);
	CString strText;
	pEdit->GetWindowText(strText);

	if (strText.IsEmpty() )
	{
		UpdateData();
		pWnd->EnableWindow(FALSE);
		return;
	}

	char buff;
	for (int i=0; i < strText.GetLength(); i++)
	{
		buff = strText.GetAt(i);
		if ( (isspace((int)buff)) || (buff == ':') || ((int)buff < 32) || ((int)buff > 127) )
		{
			CString strMessage;
			CString strCaption;
			strMessage.LoadString(IDS_HEADER_TEXT_WARN);
			strCaption.LoadString(IDS_CUSTOM_HEADER_ERROR);
			::MessageBox(this->GetSafeHwnd(), strMessage, strCaption, MB_OK);
			UpdateData(FALSE);
			pEdit->SetFocus();
			return;
		}
	}

	UpdateData();

	if (!m_strHeader.IsEmpty() && pWnd)
	{
		pWnd->EnableWindow();
	}
	else if(m_strHeader.IsEmpty() && pWnd)
	{
		pWnd->EnableWindow(FALSE);
	}

}

/////////////////////////////////////////////////////////////////////////////
// CCustomHeadersDlg dialog


CCustomHeadersDlg::CCustomHeadersDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CCustomHeadersDlg::IDD, pParent)
{
	m_pParent  = pParent;
	if (!CDialog::Create(CCustomHeadersDlg::IDD, pParent))
	{
        TRACE0("Warning: creation of CCustomHeadersDlg dialog failed\n");
		if (m_pParent)
			PostMessage(WM_EDIT_CUSTOM_DONE,0,IDCANCEL);

		return;
	}
	if( m_pParent && ::IsWindow(m_pParent->m_hWnd) ){
		m_pParent->EnableWindow(FALSE);
	}

	m_nReturnCode = IDCANCEL;  //initial value

	this->EnableWindow(TRUE);
	//{{AFX_DATA_INIT(CCustomHeadersDlg)
	m_strHeader = _T("");
	//}}AFX_DATA_INIT
}


void CCustomHeadersDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCustomHeadersDlg)
	DDX_Control(pDX, IDC_HEADER_LIST, m_lbHeaderList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCustomHeadersDlg, CDialog)
	//{{AFX_MSG_MAP(CCustomHeadersDlg)
	ON_BN_CLICKED(IDC_ADD_HEADER, OnAddHeader)
	ON_BN_CLICKED(IDC_EDIT_HEADER_NAME, OnEditHeader)
	ON_LBN_SETFOCUS(IDC_HEADER_LIST, OnSetfocusHeaderList)
	ON_BN_CLICKED(IDC_REMOVE_HEADER, OnRemoveHeader)
	ON_COMMAND(IDCANCEL, OnClose)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCustomHeadersDlg message handlers

BOOL CCustomHeadersDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();


	MSG_FolderInfo *pInbox = NULL;
	MSG_GetFoldersWithFlag (WFE_MSGGetMaster(), MSG_FOLDER_FLAG_INBOX, &pInbox, 1);
	uint16 numItems;
	MSG_GetNumAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, &numItems); 
	MSG_SearchMenuItem * pHeaderItems = new MSG_SearchMenuItem [numItems];
	
	if (!pHeaderItems) 
		return FALSE;  //something bad happened here!!

	MSG_GetAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, pHeaderItems, &numItems);

	for (int i=0; i < numItems; i++)
	{
		if ( (pHeaderItems[i].attrib == attribOtherHeader) && pHeaderItems[i].isEnabled )
		{
			m_lbHeaderList.AddString(pHeaderItems[i].name);
		}
	}

	delete pHeaderItems;


	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CCustomHeadersDlg::OnAddHeader() 
{
	
	CEditHeadersDlg EditDlg( " ",this);
	if (IDOK == EditDlg.DoModal())
	{
		if ( !EditDlg.m_strHeader.IsEmpty())
		{
			m_lbHeaderList.AddString(EditDlg.m_strHeader);
		}
		else
		{
			return;
		}
	}
	else
	{
		return;
	}

	CWnd *pWnd = (CWnd*) GetDlgItem(IDC_ADD_HEADER);

	if (pWnd)
	{
		pWnd->SetFocus();
	}
	UpdateData();	
}

void CCustomHeadersDlg::OnEditHeader() 
{
	int iSel = m_lbHeaderList.GetCurSel();
	CString strHeader;

	if (iSel >= 0)
	{
		m_lbHeaderList.GetText(iSel,strHeader);
	}
	else
	{
		return;
	}

	CEditHeadersDlg EditDlg( strHeader,this);
	if (IDOK == EditDlg.DoModal())
	{
		if ( !EditDlg.m_strHeader.IsEmpty() && !(EditDlg.m_strHeader.Compare(strHeader) == 0) )
		{
			m_lbHeaderList.DeleteString(iSel);
			m_lbHeaderList.AddString(EditDlg.m_strHeader);
		}
		else
		{
			return;
		}
	}
	else
	{
		return;
	}

	CWnd *pWnd = (CWnd*) GetDlgItem(IDC_ADD_HEADER);

	if (pWnd)
	{
		pWnd->SetFocus();
	}
	UpdateData();	
}




void CCustomHeadersDlg::OnSetfocusHeaderList() 
{
	CWnd *pWndRemove = (CWnd*) GetDlgItem(IDC_REMOVE_HEADER);
	CWnd *pWndEdit   = (CWnd*) GetDlgItem(IDC_EDIT_HEADER_NAME);
	if (pWndRemove && pWndEdit)
	{
		if (m_lbHeaderList.GetCount() != 0)
		{
			pWndRemove->EnableWindow();
			pWndEdit->EnableWindow();
		}

	}	
}

void CCustomHeadersDlg::OnRemoveHeader() 
{
	int iSel = m_lbHeaderList.GetCurSel();
	m_lbHeaderList.DeleteString(iSel);
	if (m_lbHeaderList.GetCount() == 0)
	{
		CWnd *pWnd = (CWnd*) GetDlgItem(IDC_REMOVE_HEADER);
		CWnd *pWnd2 = (CWnd*) GetDlgItem(IDC_EDIT_HEADER_NAME);
		
		if (pWnd)
		{
			pWnd->EnableWindow(FALSE);
			pWnd2->EnableWindow(FALSE);
		}
		pWnd = (CWnd*) GetDlgItem(IDC_ADD_HEADER);
		if (pWnd)
			pWnd->SetFocus();
	}
	else
		m_lbHeaderList.SetFocus();
}

void CCustomHeadersDlg::OnOK() 
{
	// TODO: Add extra validation here
	if (m_lbHeaderList.GetCount() != 0)
	{
		int nListCount = m_lbHeaderList.GetCount();
		CString strTemp;
		m_lbHeaderList.GetText(0,strTemp);
		CString strHeaderPrefList = strTemp; 

		for (int i = 1; i < nListCount; i++)
		{
			m_lbHeaderList.GetText(i,strTemp);
			strHeaderPrefList += ": " + strTemp; 
		}
		PREF_SetCharPref("mailnews.customHeaders", strHeaderPrefList);
	}
	else
	{
		PREF_SetCharPref("mailnews.customHeaders","");
	}
	m_nReturnCode = IDOK;
	OnClose();
}

void CCustomHeadersDlg::PostNcDestroy()
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


void CCustomHeadersDlg::OnClose()
{
	GetParent()->PostMessage(WM_EDIT_CUSTOM_DONE,0,m_nReturnCode);
    DestroyWindow();
}
