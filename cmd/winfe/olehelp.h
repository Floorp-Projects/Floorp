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

/////////////////////////////////////////////////////////////////////////////
// COleHelp command target

class COleHelp : public CCmdTarget
{
	DECLARE_DYNCREATE(COleHelp)

	COleHelp();           // protected constructor used by dynamic creation

// Attributes
public:
    CStubsCX *m_pSlaveCX;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COleHelp)
	public:
	virtual void OnFinalRelease();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~COleHelp();

	// Generated message map functions
	//{{AFX_MSG(COleHelp)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(COleHelp)

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(COleHelp)
	afx_msg void HtmlHelp(LPCTSTR pMapFileUrl, LPCTSTR pId, LPCTSTR pSearch);
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
