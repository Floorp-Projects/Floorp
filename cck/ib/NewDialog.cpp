/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// NewDialog.cpp : implementation file
//

#include "stdafx.h"
#include "NewDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewDialog dialog


CNewDialog::CNewDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CNewDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewDialog)
	m_EditField = _T("");
	//}}AFX_DATA_INIT
}


void CNewDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewDialog)
	DDX_Text(pDX, IDC_EDIT1, m_EditField);
    DDV_INIFile(pDX, m_EditField);

	//}}AFX_DATA_MAP
}

void CNewDialog::DDV_INIFile(CDataExchange* pDX, CString value)
{
	if(pDX->m_bSaveAndValidate) 
	{
		value.TrimRight();

		if(value.IsEmpty())
		{
			CWnd nbox;
			nbox.MessageBox("Please enter a Configuration Name" ,"Error",MB_ICONEXCLAMATION);
			pDX->Fail();
		}

		else if(value.Right(4) != ".nci")
			value = value +".nci";

		myData = value;
	}
}


BEGIN_MESSAGE_MAP(CNewDialog, CDialog)
	//{{AFX_MSG_MAP(CNewDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewDialog message handlers

BOOL CNewDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
CRect tmpRect = CRect(7,7,173,13);
CWnd * dlg;

dlg = GetDlgItem(IDC_BASE_TEXT);
dlg->SetWindowText("Customization is in Progress");
/*dlg = GetDlgItem(IDC_TITLE_TEXT);
dlg->ShowWindow(SW_HIDE);
dlg = GetDlgItem(IDC_EDIT1);
dlg->ShowWindow(SW_HIDE);
dlg = GetDlgItem(IDOK);
dlg->ShowWindow(SW_HIDE);
dlg = GetDlgItem(IDCANCEL);
dlg->ShowWindow(SW_HIDE);
*/
SetWindowText("Progress");	

//((CProgressCtrl*)dlg)->Create(WS_TABSTOP|PBS_VERTICAL, tmpRect, this, 12345);
//dlg->ShowWindow(SW_SHOW);

	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CNewDialog::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::OnCommand(wParam, lParam);
}


void CNewDialog::OnOK() 
{
	// TODO: Add extra validation here
//	UpdateData();
//	myData = m_EditField;
	CDialog::OnOK();
}

void CNewDialog::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

CString CNewDialog::GetData() 
{
	return myData;
}
