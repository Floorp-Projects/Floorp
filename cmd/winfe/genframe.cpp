/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// genframe.cpp : implementation file
//

#include "stdafx.h"
#include "confhook.h"
#ifdef MOZ_MAIL_NEWS
#include "mailfrm.h"
#endif
#ifdef EDITOR
#include "edt.h"
#endif
#include "winclose.h"
#include "prefapi.h"
#ifdef MOZ_OFFLINE
#include "dlgdwnld.h"
#endif
#include "genchrom.h"
#include "intl_csi.h"
#include "quickfil.h"
#ifndef XP_WIN16
#include "navFram.h"
#endif

#include "property.h"

#ifdef _WIN32
#include "intelli.h"
#endif

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

// for whitebox testing
//#define DEBUG_WHITEBOX
#ifdef DEBUG_WHITEBOX
#include "TestcaseDlg.h"
#endif

extern void wfe_Progress(MWContext *pContext, const char *pMessage);

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

UINT NEAR WM_FINDREPLACE = ::RegisterWindowMessage(FINDMSGSTRING);
UINT NEAR WM_HELPMSG = ::RegisterWindowMessage(HELPMSGSTRING);

//CLM: Global in TEMPLATE.CPP - saves last frame location
//     for new window postioning - cleared in ::OnClose method here
extern CGenericFrame *wfe_pLastFrame;

//
// Bookmark stuff
// 

// This is used to remember the number of existing menu items on the
// bookmark menu before we add the bookmark items. We need this so we
// know how many menu items to delete when the menu goes away
static int      nExistingBookmarkMenuItems;

// We destroy the menu items we added when we receive the WM_MENUSELECT.
// Because Windows sends the WM_MENUSELECT that the menu is going away
// before they send the WM_COMMAND, we need to remember the last menu item
// selected and its data (i.e. URL address or History_entry*)
static UINT     nLastSelectedCmdID;
static void*    nLastSelectedData;

// Macro to use for graying toolbar and menu items
//  when we don't have a context or it is in "waiting" mode
// This is used by both Navigator and Composer frames,
//  so we check if Editor is blocked
#define CAN_INTERACT_IF_EDITOR  (GetMainContext() != NULL && GetMainContext()->GetContext() != NULL \
                       && !GetMainContext()->GetContext()->waitingMode  \
                       && (!EDT_IS_EDITOR(GetMainContext()->GetContext()) \
						   || !EDT_IsBlocked(GetMainContext()->GetContext())))

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CNSGenFrame, CFrameWnd)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

extern "C" void WFE_StartCalendar()
{
	if(!FEU_IsCalendarAvailable())
		return;

	CString installDirectory, executable;

#ifdef WIN32
	CString calRegistry;

	calRegistry.LoadString(IDS_CALENDAR_REGISTRY);

	calRegistry = FEU_GetCurrentRegistry(calRegistry);
	if(calRegistry.IsEmpty())
		return;

    installDirectory = FEU_GetInstallationDirectory(calRegistry, szLoadString(IDS_INSTALL_DIRECTORY));
	executable = szLoadString(IDS_CALENDAR32EXE);
#else ifdef XP_WIN16
    installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_CALENDAR),szLoadString(IDS_INSTALL_DIRECTORY));
	executable = szLoadString(IDS_CALENDAR16EXE);
#endif
    if(!installDirectory.IsEmpty())
	{
		executable = installDirectory + executable;

		if(	WinExec(executable, SW_SHOW) >= 31)
			return;
	}
	::MessageBox(NULL, szLoadString(IDS_CANTCALENDAR), szLoadString(IDS_CALENDAR), MB_OK);
}

/////////////////////////////////////////////////////////////////////////////
// CGenericFrame

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CGenericFrame, CNSGenFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

// The Event Handler
static void qfNotifyProcedure (HT_Notification ns, HT_Resource n, HT_Event whatHappened) 
{
	if (whatHappened == HT_EVENT_NODE_OPENCLOSE_CHANGED)
	{
		// The node was opened.
		PRBool openState;
		HT_GetOpenState(n, &openState);
		if (openState)
		{
			CGenericFrame* pFrame = (CGenericFrame*)ns->data;
			pFrame->FinishMenuExpansion(n);
		}
	}
}

CGenericFrame::CGenericFrame()
{
	// Construct the notification struct used by HT
	HT_Notification ns = new HT_NotificationStruct;
	ns->notifyProc = qfNotifyProcedure;
	ns->data = this;

	m_BookmarkMenuPane = theApp.m_bInGetCriticalFiles ? NULL : HT_NewQuickFilePane(ns);
	m_nBookmarkItems = 0;
	m_nFileBookmarkItems = 0;
    m_pHotlistMenuMap = new CMapWordToPtr();
	m_isClosing = FALSE;
#ifdef XP_WIN32
	m_pSubmenuMap = new CMapPtrToPtr();
#else
	m_pSubmenuMap = new CMapWordToPtr();
#endif

#ifdef XP_WIN32
	m_pFileSubmenuMap = new CMapPtrToPtr();
#else
	m_pFileSubmenuMap = new CMapWordToPtr();
#endif

	m_pBookmarkBitmap = NULL;
	m_pBookmarkFolderBitmap = NULL;
	m_pBookmarkFolderOpenBitmap = NULL;
	m_pBookmarksMenu = NULL;
	m_BookmarksGarbageList = NULL;
	m_OriginalBookmarksGarbageList = NULL;

#ifdef _WIN32
	m_pMenuMap = new CMapPtrToPtr();
#else
	m_pMenuMap = new CMapWordToPtr();
#endif

#ifdef EDITOR
	extern void FED_SetupStrings(void);
	FED_SetupStrings();
    m_bCloseFrame = FALSE;
    m_bSkipSaveEditChanges = FALSE;
#endif

	//	Initially we'll be allowing the frame to resize.
	m_bCanResize = TRUE;
	m_bDisableHotkeys = FALSE;
	m_bPreCreated = FALSE;
	m_bZOrderLocked = FALSE;
	m_bBottommost = FALSE;

	m_pChrome = NULL;
	m_wAddedExStyles = 0L;
	m_wRemovedExStyles = 0L;

	m_hPopupParent = NULL;
	m_bConference = FALSE;

#ifdef XP_WIN32
	// jliu added the following to support CJK caption print
	m_bActive = FALSE;
	m_csid    = CIntlWin::GetSystemLocaleCsid();
	SetupCapFont( m_csid );		// setup the default font first
	DWORD dwVersion = ::GetVersion();
	if( LOBYTE(dwVersion) < 4 )
		capStyle = DT_CENTER;	// for NT 3.51
	else
		capStyle = DT_LEFT;		// for NT 4.0 and Win95
#endif
}



#ifdef XP_WIN32
// jliu added the following to support CJK caption print
int16 CGenericFrame::GetTitleWinCSID()
{
	CWinCX *pContext = GetMainWinContext();
	MWContext* pMWContext = NULL;
	if( pContext == NULL || ( pMWContext = pContext->GetContext() ) == NULL ){
		return -1;
	}
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pMWContext);
	
	return INTL_GetCSIWinCSID(csi);
}

////////////////////////////////////////////////////////////////////
// this function is called outside to get the wincsid
// and then setup the Caption Font.
//
void CGenericFrame::SetupCapFont( int16 csid, BOOL force )
{
	if( !force && ( m_csid == csid ) && ( hCapFont.m_hObject != NULL ) )
		return;

	// set the wincsid
	if( csid <= 0 )
		m_csid = CIntlWin::GetSystemLocaleCsid();
	else
		m_csid = csid;

	// set the caption print font
	if( hCapFont.m_hObject != NULL )
		hCapFont.DeleteObject();

	// get the system caption print font first
	NONCLIENTMETRICS nc;
	nc.cbSize = sizeof(NONCLIENTMETRICS);
	BOOL b = SystemParametersInfo( SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &nc, 0 );
	XP_ASSERT( b );

	LOGFONT lf = nc.lfCaptionFont;	// get the logic font used to paint the caption
	csid = CIntlWin::GetSystemLocaleCsid();
	if( force || ( m_csid !=  csid ) ){		// in case user changes system font
		lf.lfCharSet = IntlGetLfCharset( m_csid );
 		_tcscpy( lf.lfFaceName, IntlGetUIPropFaceName( m_csid ) );
		hCapFont.CreateFontIndirect( &lf );
	}

	// create the default font if the above step failure
	if( hCapFont.m_hObject == NULL ){
		hCapFont.CreateFontIndirect( &nc.lfCaptionFont );
		m_csid = csid;
	}
}

//////////////////
// User changed system settings, such  as font. Or changed
// encoding method, coming from compfrm.cpp
// Invalidate everything to generate anew.
// 
LRESULT CGenericFrame::OnSettingChange( WPARAM, LPARAM )
{
	SetupCapFont( GetTitleWinCSID(), TRUE );
	SendMessage(WM_NCPAINT);
	return 0;
}


//////////////////
// Someone called SetWindowText: paint new text.
//
LRESULT CGenericFrame::OnSetText(WPARAM wParam, LPARAM lParam)
{
	SetupCapFont( GetTitleWinCSID() );

	// Turn WS_VISIBLE style off before calling Windows to
	// set the text, then turn it back on again after.
	//
	DWORD dwStyle = GetStyle();
	if( dwStyle & WS_VISIBLE )
		SetWindowLong( m_hWnd, GWL_STYLE, dwStyle & ~WS_VISIBLE );
	LRESULT lRet = DefWindowProc(WM_SETTEXT, wParam, lParam); 
	if( dwStyle & WS_VISIBLE )
		SetWindowLong( m_hWnd, GWL_STYLE, dwStyle );

	SendMessage(WM_NCPAINT);	// paint non-client area (frame too)

	return lRet;
}

//////////////////
// Non-client area (de)activated: paint it
//
BOOL CGenericFrame::OnNcActivate(BOOL bActive)
{
	if( m_nFlags & WF_STAYACTIVE )
		bActive = TRUE;
	if( !IsWindowEnabled() )
		bActive = FALSE;
	if( bActive == m_bActive )	
		return TRUE;

	MSG& msg = AfxGetThreadState()->m_lastSentMsg;
	if( msg.wParam > 1 )
		msg.wParam = TRUE;
	m_bActive = bActive = msg.wParam;

	// Turn WS_VISIBLE off before calling DefWindowProc,
	// so DefWindowProc won't paint and thereby cause flicker.
	//
	DWORD dwStyle = GetStyle();
	if( dwStyle & WS_VISIBLE )
		::SetWindowLong(m_hWnd, GWL_STYLE, (dwStyle & ~WS_VISIBLE));
	DefWindowProc(WM_NCACTIVATE, bActive, 0L);	// don't call CFrameWnd::OnNcActivate()
	if( dwStyle & WS_VISIBLE )
		::SetWindowLong(m_hWnd, GWL_STYLE, dwStyle);

	SendMessage( WM_NCPAINT );	// paint non-client area and frame

	return TRUE;
}


void CGenericFrame::OnNcPaint()
{
	DWORD dwStyle = GetStyle();
	if( !( dwStyle & WS_CAPTION )  ){
		CFrameWnd::OnNcPaint();			// do the default one first
		return;
	}

	CSize szFrame = (dwStyle & WS_THICKFRAME) ?
		CSize(GetSystemMetrics(SM_CXSIZEFRAME),
			   GetSystemMetrics(SM_CYSIZEFRAME)) :
		CSize(GetSystemMetrics(SM_CXFIXEDFRAME),
				GetSystemMetrics(SM_CYFIXEDFRAME));

	int dxIcon = GetSystemMetrics(SM_CXSIZE); // width of caption icon/button

	// Compute rectangle
	CRect rc;		// window rect in screen coords
	GetWindowRect( &rc );
	rc.left  += szFrame.cx;				// frame
	rc.right -= szFrame.cx;				// frame
	rc.top   += szFrame.cy;				// top = end of frame
	rc.bottom = rc.top + GetSystemMetrics(SM_CYCAPTION)  // height of caption
		- GetSystemMetrics(SM_CYBORDER);				  // minus gray shadow border


	MSG& msg = AfxGetThreadState()->m_lastSentMsg;

	// Don't paint if the caption doesn't lie within the region.
	//
	if (msg.wParam > 1 && !::RectInRegion((HRGN)msg.wParam, &rc)) {
		CFrameWnd::OnNcPaint();			// do the default one first
		return;							// quit
	}


	// setup the update region, include the title text region

	CRect rectCaption;
	GetWindowRect( &rectCaption );

	CWindowDC dc(this);
	CFont* hFontOld = dc.SelectObject( &hCapFont );

	HRGN hRgnCaption = ::CreateRectRgnIndirect(&rc);
	HRGN hRgnNew     = ::CreateRectRgnIndirect(&rc);
	if( msg.wParam > 1 ){
		// wParam is a valid region: subtract caption from it
		::CombineRgn( hRgnNew, (HRGN)msg.wParam, hRgnCaption, RGN_OR );
	} else {
		// wParam is not a valid region: create one that's the whole window
		DeleteObject( hRgnNew );
		hRgnNew = ::CreateRectRgnIndirect(&rectCaption);
	}

	// call the default NcPaint

	WPARAM savewp = msg.wParam;		// save original wParam
	msg.wParam = (WPARAM)hRgnNew;	// set new region for DefWindowProc
	CFrameWnd::OnNcPaint();			// paint the caption bar except title text
	DeleteObject( hRgnCaption );	// clean up
	DeleteObject( hRgnNew );		// clean up
	msg.wParam = savewp;			// restore original wParam


	// Compute rectangle
	// Within the basic button rectangle, Windows 95 uses a 1 or 2 pixel border
	// Icon has 2 pixel border on left, 1 pixel on top/bottom, 0 right
	// Close box has a 2 pixel border on all sides but left, which is zero
	// Minimize button has 2 pixel border on all sides but right.

	rectCaption -= rectCaption.TopLeft(); // convert caption rectangle origin to (0,0)
	rectCaption.left  += (szFrame.cx + 2 + dxIcon);
	rectCaption.right -= (szFrame.cx + 2 + dxIcon);
	rectCaption.top   += szFrame.cy;				// top = end of frame
	rectCaption.bottom = rectCaption.top + GetSystemMetrics(SM_CYCAPTION)	// height of caption
		- GetSystemMetrics(SM_CYBORDER);			// minus gray shadow border


	// i don't know why MS supports this feature ...
	if( ( dwStyle & WS_MAXIMIZEBOX ) || ( dwStyle & WS_MINIMIZEBOX ) ){
		rectCaption.right -= ( dxIcon + 2 + dxIcon );
		if( capStyle != DT_LEFT )	// for NT 3.51
			rectCaption.right += dxIcon + 4;
	}

	// create a brush for caption background
	CBrush brCaption;
	brCaption.CreateSolidBrush(	
		::GetSysColor( m_bActive ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION ) );

	CString strTitle;
	GetWindowText( strTitle );

	dc.FillRect(&rectCaption, &brCaption);
	dc.SetTextColor(::GetSysColor(m_bActive ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT));
	dc.SetBkMode(TRANSPARENT);

	int titleLen = _tcslen( strTitle );
	UINT cs = capStyle;
	if( cs != DT_LEFT ){	// it is NT 3.51
		CSize size = dc.GetTextExtent( strTitle, titleLen );
		if( size.cx >= rectCaption.Width() )
			cs = DT_LEFT;
	}
	CIntlWin::DrawText( m_csid, (HDC)dc, strTitle.GetBuffer( titleLen ), titleLen,
		&rectCaption, cs|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS );

	dc.SelectObject(hFontOld);
}

