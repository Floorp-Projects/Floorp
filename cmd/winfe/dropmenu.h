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

// dropmenu.h
// Provides the interface for a popup menu in which you can drag and drop
// Created by Scott Putterman on 9/10/96

#ifndef _DROPMENU_H
#define _DROPMENU_H

#include "stdafx.h"
#include "tlbutton.h"

#define DM_DROPOCCURRED		WM_USER + 50
#define DM_CLOSEALLMENUS	WM_USER + 51
#define DM_MENUCLOSED		WM_USER + 52
#define DM_FILLINMENU		WM_USER + 53
#define DM_DRAGGINGOCCURRED WM_USER + 54

typedef enum {eABOVE=1, eON=2, eBELOW=3, eNONE} MenuSelectionType;

class CDropMenuButton : public CStationaryToolbarButton {

public:
	CDropMenuButton();

protected:
	BOOL m_bButtonPressed;    // have had a mouse down
	BOOL m_bMenuShowing;	  // is the menu showing
	BOOL m_bFirstTime;		  // is this the first time we've moved after a mouse up

	// Generated message map functions
	//{{AFX_MSG(CDropMenuButton)
	virtual afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	virtual afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual afx_msg LRESULT OnMenuClosed(WPARAM, LPARAM); 

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

class CDropMenu;

class CDropMenuItem  {

private:
	CString		m_szText;
	UINT		m_nCommandID;
	void*		m_pCustomIcon;					// An icon can be drawn instead of using bitmaps
	IconType	m_nIconType;					// The icon type.
	HBITMAP     m_hUnselectedBitmap;		// Just like the toolbar buttons (DWH)
	HBITMAP     m_hSelectedBitmap;
	CDropMenu * m_pSubMenu;
	BOOL		m_bIsSeparator;
	CSize		m_unselectedBitmapSize;
	CSize		m_selectedBitmapSize;
	CSize		m_textSize;
	CSize		m_totalSize;
	CRect		m_menuItemRect;
	BOOL		m_bShowFeedback;			//Does this show selection information
	BOOL		m_bIsEmpty;					//Is the submenu empty
	int			m_nColumn;					//The column in the menu this item is in
	void *		m_pUserData;
public:
	CDropMenuItem(UINT nFlags, CString szText, UINT nCommandID,
				  HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap, BOOL bShowFeedback,
				  void *pUserData = NULL);
	CDropMenuItem(UINT nFlags, CString szText, UINT nCommandID, CDropMenu *pSubMenu, BOOL bIsEmpty,
				  HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap, BOOL bShowFeedback,
				  void *pUserData = NULL);

	~CDropMenuItem();

	CDropMenu *GetSubMenu(void)    { return m_pSubMenu; }
	CString    GetText(void)       { return m_szText; }
	UINT       GetCommand(void)    { return m_nCommandID; }
	HBITMAP	   GetUnselectedBitmap(void)   { return m_hUnselectedBitmap; }
	HBITMAP    GetSelectedBitmap(void)     { return m_hSelectedBitmap; }
	BOOL	   IsSeparator(void)   { return m_bIsSeparator; }
	BOOL	   IsSubMenu(void)	   { return m_pSubMenu != NULL && !m_bIsSeparator; }
	BOOL	   IsMenuItem(void)	   { return m_pSubMenu == NULL && !m_bIsSeparator; }
	BOOL	   IsEmpty(void)	   { return m_bIsEmpty; }
	CSize	   GetUnselectedBitmapSize(void) { return m_unselectedBitmapSize; }
	void	   SetUnselectedBitmapSize(CSize bitmapSize) { m_unselectedBitmapSize = bitmapSize; }
	CSize	   GetSelectedBitmapSize(void) { return m_selectedBitmapSize; }
	void	   SetSelectedBitmapSize(CSize bitmapSize) { m_selectedBitmapSize = bitmapSize; }
	CSize	   GetTextSize(void)   { return m_textSize; }
	void	   SetTextSize(CSize textSize) { m_textSize = textSize; }
	CSize	   GetTotalSize(void)  { return m_totalSize; }
	void	   SetTotalSize(CSize totalSize) { m_totalSize = totalSize; }
	void	   GetMenuItemRect(LPRECT rect);
	void	   SetMenuItemRect(LPRECT rect);
	BOOL	   GetShowFeedback(void) { return m_bShowFeedback;}
	void	   SetColumn(int nColumn) { m_nColumn = nColumn; }
	int		   GetColumn(void) { return m_nColumn; }
	void *	   GetUserData(void) { return m_pUserData; }
	void	   SetIcon(void* pIcon, IconType iconType) { m_pCustomIcon = pIcon; m_nIconType = iconType; }
	HICON	   GetLocalFileIcon() { return (HICON)m_pCustomIcon; }
	NSNavCenterImage* GetCustomIcon() { return (NSNavCenterImage*)m_pCustomIcon; }
	IconType GetIconType() { return m_nIconType; }
};

#define DT_DROPOCCURRED WM_USER + 60
#define DT_DRAGGINGOCCURRED WM_USER + 61
#define PT_REFRESH WM_USER + 62

class CDropMenuDropTarget : public COleDropTarget
{
protected:
	CWnd *m_pOwner;

public:
	CDropMenuDropTarget(CWnd *pOwner);
	 
protected:
	virtual DROPEFFECT OnDragEnter(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point );
	virtual BOOL OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point);
	
};

