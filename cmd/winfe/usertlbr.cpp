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

#include "stdafx.h"
#include "usertlbr.h"
#include "shcut.h"
#include "dropmenu.h"
#include "prefapi.h"
#include "rdfliner.h"

extern "C" {
#include "xpgetstr.h"
};

#define LEFT_TOOLBAR_MARGIN 10
#define RIGHT_TOOLBAR_MARGIN 20
#define SPACE_BETWEEN_BUTTONS 2

#define MAX_TOOLBAR_BUTTONS 60
#define MAX_TOOLBAR_ROWS 3

int CLinkToolbar::m_nMinToolbarButtonChars = 15;
int CLinkToolbar::m_nMaxToolbarButtonChars = 30;

//////////////////////////////////////////////////////////////////////////////////////
//							CLinkToolbarButtonDropTarget
//////////////////////////////////////////////////////////////////////////////////////

DROPEFFECT CLinkToolbarButtonDropTarget::ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{

	// Drop target is only used for drops directly onto folder personal toolbar buttons.
	// This drop target is NOT used for the Aurora selector bar drops.
	return ProcessDragOver(pWnd, pDataObject, dwKeyState, point);
}

DROPEFFECT CLinkToolbarButtonDropTarget::ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{
	CLinkToolbarButton* pButton = (CLinkToolbarButton*)m_pButton;

	// Treat as a drop onto the folder.
	return RDFGLOBAL_TranslateDropAction(pButton->GetNode(), pDataObject, 2);
}

BOOL CLinkToolbarButtonDropTarget::ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point)
{
	// Treat as a drop onto the folder.
	CLinkToolbarButton* pButton = (CLinkToolbarButton*)m_pButton;
	RDFGLOBAL_PerformDrop(pDataObject, pButton->GetNode(), 2);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////
//							CLinkToolbarButton
//////////////////////////////////////////////////////////////////////////////////////
static HBITMAP m_hbmpBM = NULL;
static HBITMAP m_hbmpBMSelected = NULL;
static HBITMAP m_hbmpFolder = NULL;
static HBITMAP m_hbmpSelectedFolder = NULL;
static HBITMAP m_hbmpFolderOpen =  NULL;
static int nBitmapRefCount = 0;

CLinkToolbarButton::CLinkToolbarButton()
{
	m_bShouldShowRMMenu = TRUE;

	m_Node = NULL;

	m_uCurBMID = 0;

    currentRow = 0;
}


CLinkToolbarButton::~CLinkToolbarButton()
{
	if(m_Node && HT_IsContainer(m_Node))
	{
		nBitmapRefCount--;

		if(nBitmapRefCount == 0)
		{
			if(m_hbmpBM)
			{
				DeleteObject(m_hbmpBM);
			}

			if(m_hbmpFolder)
			{
				DeleteObject(m_hbmpFolder);
			}

			if(m_hbmpSelectedFolder)
			{
				DeleteObject(m_hbmpSelectedFolder);
			}

			if(m_hbmpFolderOpen)
			{
				DeleteObject(m_hbmpFolderOpen);
			}

			if(m_hbmpBMSelected)
			{
				DeleteObject(m_hbmpBMSelected);
			}
		}

	}
}

int CLinkToolbarButton::Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
								LPCTSTR pButtonText, LPCTSTR pToolTipText,
								LPCTSTR pStatusText, 
								CSize bitmapSize, int nMaxTextChars, int nMinTextChars, BOOKMARKITEM bookmark,
								HT_Resource pNode, DWORD dwButtonStyle )
{
	
	m_bookmark = bookmark;

	char *protocol = NULL;

	BOOL bResult = CToolbarButton::Create(pParent, nToolbarStyle, noviceButtonSize, advancedButtonSize,
		pButtonText, pToolTipText, pStatusText, 0, 0,
		bitmapSize, TRUE, 0, nMaxTextChars, nMinTextChars, dwButtonStyle);

	if(bResult)
	{
		SetNode(pNode);
		m_nIconType = DetermineIconType(pNode, UseLargeIcons());

		m_nBitmapID = GetBitmapID();
		m_nBitmapIndex = GetBitmapIndex();

		HDC hDC = ::GetDC(m_hWnd);

		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
		HBITMAP hBitmap = WFE_LookupLoadAndEnterBitmap(hDC, m_nBitmapID, TRUE, hPalette,
													   sysInfo.m_clrBtnFace, RGB(255, 0, 255));
		::ReleaseDC(m_hWnd, hDC);

		SetBitmap(hBitmap, TRUE);
		
		if (m_menu.m_hMenu == NULL || (m_menu.m_hMenu != NULL && !IsMenu(m_menu.m_hMenu)))
          m_menu.CreatePopupMenu();
	}

	return bResult;
}

CSize CLinkToolbarButton::GetMinimalButtonSize(void)
{
    CString szText(HT_GetNodeName(m_Node));
    return GetButtonSizeFromChars(szText, m_nMinTextChars);
}

CSize CLinkToolbarButton::GetMaximalButtonSize(void)
{
    CString szText(HT_GetNodeName(m_Node));
    return GetButtonSizeFromChars(szText, m_nMaxTextChars);
}

void CLinkToolbarButton::OnAction(void)
{
	if(m_Node && !HT_IsContainer(m_Node))
	{
			CAbstractCX * pCX = FEU_GetLastActiveFrameContext();
			ASSERT(pCX != NULL);
			if (pCX != NULL)
			{
				pCX->NormalGetUrl((LPTSTR)HT_GetNodeURL(m_Node));
			}
	}
}

void CLinkToolbarButton::FillInOleDataSource(COleDataSource *pDataSource)
{
	CLIPFORMAT cfHTNode = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_HTNODE_FORMAT);
    HANDLE hString = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,1);
    pDataSource->CacheGlobalData(cfHTNode, hString);

	// Need to "select" the button in the view.  Hack.
	HT_SetSelection(m_Node);

	// Now the view is sufficient
	RDFGLOBAL_BeginDrag(pDataSource, HT_GetView(m_Node));
	
}