#endif	// #ifdef XP_WIN32


CGenericFrame::~CGenericFrame()
{
	if (m_BookmarkMenuPane)
	{
		HT_DeletePane(m_BookmarkMenuPane);
		m_BookmarkMenuPane = NULL;
	}

#ifdef XP_WIN32
	if( hCapFont.m_hObject != NULL )
		hCapFont.DeleteObject();
#endif

	if (m_pHotlistMenuMap) {
    	delete m_pHotlistMenuMap;
		m_pHotlistMenuMap = NULL;
	}

	if (m_pSubmenuMap) {
		delete m_pSubmenuMap;
		m_pSubmenuMap = NULL;
	}

	if (m_pFileSubmenuMap) {
		delete m_pFileSubmenuMap;
		m_pFileSubmenuMap = NULL;
	}

	if (m_pMenuMap)
	{
		DeleteMenuMapIDs();
		delete m_pMenuMap;
		m_pMenuMap = NULL;
	}

	if (m_pChrome) {
		m_pChrome->Release();
	}

    // Remove it from the list of frames kept by theApp
    //  Done here as was done in OnClose, but needed here since
    //      hidden frame when in place never remove's itself from the
    //      list as OnClose never gets called if not active when
    //      deleted.
    if(theApp.m_pFrameList == this) {
        // first one
        theApp.m_pFrameList = m_pNext;

        // Reset the global last frame
        //  (this is unlikely unless we are only frame left)
        if( wfe_pLastFrame == this )
            wfe_pLastFrame = NULL;
    } else {
        // loop through for the one pointing to me
        for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext)
            if(f->m_pNext == this){
                // Change the "last frame" to the one before
                if( wfe_pLastFrame == this )
                    wfe_pLastFrame = f;    
                f->m_pNext = m_pNext;
            }
    }
}

#define SEPARATORS_BEFORE_BOOKMARKS_START 2
#define ITEMS_BEFORE_BOOKMARKS_START 6
#define ITEMS_AFTER_BOOKMARKS_END 0

void CGenericFrame::BuildDirectoryMenu(CMenu * pMenu) 
{
	if (!pMenu) return; 
	if(GetMainContext() == NULL)    {
		return;
	}

    int idx =0, idx2=0;
    int iItems = pMenu->GetMenuItemCount();	
    char *label;
	int bOK = PREF_NOERROR;
  
    for (idx=0; bOK==PREF_NOERROR; idx++) {
		bOK = PREF_CopyIndexConfigString("menu.places.item",idx,"label",&label);

		// if the label is blank, truncate the menu here.
		if (!label || !label[0])
			bOK = PREF_ERROR;

		if (bOK == PREF_NOERROR) {
			if (idx < iItems) {
				if (!strcmpi("SEPARATOR",label) || !strcmpi("-",label))
					pMenu->ModifyMenu(CASTUINT(idx),CASTUINT(MF_BYPOSITION | MF_SEPARATOR),NULL,"");
				else
					pMenu->ModifyMenu(CASTUINT(idx),CASTUINT(MF_BYPOSITION | MF_STRING),CASTUINT(FIRST_PLACES_MENU_ID + idx),label);
			} else {
				if (!strcmpi("SEPARATOR",label) || !strcmpi("-",label))
					pMenu->InsertMenu(CASTUINT(idx),CASTUINT(MF_BYPOSITION | MF_SEPARATOR),NULL,"");
				else
					pMenu->InsertMenu(CASTUINT(idx),CASTUINT(MF_BYPOSITION | MF_STRING),CASTUINT(FIRST_PLACES_MENU_ID + idx),label);
			}
		}
		if (label) {
			XP_FREE(label);
			label = NULL;
		}
    }
}            

void CGenericFrame::BuildFileBookmarkMenu(CTreeMenu* pMenu, HT_Resource pRoot) 
{
	char *entryName;

	entryName = HT_GetNodeName(pRoot);

	if(strcmp(entryName, "") == 0)
		entryName = szLoadString(IDS_UNTITLED_FOLDER);

	CTreeItem *item= new CTreeItem(NULL, NULL, FIRST_FILE_BOOKMARK_MENU_ID + m_nFileBookmarkItems, CString(entryName));

	pMenu->AddItem(item, 0, NULL);
	m_pHotlistMenuMap->SetAt(FIRST_FILE_BOOKMARK_MENU_ID + m_nFileBookmarkItems++,	pRoot);

	pMenu->InsertMenu(FIRST_FILE_BOOKMARK_MENU_ID + m_nFileBookmarkItems++, MF_BYPOSITION | MF_SEPARATOR);

	m_BookmarksGarbageList->Add(item);
	CTreeItem *pItem;

	HT_Cursor theCursor = HT_NewCursor(pRoot);
	HT_Resource pEntry = NULL;
	while (pEntry = HT_GetNextItem(theCursor))
	{
		if(HT_IsContainer(pEntry))
		{
			entryName = HT_GetNodeName(pEntry);
			
			if(strcmp(entryName, "") == 0)
				entryName = szLoadString(IDS_UNTITLED_FOLDER);

			pItem= new CTreeItem(NULL, NULL, FIRST_FILE_BOOKMARK_MENU_ID + m_nFileBookmarkItems, FEU_EscapeAmpersand(CString(entryName)));
			pMenu->AddItem(pItem, FIRST_FILE_BOOKMARK_MENU_ID + m_nFileBookmarkItems, NULL);
			m_pHotlistMenuMap->SetAt(FIRST_FILE_BOOKMARK_MENU_ID + m_nFileBookmarkItems++, pEntry);
			m_BookmarksGarbageList->Add(pItem);
		}
	}

}

// Append menu items for each of the bookmark entries in pRoot. Uses
// member data m_nBookmarkItems
void CGenericFrame::BuildBookmarkMenu(CTreeMenu* pMenu, HT_Resource pRoot, int nStart) 
{
	ASSERT(pMenu && pRoot);
	if (!pMenu || !pRoot)
		return;

	m_pCachedTreeMenu = pMenu;
	m_nCachedStartPoint = nStart;

	// Send the open/close event.
	PRBool isOpen;
	HT_GetOpenState(pRoot, &isOpen);
	if (!isOpen)
	{
		HT_SetOpenState(pRoot, PR_TRUE);
	}
	else FinishMenuExpansion(pRoot);
}

void CGenericFrame::FinishMenuExpansion(HT_Resource pRoot)
{
	CTreeMenu* pMenu = m_pCachedTreeMenu;
	int nStart = m_nCachedStartPoint;

	// See how big the menu can be. Menus taller than the screen cause GPFs and hangs
	// Do we want this static?  In Win95, menu font can change and this may no longer
	// be valid after the first time through
    //static int nItemHeight = GetSystemMetrics(SM_CYMENU);
	int nItemHeight = GetSystemMetrics(SM_CYMENU);
    static int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    // While we have more items in the list and we haven't run out of screen
    // real-estate continue to add items
    BOOL        bStillRoom = TRUE;
    int         nItemsAdded = pMenu->GetMenuItemCount(); // remember the constant items

	HT_Cursor theCursor = HT_NewCursor(pRoot);
	HT_Resource pEntry = NULL;
	while (theCursor && (pEntry = HT_GetNextItem(theCursor)) && bStillRoom)
	{
    	char*   lpszShortName = NULL;
		CString	csAmpersandName;
	
		// Don't add more menu items than we can handle
		if((FIRST_BOOKMARK_MENU_ID + m_nBookmarkItems) > LAST_BOOKMARK_MENU_ID) 
			break;

		// fe_MiddleCutString() doesn't like NULL pointers
		if (HT_GetNodeName(pEntry)){
			lpszShortName = fe_MiddleCutString(HT_GetNodeName(pEntry), MAX_MENU_ITEM_LENGTH);
			csAmpersandName = FEU_EscapeAmpersand(CString(lpszShortName));
		}

		// If this is a nested list or an item we must have a name
		if ( !lpszShortName && !HT_IsSeparator(pEntry)) 
			continue;

		if (HT_IsSeparator(pEntry)) 
		{
			// Handle separators
			pMenu->InsertMenu(nStart, MF_BYPOSITION | MF_SEPARATOR);
		} 
		else if (HT_IsContainer(pEntry))
		{
			// Append a nested pop-up menu. We add an empty menu now, and when
			// the user displays the sub-menu we dynamically add the menu items.
			// This is faster, and it uses fewer USER resources (big deal for
			// Win16)

			CTreeMenu *pSubMenu=new CTreeMenu;
			pSubMenu->CreatePopupMenu();

			IconType nIconType = DetermineIconType(pEntry, FALSE);
			void* pCustomIcon = NULL;
			if (nIconType == LOCAL_FILE)
				pCustomIcon = FetchLocalFileIcon(pEntry);
			else if (nIconType == ARBITRARY_URL)
				pCustomIcon = FetchCustomIcon(pEntry, pMenu, FALSE);
			CTreeItem *item= new CTreeItem(m_pBookmarkFolderBitmap, m_pBookmarkFolderOpenBitmap,
										   (UINT)pSubMenu->GetSafeHmenu(),csAmpersandName, pSubMenu);

			pSubMenu->SetParent(this);
			pMenu->AddItem(item, nStart, pSubMenu, pCustomIcon, nIconType);
			m_BookmarksGarbageList->Add(item);
		
			// Add the HMENU to the map. We need to do this because Windows doesn't
			// allow any client data to be associated with menu items. We can't use
			// the same map as we use for menu items, because sub-menus don't have
			// command IDs
			m_pSubmenuMap->SetAt(pSubMenu->GetSafeHmenu(), pEntry);
		}
		else
		{
			// We're an URL
			IconType nIconType = DetermineIconType(pEntry, FALSE);
			void* pCustomIcon = NULL;
			if (nIconType == LOCAL_FILE)
				pCustomIcon = FetchLocalFileIcon(pEntry);
			else if (nIconType == ARBITRARY_URL)
				pCustomIcon = FetchCustomIcon(pEntry, pMenu, FALSE);
			CTreeItem *item= new CTreeItem(m_pBookmarkBitmap, m_pBookmarkBitmap, 
				FIRST_BOOKMARK_MENU_ID + m_nBookmarkItems, csAmpersandName);

			pMenu->AddItem(item, nStart, NULL, pCustomIcon, nIconType);
			m_BookmarksGarbageList->Add(item);
		
			// Add the URL to the map. We need to do this because Windows doesn't
			// allow any client data to be associated with menu items.
			m_pHotlistMenuMap->SetAt(FIRST_BOOKMARK_MENU_ID + m_nBookmarkItems, pEntry);
		} 	

		// Even though separators and pull-right sub-menus don't have command IDs,
		// increment the count of bookmark items for all three, because we rely
		// on this being greater than zero to indicate that we have already built
		// the bookmark menu
		m_nBookmarkItems++;

		// In all three cases we added a new menu item
		nItemsAdded++;

		nStart++;

		// Make sure we haven't grown too much -- allow to fill 4/5 of the
		// screen
		if ((nItemsAdded * nItemHeight > nScreenHeight * 4 / 5)) 
		{
			// Stop adding new ones
			bStillRoom = FALSE;

			// Let the user know we have truncated the list
			CTreeItem *item= new CTreeItem(NULL, NULL, ID_HOTLIST_VIEW, szLoadString(IDS_MORE_BOOKMARKS));

			pMenu->AddItem(item, nStart, NULL);
			m_BookmarksGarbageList->Add(item);
		}

		if (lpszShortName)
			XP_FREE(lpszShortName);
	}
}


BEGIN_MESSAGE_MAP(CGenericFrame, CFrameWnd)
    ON_WM_TIMER()
    ON_WM_ENTERIDLE()
    ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_QUERYNEWPALETTE()
	ON_WM_PALETTECHANGED()
    ON_WM_INITMENUPOPUP()
	ON_WM_MENUSELECT()
	ON_WM_MEASUREITEM()
	ON_WM_MENUCHAR()
	ON_MESSAGE(WM_SETMESSAGESTRING, OnSetMessageString)
	ON_WM_SETFOCUS()
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_WINDOWPOSCHANGING()
	ON_WM_SIZE()
	ON_WM_MOVE()
	ON_WM_GETMINMAXINFO()
	ON_WM_SETFOCUS()
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_ACTIVATE()

    ON_COMMAND(ID_FILE_CLOSE, OnFrameExit)
	ON_COMMAND(ID_FILE_PAGE_SETUP, OnFilePageSetup)
	ON_COMMAND(ID_OPEN_MAIL_WINDOW, OnOpenMailWindow)
	ON_COMMAND(ID_OPEN_NEWS_WINDOW, OnOpenNewsWindow)
    ON_COMMAND(ID_WINDOW_BOOKMARKWINDOW, OnShowBookmarkWindow)
    ON_COMMAND(ID_WINDOW_ADDRESSBOOK, OnShowAddressBookWindow)
#ifdef MOZ_MAIL_NEWS
    ON_COMMAND(ID_MIGRATION_TOOLS, OnMigrationTools)
    ON_UPDATE_COMMAND_UI(ID_MIGRATION_TOOLS, OnUpdateMigrationTools)
#endif //MOZ_MAIL_NEWS
#if defined(OJI) || defined(JAVA)
    ON_COMMAND(ID_OPTIONS_SHOWJAVACONSOLE, OnToggleJavaConsole)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWJAVACONSOLE, OnUpdateJavaConsole)
#endif
	ON_COMMAND(ID_FILE_MAILNEW, OnFileMailNew) 
#ifdef MOZ_TASKBAR
	ON_COMMAND(ID_WINDOW_TASKBAR, OnTaskbar)
#endif /* MOZ_TASKBAR */
	ON_COMMAND(ID_WINDOW_LIVECALL, OnLiveCall)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_LIVECALL, OnUpdateLiveCall)
	ON_COMMAND(ID_WINDOW_CALENDAR, OnCalendar)
	ON_COMMAND(ID_WINDOW_IBMHOSTONDEMAND, OnIBMHostOnDemand)
	ON_COMMAND(ID_WINDOW_NETCASTER, OnNetcaster)
	ON_COMMAND(ID_WINDOW_AIM, OnAim)
	ON_COMMAND(ID_COMMAND_ABOUT, OnAbout)
    ON_COMMAND(ID_APP_ABOUT, OnAbout)
    ON_COMMAND(ID_DOCUMENT_TOP, OnDocumentTop)
    ON_COMMAND(ID_DOCUMENT_BOTTOM, OnDocumentBottom)
    ON_COMMAND(ID_HOTLIST_FISHCAM, OnFishCam)
    ON_COMMAND(ID_OPTIONS_PREFERENCES, OnDisplayPreferences)
    ON_COMMAND(ID_OPTIONS_MAILANDNEWS, OnPrefsMailNews)
    ON_COMMAND(ID_BIG_SWITCH, OnGoOffline)
	ON_UPDATE_COMMAND_UI(ID_BIG_SWITCH, OnUpdateGoOffline)
#ifdef MOZ_MAIL_NEWS
    ON_COMMAND(ID_SYNCHRONIZE, OnSynchronize)
    ON_UPDATE_COMMAND_UI(ID_SYNCHRONIZE, OnUpdateSynchronize)
#endif
#ifdef MOZ_OFFLINE
    ON_UPDATE_COMMAND_UI(IDS_ONLINE_STATUS, OnUpdateOnlineStatus)
#endif  //MOZ_OFFLINE
    ON_COMMAND(ID_DONEGOINGOFFLINE, OnDoneGoingOffline)
#ifdef FORTEZZA
    ON_COMMAND(ID_FORTEZZA_CARD, OnStartFortezzaCard)
    ON_COMMAND(ID_FORTEZZA_CHANGE, OnStartFortezzaChange)
    ON_COMMAND(ID_FORTEZZA_VIEW, OnStartFortezzaView)
    ON_COMMAND(ID_FORTEZZA_INFO, OnDoFortezzaInfo)
    ON_COMMAND(ID_FORTEZZA_LOG, OnDoFortezzaLog)
#endif
    ON_COMMAND(ID_FILE_CLOSE, OnFrameExit)

	ON_UPDATE_COMMAND_UI(ID_VIEW_COMMANDTOOLBAR, OnUpdateViewCommandToolbar)
	ON_COMMAND(ID_VIEW_COMMANDTOOLBAR, OnViewCommandToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOCATIONTOOLBAR, OnUpdateViewLocationToolbar)
	ON_COMMAND(ID_VIEW_LOCATIONTOOLBAR, OnViewLocationToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CUSTOMTOOLBAR, OnUpdateViewCustomToolbar)
	ON_COMMAND(ID_VIEW_CUSTOMTOOLBAR, OnViewCustomToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NAVCENTER, OnUpdateViewNavCenter)
	ON_COMMAND(ID_VIEW_NAVCENTER, OnViewNavCenter)
	ON_UPDATE_COMMAND_UI(ID_CUSTOMIZE_TOOLBAR, OnUpdateCustomizeToolbar)
	ON_COMMAND(ID_CUSTOMIZE_TOOLBAR, OnCustomizeToolbar)
	ON_UPDATE_COMMAND_UI(ID_PLACES, OnUpdatePlaces)
	ON_UPDATE_COMMAND_UI(ID_SECURITY, OnUpdateSecurity)
	ON_COMMAND(ID_SECURITY, OnSecurity)
    ON_COMMAND(ID_GO_HISTORY, OnGoHistory)

	ON_COMMAND(ID_VIEW_INCREASEFONT, OnIncreaseFont)
	ON_COMMAND(ID_VIEW_DECREASEFONT, OnDecreaseFont)

	ON_COMMAND(ID_ANIMATION_BONK, OnAnimationBonk )

	ON_COMMAND(ID_NEW_FRAME, OnNewFrame)
	ON_UPDATE_COMMAND_UI(ID_NEW_FRAME, OnUpdateNewFrame)
	ON_COMMAND(ID_FILE_OPENURL, OnFileOpenurl)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENURL, OnUpdateFileOpenurl)

    ON_COMMAND(ID_NEXT_WINDOW, OnNextWindow)
    ON_COMMAND(ID_SAVE_OPTIONS, OnSaveOptions)
    ON_COMMAND(ID_HOTLIST_VIEW,OnShowBookmarkWindow) 
	ON_COMMAND(ID_HOTLIST_ADDCURRENTTOHOTLIST, OnHotlistAddcurrenttohotlist)
	ON_UPDATE_COMMAND_UI(ID_HOTLIST_ADDCURRENTTOHOTLIST, OnUpdateHotlistAddcurrenttohotlist)
	ON_COMMAND(ID_HOTLIST_TOOLBAR, OnHotlistAddcurrenttotoolbar)
	ON_UPDATE_COMMAND_UI(ID_HOTLIST_TOOLBAR, OnUpdateHotlistAddcurrenttotoolbar)
#ifdef MOZ_TASKBAR
	ON_UPDATE_COMMAND_UI(ID_WINDOW_TASKBAR, OnUpdateTaskbar)
#endif /* MOZ_TASKBAR */   
	ON_COMMAND(ID_TOOLS_MAIL, OnOpenMailWindow)
	ON_COMMAND(ID_TOOLS_INBOX, OnOpenInboxWindow)
	ON_COMMAND(ID_TOOLS_NEWS, OnOpenNewsWindow)
	ON_COMMAND(ID_TOOLS_WEB, OnToolsWeb)
#ifdef EDITOR
	ON_COMMAND(ID_TOOLS_EDITOR, OnOpenComposerWindow)
#endif
#ifdef MOZ_MAIL_NEWS   
	ON_COMMAND(ID_TOOLS_ADDRESSBOOK, OnShowAddressBookWindow)
#endif /* MOZ_MAIL_NEWS */  
	ON_COMMAND(ID_TOOLS_BOOKMARKS, OnShowBookmarkWindow)
	ON_COMMAND(ID_VIEW_ADVANCEDTOOLBARS, OnShowAdvanced)
	ON_COMMAND(ID_EDIT_LDAPSEARCH, OnLDAPSearch)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ADVANCEDTOOLBARS, OnUpdateShowAdvanced)
    ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)
    ON_COMMAND(IDM_SEARCHADDRESSES, OnLDAPSearch)
	ON_COMMAND(ID_COMMAND_PAGE_FROM_WIZARD, OnPageFromWizard)
    ON_REGISTERED_MESSAGE(WM_HELPMSG, OnHelpMsg)


#ifdef ON_COMMAND_RANGE
        ON_COMMAND_RANGE(ID_OPTIONS_ENCODING_1, ID_OPTIONS_ENCODING_70, OnToggleEncoding)
        ON_UPDATE_COMMAND_UI_RANGE(ID_OPTIONS_ENCODING_1, ID_OPTIONS_ENCODING_70, OnUpdateEncoding)
#endif

#ifdef EDITOR
	ON_COMMAND(ID_EDT_NEW_DOC_BLANK, OnEditNewBlankDocument)
  	ON_UPDATE_COMMAND_UI(ID_EDT_NEW_DOC_BLANK, OnUpdateCanInteract)
    ON_COMMAND(ID_ACTIVATE_SITE_MANAGER, OnActivateSiteManager)
    ON_UPDATE_COMMAND_UI(ID_ACTIVATE_SITE_MANAGER, OnUpdateActivateSiteManager)
	ON_COMMAND(ID_EDIT_DOCUMENT, OnNavigateToEdit)
	ON_COMMAND(ID_EDIT_FRAME, OnEditFrame)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DOCUMENT, OnUpdateCanInteract)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FRAME, OnUpdateEditFrame)
	ON_COMMAND(ID_EDT_NEW_DOC_FROM_TEMPLATE, OnEditNewDocFromTemplate)
  	ON_UPDATE_COMMAND_UI(ID_EDT_NEW_DOC_FROM_TEMPLATE, OnUpdateCanInteract)
#ifdef XP_WIN32
    ON_REGISTERED_MESSAGE(WM_SITE_MANAGER, OnSiteMgrMessage)
	// jliu added the following to support CJK caption print
	ON_WM_NCPAINT()
	ON_MESSAGE(WM_SETTEXT, OnSetText)
	ON_WM_NCACTIVATE()
	ON_MESSAGE(WM_SETTINGCHANGE, OnSettingChange)
#endif
    ON_REGISTERED_MESSAGE(WM_NG_IS_ACTIVE, OnNetscapeGoldIsActive)
    ON_REGISTERED_MESSAGE(WM_OPEN_EDITOR, OnOpenEditor)
    ON_REGISTERED_MESSAGE(WM_OPEN_NAVIGATOR, OnOpenNavigator)
#endif // EDITOR
	ON_COMMAND(ID_COMMAND_HELPINDEX, OnHelpMenu)
	ON_UPDATE_COMMAND_UI(ID_BOOKMARKS_FILEROOT, OnUpdateFileRoot)

//  Conditionally compile these if supported.
#ifdef WM_DEVICECHANGE
    ON_WM_DEVICECHANGE()
#endif
#ifdef WM_SIZING
	ON_WM_SIZING()
#endif

#ifdef _WIN32
    ON_REGISTERED_MESSAGE(msg_MouseWheel, OnHackedMouseWheel)
#endif

#ifdef DEBUG_WHITEBOX
	ON_COMMAND(IDS_WHITEBOX_MENU, OnWhiteBox)
#endif
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGenericFrame message handlers

void CGenericFrame::RecalcLayout ( BOOL bNotify )
{
    for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext)
        f->CFrameWnd::RecalcLayout ( );
}

void CGenericFrame::GetMessageString( UINT nID, CString& rMessage ) const
{
	LPTSTR lpsz = NULL;
	if (nID >= FIRST_BOOKMARK_MENU_ID && nID <= LAST_BOOKMARK_MENU_ID) {
		m_pHotlistMenuMap->Lookup(nID, (void*&)lpsz);
		if (lpsz) {
			rMessage = lpsz;
		}
	} else if (IS_PLACESMENU_ID(nID)  || IS_HELPMENU_ID(nID)) {
		rMessage = "";  // for now -- jonm
	} else if (nID==ID_COMMAND_HELPINDEX) {
		rMessage = szLoadString(IDS_COMMAND_HELPINDEX);
	} else if (nID==ID_COMMAND_ABOUT) {
		rMessage = szLoadString(IDS_COMMAND_ABOUT);
	} else {
#ifdef _WIN32
		if ( nID ) {
			CFrameWnd::GetMessageString( nID, rMessage );
		}
#else
		// Use the wParam as a string ID
		lpsz = szLoadString(nID);
		// If we got an empty string, punt
		if (lpsz && lpsz[0]) {
			// Only display until the newline so we don't get the tool tip string
			AfxExtractSubString( rMessage, lpsz, 0 );
		}
#endif
	}
}

// Read the initial sizes out of preferences file
//
BOOL CGenericFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	BOOL retval;

	if(m_hPopupParent)
		cs.style |= WS_POPUP;

	m_bPreCreated = TRUE;

	// Let's sanity check those values, kiddies

	if ( cs.x > ( GetSystemMetrics(SM_CXFULLSCREEN) - 100 ) ) {
		cs.x = GetSystemMetrics(SM_CXFULLSCREEN) - 100;
	}
	if ( cs.y > ( GetSystemMetrics(SM_CYFULLSCREEN) - 100 ) ) {
		cs.y = GetSystemMetrics(SM_CYFULLSCREEN) - 100;
	}
       
	retval = CFrameWnd::PreCreateWindow(cs);


#ifdef WIN32
	// Modify Ex_Styles set by MFC.
	cs.dwExStyle |= m_wAddedExStyles;
        cs.dwExStyle &= ~m_wRemovedExStyles;
#endif

        return(retval);
}

//
// Create widgets used by all frame windows 
//
static void CopyMenu(CMenu *srcMenu, CTreeMenu *destMenu, CTreeItemList *pGarbageList)
{
	if (!srcMenu)
	    return;

	int nCount = srcMenu->GetMenuItemCount();
	for(int dstPos = 0, srcPos = 0; dstPos< nCount; dstPos++, srcPos++)
	{
		UINT nID = srcMenu->GetMenuItemID(srcPos);
		
		if(nID == 0)
		{
			destMenu->InsertMenu(dstPos, MF_BYPOSITION | MF_SEPARATOR);
		}
		else
		{
			char lpszMenuString[64];
			
			srcMenu->GetMenuString(srcPos, lpszMenuString, 64, MF_BYPOSITION);
			CString menuString(lpszMenuString);
			int nMnemonicPos;
			TCHAR cMnemonicChar = 0;

			if((nMnemonicPos = menuString.Find('&')) != -1)
				if(nMnemonicPos + 1 < menuString.GetLength())
					cMnemonicChar = menuString[nMnemonicPos + 1];

			if(nID == (UINT) -1)
			{
				CMenu *pSubMenu = srcMenu->GetSubMenu(srcPos);
				CTreeMenu *pTreeSubMenu=new CTreeMenu;

				pTreeSubMenu->CreatePopupMenu();
				
				CopyMenu(pSubMenu, pTreeSubMenu, pGarbageList);

				srcMenu->RemoveMenu(srcPos, MF_BYPOSITION);
				CTreeItem *pItem= new CTreeItem(NULL, NULL,(UINT)pTreeSubMenu->GetSafeHmenu(), menuString, pTreeSubMenu);
				((CTreeMenu*)destMenu)->AddItem(pItem, dstPos,pTreeSubMenu );

				if(cMnemonicChar != 0)
					destMenu->AddMnemonic(cMnemonicChar, 0, dstPos);

				pGarbageList->Add(pItem);
				//since we removed a menu we want to use the same position next time
				//and there is one less item in the menu we are copying.
				srcPos--;
			}
			else if(nID > 0)
			{
				CTreeItem *pItem= new CTreeItem(NULL, NULL, nID, lpszMenuString);
				((CTreeMenu*)destMenu)->AddItem(pItem, dstPos);

				if(cMnemonicChar != 0)
					destMenu->AddMnemonic(cMnemonicChar, nID, dstPos);

				pGarbageList->Add(pItem);
				//destMenu->InsertMenu(i, MF_BYPOSITION | MF_STRING, nID, (const char*)menuString);
			}
		}
	}
	((CTreeMenu*)destMenu)->CalculateItemDimensions();

}


void CGenericFrame::InitFileBookmarkMenu(void)
{
	int nCount = m_pFileBookmarkMenu->GetMenuItemCount();

	for(int i = nCount - 1; i >=0; i--)
		m_pFileBookmarkMenu->DeleteMenu(i, MF_BYPOSITION);
	m_pFileBookmarkMenu->ClearItems(0, 0);
	m_pFileSubmenuMap->RemoveAll();

	HT_View pView = HT_GetSelectedView(m_BookmarkMenuPane);

	m_pFileSubmenuMap->SetAt(m_pFileBookmarkMenu->GetSafeHmenu(), HT_TopNode(pView));
}

void CGenericFrame::LoadBookmarkMenuBitmaps(void)
{
	HDC hDC = ::GetDC(m_hWnd);

	WFE_InitializeUIPalette(hDC);
	HPALETTE hPalette = WFE_GetUIPalette(this);
	m_pBookmarksMenu->SetParent(this);
	HPALETTE hOldPalette = ::SelectPalette(hDC, hPalette, FALSE);
	m_pBookmarkBitmap=new CBitmap;
	m_pBookmarkBitmap->Attach(wfe_LoadBitmap(AfxGetResourceHandle(), hDC, MAKEINTRESOURCE(IDB_BOOKMARK_ITEM)));

	m_pBookmarkFolderBitmap=new CBitmap;
	m_pBookmarkFolderBitmap->Attach(wfe_LoadBitmap(AfxGetResourceHandle(), hDC, MAKEINTRESOURCE(IDB_BOOKMARK_FOLDER2)));

	m_pBookmarkFolderOpenBitmap=new CBitmap;
	m_pBookmarkFolderOpenBitmap->Attach(wfe_LoadBitmap(AfxGetResourceHandle(), hDC, MAKEINTRESOURCE(IDB_BOOKMARK_FOLDER_OPEN)));

	::SelectPalette(hDC, hOldPalette, TRUE);
	::ReleaseDC(m_hWnd, hDC);
}

void CGenericFrame::AddToMenuMap(int nIndex, UINT nID)
{
	HMENU hMainMenu = ::GetMenu(m_hWnd);
	
	if(hMainMenu)
	{
		HMENU hSubMenu = ::GetSubMenu(hMainMenu, nIndex);

		if(hSubMenu && m_pMenuMap)
		{
			UINT * pID = new UINT;
			
			if(pID)
			{
				*pID = nID;
#ifdef _WIN32
				m_pMenuMap->SetAt(hSubMenu, pID);
#else
				m_pMenuMap->SetAt((WORD)hSubMenu, pID);
#endif
			}
		}

	}

}

void CGenericFrame::OnDestroy()
{
	ASSERT(!m_pHotlistMenuMap || m_pHotlistMenuMap->IsEmpty());
	ASSERT(!m_pSubmenuMap || m_pSubmenuMap->IsEmpty());
//	ASSERT(!m_pFileSubmenuMap || m_pFileSubmenuMap->IsEmpty());

	// Unload the bookmark bitmaps and delete them
	if ( m_pBookmarkBitmap ) {
		m_pBookmarkBitmap->DeleteObject();
		m_pBookmarkFolderBitmap->DeleteObject();
		m_pBookmarkFolderOpenBitmap->DeleteObject();
	}
	delete m_pBookmarkBitmap;
	delete m_pBookmarkFolderBitmap;
	delete m_pBookmarkFolderOpenBitmap;
	if ( m_pBookmarksMenu ) {
		HMENU hBookmarkMenu=m_pBookmarksMenu->m_hMenu;
		ASSERT(hBookmarkMenu);
		int nCount;
		if ((nCount = ::GetMenuItemCount(hBookmarkMenu)) > 0) {


			// Because the bookmark menu has separators and pull-right sub-menus,
			// neither of which have command IDs, we can't tell just by looking
			// at the menu items whether they should be deleted. So instead we
			// delete everything
			for (int i = nCount - 1;
				 i >= 0; i--) {
				// This will delete any sub-menus as well (recursively)
				if(m_pBookmarksMenu->GetSubMenu(i) != NULL)
					m_pBookmarksMenu->GetSubMenu(i)->DestroyMenu();
				VERIFY(::DeleteMenu(hBookmarkMenu, i, MF_BYPOSITION));
			}
			//don't care about separator before bookmarks
			m_pBookmarksMenu->ClearItems(0, 0);
			m_nBookmarkItems = 0;  // reset the number of current bookmark menu items
			delete m_OriginalBookmarksGarbageList;
		}

		m_pBookmarksMenu->DestroyMenu();
	}
	delete m_pBookmarksMenu;
    
    //
    // Explicitly destroy the status bar window.  Normally the status bar is a child of this
    // frame and receives a WM_DESTROY as a result of this window destroying.  However, when 
    // this frame is serving an OLE container the status bar's parent is the container, hence
    // we need to destroy the window now.  Note the status bar is an auto-delete CWnd object.
    //
 	LPNSSTATUSBAR pIStatusBar = NULL;
 	if( GetChrome() && 
        SUCCEEDED(GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *)&pIStatusBar )) && 
        pIStatusBar ) 
 	{
        CNetscapeStatusBar *pStatusBar = ((CGenericStatusBar *)pIStatusBar)->GetNetscapeStatusBar();
        
        if( pStatusBar )
        {
            pStatusBar->DestroyWindow();
        }
 		pIStatusBar->Release();
 	}
    
	// we no longer need a taskbar.
#ifdef MOZ_TASKBAR
	theApp.GetTaskBarMgr().Reference(FALSE);
#endif /* MOZ_TASKBAR */   

}

BOOL CGenericFrame::OnQueryNewPalette() 
{
	// Have the main context in this window realize its palette.
	CWinCX *pWinCX = GetMainWinContext();
	CView* focusWnd = GetActiveView();
	BOOL retval = FALSE;
	HPALETTE hPal = WFE_GetUIPalette(this);	
	if (pWinCX) {
		if (!pWinCX->IsDestroyed()) {
		//	Make sure this is needed.
			if(pWinCX->GetPalette() != NULL) {
				//	Realize the palette.
				retval = CFrameGlue::GetFrameGlue(this)->RealizePalette(this, pWinCX->GetPane(), FALSE);
			}
		}
	}
	else  {
		CDC* pDC = GetDC();
		if (pDC) {
			::SelectPalette(pDC->GetSafeHdc(), hPal, FALSE);
			retval = ::RealizePalette(pDC->GetSafeHdc());
			Invalidate();
			SendMessageToDescendants(WM_PALETTECHANGED, (WPARAM)GetSafeHwnd());
#ifdef MOZ_TASKBAR
			if(theApp.GetTaskBarMgr().IsInitialized() && theApp.GetTaskBarMgr().IsFloating())
				theApp.GetTaskBarMgr().ChangeTaskBarsPalette(GetSafeHwnd());
#endif /* MOZ_TASKBAR */
			ReleaseDC(pDC);
		}

	}
	return retval;	
}

// Only top-level windows will receive this message
void CGenericFrame::OnPaletteChanged(CWnd* pFocusWnd) 
{

//	CFrameWnd::OnPaletteChanged(pFocusWnd);
	
	//	Don't do this if we caused it to happen.
	if(pFocusWnd == (CWnd *)this)	{
		return;
	}

	TRACE("CGenericFrame::OnPaletteChanged(%p)\n", pFocusWnd);
	// Tell the main context to realize the palette and redraw.		   f
	CWinCX *pWinCX = GetActiveWinContext();
	if(pWinCX && !pWinCX->IsDestroyed())	{
        HPALETTE pPal = pWinCX->GetPalette();
		//	Make sure this is needed.
		if( pPal )	{
			//	Realize the palette.
            //	Detect if it's a child view, and don't do anything.	 If we cause a
			//  palette change.

			if ((IsChild(pFocusWnd) == TRUE) && FEU_IsNetscapeFrame(pFocusWnd)) {
					TRACE("Blocking child view palette change\n");
            }
			else {
				// Either a plug-in or a Java child window realized its
				// palette, or a different top-level window realized
				// its palette.  Realize the Navigator's palette in
				// the background.
					CFrameGlue::GetFrameGlue(this)->RealizePalette(this, pFocusWnd->GetSafeHwnd(), TRUE);
			}
		}
	}
}
extern void FE_PokeLibSecOnIdle();

BOOL CGenericFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{

   if( nID >= ID_OPTIONS_ENCODING_1 && nID <= ID_OPTIONS_ENCODING_70 && 
       (nCode == CN_UPDATE_COMMAND_UI))  
   {
          OnUpdateEncoding( (CCmdUI*) pExtra );
          return TRUE;
   }
    // give me extra randomness for the first couple of messages
    static int s_nLibSecCount = 0;
    if(s_nLibSecCount < 300) {
        FE_PokeLibSecOnIdle();
        s_nLibSecCount++;
    }
        

	//	Pump through any active child first.
	//	This will catch active control bars, etc.
	CWnd *pFocus = GetFocus();
	if( pFocus )	{
		//	Make sure it's a child of us, and not a CGenericView (we handle those below).
		//	Also make sure it's not a child of our main view.
		//		Those are handled by the view, not us.
		BOOL bFrameChild = IsChild(pFocus);
		BOOL bGenericView = pFocus->IsKindOf(RUNTIME_CLASS(CGenericView));
		BOOL bViewChild = FALSE;
		if(GetMainWinContext() && GetMainWinContext()->GetPane())	{
            bViewChild = ::IsChild(GetMainWinContext()->GetPane(), pFocus->m_hWnd);
		}

		if(bFrameChild == TRUE && bGenericView == FALSE && bViewChild == FALSE)	{
			//	Try an OnCmdMessage, probably a URL bar, or message list.
			//	Walk up the list of parents until we reach ourselves.
			CWnd *pTarget = pFocus;
			while(pTarget != NULL && pTarget != (CWnd *)this)	{
				if(pTarget->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))	{
					return(TRUE);
				}
				pTarget = pTarget->GetParent();
				
                //  There are cases now where a child is actually a CGenericView
                //      such as the NavCenter HTML pane.  Do not allow it to
                //      receive these messages.
                if(pTarget->IsKindOf(RUNTIME_CLASS(CGenericView))) {
                    pTarget = NULL;
                }
			}
		}
	}

	//	Pump the message through our active context first.
    CWinCX *pActiveCX = GetActiveWinContext();
    CGenericView *pGenView = pActiveCX ? pActiveCX->GetView() : NULL;
    if(pGenView)    {
        if(pGenView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))  {
            return(TRUE);
        }
    }

	//	Next, pump the message through our main context.
    pActiveCX = GetMainWinContext();
    pGenView = pActiveCX ? pActiveCX->GetView() : NULL;
    if(pGenView)    {
        if(pGenView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))  {
            return(TRUE);
        }
    }

    // always enable bookmark entries
    if((nID >= FIRST_BOOKMARK_MENU_ID) && (nID < LAST_BOOKMARK_MENU_ID) && (nCode == CN_UPDATE_COMMAND_UI))  {
        ASSERT(pExtra);
        CCmdUI* pCmdUI = (CCmdUI*)pExtra;
        pCmdUI->Enable();
        return(TRUE);
    }

    // always enable windows menu entries
    if((nID >= ID_WINDOW_WINDOW_0) && (nID < ID_WINDOW_WINDOW_0 + 10) && (nCode == CN_UPDATE_COMMAND_UI))  {
        ASSERT(pExtra);
        CCmdUI* pCmdUI = (CCmdUI*)pExtra;
        pCmdUI->Enable();
        return(TRUE);
    }

    // was this a windows menu selection ?
    if((nID >= ID_WINDOW_WINDOW_0) && (nID < ID_WINDOW_WINDOW_0 + 10) && (nCode == CN_COMMAND))  {
        CGenericFrame * pFrame = theApp.m_pFrameList;
		BOOL bDone = FALSE;
        // walk down frame list till we get to the one we wanted
        int iToGo = CASTINT(nID - ID_WINDOW_WINDOW_0);
        while( pFrame && !bDone ) {
			if (iToGo == 0) {
				bDone = TRUE;
	            break;
			} else {
				iToGo--;
				// Fall through to default to move to next frame.
			}
			pFrame = pFrame->m_pNext;
        }

        // if we got something un-minimize it and pop it to the front
        if(pFrame) {
            if(pFrame->IsIconic())  {
                pFrame->ShowWindow(SW_RESTORE);
            }
            pFrame->BringWindowToTop();
        }

        // assume we got something or at least the message was for us
        return(TRUE);
    }

	// then pump through frame  
	return(CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo));
}


#ifdef EDITOR
// Used to gray menu/toolbar commands depending on presence of
//   context and its loading and blocked states
void CGenericFrame::OnUpdateCanInteract(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetMainWinContext() && GetMainWinContext()->GetContext());
}

void CGenericFrame::OnUpdateEditFrame(CCmdUI *pCmdUI)
{
    // enable EditFrame menu only if a frame in a frameset is selected.
	pCmdUI->Enable(GetActiveWinContext() && GetActiveWinContext()->GetContext() && GetActiveWinContext()->IsGridCell());
}


//
// NOTE: Editor-related methods of CGenericFrame are in EDFRAME.CPP
//
#endif // EDITOR

void CGenericFrame::BuildHelpMenu(CMenu * pMenu) 
{
	if (!pMenu) return; 
	if(GetMainContext() == NULL)	{
		return;
	}
	//static bChanged = TRUE;
	
	//if (!bChanged) return;

    int idx =0, idx2=0;
    int iItems = pMenu->GetMenuItemCount();	
    char *label;
	int bOK = PREF_NOERROR;

	CString helpContents;
	helpContents.LoadString(IDS_HELPCONTENTS);
	if (idx < iItems) 
		pMenu->ModifyMenu(idx,MF_BYPOSITION | MF_STRING,ID_COMMAND_HELPINDEX, helpContents);
	else
		pMenu->InsertMenu(idx,MF_BYPOSITION | MF_STRING,ID_COMMAND_HELPINDEX, helpContents);

    for (idx=1; bOK==PREF_NOERROR; idx++) {
		bOK = PREF_CopyIndexConfigString("menu.help.item",idx,"label",&label);
		
		// if the label is blank, truncate the menu here.
		if (!label || !label[0])
			bOK = PREF_ERROR;

		if (bOK == PREF_NOERROR) {
			if (idx < iItems) {
				if (!strcmpi("SEPARATOR",label) || !strcmpi("-",label))
					pMenu->ModifyMenu(idx,MF_BYPOSITION | MF_SEPARATOR,NULL,"");
				else
					pMenu->ModifyMenu(CASTUINT(idx),CASTUINT(MF_BYPOSITION | MF_STRING),CASTUINT(FIRST_HELP_MENU_ID + idx),label);
			} else {
				if (!strcmpi("SEPARATOR",label) || !strcmpi("-",label))
					pMenu->InsertMenu(idx,MF_BYPOSITION | MF_SEPARATOR,NULL,"");
				else
					pMenu->InsertMenu(CASTUINT(idx),CASTUINT(MF_BYPOSITION | MF_STRING),CASTUINT(FIRST_HELP_MENU_ID + idx),label);
			}
		}
		if (label) {
			XP_FREE(label);
			label = NULL;
		}
    }

	//  Add about page item
	//  Must be done here, such that choice is always
	//      available.
	CString aboutCommunicator;
	aboutCommunicator.LoadString(IDS_ABOUTNETSCAPE);

	if (idx < iItems) 
		pMenu->ModifyMenu(idx,MF_BYPOSITION | MF_STRING,ID_COMMAND_ABOUT, aboutCommunicator);
	else
		pMenu->InsertMenu(idx,MF_BYPOSITION | MF_STRING,ID_COMMAND_ABOUT, aboutCommunicator);
}

/****************************************************************************
*
*	CGenericFrame::OnToolsWeb
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Handles the Tools->Web menu item, and can be called by anyone else
*		(such as the task bar) that wants to perform the trick of getting the
*		proper browser window up.
*
****************************************************************************/

void CGenericFrame::OnToolsWeb()
{
	// TODO - not exactly to spec yet...
	
	// Single click on web icon means bring forth the browser window
	CAbstractCX * pCX = GetMainContext();
	if (pCX != NULL)
	{
		int nCount = FEU_GetNumActiveFrames(MWContextBrowser);
		
		if((nCount == 1 && pCX->GetContext()->type == MWContextBrowser && !EDT_IS_EDITOR(pCX->GetContext())) || nCount == 0)
			pCX->NewWindow();
		else if(nCount > 1 || !(pCX->GetContext()->type == MWContextBrowser && !EDT_IS_EDITOR(pCX->GetContext())))
		{
			CFrameWnd *pFrame;

			//if we are a browser then get the bottom most browser
			if(pCX->GetContext()->type == MWContextBrowser)
			{
				pFrame = FEU_GetBottomFrame(MWContextBrowser);
			}
			else
			// if we are not a browser, then get the last active browser
				pFrame = FEU_GetLastActiveFrame(MWContextBrowser);

			if(pFrame)
			{
				if(pFrame->IsIconic())
		            pFrame->ShowWindow(SW_RESTORE);

#ifdef _WIN32
				pFrame->SetForegroundWindow();
#else
				pFrame->SetActiveWindow();
#endif
			}
		}
	}
			
} // END OF	FUNCTION CGenericFrame::OnToolsWeb()

//
// Build the "Window" menu 
//
void CGenericFrame::OnConstructWindowMenu(CMenu * pPopup)
{
	UINT nFlags = MF_STRING | MF_ENABLED; 

    ASSERT(pPopup);
    if(!pPopup)
        return;

	if(theApp.m_hPostalLib)
		FEU_AltMail_SetAltMailMenus(pPopup); 

    // delete the old menu items
	int index = 0;
	UINT nID = 0;
    int iCount = pPopup->GetMenuItemCount();

	while ( index < iCount && nID != ID_WINDOW_WINDOW_0 ) {
		nID = pPopup->GetMenuItemID(index);
		if (nID != ID_WINDOW_WINDOW_0)
			index++;
	}

	while ( iCount > index ) {
		pPopup->RemoveMenu(index, MF_BYPOSITION);
		iCount--;
	}
    
    // list windows here

    CGenericFrame * pFrame = NULL;
    int nItemCount = 0;

	// We can't have more than 10 window menu items because we only reserved 10 IDs. Sigh.
    for(pFrame = theApp.m_pFrameList; pFrame && nItemCount < 10; pFrame = pFrame->m_pNext) {
        CAbstractCX * pContext = pFrame->GetMainContext();

        // all better be enabled
        nFlags = MF_STRING | MF_ENABLED;
        if(!pContext)
            continue;

        MWContext * pXPContext = pContext->GetContext();
        if(!pXPContext)
            continue;

        if(pFrame == this)
            nFlags |= MF_CHECKED;

  		CString csContext = "";
		CString csTitle;

		csContext.Format(_T("&%d "), nItemCount);
		csTitle.LoadString(IDS_TITLE_NOTITLE);

		switch (pXPContext->type) {
		case MWContextBrowser:
            if( EDT_IS_EDITOR(pXPContext) ){
			    csContext += szLoadString(IDS_TITLE_COMPOSE_PAGE);
            } else {
			    csContext += szLoadString(IDS_TITLE_WEB);
            }
			break;
#ifdef MOZ_MAIL_NEWS         
		case MWContextMessageComposition:
			csContext += szLoadString(IDS_TITLE_COMPOSE_MESSAGE);
			csTitle.LoadString(IDS_TITLE_NOSUBJECT);
			break;
		case MWContextMail:
		case MWContextMailMsg:
			if ( pFrame->IsKindOf( RUNTIME_CLASS( CMailNewsFrame ) ) ) {
				csContext += ((CMailNewsFrame *) pFrame)->GetWindowMenuTitle();
			}
			break;
		case MWContextAddressBook:
			csContext += szLoadString( IDS_TITLE_ADDRESSBOOK );
			break;
#endif /* MOZ_MAIL_NEWS */        
		case MWContextBookmarks:
			csContext += szLoadString( IDS_TITLE_BOOKMARKS );
			break;
		default:
			continue;
		}

		switch (pXPContext->type) {
		case MWContextMessageComposition:
		case MWContextBrowser:
			if(pXPContext->title) {
				csContext += fe_MiddleCutString(pXPContext->title, 40);
			} else {
				csContext += csTitle;
			}
			break;
		}

        if(pFrame == this)
            nFlags |= MF_CHECKED;
		pPopup->AppendMenu( nFlags, CASTUINT(ID_WINDOW_WINDOW_0 + nItemCount), csContext);

        // we added an item
        nItemCount++;
    }

    // keep windows happy
    DrawMenuBar();
}

// this deletes all of the UINT * nIDs stored in the menu map.
void CGenericFrame::DeleteMenuMapIDs(void)
{
	if(!m_pMenuMap)
		return;

	POSITION pos = m_pMenuMap->GetStartPosition();
	HMENU hMenu;
	UINT * pID;

	while(pos != NULL)
	{
		pID = NULL;
#ifdef _WIN32
		m_pMenuMap->GetNextAssoc( pos, (void*&)hMenu, (void*&)pID );
#else
		m_pMenuMap->GetNextAssoc( pos, (WORD&)hMenu, (void*&)pID );
#endif
		if(pID)
		{
			delete pID;
		}

	}


}

// This copies src into dest.
static void CopyMenu(CMenu *srcMenu, CMenu *destMenu)
{
	if (!srcMenu)
	    return;

	int nCount = srcMenu->GetMenuItemCount();
	for(int dstPos = 0, srcPos = 0; dstPos< nCount; dstPos++, srcPos++)
	{
		UINT nID = srcMenu->GetMenuItemID(srcPos);
		
		// it's a separator
		if(nID == 0)
		{
			destMenu->InsertMenu(dstPos, MF_BYPOSITION | MF_SEPARATOR);
		}
		else
		{
			char lpszMenuString[64];
			
			srcMenu->GetMenuString(srcPos, lpszMenuString, 63, MF_BYPOSITION);
			CString menuString(lpszMenuString);

			// it's a submenu
			if(nID == (UINT) -1)
			{
				CMenu *pSubMenu = srcMenu->GetSubMenu(srcPos);
				CMenu *pNewSubMenu=new CMenu;
		
				if(pNewSubMenu)
				{
					pNewSubMenu->CreatePopupMenu();
					
					CopyMenu(pSubMenu, pNewSubMenu);
					destMenu->InsertMenu(dstPos, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT)pNewSubMenu->m_hMenu, menuString);
					// detach and delete the submenu created above.
					pNewSubMenu->Detach();
					delete pNewSubMenu;
				}

			}
			// it's a regular menu item.
			else if(nID > 0)
			{
				destMenu->InsertMenu(dstPos, MF_BYPOSITION | MF_STRING, nID, menuString);
			}
		}
	}

}

//  This loads a menu from the menubar dynamically
void CGenericFrame::LoadFrameMenu(CMenu *pPopup, UINT nIndex)
{
	
	CMenu *pMenu = GetMenu();

	if(pMenu)
	{
		CMenu *pSubMenu = pMenu->GetSubMenu(nIndex);
		// verify that this menu is on the main menubar.
		if(pSubMenu && (pSubMenu == pPopup))
		{
			UINT * pID;
			// find the resource id associated with the menu
#ifdef _WIN32
			if(m_pMenuMap && m_pMenuMap->Lookup(pPopup->m_hMenu, (void*&)pID))
#else
			if(m_pMenuMap && m_pMenuMap->Lookup((WORD)pPopup->m_hMenu, (void*&)pID))
#endif
			{
				// clear everything out
				int nCount = pPopup->GetMenuItemCount();
				for(int i = nCount - 1; i >= 0; i--)
				{
					pPopup->DeleteMenu(i, MF_BYPOSITION);
				}

				CMenu *pSrcPopup = new CMenu();
				if(pSrcPopup && pID)
				{
					// load the menu and copy it into pPopup.
					if(pSrcPopup->LoadMenu(*pID));	
						CopyMenu(pSrcPopup, pPopup);
					delete pSrcPopup;
				}
			}
		}
	}

}

// Delete the menu items in the menus in m_pMenuMap
void CGenericFrame::DeleteMenuMapMenus(void)
{
	if(!m_pMenuMap)
		return;

	POSITION pos = m_pMenuMap->GetStartPosition();
	HMENU hMenu;
	UINT * pID;

	while(pos != NULL)
	{
		hMenu = NULL;
#ifdef _WIN32
		m_pMenuMap->GetNextAssoc( pos, (void*&)hMenu, (void*&)pID );
#else
		m_pMenuMap->GetNextAssoc( pos, (WORD&)hMenu, (void*&)pID );
#endif
		if(hMenu)
		{
			DeleteFrameMenu(hMenu);
		}

	}


}

void CGenericFrame::DeleteFrameMenu(HMENU hMenu)
{
	if(hMenu)
	{
		int nCount = ::GetMenuItemCount(hMenu);

		// It's easiest to do this by walking backwards over the menu items
		for (int i = nCount - 1; i >= 0; i--) {
			VERIFY(::DeleteMenu(hMenu, i, MF_BYPOSITION));
		}
	}

}



void CGenericFrame::OnInitMenuPopup(CMenu * pPopup, UINT nIndex, BOOL bSysMenu) 
{
	AfxLockTempMaps();
	// load this menu if it needs to be loaded dynamically.
	LoadFrameMenu(pPopup, nIndex);
	CFrameWnd::OnInitMenuPopup(pPopup, nIndex, bSysMenu);
	if (!bSysMenu)
	{
		if( m_pBookmarksMenu && pPopup->m_hMenu == m_pBookmarksMenu->m_hMenu)
		{
			m_BookmarksGarbageList = new CTreeItemList;

			// We need to check whether we have already built the menu.
			// Even though we destroy the menu items each time the menus go away,
			// this only happens when the user is all done with the menu bar and
			// not when each individual pop-up menu is dismissed
			//
			// Note: we use a different check to tell whether the menu has been
			// built. We do this because the bookmark menu can have pull-right
			// sub-menus and separators, neither of which have command IDs. That
			// means the check we use for history elements won't work
			if (m_nBookmarkItems == 0) {
				// Before we add the menu items, get the number of existing
				// menu items. We need to know this so we can remove the items
				// we have added
				nExistingBookmarkMenuItems = pPopup->GetMenuItemCount();
		
				// Now go ahead and add the bookmark items
				BuildBookmarkMenu((CTreeMenu*)pPopup,
							  HT_TopNode(HT_GetSelectedView(m_BookmarkMenuPane)),
							  ITEMS_BEFORE_BOOKMARKS_START);

				// if there were any bookmarks, add a separator after them
				int nCount = pPopup->GetMenuItemCount();
				if(nCount > nExistingBookmarkMenuItems && ITEMS_AFTER_BOOKMARKS_END > 0)
					pPopup->InsertMenu(nCount - ITEMS_AFTER_BOOKMARKS_END, MF_SEPARATOR | MF_BYPOSITION);
			}

			((CTreeMenu*)pPopup)->CalculateItemDimensions();		
		}  
		else if (pPopup->GetMenuItemID(0) == IDC_FIRST_PLACES_MENU_ID) 
		{
			BuildDirectoryMenu(pPopup);
		}  
		else if (pPopup->GetMenuItemID(0) == IDC_FIRST_HELP_MENU_ID) 
		{
			BuildHelpMenu(pPopup);
		} 
		else 
		{
			HT_Resource      pEntry;

			// See if this is a bookmark sub-menu
			ASSERT(m_pSubmenuMap);
			if (m_pSubmenuMap->Lookup(pPopup->GetSafeHmenu(), (void*&)pEntry)) 
			{
				// If the menu isn't empty then we've already added the bookmark
				// items
				if (pPopup->GetMenuItemCount() == 0) 
				{
					// Haven't yet added the bookmark menu items for this sub-menu.
					// Do it now
					BuildBookmarkMenu((CTreeMenu*)pPopup, pEntry);
					((CTreeMenu*)pPopup)->CalculateItemDimensions();
				}
			}

			//see if this is a file bookmark sub-menu
			if(pPopup == m_pFileBookmarkMenu)
				InitFileBookmarkMenu();

			if (m_pFileSubmenuMap->Lookup(pPopup->GetSafeHmenu(), (void*&)pEntry)) 
			{

				if(pPopup->GetMenuItemCount()==0)
				{
					BuildFileBookmarkMenu((CTreeMenu*)pPopup, pEntry);
					((CTreeMenu*)pPopup)->CalculateItemDimensions();
				}

			}
			CMenu * pFrameMenu = GetMenu();
			if (pFrameMenu != NULL)
			{
				if ((pPopup != NULL) &&
					(pPopup->GetMenuItemID(0) == ID_TOOLS_WEB))
				{
					OnConstructWindowMenu(pPopup);
				}
				if ((pPopup != NULL) &&
					(pPopup->GetMenuItemID(0) == IDC_FIRST_HELP_MENU_ID))
				{
					BuildHelpMenu(pPopup);
				}
			}
		}
	}
	// This function must always be executed, so please, do NOT put any
	// return statements in the above code!
	AfxUnlockTempMaps();
}

//
// Over-ride this cuz something is writing the status message into
//   pane 0 instead of actually looking up what the correct pane to write into
//   was.
//
LRESULT CGenericFrame::OnSetMessageString(WPARAM wParam, LPARAM lParam)
{
	CString csStatus = _T("");

	// set the message bar text
	if (lParam != NULL)
	{
		ASSERT(wParam == 0);    // can't have both an ID and a string
		// set an explicit string
		csStatus = (LPCSTR)lParam;
	} else {
		GetMessageString( (UINT) wParam, csStatus );
	}

    if( csStatus.IsEmpty() ) {
        CAbstractCX *pContext = GetMainContext();

        if( pContext ) {
            wfe_Progress( pContext->GetContext(), NULL );
        }
    }
    else {
    	CStatusBar* pMessageBar = (CStatusBar *) GetMessageBar();
    	if ( pMessageBar ) {
    		int nIndex = pMessageBar->CommandToIndex( ID_SEPARATOR );
    		if ( nIndex >= 0 ) {
    			pMessageBar->SetPaneText( nIndex, csStatus );
    		}
    	}
    }
	UINT nIDLast = m_nIDLastMessage;
	m_nIDLastMessage = (UINT)wParam;    // new ID (or 0)
	m_nIDTracking = (UINT)wParam;       // so F1 on toolbar buttons work
	return nIDLast;
}

void CGenericFrame::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if ( nFlags == 0xffff ) {
        
 		DeleteMenuMapMenus();

		HMENU hMenu = ::GetMenu(m_hWnd);

		// Not all top-level windows have a menu bar (e.g. the chromeless ones)
		// Also, Editor frame doesn't have these submenus
		if (hMenu && m_pBookmarksMenu ) {
			int nCount;
			// Menu is going away so destroy all the menu items we added (both
			// bookmark and history). Cleanup the bookmark menu first.
			HMENU hBookmarkMenu=m_pBookmarksMenu->m_hMenu;
			ASSERT(hBookmarkMenu);

			if ((nCount = ::GetMenuItemCount(hBookmarkMenu)) > 
				 ITEMS_BEFORE_BOOKMARKS_START + ITEMS_AFTER_BOOKMARKS_END) {
	
				//FileBookMark is always the first popup menu in the Bookmark menu loop through to find it.
				UINT nfileBookmarksPosition = 0; 
				while(!m_pBookmarksMenu->GetSubMenu(nfileBookmarksPosition) && nfileBookmarksPosition < m_pBookmarksMenu->GetMenuItemCount())
					nfileBookmarksPosition++;
				CTreeMenu *pFileBookmarkMenu = (CTreeMenu*)m_pBookmarksMenu->GetSubMenu(nfileBookmarksPosition);

				// Because the bookmark menu has separators and pull-right sub-menus,
				// neither of which have command IDs, we can't tell just by looking
				// at the menu items whether they should be deleted. So instead we
				// delete everything
				for (int i = nCount - 1 - ITEMS_AFTER_BOOKMARKS_END;
					 i >= ITEMS_BEFORE_BOOKMARKS_START; i--) {
					// This will delete any sub-menus as well (recursively)
					if(m_pBookmarksMenu->GetSubMenu(i) != NULL)
						m_pBookmarksMenu->GetSubMenu(i)->DestroyMenu();
					VERIFY(::DeleteMenu(hBookmarkMenu, i, MF_BYPOSITION));
				}
				//don't care about separator before bookmarks
				m_pBookmarksMenu->ClearItems(ITEMS_BEFORE_BOOKMARKS_START - SEPARATORS_BEFORE_BOOKMARKS_START, 
											 ITEMS_AFTER_BOOKMARKS_END);

				// we also need to delete items added to file bookmarks
	
				nCount = pFileBookmarkMenu->GetMenuItemCount();

				for(i = nCount - 1; i >= 0 ; i--)
				{
					// This will delete any sub-menus as well (recursively)
					if(pFileBookmarkMenu->GetSubMenu(i) != NULL)
						pFileBookmarkMenu->GetSubMenu(i)->DestroyMenu();
					VERIFY(pFileBookmarkMenu->DeleteMenu(i, MF_BYPOSITION));
				}

				pFileBookmarkMenu->ClearItems(0, 0);

				delete m_BookmarksGarbageList;
				m_nBookmarkItems = 0;  // reset the number of current bookmark menu items
				m_nFileBookmarkItems = 0;
			}

			// The maps are no longer needed
			if (m_pHotlistMenuMap)
				m_pHotlistMenuMap->RemoveAll();

			if (m_pSubmenuMap)
				m_pSubmenuMap->RemoveAll();

			if (m_pFileSubmenuMap)
			{
				m_pFileSubmenuMap->RemoveAll();
				// but we need to make sure that the file menu is still in the map.
				//FileBookMark is always the first popup menu in the Bookmark menu loop through to find it.
				UINT nfileBookmarksPosition = 0; 
				while(!m_pBookmarksMenu->GetSubMenu(nfileBookmarksPosition) && nfileBookmarksPosition < m_pBookmarksMenu->GetMenuItemCount())
					nfileBookmarksPosition++;
				CTreeMenu *pFileBookmarkMenu = (CTreeMenu*)m_pBookmarksMenu->GetSubMenu(nfileBookmarksPosition);

				if(pFileBookmarkMenu != NULL)
				{
					m_pFileSubmenuMap->SetAt(pFileBookmarkMenu->GetSafeHmenu(), 
						HT_TopNode(HT_GetSelectedView(m_BookmarkMenuPane)));
				}
			}

		}
	} else if (!(nItemID == 0 || nFlags & (MF_SEPARATOR|MF_POPUP|MF_MENUBREAK|MF_MENUBARBREAK))) {
		// If the menu item is for a bookmark item or history entry, save
		// away the client data for later. We need to do this because Windows
		// sends the WM_COMMAND for a selected menu item after sending the
		// WM_MENUSELECT to indicate the menu has been taken down. Alternatively
		// we could wait to purge the map until later, but Win32 allows client
		// data to be associated with a menu item and there would be no map in that
		// case anyway
		if (IS_BOOKMARK_ID(nItemID) || IS_HISTORY_ID(nItemID)) {
			ASSERT(m_pHotlistMenuMap);

			if (m_pHotlistMenuMap->Lookup(nItemID, nLastSelectedData))
				nLastSelectedCmdID = nItemID;
		}

		if (IS_FILE_BOOKMARK_ID(nItemID)) {
			if(m_pHotlistMenuMap->Lookup(nItemID, nLastSelectedData))
				nLastSelectedCmdID = nItemID;
		}
    } 

#ifdef EDITOR
    // Do Composer stuff
    OnMenuSelectComposer(nItemID, nFlags, hSysMenu);
#endif // EDITOR

    CFrameWnd::OnMenuSelect(nItemID, nFlags, hSysMenu);
}


// Because CTreeMenu's are owner draw, they don't handle mnemonics.  Therefore
// we have to handle them ourselves.
LRESULT CGenericFrame::OnMenuChar( UINT nChar, UINT nFlags, CMenu* pMenu )
{

	LRESULT result = 0;


	if(pMenu == m_pBookmarksMenu)
	{
		int nPosition;
		UINT nCommand;

		if(m_pBookmarksMenu->GetMnemonic(nChar, nCommand, nPosition))
		{
			if(nCommand == 0)
				return MAKELRESULT(nPosition, 2);
			else
			{
				SendMessage(WM_COMMAND, nCommand);
				return MAKELRESULT(0, 1);
			}

		}
    }
	return CFrameWnd::OnMenuChar(nChar, nFlags, pMenu);
}

// Find a popup menu matching the given ID,
//   including searching into submenus
// This is necessary in order to act upon MeasureItem
//  messages in the OnMeasureItem function.  
// Userdata will always be of type CTreeItem *
static CMenu* FindPopupMenu(CMenu* pMenu, UINT nID, DWORD userData)
{
    if( pMenu )
    {
	    int iCount= pMenu->GetMenuItemCount();

	    for( int i = 0; i < iCount; i++ )
	    {
		    CMenu* pPopup = pMenu->GetSubMenu(i);
		    //if it's a submenu then see if it is or if it contains the submenu we want 
		    if( pPopup )
		    {
			    //if the submenu and the menu we are looking for are the same then return this
			    if( (UINT)pPopup->m_hMenu == ((CTreeItem*)userData)->GetID() )
			    {
				    return pMenu;
			    }
			    // otherwise recurse to child popup
			    pPopup = FindPopupMenu( pPopup, nID, userData );

			    // check popups on this popup
			    if( pPopup )
				    return pPopup;
		    }
		    // otherwise check to see if it is the menu item that we want
		    else if( nID == pMenu->GetMenuItemID(i) )
		    {
                return pMenu;
		    }
	    }
    }
	return NULL;
}

/****************************************************************************
*
*	CGenericFrame::OnMeasureItem
*
*	PARAMETERS:
*		nIDCtl	- control ID
*		lpMI	- pointer to MEASUREITEMSTRUCT
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We must override this handler because MFC has a bug that prevents
*		the CMenu::MeasureItem() from being called for owner draw sub-menus.
*
****************************************************************************/

void CGenericFrame::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMI) 
{
	if (lpMI->CtlType == ODT_MENU)
	{
		ASSERT(lpMI->CtlID == 0);
		CMenu * pMenu = NULL;
		
#if defined(WIN32)
		_AFX_THREAD_STATE* pThreadState = AfxGetThreadState();
		if (pThreadState->m_hTrackingWindow == m_hWnd)
		{
			// start from popup
			pMenu = CMenu::FromHandle(pThreadState->m_hTrackingMenu);
		}
		else
		{
			// start from menubar
			pMenu = GetMenu();
		}
#else	// WIN16
		// start from menubar
		pMenu = GetMenu();
#endif	// WIN32
			
		ASSERT(pMenu != NULL);
		pMenu = FindPopupMenu(pMenu, lpMI->itemID, lpMI->itemData);
		if (pMenu != NULL)
		{
			pMenu->MeasureItem(lpMI);
		}
	}
	else
	{
		// Pass off to base class if not a menu
		CFrameWnd::OnMeasureItem(nIDCtl, lpMI);
	}

} // END OF	FUNCTION CGenericFrame::OnMeasureItem()

//  Scroll to the top of the document
//
void CGenericFrame::OnDocumentTop()  
{
   	MWContext *pContext = GetActiveWinContext() != NULL ? GetActiveWinContext()->GetContext() : NULL;
	if (pContext)
#ifdef EDITOR
		if (EDT_IS_EDITOR(pContext))
            EDT_BeginOfDocument(pContext, FALSE);
        else
#endif // EDITOR
			FE_SetDocPosition(GetActiveWinContext()->GetContext(), FE_VIEW, 0, 0);
}

//  Scroll to the bottom of the document
//
void CGenericFrame::OnDocumentBottom() 
{
   	MWContext *pContext = GetActiveWinContext() != NULL ? GetActiveWinContext()->GetContext() : NULL;
    if (pContext)
#ifdef EDITOR
		if (EDT_IS_EDITOR(pContext))
			EDT_EndOfDocument(pContext, FALSE);
		else
#endif // EDITOR
			FE_SetDocPosition(pContext, FE_VIEW, 0, GetActiveWinContext()->GetDocumentHeight());
}

void CGenericFrame::OnAbout()
{
    CAbstractCX * pContext = GetMainContext();
	if(pContext)	{
        if (pContext->GetContext()->type != MWContextBrowser ||
            EDT_IS_EDITOR(pContext->GetContext()))
    		pContext->NormalGetUrl(szLoadString(IDS_ABOUT_PAGE),NULL,NULL,TRUE);
        else
    		pContext->NormalGetUrl(szLoadString(IDS_ABOUT_PAGE));
	}
}

///////////////////////////////////////////////////////////////////////////////

void CGenericFrame::OnShowBookmarkWindow()
{   
    theApp.CreateNewNavCenter(NULL, TRUE, HT_VIEW_BOOKMARK);
}

///////////////////////////////////////////////////////////////////////////////

#if defined(OJI) || defined(JAVA)
void CGenericFrame::OnToggleJavaConsole()
{
#ifdef OJI
    JVMMgr* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr == NULL) 
        return;
    NPIJVMPlugin* jvm = jvmMgr->GetJVM();
    if (jvm) {
        if (jvm->IsConsoleVisible()) {
            jvm->HideConsole();
        } else {
            jvm->ShowConsole();
        }
        jvm->Release();
    }
    jvmMgr->Release();
#else
    if( LJ_IsConsoleShowing() ) {
      LJ_HideConsole();
    } else {
      LJ_ShowConsole();
    }
#endif
}

void CGenericFrame::OnUpdateJavaConsole(CCmdUI* pCmdUI)
{   
#ifdef OJI
    JVMMgr* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr == NULL) {
        pCmdUI->Enable(FALSE);
    }
    NPIJVMPlugin* jvm = jvmMgr->GetJVM();
    if (jvm) {
        if (jvm->GetJVMStatus() != JVMStatus_Failed) {
            pCmdUI->SetCheck(jvm->IsConsoleVisible());
        } else {
            pCmdUI->Enable(FALSE);
        }
        jvm->Release();
    }
    jvmMgr->Release();
#else
    if (LJJavaStatus_Failed != LJ_GetJavaStatus()) {
        pCmdUI->SetCheck( LJ_IsConsoleShowing() );
    } else {
        pCmdUI->Enable(FALSE);
    }
#endif
}
#endif  /* OJI || JAVA */

void CGenericFrame::OnTaskbar()
{
#ifdef MOZ_TASKBAR
	if(theApp.GetTaskBarMgr().IsFloating())
		theApp.GetTaskBarMgr().OnDockTaskBar();
	else
		theApp.GetTaskBarMgr().OnUnDockTaskBar();
#endif  /* MOZ_TASKBAR */
}

void CGenericFrame::OnUpdateTaskbar(CCmdUI *pCmdUI)
{
#ifdef MOZ_TASKBAR
	BOOL bUndocked = theApp.GetTaskBarMgr().IsFloating();
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_WINDOW_TASKBAR), CASTUINT(MF_BYCOMMAND | MF_STRING), CASTUINT(ID_WINDOW_TASKBAR),
                                    szLoadString(CASTUINT(bUndocked ? IDS_DOCK_TASKBAR : IDS_SHOW_TASKBAR)) );
    } else {
        pCmdUI->SetCheck(bUndocked);
    }
#endif /* MOZ_TASKBAR */


}

void CGenericFrame::OnUpdateLiveCall(CCmdUI* pCmdUI)
{   
    pCmdUI->Enable(m_bConference);
}

void CGenericFrame::OnIBMHostOnDemand()
{

	CString installDirectory, executable;
	
	if(!FEU_IsIBMHostOnDemandAvailable)
		return;

#ifdef WIN32
	CString ibmHostRegistry;

	ibmHostRegistry.LoadString(IDS_3270_REGISTRY);

	ibmHostRegistry = FEU_GetCurrentRegistry(ibmHostRegistry);
	if(ibmHostRegistry.IsEmpty())
		return;

    installDirectory = FEU_GetInstallationDirectory(ibmHostRegistry, szLoadString(IDS_INSTALL_DIRECTORY));
#else ifdef XP_WIN16
    installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_3270), szLoadString(IDS_INSTALL_DIRECTORY));
#endif
    if(!installDirectory.IsEmpty())

	{
		executable = installDirectory + szLoadString(IDS_3270HTML);

		if(((CDocTemplate *)theApp.m_ViewTmplate)->OpenDocumentFile( executable ))
		{
				return;
		}
	}
	MessageBox(szLoadString(IDS_CANT3270), szLoadString(IDS_IBMHOSTONDEMAND), MB_OK);


 }


void CGenericFrame::OnNetcaster()
{
	FEU_OpenNetcaster() ;
}

void CGenericFrame::OnAim()
{
	FEU_OpenAim();
}

void CGenericFrame::OnCalendar()
{
	WFE_StartCalendar();
}

void CGenericFrame::OnShowAdvanced()
{
	int32 prefInt;
	PREF_GetIntPref("browser.chrome.button_style",&prefInt);
	
	prefInt = (prefInt + 1) % 3;  // reverse it!
	PREF_SetIntPref("browser.chrome.button_style",prefInt);
	theApp.m_pToolbarStyle = CASTINT(prefInt);

	//theApp.m_pAdvancedToolbar = theApp.m_pAdvancedToolbar->IsYes() ? "No" : "Yes";

	CGenericFrame *pGenFrame;
	for(pGenFrame = theApp.m_pFrameList; pGenFrame; pGenFrame = pGenFrame->m_pNext) {
		LPNSTOOLBAR pIToolBar = NULL;
		pGenFrame->GetChrome()->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
		if (pIToolBar) {
	       pIToolBar->SetToolbarStyle(theApp.m_pToolbarStyle);
		   pIToolBar->Release();
		   pGenFrame->RecalcLayout();
		}
    }
}

void CGenericFrame::OnUpdateShowAdvanced( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck(theApp.m_pToolbarStyle);
	pCmdUI->Enable(TRUE);
}

void CGenericFrame::OnViewCommandToolbar()
{
	GetChrome()->ShowToolbar(ID_NAVIGATION_TOOLBAR, !GetChrome()->GetToolbarVisible(ID_NAVIGATION_TOOLBAR));
}

void CGenericFrame::OnUpdateViewCommandToolbar(CCmdUI *pCmdUI)
{
	BOOL bShow = GetChrome()->GetToolbarVisible(ID_NAVIGATION_TOOLBAR);
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_VIEW_COMMANDTOOLBAR), CASTUINT(MF_BYCOMMAND | MF_STRING), CASTUINT(ID_VIEW_COMMANDTOOLBAR),
                                    szLoadString(CASTUINT(bShow ? IDS_HIDE_COMMANDTOOLBAR : IDS_SHOW_COMMANDTOOLBAR)) );
    } else {
        pCmdUI->SetCheck(bShow);
    }
}

void CGenericFrame::OnViewNavCenter()
{
	CNSNavFrame* pFrame = GetDockedNavCenter();
	if (pFrame)
		pFrame->DeleteNavCenter();
	else theApp.CreateNewNavCenter(this);
}

void CGenericFrame::OnUpdateViewNavCenter(CCmdUI *pCmdUI)
{
	CNSNavFrame* pFrame = GetDockedNavCenter();
	BOOL bShow = (pFrame != NULL);

    if( pCmdUI->m_pMenu )
	{
        pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_VIEW_NAVCENTER), CASTUINT(MF_BYCOMMAND | MF_STRING), CASTUINT(ID_VIEW_NAVCENTER),
                                    szLoadString(CASTUINT(bShow ? IDS_HIDE_NAVCENTER : IDS_SHOW_NAVCENTER)) );
    } 
	else 
	{
        pCmdUI->SetCheck(bShow);
    }
}

void CGenericFrame::OnViewLocationToolbar()
{
	GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, !GetChrome()->GetToolbarVisible(ID_LOCATION_TOOLBAR));
}

void CGenericFrame::OnUpdateViewLocationToolbar(CCmdUI *pCmdUI)
{
	BOOL bShow = GetChrome()->GetToolbarVisible(ID_LOCATION_TOOLBAR);
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_VIEW_LOCATIONTOOLBAR), CASTUINT(MF_BYCOMMAND | MF_STRING), CASTUINT(ID_VIEW_LOCATIONTOOLBAR),
                                    szLoadString(CASTUINT(bShow ? IDS_HIDE_LOCATIONTOOLBAR : IDS_SHOW_LOCATIONTOOLBAR)) );
    } else {
        pCmdUI->SetCheck(bShow);
    }
}

static void
NiceReloadAllWindows()
{
	for(CGenericFrame *f = theApp.m_pFrameList; f; f= f->m_pNext){
		CWinCX *pContext = f->GetMainWinContext();
		if(pContext && pContext->GetContext()) {
#ifdef EDITOR
			if( EDT_IS_EDITOR(pContext->GetContext())) 
				EDT_RefreshLayout(pContext->GetContext());
			else
#endif // EDITOR
				pContext->NiceReload();
		}
	}
}

void CGenericFrame::OnIncreaseFont()
{
	CWinCX *pWinCX = GetActiveWinContext();

	if(!pWinCX)
		pWinCX = GetMainWinContext();

	if(pWinCX)
		pWinCX->ChangeFontOffset(1);

}

int CGenericFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if(CFrameWnd::OnCreate(lpCreateStruct) == -1)
        return(-1);

	ApiApiPtr(api);
	LPUNKNOWN pUnk = api->CreateClassInstance(APICLASS_CHROME, NULL);
	if (pUnk) {
		HRESULT hRes = pUnk->QueryInterface(IID_IChrome,(LPVOID*)&m_pChrome);
		ASSERT(hRes==NOERROR);
		pUnk->Release();
		m_pChrome->Initialize(this);
	}

    // guess we made it

	m_bConference = FEU_IsConfAppAvailable();

	CMenu *pMenu=GetMenu();
	if (pMenu)
	{

		#ifdef DEBUG_WHITEBOX
			pMenu->AppendMenu(MF_STRING,IDS_WHITEBOX_MENU,"&WhiteBox");
		#endif

		CString communicatorStr, bookmarksStr, fileBookmarkStr;

		//Communicator is always the second to last menu popup so adjust accordingly
		int nWindowPosition = pMenu->GetMenuItemCount() - 2;

		if(nWindowPosition != -1)
		{
			CMenu *pWindowMenu = pMenu->GetSubMenu(nWindowPosition);

			// Check if we need to add the Calendar menu item

			if(FEU_IsCalendarAvailable())
			{
				int nTaskBarPosition = WFE_FindMenuItem(pWindowMenu, ID_WINDOW_TASKBAR);

				if(nTaskBarPosition != -1)
					pWindowMenu->InsertMenu(nTaskBarPosition - 1, MF_BYPOSITION, ID_WINDOW_CALENDAR, szLoadString(IDS_CALENDAR_MENU));
			}

			if(FEU_IsIBMHostOnDemandAvailable())
			{
				int nTaskBarPosition = WFE_FindMenuItem(pWindowMenu, ID_WINDOW_TASKBAR);

				if(nTaskBarPosition != -1)
					pWindowMenu->InsertMenu(nTaskBarPosition - 1, MF_BYPOSITION, ID_WINDOW_IBMHOSTONDEMAND, szLoadString(IDS_IBMHOSTONDEMAND_MENU));
			}

 			if(FEU_IsNetcasterAvailable())
 			{
 				int nTaskBarPosition = WFE_FindMenuItem(pWindowMenu, ID_WINDOW_TASKBAR);

	#ifndef MOZ_TASKBAR
				if (nTaskBarPosition == -1)
					nTaskBarPosition = 2 ;  // Right below &Navigator on Lite version
	#endif
 
 				if(nTaskBarPosition != -1)
 					pWindowMenu->InsertMenu(nTaskBarPosition - 1, MF_BYPOSITION, ID_WINDOW_NETCASTER, szLoadString(IDS_NETCASTER_MENU));
 			}
 
			if(FEU_IsAimAvailable())
 			{
 				int nTaskBarPosition = WFE_FindMenuItem(pWindowMenu, ID_WINDOW_TASKBAR);
 
 				if(nTaskBarPosition != -1)
 					pWindowMenu->InsertMenu(nTaskBarPosition - 1, MF_BYPOSITION, ID_WINDOW_AIM, szLoadString(IDS_AIM_MENU));
 			}

	#ifdef XP_WIN32
			// We can start or activate LiveWire SiteManager
			//  if we have a registration handle
			if( bSiteMgrIsRegistered ){
				int nTaskBarPosition = WFE_FindMenuItem(pWindowMenu, ID_WINDOW_TASKBAR);

				if(nTaskBarPosition != -1)
					pWindowMenu->InsertMenu(nTaskBarPosition - 1, MF_BYPOSITION, ID_ACTIVATE_SITE_MANAGER, szLoadString(IDS_SITE_MANAGER));
			}
	#endif


			// Replace the bookmark menu with a new menu that can have ON_DRAWITEM overridden
			//BookMark is the First Popup in the Communicator Menu so loop through to find it.
			UINT nBookmarksPosition = 0;
			while(!pWindowMenu->GetSubMenu(nBookmarksPosition) && nBookmarksPosition < pWindowMenu->GetMenuItemCount())
				nBookmarksPosition++;

			if(nBookmarksPosition < pWindowMenu->GetMenuItemCount())
			{
				CMenu *pOldBookmarksMenu = pWindowMenu->GetSubMenu(nBookmarksPosition);
				char bookmarksStr[30];

				if(pOldBookmarksMenu)
				{
					m_pBookmarksMenu=new CTreeMenu;

					m_pBookmarksMenu->CreatePopupMenu();
					
					m_OriginalBookmarksGarbageList = new CTreeItemList;

					CopyMenu(pOldBookmarksMenu, m_pBookmarksMenu, m_OriginalBookmarksGarbageList);
					pWindowMenu->GetMenuString(nBookmarksPosition, bookmarksStr, 30, MF_BYPOSITION);
					pWindowMenu->DeleteMenu(nBookmarksPosition, MF_BYPOSITION);
					pWindowMenu->InsertMenu(nBookmarksPosition, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT)m_pBookmarksMenu->GetSafeHmenu(), bookmarksStr);

					//FileBookmark is the First Popup in the BookMarks Menu so loop through to find it.
					UINT nfileBookmarksPosition = 0;
					while(!m_pBookmarksMenu->GetSubMenu(nfileBookmarksPosition) && nfileBookmarksPosition < m_pBookmarksMenu->GetMenuItemCount())
						nfileBookmarksPosition++;
					m_pFileBookmarkMenu = (CTreeMenu*)m_pBookmarksMenu->GetSubMenu(nfileBookmarksPosition);

					LoadBookmarkMenuBitmaps();
				}
			}
		}
	}

#ifdef MOZ_TASKBAR
	// We use a taskbar
	theApp.GetTaskBarMgr().Reference(TRUE);
#endif // MOZ_TASKBAR
    return 0;
}


void CGenericFrame::OnDecreaseFont()
{
	CWinCX *pWinCX = GetActiveWinContext();

	if(!pWinCX)
		pWinCX = GetMainWinContext();

	if(pWinCX)
		pWinCX->ChangeFontOffset(-1);
}

void CGenericFrame::OnUpdateViewCustomToolbar(CCmdUI *pCmdUI)
{
	BOOL bShow = GetChrome()->GetToolbarVisible(ID_PERSONAL_TOOLBAR);
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_VIEW_CUSTOMTOOLBAR), CASTUINT(MF_BYCOMMAND | MF_STRING), CASTUINT(ID_VIEW_CUSTOMTOOLBAR),
                                    szLoadString(CASTUINT(bShow ? IDS_HIDE_CUSTOMTOOLBAR : IDS_SHOW_CUSTOMTOOLBAR)) );
    } else {
        pCmdUI->SetCheck(bShow);
    }
}

void CGenericFrame::OnViewCustomToolbar()
{
	GetChrome()->ShowToolbar(ID_PERSONAL_TOOLBAR, !GetChrome()->GetToolbarVisible(ID_PERSONAL_TOOLBAR));
}

void CGenericFrame::OnCustomizeToolbar()
{
	GetChrome()->Customize();
}

