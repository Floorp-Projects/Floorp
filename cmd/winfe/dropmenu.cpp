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

// dropmenu.cpp
// Implements a popup menu in which you can drag and drop

#include "stdafx.h"
#include "dropmenu.h"

#define nIMG_SPACE 4
#define MAX_DROPMENU_ITEM_LENGTH 45


//////////////////////////////////////////////////////////////////////////
//					Class CDropMenuButton
//////////////////////////////////////////////////////////////////////////
CDropMenuButton::CDropMenuButton(void)
{

	m_bButtonPressed = FALSE;
	m_bMenuShowing = FALSE;
	m_bFirstTime = FALSE;
}
//////////////////////////////////////////////////////////////////////////
//					Messages for CDropMenuButton
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CDropMenuButton, CStationaryToolbarButton)
	//{{AFX_MSG_MAP(CWnd)
 	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(DM_MENUCLOSED, OnMenuClosed)

	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

void CDropMenuButton::OnLButtonDown(UINT nFlags, CPoint point)
{
	CStationaryToolbarButton::OnLButtonDown(nFlags, point);

	m_bButtonPressed = TRUE;
}

void CDropMenuButton::OnMouseMove(UINT nFlags, CPoint point)
{
	if(m_bMenuShowing) 
	{
		if(m_bFirstTime)
		{
			ReleaseCapture();
			m_eState = eBUTTON_DOWN;
			RedrawWindow();
			m_bFirstTime = FALSE;
		}
	}
	else
	{
		CStationaryToolbarButton::OnMouseMove(nFlags, point);
	}
}

void CDropMenuButton::OnLButtonUp(UINT nFlags, CPoint point)
{
 	if(m_bButtonPressed && m_bEnabled && m_bHaveFocus)
 	{
 		OnAction();
 		m_bMenuShowing = TRUE;
 		m_bFirstTime = TRUE;
 	}
 	else
 	{
 		CStationaryToolbarButton::OnLButtonUp(nFlags, point);
 		m_bMenuShowing = FALSE;
 	}
	m_bButtonPressed = FALSE;
}

LRESULT CDropMenuButton::OnMenuClosed(WPARAM wParam, LPARAM lParam)
{

	m_bMenuShowing = FALSE;
	m_eState = eNORMAL;
	RedrawWindow();

	return 1;
}

//////////////////////////////////////////////////////////////////////////
//					Class CDropMenuItem
//////////////////////////////////////////////////////////////////////////

CDropMenuItem::CDropMenuItem(UINT nFlags, CString szText, UINT nCommandID,
							 HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap, 
							 BOOL bShowFeedback, void *pUserData)
{
	m_nIconType = BUILTIN_BITMAP;

	if(nFlags & MF_SEPARATOR)
	{
		m_bIsSeparator = TRUE;
	}
	else if(nFlags == MF_STRING)
	{
		m_bIsSeparator = FALSE;
		char *pCutText = fe_MiddleCutString(szText.GetBuffer(szText.GetLength() + 1), MAX_DROPMENU_ITEM_LENGTH);
		m_szText = pCutText;
		szText.ReleaseBuffer();
		free(pCutText);
		m_hUnselectedBitmap = hUnselectedBitmap;

		m_hSelectedBitmap = (hSelectedBitmap == NULL) ? hUnselectedBitmap : hSelectedBitmap;

		m_nCommandID = nCommandID;
		m_pSubMenu = NULL;
	}
	m_bIsEmpty = TRUE;
	m_unselectedBitmapSize.cx = m_unselectedBitmapSize.cy = 0;
	m_selectedBitmapSize.cx = m_selectedBitmapSize.cy = 0;
	m_textSize.cx = m_textSize.cy = 0;
	m_totalSize.cx = m_totalSize.cy = 0;
	m_menuItemRect.SetRectEmpty();
	m_bShowFeedback = bShowFeedback;
	m_pUserData = pUserData;

}	

CDropMenuItem::CDropMenuItem(UINT nFlags, CString szText, UINT nCommandID, CDropMenu *pSubMenu, BOOL bIsEmpty,
							 HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap, BOOL bShowFeedback, void *pUserData)
{
	if(nFlags & MF_POPUP)
	{
		m_bIsSeparator = FALSE;
		char *pCutText = fe_MiddleCutString(szText.GetBuffer(szText.GetLength() + 1), MAX_DROPMENU_ITEM_LENGTH);
		m_szText = pCutText;
		szText.ReleaseBuffer();

		free(pCutText);

		m_hUnselectedBitmap = hUnselectedBitmap;
		m_hSelectedBitmap = (hSelectedBitmap == NULL) ? hUnselectedBitmap : hSelectedBitmap;

		m_nCommandID = nCommandID;
		m_pSubMenu = pSubMenu;
	}
	m_bIsEmpty = bIsEmpty;
	m_unselectedBitmapSize.cx = m_unselectedBitmapSize.cy = 0;
	m_selectedBitmapSize.cx = m_selectedBitmapSize.cy = 0;
	m_textSize.cx = m_textSize.cy = 0;
	m_totalSize.cx = m_totalSize.cy = 0;
	m_menuItemRect.SetRectEmpty();
	m_bShowFeedback = bShowFeedback;
	m_pUserData = pUserData;
}

CDropMenuItem::~CDropMenuItem()
{

	if(IsSubMenu())
	{
		CDropMenu *pSubMenu = GetSubMenu();
		pSubMenu->DestroyDropMenu();
		delete pSubMenu;
	}
}
void  CDropMenuItem::GetMenuItemRect(LPRECT rect)
{
	rect->left = m_menuItemRect.left;
	rect->right = m_menuItemRect.right;
	rect->top = m_menuItemRect.top;
	rect->bottom = m_menuItemRect.bottom;

}

void CDropMenuItem::SetMenuItemRect(LPRECT rect)
{
	m_menuItemRect.left = rect->left;
	m_menuItemRect.right = rect->right;
	m_menuItemRect.top = rect->top;
	m_menuItemRect.bottom = rect->bottom;
}


///////////////////////////////////////////////////////////////////////////
//							Class CDropMenuDropTarget
///////////////////////////////////////////////////////////////////////////

CDropMenuDropTarget::CDropMenuDropTarget(CWnd *pOwner)
{
	m_pOwner = pOwner;

}


DROPEFFECT CDropMenuDropTarget::OnDragEnter(CWnd * pWnd,	COleDataObject * pDataObject,
											   DWORD dwKeyState, CPoint point)
{
	return OnDragOver(pWnd, pDataObject, dwKeyState, point);
	
}

DROPEFFECT CDropMenuDropTarget::OnDragOver(CWnd * pWnd, COleDataObject * pDataObject,
											  DWORD dwKeyState, CPoint point )
{
	CDropMenuDragData dragData(pDataObject, point);
	m_pOwner->SendMessage(DT_DRAGGINGOCCURRED, (WPARAM) 0, (LPARAM)(&dragData));
	
	return dragData.m_DropEffect;
}

BOOL CDropMenuDropTarget::OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point)
{
	CDropMenuDragData* dragData = new CDropMenuDragData(pDataObject, point);
	m_pOwner->SendMessage(DT_DROPOCCURRED, (WPARAM) 0, (LPARAM)(dragData));
	
	return TRUE;
}

// End CDropMenuDropTarget implementation

// CDropMenuColumn
CDropMenuColumn::CDropMenuColumn(int nFirstItem, int nLastItem, int nWidth, int nHeight)
{
	m_nFirstItem = nFirstItem;
	m_nLastItem = nLastItem;
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_bHasSubMenu = FALSE;
}


#define NOSELECTION -1
#define MENU_BORDER_SIZE 3

#define SUBMENU_WIDTH 9
#define SUBMENU_HEIGHT 11

#define SUBMENU_LEFT_MARGIN 3
#define SUBMENU_RIGHT_MARGIN 7

#define IDT_SUBMENU_OFF	16384
#define SUBMENU_DELAY_OFF_MS 400

#define IDT_SUBMENU_ON 16385
#define SUBMENU_DELAY_ON_MS 400

#define IDT_HAVE_FOCUS 16386
#define HAVE_FOCUS_DELAY 400

#define IDT_UNHOOK 16387
#define UNHOOK_DELAY 100

#define SEPARATOR_SPACE 2
#define SEPARATOR_HEIGHT 2
#define SEPARATOR_WIDTH 2
#define SPACE_BETWEEN_ITEMS 2

static HHOOK currentMouseHook;
static HHOOK currentKeyboardHook;
static CDropMenu * currentMenu = NULL;		//This refers to a submenu of the current dropmenu
static CDropMenu * gLastMenu = NULL;		//This refers to the most recent complete dropmenu
static LONG oldFrameProc;
static UINT gUnhookTimer = 0;

CDropMenu* CDropMenu::GetLastDropMenu()
{
	return gLastMenu;
}

void CDropMenu::SetLastDropMenu(CDropMenu* menu)
{
	gLastMenu = menu;
}

CDropMenu::CDropMenu()
{

	m_pMenuItemArray.SetSize(0, 10);

	m_WidestImage = 0;
	m_WidestText = 0;

	m_nSelectedItem = -1;
	m_eSelectedItemSelType = eNONE;
	m_bShowing = FALSE;
	m_pDropTarget = NULL;


	m_pUnselectedItem = NULL;
	m_pSelectedItem = NULL;

	m_nSelectTimer = 0;
	m_nUnselectTimer = 0;

	m_pMenuParent = NULL;
	m_pShowingSubmenu = NULL;

	m_bDropOccurring = FALSE;
	m_bDragging = FALSE;
	m_bRight = TRUE;
	m_bMouseUsed = TRUE;
	m_bIsOpen = FALSE;
	m_pOverflowMenuItem = NULL;
	m_bAboutToShow = FALSE;
	m_pParentMenuItem = NULL;
	m_hSubmenuUnselectedBitmap = NULL;
	m_hSubmenuSelectedBitmap = NULL;
	m_pUserData = NULL;
}

