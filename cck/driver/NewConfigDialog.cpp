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


// NewConfigDialog.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "NewConfigDialog.h"

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
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNewConfigDialog, CDialog)
	//{{AFX_MSG_MAP(CNewConfigDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewConfigDialog message handlers

void CNewConfigDialog::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData();
	newConfigName = m_NewConfig_field;

	CDialog::OnOK();
}

CString CNewConfigDialog::GetConfigName() 
{
	return newConfigName;
}