BOOKMARKITEM CLinkToolbarButton::GetBookmarkItem(void)
{
	return m_bookmark;
}

void CLinkToolbarButton::SetBookmarkItem(BOOKMARKITEM bookmark)
{
/*	m_bookmark = bookmark;
    CString strText(bookmark.szText);
	CToolbarButton::SetText(strText);
	*/
}

void CLinkToolbarButton::SetNode(HT_Resource pNode, BOOL bAddRef)
{
	if (pNode == NULL)
		return;

	m_Node = pNode;
	
	// if it's a header and we haven't already loaded the bitmaps, load the bitmaps
	if(HT_IsContainer(pNode) && bAddRef)
	{	
		HDC hDC = ::GetDC(m_hWnd);

		HINSTANCE hInstance = AfxGetResourceHandle();
		WFE_InitializeUIPalette(hDC);
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
		if(nBitmapRefCount == 0)
		{
			m_hbmpBM = WFE_LoadTransparentBitmap(hInstance, hDC, sysInfo.m_clrMenu, RGB(255, 0, 255),
												 hPalette, IDB_BOOKMARK_ITEM);
			m_hbmpBMSelected = WFE_LoadTransparentBitmap(hInstance, hDC,sysInfo.m_clrHighlight,
														 RGB(255, 0, 255), hPalette, IDB_BOOKMARK_ITEM);
			m_hbmpFolder = WFE_LoadTransparentBitmap(hInstance, hDC,sysInfo.m_clrMenu, RGB(255, 0, 255),
													 hPalette, IDB_BOOKMARK_FOLDER2);

			m_hbmpSelectedFolder = WFE_LoadTransparentBitmap(hInstance, hDC, sysInfo.m_clrHighlight, RGB(255, 0, 255),
													 hPalette, IDB_BOOKMARK_FOLDER2);

			m_hbmpFolderOpen = WFE_LoadTransparentBitmap(hInstance, hDC, sysInfo.m_clrHighlight,
														 RGB(255, 0, 255), hPalette, IDB_BOOKMARK_FOLDER_OPEN);
		}
		nBitmapRefCount++;

		::ReleaseDC(m_hWnd, hDC);
	}

}

void CLinkToolbarButton::EditTextChanged(char *pText)
{
	if (pText)
	{
		HT_SetNodeName(m_Node, pText);
		SetText(pText);	
		SetToolTipText(pText);
		delete []pText;
	}
    RemoveTextEdit();

	((CLinkToolbar*)GetParent())->LayoutButtons(-1);
}

void CLinkToolbarButton::DrawPicturesAndTextMode(HDC hDC, CRect rect)
{
	 DrawBitmapOnSide(hDC, rect);
}

void CLinkToolbarButton::DrawPicturesMode(HDC hDC, CRect rect)
{
	DrawBitmapOnSide(hDC, rect);
}

void CLinkToolbarButton::DrawButtonText(HDC hDC, CRect rcTxt, CSize sizeTxt, CString strTxt)
{
    CToolbarButton::DrawButtonText(hDC, rcTxt, sizeTxt, strTxt);
}

CSize CLinkToolbarButton::GetButtonSizeFromChars(CString s, int c)
{
    if(m_nToolbarStyle != TB_TEXT)
		return(GetBitmapOnSideSize(s, c));
	else
		return(GetTextOnlySize(s, c));
}

void CLinkToolbarButton::GetPicturesAndTextModeTextRect(CRect &rect)
{
	GetBitmapOnSideTextRect(rect);
}

void CLinkToolbarButton::GetPicturesModeTextRect(CRect &rect)
{
	GetBitmapOnSideTextRect(rect);
}


BOOL CLinkToolbarButton::CreateRightMouseMenu(void)
{
	if (m_bShouldShowRMMenu)
	{
		m_MenuCommandMap.Clear();
		HT_SetSelection(m_Node); // Make sure the node is the selection in the view.
		HT_View theView = HT_GetView(m_Node);
		HT_Cursor theCursor = HT_NewContextualMenuCursor(theView, PR_FALSE, PR_FALSE);
		if (theCursor != NULL)
		{
			// We have a cursor. Attempt to iterate
			HT_MenuCmd theCommand; 
			while (HT_NextContextMenuItem(theCursor, &theCommand))
			{
				char* menuName = HT_GetMenuCmdName(theCommand);
				if (theCommand == HT_CMD_SEPARATOR)
					m_menu.AppendMenu(MF_SEPARATOR);
				else
				{
					// Add the command to our command map
					CRDFMenuCommand* rdfCommand = new CRDFMenuCommand(menuName, theCommand);
					int index = m_MenuCommandMap.AddCommand(rdfCommand);
					m_menu.AppendMenu(MF_ENABLED, index+FIRST_HT_MENU_ID, menuName);
				}
			}
			HT_DeleteCursor(theCursor);
		}
	}
	return m_bShouldShowRMMenu;
}

CWnd* CLinkToolbarButton::GetMenuParent(void)
{
	return this;
}