CDropMenu::~CDropMenu()
{
    //  Clear out any dangling pointers.
    if(currentMenu == this) {
        currentMenu = NULL;
    }
    if(gLastMenu == this)   {
        gLastMenu = NULL;
    }

	int nCount = m_pMenuItemArray.GetSize();

	for(int i = 0; i < nCount; i++)
	{
		CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[i];
		if(pItem != m_pOverflowMenuItem)
			delete pItem;
	}

	int nColumnCount = m_pColumnArray.GetSize();

	for(int j = 0 ; j < nColumnCount; j++)
		delete (CDropMenuColumn*)m_pColumnArray[j];

	if(m_pDropTarget)
		delete m_pDropTarget;

	if(m_pOverflowMenuItem != NULL)
		delete m_pOverflowMenuItem;

	m_pMenuItemArray.RemoveAll();

	if(m_pMenuParent == NULL)
	{
		if(m_hSubmenuSelectedBitmap != NULL)
		{
			::DeleteObject(m_hSubmenuSelectedBitmap);
			m_hSubmenuSelectedBitmap = NULL;
		}

		if(m_hSubmenuUnselectedBitmap != NULL)
		{
			::DeleteObject(m_hSubmenuUnselectedBitmap);
			m_hSubmenuUnselectedBitmap = NULL;
		}
	}

	if(m_pMenuParent != NULL && m_pMenuParent->m_pShowingSubmenu == this)
		m_pMenuParent->m_pShowingSubmenu = NULL;

	m_pMenuParent = NULL;
	m_bShowing = FALSE;


}
 
UINT CDropMenu::GetMenuItemCount(void)
{
	return m_pMenuItemArray.GetSize();
}

void CDropMenu::AppendMenu(UINT nFlags, UINT nIDNewItem, CString szText, BOOL bShowFeedback,
						   HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap, 
						   void* icon, IconType iconType, void *pUserData)
{

	CDropMenuItem *pMenuItem = new CDropMenuItem(nFlags, szText, nIDNewItem,
												 hUnselectedBitmap, hSelectedBitmap, bShowFeedback, pUserData); 
	
	pMenuItem->SetIcon(icon, iconType);
	m_pMenuItemArray.Add(pMenuItem);
}

void CDropMenu::AppendMenu(UINT nFlags, UINT nIDNewItem, CDropMenu *pSubMenu, BOOL bIsEmpty, CString szText,
						   BOOL bShowFeedback, HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap, 
						   void* icon, IconType iconType, void *pUserData)
{
	CDropMenuItem *pMenuItem = new CDropMenuItem(nFlags, szText, nIDNewItem, pSubMenu, bIsEmpty,
												 hUnselectedBitmap, hSelectedBitmap, bShowFeedback, pUserData); 
	pMenuItem->SetIcon(icon, iconType);
	m_pMenuItemArray.Add(pMenuItem);
}

void CDropMenu::DeleteMenu(UINT nPosition, UINT nFlags)
{
	CDropMenuItem *pItem;

	if(nFlags == MF_BYPOSITION)
	{
		UINT nCount = m_pMenuItemArray.GetSize();

		if(nPosition >= 0 && nPosition < nCount)
		{
			pItem = (CDropMenuItem *)m_pMenuItemArray[nPosition];
			m_pMenuItemArray.RemoveAt(nPosition, 1);
		}
	}
	else if(nFlags == MF_BYCOMMAND)
	{
		int nCommandPosition = FindCommand(nPosition);
		if(nCommandPosition != -1)
		{
			pItem = (CDropMenuItem *)m_pMenuItemArray[nCommandPosition];
			m_pMenuItemArray.RemoveAt(nCommandPosition, 1);
		}
	}

	if(pItem != m_pOverflowMenuItem)
		delete pItem;

}

void CDropMenu::CreateOverflowMenuItem(UINT nCommand, CString szText, HBITMAP hUnselectedBitmap, HBITMAP hSelectedBitmap)
{

	m_pOverflowMenuItem = new CDropMenuItem(MF_STRING, szText, nCommand, hUnselectedBitmap,
											hSelectedBitmap, FALSE); 

}

LRESULT CALLBACK EXPORT HandleMouseEvents(int nCode, WPARAM wParam, LPARAM lParam) 
{
	if(nCode < 0)
	    return CallNextHookEx(currentMouseHook, nCode, wParam, lParam); 

	MOUSEHOOKSTRUCT *mouseHook = (MOUSEHOOKSTRUCT*)lParam;

	if(currentMenu != NULL)
	{
		//only want to deactivate if point is not in our menu or in our parent or children
		if(nCode >= 0 && (wParam == WM_NCLBUTTONDOWN || wParam == WM_LBUTTONDOWN ||
						  ((wParam == WM_LBUTTONUP || wParam == WM_NCLBUTTONUP) && currentMenu->IsDragging())))
		{
			
			if(currentMenu != NULL && !currentMenu->PointInMenus(mouseHook->pt))
			{
				currentMenu->Deactivate();
			}
		}
	}
    return CallNextHookEx(currentMouseHook, nCode, wParam, lParam); 

}

LRESULT CALLBACK EXPORT HandleKeyboardEvents(int nCode, WPARAM wParam, LPARAM lParam)
{
	
	if(nCode < 0)
	{
		return CallNextHookEx(currentKeyboardHook, nCode, wParam, lParam);
	}

	//only want to handle on a keydown
	if(!(HIWORD(lParam) & KF_UP) && currentMenu != NULL)
	{
		currentMenu->HandleKeystroke(wParam);
	}

	//we don't want to pass the keyboard event onwards
	return 1;
}

LRESULT CALLBACK EXPORT FrameProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(uMsg == WM_ACTIVATE)
	{
		if(LOWORD(wParam) == WA_INACTIVE && !currentMenu->IsShowingMenu((HWND)lParam))
		{
			currentMenu->Deactivate();
		}

	}

	return CallWindowProc((WNDPROC)oldFrameProc, hWnd, uMsg, wParam, lParam);	
}

// oppX and oppY refer to the x and y coordinates if the menu has to open in the opposite direction.  Default is to not use them
void CDropMenu::TrackDropMenu(CWnd *pParent, int x, int y, BOOL bDragging, CDropMenuDropTarget *pDropTarget, BOOL bShowFeedback,  int oppX, int oppY)
{
	if(!m_bShowing)
	{
		m_bShowFeedback = bShowFeedback;
		m_bDragging = bDragging;
		m_bHasSubMenu = FALSE;
		m_pParent = pParent;
		m_bFirstPaint =TRUE;
		if(m_pMenuParent != NULL)
		{
			m_bRight = m_pMenuParent->m_bRight;
		}

		//	m_bNotDestroyed = TRUE;
		if(GetMenuItemCount() == 0)
			GetTopLevelParent()->SendMessage(DM_FILLINMENU, 0, (LPARAM)this);



		m_bShowing = TRUE;
		m_bAboutToShow = FALSE;
		m_bIsOpen = TRUE;

		if(m_hSubmenuUnselectedBitmap == NULL)
			m_hSubmenuUnselectedBitmap = ::LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(IDB_SUBMENU1));

		if(m_hSubmenuSelectedBitmap == NULL)
			m_hSubmenuSelectedBitmap = ::LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(IDB_SUBMENU2));

		if(!::IsWindow(m_hWnd))
		{
			CWnd::CreateEx(0, AfxRegisterWndClass( CS_SAVEBITS ,
					theApp.LoadStandardCursor(IDC_ARROW), 
					(HBRUSH)(COLOR_MENU + 1), NULL),"test", WS_POPUP , 0, 0, 0,0, pParent->m_hWnd, NULL, NULL);

			SetDropMenuTarget(pDropTarget);
		}
		else delete pDropTarget;

		CSize size = GetMenuSize();

		CPoint startPoint = FindStartPoint(x, y, size, oppX, oppY);

		if(m_pMenuParent == NULL)
		{
			if(gLastMenu !=NULL)
			{
				//we only want one drop menu open at a time, so close the last one if one is
				//still open. This will probably only happen on drag and drop.
				gLastMenu->Deactivate();
			}

			oldFrameProc =  SetWindowLong(GetParentFrame()->m_hWnd, GWL_WNDPROC, (LONG)FrameProc);
#ifdef _WIN32
			currentMouseHook = SetWindowsHookEx(WH_MOUSE, HandleMouseEvents, AfxGetInstanceHandle(), GetCurrentThreadId());
			currentKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, HandleKeyboardEvents, AfxGetInstanceHandle(),GetCurrentThreadId());
#else
			currentMouseHook = SetWindowsHookEx(WH_MOUSE, HandleMouseEvents, AfxGetInstanceHandle(), GetCurrentTask());
			currentKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, HandleKeyboardEvents, AfxGetInstanceHandle(), GetCurrentTask());
#endif
			gLastMenu = this;
		}

		SetWindowPos(NULL, startPoint.x, startPoint.y, 
					 size.cx + (2*MENU_BORDER_SIZE), size.cy + (2 * MENU_BORDER_SIZE),
					 SWP_NOZORDER  | SWP_NOACTIVATE);
		ShowWindow(SW_SHOWNA);
		//SetActiveWindow();
	}
	else delete pDropTarget;
}

void CDropMenu::SetDropMenuTarget(CDropMenuDropTarget *pDropTarget)
{

	m_pDropTarget = pDropTarget;

	DragAcceptFiles();
	m_pDropTarget->Register(this);

}

BOOL CDropMenu::PointInMenus(POINT pt)
{

	CRect rect;

	GetWindowRect(&rect);

	if(rect.PtInRect(pt))
	{
		return TRUE;
	}
	else
	{
		CDropMenu *menu = m_pMenuParent;

		while(menu != NULL && IsWindow(menu->m_hWnd))
		{
			menu->GetWindowRect(&rect);
			if(rect.PtInRect(pt))
			{
				return TRUE;
			}
			menu=menu->m_pMenuParent;
		}

		menu = m_pShowingSubmenu;

		while(menu !=NULL && IsWindow(menu->m_hWnd))
		{
			menu->GetWindowRect(&rect);
			if(rect.PtInRect(pt))
			{
				return TRUE;
			}
			menu = menu->m_pShowingSubmenu;
		}

	}
	return FALSE;
}

