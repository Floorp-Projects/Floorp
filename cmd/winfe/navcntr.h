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

#ifndef NAVCNTR_H
#define NAVCNTR_H

#include "usertlbr.h"

//  Window Control IDs for NavCenter.
//  Improves findability.
#define NC_IDW(ID) (AFX_IDW_PANE_LAST - (ID))
#define NC_IDW_MISCVIEW NC_IDW(0)
#define NC_IDW_SELECTOR NC_IDW(1)
#define NC_IDW_OUTLINER NC_IDW(2)
#define NC_IDW_HTMLPANE NC_IDW(3)
#define NC_IDW_DRAGEDGE NC_IDW(5)
#define NC_IDW_NAVMENU NC_IDW(6)


#define ICONXPOS  5
#define ICONYPOS  5
#define ICONYINC  42
#define SCROLLID 2900
#define ICONBASEID 3000
#define BUTTON_HEIGHT 28
#define BUTTON_WIDTH 28
#define IMAGE_SIZE 23
#define TEXT_HEIGHT 20
#define SCROLLTIMERID 2910
#define NOSCROLL 0
#define SCROLLUP 1
#define SCROLLDOWN 2
#define SCROLLLEFT 3
#define SCROLLRIGHT 4
#define SCROLLSTEP 32
#define MAX_BUTTON_TITLE 54

#define EDIT_CONTROL  4000

#define IDT_PANESWITCH 40000
#define IDT_SWITCHDELAY	500

#include "cxicon.h"
#include "htrdf.h"

#define ID_BUTTONEVENT 1000
#define ID_MOUSE_MOVE  1001

class CRDFContentView;

class CSelectorButton : public CRDFToolbarButton
{
friend class CSelector;

protected:
	CView* pView;	// View including the HTML pane
	CRDFContentView* m_pTreeView; // Pointer straight to the RDF Tree view.
	CPaneCX* m_pPane;
	CSelector* m_pSelector;
	HT_View m_HTView; // Pointer to HT_View if one exists.  Could be NULL.
	HT_Pane m_Pane; // This is the button's popup menu pane.
	// The Selector Button actually has two HT_Nodes, two HT_Views, and two HT_Panes. One set is the pane in
	// which the button is found (along with all the other buttons).  The second set is for the popup menu
	// that comes up on a timed mouse down on the button.  
	// m_HTView is used with the pane that contains all the buttons.
	// m_Pane is used for the popup menu.
	// The base class has an m_Node member variable.  This is the popup variable's top node in its selected
	// HT_View.  -- Dave H.

public:
	CSelectorButton(CSelector* pSelector) 
		:m_pSelector(pSelector), pView(NULL), m_pPane(NULL), 
		 m_HTView(NULL) {};
	
	~CSelectorButton();

	void SetHTView(HT_View v) { m_HTView = v; }
	virtual HT_View GetHTView() { return m_HTView; }

	CView* GetView() { return pView; }

	virtual BOOL UseLargeIcons() { return TRUE; }

	virtual void OnAction(void);

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText,
			   CSize bitmapSize, int nMaxTextChars, int nMinTextChars, 
			   HT_Resource pNode, DWORD dwButtonStyle = 0, CView* view = NULL, CPaneCX* pane = NULL);

	void SetDepressed(BOOL b) { m_bDepressed = b; Invalidate(); }

	CRDFContentView* GetTreeView() { return m_pTreeView; }	// Returns only the tree

	void SetTreeView(CRDFContentView* tree) { m_pTreeView = tree; } // Sets the tree view

	virtual void DrawPicturesMode(HDC hDC, CRect rect);
		// Overridden because LinkToolbarButtons draw text with the pictures always.  We
		// want to revert to ToolbarButton behavior.

	virtual void DrawPicturesAndTextMode(HDC hDC, CRect rect);
		// Overridden to ensure bitmap is drawn on top rather than on side (as is the case
		// with LinkToolbar buttons).

	virtual CSize GetButtonSizeFromChars(CString s, int c);
		// Overridden to put the bitmap on top.

	virtual void GetPicturesAndTextModeTextRect(CRect &rect);
		// more bitmap on top goodness

	virtual void GetPicturesModeTextRect(CRect &rect);
		// gotta have that bitmap on top

	virtual CPoint RequestMenuPlacement()
	{
		CPoint point(GetRequiredButtonSize().cx, 0);
		ClientToScreen(&point);
		return point;  // Want it to open on the side.
	}

	void DisplayMenuOnDrag();
		// Called when the user mouses over a selector button and NO TREE IS DISPLAYED (i.e., just the
		// selector is showing in a docked view.

	virtual BOOL CreateRightMouseMenu();
		// Overridden to bring up context menus.

};

class CSelectorDropTarget : public COleDropTarget
{
public:
//Construction
    CSelectorDropTarget(CSelector* parentFrame);
    ~CSelectorDropTarget();
    BOOL        OnDrop(CWnd *, COleDataObject *, DROPEFFECT, CPoint);
    DROPEFFECT  OnDragEnter(CWnd *, COleDataObject *, DWORD, CPoint);
    DROPEFFECT  OnDragOver(CWnd *, COleDataObject *, DWORD, CPoint);
    void        OnDragLeave(CWnd *);
    CSaveCX*		pSaveContext;
	char		fileName[_MAX_PATH];
	CFile* fd;
	CSelector* m_pParent;
	CString		m_url;
private:
    int m_nIsOKDrop;
};

