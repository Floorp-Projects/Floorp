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

// mainfrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"

#include "rosetta.h"
#include "dialog.h"
#include "winclose.h"
#include "cntritem.h"
#include "timer.h"
#include "cxsave.h"
#include "edt.h"
#include "mainfrm.h"
#include "edframe.h"
#include "netsvw.h"
#include "hiddenfr.h"
#include "intlwin.h"
#include "msgcom.h"
#include "custom.h"
#include "statbar.h"
#include "histbld.h"
#include "nethelp.h"
#include "mnwizard.h"
#include "ssl.h"
#include "prefapi.h"
#include "prefInfo.h"
#include "medit.h"

#include "np.h"
#include "npapi.h"			// for NPWindow

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

#define SECURE_BITMAP_INDEX 10
#define MIDSECURE_BITMAP_INDEX 9
#define UNSECURE_BITMAP_INDEX 8
//
// If we are already calling NET_ProcessNet() cuz of a socket ready message
//   do not call it again via a timer interrupt
//        
MODULE_PRIVATE int winfeInProcessNet = FALSE;
static int calledProcessNet = FALSE;

// remember a socket which got a network event while we were thinking
static long m_nSocketToDealWith = 0;         

extern CMapStringToOb DNSCacheMap;

static CHistoryMenuBuilder histBuilder;

#include "toolbar.cpp"

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics
 
#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    CGenericFrame::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    CGenericFrame::Dump(dc);
}

#endif //_DEBUG

//
// Pop up an alert box for the user
//
void CMainFrame::Alert(char * Msg)
{
    MessageBox(Msg, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);
}

//
// Pop up a message to the user.
// Return TRUE if the user clicks on the "OK" button else FALSE
//
int CMainFrame::Confirm(char * Msg)
{
    int res = GetActiveWindow()->MessageBox(Msg, szLoadString(AFX_IDS_APP_TITLE), MB_ICONQUESTION | MB_OKCANCEL);
    return(res == IDOK);
}

char * CMainFrame::Prompt(const char * Msg, const char * Dflt)
{
    // CDialogPRMT foo(this->GetActiveWindow());
    // Must always use ActiveWindow so preference dialogs are the parent,
    //   else user can move focus to preference dialog and loose prompt dialog
    CDialogPRMT foo(this->GetActiveWindow());
    return(foo.DoModal(Msg, Dflt));
}


char * CMainFrame::PromptPassword(char * Msg)
{
    // CDialogPASS foo(this);
    CDialogPASS foo(this->GetActiveWindow());
    return(foo.DoModal(Msg));
}

#define IS_IN_WINDOW(hParent,hChild)\
	(((hParent) == (hChild)) || ::IsChild(hParent, hChild))