void CDropMenu::HandleKeystroke(WPARAM key)
{
	//Make all open windows know that we are in keyboard mode still
	CDropMenu *pMenu = this;

	while(pMenu != NULL)
	{
		pMenu->m_bMouseUsed = FALSE;
		pMenu = pMenu->m_pMenuParent;
	}

	if(key == VK_ESCAPE || !m_bDragging)
	{
		switch(key)
		{
			case VK_ESCAPE:  currentMenu->Deactivate();
							 break;
			case VK_UP:		 HandleUpArrow();
							 break;
			case VK_DOWN:	 HandleDownArrow();
							 break;
			case VK_LEFT:	 HandleLeftArrow();
							 break;
			case VK_RIGHT:	 HandleRightArrow();
							 break;
			case VK_RETURN:  HandleReturnKey();
		}
	}

}

BOOL CDropMenu::OnCommand( WPARAM wParam, LPARAM lParam )
{
	ShowWindow(SW_HIDE);

	m_pParent->SendMessage(WM_COMMAND, wParam, lParam);

	return 1;
}

BOOL CDropMenu::DestroyWindow(void)
{
	return(CWnd::DestroyWindow());
}


void CDropMenu::Deactivate(void)
{
		if(m_pMenuParent != NULL)
		{
			m_pMenuParent->SendMessage(DM_CLOSEALLMENUS, 0, 0);

		}
		ShowWindow(SW_HIDE);
}

BOOL CDropMenu::ShowWindow( int nCmdShow )
{
	if(nCmdShow == SW_HIDE && IsWindowVisible())
	{
		m_bIsOpen = FALSE;
		if(m_pMenuParent != NULL)
		{
			//Turn on parent's focus timer
			m_pMenuParent->m_nHaveFocusTimer = m_pMenuParent->SetTimer(IDT_HAVE_FOCUS, HAVE_FOCUS_DELAY, NULL);
			// we only want the parent to be the current menu if the parent hasn't already
			// brought up a new submenu.  Otherwise we have timing conflicts
			if(m_pMenuParent->m_bShowing && 
			  (m_pMenuParent->m_pShowingSubmenu == this || m_pMenuParent->m_pShowingSubmenu == NULL))
			{
				currentMenu = m_pMenuParent;
			}

		}
		KillTimer(m_nHaveFocusTimer);

		//if this is the top level parent then the hook needs to be destroyed
		if(m_pMenuParent == NULL)
		{
			UnhookWindowsHookEx(currentMouseHook);
			UnhookWindowsHookEx(currentKeyboardHook);
			SetWindowLong(GetParentFrame()->m_hWnd, GWL_WNDPROC, (LONG)oldFrameProc);
			currentMenu = NULL;
			gLastMenu = NULL;
		}
		OnTimer(IDT_SUBMENU_OFF);
		m_bShowing = FALSE;
		m_nSelectedItem = -1;

		if(m_pShowingSubmenu)
		{
			//check to see if it's valid first because we may
			//want to deactivate before it was created due to
			//the timer never having been called to create it
			if(IsWindow(m_pShowingSubmenu->m_hWnd))
			{
				m_pShowingSubmenu->ShowWindow(SW_HIDE);
			}
			m_pShowingSubmenu = NULL;
		}
		if(m_pMenuParent == NULL)
		{
			m_pParent->PostMessage(DM_MENUCLOSED, 0, 0);

		}
	}
	else if(nCmdShow == SW_SHOWNA)
	{
		if(m_pMenuParent != NULL)
		{
			//since we are being shown, turn off parent's focus timer
			m_pMenuParent->KillTimer(m_nHaveFocusTimer);
		}

		m_nHaveFocusTimer = SetTimer(IDT_HAVE_FOCUS, HAVE_FOCUS_DELAY, NULL);
		currentMenu = this;

	}

	return CWnd::ShowWindow(nCmdShow);


}

void CDropMenu::PostNcDestroy(void )
{
	CWnd::PostNcDestroy();
}

void CDropMenu::DestroyDropMenu(void)
{
	DestroyWindow();

}
//////////////////////////////////////////////////////////////////////////
//					Messages for CDropMenu
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CDropMenu, CWnd)
	//{{AFX_MSG_MAP(CWnd)
 	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_NCHITTEST()
	ON_MESSAGE(DT_DROPOCCURRED, OnDropTargetDropOccurred)
	ON_MESSAGE(DT_DRAGGINGOCCURRED, OnDropTargetDraggingOccurred)
	ON_MESSAGE(DM_CLOSEALLMENUS, OnCloseAllMenus)
	ON_WM_TIMER()
	ON_WM_ACTIVATE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

void CDropMenu::OnPaint()
{
	CRect updateRect;


	if(GetUpdateRect(&updateRect))
	{
		CPaintDC dcPaint(this);	// device context for painting

		CRect rcClient;
		
		GetClientRect(&rcClient);

		HDC hSrcDC = dcPaint.m_hDC;

		HDC hMemDC = ::CreateCompatibleDC(hSrcDC);
		HBITMAP hbmMem = CreateCompatibleBitmap(hSrcDC,
                                    rcClient.Width(),
                                    rcClient.Height());

		//
		// Select the bitmap into the off-screen DC.
		//
		COLORREF rgbOldBk = ::SetBkColor(hMemDC, sysInfo.m_clrMenu);

		HBITMAP hbmOld = (HBITMAP)::SelectObject(hMemDC, hbmMem);

		HBRUSH brFace = sysInfo.m_hbrMenu;
		::FillRect(hMemDC, rcClient, brFace);
		
		CRect intersectRect;
		int nNumColumns = m_pColumnArray.GetSize();
		int nFirstItem, nLastItem;
		CDropMenuColumn *pColumn;
		int nStartX = 0;

		for(int j = 0; j <=m_nLastVisibleColumn; j++)
		{
			int y = MENU_BORDER_SIZE;

			pColumn = (CDropMenuColumn*)m_pColumnArray[j];

			nFirstItem = pColumn->GetFirstItem();
			nLastItem = pColumn->GetLastItem();

			for(int i = nFirstItem; i <= nLastItem; i++)
			{
				CDropMenuItem *item = (CDropMenuItem*)m_pMenuItemArray[i];

				if(m_bShowFeedback)
					y += SPACE_BETWEEN_ITEMS;

				if(item->IsSeparator())
				{
					CRect separatorRect(nStartX + MENU_BORDER_SIZE, y, nStartX + MENU_BORDER_SIZE + pColumn->GetWidth(), y + SEPARATOR_SPACE * 2 + SEPARATOR_HEIGHT);

					if(intersectRect.IntersectRect(separatorRect, updateRect))
					{
						DrawSeparator(hMemDC, separatorRect.left, y, separatorRect.Width() - (2 * MENU_BORDER_SIZE));
					}
					y += SEPARATOR_SPACE * 2 + SEPARATOR_HEIGHT;
				}
				else
				{
					CRect itemRect;

					item->GetMenuItemRect(itemRect);
					if(intersectRect.IntersectRect(itemRect, updateRect))
					{
						CRect rect(nStartX + MENU_BORDER_SIZE, y ,nStartX + MENU_BORDER_SIZE + pColumn->GetWidth(), y + itemRect.Height()); 
						
						DrawItem(pColumn, item, rect, hMemDC, i == m_nSelectedItem, 
								(i == m_nSelectedItem) ? m_eSelectedItemSelType : eNONE);
					}

					y += itemRect.Height();
				}

			}
			nStartX += pColumn->GetWidth();
			if(j < m_nLastVisibleColumn)
			{
				DrawVerticalSeparator(hMemDC, nStartX, MENU_BORDER_SIZE, rcClient.Height() - (2 * MENU_BORDER_SIZE));
				nStartX += SEPARATOR_WIDTH;
			}
		}
		// need to make sure selection type shows up
		if(m_nSelectedItem != NOSELECTION && m_eSelectedItemSelType != eNONE)
		{
			CRect itemRect;
			CDropMenuItem *item = (CDropMenuItem*)m_pMenuItemArray[m_nSelectedItem];
			
			if(!item->IsSeparator())
			{
				item->GetMenuItemRect(itemRect);

				CRect rect(itemRect.left, itemRect.top ,itemRect.left + ((CDropMenuColumn*)m_pColumnArray[item->GetColumn()])->GetWidth(), itemRect.top + itemRect.Height()); 
						
				DrawItem((CDropMenuColumn*)m_pColumnArray[item->GetColumn()], item, rect, hMemDC, TRUE, m_eSelectedItemSelType );
			}

		}
		DrawBorders(hMemDC, rcClient);

		::BitBlt(hSrcDC, 0, 0, rcClient.Width(), rcClient.Height(), hMemDC, 0, 0,
					SRCCOPY);

		::SetBkColor(hMemDC, rgbOldBk);

		::SelectObject(hMemDC, hbmOld);
		::DeleteObject(hbmMem);
 
		::DeleteDC(hMemDC);
	}
	if(m_bFirstPaint)
	{
		MSG msg;

		PeekMessage(&msg, this->m_hWnd, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE);
		m_bFirstPaint = FALSE;
	}

}

void CDropMenu::OnMouseMove(UINT nFlags, CPoint point)
{
	if(!m_bMouseUsed)
	{
		m_bMouseUsed = TRUE;
		return;
	}

	m_bMouseUsed = TRUE;
	
	KeepMenuAroundIfClosing();
	ChangeSelection(point);
}

UINT CDropMenu::OnNcHitTest( CPoint point )
{
	return CWnd::OnNcHitTest(point);
}