BOOL CLinkToolbarButton::OnCommand(UINT wParam, LONG lParam)
{
	BOOL bRtn = TRUE;
	
	if (wParam >= FIRST_HT_MENU_ID && wParam <= LAST_HT_MENU_ID)
	{
		// A selection was made from the context menu.
		// Use the menu map to get the HT command value
		CRDFMenuCommand* theCommand = (CRDFMenuCommand*)(m_MenuCommandMap.GetCommand((int)wParam-FIRST_HT_MENU_ID));
		if (theCommand)
		{
			HT_MenuCmd htCommand = theCommand->GetHTCommand();
			HT_DoMenuCmd(HT_GetPane(GetHTView()), htCommand);
		}

		return bRtn;
	}

	if(wParam == ID_HOTLIST_VIEW || wParam == ID_HOTLIST_ADDCURRENTTOHOTLIST)
	{
		GetParentFrame()->SendMessage(WM_COMMAND, wParam, lParam);
	}
	else if(wParam == ID_DELETE_BUTTON)
	{
		HT_RemoveChild(HT_GetParent(m_Node), m_Node);
	}
	else if(wParam == ID_BUTTON_PROPERTIES)
	{
		// PROPERTIES
	}
	else if(wParam == ID_RENAME_BUTTON)
	{
		AddTextEdit();
		SetTextEditText(m_pButtonText);
	}
	// Only interested in commands from bookmark quick file menu
	else if ((wParam >= FIRST_BOOKMARK_MENU_ID))
	{
		void * pBookmark = NULL;
		m_BMMenuMap.Lookup(wParam, pBookmark);
		if (pBookmark != NULL)
		{
			HT_Resource theNode = (HT_Resource)pBookmark;
			CAbstractCX *pCX = FEU_GetLastActiveFrameContext();
			ASSERT(pCX != NULL);
			if (pCX != NULL)
			{
				pCX->NormalGetUrl((LPTSTR)HT_GetNodeURL(theNode));
			}
		}
	}
	else 
	{
		bRtn = CLinkToolbarButtonBase::OnCommand(wParam, lParam);
	}
	
	return(bRtn);
	
} 

///////////////////////////////////////////////////////////////////////////////////
//									CLinkToolbarButton Messages
///////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CLinkToolbarButton, CLinkToolbarButtonBase)
	//{{AFX_MSG_MAP(CLinkToolbarButton)
	ON_MESSAGE(NSDRAGMENUOPEN, OnDragMenuOpen) 
	ON_MESSAGE(DM_FILLINMENU, OnFillInMenu)
	ON_MESSAGE(DT_DROPOCCURRED, OnDropMenuDropOccurred)
	ON_MESSAGE(DT_DRAGGINGOCCURRED, OnDropMenuDraggingOccurred)
	ON_MESSAGE(DM_MENUCLOSED, OnDropMenuClosed)
	ON_WM_SYSCOLORCHANGE()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

LRESULT CLinkToolbarButton::OnDragMenuOpen(WPARAM wParam, LPARAM lParam)
{
// Set our drop menu's user data.
	if (m_pDropMenu)
		m_pDropMenu->SetUserData(m_Node);

	if(m_Node == NULL || !HT_IsContainer(m_Node))
		return 1;
	m_uCurBMID = FIRST_BOOKMARK_MENU_ID;

	m_pCachedDropMenu = (CDropMenu *)lParam;  // Set our drop menu
	PRBool isOpen;

	HT_Resource theNode = (HT_Resource)m_pCachedDropMenu->GetUserData();
	HT_GetOpenState(theNode, &isOpen);
	if (isOpen)
		FillInMenu(theNode);
	else HT_SetOpenState(theNode, (PRBool)TRUE);

	return 1;

}

LRESULT CLinkToolbarButton::OnFillInMenu(WPARAM wParam, LPARAM lParam)
{
// Set our drop menu's user data.
	if (m_pDropMenu)
		m_pDropMenu->SetUserData(m_Node);

	m_pCachedDropMenu = (CDropMenu *)lParam;  // Set our drop menu
	PRBool isOpen;
	HT_Resource theNode = (HT_Resource)m_pCachedDropMenu->GetUserData();
	HT_GetOpenState(theNode, &isOpen);
	if (isOpen)
		FillInMenu(theNode);
	else HT_SetOpenState(theNode, (PRBool)TRUE);
	
	return 1;
}

LRESULT CLinkToolbarButton::OnDropMenuDraggingOccurred(WPARAM wParam, LPARAM lParam)
{
	CDropMenuDragData* pData = (CDropMenuDragData*)lParam;
	UINT nCommand = pData->m_nCommand;
	MenuSelectionType eSelType = pData->eSelType;

	void * pBookmarkInsertAfter = NULL;
	m_BMMenuMap.Lookup(nCommand, pBookmarkInsertAfter);
	HT_Resource theNode = (HT_Resource)pBookmarkInsertAfter;

	int dragFraction = 2;
	if (eSelType == eON)
		dragFraction = 2;
	else if (eSelType == eBELOW)
		dragFraction = 3;
	else if (eSelType == eABOVE)
		dragFraction = 1;

	// Next we get the result.
	DROPEFFECT answer = RDFGLOBAL_TranslateDropAction(theNode, pData->m_pDataSource, dragFraction);
	
	// Place the result into our data structure for the droptarget to use.
	pData->m_DropEffect = answer;
	
	return 1;

}

LRESULT CLinkToolbarButton::OnDropMenuDropOccurred(WPARAM wParam, LPARAM lParam)
{
	CDropMenuDragData* pData = (CDropMenuDragData*)lParam;
	UINT nCommand = pData->m_nCommand;
	MenuSelectionType eSelType = pData->eSelType;

	void * pBookmarkInsertAfter = NULL;
	m_BMMenuMap.Lookup(nCommand, pBookmarkInsertAfter);
	HT_Resource theNode = (HT_Resource)pBookmarkInsertAfter;

	int dragFraction = 2;
	if (eSelType == eON)
		dragFraction = 2;
	else if (eSelType == eBELOW)
		dragFraction = 3;
	else if (eSelType == eABOVE)
		dragFraction = 1;

	RDFGLOBAL_PerformDrop(pData->m_pDataSource, theNode, dragFraction);
	
	delete pData; // Clean this structure up.

	return 1;
}
	
