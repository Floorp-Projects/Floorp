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

// BrowserView.h : interface of the CBrowserView class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _BROWSERVIEW_H
#define _BROWSERVIEW_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "IBrowserFrameGlue.h"

/////////////////////////////////////////////////////////////////////////////
// CBrowserView window

class CBrowserFrame;
class CBrowserImpl;
class CFindDialog;
class CPrintProgressDialog;

class CTests;
//class CNsIRequest;
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

	void GetBrowserWindowTitle(nsCString& title);
	
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

    CTests *qaTests ;
	//CNsIRequest *nsiRequest;

	// Mozilla interfaces
	//
	nsCOMPtr<nsIWebBrowser> mWebBrowser;
	nsCOMPtr<nsIBaseWindow> mBaseWindow;
	nsCOMPtr<nsIWebNavigation> mWebNav;	

	typedef void* nsNativeWidget;

	//nsCOMPtr<nsISHistory> mSessionHistory; // de: added in 5/11/01

	void UpdateBusyState(PRBool aBusy);
	PRBool mbDocumentLoading;

	void SetCtxMenuLinkUrl(nsAutoString& strLinkUrl);
	nsAutoString mCtxMenuLinkUrl;

	void SetCtxMenuImageSrc(nsAutoString& strImgSrc);
	nsAutoString mCtxMenuImgSrc;

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

    BOOL OpenViewSourceWindow(const char* pUrl);
    BOOL IsViewSourceUrl(CString& strUrl);

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowserView)

		
	// de: qa methods
	static void WriteToOutputFile(char * strInput);
	static char *theUrl;
	static void RvTestResult(nsresult rv, char *pLine, int displayMethod=1);
	static void QAOutput(char *pLine, int displayMethod=1);

	public:
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL


	// Generated message map functions
protected:
	//{{AFX_MSG(CBrowserView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize( UINT, int, int );
	afx_msg void OnUrlSelectedInUrlBar();
	afx_msg void OnNewUrlEnteredInUrlBar();
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
	afx_msg void OnUpdateFilePrint(CCmdUI* pCmdUI);
	afx_msg LRESULT OnFindMsg(WPARAM wParam, LPARAM lParam);
	afx_msg void OnUpdateNavBack(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavForward(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavStop(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCut(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCopy(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePaste(CCmdUI* pCmdUI);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif //_BROWSERVIEW_H