void CDropMenu::OnLButtonUp(UINT nFlags, CPoint point)
{
	//if we haven't painted yet then the user hasn't had a chance to mouse up.
	if(m_bFirstPaint || m_nSelectedItem == NOSELECTION)
		return;

	MenuSelectionType eSelType;
	int nSelection = FindSelection(point, eSelType);

	if(nSelection != -1)
	{
		CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[nSelection];
		
		if(!pItem->IsSubMenu())
		{
			SendMessage(WM_COMMAND, (WPARAM) pItem->GetCommand(), (LPARAM) m_hWnd);
		}
	}
}

void CDropMenu::OnLButtonDown(UINT nFlags, CPoint point)
{
	// we want our frame to stay the active window.  Causes flicker
	GetParentFrame()->SetActiveWindow();
}

void CDropMenu::OnRButtonDown(UINT nFlags, CPoint point)
{
	// we want our frame to stay the active window.  Causes flicker
	GetParentFrame()->SetActiveWindow();
}


LRESULT CDropMenu::OnDropTargetDraggingOccurred(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
	{
		CDropMenuDragData* pData = (CDropMenuDragData*)lParam;
		KeepMenuAroundIfClosing();
		ChangeSelection(pData->m_PointHit);

		// Ok, now we need to figure out what our drag feedback should be.
		// note that point is already in our coordinates since we got it from our drop target
		MenuSelectionType eSelType;
		int nSelection = FindSelection(pData->m_PointHit, eSelType);

		if(nSelection != -1)
		{
			CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[nSelection];
			pData->m_nCommand = pItem->GetCommand();
			pData->eSelType = eSelType;
			m_pParent->SendMessage(DT_DRAGGINGOCCURRED, 1, lParam);
		}
	}
	else 
	{
		m_pParent->SendMessage(DT_DRAGGINGOCCURRED, 1, lParam);
	}

	return 1;
}

LRESULT CDropMenu::OnDropTargetDropOccurred(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
	{
		CDropMenuDragData* pData = (CDropMenuDragData*)lParam;
		
		// Ok, now we need to figure out what our drag feedback should be.
		// note that point is already in our coordinates since we got it from our drop target
		MenuSelectionType eSelType;
		int nSelection = FindSelection(pData->m_PointHit, eSelType);

		if(nSelection != -1)
		{
			CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[nSelection];
			pData->m_nCommand = pItem->GetCommand();
			pData->eSelType = eSelType;
			m_pParent->SendMessage(DT_DROPOCCURRED, 1, lParam);
		}
	}
	else 
	{
		m_pParent->SendMessage(DT_DROPOCCURRED, 1, lParam);
	}

	ShowWindow(SW_HIDE);

	return 1;
}

LRESULT CDropMenu::OnCloseAllMenus(WPARAM wParam, LPARAM lParam)
{

	if(m_pMenuParent != NULL)
	{
		m_pMenuParent->PostMessage(DM_CLOSEALLMENUS, wParam, lParam);
	}

	ShowWindow(SW_HIDE);

	return 1;
}


void CDropMenu::OnTimer( UINT  nIDEvent )
{
	if(nIDEvent == IDT_SUBMENU_OFF)
	{
		KillTimer(m_nUnselectTimer);

		if(m_pUnselectedItem != NULL && m_pUnselectedItem->GetSubMenu()->m_bShowing==FALSE)
		{
			m_pUnselectedItem->GetSubMenu()->ShowWindow(SW_HIDE);
		}
		m_pUnselectedItem = NULL;
		
	}
	else if(nIDEvent == IDT_SUBMENU_ON)
	{
		KillTimer(m_nSelectTimer);

		if(m_bShowing)
		{
			
			if(m_pSelectedItem)
			{
				CDropMenu *pSubMenu = m_pSelectedItem->GetSubMenu();
				if(pSubMenu && pSubMenu->m_bAboutToShow)
					ShowSubmenu(m_pSelectedItem);
				else
					m_pSelectedItem = NULL;

			}
			m_nSelectTimer = 0;
		}
	}
	else if(nIDEvent == IDT_HAVE_FOCUS)
	{
		if(m_nSelectedItem != -1 && m_bMouseUsed)
		{
			CDropMenuItem *pItem = (CDropMenuItem*)m_pMenuItemArray[m_nSelectedItem];
			
			CPoint point;

			GetCursorPos(&point);

			ScreenToClient(&point);

			CRect rcClient, rect;
			GetClientRect(rcClient);

			if (!rcClient.PtInRect(point) && m_pShowingSubmenu == NULL )
			{
				pItem->GetMenuItemRect(rect);
				rect.left = 0;
				rect.right = rcClient.right;

				m_nSelectedItem = -1;

				InvalidateRect(&rect);
			}
		}
	}

}

void CDropMenu::OnActivate( UINT nState, CWnd* pWndOther, BOOL bMinimized )
{
	if(nState == WA_ACTIVE)
	{
		pWndOther->SetActiveWindow();
	}
}

void CDropMenu::OnDestroy( )
{
	m_pDropTarget->Revoke();
	CWnd::OnDestroy();
}
  

//////////////////////////////////////////////////////////////////////////
//					Helpers for CDropMenu
//////////////////////////////////////////////////////////////////////////

void CDropMenu::DrawSeparator(HDC hDC, int x, int y, int nWidth)
{

	HPEN pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
	HPEN pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined (WIN32)
	::MoveToEx(hDC, x, y + SEPARATOR_SPACE, NULL);
#else
	::MoveTo(hDC, x, y + SEPARATOR_SPACE);
#endif
	::LineTo(hDC, x + nWidth, y + SEPARATOR_SPACE);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);
   
#if defined(WIN32)   
	pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHILIGHT));
#else
	pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
#endif

	pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined(WIN32)
	::MoveToEx(hDC, x, y + SEPARATOR_SPACE + 1, NULL);
#else
	::MoveTo(hDC, x, y + SEPARATOR_SPACE + 1);
#endif

	::LineTo(hDC, x + nWidth, y + SEPARATOR_SPACE + 1);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);

}

void CDropMenu::DrawVerticalSeparator(HDC hDC, int x, int y, int nHeight)
{
	HPEN pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
	HPEN pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined (WIN32)
	::MoveToEx(hDC, x + SEPARATOR_SPACE, y, NULL);
#else
	::MoveTo(hDC, x + SEPARATOR_SPACE, y);
#endif
	::LineTo(hDC, x + SEPARATOR_SPACE, y + nHeight);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);
   
#if defined(WIN32)   
	pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHILIGHT));
#else
	pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
#endif

	pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined(WIN32)
	::MoveToEx(hDC, x + SEPARATOR_SPACE + 1, y, NULL);
#else
	::MoveTo(hDC, x + SEPARATOR_SPACE + 1, y);
#endif

	::LineTo(hDC, x + SEPARATOR_SPACE + 1, y + nHeight);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);
}

void CDropMenu::DrawBorders(HDC hDC, CRect rcClient)
{

	//Draw inner rect which is all BTN_FACE
	RECT rect;
	::SetRect(&rect, rcClient.left + 2, rcClient.top + 2, rcClient.right - 2, rcClient.bottom - 2);
	::FrameRect(hDC, &rect, sysInfo.m_hbrBtnFace);
  
#if defined(WIN32)
	HPEN pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHILIGHT));
#else
	HPEN pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
#endif

	HPEN pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined(WIN32)
	::MoveToEx(hDC, rcClient.left + 1, rcClient.bottom - 1, NULL);
#else
	::MoveTo(hDC, rcClient.left + 1, rcClient.bottom - 1);
#endif
	::LineTo(hDC, rcClient.left + 1, rcClient.top + 1);
	::LineTo(hDC, rcClient.right - 1, rcClient.top + 1);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);

	pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));

	pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined(WIN32)
	::MoveToEx(hDC, rcClient.left, rcClient.bottom - 1, NULL);
#else
	::MoveTo(hDC, rcClient.left, rcClient.bottom - 1);
#endif

	::LineTo(hDC, rcClient.left, rcClient.top);
	::LineTo(hDC, rcClient.right - 1, rcClient.top);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);

	pen = ::CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

	pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined(WIN32)
	::MoveToEx(hDC, rcClient.right - 1, rcClient.top, NULL);
#else
	::MoveTo(hDC, rcClient.right - 1, rcClient.top);
#endif

	::LineTo(hDC, rcClient.right - 1 , rcClient.bottom - 1);
	::LineTo(hDC, rcClient.left - 1, rcClient.bottom - 1);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);

	pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));

	pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined(WIN32)
	::MoveToEx(hDC, rcClient.right - 2, rcClient.top + 1, NULL);
#else
	::MoveTo(hDC, rcClient.right - 2, rcClient.top + 1);
#endif
	::LineTo(hDC, rcClient.right - 2, rcClient.bottom - 2);
	::LineTo(hDC, rcClient.left, rcClient.bottom -2);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);

}


void CDropMenu::DrawItem(CDropMenuColumn *pColumn, CDropMenuItem *pItem, CRect rc, HDC hDC, BOOL bIsSelected, MenuSelectionType eSelType)
{
	// Extract the goods from lpDI
	ASSERT(pItem != NULL);
	
	// Get proper colors and paint the background first
	COLORREF rgbTxt;
	HBRUSH brFace;
	COLORREF rgbOldBk;

	if (bIsSelected && (eSelType == eNONE || eSelType == eON))
	{
		rgbOldBk = ::SetBkColor(hDC, sysInfo.m_clrHighlight);

		brFace = sysInfo.m_hbrHighlight;
		rgbTxt = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
	}
	else
	{
		rgbOldBk = ::SetBkColor(hDC, sysInfo.m_clrMenu);

		brFace = sysInfo.m_hbrMenu;
		rgbTxt = ::GetSysColor(COLOR_MENUTEXT);
	}
	
	::FillRect(hDC, rc, brFace);
	
	if(eSelType == eABOVE)
		DrawSeparator(hDC, rc.left, rc.top - SEPARATOR_SPACE - 2, rc.Width() / 2);
	else if(eSelType == eBELOW)
		DrawSeparator(hDC, rc.left, rc.bottom - SEPARATOR_HEIGHT - SEPARATOR_SPACE + 2, rc.Width() / 2);

	CDC *pDC = CDC::FromHandle(hDC);
	// Now paint the bitmap, left side of menu
	rc.left += nIMG_SPACE;
	CSize imgSize = DrawImage(pDC, rc, pColumn, pItem, bIsSelected, eSelType);
	
	// And now the text
	HFONT MenuFont;
#if defined(WIN32)
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		MenuFont = theApp.CreateAppFont( ncm.lfMenuFont );
	}
	else
	{
		MenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
	}
