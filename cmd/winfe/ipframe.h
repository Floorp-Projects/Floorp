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

#ifndef __InPlaceFrame_H
#define __InPlaceFrame_H
// ipframe.h : interface of the CInPlaceFrame class
//
//#include "nstoolbr.h"
#include "frameglu.h"
#include "urlbar.h"

#include "statbar.h"

class CInPlaceFrame : public COleIPFrameWnd, public CFrameGlue
{
	DECLARE_DYNCREATE(CInPlaceFrame)
public:
	CInPlaceFrame();

//	CFrameGlue required overrides
public:
//	CNSToolbar2 m_wndToolBar;
//	CCommandToolbar *m_wndToolBar;
	CURLBar		m_wndURLBar;
	BOOL		m_iShowURLBar;
	BOOL		m_iShowStarterBar;
	int16		m_iCSID;

	virtual CFrameWnd *GetFrameWnd();
	virtual void UpdateHistoryDialog();
    void	ShowControlBars(CFrameWnd* pFrameWnd, BOOL bShow);
	void	ReparentControlBars(void);
 
	CNetscapeStatusBar m_barStatus;
        
//	The frame we took over from.
private:
	CFrameGlue *m_pProxy2Frame;

//	Wether or not we've formally created the
//		control bars.
private:
	BOOL m_bCreatedControls;

// Attributes
public:
	void DestroyResizeBar(); //JOKI

// Operations
public:
	virtual BOOL LoadFrame(UINT nIDResource,
		DWORD dwDefaultStyle = WS_CHILD|WS_BORDER|WS_CLIPSIBLINGS,
		CWnd* pParentWnd = NULL,
		CCreateContext* pContext = NULL);
	virtual BOOL BuildSharedMenu();


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInPlaceFrame)
	public:
	virtual BOOL OnCreateControlBars(CWnd* pWndFrame, CWnd* pWndDoc);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    afx_msg void OnInitMenuPopup(CMenu * pPopup, UINT nIndex, BOOL bSysMenu);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnToolsWeb();

	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CInPlaceFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	COleResizeBar   m_wndResizeBar;
	COleDropTarget m_dropTarget;

// Generated message map functions
protected:

	void AddBookmarksMenu(void);
	virtual HMENU GetInPlaceMenu();

	//{{AFX_MSG(CInPlaceFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnHotlistView();
    afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnQueryNewPalette();
	afx_msg void OnPaletteChanged(CWnd* pFocusWnd);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif // __InPlaceFrame_H
