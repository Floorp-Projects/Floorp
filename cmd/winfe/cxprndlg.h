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

#ifndef __ContextPrintDialog_H
#define __ContextPrintDialog_H

// cxprndlg.h : header file
//
class CPrintCX;

/////////////////////////////////////////////////////////////////////////////
// CPrintStatusDialog dialog

class CPrintStatusDialog : public CDialog
{
// Construction
public:
	CPrintStatusDialog(CWnd* pParent, CPrintCX *pCX);
	BOOL Create(UINT id, CWnd * pParent) { return(CDialog::Create(id, pParent)); }
private:
	//	The context owning us.
	CPrintCX *m_pCX;

public:
// Dialog Data
	//{{AFX_DATA(CPrintStatusDialog)
	enum { IDD = IDD_CONTEXT_PRINT_STATUS };
	CString	m_csLocation;
	CString	m_csProgress;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrintStatusDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPrintStatusDialog)
	virtual void OnCancel();
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // __ContextPrintDialog_H