#else	// Win16
	MenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
#endif	// WIN32

	HFONT pOldFont = (HFONT)::SelectObject(hDC, MenuFont);
	int nOldBkMode = ::SetBkMode(hDC, TRANSPARENT);
	COLORREF rgbOldTxt = ::SetTextColor(hDC, rgbTxt);

	//Figure out where text should go in relation to bitmaps
	rc.left += imgSize.cx + nIMG_SPACE + (pColumn->GetWidestImage()-imgSize.cx);

	//Deal with the case where there is a tab for an accelerator
	CString label=pItem->GetText();

	DrawText(hDC, (const char*)label, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE |DT_EXPANDTABS);
	
	::SetBkMode(hDC, nOldBkMode);
	::SetTextColor(hDC, rgbOldTxt);
	::SelectObject(hDC, pOldFont);
	
#if defined(WIN32)
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		theApp.ReleaseAppFont(MenuFont);
	}
#endif

	// Draw submenu bitmap
	if(pItem->IsSubMenu() && !pItem->IsEmpty())
	{
		HBITMAP pSubmenuBitmap;

		if(bIsSelected && (eSelType == eNONE || eSelType == eON))
		{
			pSubmenuBitmap = m_hSubmenuSelectedBitmap;
		}
		else
		{
			pSubmenuBitmap = m_hSubmenuUnselectedBitmap;
		}

		// Create a scratch DC and select our bitmap into it.
		HDC pBmpDC = CreateCompatibleDC(hDC);
		HBITMAP pOldBmp = (HBITMAP)::SelectObject(pBmpDC, pSubmenuBitmap);
			
		// Center the image horizontally and vertically
		CPoint ptDst(rc.right - SUBMENU_WIDTH - SUBMENU_RIGHT_MARGIN, rc.top +
			(((rc.Height() - SUBMENU_HEIGHT) + 1) / 2));
			
		// Call the handy transparent blit function to paint the bitmap over
		// the background.
		::FEU_TransBlt(hDC, ptDst.x, ptDst.y,SUBMENU_WIDTH, SUBMENU_HEIGHT,
					   pBmpDC, 0, 0, WFE_GetUIPalette(GetParentFrame()),RGB(255, 0, 255) );
		
		// Cleanup
		::SelectObject(pBmpDC, pOldBmp);
		::DeleteDC(pBmpDC);

	}
	::SetBkColor(hDC, rgbOldBk);

} // END OF	FUNCTION CDropMenu::DrawItem()

/****************************************************************************
*
*	CDropMenu::DrawImage
*
*	PARAMETERS:
*		pDC		- pointer to device context to draw on
*		rect	- bounding rectangle to draw in
*		pItem	- pointer to CDropMenuItem that contains the data
*		bIsSelected - whether this item is selected
*
*	RETURNS:
*		width of the image, in pixels
*
*	DESCRIPTION:
*		Protected helper function for drawing the bitmap image in a menu
*		item. We return the size of the painted image so the caller can
*		position the menu text after the image.
*
****************************************************************************/

CSize CDropMenu::DrawImage(CDC * pDC, const CRect & rect, CDropMenuColumn *pColumn, CDropMenuItem * pItem, BOOL bIsSelected,
						   MenuSelectionType eSelType)
{
	CSize sizeImg;
	
	sizeImg.cx = sizeImg.cy = 0;

	HBITMAP hImg = (bIsSelected && (!m_bShowFeedback || eSelType == eON || eSelType == eNONE)) ?
					  pItem->GetSelectedBitmap() : pItem->GetUnselectedBitmap();
	if (hImg != NULL)
	{
		// Get image dimensions
		BITMAP bmp;
		GetObject(hImg, sizeof(bmp), &bmp);

		sizeImg.cx = bmp.bmWidth;
		sizeImg.cy = bmp.bmHeight;
		
		// Create a scratch DC and select our bitmap into it.
		HDC hBmpDC = ::CreateCompatibleDC(pDC->m_hDC);
		HBITMAP hOldBmp = (HBITMAP)SelectObject(hBmpDC, hImg);
			
		// Center the image horizontally and vertically
		CPoint ptDst(rect.left + (pColumn->GetWidestImage() - sizeImg.cx)/2, rect.top +
			(((rect.Height() - sizeImg.cy) + 1) / 2));
			
		// Call the handy transparent blit function to paint the bitmap over
		// the background.

		if (pItem->GetIconType() == LOCAL_FILE)
		{
			HICON hIcon = pItem->GetLocalFileIcon();
			DrawIconEx(pDC->m_hDC, ptDst.x, ptDst.y, hIcon, 0, 0, 0, NULL, DI_NORMAL);
		}
		else if (pItem->GetIconType() == ARBITRARY_URL)
		{
			CRDFImage* pImage = pItem->GetCustomIcon();
			HDC hDC = pDC->m_hDC;
			int left = ptDst.x;
			int top = ptDst.y;
			int imageWidth = 16;
			int imageHeight = 16;
			COLORREF bkColor = (bIsSelected && (!m_bShowFeedback || eSelType == eON || eSelType == eNONE)) ? 
				GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_MENU);
			if (pImage && pImage->FrameLoaded()) 
			{
				// Now we draw this bad boy.
				if (pImage->m_BadImage) 
				{		 
					// display broken icon.
					HDC tempDC = ::CreateCompatibleDC(hDC);
					HBITMAP hOldBmp = (HBITMAP)::SelectObject(tempDC,  CRDFImage::m_hBadImageBitmap);
					::StretchBlt(hDC, 
						 left, top,
						imageWidth, imageHeight, 
						tempDC, 0, 0, 
						imageWidth, imageHeight, SRCCOPY);
					::SelectObject(tempDC, hOldBmp);
					::DeleteDC(tempDC);
				}
				else if (pImage->bits ) 
				{
					// Center the image. 
					long width = pImage->bmpInfo->bmiHeader.biWidth;
					long height = pImage->bmpInfo->bmiHeader.biHeight;
					int xoffset = (imageWidth-width)/2;
					int yoffset = (imageHeight-height)/2;
					if (xoffset < 0) xoffset = 0;
					if (yoffset < 0) yoffset = 0;
					if (width > imageWidth) width = imageWidth;
					if (height > imageHeight) height = imageHeight;

					HPALETTE hPal = WFE_GetUIPalette(NULL);
					HPALETTE hOldPal = ::SelectPalette(hDC, hPal, TRUE);

					::RealizePalette(hDC);
					
					if (pImage->maskbits) 
					{
						WFE_StretchDIBitsWithMask(hDC, TRUE, NULL,
							left+xoffset, top+xoffset,
							width, height,
							0, 0, width, height,
							pImage->bits, pImage->bmpInfo,
							pImage->maskbits, FALSE, bkColor);
					}
					else 
					{
						::StretchDIBits(hDC,
							left+xoffset, top+xoffset,
							width, height,
							0, 0, width, height, pImage->bits, pImage->bmpInfo, DIB_RGB_COLORS,
							SRCCOPY);
					}

					::SelectPalette(hDC, hOldPal, TRUE);
				}
			}
		}
		else ::BitBlt(pDC->m_hDC, ptDst.x, ptDst.y, sizeImg.cx, sizeImg.cy, hBmpDC, 0, 0, SRCCOPY);
		
		// Cleanup
		::SelectObject(hBmpDC, hOldBmp);
		::DeleteDC(hBmpDC);

	}

	return(sizeImg);
	
} // END OF	FUNCTION CTreeMenu::DrawImage()
	
CDropMenuDropTarget *CDropMenu::GetDropMenuDropTarget(CWnd *pOwner)
{

	return new CDropMenuDropTarget(pOwner);
}

CSize CDropMenu::GetMenuSize(void)
{
	CSize totalSize, itemSize;

	int nColumnHeight = MENU_BORDER_SIZE, nColumnWidth = 0;


	CalculateItemDimensions();
	int nCount = m_pMenuItemArray.GetSize();

	int nScreenLeft = 0;
	int nScreenRight = GetSystemMetrics(SM_CXFULLSCREEN);
	int nScreenTop = 0;
	int nScreenBottom = GetSystemMetrics(SM_CYFULLSCREEN);
#ifdef XP_WIN32
	RECT rect;
	if(SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&rect,0))
	{
		nScreenLeft = rect.left;
		nScreenRight = rect.right;
		nScreenTop = rect.top;
		nScreenBottom = rect.bottom;
	}
#endif

	int nStartX= MENU_BORDER_SIZE;
	int nOldStartX = MENU_BORDER_SIZE;

	int nMaxColumnHeight = 0;

	int nNumColumns = m_pColumnArray.GetSize();
	CSize columnSize;
	CDropMenuColumn *pColumn;
	m_nLastVisibleColumn = nNumColumns - 1;

	for(int i = 0; i < nNumColumns; i++)
	{
		pColumn = (CDropMenuColumn *)m_pColumnArray[i];
		if(pColumn->GetHasSubMenu())
			pColumn->SetWidth(pColumn->GetWidth() + SUBMENU_LEFT_MARGIN + SUBMENU_RIGHT_MARGIN + SUBMENU_WIDTH);
		else
			pColumn->SetWidth(pColumn->GetWidth() + SUBMENU_RIGHT_MARGIN);

		columnSize = CalculateColumn(pColumn, nStartX);

		if(nScreenLeft + nStartX + columnSize.cx >= nScreenRight)
		{
			//if we have more than one column, don't put in the last separator
			if(nNumColumns > 1)
			{
				if(m_pOverflowMenuItem != NULL)
				{
					if(i  > 0)
						nStartX = AddOverflowItem(i - 1, nOldStartX);
					else
						nStartX = AddOverflowItem(i, nStartX - SEPARATOR_WIDTH);
				}


			}
			m_nLastVisibleColumn = i - 1;
			break;
		}
	
		if(columnSize.cy > nMaxColumnHeight)
			nMaxColumnHeight = columnSize.cy;

		nOldStartX = nStartX;
		nStartX += columnSize.cx;
		if(i + 1 < nNumColumns)
			nStartX += SEPARATOR_WIDTH;
	}

	totalSize.cy = nMaxColumnHeight;
	totalSize.cx = ((nNumColumns > 1) ? nStartX - SEPARATOR_WIDTH : nStartX);
	return totalSize;
}

