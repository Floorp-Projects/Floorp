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
#include "dropmenu.h"
#include "mailqf.h"
#include "wfemsg.h"
#include "mailmisc.h"
#include "mailfrm.h"

/////////////////////////////////////////////////////////////////////
// CMailQFMenuDropTarget

class CMailQFMenuDropTarget: public CDropMenuDropTarget
{
public:
	CMailQFMenuDropTarget(CWnd *pOwner);
	 
protected:
	virtual DROPEFFECT OnDragEnter(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point );
	virtual BOOL OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point);
	
};

/////////////////////////////////////////////////////////////////////
// CMailQFButton

CMailQFButton::CMailQFButton(): CMailQFButtonParent()
{
	m_pDropMenu = new CMailQFDropMenu;
	m_hFolderBitmap = NULL;
	m_hSelectedFolderBitmap = NULL;
}

CMailQFButton::~CMailQFButton()
{
	m_pDropMenu->DestroyDropMenu();
	delete m_pDropMenu;
	if(m_hFolderBitmap)
		DeleteObject(m_hFolderBitmap);
}

BOOL CMailQFButton::Create(const CRect & rect, CWnd* pwndParent, UINT uID)
{
	CString csLabel, csTip, csStatus;
	
	WFE_ParseButtonString( ID_MESSAGE_FILE, csStatus, csTip, csLabel );
	
	BOOL bRtn = CMailQFButtonParent::Create(pwndParent, theApp.m_pToolbarStyle, CSize(50, 40), CSize(30, 27), 
					csLabel, csTip, csStatus, 
					IDR_MAILTHREAD, 4, CSize(23, 21), TRUE, uID, 10);

	m_DropTarget.Register(this);
	
	if(bRtn){
		HINSTANCE hInst = AfxGetResourceHandle();
		HDC hDC = ::GetDC(m_hWnd);
		WFE_InitializeUIPalette(hDC);
		m_hFolderBitmap = WFE_LoadTransparentBitmap(hInst, hDC, sysInfo.m_clrMenu, RGB(255, 0, 255),
											 WFE_GetUIPalette(GetParentFrame()), IDB_MAILFOLDER);
		m_hSelectedFolderBitmap = WFE_LoadTransparentBitmap(hInst, hDC,sysInfo.m_clrHighlight,
													 RGB(255, 0, 255), WFE_GetUIPalette(GetParentFrame()), IDB_MAILFOLDEROPEN);

		::ReleaseDC(m_hWnd, hDC);
	}

	return(bRtn);

}


void CMailQFButton::OnPaletteChanged( CWnd* pFocusWnd )
{
	if (pFocusWnd != this) {
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
		HDC hDC = ::GetDC(m_hWnd);
		HPALETTE hOldPalette = ::SelectPalette(hDC, hPalette, TRUE);
		::SelectPalette(hDC, hOldPalette, FALSE);
		::ReleaseDC(m_hWnd, hDC);
		Invalidate();
	}
}


void CMailQFButton::OnAction()
{
	OnBtnClicked(FALSE);
}

void CMailQFButton::BuildMenu( MSG_FolderInfo *folderInfo, CDropMenu *pMenu, UINT &nID )
{
	MSG_Master *master = WFE_MSGGetMaster();
	int32 iCount = MSG_GetFolderChildren( master, folderInfo, NULL, 0 );

	MSG_FolderInfo **folderInfos = new MSG_FolderInfo*[iCount];
	if ( iCount && folderInfos ) {
		MSG_GetFolderChildren( master, folderInfo, folderInfos, iCount );

		for ( int i = 0; i < iCount; i++ ) {
			MSG_FolderLine folderLine;
			
			if ( MSG_GetFolderLineById( master, folderInfos[i], &folderLine ) ) {
				if ( folderLine.numChildren > 0 ) {
					CDropMenu *pNewMenu = new CMailQFDropMenu;
					pMenu->AppendMenu(MF_POPUP, nID, pNewMenu, FALSE, folderLine.name, FALSE, m_hFolderBitmap, m_hSelectedFolderBitmap);
					nID++;
					BuildMenu( folderInfos[i], pNewMenu, nID );
				} else {
					pMenu->AppendMenu(MF_STRING, nID, folderLine.name, FALSE, m_hFolderBitmap, m_hSelectedFolderBitmap);
					nID++;
				}
			}
		}
	}
}