class CDropMenuColumn {

private:
	int m_nFirstItem;
	int m_nLastItem;
	int m_nWidth;
	int m_nHeight;
	int m_nWidestImage;
	BOOL m_bHasSubMenu;

public:
	CDropMenuColumn(int nFirstItem, int nLastItem, int nWidth, int nHeight);

	CDropMenuColumn() {;}
	void SetFirstItem(int nFirstItem) { m_nFirstItem = nFirstItem; }
	int  GetFirstItem(void) { return m_nFirstItem; }
	void SetLastItem(int nLastItem) { m_nLastItem = nLastItem; }
	int  GetLastItem(void) { return m_nLastItem; }
	void SetWidth(int nWidth) { m_nWidth = nWidth; }
	int  GetWidth(void) { return m_nWidth; }
	void SetHeight(int nHeight) { m_nHeight = nHeight; }
	int  GetHeight(void) { return m_nHeight; }
	void SetWidestImage(int nWidestImage) { m_nWidestImage = nWidestImage; }
	int  GetWidestImage(void) { return m_nWidestImage; }
	void SetHasSubMenu(BOOL bHasSubMenu) {m_bHasSubMenu = bHasSubMenu; }
	BOOL GetHasSubMenu(void) { return m_bHasSubMenu; }

};

class CDropMenuDragData
{
public:
	CPoint m_PointHit;				// Added by Dave H.  In our RDF world, we need to give drag feedback
	MenuSelectionType eSelType;		// in the drop menus, since some drops won't be allowed.
	COleDataObject* m_pDataSource;
	UINT m_nCommand;
	DROPEFFECT m_DropEffect;

public:
	CDropMenuDragData(COleDataObject* pDataSource, const CPoint& point)
		:m_pDataSource(pDataSource), m_PointHit(point) { m_DropEffect = DROPEFFECT_NONE; };
};

class CDropMenu: public CWnd {

protected:
	CPtrArray m_pMenuItemArray;
	CPtrArray m_pColumnArray;

private:
	int m_WidestImage;
	int m_WidestText;

	int m_nSelectedItem;
	MenuSelectionType m_eSelectedItemSelType; //is selection above, on, below or on?

	CWnd *m_pParent;					//The real parent of this menu (could me some other window)
	CDropMenu *m_pMenuParent;			//The menu parent of this menu (if its real parent is
										//another type of window, this is NULL

	CDropMenu *m_pShowingSubmenu;		//The current submenu that is showing

	CDropMenuDropTarget *m_pDropTarget;

	HBITMAP m_hSubmenuUnselectedBitmap;
	HBITMAP m_hSubmenuSelectedBitmap;

	BOOL m_bShowing;					//even though it may be showing on screen, is it a submenu
										//that has been killed and is just waiting the specified delay time
	BOOL m_bAboutToShow;				//A submenu that is about to be opened but is not yet on the screen.
	BOOL m_bHasSubMenu;					//does this menu have at least one submenu
	BOOL m_bNotDestroyed;
	BOOL m_bDropOccurring;				//Have we just been dropped on

	BOOL m_bMouseUsed;					//Is the mouse or keyboard being used
	BOOL m_bDragging;					//Is this menu being displayed for drag and drop
	CDropMenuItem *m_pUnselectedItem;	//Which menu has most recently been unselected
	UINT m_nUnselectTimer;				//the unselected menu timer

	CDropMenuItem *m_pSelectedItem;		//Which menu has most recently been selected
	UINT m_nSelectTimer;				//the selected menu timer

	UINT m_nHaveFocusTimer;
	BOOL m_bRight;						//is this window to the right of its parent
	BOOL m_bIsOpen;						//Are we currently open
	BOOL m_bShowFeedback;				//Does this draw feedback above and below menu item

	HHOOK m_mouseHook;
	CDropMenuItem *m_pOverflowMenuItem;	//The overflow menu item;
	int	 m_nLastVisibleColumn;			//The last column that will show up on the screen before it overflows
	BOOL m_bFirstPaint;
	CDropMenuItem *m_pParentMenuItem;	//The menu item this submenu belongs to
	void * m_pUserData;					// User data attached to the menu item
public:

	CDropMenu();
	~CDropMenu();

	UINT GetMenuItemCount();

	void AppendMenu(UINT nFlags, UINT nIDNewItem, CString szText, BOOL bShowFeedback, HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap = NULL, 
					void* pCustomIcon = NULL, IconType iconType = BUILTIN_BITMAP, void *pUserData = NULL);
	void AppendMenu(UINT nFlags, UINT nIDNewItem, CDropMenu *pSubMenu, BOOL bIsEmpty,
					CString szText, BOOL bShowFeedback, HBITMAP pUnselectedBitmap, HBITMAP hSelectedBitmap = NULL,
					void* pCustomIcon = NULL, IconType iconType = BUILTIN_BITMAP, void *pUserData = NULL);
	
