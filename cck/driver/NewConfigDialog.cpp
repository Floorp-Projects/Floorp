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


// NewConfigDialog.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "NewConfigDialog.h"
#include "globals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewConfigDialog dialog


CNewConfigDialog::CNewConfigDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CNewConfigDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewConfigDialog)
	m_NewConfig_field = _T("");
	//}}AFX_DATA_INIT
}


void CNewConfigDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewConfigDialog)
	DDX_Text(pDX, IDC_EDIT1, m_NewConfig_field);
    DDV_Config(pDX, m_NewConfig_field);

	//}}AFX_DATA_MAP
}

void CNewConfigDialog::DDV_Config(CDataExchange* pDX, CString value)
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
		newConfigName = value;
	}
}

BEGIN_MESSAGE_MAP(CNewConfigDialog, CDialog)
	//{{AFX_MSG_MAP(CNewConfigDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewConfigDialog message handlers

BOOL CNewConfigDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	// TODO: Add extra initialization here

	CString DlgTitle = GetGlobal("DialogTitle");
	if (!DlgTitle.IsEmpty())
	{
		SetWindowText(DlgTitle);
		GetDlgItem(IDC_STATIC1)->SetWindowText("");
		GetDlgItem(IDC_STATIC2)->SetWindowText("");
		GetDlgItem(IDOK)->SetWindowText(DlgTitle);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CNewConfigDialog::OnOK() 
{
	// TODO: Add extra validation here
//	UpdateData();
//	newConfigName = m_NewConfig_field;

	CDialog::OnOK();
}

CString CNewConfigDialog::GetConfigName() 
{
	return newConfigName;
}