CSize CDropMenu::CalculateColumn(CDropMenuColumn *pColumn, int nStartX)
{
	int nStart = pColumn->GetFirstItem();
	int nEnd = pColumn->GetLastItem();
	int nColumnWidth = pColumn->GetWidth();
	int nItemHeight;

	CSize totalSize(0, MENU_BORDER_SIZE);
	CSize itemSize;
	for(int i = nStart; i <= nEnd; i++)
	{
		CDropMenuItem *item = (CDropMenuItem*)m_pMenuItemArray[i];

		if(m_bShowFeedback)
			totalSize.cy += SPACE_BETWEEN_ITEMS;

		if(item->IsSeparator())
		{

			CRect rect (nStartX, totalSize.cy, nStartX + nColumnWidth, totalSize.cy + SEPARATOR_SPACE * 2 + SEPARATOR_HEIGHT);
			item->SetMenuItemRect(rect);

			nItemHeight = SEPARATOR_SPACE * 2 + SEPARATOR_HEIGHT;

		}
		else
		{
			itemSize=MeasureItem(item);
			CRect rect(nStartX, totalSize.cy, nStartX + nColumnWidth, totalSize.cy + itemSize.cy);

			item->SetMenuItemRect(rect);

			nItemHeight = itemSize.cy;

		}

		totalSize.cy += nItemHeight;

	}
	return CSize(pColumn->GetWidth(), totalSize.cy);
}

//returns the x coordinate of the right side of this last column
int CDropMenu::AddOverflowItem(int nColumn, int nStartX)
{
	int nScreenTop = 0;
	int nScreenBottom = GetSystemMetrics(SM_CYFULLSCREEN);
#ifdef XP_WIN32
	RECT rect;
	if(SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&rect,0))
	{
		nScreenTop = rect.top;
		nScreenBottom = rect.bottom;
	}
#endif

	CDropMenuColumn *pColumn = (CDropMenuColumn *)m_pColumnArray[nColumn];

	int nPosition = pColumn->GetLastItem();

	CDropMenuItem *pLastItem = (CDropMenuItem*)m_pMenuItemArray[nPosition];

	if(pLastItem == m_pOverflowMenuItem)
		return pColumn->GetWidth();

	int nHeight = pColumn->GetHeight();
	CRect overflowRect;
	CRect itemRect;
	CDropMenuItem *pItem;

	CSize overflowSize = MeasureItem(m_pOverflowMenuItem);
	overflowSize.cx += pColumn->GetWidestImage();

	while(nScreenTop + nHeight + overflowSize.cy > nScreenBottom)
	{
		pItem = (CDropMenuItem*)m_pMenuItemArray[nPosition];
		pItem->GetMenuItemRect(&itemRect);


		nHeight -= itemRect.Height();
		nPosition--;
	}

	// add 1 since we've subtracted 1 away
	pColumn->SetLastItem(nPosition + 1);
	pColumn->SetHeight(nHeight + overflowSize.cy);
	m_pOverflowMenuItem->SetColumn(nColumn);
	if(overflowSize.cx > pColumn->GetWidth())
		pColumn->SetWidth(overflowSize.cx);
	m_pMenuItemArray.InsertAt(nPosition + 1, m_pOverflowMenuItem, 1);
	itemRect.SetRect(nStartX, nHeight, nStartX + pColumn->GetWidth(), nHeight + overflowSize.cy);
	m_pOverflowMenuItem->SetMenuItemRect(itemRect);
	return (nStartX + pColumn->GetWidth());

}

void CDropMenu::CalculateItemDimensions(void)
{

	int numItems = m_pMenuItemArray.GetSize();

	int nScreenTop = 0;
	int nScreenBottom = GetSystemMetrics(SM_CYFULLSCREEN);
#ifdef XP_WIN32
	RECT rect;
	if(SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&rect,0))
	{
		nScreenTop = rect.top;
		nScreenBottom = rect.bottom;
	}
#endif

	int nNumCols = 0;
	int nColumnHeight = nScreenBottom + 1;
	int nColumnWidth = 0;
	CSize textSize, unselectedBitmapSize, selectedBitmapSize, itemSize;
	CDropMenuColumn *pColumn = NULL;
	int nMaxBitmapSize = 0, nMaxTextSize = 0;

	int nCount = m_pColumnArray.GetSize();

	//make sure column array is empty
	for(int j = 0 ; j < nCount; j++)
		delete (CDropMenuColumn*)m_pColumnArray[j];

	m_pColumnArray.RemoveAll();

	for(int i=0; i<numItems; i++)
	{
		CDropMenuItem *item = (CDropMenuItem*)m_pMenuItemArray[i];

		CalculateOneItemsDimensions(item);

		textSize = item->GetTextSize();
		selectedBitmapSize = item->GetSelectedBitmapSize();
		unselectedBitmapSize = item->GetUnselectedBitmapSize();
		itemSize = MeasureItem(item);

		if(nScreenTop + nColumnHeight + itemSize.cy > nScreenBottom)
		{
			pColumn = new CDropMenuColumn;
			pColumn->SetHasSubMenu(FALSE);
			pColumn->SetFirstItem(i);

			m_pColumnArray.Add(pColumn);
			nColumnHeight = nColumnWidth = 0;
			nMaxBitmapSize = 0, nMaxTextSize = 0;
			nNumCols++;
		}

		item->SetColumn(nNumCols - 1);
		pColumn->SetLastItem(i);

		if(unselectedBitmapSize.cx > nMaxBitmapSize)
			nMaxBitmapSize = unselectedBitmapSize.cx;

		if(selectedBitmapSize.cx > nMaxBitmapSize)
			nMaxBitmapSize= selectedBitmapSize.cx;

		if(textSize.cx > nMaxTextSize)
			nMaxTextSize = textSize.cx;

		if(item->IsSubMenu())
			pColumn->SetHasSubMenu(TRUE);

		nColumnHeight += itemSize.cy;
		if(m_bShowFeedback)
			nColumnHeight += SPACE_BETWEEN_ITEMS;

		pColumn->SetHeight(nColumnHeight);

		pColumn->SetWidth(nMaxBitmapSize + nMaxTextSize + ( nIMG_SPACE * 3));
		pColumn->SetWidestImage(nMaxBitmapSize);
		if(item == m_pOverflowMenuItem)
			return;
	}

	if(m_pOverflowMenuItem != NULL)
		CalculateOneItemsDimensions(m_pOverflowMenuItem);

}

void CDropMenu::CalculateOneItemsDimensions(CDropMenuItem* item)
{
	CSize unselectedBitmapSize, selectedBitmapSize, textSize;
	CString text;

	if(!item->IsSeparator())
	{
		unselectedBitmapSize = MeasureBitmap(item->GetUnselectedBitmap());
		item->SetUnselectedBitmapSize(unselectedBitmapSize);
		
		selectedBitmapSize = MeasureBitmap(item->GetSelectedBitmap());
		item->SetSelectedBitmapSize(selectedBitmapSize);

		if(item->IsSubMenu())
		{
			m_bHasSubMenu = TRUE;
		}


		text = item->GetText();

		textSize = MeasureText(text);


		item->SetTextSize(textSize);
	}


}

/****************************************************************************
*
*	CTreeMenu::MeasureBitmap
*
*	PARAMETERS:
*		pBitmap:  The bitmap to measure
*
*	RETURNS:
*		The size of the bitmap
*
*	DESCRIPTION:
*		Given a bitmap, this returns its dimensions
*
****************************************************************************/

CSize CDropMenu::MeasureBitmap(HBITMAP hBitmap)
{

	CSize size;

	if (hBitmap != NULL)
	{
		BITMAP bmp;
		GetObject(hBitmap, sizeof(bmp), &bmp);

		size.cx = bmp.bmWidth;
		size.cy = bmp.bmHeight;
	}
	else
		size.cx = size.cy = 0;

	return size;


}


/****************************************************************************
*
*	CTreeMenu::MeasureText
*
*	PARAMETERS:
*		text:  The CString text to be measured
*
*	RETURNS:
*		Returns the size of the text in the current font
*
*	DESCRIPTION:
*		Given a CString this returns the dimensions of that text
*
****************************************************************************/

CSize CDropMenu::MeasureText(CString text)
{

	HFONT hMenuFont;

#if defined(WIN32)
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		hMenuFont = theApp.CreateAppFont( ncm.lfMenuFont );
	}
	else
	{
		hMenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
	}
#else	// Win16
	hMenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
#endif	// WIN32

	HDC hDC = ::GetDC(m_hWnd);

	HFONT hOldFont = (HFONT) ::SelectObject(hDC, hMenuFont);
	CSize sizeTxt = ::GetTabbedTextExtent(hDC, (LPCSTR)text, strlen(text), 0, NULL);
	::SelectObject(hDC, hOldFont);

#if defined(WIN32)
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		theApp.ReleaseAppFont(hMenuFont);
	}
#endif


	::ReleaseDC(m_hWnd, hDC);

	return sizeTxt;

}

/****************************************************************************
*
*	CTreeMenu::MeasureItem
*
*	PARAMETERS:
*		lpMI	- pointer to LPMEASUREITEMSTRUCT
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We must override this function to inform the system of our menu
*		item dimensions, since we're owner draw style.
*
****************************************************************************/
								//menu item and end of accelerator

