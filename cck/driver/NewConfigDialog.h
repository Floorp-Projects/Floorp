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

// NewConfigDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNewConfigDialog dialog

class CNewConfigDialog : public CDialog
{
// Construction
public:
	CNewConfigDialog(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewConfigDialog)
	enum { IDD = IDD_NEWCONFIG_DIALOG };
	CString	m_NewConfig_field;
	//}}AFX_DATA

	CString GetConfigName();
	void DDV_Config(CDataExchange* pDX, CString value);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewConfigDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
	CString newConfigName;

protected:

	// Generated message map functions
	//{{AFX_MSG(CNewConfigDialog)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