LRESULT CLinkToolbarButton::OnDropMenuClosed(WPARAM wParam, LPARAM lParam)
{
	int nCount;
	if(m_pDropMenu != NULL)
	{
		HT_Resource theNode = (HT_Resource)m_pDropMenu->GetUserData();
		nCount = m_pDropMenu->GetMenuItemCount();

		// clean out the menu 
		for(int i = nCount - 1; i >= 0; i--)
		{
			m_pDropMenu->DeleteMenu(i, MF_BYPOSITION);
		}
		m_pDropMenu->DestroyDropMenu();
		delete m_pDropMenu;
		m_pDropMenu = NULL;

		if (theNode != NULL)
			HT_SetOpenState(theNode, (PRBool)FALSE);
	}
	
	return 1;
}

void CLinkToolbarButton::OnSysColorChange( )
{
	if(nBitmapRefCount > 0)
	{
		VERIFY(::DeleteObject(m_hbmpBM));
		VERIFY(::DeleteObject(m_hbmpBMSelected));
		VERIFY(::DeleteObject(m_hbmpFolder));
		VERIFY(::DeleteObject(m_hbmpSelectedFolder));
		VERIFY(::DeleteObject(m_hbmpFolderOpen));
		
		HINSTANCE hInstance = AfxGetResourceHandle();
		HDC hDC = ::GetDC(m_hWnd);
		WFE_InitializeUIPalette(hDC);
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());

		m_hbmpBM = WFE_LoadTransparentBitmap(hInstance, hDC, sysInfo.m_clrMenu, RGB(255, 0, 255),
											 hPalette, IDB_BOOKMARK_ITEM);
		m_hbmpBMSelected = WFE_LoadTransparentBitmap(hInstance, hDC,sysInfo.m_clrHighlight,
													 RGB(255, 0, 255), hPalette, IDB_BOOKMARK_ITEM);
		m_hbmpFolder = WFE_LoadTransparentBitmap(hInstance, hDC,sysInfo.m_clrMenu, RGB(255, 0, 255),
												 hPalette, IDB_BOOKMARK_FOLDER2);

		m_hbmpSelectedFolder = WFE_LoadTransparentBitmap(hInstance, hDC,sysInfo.m_clrHighlight, RGB(255, 0, 255),
												 hPalette, IDB_BOOKMARK_FOLDER2);

		m_hbmpFolderOpen = WFE_LoadTransparentBitmap(hInstance, hDC, sysInfo.m_clrHighlight,
														 RGB(255, 0, 255), hPalette, IDB_BOOKMARK_FOLDER_OPEN);

		::ReleaseDC(m_hWnd, hDC);
	}

	if(m_bBitmapFromParent)
	{
		HDC hDC = ::GetDC(m_hWnd);

		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());

		HBITMAP hBitmap = WFE_LookupLoadAndEnterBitmap(hDC, m_nBitmapID, TRUE, hPalette,
													   sysInfo.m_clrBtnFace, RGB(255, 0, 255));
		::ReleaseDC(m_hWnd, hDC);
		SetBitmap(hBitmap, TRUE);
	}
	CToolbarButton::OnSysColorChange();
}

///////////////////////////////////////////////////////////////////////////
//							CLinkToolbarButton Helpers
///////////////////////////////////////////////////////////////////////////

void CLinkToolbarButton::FillInMenu(HT_Resource theNode)
{
	m_pCachedDropMenu->CreateOverflowMenuItem(ID_HOTLIST_VIEW, CString(szLoadString(IDS_MORE_BOOKMARKS)), NULL, NULL );

	HT_Cursor theCursor = HT_NewCursor(theNode);
	HT_Resource theItem = NULL;
	while (theItem = HT_GetNextItem(theCursor))
	{
		IconType nIconType = DetermineIconType(theItem, FALSE);
		void* pCustomIcon = NULL;
		if (nIconType == LOCAL_FILE)
			pCustomIcon = FetchLocalFileIcon(theItem);
		else if (nIconType == ARBITRARY_URL)
			pCustomIcon = FetchCustomIcon(theItem, this, FALSE);

		HT_SetNodeFEData(theItem, this);

		if (HT_IsContainer(theItem))
		{
			CDropMenu *pSubMenu = new CDropMenu;
			pSubMenu->SetUserData(theItem);

			CString csAmpersandString = FEU_EscapeAmpersand(HT_GetNodeName(theItem));

			m_pCachedDropMenu->AppendMenu(MF_POPUP, m_uCurBMID, pSubMenu, FALSE, 
				csAmpersandString, TRUE, m_hbmpFolder, m_hbmpFolderOpen, pCustomIcon, nIconType);
			
			m_BMMenuMap.SetAt(m_uCurBMID, theItem);
			m_uCurBMID++;
		}
		else if (!HT_IsSeparator(theItem))
		{
			CString csAmpersandString = FEU_EscapeAmpersand(HT_GetNodeName(theItem));
			m_pCachedDropMenu->AppendMenu(MF_STRING, m_uCurBMID, csAmpersandString, TRUE, 
				m_hbmpBM, m_hbmpBMSelected, pCustomIcon, nIconType);
			m_BMMenuMap.SetAt(m_uCurBMID, theItem);
			m_uCurBMID++;
		}
		else
		{
			m_pCachedDropMenu->AppendMenu(MF_SEPARATOR, 0, TRUE, NULL, NULL);
		}
	}
	
	HT_DeleteCursor(theCursor);
	m_pCachedDropMenu = NULL;
}

UINT CLinkToolbarButton::GetBitmapID(void)
{
	if (m_nBitmapID != 0)
		return m_nBitmapID;

	if (m_Node && HT_IsContainer(m_Node))
		return IDB_BUTTON_FOLDER;
	else return IDB_USERBTN;
}

UINT CLinkToolbarButton::GetBitmapIndex(void)
{
	return 0;
}

