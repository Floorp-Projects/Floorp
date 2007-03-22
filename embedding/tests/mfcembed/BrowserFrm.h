/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
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
#include "BrowserToolTip.h"
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
        USES_CONVERSION;
        COMBOBOXEXITEM ci;
        ci.mask = CBEIF_TEXT; ci.iItem = -1;
        ci.pszText = const_cast<TCHAR *>((LPCTSTR)url);
        InsertItem(&ci);

        if(bAddToMRUList)
            m_MRUList.AddURL(T2CA(url));
    }
    inline void LoadMRUList() {
        for (int i=0;i<m_MRUList.GetNumURLs();i++) 
        {
            USES_CONVERSION;
            CString urlStr(A2CT(m_MRUList.GetURL(i)));
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

// CMyStatusBar class
class CMyStatusBar : public CStatusBar
{
public:
    CMyStatusBar();
    virtual ~CMyStatusBar();

protected:
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()
};

class CBrowserFrame : public CFrameWnd
{    
public:
    CBrowserFrame();
    CBrowserFrame(PRUint32 chromeMask);

protected: 
    DECLARE_DYNAMIC(CBrowserFrame)

public:
    inline CBrowserImpl *GetBrowserImpl() { return m_wndBrowserView.mpBrowserImpl; }

    CToolBar    m_wndToolBar;
    CMyStatusBar  m_wndStatusBar;
    CProgressCtrl m_wndProgressBar;
    CUrlBar m_wndUrlBar;
    CReBar m_wndReBar;
    CBrowserToolTip m_wndTooltip;

    // The view inside which the embedded browser will
    // be displayed in
    CBrowserView    m_wndBrowserView;

    void UpdateSecurityStatus(PRInt32 aState);
    void ShowSecurityInfo();

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
    void SetEditable(BOOL isEditor) { mIsEditor = isEditor; }
    BOOL GetEditable() { return mIsEditor; }

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

private:
    BOOL mIsEditor;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif //_IBROWSERFRM_H
