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
// TestcaseDlg.cpp : implementation file
//

#include "stdafx.h"
#include "genframe.h"
#include "TestcaseDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CWhiteBoxDlg dialog

CWhiteBoxDlg::CWhiteBoxDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CWhiteBoxDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CWhiteBoxDlg)
	//}}AFX_DATA_INIT
	
}

CWhiteBoxDlg::~CWhiteBoxDlg()
{
}

void CWhiteBoxDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWhiteBoxDlg)
	DDX_Control(pDX, IDC_LIST, m_list);
	DDX_Control(pDX, IDC_DELETE, m_delete);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CWhiteBoxDlg, CDialog)
	//{{AFX_MSG_MAP(CWhiteBoxDlg)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_RUN, OnRun)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_BN_CLICKED(IDC_IMPORT, OnImport)
	ON_LBN_SELCHANGE(IDC_LIST, OnSelChangeList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWhiteBoxDlg message handlers

BOOL CWhiteBoxDlg::OnInitDialog()
{
	CString text;

	CDialog::OnInitDialog();
	ListTestCases();
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}



void CWhiteBoxDlg::OnAdd() 
{
	CTCData* pTestCase = new CTCData;
	CString str;

	
	GetDlgItem(IDC_NAME)->GetWindowText(pTestCase->m_strName);
	GetDlgItem(IDC_DESC)->GetWindowText(pTestCase->m_strDesc);
	GetDlgItem(IDC_ID)->GetWindowText(str);

	//convert!
	pTestCase->m_nID = atoi((LPCSTR)str);

	m_tcManager.AddTestCase(pTestCase);
	
	ListTestCases();

	// Reset the text boxes
	GetDlgItem(IDC_NAME)->SetWindowText("");
	GetDlgItem(IDC_DESC)->SetWindowText("");
	GetDlgItem(IDC_ID)->SetWindowText("0");
	
	//GetDlgItem(IDC_NAME)->SetFocus();
	UpdateData(FALSE);
}

void CWhiteBoxDlg::OnDelete() 
{
	
	LPINT m_TestCasesToRemove;
	int nWhich, nNumSelected;
		
	nNumSelected = m_list.GetSelCount();
	if (nNumSelected != 0)
	{
		m_TestCasesToRemove = (LPINT)calloc(sizeof(int),nNumSelected); 	
		m_list.GetSelItems(nNumSelected,m_TestCasesToRemove);

		// append -1 to the end
		//m_TestCasesToRemove[nNumSelected] = -1;
		m_tcManager.RemoveTestCases(m_TestCasesToRemove, nNumSelected);
		
		free(m_TestCasesToRemove);
	}
	ListTestCases();

/*	if ( (nWhich = m_list.GetCurSel()) != LB_ERR )
	{
		nTestCase = m_list.GetItemData(nWhich);
		pTCData = (CTCData*)m_tcManager.m_TCData[nTestCase];
		delete pTCData;
		m_tcManager.RemoveTestCase(nTestCase);
		
	}
*/
	// Set cursor
	if (nWhich+1 <= m_list.GetCount())
		m_list.SetAnchorIndex(nWhich+1);
	else
	if (nWhich != 0)
		m_list.SetAnchorIndex(nWhich);
	else
		m_delete.EnableWindow(FALSE);



}

void CWhiteBoxDlg::OnRun() 
{

	LPINT m_TestCasesToRun;
		
	int nNumSelected = m_list.GetSelCount();
	if (nNumSelected != 0)
	{
		m_TestCasesToRun = (LPINT)calloc(sizeof(int),nNumSelected + 1); 	
		m_tcManager.m_iNumTestCasesToRun = m_list.GetSelItems(nNumSelected,m_TestCasesToRun);

		// append -1 to the end
		m_TestCasesToRun[m_tcManager.m_iNumTestCasesToRun] = -1;
		CDialog::OnOK();
		m_tcManager.ExecuteTestCases(m_TestCasesToRun);
		
		free(m_TestCasesToRun);
	}
}

void CWhiteBoxDlg::OnImport()
{
	m_tcManager.LoadData();
	ListTestCases();
}

void CWhiteBoxDlg::OnSelChangeList() 
{
	// enable the delete button now that an item has been selected
	m_delete.EnableWindow(TRUE);    
}

// Saves Data And Exits
void CWhiteBoxDlg::OnSave() 
{
	// TODO: Add extra validation here

	m_tcManager.SaveData();
}

void CWhiteBoxDlg::ListTestCases()
{

	CTCData* pTCData;
	CString tmpstr, strTestCase;
	int nI;

	CListBox* pListBox = (CListBox*)GetDlgItem(IDC_LIST);

	pListBox->ResetContent();

	for (nI = 0;nI < m_tcManager.m_TCData.GetSize();nI++)
	{
		pTCData = (CTCData*)m_tcManager.m_TCData[nI];
		tmpstr.Format("%d",pTCData->m_nID);
		strTestCase = pTCData->m_strName + "\t" + pTCData->m_strDesc + "\t" + tmpstr;

		m_list.AddString(strTestCase);
		//nWhich = pListBox->AddString(strTestCase);
			//pListBox->SetItemData(nWhich,nI);

			/*
			bSelect = FALSE;
			for (nJ = 0;nJ < m_pCourse->m_Students.GetSize();nJ++)
			{
				if ( m_pCourse->m_Students[nJ] == (DWORD)nI )
				{
					bSelect = TRUE;
					break;
				}
			}
			if ( bSelect )
				pListBox->SetSel(nWhich,TRUE);
			*/
	}
	
}