void CMailQFButton::BuildMenu()
{
	// *** If you change the way this menu is built, you also need to
	// change CMailNewsFrame::FolderInfoFromMenuID and make it the same
	// as CMailNewsFrame::UpdateMenu ***

	int nCount = m_pDropMenu->GetMenuItemCount();
	// clean out the menu before adding to it
	for(int i = nCount - 1; i >= 0; i--)
	{
		m_pDropMenu->DeleteMenu(i, MF_BYPOSITION);
	}

	UINT nID = FIRST_MOVE_MENU_ID;

	int32 iLines = MSG_GetFolderChildren (WFE_MSGGetMaster(), NULL, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];
	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (WFE_MSGGetMaster(), NULL, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (WFE_MSGGetMaster(), ppFolderInfo[i], &folderLine)) {
				if ( folderLine.flags & MSG_FOLDER_FLAG_MAIL ) {
					if (nID > FIRST_MOVE_MENU_ID)
						m_pDropMenu->AppendMenu(MF_SEPARATOR, 0, "", FALSE, NULL);
					BuildMenu( ppFolderInfo[i], m_pDropMenu, nID );
				}
			}
		}
		delete ppFolderInfo;
	}
}

BEGIN_MESSAGE_MAP( CMailQFButton, CMailQFButtonParent)
	ON_MESSAGE( QF_DRAGGINGBOOKMARK, OnQuickfileDrag )
	ON_MESSAGE( DM_DROPOCCURRED, OnDrop )
#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE(FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnMoveOrCopy )
	ON_COMMAND_RANGE(FIRST_COPY_MENU_ID, LAST_COPY_MENU_ID, OnMoveOrCopy )
	ON_WM_SYSCOLORCHANGE()
#endif
END_MESSAGE_MAP()

#ifndef ON_COMMAND_RANGE

BOOL CMailQFButton::OnCommand( WPARAM wParam, LPARAM lParam )
{
	UINT nID = wParam;

	if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
		OnMoveOrCopy( nID );
		return TRUE;
	}
	if ( nID >= FIRST_COPY_MENU_ID && nID <= LAST_COPY_MENU_ID ) {
		OnMoveOrCopy( nID );
		return TRUE;
	}
	return CMailQFButtonParent::OnCommand( wParam, lParam );
}

#endif

void CMailQFButton::OnBtnClicked(BOOL bDragging)
{
	RECT rc;
	GetWindowRect(&rc);
	
	if (!m_bMenuShowing) {
		BuildMenu( );

		CMailQFMenuDropTarget *dropTarget = new CMailQFMenuDropTarget(m_pDropMenu);

		m_bMenuShowing = TRUE;
		m_pDropMenu->TrackDropMenu(this, rc.left, rc.bottom+1, bDragging, dropTarget);
	}
}

LRESULT CMailQFButton::OnQuickfileDrag(WPARAM wParam, LPARAM lParam)
{
	if ( m_eState != eBUTTON_DOWN ) {
		//keep button feedback in mousedown state
		m_eState = eBUTTON_DOWN;
		RedrawWindow();

		OnBtnClicked(TRUE);
	}
	return 1;
}

LRESULT CMailQFButton::OnDrop(WPARAM wParam, LPARAM lParam)
{
	LRESULT res = 0;

	UINT nID = LOWORD(lParam);

	// Dig out the frame that initiated the drag.-
	HGLOBAL hData = (HGLOBAL) wParam;
	MailNewsDragData *pData = (MailNewsDragData *) GlobalLock( hData );

	LPMAILFRAME pMailFrame = NULL;
	LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pData->m_pane );

	if ( pUnk ) {
		pUnk->QueryInterface( IID_IMailFrame, (LPVOID *) &pMailFrame );

		if (pMailFrame) {
			CMailNewsFrame *pFrame = pMailFrame->GetMailNewsFrame();

			if ( pFrame ) {
				res = pFrame->SendMessage( WM_COMMAND, (WPARAM) nID, 0 );
			}
		}
	}

	GlobalUnlock( hData );
	return res;
}

void CMailQFButton::OnMoveOrCopy( UINT nID )
{
	GetParentFrame()->SendMessage( WM_COMMAND, (WPARAM) nID, 0 );
}

void CMailQFButton::OnSysColorChange( )
{
	if(m_hFolderBitmap)
	{
		VERIFY(::DeleteObject(m_hFolderBitmap));
		VERIFY(::DeleteObject(m_hSelectedFolderBitmap));
		
		HINSTANCE hInstance = AfxGetResourceHandle();
		HDC hDC = ::GetDC(m_hWnd);
		WFE_InitializeUIPalette(hDC);

		m_hFolderBitmap = WFE_LoadTransparentBitmap(hInstance, hDC, sysInfo.m_clrMenu, RGB(255, 0, 255),
											 WFE_GetUIPalette(GetParentFrame()), IDB_BOOKMARK_FOLDER2);
		m_hSelectedFolderBitmap = WFE_LoadTransparentBitmap(hInstance, hDC,sysInfo.m_clrHighlight,
													 RGB(255, 0, 255), WFE_GetUIPalette(GetParentFrame()), IDB_BOOKMARK_FOLDER2);

		::ReleaseDC(m_hWnd, hDC);
	}

	CMailQFButtonParent::OnSysColorChange();
}
/////////////////////////////////////////////////////////////////////
// CMailQFDropTarget

DROPEFFECT CMailQFDropTarget::OnDragEnter(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	// Only interested in bookmarks
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT)))
	{
		pWnd->SendMessage(QF_DRAGGINGBOOKMARK, 0, 0);
	}
	return(deReturn);

} // END OF	FUNCTION CMailQFDropTarget::OnDragEnter()

DROPEFFECT CMailQFDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	return(deReturn);

} // END OF	FUNCTION CMailQFDropTarget::OnDragOver()

BOOL CMailQFDropTarget::OnDrop(CWnd * pWnd,
	COleDataObject * pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	return(bRtn);
	
} // END OF	FUNCTION CMailQFDropTarget::OnDrop()

/////////////////////////////////////////////////////////////////////
// CMailQFDropMenu

CDropMenuDropTarget *CMailQFDropMenu::GetDropMenuDropTarget(CWnd *pOwner)
{
	return new CMailQFMenuDropTarget(pOwner);
}

BEGIN_MESSAGE_MAP( CMailQFDropMenu, CMailQFDropMenuParent )
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

void CMailQFDropMenu::OnLButtonUp(UINT nFlags, CPoint point)
{
	MenuSelectionType eSelType;
	int nSelection = FindSelection(point, eSelType);

	if(nSelection != -1)
	{
		CDropMenuItem *pItem = (CDropMenuItem *)m_pMenuItemArray[nSelection];
#ifdef _WIN32	
		SendMessage(WM_COMMAND, MAKEWPARAM(pItem->GetCommand(), 0), 0);
#else
		SendMessage(WM_COMMAND, (WPARAM)pItem->GetCommand(), MAKELPARAM( m_hWnd, 0) );
#endif
	}
}


///////////////////////////////////////////////////////////////////////////
// CMailQFMenuDropTarget

CMailQFMenuDropTarget::CMailQFMenuDropTarget(CWnd *pOwner): CDropMenuDropTarget( pOwner )
{
}


DROPEFFECT CMailQFMenuDropTarget::OnDragEnter(CWnd * pWnd,	COleDataObject * pDataObject,
											   DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks now
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT)))
	{
		deReturn = DROPEFFECT_MOVE;
	}

	return(deReturn);
	
}

DROPEFFECT CMailQFMenuDropTarget::OnDragOver(CWnd * pWnd, COleDataObject * pDataObject,
											  DWORD dwKeyState, CPoint point )
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;

	// Only interested in bookmarks now
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT)))
	{
		m_pOwner->SendMessage(DT_DRAGGINGOCCURRED, (WPARAM) 0, MAKELPARAM(point.x, point.y));
		deReturn = DROPEFFECT_MOVE;
	}

	return(deReturn);
	
}

BOOL CMailQFMenuDropTarget::OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	// Only interested in bookmarks now 
    CLIPFORMAT cfMessage = ::RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT);
	if (pDataObject->IsDataAvailable(cfMessage))
	{
		HGLOBAL hContent = pDataObject->GetGlobalData(cfMessage);
		pWnd->SendMessage(DT_DROPOCCURRED, (WPARAM) hContent, MAKELPARAM( point.x, point.y ) );
	}
	
	return(bRtn);
}
