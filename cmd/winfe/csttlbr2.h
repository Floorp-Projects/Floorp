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
#ifndef _CSTTLBR2_H
#define _CSTTLBR2_H

#include "stdafx.h"
#include "toolbar2.h"
#include "animbar2.h"

#define CT_HIDETOOLBAR		(WM_USER + 15)
#define CT_DRAGTOOLBAR		(WM_USER + 16)
#define CT_DRAGTOOLBAR_OVER (WM_USER + 17)
#define CT_CUSTOMIZE		(WM_USER + 18)
#define IDC_COLLAPSE        (WM_USER + 19)


typedef	enum  {eLARGE_HTAB, eSMALL_HTAB} HTAB_BITMAP;

class CCustToolbar;

#ifdef XP_WIN16

class CNetscapeControlBar : public CControlBar {

protected:

virtual LRESULT WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam);



};

#endif


// Class:  CToolbarWindow
//
// The window that resides within the Tabbed toolbar.  It holds the toolbar
// passed in by the user of the customizable toolbar

class CToolbarWindow  {

protected:
	CWnd *m_pToolbar;			// the toolbar we're storing
	int m_nNoviceHeight;		// the height when in novice mode
	int m_nAdvancedHeight;		// the height when in advanced mode
	int m_nToolbarStyle;		// is this pictures and text, pictures, or text
	HTAB_BITMAP m_nHTab;		// Type of horizontal tab

public:
	CToolbarWindow(CWnd *pToolbar, int nToolbarStyle, int nNoviceHeight, int nAdvancedHeight,
				   HTAB_BITMAP nHTab);
	~CToolbarWindow();
	CWnd *GetToolbar(void);

	virtual CWnd* GetNSToolbar() { return NULL; } 

	// if bWindowHeight is TRUE then use toolbar's height, if FALSE use passed in height
	virtual int GetHeight(void);
	int GetNoviceHeight(void) { return m_nNoviceHeight; }
	int GetAdvancedHeight(void) { return m_nAdvancedHeight; }

	virtual void SetToolbarStyle(int nToolbarStyle) { m_nToolbarStyle = nToolbarStyle;}
	int GetToolbarStyle(void) { return m_nToolbarStyle; }
	
	HTAB_BITMAP GetHTab(void) { return m_nHTab; }

	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler ){}

};

// Class:  CButtonToolbarWindow
//
// The window that resides within the Tabbed toolbar.  It MUST hold a CNSToolbar2.
class CButtonToolbarWindow: public CToolbarWindow {

public:
	CButtonToolbarWindow(CWnd *pToolbar, int nToolbarStyle, int nNoviceHeight, int nAdvancedHeight,
						 HTAB_BITMAP nHTab);

	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );
	virtual void SetToolbarStyle(int nToolbarStyle);
	virtual int GetHeight(void);
	virtual CWnd* GetNSToolbar() { return GetToolbar(); }


};

// Class:  // Class:  CControlBarToolbarWindow
//
// The window that resides within the Tabbed toolbar.  It does not hold a CNSToolbar2.
class CControlBarToolbarWindow: public CToolbarWindow {

public:
	CControlBarToolbarWindow(CWnd *pToolbar, int nToolbarStyle, int nNoviceHeight, int nAdvancedHeight,
						 HTAB_BITMAP nHTab);

	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );
	virtual int GetHeight(void);
};



class CDragToolbar : public CWnd {

private:
	CToolbarWindow *	m_pToolbar;
	BOOL				m_bIsOpen;
	BOOL				m_bIsShowing;
	CPoint				m_mouseDownPoint;
	BOOL				m_bDragging;
	BOOL				m_bMouseDown;
	UINT				m_nDragTimer;
	BOOL				m_bMouseInTab;
	UINT				m_nTabFocusTimer;
	CAnimationBar2*		m_pAnimation;
	HTAB_BITMAP			m_eHTabType;
	BOOL				m_bEraseBackground;
	CString				m_tabTip;
	int					m_nToolID;
	CNSToolTip2		m_toolTip;
	UINT				m_nToolbarID;
public:
	CDragToolbar();
	~CDragToolbar();
	int Create(CWnd *pParent, CToolbarWindow *pToolbar);

