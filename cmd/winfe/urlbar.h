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
// CURLBar dialog

#include "toolbar2.h"
#include "dropmenu.h"

#ifndef URLBAR_H
#define URLBAR_H

class CEditWnd : public CGenericEdit
{
protected:
    UINT m_idTimer;
    BOOL m_bRestart;
    char * m_pComplete;
    BOOL m_Scroll;
    CWnd* m_pBar;
    CNSToolTip2 *m_ToolTip;

public:
    CEditWnd(CWnd* bar) { m_pBar = bar; m_ToolTip = 0; m_idTimer = 0; m_bRestart = TRUE; m_pComplete = NULL; m_Scroll = FALSE; }
    ~CEditWnd();
    void UrlCompletion(void);
    void DrawCompletion(CString & cs, char * pszResult);
    void SetToolTip(const char *inTipStr);
    virtual BOOL PreTranslateMessage ( MSG * msg );
    virtual LRESULT DefWindowProc( UINT message, WPARAM wParam, LPARAM lParam );
    virtual afx_msg void OnTimer( UINT  nIDEvent );
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP();
};

class CPageProxyWindow : public CWnd
{
private:
	BOOL			m_bEraseBackground;
	BOOL			m_bDraggingURL;
	BOOL			m_bDragIconHit;
	BOOL			m_bDragStatusHint;
	LPMWCONTEXT		m_pIMWContext;
	CPoint			m_cpLBDown;
	UINT			m_hFocusTimer;

	CNSToolTip2  m_ToolTip;

	CBitmap*		m_pBitmap;
	HICON			m_hIcon;

public:
	CPageProxyWindow();
	~CPageProxyWindow();
	BOOL Create(CWnd *pParent);
	void SetContext(LPUNKNOWN pUnk);

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

    // Generated message map functions
	//{{AFX_MSG(CPageProxyWindow)
	afx_msg void OnPaint();
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	virtual afx_msg void OnTimer( UINT  nIDEvent );
    afx_msg int OnMouseActivate( CWnd *, UINT, UINT );
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()


};

class CProxySurroundWnd : public CWnd
{
public:
	BOOL Create(CWnd *pParent);

	afx_msg void OnPaint();

	DECLARE_MESSAGE_MAP()
};

#define CURLBarBase	CDialogBar

class CURLBar : public CURLBarBase
{
	DECLARE_DYNCREATE(CURLBar)

protected:
	BOOL				m_bEraseBackground;
    LPUNKNOWN           m_pUnkTabCtrl;
	//CLM: Bitmap target drag current URL (bookmark format)
	CWnd                * m_pDragURL;
	LPMWCONTEXT			  m_pIMWContext;
	int 			      m_iViewType;
	int					  m_nTextStatus; // TRUE if 'location' is displayed as the text else FALSE
	int					  m_DragIconY;         //CLM: Icon for dragging URL of current doc
	int					  m_DragIconX;
	BOOL                  m_bDraggingURL;      // For drag/drop initiation
    BOOL                  m_bDragIconHit;
    BOOL                  m_bDragStatusHint;
	CPoint                m_cpLBDown;
    int                   m_lastSelection;
	HFONT				m_pFont;   // font for URL bar
	CDropMenu			* m_pDropMenu;
	CPageProxyWindow    * m_pPageProxy;
	CProxySurroundWnd	* m_pProxySurround;
	HBRUSH				  m_hBackgroundBrush;
	int					  m_nBoxHeight;
	
public:
// Dialog Data
    enum { IDD = IDD_URLTITLEBAR };

// XXX This should really have accessor fuctions
    BOOL                  m_bAddToDropList;
	CEditWnd*			  m_pBox;

// Construction
    CURLBar();  // standard constructor
    ~CURLBar(); 

	void SetContext(LPUNKNOWN pUnk);
	LPMWCONTEXT GetContext() const { return m_pIMWContext; }

    void UpdateFields( const char * msg);
    void SetToolTip(const char * inTip);
	
// Implementation
protected:
	void ProcessEnterKey();
	
	// Overrides
    virtual LRESULT WindowProc( UINT message, WPARAM wParam, LPARAM lParam );
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
    // Generated message map functions
	//{{AFX_MSG(CURLBar)
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditPaste();
	afx_msg void OnEditUndo();
	afx_msg void OnEditChange();
	afx_msg void OnSelChange();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
    afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnDestroy( );
	afx_msg void OnPaint();
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg HBRUSH OnCtlColor( CDC*, CWnd*, UINT );
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );

	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

#endif // URLBAR_H