void CLinkToolbarButton::SetTextWithoutResize(CString text)
{
    if(m_pButtonText != NULL)
	{
		XP_FREE(m_pButtonText);
	}

	m_pButtonText = (LPTSTR)XP_ALLOC(text.GetLength() +1);
	XP_STRCPY(m_pButtonText, text);
}

void CLinkToolbarButton::LoadComplete(HT_Resource r)
{
	Invalidate();
}

void CLinkToolbarButton::DrawCustomIcon(HDC hDC, int x, int y)
{
	int size = UseLargeIcons() ? 23 : 16;
	DrawArbitraryURL(m_Node, x, y, size, size, hDC, 
					m_bDepressed ? (::GetSysColor(COLOR_BTNSHADOW)) :  (::GetSysColor(COLOR_BTNFACE)), 
					 this, UseLargeIcons());
}


void CLinkToolbarButton::DrawLocalIcon(HDC hDC, int x, int y)
{
	if (m_Node)
		DrawLocalFileIcon(m_Node, x, y, hDC);
}


///////////////////////////////////////////////////////////////////////////
//							Class CLinkToolbarDropTarget
///////////////////////////////////////////////////////////////////////////

void CLinkToolbarDropTarget::Toolbar(CLinkToolbar *pToolbar)
{
	m_pToolbar = pToolbar;
}

DROPEFFECT CLinkToolbarDropTarget::OnDragEnter(CWnd * pWnd,	COleDataObject * pDataObject,
											   DWORD dwKeyState, CPoint point)
{
	return OnDragOver(pWnd, pDataObject, dwKeyState, point);	
}

DROPEFFECT CLinkToolbarDropTarget::OnDragOver(CWnd * pWnd, COleDataObject * pDataObject,
											  DWORD dwKeyState, CPoint point )
{
	int nStartX = 0;

	RECT buttonRect;

    int currentRow = 0;

	CLinkToolbarButton* pButton = NULL;

	for(int i = 0; i < m_pToolbar->GetNumButtons(); i++)
	{
        pButton = (CLinkToolbarButton*)(m_pToolbar->GetNthButton(i));
		pButton->GetClientRect(&buttonRect);
	    pButton->MapWindowPoints(m_pToolbar, &buttonRect);
        
		nStartX += (buttonRect.right - buttonRect.left) + SPACE_BETWEEN_BUTTONS;
        if (currentRow != pButton->GetRow())
        {
            currentRow++;
            nStartX = LEFT_TOOLBAR_MARGIN + (buttonRect.right - buttonRect.left) + SPACE_BETWEEN_BUTTONS;
        }

		if(point.x < nStartX && (point.y >= buttonRect.top && point.y <= buttonRect.bottom))
           break;

		pButton = NULL;
	}
	
	HT_Resource theNode = pButton ? pButton->GetNode() : HT_TopNode(HT_GetSelectedView(m_pToolbar->GetPane()));
	
	m_pToolbar->SetDragButton(pButton);

	if (pButton == NULL)
		m_pToolbar->SetDragFraction(2);
	else 
	{
		// Do the whole computation of drag fraction.  Cache our drag fraction and button.
		CRect rect;
		pButton->GetClientRect(&rect);
		if (HT_IsContainer(pButton->GetNode()))
		{
			if (point.x <= rect.Width()/3)
				m_pToolbar->SetDragFraction(1);
			else if (point.x <= 2*(rect.Width()/3))
				m_pToolbar->SetDragFraction(2);
			else m_pToolbar->SetDragFraction(3);
		}
		else if (point.x <= rect.Width()/2)
			m_pToolbar->SetDragFraction(1);
		else m_pToolbar->SetDragFraction(2);
	}

	return RDFGLOBAL_TranslateDropAction(theNode, pDataObject, m_pToolbar->GetDragFraction());
}

BOOL CLinkToolbarDropTarget::OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point)
{
	HT_Resource theNode = HT_TopNode(HT_GetSelectedView(m_pToolbar->GetPane()));
	if (m_pToolbar->GetDragButton())
	  theNode = m_pToolbar->GetDragButton()->GetNode();

	RDFGLOBAL_PerformDrop(pDataObject, theNode, m_pToolbar->GetDragFraction());
	
	return TRUE;
}

// End CLinkToolbarDropTarget implementation

///////////////////////////////////////////////////////////////////////////
//							Class CLinkToolbar
///////////////////////////////////////////////////////////////////////////
#define LINKTOOLBARHEIGHT 21
#define SPACE_BETWEEN_ROWS 2

// The Event Handler for HT notifications on the personal toolbar
static void ptNotifyProcedure (HT_Notification ns, HT_Resource n, HT_Event whatHappened) 
{
  CLinkToolbar* theToolbar = (CLinkToolbar*)ns->data;
  if (n != NULL)
  {
	HT_View theView = HT_GetView(n);
	if (theView != NULL)
	{
		if (whatHappened == HT_EVENT_NODE_OPENCLOSE_CHANGED)
		{
			PRBool openState;
			HT_GetOpenState(n, &openState);
			if (openState)
			{
				if (n == HT_TopNode(theView))
				{
					// Initial population of the toolbar. We should only receive this event once.
					theToolbar->FillInToolbar();
				}
				else 
				{
					// Toolbar button menu.  Populate it.
					CLinkToolbarButton* theButton = (CLinkToolbarButton*)(HT_GetNodeFEData(n));
					theButton->FillInMenu(n);
				}
			}
		}
		else if (whatHappened == HT_EVENT_VIEW_REFRESH)
		{
			theToolbar->LayoutButtons(-1);
		}
		else if (HT_TopNode(theView) == HT_GetParent(n))
		{
			// Aside from the opening/closing, we only respond to events that occurred on the top node of
			// the view or the immediate children (the buttons on the toolbar).
			if (whatHappened == HT_EVENT_NODE_DELETED_DATA ||
				 whatHappened == HT_EVENT_NODE_DELETED_NODATA)
			{
				// Delete the button
				if (HT_EVENT_NODE_DELETED_DATA == whatHappened)
				{
					// Destroy the toolbar button
					CLinkToolbarButton* pButton = (CLinkToolbarButton*)HT_GetNodeFEData(n);
					if (theToolbar->m_hWnd)
					  theToolbar->RemoveButton(pButton);
					else theToolbar->DecrementButtonCount();
					delete pButton;

				}
			}
			else if (whatHappened == HT_EVENT_NODE_ADDED)
			{
				theToolbar->AddHTButton(n);
				theToolbar->LayoutButtons(-1);
			}
			else if (whatHappened == HT_EVENT_NODE_VPROP_CHANGED)
			{
				CLinkToolbarButton* pButton = (CLinkToolbarButton*)HT_GetNodeFEData(n);
				pButton->SetText(HT_GetNodeName(n)); // Update our name.
				theToolbar->LayoutButtons(-1);
			}
		}
	}
  }
}

