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

#include "cxprndlg.h"
#include "cxprint.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPrintStatusDialog dialog


CPrintStatusDialog::CPrintStatusDialog(CWnd* pParent, CPrintCX *pCX)
	: CDialog(CPrintStatusDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPrintStatusDialog)
	m_csLocation = _T("");
	m_csProgress = _T("");
	//}}AFX_DATA_INIT

	//	Save the context which owns us.
	m_pCX = pCX;
}


void CPrintStatusDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrintStatusDialog)
	DDX_Text(pDX, IDC_LOCATION, m_csLocation);
	DDX_Text(pDX, IDC_PROGRESS, m_csProgress);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrintStatusDialog, CDialog)
	//{{AFX_MSG_MAP(CPrintStatusDialog)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_QUERYDRAGICON()
	ON_WM_SYSCOMMAND()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CPrintStatusDialog message handlers

void CPrintStatusDialog::OnCancel() 
{
	//	Let the owning context know that the cancel button has been
	//		hit.
	TRACE("Received request to cancel print job.\n");
	m_pCX->CancelPrintJob();
	
	//	Don't call their on canel, or it blows away the dialog.
	//CDialog::OnCancel();
}

BOOL CPrintStatusDialog::OnEraseBkgnd(CDC *pDC)	{
	if(IsIconic() == TRUE)	{
		return(TRUE);
	}

	return(CDialog::OnEraseBkgnd(pDC));
}

void CPrintStatusDialog::OnSysCommand(UINT nID, LPARAM lParam)	{
	//	Don't maximize ourselves
	if(nID == SC_MAXIMIZE)	{
		return;
	}

	CDialog::OnSysCommand(nID, lParam);
}

void CPrintStatusDialog::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	//	Check to see if we need to draw our icon.
	if(IsIconic() != FALSE)	{
		HICON hIcon = theApp.LoadIcon(IDR_MAINFRAME);
		ASSERT(hIcon);
		dc.DrawIcon(2, 2, hIcon);
	}
	
	// Do not call CDialog::OnPaint() for painting messages
}

void CPrintStatusDialog::OnSize(UINT nType, int cx, int cy) 
{
	//	Change any maximize request to a normal request.
	if(nType == SIZE_MAXIMIZED)	{
		nType = SIZE_RESTORED;
	}

	CDialog::OnSize(nType, cx, cy);
}

HCURSOR CPrintStatusDialog::OnQueryDragIcon()	{
	//	Return the icon that will show up when dragged.
	HICON hIcon = theApp.LoadIcon(IDR_MAINFRAME);
	ASSERT(hIcon);
	return((HCURSOR)hIcon);
}
