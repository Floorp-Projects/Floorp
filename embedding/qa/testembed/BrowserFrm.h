/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "MostRecentUrls.h"

// A simple UrlBar class...
class CUrlBar : public CComboBoxEx
{
public:
	inline void GetEnteredURL(CString& url) {
		GetEditCtrl()->GetWindowText(url);
	}
	inline void GetSelectedURL(CString& url) {
		GetLBText(GetCurSel(), url);
	}
	inline void SetCurrentURL(LPCTSTR pUrl) {
		SetWindowText(pUrl);
	}
	inline void AddURLToList(CString& url, bool bAddToMRUList = true) {
		COMBOBOXEXITEM ci;
		ci.mask = CBEIF_TEXT; ci.iItem = -1;
		ci.pszText = (LPTSTR)(LPCTSTR)url;
		InsertItem(&ci);

        if(bAddToMRUList)
            m_MRUList.AddURL((LPTSTR)(LPCTSTR)url);
	}
    inline void LoadMRUList() {
        for (int i=0;i<m_MRUList.GetNumURLs();i++) 
        {
            CString urlStr(_T(m_MRUList.GetURL(i)));
            AddURLToList(urlStr, false); 
        }
    }
    inline BOOL EditCtrlHasFocus() {
        return (GetEditCtrl()->m_hWnd == CWnd::GetFocus()->m_hWnd);
    }
    inline BOOL EditCtrlHasSelection() {
        int nStartChar = 0, nEndChar = 0;
        if(EditCtrlHasFocus())
            GetEditCtrl()->GetSel(nStartChar, nEndChar);
        return (nEndChar > nStartChar) ? TRUE : FALSE;
    }
    inline BOOL CanCutToClipboard() {
        return EditCtrlHasSelection();
    }
    inline void CutToClipboard() {
        GetEditCtrl()->Cut();
    }
    inline BOOL CanCopyToClipboard() {
        return EditCtrlHasSelection();
    }
    inline void CopyToClipboard() {
        GetEditCtrl()->Copy();
    }
    inline BOOL CanPasteFromClipboard() {
        return EditCtrlHasFocus();
    }
    inline void PasteFromClipboard() {
        GetEditCtrl()->Paste();
    }
    inline BOOL CanUndoEditOp() {
        return EditCtrlHasFocus() ? GetEditCtrl()->CanUndo() : FALSE;
    }
    inline void UndoEditOp() {        
        if(EditCtrlHasFocus())
            GetEditCtrl()->Undo();
    }

protected:
      CMostRecentUrls m_MRUList;
};

class CBrowserFrame : public CFrameWnd
{	
public:
	CBrowserFrame(PRUint32 chromeMask);

protected: 
	DECLARE_DYNAMIC(CBrowserFrame)

public:
	inline CBrowserImpl *GetBrowserImpl() { return m_wndBrowserView.mpBrowserImpl; }

	CToolBar    m_wndToolBar;
	CStatusBar  m_wndStatusBar;
	CProgressCtrl m_wndProgressBar;
	CUrlBar m_wndUrlBar;
	CReBar m_wndReBar;
	// The view inside which the embedded browser will
	// be displayed in
	CBrowserView    m_wndBrowserView;

    // Wrapper functions for UrlBar clipboard operations
    inline BOOL CanCutUrlBarSelection() { return m_wndUrlBar.CanCutToClipboard(); }
    inline void CutUrlBarSelToClipboard() { m_wndUrlBar.CutToClipboard(); }
    inline BOOL CanCopyUrlBarSelection() { return m_wndUrlBar.CanCopyToClipboard(); }
    inline void CopyUrlBarSelToClipboard() { m_wndUrlBar.CopyToClipboard(); }
    inline BOOL CanPasteToUrlBar() { return m_wndUrlBar.CanPasteFromClipboard(); }
    inline void PasteFromClipboardToUrlBar() { m_wndUrlBar.PasteFromClipboard(); }
    inline BOOL CanUndoUrlBarEditOp() { return m_wndUrlBar.CanUndoEditOp(); }
    inline void UndoUrlBarEditOp() { m_wndUrlBar.UndoEditOp(); }

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
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif //_IBROWSERFRM_H
