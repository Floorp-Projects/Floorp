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

#ifndef __GenericView_H
#define __GenericView_H
// genview.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGenericView view

class CGenericView : public CView
{
protected:
	CGenericView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CGenericView)

//	Which window currently has focus, used to set form element focus.
//	This possibly belongs to the Frame....
public:
    HWND m_hWndFocus;

//	Document access, actually routed through the context.
public:
    CGenericDoc *GetDocument();

protected:
    //	The context.
	CWinCX *m_pContext;

    // used for drawing backgrounds of forms    
    HBRUSH   m_hCtlBrush;
    COLORREF m_rgbBrushColor;

public:
	CWinCX *GetContext() const	{
		return(m_pContext);
	}
	virtual void SetContext(CAbstractCX *pContext);
	void ClearContext()	{
		m_pContext = NULL;
	}

//	Frame access.
public:
	CFrameGlue *GetFrame() const;

//	Frame closing notification.
public:
	virtual void FrameClosing();

//	Wether or not we are in print preview
// TODO: Move the print code to CGenericView
protected:
	BOOL m_bInPrintPreview;

// To restore Format/Character toolbar in Message Composer after Print Preview
    BOOL m_bRestoreComposerToolbar;

public:
	BOOL IsInPrintPreview() const	{
		return(m_bInPrintPreview);
	}

//	Need some friends which can call our protected functions.
private:
	friend class CGenericFrame;
    friend class CNetscapePreviewView;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGenericView)
	public:
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnActivateView(BOOL bActivate, CView *pActivateView, CView *pDeactivateView);
	virtual BOOL PreTranslateMessage(MSG * pMsg);
	//}}AFX_VIRTUAL

//#ifndef NO_TAB_NAVIGATION
public :
	BOOL CGenericView::procTabNavigation( UINT nChar, UINT forward, UINT controlKey );
//#endif /* NO_TAB_NAVIGATION */

// Implementation
protected:
	virtual ~CGenericView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

public:
    // OnFindReplace message goes first to our frame
	LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);

#ifdef LAYERS
	virtual BOOL OnRButtonDownForLayer(UINT nFlags, CPoint& point, 
					   long lX, long lY, CL_Layer *layer) 
	  { return FALSE; }
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CGenericView)
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg int OnMouseActivate( CWnd *, UINT, UINT );
    afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );
	afx_msg void OnFileMailto();
	afx_msg void OnUpdateFileMailto(CCmdUI* pCmdUI);
	afx_msg void OnFileOpen();
	afx_msg void OnUpdateFileOpen(CCmdUI* pCmdUI);
	afx_msg void OnNetscapeSaveAs();
	afx_msg void OnUpdateNetscapeSaveAs(CCmdUI* pCmdUI);
	afx_msg void OnNetscapeSaveFrameAs();
	afx_msg void OnUpdateNetscapeSaveFrameAs(CCmdUI* pCmdUI);
	afx_msg void OnNavigateBack();
	afx_msg void OnUpdateNavigateBack(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavigateForward(CCmdUI* pCmdUI);
	afx_msg void OnNavigateForward();
	afx_msg void OnNavigateReload();
	afx_msg void OnUpdateNavigateReload(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewLoadimages(CCmdUI* pCmdUI);
	afx_msg void OnViewLoadimages();
	afx_msg void OnFilePrint();
	afx_msg void OnUpdateFilePrint(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilePrintPreview(CCmdUI* pCmdUI);
	afx_msg void OnEditFindincurrent();
	afx_msg void OnUpdateEditFindincurrent(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditWithFrameFindincurrent(CCmdUI* pCmdUI);
	afx_msg void OnEditFindAgain();
	afx_msg void OnUpdateEditFindAgain(CCmdUI* pCmdUI);
	afx_msg void OnNavigateInterrupt();
	afx_msg void OnUpdateNavigateInterrupt(CCmdUI* pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
    afx_msg void OnSelectAll();
	afx_msg void OnFileViewsource();
	afx_msg void OnUpdateFileViewsource(CCmdUI* pCmdUI);
	afx_msg void OnFileDocinfo();
	afx_msg void OnUpdateFileDocinfo(CCmdUI* pCmdUI);
	afx_msg void OnViewPageServices();
	afx_msg void OnUpdateViewPageServices(CCmdUI* pCmdUI);
	afx_msg void OnGoHome();
	afx_msg void OnUpdateGoHome(CCmdUI* pCmdUI);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnFileUploadfile();
	afx_msg void OnUpdateFileUploadfile(CCmdUI* pCmdUI);
	afx_msg void OnNavigateReloadcell();
	afx_msg void OnUpdateNavigateReloadcell(CCmdUI* pCmdUI);
	afx_msg void OnViewFrameInfo();
	afx_msg void OnUpdateViewFrameInfo(CCmdUI* pCmdUI);
	afx_msg void OnViewFrameSource();
	afx_msg void OnUpdateViewFrameSource(CCmdUI* pCmdUI);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnNcPaint();
    afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif // __GenericView_H