	CWnd *GetToolbar(void);
	int   GetToolbarHeight(void);
	int GetMouseOffsetWithinToolbar(void) { return m_mouseDownPoint.y; }
	void SetMouseOffsetWithinToolbar(int y) { m_mouseDownPoint.y = y; }
	void SetShowing(BOOL bIsShowing) { m_bIsShowing = bIsShowing; }
	BOOL GetShowing(void) { return m_bIsShowing; }
	void SetOpen(BOOL bIsOpen) { m_bIsOpen = bIsOpen; }
	BOOL GetOpen(void) { return m_bIsOpen;}
	void SetTabTip(CString tabTip); 
	CString &GetTabTip (void) { return m_tabTip; }
	void SetToolID(int nToolID) { m_nToolID = nToolID; }
	int  GetToolID(void) { return m_nToolID;	}
	void SetToolbarID(int nToolbarID) {m_nToolbarID = nToolbarID;}
	UINT GetToolbarID(void) { return m_nToolbarID;}
	void SetToolbarStyle(int nToolbarStyle);
	void SetAnimation(CAnimationBar2 *pAnimation);
	HTAB_BITMAP GetHTabType(void) { return m_eHTabType;}
	void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );
	// Generated message map functions
	//{{AFX_MSG(CDragToolbar)
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnPaint(void);
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual afx_msg void OnTimer( UINT  nIDEvent );
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );
	afx_msg void OnSysColorChange();
	//}}AFX_MSG


private:
	void ArrangeToolbar(int nWidth, int nHeight);
	void StopDragging(void);
	void CheckIfMouseInTab(CPoint point);


	DECLARE_MESSAGE_MAP()

};

class CCustToolbarExternalTab {

private:
	CWnd *				m_pOwner;
	HTAB_BITMAP			m_eHTabType;
	CString				m_tabTip;
	int					m_nToolID;
	UINT				m_nTabID;

public:
	CCustToolbarExternalTab(CWnd *pOwner, HTAB_BITMAP eHTabType, UINT nTipID, UINT nTabID);

	CWnd *GetOwner(void);
	HTAB_BITMAP GetHTabType(void);
	CString &GetTabTip (void);
	void SetToolID(int nToolID) { m_nToolID = nToolID; }
	int  GetToolID(void) { return m_nToolID;	}
	UINT GetTabID(void);


};

class CRDFToolbar;

class CCustToolbar : public CControlBar {


private:
	CFrameWnd *			m_pParent;
	CDragToolbar**		m_pToolbarArray;
	CDragToolbar**		m_pHiddenToolbarArray;
	int					m_nNumToolbars;
	int					m_nActiveToolbars;
	CAnimationBar2*		m_pAnimation;			//The Netscape Icon Animation
	int					m_nAnimationPos;
	int					m_nNumOpen;
	int					m_nNumShowing;
	CPoint				m_oldDragPoint;
	HBITMAP				m_pHorizTabArray[4];
	BOOL				m_bEraseBackground;
	CNSToolTip2			m_toolTip;
	BOOL				m_bSaveToolbarInfo;		// Do we save toolbar state
	UINT				m_nTabHaveFocusTimer;
	int					m_nMouseOverTab;
	CPtrArray			m_externalTabArray;
	BOOL				m_bBottomBorder;

	enum HORIZTAB {LARGE_FIRST, LARGE_OTHER, SMALL_FIRST, SMALL_OTHER};

public:

	//Construction/destruction
	CCustToolbar(int nNumToolbars);
	~CCustToolbar(); 

		//Creation
	int Create(CFrameWnd* pParent, BOOL bHasAnimation);

	void AddNewWindow(UINT nToolbarID, CToolbarWindow* pWindow,  int nPosition, int nNoviceHeight, int nAdvancedHeight,
					  UINT nTabBitmapIndex, CString tabTip, BOOL bIsNoviceMode, BOOL bIsOpen,
					  BOOL bIsAnimation);
	// Call this function when you are finished adding the toolbars that go in the
	// customizable toolbar. 
	void FinishedAddingNewWindows(void){}

	//Controlling the animated icon
    void StopAnimation();
	void StartAnimation();
	void SetToolbarStyle(int nToolbarStyle);

	BOOL IsWindowShowing(CWnd *pToolbar);
	BOOL IsWindowShowing(UINT nToolbarID);

