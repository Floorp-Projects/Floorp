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

// oleregis.h : header file
//



/////////////////////////////////////////////////////////////////////////////
// COleRegistry command target

class COleRegistry : public CCmdTarget
{
	DECLARE_DYNCREATE(COleRegistry)
protected:
	COleRegistry();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:
    static void RegisterIniViewers();
    static void RegisterIniProtocolHandlers();
    static void Startup();
    static void Shutdown();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COleRegistry)
	public:
	virtual void OnFinalRelease();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~COleRegistry();

	// Generated message map functions
	//{{AFX_MSG(COleRegistry)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(COleRegistry)

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(COleRegistry)
	afx_msg BOOL RegisterViewer(LPCTSTR pMimeType, LPCTSTR pRegistryName);
	afx_msg BOOL RegisterProtocol(LPCTSTR pProtcol, LPCTSTR pRegistryName);
    afx_msg BOOL RegisterStartup(LPCTSTR pRegistryName);
    afx_msg BOOL RegisterShutdown(LPCTSTR pRegistryName);
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
};

/////////////////////////////////////////////////////////////////////////////