	void DeleteMenu(UINT nPosition, UINT nFlags);

	void CreateOverflowMenuItem(UINT nCommand, CString szText, HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap = NULL);

	BOOL IsOpen(void)		   { return m_bIsOpen; }
	BOOL IsDragging(void)  { return m_bDragging;}

	int GetSelected(void) { return m_nSelectedItem; }
	int Create(CWnd *pParent, int x, int y);

	void TrackDropMenu(CWnd* pParent, int x, int y,  BOOL bDragging = FALSE, CDropMenuDropTarget *pDropTarget = NULL, BOOL bShowFeedback = FALSE, int oppX = -1, int oppY = -1);
	BOOL PointInMenus(POINT pt);
	BOOL IsShowingMenu(HWND hwnd);
	void HandleKeystroke(WPARAM key);

	virtual BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	void Deactivate(void); 
	void SetUserData(void *pUserData) { m_pUserData = pUserData; }
	void *GetUserData(void) { return m_pUserData; }
	void PostNcDestroy(void );
	void DestroyDropMenu(void);

	static CDropMenu* GetLastDropMenu(); // Handle to the gLastMenu var.
	static void SetLastDropMenu(CDropMenu* menu); // Sets the gLastMenu var.

protected:
	void DrawSeparator(HDC hDC, int x, int y, int nWidth);
	void DrawVerticalSeparator(HDC hDC, int x, int y, int nHeight);
	void DrawBorders(HDC hDC, CRect rcClient);
	void	SetDropMenuTarget(CDropMenuDropTarget *pDropTarget);
	virtual CSize GetMenuSize(void);
	virtual CSize CalculateColumn(CDropMenuColumn *pColumn, int nStartX);
	int AddOverflowItem(int nColumn, int nStartX);
	virtual void CalculateItemDimensions(void);
	virtual void CalculateOneItemsDimensions(CDropMenuItem* item);
	virtual CSize MeasureBitmap(HBITMAP hBitmap);
	virtual CSize MeasureText(CString text);
	virtual CSize MeasureItem(CDropMenuItem *pItem);
	virtual void  DrawItem(CDropMenuColumn *pColumn, CDropMenuItem *pItem, CRect rc, HDC hDC, BOOL bIsSelected, MenuSelectionType eSelType);
	virtual CSize DrawImage(CDC * pDC, const CRect & rect, CDropMenuColumn *pColumn, CDropMenuItem * pItem, BOOL bIsSelected,
							MenuSelectionType eSelType);
	virtual CDropMenuDropTarget *GetDropMenuDropTarget(CWnd *pOwner);
	void ChangeSelection(CPoint point);
	int  FindSelection(CPoint point, MenuSelectionType &eSelType);
	int  FindSelectionInColumn(CDropMenuColumn *pColumn, CPoint point, MenuSelectionType &eSelType);
	MenuSelectionType FindSelectionType(CDropMenuItem *pItem, int y, int nHeight);
	void SelectMenuItem(int nItemIndex, BOOL bIsSelected, BOOL bHandleSubmenus, MenuSelectionType eSelType);
	BOOL NoSubmenusShowing(void);
	CDropMenu *GetMenuParent(void);
	BOOL IsDescendant(CWnd *pDescendant);
	BOOL IsAncestor(CWnd *pAncestor);
	void DropMenuTrackDropMenu(CDropMenu *pParent, int x, int y, BOOL bDragging, CDropMenuDropTarget *pDropTarget,
							   BOOL bShowFeedback, int oppX =- 1, int oppY = -1);
	virtual BOOL DestroyWindow(void);
	BOOL ShowWindow( int nCmdShow );
	int FindCommand(UINT nCommand);
	CPoint FindStartPoint(int x, int y, CSize size, int oppX = -1, int oppY = -1);
	void HandleUpArrow(void);
	void HandleDownArrow(void);
	void HandleLeftArrow(void);
	void HandleRightArrow(void);
	void HandleReturnKey(void);
	void ShowSubmenu(CDropMenuItem * pItem);
	int	FindMenuItemPosition(CDropMenuItem* pMenuItem);
	void InvalidateMenuItemRect(CDropMenuItem *pMenuItem);
	void KeepMenuAroundIfClosing(void);
	void SetSubmenuBitmaps(HBITMAP hSelectedBitmap, HBITMAP hUnselectedBitmap);
	CWnd *GetTopLevelParent(void);

		// Generated message map functions
	//{{AFX_MSG(CDropMenu)
	afx_msg void OnPaint();
	virtual afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg UINT OnNcHitTest( CPoint point );
	afx_msg LRESULT OnDropTargetDropOccurred(WPARAM, LPARAM); 
	afx_msg LRESULT OnDropTargetDraggingOccurred(WPARAM, LPARAM);
	afx_msg LRESULT OnCloseAllMenus(WPARAM, LPARAM); 
	afx_msg void OnTimer( UINT  nIDEvent );
	afx_msg void OnActivate( UINT nState, CWnd* pWndOther, BOOL bMinimized );
	afx_msg void OnDestroy( );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};



#endif