CLinkToolbar::CLinkToolbar(int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, int nPicturesHeight,
						   int nTextHeight)
	 : CNSToolbar2(nMaxButtons, nToolbarStyle, nPicturesAndTextHeight, nPicturesHeight, nTextHeight)
{
	m_nNumberOfRows = 1;
	
	// Construct the notification struct used by HT
	HT_Notification ns = new HT_NotificationStruct;
	ns->notifyProc = ptNotifyProcedure;
	ns->data = this;
	
	// Construct the pane and give it our notification struct
	m_PersonalToolbarPane = HT_NewPersonalToolbarPane(ns);
	if (m_PersonalToolbarPane)
		HT_SetPaneFEData(m_PersonalToolbarPane, this);
}

CLinkToolbar* CLinkToolbar::CreateUserToolbar(CWnd* pParent)
{
	// read in the maximum size we're allowing for personal toolbar items
	int32 nMaxToolbarButtonChars;
    int32 nMinToolbarButtonChars;

	if(PREF_GetIntPref("browser.personal_toolbar_button.max_chars", &nMaxToolbarButtonChars) ==
		PREF_ERROR)
		m_nMaxToolbarButtonChars = 30;
	else
		m_nMaxToolbarButtonChars = CASTINT(nMaxToolbarButtonChars);

    if(PREF_GetIntPref("browser.personal_toolbar_button.min_chars", &nMinToolbarButtonChars) ==
		PREF_ERROR)
		m_nMinToolbarButtonChars = 15;
	else
		m_nMinToolbarButtonChars = CASTINT(nMinToolbarButtonChars);

	CLinkToolbar* pToolbar = new CLinkToolbar(MAX_TOOLBAR_BUTTONS,theApp.m_pToolbarStyle, 43, 27, 27);

	if (pToolbar->Create(pParent))
	{
		pToolbar->SetButtonsSameWidth(FALSE);

		// Top node is already open.  Fill it in.
		pToolbar->FillInToolbar();
	}

	return pToolbar;
}

CLinkToolbar::~CLinkToolbar()
{
	if (m_PersonalToolbarPane)
	{
		HT_Pane oldPane = m_PersonalToolbarPane;
		m_PersonalToolbarPane = NULL;
		HT_DeletePane(oldPane);
	}
}

int CLinkToolbar::Create(CWnd *pParent)
{

	int result = CNSToolbar2::Create(pParent);

	if(!result)
		return FALSE;

	m_DropTarget.Register(this);
	m_DropTarget.Toolbar(this);

	DragAcceptFiles(FALSE);

	return result;
}

void CLinkToolbar::FillInToolbar()
{
	if (!m_PersonalToolbarPane)
		return;

	HT_View theView = HT_GetSelectedView(m_PersonalToolbarPane);
	if (theView == NULL)
		return;

	HT_Resource top = HT_TopNode(theView);
	HT_Cursor cursor = HT_NewCursor(top);
	if (cursor == NULL)
		return;

	HT_Resource item = NULL;
	while (item = HT_GetNextItem(cursor))
		AddHTButton(item);

	HT_DeleteCursor(cursor);

	LayoutButtons(-1);
}

void CLinkToolbar::AddHTButton(HT_Resource item)
{
	if (HT_IsSeparator(item))
		return;

	CLinkToolbarButton* pButton = new CLinkToolbarButton;
	BOOKMARKITEM bookmark;

	XP_STRCPY(bookmark.szText, HT_GetNodeName(item));
	XP_STRCPY(bookmark.szAnchor, HT_GetNodeURL(item));

	CString csAmpersandString = FEU_EscapeAmpersand(CString(bookmark.szText));

	pButton->Create(this, theApp.m_pToolbarStyle, CSize(60,42), CSize(85, 25), csAmpersandString,
					bookmark.szText, bookmark.szAnchor, CSize(23,17), 
					m_nMaxToolbarButtonChars, m_nMinToolbarButtonChars, bookmark,
					item,
                    (HT_IsContainer(item) ? TB_HAS_DRAGABLE_MENU | TB_HAS_IMMEDIATE_MENU : 0));
		
	if(HT_IsContainer(item))
	{
		CLinkToolbarButtonDropTarget *pDropTarget = new CLinkToolbarButtonDropTarget;
		pButton->SetDropTarget(pDropTarget);
	}

	HT_SetNodeFEData(item, pButton);

	AddButtonAtIndex(pButton); // Have to put the button in the array, since the toolbar base class depends on it.
}

int CLinkToolbar::GetHeight(void)
{
    return m_nNumberOfRows * (LINKTOOLBARHEIGHT + SPACE_BETWEEN_ROWS) + SPACE_BETWEEN_ROWS;
}