CSize CDropMenu::MeasureItem(CDropMenuItem *pItem)
{
	ASSERT(pItem != NULL);
	
	CSize size;

	if(pItem->IsSeparator())
	{
		size.cx = 0;
		size.cy = SEPARATOR_SPACE * 2 + SEPARATOR_HEIGHT;
	}
	else
	{
		// First, get all of the dimensions
		CSize unselectedSizeImg=pItem->GetUnselectedBitmapSize();
		CSize selectedSizeImg = pItem->GetSelectedBitmapSize();
		CSize sizeText=pItem->GetTextSize();

		size.cx = sizeText.cx + (nIMG_SPACE * 2);

		int maxImageHeight = max(unselectedSizeImg.cy, selectedSizeImg.cy);

		size.cy = max(maxImageHeight, sizeText.cy) + 4;
	}
	return size;
	
}

void CDropMenu::ChangeSelection(CPoint point)
{
	if(m_bShowing)
	{
		MenuSelectionType eSelType;
		int nSelection = FindSelection(point, eSelType);

		eSelType = m_bShowFeedback ? eSelType : eNONE;

		if(m_nSelectedItem != nSelection || m_eSelectedItemSelType != eSelType) 
		{
			SelectMenuItem(m_nSelectedItem, FALSE, TRUE, m_eSelectedItemSelType);
			SelectMenuItem(nSelection, TRUE, TRUE, eSelType);
		}
	}
}

int CDropMenu::FindSelection(CPoint point, MenuSelectionType &eSelType)
{

	eSelType = eNONE;
	int nNumColumns = m_pColumnArray.GetSize();
	CRect colRect;
	CDropMenuColumn *pColumn;
	int nStartX = MENU_BORDER_SIZE;
	int nColumnWidth;

	for(int i = 0; i < nNumColumns; i++)
	{
		
		pColumn = (CDropMenuColumn*)m_pColumnArray[i];	
		nColumnWidth = pColumn->GetWidth();

		colRect.SetRect(nStartX, 0, nStartX + nColumnWidth, pColumn->GetHeight());

		if(colRect.PtInRect(point))
		{

			return (FindSelectionInColumn(pColumn, point, eSelType));
		}
		else
		{
			nStartX += nColumnWidth + SEPARATOR_WIDTH;
		}

	}

	return -1;
}

int  CDropMenu::FindSelectionInColumn(CDropMenuColumn *pColumn, CPoint point, MenuSelectionType &eSelType)
{

	int y = MENU_BORDER_SIZE;

	if(point.y >=0 && point.y <= MENU_BORDER_SIZE)
		return 0;
	
	int nFeedbackHeight = m_bShowFeedback ? SPACE_BETWEEN_ITEMS : 0;
	CDropMenuItem *item;
	int nFirstItem = pColumn->GetFirstItem();
	int nLastItem = pColumn->GetLastItem();

	for(int i = nFirstItem; i <= nLastItem; i++)
	{
		item = (CDropMenuItem*)m_pMenuItemArray[i];

		CRect rect;

		item->GetMenuItemRect(rect);

		if( point.y >= y && point.y < y + rect.Height() + nFeedbackHeight)
		{
			if(item->GetShowFeedback())
				eSelType = FindSelectionType(item, point.y - y, rect.Height() + nFeedbackHeight);
			return i;
		}

		y += rect.Height() + nFeedbackHeight;
	}

	//if we're in the border between the bottom and the last menu item
	if(point.y >= y && point.y <= y + 2*MENU_BORDER_SIZE)
	{
		if(item->GetShowFeedback())
			eSelType = eBELOW;
		return i - 1;
	}



	return -1;

}

MenuSelectionType CDropMenu::FindSelectionType(CDropMenuItem *pItem, int y, int nHeight)
{
	if(pItem->IsSubMenu())
	{
		if (y >= 0 && y <= nHeight/4.0)
			return eABOVE;
		else if (y <= 3.0*nHeight/4.0)
			return eON;
		else return eBELOW;
	}
	else
	{
		if(y >= 0 && y <= nHeight / 2.0)
			return eABOVE;
		else
			return eBELOW;
	}
}

void CDropMenu::SelectMenuItem(int nItemIndex, BOOL bIsSelected, BOOL bHandleSubmenus, MenuSelectionType eSelType)
{

	RECT rcClient;

	GetClientRect(&rcClient);

	if(nItemIndex != -1)
	{
		CDropMenuItem *item = (CDropMenuItem*)m_pMenuItemArray[nItemIndex];

		InvalidateMenuItemRect(item);
	

		if(!bIsSelected)
		{

			m_pShowingSubmenu = NULL;

			m_nSelectedItem = -1;
			m_eSelectedItemSelType = eNONE;

			if(bHandleSubmenus && item->IsSubMenu() && !item->IsEmpty() && item->GetSubMenu()->m_bShowing)
			{
				if(m_pUnselectedItem == NULL)
				{
					item->GetSubMenu()->m_bShowing = FALSE;
					m_pUnselectedItem = item;
					m_nUnselectTimer = SetTimer(IDT_SUBMENU_OFF ,SUBMENU_DELAY_OFF_MS, NULL);
				}
			}

		}
		else
		{
			m_nSelectedItem = nItemIndex;
			m_eSelectedItemSelType = eSelType;
			if(m_nSelectTimer != 0)
			{
				KillTimer(m_nSelectTimer);
			}

			if(bHandleSubmenus && item->IsSubMenu() && !item->IsEmpty() && (eSelType == eON || eSelType == eNONE))
			{
				m_pShowingSubmenu = item->GetSubMenu();
				m_pSelectedItem = item;
				m_pSelectedItem->GetSubMenu()->m_bAboutToShow = TRUE;
				m_nSelectTimer = SetTimer(IDT_SUBMENU_ON, SUBMENU_DELAY_ON_MS, NULL);

			}

		}
	}
}


BOOL CDropMenu::NoSubmenusShowing(void)
{
	CWnd *child = GetWindow(GW_CHILD);

	if(child != NULL)
	{
		while(child != NULL)
		{
			if(child->IsWindowVisible())
			{
				return FALSE;
			}

			child = GetWindow(GW_HWNDNEXT);
		}
		
	}

	return TRUE;
}

CDropMenu *CDropMenu::GetMenuParent(void)
{

	return m_pMenuParent;
}

BOOL CDropMenu::IsDescendant(CWnd *pDescendant)
{
	CDropMenu *pSubmenu = m_pShowingSubmenu;

	while(pSubmenu != NULL)
	{
		if(pSubmenu == pDescendant)
		{
			return TRUE;
		}

		pSubmenu = pSubmenu->m_pShowingSubmenu;
	}

	return FALSE;
}

BOOL CDropMenu::IsAncestor(CWnd *pAncestor)
{

	CDropMenu *pMenu = this;

	while(pMenu->m_pParent ==  pMenu->m_pMenuParent)
	{
		if(pMenu->m_pMenuParent == pAncestor)
		{
			return TRUE;
		}


		pMenu = pMenu->m_pMenuParent;
	}

	return FALSE;
}

//Sees if the hwnd is either this menu, an ancestor or a submenu
BOOL CDropMenu::IsShowingMenu(HWND hwnd)
{

	if(hwnd == m_hWnd)
	{
		return TRUE;
	}
	else
	{
		CDropMenu *menu = m_pMenuParent;

		while(menu != NULL)
		{
			if(hwnd == menu->m_hWnd)
			{
				return TRUE;
			}
			menu=menu->m_pMenuParent;
		}

		menu = m_pShowingSubmenu;

		while(menu !=NULL)
		{
			if(hwnd == menu->m_hWnd)
			{
				return TRUE;
			}
			menu = menu->m_pShowingSubmenu;
		}

	}
	return FALSE;
}

void CDropMenu::DropMenuTrackDropMenu(CDropMenu *pParent, int x, int y, BOOL bDragging, CDropMenuDropTarget *pDropTarget,
									  BOOL bShowFeedback, int oppX, int oppY)
{
	m_pMenuParent = pParent;

	pParent->m_pShowingSubmenu = this;

	TrackDropMenu(pParent, x, y, bDragging, pDropTarget, bShowFeedback, oppX, oppY);
}

int CDropMenu::FindCommand(UINT nCommand)
{

	int nCount = m_pMenuItemArray.GetSize();

	for(int i = 0; i < nCount; i++)
	{
		CDropMenuItem * pItem = (CDropMenuItem*)m_pMenuItemArray[i];
		if(!pItem->IsSeparator())
		{
			if(pItem->GetCommand() == nCommand)
			{
				return i;
			}
		}
	}
	
	return -1;

}

CPoint CDropMenu::FindStartPoint(int x, int y, CSize size, int oppX, int oppY)
{

	int nScreenLeft = 0;
	int nScreenRight = GetSystemMetrics(SM_CXFULLSCREEN);
	int nScreenTop = 0;
	int nScreenBottom = GetSystemMetrics(SM_CYFULLSCREEN);
#ifdef XP_WIN32
	RECT rect;
	if(SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&rect,0))
	{
		nScreenLeft = rect.left;
		nScreenRight = rect.right;
		nScreenTop = rect.top;
		nScreenBottom = rect.bottom;
	}
