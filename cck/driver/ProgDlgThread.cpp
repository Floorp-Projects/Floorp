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

// ProgDlgThread.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "ProgDlgThread.h"

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