void CGenericFrame::OnUpdateCustomizeToolbar(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void CGenericFrame::OnUpdatePlaces(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void CGenericFrame::OnSaveConfiguration()
{
	// Save out the preferences that might have changed
	PREF_SavePrefFile();

  // Save global configuration stuff
  theApp.WriteProfileString("Main", "Temp Directory", theApp.m_pTempDir); 
    
  // security
  if(theApp.m_nSecurityCheck[SD_ENTERING_SECURE_SPACE])
    theApp.WriteProfileString("Security", "Warn Entering", "yes");
  else 
    theApp.WriteProfileString("Security", "Warn Entering", "no");

  if(theApp.m_nSecurityCheck[SD_LEAVING_SECURE_SPACE])
    theApp.WriteProfileString("Security", "Warn Leaving", "yes");
  else 
    theApp.WriteProfileString("Security", "Warn Leaving", "no");

  if(theApp.m_nSecurityCheck[SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN])
    theApp.WriteProfileString("Security", "Warn Mixed", "yes");
  else 
    theApp.WriteProfileString("Security", "Warn Mixed", "no");
    
  if(theApp.m_nSecurityCheck[SD_INSECURE_POST_FROM_INSECURE_DOC])
    theApp.WriteProfileString("Security", "Warn Insecure Forms", "yes");
  else 
    theApp.WriteProfileString("Security", "Warn Insecure Forms", "no");
}

// For Whitebox testing
#ifdef DEBUG_WHITEBOX
void CGenericFrame::OnWhiteBox() 
{
 	CWhiteBoxDlg WhiteBoxDlg;
	WhiteBoxDlg.DoModal();

}
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetscapePropertySheet dialog

BEGIN_MESSAGE_MAP(CNetscapePropertySheet, CPropertySheet)
	ON_COMMAND(IDOK, OnOK)
    ON_COMMAND(ID_HELP, OnHelp)
    ON_WM_CREATE()
    ON_WM_DESTROY()
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()

CNetscapePropertySheet::CNetscapePropertySheet(const char * pName, CWnd * pParent, UINT nStart,
                                               MWContext *pMWContext, BOOL bUseApplyButton)
    : CPropertySheet(pName, pParent, nStart),
      m_pMWContext(pMWContext),
      m_bUseApplyButton(bUseApplyButton)
{
#if defined(MSVC4) 
    // CLM: For preferences dialogs, ommitting the Apply button is good,
    //  but in the Editor, Apply is very useful, so I added this param
    //  Default behavior is to not use apply button
    if( !bUseApplyButton )
        m_psh.dwFlags |= PSH_NOAPPLYNOW;

#endif
}

void CNetscapePropertySheet::SetPageTitle (int nPage, LPCTSTR pszText)
{
    //TODO: How can we do this in Win16???

#ifndef XP_WIN16
    CTabCtrl* pTab = GetTabControl();
    ASSERT (pTab);
 
    TC_ITEM ti;
    ti.mask = TCIF_TEXT;
    ti.pszText = (char*)pszText;
    VERIFY (pTab->SetItem (nPage, &ti));
#endif

}
 


//
// Pass the help message along to the active page cuz that is the only
//   place we have easy access to the dialog ID
//
void CNetscapePropertySheet::OnHelp()
{
    CNetscapePropertyPage * pPage = (CNetscapePropertyPage *) GetActivePage();
    if(pPage)
        pPage->OnHelp();
}
                          

#ifdef XP_WIN32
BOOL CNetscapePropertySheet::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32

//
// MFC's OnOK() only calls for current page, we want to do all pages
//
void CNetscapePropertySheet::OnOK()
{

	ASSERT_VALID(this);

#if defined(MSVC4)
    // Save active page to pass out after we close down
    m_nCurrentPage = GetActiveIndex();
#endif

    if (GetActivePage()->OnKillActive())
	{
        // call OnOK for everyone
        int count = m_pages.GetSize();
        for(int i = 0; i < count; i++)
            ((CPropertyPage *) m_pages[i])->OnOK();

		if (!m_bModeless)
			EndDialog(IDOK);
	}

}

#if defined(MSVC4)
//
// Make sure the starting page is actually a page that exists
//
BOOL CNetscapePropertySheet::SetCurrentPage(int iPage)
{
	int nCurPage = 0;

    if(iPage < m_pages.GetSize())
        nCurPage = iPage;

    m_nCurrentPage = nCurPage;
    return(SetActivePage(nCurPage));
            
}
#else	
//
// Make sure the starting page is actually a page that exists
//
int CNetscapePropertySheet::SetCurrentPage(int iPage)
{
    if(iPage < m_pages.GetSize())
        m_nCurPage = iPage;
    else
        m_nCurPage = 0;

    return(m_nCurPage);
            
}
#endif	// MSVC4

#if defined(MSVC4)
// A change to MFC broke this -- it doesn't return last page
//     once the dialog is closed, which is how we always use it!
int CNetscapePropertySheet::GetCurrentPage()
{
    if( m_hWnd ){
        m_nCurrentPage = GetActiveIndex();
    }
    // We will set this in OnOK so it reflects
    //  correct page just before exiting dialog
    return m_nCurrentPage;
}
#endif

int CNetscapePropertySheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if( CPropertySheet::OnCreate(lpCreateStruct) == -1 ){
        return -1;
    }

#ifdef XP_WIN16    
    // Since we can't remove Apply button using PSH_NOAPPLYNOW
    //   in MFC 1.52, let's do it ourselves
    if( !m_bUseApplyButton ){
        CWnd *pButton = GetDlgItem(ID_APPLY_NOW);
        CRect crRectApply;
        CRect crRectCancel;

        if( pButton ){
            pButton->GetWindowRect(&crRectApply);
            ScreenToClient(&crRectApply);
            // Hide Apply button
            GetDlgItem(ID_APPLY_NOW)->ShowWindow(SW_HIDE);

            // Move the Cancel button to the Apply position
            pButton = GetDlgItem(IDCANCEL);
            if( pButton ){
                pButton->GetWindowRect(&crRectCancel);
                ScreenToClient(&crRectCancel);
                pButton->SetWindowPos(NULL, crRectApply.left, crRectApply.top, 0,0,
                                      SWP_NOACTIVATE | SWP_NOSIZE);

                // Move the OK button to the Cancel position
                pButton = GetDlgItem(IDOK);
                if( pButton ){
                    pButton->SetWindowPos(NULL, crRectCancel.left, crRectCancel.top, 0,0, 
                                          SWP_NOACTIVATE | SWP_NOSIZE);
                }
            }
        }
    }
#endif
    return 0;
}

void CNetscapePropertySheet::OnDestroy()
{
    CPropertySheet::OnDestroy();
}

extern int32 wfe_iFontSizeMode;

#ifdef FORTEZZA
/*
 * It would be nice to replace the following code with a single routine
 * that switches off the Resource ID.  (do this in CGenericFrame::OnCmdMsg)
 */
void CGenericFrame::OnStartFortezzaCard()
{
	MWContext *pContext = GetMainContext() != NULL ? 
					GetMainContext()->GetContext() : NULL;
	SSL_FortezzaMenu(pContext,SSL_FORTEZZA_CARD_SELECT);
}
void CGenericFrame::OnStartFortezzaChange()
{
	MWContext *pContext = GetMainContext() != NULL ? 
					GetMainContext()->GetContext() : NULL;
	SSL_FortezzaMenu(pContext,SSL_FORTEZZA_CHANGE_PERSONALITY);
}
void CGenericFrame::OnStartFortezzaView()
{
	MWContext *pContext = GetMainContext() != NULL ? 
					GetMainContext()->GetContext() : NULL;
	SSL_FortezzaMenu(pContext,SSL_FORTEZZA_VIEW_PERSONALITY);
}
void CGenericFrame::OnDoFortezzaInfo()
{
	MWContext *pContext = GetMainContext() != NULL ? 
					GetMainContext()->GetContext() : NULL;
	SSL_FortezzaMenu(pContext,SSL_FORTEZZA_CARD_INFO);		
}
void CGenericFrame::OnDoFortezzaLog()
{
	MWContext *pContext = GetMainContext() != NULL ? 
					GetMainContext()->GetContext() : NULL;
	SSL_FortezzaMenu(pContext,SSL_FORTEZZA_LOGOUT);
}
#endif

//
// Just call OnClose
//
void CGenericFrame::OnFrameExit()
{
    PostMessage(WM_CLOSE);
}

void CGenericFrame::OnSetFocus(CWnd* pOldWnd) 
{
	CFrameWnd::OnSetFocus(pOldWnd);

	//	Put us on the active stack.
	SetAsActiveFrame();
}

void CGenericFrame::OnWindowPosChanged(WINDOWPOS *lpWndPos)
{
	//	Call the base.
	CFrameWnd::OnWindowPosChanged(lpWndPos);

	//	Tell ncapi people window is changing position.
	if(GetMainContext() && GetMainContext()->GetContext()->type == MWContextBrowser)	{
		CWindowChangeItem::Sizing(GetMainContext()->GetContextID(), lpWndPos->x, lpWndPos->y, lpWndPos->cx, lpWndPos->cy);
	}
}

void CGenericFrame::OnWindowPosChanging(WINDOWPOS *lpWndPos)
{
	if (m_bZOrderLocked){
		lpWndPos->flags = lpWndPos->flags | SWP_NOZORDER | SWP_NOOWNERZORDER;
	}
			  
	//  Call the base
	CFrameWnd::OnWindowPosChanging( lpWndPos);
}

void CGenericFrame::OnSize(UINT nType, int cx, int cy) 
{
	CFrameWnd::OnSize(nType, cx, cy);
	
    //  See what type of information we're going to be sending to our window tracking service.
	//	ncapi
	if(GetMainContext() != NULL && GetMainContext()->GetContext()->type == MWContextBrowser)	{
	    switch(nType)   {
	        case SIZE_MAXIMIZED:
	            CWindowChangeItem::Maximizing(GetMainContext()->GetContextID());
	            break;
	        case SIZE_MINIMIZED:
	            CWindowChangeItem::Minimizing(GetMainContext()->GetContextID());
	            break;
	        case SIZE_RESTORED:
	            CWindowChangeItem::Normalizing(GetMainContext()->GetContextID());
	            break;
	    }
	}
}


BOOL CGenericFrame::OnCommand(UINT wParam,LONG lParam)
{               
#ifdef WIN32
    int nCode = HIWORD(wParam);
#else
    int nCode = HIWORD(lParam);
#endif

    UINT nID = LOWORD(wParam);
    if( nID >= ID_OPTIONS_ENCODING_1 && nID <= ID_OPTIONS_ENCODING_70)
    {
          OnToggleEncoding( nID );
          return TRUE;
    }
    if (m_bDisableHotkeys && nCode==1) {
	switch (LOWORD(wParam)) {
	    case ID_SECURITY:
	    case ID_APP_EXIT:
	    case ID_EDIT_PASTE:
	    case ID_EDIT_COPY:
	    case ID_EDIT_CUT:
		break;
	    default:
		return(TRUE);
	}
    }
    
    if(CFrameWnd::OnCommand(wParam, lParam))	{
    	return(TRUE);
	}


	// See if it was a bookmark item
	if (IS_BOOKMARK_ID(nID) && GetMainContext()) {
		// Make sure the command ID is consistent with the last
		// selected menu item
		ASSERT(nLastSelectedCmdID == nID && nLastSelectedData);
		if (nLastSelectedCmdID != nID || !nLastSelectedData)
			return FALSE;

		char* nodeURL = HT_GetNodeURL((HT_Resource)nLastSelectedData);

#ifdef EDITOR        
        // It is much safer to go through Composer's LoadUrl routine
	    if(EDT_IS_EDITOR(GetMainContext()->GetContext()))
    	    FE_LoadUrl(nodeURL, TRUE);
        else
#endif
		{
			// Add HT_Launch support
			if (!HT_Launch((HT_Resource)nLastSelectedData, GetMainContext()->GetContext()))
				GetMainContext()->NormalGetUrl(nodeURL);
		}

		return TRUE;
	}

	if(IS_FILE_BOOKMARK_ID(nID) && GetMainContext()) {
		if(nLastSelectedCmdID != nID || !nLastSelectedData)
			return FALSE;

		if(GetMainContext()) 
		{
			FileBookmark((HT_Resource)nLastSelectedData);
		}

		return TRUE;

	}

	return CommonCommand(wParam, lParam);
}

// returns TRUE if something was added to the folder, false otherwise
BOOL CGenericFrame::FileBookmark(HT_Resource pFolder)
{
	return FALSE;
}

void CGenericFrame::OnAnimationBonk()
{
	if ( GetMainContext() ) {
		GetMainContext()->NormalGetUrl( szLoadString( IDS_DIRECT_HOME ) );
	}
}

void CGenericFrame::OnUpdateFileRoot(CCmdUI* pCmdUI)
{
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(ID_BOOKMARKS_FILEROOT, MF_BYCOMMAND | MF_STRING, ID_BOOKMARKS_FILEROOT,
									HT_GetNodeName(HT_TopNode(HT_GetSelectedView(m_BookmarkMenuPane))));
    }

}

void CGenericFrame::OnNewFrame() 
{
	//      Pass it off to the context for handling.
	if(GetMainContext())        {
		GetMainContext()->NewWindow();
	}
}

void CGenericFrame::OnUpdateNewFrame(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetMainContext())        {
		pCmdUI->Enable(GetMainContext()->CanNewWindow());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

// This is used by browser or editor
//   to open a URL into either a browser 
//   or editor window (Radio buttons 
//   added to dialog to choose destination)
// If destination is a Browser and calling from Browser, 
//  open in same window (same as current behavior), 
//  for all other cases, open in new window
void CGenericFrame::OnFileOpenurl() 
{
	if(GetMainContext())        {
		GetMainContext()->OpenUrl();
	}
}

void CGenericFrame::OnUpdateFileOpenurl(CCmdUI* pCmdUI) 
{
	if(GetMainContext())        {
		pCmdUI->Enable(GetMainContext()->CanOpenUrl());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericFrame::OnHotlistAddcurrenttohotlist() 
{
	if(GetMainContext())        {
		GetMainContext()->AddToBookmarks();
	}
}

void CGenericFrame::OnUpdateHotlistAddcurrenttohotlist(CCmdUI* pCmdUI) 
{
	if(GetMainContext())        {
		pCmdUI->Enable(GetMainContext()->CanAddToBookmarks());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericFrame::OnHotlistAddcurrenttotoolbar() 
{
/* PERSONAL
	if(GetMainContext())        {
		History_entry *pHistEnt = SHIST_GetCurrent( &(GetMainContext()->GetContext()->hist) );
		ASSERT(pHistEnt);

		BM_Entry *pEntry = BM_NewUrl( GetMainContext()->GetContext()->title, pHistEnt->address, NULL, 
									  pHistEnt->last_access );
		ASSERT(pEntry);

		BM_AppendToHeader( theApp.m_pBmContext, theApp.GetLinkToolbarManager().GetToolbarHeader(), 
						   pEntry );

		theApp.GetLinkToolbarManager().ToolbarHeaderChanged();

	}
*/
}

void CGenericFrame::OnUpdateHotlistAddcurrenttotoolbar(CCmdUI* pCmdUI) 
{
	if(GetMainContext())        {
		pCmdUI->Enable(GetMainContext()->CanAddToBookmarks());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

// Give the focus to the next top-level window
// 
void CGenericFrame::OnNextWindow()
{
	// User has pressed ctrl-tab to switch windows
	  
	if (this->m_pNext) {// goto next if exists
		this->m_pNext->SetActiveWindow(); 
	} else { // else start at beginning
		if(theApp.m_pFrameList)
			theApp.m_pFrameList->SetActiveWindow();
	}   
}


void CGenericFrame::OnSaveOptions()
{
	// Save Encoding Info
	theApp.m_iCSID = m_iCSID ;	// Change Global default CSID
	PREF_SetIntPref("intl.character_set",m_iCSID);
}

LRESULT CGenericFrame::OnFindReplace(WPARAM wParam, LPARAM lParam) 
{

    CWinCX *pContext = GetActiveWinContext();

    if(!pContext)
        pContext = GetMainWinContext();

    if(pContext && pContext->GetPane())
        return(pContext->GetView()->OnFindReplace(wParam, lParam));

    return(FALSE);

}

void CGenericFrame::OnMove(int x, int y) 
{
	CFrameWnd::OnMove(x, y);
	
	//	Tell our main context that we've moved.
	if(GetMainWinContext())	{
		GetMainWinContext()->OnMoveCX();
	}
}

void CGenericFrame::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI) 
{
	//	Do the default first.
	CFrameWnd::OnGetMinMaxInfo(lpMMI);

	//	If we're not allowing resize, be sure and set the min max tracking information
	//		which effectively blocks the user from resizing the window.
	if(m_bCanResize == FALSE && IsIconic() == FALSE && IsZoomed() == FALSE)	{
		//	Set up the min and max tracking info to be the same to block resize.
		lpMMI->ptMinTrackSize.x = lpMMI->ptMaxTrackSize.x = m_crMinMaxRect.Width();
		lpMMI->ptMinTrackSize.y = lpMMI->ptMaxTrackSize.y = m_crMinMaxRect.Height();
	}
}

// Sets a flag determining wether or now we'll let the user resize the window.
void CGenericFrame::EnableResize(BOOL bEnable)
{
    long lStyles = 0;
    
    // Set the flag.
    m_bCanResize = bEnable;
    GetWindowRect(m_crMinMaxRect);

    lStyles = GetWindowLong(GetSafeHwnd(), GWL_STYLE);
    if (lStyles & WS_POPUP)
	return;
    if (bEnable) { 
	// We need to make sure the windows maximize buttons come back if we killed them.
	if (lStyles & WS_MAXIMIZEBOX)
	    return;
	lStyles |= WS_MAXIMIZEBOX;
	SetWindowLong(GetSafeHwnd(), GWL_STYLE, lStyles);
    }
    else { //The window is being set non-resizable.  Kill the windows maximize button as well.
	if (!(lStyles & WS_MAXIMIZEBOX))
	    return;
	lStyles &= ~WS_MAXIMIZEBOX;
	SetWindowLong(GetSafeHwnd(), GWL_STYLE, lStyles);
    }
}

void CGenericFrame::DisableHotkeys(BOOL bDisable)
{
    // Set the flag.
    m_bDisableHotkeys = bDisable;
}

void CGenericFrame::SetZOrder(BOOL bZLock, BOOL bBottommost)
{
    // Set the flag.
    m_bZOrderLocked = bZLock;
    m_bBottommost = bBottommost;

    if (m_bBottommost)
	m_bZOrderLocked = TRUE;
}

#ifdef WIN32
//These functions don't do anything on Win16.
void CGenericFrame::SetExStyles(DWORD wAddedExStyles, DWORD wRemovedExStyles)
{
    // Set any ws_ex_styles for the window
    m_wAddedExStyles |= wAddedExStyles;
    m_wRemovedExStyles |= wRemovedExStyles;
}

DWORD CGenericFrame::GetRemovedExStyles()
{
    // Set any ws_ex_styles we are setting for the window
    return (m_wRemovedExStyles);
}
#endif 

void CGenericFrame::SetAsPopup(HWND hPopupParent)
{

	m_hPopupParent = hPopupParent;
}

#ifdef WM_SIZING
void CGenericFrame::OnSizing(UINT nSide, LPRECT lpRect)
{
    //  User is resizing window.
    //  Save the current size of the window (used in get min max info)
    //      in case we have to hold us to that.
    GetWindowRect(m_crMinMaxRect);
}
#endif

#ifdef _WIN32
LONG CGenericFrame::OnHackedMouseWheel(WPARAM wParam, LPARAM lParam)
{
    //  Determine if we're the destination.
    BOOL bUs = TRUE;
    HWND hFocus = ::GetFocus();
    if(hFocus)  {
        //  Is the focus window our child?
        BOOL bChild = ::IsChild(m_hWnd, hFocus);
        if(bChild)    {
            //  Pass along.
            bUs = FALSE;
        }
    }

    //  Either call ourselves or pass along.
    if(bUs == TRUE) {
        //  Zero means not handled.
        return(0);
    }
    else    {
        return(::SendMessage(hFocus, msg_MouseWheel, wParam, lParam));
    }
}
#endif

// Call to start a new Browser window with supplied URL
//  or load homepage if none supplied

MWContext * wfe_CreateNavigator(char * pUrl)
{
	URL_Struct *pUrlStruct = NULL;
    
    if ( pUrl != NULL && XP_STRLEN(pUrl) > 0 )
    {
        // Create a new URL structure and copy URL string to it
        pUrlStruct = NET_CreateURLStruct(pUrl, NET_DONT_RELOAD);
        // Must clear this to correctly load URL
        if ( pUrlStruct ) {
	        pUrlStruct->fe_data = NULL;
        }
    }

    // Always create a new context 
    return CFE_CreateNewDocWindow(NULL, pUrlStruct );
}