void CLinkToolbar::SetMinimumRows(int rowWidth)
{
    int rowCount = 1;
    int totalLine = 0;
    int rowSpace = rowWidth - RIGHT_TOOLBAR_MARGIN - LEFT_TOOLBAR_MARGIN;

    if (rowSpace <= 0)
    {
        SetRows(rowCount);
        return;
    }

	HT_Cursor cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
	if (!cursor)
		return;
	HT_Resource item;
    while (rowCount < MAX_TOOLBAR_ROWS && (item = HT_GetNextItem(cursor)))
	{
        // Get the current button
		CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
        if (!pButton) // Separator
			continue;

        CSize s = pButton->GetMinimalButtonSize();
        int tempTotal = totalLine + s.cx;
        if (tempTotal > rowSpace)
        {
            rowCount++;
            totalLine = s.cx;
        }
        else totalLine = tempTotal;

        totalLine += SPACE_BETWEEN_BUTTONS;
    }
	HT_DeleteCursor(cursor);

    SetRows(rowCount); 
}
    
void CLinkToolbar::ComputeLayoutInfo(CLinkToolbarButton* pButton, int numChars, int rowWidth, int& usedSpace)
{
   CString originalText = HT_GetNodeName(pButton->GetNode());

   int currCount = originalText.GetLength();
        
   // Start off at the maximal string
   CString strTxt = originalText;

   if (currCount > numChars)
   {
       strTxt = originalText.Left(numChars-3) + "...";
   }
   
   pButton->SetTextWithoutResize(strTxt);
   pButton->SetButtonSize(pButton->GetButtonSizeFromChars(strTxt, numChars));

// Determine how much additional padding we'll use to fill out a row if this button doesn't fit.
    int rowUsage = usedSpace % rowWidth;
    if (rowUsage == 0)
        rowUsage = rowWidth;
    int additionalPadding = rowWidth - rowUsage;
        
    int tempTotal = rowUsage + pButton->GetButtonSize().cx;

// The button doesn't fit.  Flesh out this row and start a new one.    
    if (tempTotal > rowWidth)
      usedSpace += additionalPadding;
    
// Add this button to the row.
    usedSpace += pButton->GetButtonSize().cx + SPACE_BETWEEN_BUTTONS;

// Set this button's row information, so it knows which row it is currently residing on.
    int currentRow = usedSpace/rowWidth;
    if (usedSpace % rowWidth == 0)
        currentRow--;

    pButton->SetRow(currentRow);

}

void CLinkToolbar::LayoutButtons(int nIndex)
{
    int width = m_nWidth;

    if (width <= 0)
    {
        CRect rect;
        GetParentFrame()->GetClientRect(&rect);
        width = rect.Width();
    }
    
    int numButtonsAtMin = 0;
    int numButtonsAtMax = 0;
    int idealSpace = 0;
   
// First quickly determine what the minimum # of rows we consume is.  This is our allowed space.
    int oldRows = GetRows();

    int rowWidth = width-RIGHT_TOOLBAR_MARGIN-LEFT_TOOLBAR_MARGIN;
    
    if (rowWidth <= 0 && m_nWidth > 0)
        rowWidth = m_nWidth - RIGHT_TOOLBAR_MARGIN - LEFT_TOOLBAR_MARGIN;

    SetMinimumRows(rowWidth);

    int newRows = GetRows();

    int allowedSpace = rowWidth * GetRows(); // Toolbar width * numRows
    int usedSpace = 0;

    int numChars = 0; // Start off trying to fit the whole thing on the toolbar.
    int minChars = 0;
	
	HT_Cursor cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
	if (!cursor)
		return;
	HT_Resource item;
    while ((item = HT_GetNextItem(cursor)))
	{
        // Get the current button
		CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
		if (!pButton)
			continue;

        if (numChars == 0)
        {
            numChars = pButton->GetMaxTextCharacters();
            minChars = pButton->GetMinTextCharacters();
        }

        // See how much this num chars takes up
        ComputeLayoutInfo(pButton, numChars, rowWidth, usedSpace);
    }
	HT_DeleteCursor(cursor);

    while (usedSpace > allowedSpace && numChars > minChars)
    {
        usedSpace = 0;
        numChars--;
        // Let's see what we can fit.
        HT_Cursor cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
		if (!cursor)
			return;
		HT_Resource item;
		while ((item = HT_GetNextItem(cursor)))
		{
			// Get the current button
			CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
			if (!pButton)  // Separator
				continue;

            // See how much this num chars takes up
            ComputeLayoutInfo(pButton, numChars, rowWidth, usedSpace);
        }
		HT_DeleteCursor(cursor);
    }

    // That's it.  lay them out with this number of characters.
    
    int nStartX = LEFT_TOOLBAR_MARGIN;
    int nStartY = SPACE_BETWEEN_ROWS;

    int row = 0;

	CSize buttonSize;
    CString strTxt;

	cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
	if (!cursor)
		return;
	while ((item = HT_GetNextItem(cursor)))
	{
        // Get the current button
		CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
        if (!pButton)  // Separator
			continue;

        buttonSize = pButton->GetButtonSize();  // The size we must be
        
        int tempTotal = nStartX + buttonSize.cx;
        if (tempTotal > (width - RIGHT_TOOLBAR_MARGIN))
        {
            nStartX = LEFT_TOOLBAR_MARGIN;
            nStartY += LINKTOOLBARHEIGHT + SPACE_BETWEEN_ROWS;
        }

		pButton->MoveWindow(nStartX, nStartY,
									  buttonSize.cx, buttonSize.cy);

		nStartX += buttonSize.cx + SPACE_BETWEEN_BUTTONS;
	}
	HT_DeleteCursor(cursor);

	//record the width of our toolbar
    //if (nStartY == SPACE_BETWEEN_ROWS && nStartX < (width - RIGHT_TOOLBAR_MARGIN))
	//  m_nWidth = nStartX;
    //else 
       m_nWidth = width;

    if (oldRows != newRows)
        GetParentFrame()->RecalcLayout();    
}