#endif


	if(oppX == -1)
	{
		CRect parentRect;
		m_pParent->GetWindowRect(&parentRect);

		oppX = m_bRight ? parentRect.left : parentRect.right;
	}

	int nRoomToRight = nScreenRight - (m_bRight ? x : oppX);
	int nRoomToLeft = (m_bRight ? oppX : x) - nScreenLeft;

	// if we are opening to the right we need to make sure we don't go off the right
	// side of the screen.
	if(m_bRight)
	{
		//if we will extend past the right
		if(x + size.cx > nScreenRight)
		{
			// if we can fit to the left
			if(oppX - size.cx > nScreenLeft)
			{
				if(m_pMenuParent != NULL)
				{
					x = oppX - size.cx;
					//if it's not the top level menu we want it to overlap its parent
					x -= 2;
				}
				else
				{
					x = oppX - size.cx;
				}

				m_bRight = FALSE;
			}
			else
			{
				//see where we have more space
				if(nRoomToRight >= nRoomToLeft)
					x = nScreenRight - size.cx;
				else
				{
					x = nScreenLeft;
					m_bRight = FALSE;
				}
			}
		}
		else if(m_pMenuParent != NULL)
		{
			// if we are a submenu we want to overlap our parent
			x -= 5;
		}
	}
	// if we are opening to the left we need to make sure we don't go off the left
	// side of the screen
	else
	{
		//if we will extend past the left
		if(x - size.cx < nScreenLeft)
		{
			// if we can fit to the right
			if(oppX + size.cx < nScreenRight)
			{
				x = oppX;
				//if it's not the top level menu we want it to overlap its parent
				if(m_pMenuParent != NULL)
				{
					x -= 5;
				}
				m_bRight = TRUE;
			}
			else
			{
				if(nRoomToRight >= nRoomToLeft)
				{
					x = nScreenRight - size.cx;
					m_bRight = TRUE;
				}
				else
					x = nScreenLeft;
			}
		}
		else if(m_pMenuParent != NULL)
		{
			x -= size.cx;
			// if we are a submenu we want to overlap our parent
			x -= 2;
		}
	}

	if(y + size.cy > nScreenBottom)
	{
		y -= ((y + size.cy) - nScreenBottom);
		
	}

	return CPoint(x, y);
}

void CDropMenu::HandleUpArrow(void)
{
	int nSelection;

	// need to deal with separators
	if(m_nSelectedItem == NOSELECTION || m_nSelectedItem == 0)
	{
		nSelection = GetMenuItemCount() - 1;
	}
	else
	{
		nSelection = m_nSelectedItem - 1;
	}

	while(((CDropMenuItem *)m_pMenuItemArray[nSelection])->IsSeparator())
	{
		nSelection = (nSelection == 0) ? GetMenuItemCount() - 1 : nSelection - 1;
	}

	SelectMenuItem(m_nSelectedItem, FALSE, FALSE, eNONE);
	SelectMenuItem(nSelection, TRUE, FALSE, eNONE);
}

void CDropMenu::HandleDownArrow(void)
{
	int nSelection;
	int nNumItems = GetMenuItemCount() - 1;

	// need to deal with separators
	if(m_nSelectedItem == NOSELECTION || m_nSelectedItem == nNumItems)
	{
		nSelection = 0;
	}
	else
	{
		nSelection = m_nSelectedItem + 1;
	}

	while(((CDropMenuItem *)m_pMenuItemArray[nSelection])->IsSeparator())
	{
		nSelection = (nSelection == nNumItems - 1) ? 0 : nSelection + 1;
	}

	SelectMenuItem(m_nSelectedItem, FALSE, FALSE, eNONE);
	SelectMenuItem(nSelection, TRUE, FALSE, eNONE);
}


void CDropMenu::HandleLeftArrow(void)
{

	if(m_pMenuParent != NULL)
	{
		m_bShowing = FALSE;

		m_pMenuParent->m_bMouseUsed = FALSE;
		ShowWindow(SW_HIDE);		
	}


}

void CDropMenu::HandleRightArrow(void)
{
	if(m_nSelectedItem != NOSELECTION)
	{
		CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[m_nSelectedItem];
		if(pItem->IsSubMenu())
		{
			m_pShowingSubmenu = pItem->GetSubMenu();
			m_pShowingSubmenu->m_nSelectedItem = 0;
			m_pShowingSubmenu->m_bMouseUsed = FALSE;
			ShowSubmenu(pItem);
		}
	}
}

void CDropMenu::HandleReturnKey(void)
{
	if(m_nSelectedItem != NOSELECTION)
	{
		CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[m_nSelectedItem];

		if(pItem->IsSubMenu())
		{
			HandleRightArrow();
		}
		else
		{
#ifdef _WIN32
			SendMessage(WM_COMMAND, MAKEWPARAM(pItem->GetCommand(), 0), 0);
#else
			SendMessage(WM_COMMAND, (WPARAM) pItem->GetCommand(), MAKELPARAM( m_hWnd, 0 ));
#endif
		}
	}
}

void CDropMenu::ShowSubmenu(CDropMenuItem *pItem)
{

	CRect rcClient, rect;

	GetClientRect(rcClient);

	pItem->GetMenuItemRect(rect);

	CDropMenuDropTarget *pDropTarget = GetDropMenuDropTarget(pItem->GetSubMenu());
	ClientToScreen(rect);

	m_pShowingSubmenu = pItem->GetSubMenu();

	pItem->GetSubMenu()->SetSubmenuBitmaps(m_hSubmenuSelectedBitmap, m_hSubmenuUnselectedBitmap);

	if(m_bRight)
	{
		pItem->GetSubMenu()->DropMenuTrackDropMenu(this, rect.right, rect.top - 2, m_bDragging, pDropTarget, m_bShowFeedback, rect.left);
	}
	else
	{
		pItem->GetSubMenu()->DropMenuTrackDropMenu(this, rect.left, rect.top - 2, m_bDragging, pDropTarget, m_bShowFeedback, rect.right);
	}
	
	pItem->GetSubMenu()->m_pParentMenuItem = pItem;

}

int	CDropMenu::FindMenuItemPosition(CDropMenuItem* pMenuItem)
{
	int nCount = m_pMenuItemArray.GetSize();

	for(int i = 0; i < nCount; i++)
		if(m_pMenuItemArray[i] == pMenuItem)
			return i;

	return -1;

}

void CDropMenu::SetSubmenuBitmaps(HBITMAP hSelectedBitmap, HBITMAP hUnselectedBitmap)
{

	m_hSubmenuUnselectedBitmap = hUnselectedBitmap;
	m_hSubmenuSelectedBitmap = hSelectedBitmap;

}

void CDropMenu::InvalidateMenuItemRect(CDropMenuItem *pMenuItem)
{

	CRect rcClient;
	CRect rect;

	GetClientRect(&rcClient);

	pMenuItem->GetMenuItemRect(rect);
	int nColumn = pMenuItem->GetColumn();
	CDropMenuColumn *pColumn= (CDropMenuColumn*)m_pColumnArray[nColumn];

	rect.right = rect.left + pColumn->GetWidth();
	rect.top -= 2;
	rect.bottom += 2;
	InvalidateRect(rect, TRUE);
}

void CDropMenu::InvalidateMenuItemIconRect(CDropMenuItem *pMenuItem)
{

	CRect rcClient;
	CRect rect;

	GetClientRect(&rcClient);

	pMenuItem->GetMenuItemRect(rect);
	int nColumn = pMenuItem->GetColumn();
	CDropMenuColumn *pColumn= (CDropMenuColumn*)m_pColumnArray[nColumn];

	rect.left += nIMG_SPACE;
	rect.right = rect.left + 16;
	
	InvalidateRect(rect, TRUE);
}

void CDropMenu::KeepMenuAroundIfClosing(void)
{
	// if there is movement on this menu, then it is showing even
	// if it's been told to go away before.
	m_bShowing = TRUE;
	if(m_pMenuParent)
	{
		CDropMenuItem *pSelectedItem = m_pMenuParent->m_nSelectedItem != -1 ? (CDropMenuItem*)m_pMenuParent->m_pMenuItemArray[m_pMenuParent->m_nSelectedItem] : NULL;

		CDropMenu *pSubMenu = pSelectedItem && pSelectedItem->IsSubMenu() ? pSelectedItem->GetSubMenu() : NULL;
		if((pSubMenu && pSubMenu != this) || !pSubMenu)
		{

			KillTimer(m_pMenuParent->m_nSelectTimer);
			KillTimer(m_pMenuParent->m_nUnselectTimer);
			m_pMenuParent->m_pUnselectedItem = NULL;
			m_pMenuParent->m_nSelectTimer = 0;
			m_pMenuParent->m_pShowingSubmenu = this;

			//We need to make the menu item this belongs to the selected menu item.
			int nPosition = m_pMenuParent->FindMenuItemPosition(m_pParentMenuItem);

			if(nPosition != -1)
			{

				if(pSelectedItem)
				{
					m_pMenuParent->InvalidateMenuItemRect(pSelectedItem);
				}

				m_pMenuParent->m_nSelectedItem = nPosition;
				m_pMenuParent->m_eSelectedItemSelType = eON;
				m_pMenuParent->m_pSelectedItem = m_pParentMenuItem;

				m_pMenuParent->InvalidateMenuItemRect(m_pParentMenuItem);

			}
			
		}
	}
}

CWnd *CDropMenu::GetTopLevelParent(void)
{

	CDropMenu *pMenu = this;

	while(pMenu->m_pMenuParent != NULL)
	{
		pMenu = pMenu->m_pMenuParent;
	}

	return pMenu->m_pParent;
}

void CDropMenu::LoadComplete(HT_Resource r)
{
	// Need to invalidate the line that corresponds to this particular resource.
	HT_Resource parent = HT_GetParent(r);
	if (parent)
	{
		// The offset of this node from its direct parent determines the line that should be
		// invalidated.
		int childIndex = HT_GetNodeIndex(HT_GetView(r), r);
		int parentIndex = HT_GetNodeIndex(HT_GetView(parent), parent);

		if (parent == HT_TopNode(HT_GetView(r)))
			parentIndex = -1;

		// Need to count up from the parent index to the child index and only include nodes
		// with indentation equal to the child index.
		int indentation = HT_GetItemIndentation(r);
		int offset = 0;
		for (int i = parentIndex + 1; i < childIndex; i++)
		{
			int otherIndentation = HT_GetItemIndentation(HT_GetNthItem(HT_GetView(r), i));
			if (otherIndentation == indentation)
				offset++;
		}

		CDropMenuItem *item = (CDropMenuItem*)m_pMenuItemArray[offset];
		InvalidateMenuItemIconRect(item);
	}
}
