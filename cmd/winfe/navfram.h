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

// navfram.h : interface of the CNSNavFrame class
//
/////////////////////////////////////////////////////////////////////////////
#ifndef NAVFRAM
#define NAVFRAM
#include "navbar.h"
#include "navcntr.h"
#include "navcontv.h"
#include "genframe.h"
#include "dragbar.h"   

#ifdef XP_WIN32
#define EXPAND_STEP			10
#else
#define EXPAND_STEP			1
#endif

#define DRAGWIDTH			3
#define STARTX				0
#define STARTY				0
#define MIN_CATEGORY_WIDTH 32
#define MIN_CATEGORY_HEIGHT 32
#define CX_BORDER 2
#define CY_BORDER 2
#define NAVFRAME_WIDTH 300
#define NAVFRAME_HEIGHT 480
#define DOCKSTYLE_FLOATING		0
#define DOCKSTYLE_VERTLEFT		1
#define DOCKSTYLE_VERTRIGHT		2
#define DOCKSTYLE_HORZTOP		3
#define DOCKSTYLE_HORZBOTTOM	4

// This is a CFrameWnd window that knows how to dock and float.  This 
// frame comtains 2 panes.  The left pane is for the selector.  The right pane is to
// display the content for the selector.
static CString gPrefDockOrientation = "browser.navcenter.dockstyle";
static CString gPrefDockPercentage = "browser.navcenter.docked.tree.width";
static CString gPrefFloatRect = "browser.navcenter.floating.rect";
static CString gPrefSelectorVisible = "browser.navcenter.docked.selector.visible";

BOOL nsModifyStyle(HWND hWnd, int nStyleOffset,DWORD dwRemove, DWORD dwAdd, UINT nFlags = 0);

class CNSNavFrame : public CFrameWnd
{
friend class CDragBar;
public: // create from serialization only
	CNSNavFrame();
	DECLARE_DYNCREATE(CNSNavFrame)
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNSNavFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNSNavFrame();
	CContentView* GetContentView()	{return m_nsContent;}
	// Add new view to this frame.
	//void AddViewContext(const char* pUrl, const char* pTitle, CView* pView, CPaneCX* htmlPane = NULL);

	// if pParentFrame == NULL, means we want to create the window as a floating window.
	// otherwise we want to create a window that is docked in this frame.
	void CreateNewNavCenter(CNSGenFrame* pParentFrame = NULL, BOOL useViewType = FALSE, HT_ViewType viewType = HT_VIEW_BOOKMARK);
	void DeleteNavCenter();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	virtual void StartDrag(CPoint pt, BOOL mapDesktop = TRUE);
	void Move(CPoint pt);       // called when mouse has moved
	void EndDrag(CPoint pt);             // drop
	void InitLoop();
	void CancelLoop(short dockStyle);
	void DrawFocusRect(short dockStyle, BOOL bRemoveRect = FALSE);
	BOOL Track();
	short CanDock(CPoint pt, BOOL mapDesktop = FALSE);
	void ForceFloat(BOOL show = TRUE);
	int32 GetDockStyle() { return m_dwOverDockStyle;}
	void ExpandWindow();
	void CollapseWindow();

	void ComputeDockingSizes();
	void UpdateTitleBar(HT_View pView);
    virtual void PostNcDestroy();

public:
    HT_Pane GetHTPane();
	CNavMenuBar* GetNavMenuBar() { return m_pNavMenu; }
	BOOL IsTreeVisible() { return m_pSelector->IsTreeVisible(); }

protected:  // control bar embedded members
	friend class CSelector;
	BOOL		m_bDragging;		// TRUE if we are dragging this FRAME
	BOOL		m_bDitherLast;		// 
	int32		m_dwOverDockStyle;	// The orientation 
	
	CRect		m_rectLast;
	CSize		m_sizeLast;
	CPoint		m_ptLast;            // last mouse position during drag
	CDC*		m_pDC;				 // where to draw during drag
	CDragBar*	m_DragWnd;			// the resize bar, when this frame is docked.
	
	// All of these rects are in the Desktop window's coordinates.

	CRect m_rectDrag;				// bounding rect when this frame is floating
	CRect m_rectFloat;
	CRect m_dockingDragRect;		// the bounding rect inside the parent frame.
	CRect m_parentRect;

	int m_DockWidth, m_DockHeight, m_DockSize;  // Used to determine docking sizes.
	int m_nXOffset, m_nYOffset;

	CSelector* m_pSelector;		// the selector pane.
	CContentView *m_nsContent;	// the content pane.
	CNavMenuBar* m_pNavMenu;	// the embedded menu bar.

    // Indicator used when being docked/undocked.
    enum {
        unknown,
        docked,
        beingDocked,
        undocked,
        beingUndocked
    } m_dockingState;

// Generated message map functions
protected:
	void CNSNavFrame::CalcClientArea(RECT* lpRectClient, CNSGenFrame * pParentFrame = NULL);
	void CNSNavFrame::DockFrame(CNSGenFrame* pParent, short dockStyle);
	afx_msg void OnPaint();
	//{{AFX_MSG(CNSNavFrame)
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose( );
	afx_msg LRESULT OnSizeParent(WPARAM wParam, LPARAM lParam);
    afx_msg void OnNcLButtonDown( UINT nHitTest, CPoint point );
	afx_msg void OnMouseMove(UINT nHitTest, CPoint point);
	afx_msg void OnLButtonUp(UINT nHitTest, CPoint point);

		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	afx_msg void OnSize( UINT nType, int cx, int cy );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif
