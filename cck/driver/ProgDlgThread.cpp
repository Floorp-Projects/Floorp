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

// ProgDlgThread.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "WizardMachineDlg.h"
#include "ProgDlgThread.h"
#include "ProgressDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CProgDlgThread

IMPLEMENT_DYNCREATE(CProgDlgThread, CWinThread)

CProgDlgThread::CProgDlgThread()
{
}

CProgDlgThread::~CProgDlgThread()
{
}

BOOL CProgDlgThread::InitInstance()
{
	// TODO:  perform and per-thread initialization here
	CProgressDialog ProgDialog;
	ProgDialog.DoModal();

	return TRUE;
}

int CProgDlgThread::ExitInstance()
{
	// TODO:  perform any per-thread cleanup here
	::PostQuitMessage(0);
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CProgDlgThread, CWinThread)
	//{{AFX_MSG_MAP(CProgDlgThread)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProgDlgThread message handlers