class CNavSelScrollButton : public CButton
{
public:
	CNavSelScrollButton(HBITMAP hBmp, int width, int index, int height);
	HBITMAP m_hbmp;
	int m_buttonID;
	int m_index;
	int m_width;
	int m_height;
	BOOL m_selected;
	CRect m_boundBox;
	DECLARE_DYNCREATE(CNavSelScrollButton)
	void setBoundingBox(int left, int top, int right, int bottom) {
		 m_boundBox.SetRect(left, top, right, bottom);
	}
	void AdjustPos(){	MoveWindow(m_boundBox);}
	BOOL IsPtInRect(POINT &pt) 
	{ if (PtInRect(&m_boundBox, pt)) return TRUE;
	  else return FALSE;}
	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CButton)

	//}}AFX_VIRTUAL
	DECLARE_MESSAGE_MAP()
};


class CSelector : public CView
{

friend class CSelectorButton;
public:
	DECLARE_DYNAMIC(CSelector)
	CSelector();
	virtual ~CSelector();
	void AddViewContext(const char* pTitle, CView* pView, CRDFContentView* pTree, 
						CPaneCX* htmlPane, HT_View theView);
	CView* GetCurrentView(); // Gets the view including the HTML pane
	CSelectorButton* GetCurrentButton();

	void SetCurrentView(CSelectorButton* pButton);
	void SelectNthView(int i);
	
	void PopulatePane();
	void DestroyViews();
	void ConstructView(HT_View v);
	HT_Pane GetHTPane() {return m_Pane;}
	void ShowScrollButton(CSelectorButton* button);
	void OnDraw( CDC* pDC );
	int GetScrollDirection() {return m_scrollDirection;}
	void ScrollSelector();
	void StopScrolling() {m_scrollDirection = NOSCROLL;}
	void UnSelectAll();
	HBRUSH GetNormalBrush() {return hBrush;}
	HBRUSH GetHtBrush() {return hHtBrush;}

	void SetDragButton(CSelectorButton* pButton) { m_pDragButton = pButton; };
	void SetDragFraction(int dragFraction) { m_iDragFraction = dragFraction; }

	CSelectorButton* GetDragButton() { return m_pDragButton; }
	int GetDragFraction() { return m_iDragFraction; }

	UINT GetSwitchTimer() { return m_hPaneSwitchTimer; }
	void KillSwitchTimer() { KillTimer(IDT_PANESWITCH); m_hPaneSwitchTimer = 0; }
	void SetSwitchTimer() { m_hPaneSwitchTimer = SetTimer(IDT_PANESWITCH, IDT_SWITCHDELAY, NULL); }

	CSelectorButton* GetButtonFromPoint(CPoint point, int& dragFraction);
	
	afx_msg void OnRButtonUp ( UINT nFlags, CPoint point );
	afx_msg void OnLButtonDown (UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);

	// Message that is called when a button is being dragged.
	LRESULT OnButtonDrag(WPARAM wParam, LPARAM lParam);

	// function needed for NavFram.cpp
	void RearrangeIcons();

	BOOL IsTreeVisible();  // Whether or not the tree view is currently showing.

protected:
	int m_scrollDirection;
	CSelectorButton * m_pCurButton;
	CView* m_pCurView; // The entire view (including the HTML pane)
	CRDFCommandMap m_MenuCommandMap;	// Command map for back-end generated right mouse menu commands.
	
    CSelectorDropTarget * m_pDropTarget;
	CNavSelScrollButton* m_scrollUp;
	CNavSelScrollButton* m_scrollDown;
	HBITMAP m_hScrollBmp;
	SIZE m_hScrollBmpSize;
	CRect m_boundingBox;
	int m_xpos;
	int m_ypos;
	int m_scrollPos;
	HBRUSH hBrush;
	HBRUSH hHtBrush;

	CSelectorButton* m_pDragButton; // The button that is currently hovered over in a drag.
	int m_iDragFraction;			// The drag fraction.

	UINT m_hPaneSwitchTimer;

	CPoint m_PointHit;	// The point hit on a mouse down on the selector bar (not on one of the buttons).

	HT_Pane m_Pane; // RDF Pane associated with this NavCenter frame.
	HT_Notification m_Notification; // Notification struct that is tossed around.
	//{{AFX_MSG(CMainFrame)
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnParentNotify( UINT message, LPARAM lParam );
	afx_msg void OnTimer(UINT id);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};



extern "C"  {
//  Netlib API to the stream that will save the file to disk.
NET_StreamClass *ContextSaveStream(int iFormatOut, void *pDataObj, URL_Struct *pUrl, MWContext *pContext);

unsigned int MCFSaveReady(NET_StreamClass *stream);
int MCFSaveWrite(NET_StreamClass *stream, const char *pWriteData, int32 iDataLength);
void MCFSaveComplete(NET_StreamClass *stream);
void MCFSaveAbort(NET_StreamClass *stream, int iStatus);
};

#endif
