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
 *   Rod Spears <rods@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// BrowserView.h : interface of the CBrowserView class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _BROWSERVIEW_H
#define _BROWSERVIEW_H

#if _MSC_VER > 1000
    #pragma once
#endif

#include "IBrowserFrameGlue.h"
#include "nsIPrintSettings.h"

/////////////////////////////////////////////////////////////////////////////
// CBrowserView window

class CBrowserFrame;
class CBrowserImpl;
class CFindDialog;
class CPrintProgressDialog;

class CBrowserView : public CWnd
{
public:
    CBrowserView();
    virtual ~CBrowserView();

    // Some helper methods
    HRESULT CreateBrowser();
    HRESULT DestroyBrowser();
    void OpenURL(const char* pUrl);
    void OpenURL(const PRUnichar* pUrl);
    CBrowserFrame* CreateNewBrowserFrame(PRUint32 chromeMask = nsIWebBrowserChrome::CHROME_ALL, 
                            PRInt32 x = -1, PRInt32 y = -1, 
                            PRInt32 cx = -1, PRInt32 cy = -1,
                            PRBool bShowWindow = PR_TRUE);
    void OpenURLInNewWindow(const PRUnichar* pUrl);
    void LoadHomePage();

    void GetBrowserWindowTitle(nsAString& title);
    
    // Called by the CBrowserFrame after it creates the view
    // Essentially a back pointer to the BrowserFrame
    void SetBrowserFrame(CBrowserFrame* pBrowserFrame);
    CBrowserFrame* mpBrowserFrame;

    // Called by the CBrowserFrame after it creates the view
    // The view passes this on to the embedded Browser's Impl
    // obj
    void SetBrowserFrameGlue(PBROWSERFRAMEGLUE pBrowserFrameGlue);
    PBROWSERFRAMEGLUE mpBrowserFrameGlue;

    // Pointer to the object which implements
    // the inerfaces required by Mozilla embedders
    //
    CBrowserImpl* mpBrowserImpl;

    // Mozilla interfaces
    //
    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIBaseWindow> mBaseWindow;
    nsCOMPtr<nsIWebNavigation> mWebNav;    

    void UpdateBusyState(PRBool aBusy);
    PRBool mbDocumentLoading;

    void SetCtxMenuLinkUrl(nsEmbedString& strLinkUrl);
    nsEmbedString mCtxMenuLinkUrl;

    void SetCtxMenuImageSrc(nsEmbedString& strImgSrc);
    nsEmbedString mCtxMenuImgSrc;

    void SetCurrentFrameURL(nsEmbedString& strCurrentFrameURL);
    nsEmbedString mCtxMenuCurrentFrameURL;

    inline void ClearFindDialog() { m_pFindDlg = NULL; }
    CFindDialog* m_pFindDlg;
    CPrintProgressDialog* m_pPrintProgressDlg;
    // When set to TRUE...
    // indicates that the clipboard operation needs to be 
    // performed on the UrlBar rather than on
    // the web page content
    //
    BOOL m_bUrlBarClipOp;

    // indicates whether we are currently printing
    BOOL m_bCurrentlyPrinting;

    void Activate(UINT nState, CWnd* pWndOther, BOOL bMinimized);

    BOOL OpenViewSourceWindow(const PRUnichar* pUrl);
    BOOL OpenViewSourceWindow(const char* pUrl);
    BOOL IsViewSourceUrl(CString& strUrl);

    enum _securityState {
        SECURITY_STATE_SECURE,
        SECURITY_STATE_INSECURE,
        SECURITY_STATE_BROKEN
    };
    int m_SecurityState;
    void ShowSecurityInfo();

    BOOL ViewContentContainsFrames();
    
    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CBrowserView)
    protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    //}}AFX_VIRTUAL


    // Generated message map functions
protected:
    nsCOMPtr<nsIPrintSettings> m_PrintSettings;
  BOOL                       m_InPrintPreview;

    //{{AFX_MSG(CBrowserView)
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnSize( UINT, int, int );
    // UrlBar command handlers
    //
    afx_msg void OnUrlSelectedInUrlBar();
    afx_msg void OnNewUrlEnteredInUrlBar();

    // ToolBar/Menu command handlers
    //
    afx_msg void OnFileOpen();
    afx_msg void OnFileSaveAs();
    afx_msg void OnViewSource();
    afx_msg void OnViewInfo();
    afx_msg void OnNavBack();
    afx_msg void OnNavForward();
    afx_msg void OnNavHome();
    afx_msg void OnNavReload();
    afx_msg void OnNavStop();
    afx_msg void OnCut();
    afx_msg void OnCopy();
    afx_msg void OnPaste();
    afx_msg void OnUndoUrlBarEditOp();
    afx_msg void OnSelectAll();
    afx_msg void OnSelectNone();
    afx_msg void OnCopyLinkLocation();
    afx_msg void OnOpenLinkInNewWindow();
    afx_msg void OnViewImageInNewWindow();
    afx_msg void OnSaveLinkAs();
    afx_msg void OnSaveImageAs();
    afx_msg void OnShowFindDlg();
    afx_msg void OnFilePrint();
    afx_msg void OnFilePrintPreview();
    afx_msg void OnFilePrintSetup();
    afx_msg void OnUpdateFilePrint(CCmdUI* pCmdUI);
    afx_msg void OnUpdatePrintSetup(CCmdUI* pCmdUI);
    afx_msg LRESULT OnFindMsg(WPARAM wParam, LPARAM lParam);
    afx_msg void OnViewFrameSource();
    afx_msg void OnOpenFrameInNewWindow();

    // Handlers to keep the toolbar/menu items up to date
    //
    afx_msg void OnUpdateNavBack(CCmdUI* pCmdUI);
    afx_msg void OnUpdateNavForward(CCmdUI* pCmdUI);
    afx_msg void OnUpdateNavStop(CCmdUI* pCmdUI);
    afx_msg void OnUpdateCut(CCmdUI* pCmdUI);
    afx_msg void OnUpdateCopy(CCmdUI* pCmdUI);
    afx_msg void OnUpdatePaste(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewImage(CCmdUI* pCmdUI);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

#endif //_BROWSERVIEW_H