BOOL CMainFrame::setNextTabFocus( int forward )
{
	CWinCX *pWinCX = GetMainWinContext();
	if( pWinCX ) {
		// setTabFocusNext will search siblings
		BOOL ret = pWinCX->setTabFocusNext( forward );
		if( ret ) 
			return( TRUE );
	}

	return( FALSE );
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{

	BOOL    bTranslated = CGenericFrame::PreTranslateMessage(pMsg);

	if (bTranslated)
		return TRUE;  // msg handled

	if (pMsg->message != WM_KEYDOWN)
		return FALSE;  // we're not interested in translating this message

    // Do not continue with tab processing if in an editor
	if(	(pMsg->wParam != VK_TAB && pMsg->wParam != VK_SPACE	&& pMsg->wParam != VK_RETURN)
      ||  EDT_IS_EDITOR(GetMainContext()->GetContext()) )
	  return( FALSE );

	// if a plugin has the focus, we don't touch the any (TAB,SAPCE,CR) key.
	if(	GetActiveWinContext() && GetActiveWinContext()->GetContext() ) {
		// get the plugin list
		NPEmbeddedApp * pPlugin = GetActiveWinContext()->GetContext()->pluginList;
		// check each plugin in the list, to see if a plugin is the parent
		// of the focus window.
		while( pPlugin != NULL ) {
			NPWindow* npWindow = pPlugin->wdata;
			if( npWindow && npWindow->type == NPWindowTypeWindow ) {
				// check relation of parent(plugin) - child(msg window)  
				if( npWindow->window && IS_IN_WINDOW( (HWND)npWindow->window, pMsg->hwnd) )
					return(FALSE);		// plugin has focus, dont touch TAB key
			}
			pPlugin = pPlugin->next;
		}
	}

	/*
	The windows messaging systems such as CWnd::WalkPreTranslateTree(), only
	walk from leaf to root of the window tree.

	To navigate in frames(grids), Tab navigation needs to go from one window(view)
	to its sibling. Therefor, we need to walk the tree.
	*/
	CGenericView	*pView;
	UINT			forward = GetKeyState(VK_SHIFT)<0 ? 0 : 1 ;	// 1 is forward
	UINT			controlKey = GetKeyState(VK_CONTROL)<0 ? 1 : 0 ;	// 1 controlKey, go to next group
	int				ii;

	// IF child A reached the end of tabable elements, and child B
	// has no tabable elements, we need go back to child A to set
	// tab focus. That is the "for" loop for.
	for( ii = 0; ii < 2  && !bTranslated; ii ++ ) {

		// the chrome(location and starter bar) get Tab focus first
		if( GetChrome() != NULL ) {
			if( m_tabFocusInMainFrm == TAB_FOCUS_IN_NULL ) {
	
				// first time, 
				if( pMsg->wParam != VK_TAB )     // #49928 only handle Tab key in Chrome
					return( FALSE );

				// set focus to the location box.
				bTranslated  = GetChrome()->procTabNavigation( pMsg->wParam, 1, controlKey ); 
				if( bTranslated ) {
					m_tabFocusInMainFrm = TAB_FOCUS_IN_CHROME;
					return( TRUE );
				}   // else, do nothing, fall to setting focus in views.
			} else if( m_tabFocusInMainFrm == TAB_FOCUS_IN_CHROME ) {

				if( pMsg->wParam != VK_TAB )     // #49928 only handle Tab key in Chrome 
					return( FALSE );

				bTranslated  = GetChrome()->procTabNavigation( pMsg->wParam, 0, controlKey );
				if( bTranslated ) {
					// chrome consummed the Tab key
					m_tabFocusInMainFrm = TAB_FOCUS_IN_CHROME;
					return( TRUE );
				} else {
					m_tabFocusInMainFrm = TAB_FOCUS_IN_NULL;
					// fall to setting focus in views.
				}

			}
		} else { // no chrome, user may just close the chrom
			if( m_tabFocusInMainFrm == TAB_FOCUS_IN_CHROME ) 
				m_tabFocusInMainFrm = TAB_FOCUS_IN_NULL;
				// fall to setting focus in views.
		}
		
		if( bTranslated ) 
			return(TRUE);

#ifdef DEBUG_aliu
		// we should have returned, if focus was set in chrome.
		ASSERT(  m_tabFocusInMainFrm != TAB_FOCUS_IN_CHROME  ); 
#endif
		if( ! GetActiveView() ) {		// #55453 something wrong, try my best.
			bTranslated = setNextTabFocus(forward); // setNextTabableFrame( NULL, forward );		//NULL start with the first frame
		} else {
			if( ! GetActiveView()->IsKindOf(RUNTIME_CLASS( CGenericView ) ) )
				return(FALSE);             // not html, maybe print-preview #44362

			// now try to set tab focus to the views(grids)
			if( m_tabFocusInMainFrm == TAB_FOCUS_IN_GRID ) {
				// Starting with the active view, procTabNavigation() will 
				// search siblings in this frame window.
				pView = (CGenericView *) GetActiveView( );
				
				if( pView == NULL )
					return(FALSE);

				// TODO control key is for Tab-group(Form, table) or for Frame???
				if( ! controlKey ) {
					// try active view first
					bTranslated = pView->procTabNavigation( pMsg->wParam, forward, controlKey );	// for Tab, or SpaceBar
					if ( bTranslated ) {
						m_tabFocusInMainFrm = TAB_FOCUS_IN_GRID ;
						return( TRUE );		// msg handled
					}
				}

				if( pMsg->wParam != VK_TAB )
					return( FALSE );

				// active frame has nothing to tab to.
				m_tabFocusInMainFrm = TAB_FOCUS_IN_NULL;

				// try other frames(if any), starting from this 
				// bTranslated = setNextTabableFrame( this, forward );
			}   else {    // TAB_FOCUS_IN_NULL,
				//  start with the first frame
				bTranslated = setNextTabFocus(forward);	// setNextTabableFrame( NULL, forward );
			}
		}   // if( ! GetActiveView() )

		if ( bTranslated ) {
			m_tabFocusInMainFrm = TAB_FOCUS_IN_GRID ;
			return( TRUE );		// msg handled
		}
	
	}	// for( ii = 0; ii < 2; ii ++ )
	return(FALSE);
}	// BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)


void CMainFrame::OnToggleFancyFtp()
{
	if(GetMainContext())    {
		GetMainContext()->GetContext()->fancyFTP = !GetMainContext()->GetContext()->fancyFTP;
	}
}

void CMainFrame::OnUpdateToggleFancyFtp(CCmdUI* pCmdUI)
{   
	if(GetMainContext())    {
		pCmdUI->SetCheck(GetMainContext()->GetContext()->fancyFTP);
	}
}

void CMainFrame::RefreshNewEncoding(int16 csid, BOOL bIgnore)
{
  if (m_iCSID != csid)
  {
    if(GetMainContext())    {
      int iOldCSID = m_iCSID;

      // Must set before EDT_SetEncoding.
      m_iCSID = csid;
#ifdef EDITOR      
      if( EDT_IS_EDITOR(GetMainContext()->GetContext()) )
			{
        // EDT_SetEncoding takes care of reloading.
        if ( ! EDT_SetEncoding(GetMainContext()->GetContext(), csid) ) {
            // revert m_iCSID
            m_iCSID = iOldCSID;
            return;
        }
			}
      else 
#endif /* EDITOR */      
      {
        GetMainContext()->NiceReload();
      }
    }
  }
}

//
// Toggle the showing of the location bar
//
void CMainFrame::OnOptionsTitlelocationBar()
{
	if(GetChrome()->GetToolbarVisible(ID_LOCATION_TOOLBAR)) {
		GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, FALSE);
		m_bLocationBar= FALSE;
	} else {
		GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, TRUE);
		m_bLocationBar= TRUE;
	}
}

void CMainFrame::OnUpdateOptionsTitlelocationBar(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetChrome()->GetToolbarVisible(ID_LOCATION_TOOLBAR));
}

XP_BEGIN_PROTOS
  extern char * NET_PrintNetlibStatus();
XP_END_PROTOS

void CMainFrame::OnOptionsTogglenetdebug()
{
	if(GetActiveContext() != NULL)  {
		char *rv =  NET_PrintNetlibStatus();
		FE_Alert(GetActiveContext()->GetContext(), rv);
		XP_FREE(rv);
		NET_ToggleTrace();
	}
}


void CMainFrame::OnToggleImageLoad()
{
	CWinCX *pWinCX;
	if( pWinCX = GetMainWinContext())	{
	    if( prefInfo.m_bAutoLoadImages) {
	        prefInfo.m_bAutoLoadImages = FALSE; 
			GetChrome()->ImagesButton(TRUE);
	    } else {
	        prefInfo.m_bAutoLoadImages = TRUE;; 
			GetChrome()->ImagesButton(FALSE);
	    }
	}
}

void CMainFrame::OnUpdateToggleImageLoad(CCmdUI* pCmdUI)
{
	CWinCX *pWinCX;
	if(	pWinCX = GetMainWinContext())	{
		pCmdUI->SetCheck(prefInfo.m_bAutoLoadImages);
	}
}

void CMainFrame::OnNetSearch()
{
	char * url = NULL;
	PREF_CopyConfigString("internal_url.net_search.url",&url);
	
	if(GetMainContext() && url)
	{
		GetMainContext()->NormalGetUrl(url);
		XP_FREE(url);
	}
}

void CMainFrame::OnUpdateNetSearch(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);
}


