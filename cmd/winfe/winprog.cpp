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

/*
	Windows progress functions
*/

#include "stdafx.h"
#include "winprog.h"


BOOL CXPProgressDialog::OnInitDialog( )
{
    m_ProgressMeter.SubclassDlgItem( IDC_PROGRESSMETER, this );
    return CDialog::OnInitDialog();
}

BEGIN_MESSAGE_MAP(CXPProgressDialog, CDialog)
    ON_WM_CREATE()
END_MESSAGE_MAP()

CXPProgressDialog::CXPProgressDialog(CWnd* pParent /*=NULL*/)
    : CDialog(IDD_XPPROGRESS, pParent)
{
    m_Min =0;
    m_Max =100;
    m_Range = 100;
	m_cancelCallback = NULL;
	m_cancelClosure = NULL;
}  

void CXPProgressDialog::SetCancelCallback(PW_CancelCallback cb, void*closure)  
{
	m_cancelCallback= cb;
	m_cancelClosure =closure;
}

int CXPProgressDialog::SetRange(int32 min,int32 max) 
{
	m_Min = min;
	m_Max = max;
	m_Range = max-min;

	return TRUE;
}

BOOL CXPProgressDialog::PreTranslateMessage( MSG* pMsg )
{
    return CDialog::PreTranslateMessage(pMsg);
}

int CXPProgressDialog::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
    int res = CDialog::OnCreate(lpCreateStruct);
    return res;
}

void CXPProgressDialog::OnCancel()
{
	if (m_cancelCallback)
		m_cancelCallback(m_cancelClosure);
    DestroyWindow();
}
     
void CXPProgressDialog::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PERCENTCOMPLETE, m_PercentComplete);
}

void CXPProgressDialog::SetProgressValue(int32 value)
{
	if (m_Range > 0) {
		int iPer = min(100,abs((int)(value*100/m_Range)));

		m_ProgressMeter.StepItTo(iPer);
	}
}

CWnd *FE_GetDialogOwnerFromContext(MWContext *context)
{
	if (!context || !ABSTRACTCX(context) )
		return NULL;

	return ABSTRACTCX(context)->GetDialogOwner();
}
