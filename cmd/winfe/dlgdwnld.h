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

// dlgdwnld.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDownLoadDlg dialog

class CDownLoadDlg : public CDialog
{

public:
	LOGFONT  m_LogFont;
	CFont    m_Font;
	CFont    m_Font2;
	BOOL	 m_bMode;
	CRect	 m_bmpRect1,m_bmpRect2,m_bmpRect3;
	int m_nNumberSelected;
	CBitmap m_bmp1,m_bmp2,m_bmp3;
	CDC m_memdc,m_memdc2,m_memdc3;
// Construction
public:
	CDownLoadDlg(CWnd* pParent = NULL);   // standard constructor
	
	void InitDialog(BOOL bMode) {m_bMode = bMode;};  //Sets button window font sizes
	static void ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure);
// Dialog Data
	//{{AFX_DATA(CDownLoadDlg)
	enum { IDD = IDD_GO_ONOROFF_LINE };
	BOOL	m_bDownLoadDiscusions;
	BOOL	m_bDownLoadMail;
	BOOL	m_bSendMessages;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDownLoadDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDownLoadDlg)
	afx_msg void OnButtonSelect();
	virtual void OnOK();
	afx_msg void OnPaint();
	virtual BOOL OnInitDialog();
	virtual void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