void CLinkToolbar::WidthChanged(int animWidth)
{
    CRect rect;
   
    GetParentFrame()->GetClientRect(&rect);
    int width = rect.Width() - animWidth;
    
    int numButtonsAtMin = 0;
    int numButtonsAtMax = 0;
    int idealSpace = 0;
   
// First quickly determine what the minimum # of rows we consume is.  This is our allowed space.
    int oldRows = GetRows();

    int rowWidth = width-RIGHT_TOOLBAR_MARGIN-LEFT_TOOLBAR_MARGIN;
    
    if (rowWidth <= 0 && m_nWidth > 0)
        rowWidth = m_nWidth - RIGHT_TOOLBAR_MARGIN - LEFT_TOOLBAR_MARGIN;

    SetMinimumRows(rowWidth);

    int newRows = GetRows();

    int allowedSpace = rowWidth * GetRows(); // Toolbar width * numRows
    int usedSpace = 0;

    int numChars = 0; // Start off trying to fit the whole thing on the toolbar.
    int minChars = 0;
	HT_Cursor cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
	if (!cursor)
		return;
	HT_Resource item;
    while ((item = HT_GetNextItem(cursor)))
	{
        // Get the current button
		CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
        if (!pButton)  // Separator
			continue;

        if (numChars == 0)
        {
            numChars = pButton->GetMaxTextCharacters();
            minChars = pButton->GetMinTextCharacters();
        }

        // See how much this num chars takes up
        ComputeLayoutInfo(pButton, numChars, rowWidth, usedSpace);
    }
	HT_DeleteCursor(cursor);

    while (usedSpace > allowedSpace && numChars > minChars)
    {
        usedSpace = 0;
        numChars--;
        // Let's see what we can fit.
        HT_Cursor cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
		if (!cursor)
			return;
		HT_Resource item;
		while ((item = HT_GetNextItem(cursor)))
		{
			// Get the current button
			CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
			if (!pButton)
				continue;

            // See how much this num chars takes up
            ComputeLayoutInfo(pButton, numChars, rowWidth, usedSpace);
        }
		HT_DeleteCursor(cursor);
    }

    // That's it.  lay them out with this number of characters.
    
    int nStartX = LEFT_TOOLBAR_MARGIN;
    int nStartY = SPACE_BETWEEN_ROWS;

    int row = 0;

	CSize buttonSize;
    CString strTxt;

	cursor = HT_NewCursor(HT_TopNode(HT_GetSelectedView(m_PersonalToolbarPane)));
	if (!cursor)
		return;
	
    while ((item = HT_GetNextItem(cursor)))
	{
        // Get the current button
		CLinkToolbarButton* pButton = (CLinkToolbarButton*)(HT_GetNodeFEData(item));
        if (!pButton)
			continue;

        buttonSize = pButton->GetButtonSize();  // The size we must be
        
        int tempTotal = nStartX + buttonSize.cx;
        if (tempTotal > (width - RIGHT_TOOLBAR_MARGIN))
        {
            nStartX = LEFT_TOOLBAR_MARGIN;
            nStartY += LINKTOOLBARHEIGHT + SPACE_BETWEEN_ROWS;
        }

		pButton->MoveWindow(nStartX, nStartY,
									  buttonSize.cx, buttonSize.cy);

		nStartX += buttonSize.cx + SPACE_BETWEEN_BUTTONS;
	}
	HT_DeleteCursor(cursor);


	m_nWidth = width;

    if (oldRows != newRows)
        GetParentFrame()->RecalcLayout();
}



///////////////////////////////////////////////////////////////////////////////////
//									CLinkToolbar Messages
///////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CLinkToolbar, CNSToolbar2)
	//{{AFX_MSG_MAP(CNSToolbar2)
	ON_WM_RBUTTONDOWN()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

void CLinkToolbar::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_MenuCommandMap.Clear();
	HT_View theView = HT_GetSelectedView(m_PersonalToolbarPane);
	HT_Cursor theCursor = HT_NewContextualMenuCursor(theView, PR_FALSE, PR_TRUE);
	CMenu menu;
	ClientToScreen(&point);
	if (menu.CreatePopupMenu() != 0 && theCursor != NULL)
	{
		// We have a cursor. Attempt to iterate
		HT_MenuCmd theCommand; 
		while (HT_NextContextMenuItem(theCursor, &theCommand))
		{
			char* menuName = HT_GetMenuCmdName(theCommand);
			if (theCommand == HT_CMD_SEPARATOR)
				menu.AppendMenu(MF_SEPARATOR);
			else
			{
				// Add the command to our command map
				CRDFMenuCommand* rdfCommand = new CRDFMenuCommand(menuName, theCommand);
				int index = m_MenuCommandMap.AddCommand(rdfCommand);
				menu.AppendMenu(MF_ENABLED, index+FIRST_HT_MENU_ID, menuName);
			}
		}
		HT_DeleteCursor(theCursor);

		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this, NULL);	

		menu.DestroyMenu();
	}
}

BOOL CLinkToolbar::OnCommand( WPARAM wParam, LPARAM lParam )
{
	if (wParam >= FIRST_HT_MENU_ID && wParam <= LAST_HT_MENU_ID)
	{
		// A selection was made from the context menu.
		// Use the menu map to get the HT command value
		CRDFMenuCommand* theCommand = (CRDFMenuCommand*)(m_MenuCommandMap.GetCommand((int)wParam-FIRST_HT_MENU_ID));
		if (theCommand)
		{
			HT_MenuCmd htCommand = theCommand->GetHTCommand();
			HT_DoMenuCmd(m_PersonalToolbarPane, htCommand);
		}
		return TRUE;
	}
	return((BOOL)GetParentFrame()->SendMessage(WM_COMMAND, wParam, lParam));
}
