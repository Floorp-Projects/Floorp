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

// extview.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CExtView dialog

class CExtView : public CDialog
{
// Construction
public:
	CExtView(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CExtView)
	enum { IDD = IDD_ExternalViewerConfig };
	//}}AFX_DATA
	void * m_netList;
	
	void SetMimeTypeTo(const char * mime_type);
	void SetSubtypeTo(const char * sub_type);

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Generated message map functions
	//{{AFX_MSG(CExtView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClickedAddSubType();
	afx_msg void OnClickedLaunchExtViewer();
	afx_msg void OnClickedSaveToDisk();
	afx_msg void OnClickedViewerBrowse();
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg BOOL OnInitDialog() ;
	afx_msg void OnSelchangeMimetype();
	afx_msg void OnSelchangeMimesubtype();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
