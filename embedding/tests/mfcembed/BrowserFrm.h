/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 *   Chak Nanga <chak@netscape.com> 
 */

// BrowserFrm.h : interface of the CBrowserFrame class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _IBROWSERFRM_H
#define _IBROWSERFRM_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BrowserView.h"
#include "IBrowserFrameGlue.h"

// A simple UrlBar class...
class CUrlBar : public CComboBoxEx
{
public:
	inline GetEnteredURL(CString& url) {
		GetEditCtrl()->GetWindowText(url);
	}
	inline GetSelectedURL(CString& url) {
		GetLBText(GetCurSel(), url);
	}
	inline SetCurrentURL(LPCTSTR pUrl) {
		SetWindowText(pUrl);
	}
	inline AddURLToList(CString& url) {
		COMBOBOXEXITEM ci;
		ci.mask = CBEIF_TEXT; 	ci.iItem = -1;
		ci.pszText = (LPTSTR)(LPCTSTR)url;
		InsertItem(&ci);
	}
};

class CBrowserFrame : public CFrameWnd
{	
public:
	CBrowserFrame(PRUint32 chromeMask);

protected: 
	DECLARE_DYNAMIC(CBrowserFrame)

public:
	CToolBar    m_wndToolBar;
	CStatusBar  m_wndStatusBar;
	CProgressCtrl m_wndProgressBar;
	CUrlBar m_wndUrlBar;
	CReBar m_wndReBar;
	// The view inside which the embedded browser will
	// be displayed in
	CBrowserView    m_wndBrowserView;

	// This specifies what UI elements this frame will sport
	// w.r.t. toolbar, statusbar, urlbar etc.
	PRUint32 m_chromeMask;

protected:
	//
	// This nested class implements the IBrowserFramGlue interface
	// The Gecko embedding interfaces call on this interface
	// to update the status bars etc.
	//
	class BrowserFrameGlueObj : public IBrowserFrameGlue 
	{
		//
		// NS_DECL_BROWSERFRAMEGLUE below is a macro which expands
		// to the function prototypes of methods in IBrowserFrameGlue
		// Take a look at IBrowserFrameGlue.h for this macro define
		//

		NS_DECL_BROWSERFRAMEGLUE

	} m_xBrowserFrameGlueObj;
	friend class BrowserFrameGlueObj;

public:
	void SetupFrameChrome();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowserFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CBrowserFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CBrowserFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif //_IBROWSERFRM_H