	BOOL IsWindowIconized(CWnd *pToolbar);
	int	 GetWindowPosition(CWnd *pToolbar);

	void ShowToolbar(CWnd *pToolbar, BOOL bShow);
	void ShowToolbar(UINT nToolbarID, BOOL bShow);

	void RenameToolbar(UINT nOldID, UINT nNewID, UINT nNewToolTipID);

	CWnd *GetToolbar(UINT nToolbarID);

		//Positioning/Resizing
	CSize CalcDynamicLayout(int nLength, DWORD dwMode );

	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );

	void Customize(CRDFToolbar *pRDFToolbar = NULL, int nSelectedButton = 0);
	BOOL GetSaveToolbarInfo(void);
	void SetSaveToolbarInfo(BOOL bSaveToolbarInfo);
	void SetNewParentFrame(CFrameWnd *pParent);

	// Adding an external tab will cause the customizable toolbar to display
	// a tab of eHTabType in iconized form.  If that tab is clicked, the tab
	// will be removed and a message will be sent to pOwner that the hidden
	// window should now be shown.
	void AddExternalTab(CWnd *pOwner, HTAB_BITMAP eHTabType, UINT nTipID, UINT nTabID);
	// Removing this tab will cause it to no longer be drawn and mouse clicks will no longer
	// be sent to pOwner.
	void RemoveExternalTab(UINT nTabID);

	void SetBottomBorder(BOOL bBottomBorder);
	// Generated message map functions
	//{{AFX_MSG(CCustToolbar)
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnPaint(void);
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnHideToolbar(WPARAM, LPARAM); 
	afx_msg LRESULT OnDragToolbar(WPARAM, LPARAM);
	afx_msg LRESULT OnDragToolbarOver(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT CCustToolbar::OnCustomize(WPARAM wParam, LPARAM lParam);
#ifndef WIN32
	afx_msg LRESULT OnSizeParent(WPARAM wParam, LPARAM lParam);
#endif
	virtual afx_msg void OnTimer( UINT  nIDEvent );
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );

	//}}AFX_MSG

protected:
//	virtual BOOL OnNotify( WPARAM wParam, LPARAM lParam, LRESULT* pResult );
 

private:
	int  CheckOpenButtons(CPoint point);
	int	 CheckClosedButtons(CPoint point);
	BOOL PointInClosedTab(CPoint point, HTAB_BITMAP tabType, int nNumClosedButtons, int nStartX,
						  int nBottom);
	void DrawSeparator(HDC hDC, int nStartX, int nEndX, int nStartY, BOOL bToolbarSeparator = TRUE);
	void SwitchChildren(CDragToolbar *pOriginal, CDragToolbar *pSwitch, int dir, int yPoint);
	int  FindIndex(CDragToolbar *pToolbar);
	CDragToolbar *FindToolbarFromPoint(CPoint point, CDragToolbar *pIgnore);
	int	 FindFirstShowingToolbar(int nIndex);
	HBITMAP GetClosedButtonBitmap(HTAB_BITMAP tabType, int nNumClosedButtons);
	int  GetNextClosedButtonX(HTAB_BITMAP tabType, int nNumClosedButtons, int nClosedStartX);
	void GetClosedButtonRegion(HTAB_BITMAP tabType, int nNumClosedButtons, CRgn &rgn);
	HBITMAP CreateHorizTab(UINT nID);
	int FindDragToolbarFromWindow(CWnd *pWindow, CDragToolbar **pToolbarArray);
	int FindDragToolbarFromID(UINT nToolbarID, CDragToolbar **pToolbarArray);
	void ShowDragToolbar(int nIndex, BOOL bShow);
	void OpenDragToolbar(int nIndex);
	void OpenExternalTab(int nIndex);
	void CheckAnimationChangedToolbar(CDragToolbar *pToolbar, int nIndex, BOOL bOpen);
	void ChangeToolTips(int nHeight);
	void FindToolRect(CRect & toolRect, HTAB_BITMAP eTabType, int nStartX, int nStartY, int nButtonNum);
	int  FindFirstAvailablePosition(void);
	void DrawClosedTab(HDC hCompatibleDC, HDC hDestDC, HTAB_BITMAP tabType, int nNumClosedButtons,
		  			   BOOL bMouseOver, int nStartX, int nBottom);

	DECLARE_MESSAGE_MAP()

};


#endif