void CMainFrame::OnUpdateSecurity(CCmdUI *pCmdUI)
{
	
	HG73537
	MWContext *pContext = GetMainContext()->GetContext();

	if(EDT_IS_EDITOR(pContext))
		CGenericFrame::OnUpdateSecurity(pCmdUI);
	else
	{
		int status = XP_GetSecurityStatus(pContext);	
	
		BOOL bHasSecurity = status == SSL_SECURITY_STATUS_ON_LOW || status == SSL_SECURITY_STATUS_ON_HIGH;

		if(m_pCommandToolbar)
		{
			switch(status)
			{
				case SSL_SECURITY_STATUS_ON_LOW:
							m_pCommandToolbar->ReplaceButtonBitmapIndex(ID_SECURITY, SECURE_BITMAP_INDEX);
						    break;
				case SSL_SECURITY_STATUS_ON_HIGH:
							m_pCommandToolbar->ReplaceButtonBitmapIndex(ID_SECURITY, SECURE_BITMAP_INDEX);
							break;
				default:
					m_pCommandToolbar->ReplaceButtonBitmapIndex(ID_SECURITY, UNSECURE_BITMAP_INDEX);
					break;
			}
			pCmdUI->Enable(TRUE);
		}
		else
			pCmdUI->Enable(FALSE);
	}

}

void CMainFrame::OnUpdateSecurityStatus(CCmdUI *pCmdUI)
{
	HG73537
	int status = XP_GetSecurityStatus(GetMainContext()->GetContext());	
	pCmdUI->Enable(status == SSL_SECURITY_STATUS_ON_LOW || status == SSL_SECURITY_STATUS_ON_HIGH);
}

void CMainFrame::OnUpdateViewCommandToolbar(CCmdUI *pCmdUI)
{
	CGenericFrame::OnUpdateViewCommandToolbar(pCmdUI);

	if(!IsEditFrame() && PREF_PrefIsLocked("browser.chrome.show_toolbar"))
		pCmdUI->Enable(FALSE);
}


void CMainFrame::OnUpdateViewLocationToolbar(CCmdUI *pCmdUI)
{
	CGenericFrame::OnUpdateViewLocationToolbar(pCmdUI);

	if(!IsEditFrame() && PREF_PrefIsLocked("browser.chrome.show_url_bar"))
		pCmdUI->Enable(FALSE);
}

void CMainFrame::OnUpdateViewCustomToolbar(CCmdUI *pCmdUI)
{
	CGenericFrame::OnUpdateViewCustomToolbar(pCmdUI);

	if(!IsEditFrame() && PREF_PrefIsLocked("browser.chrome.show_directory_buttons"))
		pCmdUI->Enable(FALSE);
}

BOOL CMainFrame::OnCommand(UINT wParam,LONG lParam)
{
    if(CGenericFrame::OnCommand(wParam, lParam))
	return TRUE;

	// Extract the command ID
	UINT nID = LOWORD(wParam);
	
	// IS it a command or a menu?
	if ((nID == 0) || (LOWORD(lParam) != 0)) {
		// Menu IDs cannot be 0
		// For menus, the low-order word of lParam is zero
		return FALSE;
	}
      
    // See if it was one of the items in the history list  
	if (IS_HISTORY_ID(nID) && GetMainContext()) {
		// Make sure the command ID is consistent with the last
		// selected menu item
		histBuilder.Execute(nID);

		return TRUE;
	}
	if (IS_SHORTCUT_ID(nID) && GetMainContext()) {
		LoadShortcut(nID-FIRST_SHORTCUT_ID);
	}
			       
	return FALSE;  // no obe wanted it
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopup, UINT nIndex, BOOL bSysMenu) 
{
	AfxLockTempMaps();

	CGenericFrame::OnInitMenuPopup(pPopup,nIndex,bSysMenu);
	  
	if (!bSysMenu && pPopup) {
		// if it contains Back has the first item and nIndex is not 0 then
		// fill in the menu.  nIndex would be 0 if it's a popup or if
		// it's the first menu in the system 
		if (pPopup->GetMenuItemID(0) == ID_NAVIGATE_BACK && nIndex != 0){
		// We need to check whether we have already built the menu.
		// Even though we destroy the menu items each time the menus go away,
		// this only happens when the user is all done with the menu bar and
		// not when each individual pop-up menu is dismissed
			if (pPopup->GetMenuState(FIRST_HISTORY_MENU_ID, MF_BYCOMMAND) == (UINT)-1)
				BuildHistoryMenu(pPopup);
		}
	}

	// This function must always be executed, so please, do NOT put any
	// return statements in the above code!
	AfxUnlockTempMaps();
}

void CMainFrame::GetMessageString( UINT nID, CString& rMessage ) const
{
	// See if it's a history entry
	if (nID >= FIRST_HISTORY_MENU_ID && nID <= LAST_HISTORY_MENU_ID) {
		rMessage = histBuilder.GetURL(nID);
		if (rMessage.IsEmpty())
		{
			ASSERT(FALSE);
		}
	} else {
		CGenericFrame::GetMessageString( nID, rMessage );
	}
}

void CMainFrame::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (nFlags == 0xFFFF) {
		HMENU hMenu = ::GetMenu(m_hWnd);

		// Not all top-level windows have a menu bar (e.g. the chromeless ones)
		// Also, Editor frame doesn't have these submenus
		if (hMenu && !IsEditFrame() ) {
			int     nCount;
			UINT    nCmdID;

			// Menu is going away so destroy all the menu items we 
	
			// Now cleanup the history menu. We don't need to make any assumptions
			// about the number of menu items, because there are no separators or
			// pull-right sub-menus so we can tell by looking at the command IDs
			HMENU   hPopupMenu = FEU_FindSubmenu(hMenu, ID_NAVIGATE_BACK);
	
			if (hPopupMenu) {				
				ASSERT(hPopupMenu);
				nCount = ::GetMenuItemCount(hPopupMenu);
		
				// It's easiest to do this by walking backwards over the menu items
				for (int i = nCount - 1; i >= 0; i--) {
					nCmdID = ::GetMenuItemID(hPopupMenu, i);
		
					if (IS_HISTORY_ID(nCmdID))
						VERIFY(::DeleteMenu(hPopupMenu, i, MF_BYPOSITION));
				}
			}
		}
	}

    CGenericFrame::OnMenuSelect(nItemID, nFlags, hSysMenu);
}

void CMainFrame::BuildHistoryMenu(CMenu * pMenu) 
{
	ASSERT_VALID(pMenu);
	if (!pMenu)
		return;

	histBuilder.Fill(pMenu->m_hMenu, MAX_MENU_ITEM_LENGTH, eFILLALL);
}

LRESULT CMainFrame::OnButtonMenuOpen(WPARAM wParam, LPARAM lParam)
{
	HMENU hMenu = (HMENU) lParam;
	UINT nCommand = (UINT) LOWORD(wParam);

	if(nCommand == ID_NAVIGATE_BACK) 
	{
		histBuilder.Fill(hMenu, MAX_MENU_ITEM_LENGTH, eFILLBACKWARD);
	}
	else if(nCommand == ID_NAVIGATE_FORWARD)
	{

		histBuilder.Fill(hMenu, MAX_MENU_ITEM_LENGTH, eFILLFORWARD);
	}
	else if(nCommand == ID_PLACES)
	{
		FillPlacesMenu(hMenu);
	}

	return 1;
}

void CMainFrame::FillPlacesMenu(HMENU hMenu)
{
	CString str;
	int bOK = PREF_NOERROR;
	char * label;

	for(int i = 0; bOK == PREF_NOERROR ; i++)
	{
		bOK = PREF_CopyIndexConfigString("toolbar.places.item",i,"label",&label);
		if (bOK == PREF_NOERROR && label && label[0]) {
			if (!strcmp(label,"-"))
				AppendMenu(hMenu, MF_SEPARATOR, FIRST_SHORTCUT_ID+i, "");
			else
				AppendMenu(hMenu, MF_STRING, FIRST_SHORTCUT_ID+i, label);
			XP_FREE(label);
		}
	}

}

//lpttt->szText can only hold 80 characters
#define MAX_TOOLTIP_CHARS 79
//status can only hold 1000 characters 
#define MAX_STATUS_CHARS 999

LRESULT CMainFrame::OnFillInToolTip(WPARAM wParam, LPARAM lParam)
{
	HWND hwnd = (HWND)wParam;
	LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam;

	CToolbarButton *pButton = (CToolbarButton *)CWnd::FromHandle(hwnd);
	UINT nCommand = pButton->GetButtonCommand();
	const char * pTipText = pButton->GetToolTipText();
	const char * pText = NULL;
	if(nCommand == ID_NAVIGATE_BACK || nCommand == ID_NAVIGATE_FORWARD) 
	{
		pText = FindHistoryToolTipText(nCommand);
	}
	else if(nCommand == ID_GO_HOME)
	{
		pText = theApp.m_pHomePage;
	}
	else if(nCommand == ID_FILE_PRINT)
	{
		if(GetActiveWinContext() && GetActiveWinContext()->IsGridCell())
			pText = szLoadString(ID_PRINT_FRAME_SELECTED);
		else if(GetMainContext() && GetMainContext()->IsGridParent())
			pText = szLoadString(ID_PRINT_FRAME_NOSELECTION);
		else
			pText = szLoadString(ID_PRINT_PAGE);

	}
	if(pText == NULL || XP_STRCMP(pText, "") == 0)
		strncpy(lpttt->szText, pTipText, MAX_TOOLTIP_CHARS);
	else
		strncpy(lpttt->szText, pText, MAX_TOOLTIP_CHARS);

	return 1;
}

LRESULT CMainFrame::OnFillInToolbarButtonStatus(WPARAM wParam, LPARAM lParam)
{
	UINT nCommand = LOWORD(wParam);
	char *pStatus = (char*)lParam;

	if(nCommand == ID_FILE_PRINT)
	{
		if(GetActiveWinContext() && GetActiveWinContext()->IsGridCell())
			strncpy(pStatus, szLoadString(ID_PRINT_FRAME_SELECTED), MAX_STATUS_CHARS);
		else if(GetMainContext() && GetMainContext()->IsGridParent())
			strncpy(pStatus, szLoadString(ID_PRINT_FRAME_NOSELECTION), MAX_STATUS_CHARS);
		else
			strncpy(pStatus, szLoadString(ID_PRINT_PAGE), MAX_STATUS_CHARS);
	}
	return 1;
}

void CMainFrame::OnIncreaseFont()
{
	CGenericFrame::OnIncreaseFont();
}

void CMainFrame::OnDecreaseFont()
{
	CGenericFrame::OnDecreaseFont();
}

const char *CMainFrame::FindHistoryToolTipText(UINT nCommand)
{

		CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();

		if(!pFrame)
			return NULL;

		CAbstractCX * pCX = pFrame->GetMainContext();
		
		if (!pCX)
			return NULL;


		// Get the session history list
		XP_List* pList = SHIST_GetList(pCX->GetContext());

		// Get the pointer to the current history entry
		History_entry*  pCurrentDoc = pCX->GetContext()->hist.cur_doc_ptr;

		XP_List *pCurrentNode = XP_ListFindObject(pList, pCurrentDoc);

		if (!pCurrentNode)
			return NULL;
		
		XP_List *pNode = NULL;

		if(nCommand == ID_NAVIGATE_FORWARD)
		{
			pNode = pCurrentNode->next;
		}
		else if(nCommand == ID_NAVIGATE_BACK)
		{
			pNode = pCurrentNode->prev;
		}
		
		if(pNode == NULL)
			return NULL;

		History_entry * pEntry = (History_entry*)pNode->object;
		//if the page has no title, then the url is in the title field
		return pEntry->title;
}

void CMainFrame::OnFlushCache()
{
  FE_FlushCache();
}

void CMainFrame::OnOptionsViewToolBar()
{
	LPNSTOOLBAR pIToolBar = NULL;
	GetChrome()->QueryInterface(IID_INSToolBar, (LPVOID *) &pIToolBar );
	if ( pIToolBar && pIToolBar->GetHWnd() ) {
		if (::IsWindowVisible( pIToolBar->GetHWnd()) ) {
		   m_bShowToolbar = FALSE;
		} else {
		   m_bShowToolbar = TRUE;
		}
		pIToolBar->Show( m_bShowToolbar );
		pIToolBar->Release();
	}
}

void CMainFrame::OnHelpMenu()
{
	CGenericFrame::OnHelpMenu();
}

//
// Toggle the starter bar
//
void CMainFrame::OnOptionsShowstarterbuttons()
{
}

void CMainFrame::OnUpdateOptionsShowstarterbuttons(CCmdUI* pCmdUI)
{
}

//  Post a close message on the approprate system command, can't let
//    it handle or we GPF when closing the minimized main window.
void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam)  
{
	if(nID == SC_CLOSE) {
		PostMessage(WM_CLOSE);
		return;
	}
	  
	CGenericFrame::OnSysCommand(nID, lParam);
}


// No meaning in generic frame windows?
// The only possible thing to undo is typing in the URL/Location edit field
void CMainFrame::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(FALSE);
}
