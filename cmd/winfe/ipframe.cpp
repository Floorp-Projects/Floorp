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

// ipframe.cpp : implementation of the CInPlaceFrame class
//

#include "stdafx.h"

#include "mainfrm.h"
#include "ipframe.h"
#include "ssl.h"
#include "srvritem.h"
#include "genchrom.h"
#include "quickfil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif
#ifdef XP_WIN32
HMENU AFXAPI AfxMergeMenus(HMENU hMenuShared, HMENU hMenuSource,
	LONG* lpMenuWidths, int iWidthIndex, BOOL bMergeHelpMenus = FALSE);
#else
void AFXAPI _AfxMergeMenus(CMenu* pMenuShared, CMenu* pMenuSource,
	LONG FAR* lpMenuWidths, int iWidthIndex);
#endif

/////////////////////////////////////////////////////////////////////////////
// CInPlaceFrame

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CInPlaceFrame, COleIPFrameWnd)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CInPlaceFrame, COleIPFrameWnd)
	//{{AFX_MSG_MAP(CInPlaceFrame)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_COMMAND(ID_HOTLIST_VIEW, OnHotlistView)
    ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)
	ON_WM_INITMENUPOPUP()
	ON_WM_MENUSELECT()
	ON_WM_QUERYNEWPALETTE()
	ON_WM_PALETTECHANGED()
	ON_COMMAND(ID_TOOLS_WEB, OnToolsWeb)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#include "toolbar.cpp"

/////////////////////////////////////////////////////////////////////////////
// CInPlaceFrame construction/destruction

CInPlaceFrame::CInPlaceFrame()
{
	TRACE("Creating IPFrame\n");
	m_pProxy2Frame = NULL;
	m_bCreatedControls = FALSE;
	m_bUIActive = FALSE;
}

CInPlaceFrame::~CInPlaceFrame()
{
	TRACE("Destroying IPFrame\n");
	//	Give up being the frame.
	if(m_pProxy2Frame)	{
		GetMainWinContext()->m_pFrame = m_pProxy2Frame;
		m_pProxy2Frame->SetMainContext(GetMainContext());
		m_pProxy2Frame->SetActiveContext(GetActiveContext());
		CMainFrame* pFrame = (CMainFrame* )m_pProxy2Frame->GetFrameWnd();  
		pFrame->Release();

	}

	//	Clear our ideas of the currently active context and main context.
	//	This keeps the CFrameGlue destructor from clearing the frame pointer
	//		in the context which we just reset above.
	SetMainContext(NULL);
	SetActiveContext(NULL);
}

int CInPlaceFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	TRACE("CInPlaceFrame::OnCreate called\n");
	if (COleIPFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// CResizeBar implements in-place resizing.
	if (!m_wndResizeBar.Create(this))
	{
		TRACE0("Failed to create resize bar\n");
		return -1;      // fail to create
	}

	// By default, it is a good idea to register a drop-target that does
	//  nothing with your frame window.  This prevents drops from
	//  "falling through" to a container that supports drag-drop.
	m_dropTarget.Register(this);

	TRACE("Success in OnCreate\n");
	return 0;
}

BOOL CInPlaceFrame::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext)	{
	TRACE("CInPlaceFrame::LoadFrame called\n");

	//	Let the view know that we're the frame for the time being.
	//	We'll have to handle passing along every CFrameGlue call to
	//		the real frame along with handling it ourselves,
	//		except GetFrameWnd().
	ASSERT(pContext);
	CGenericDoc *pDoc;
	if(pContext)	{
		ASSERT(pContext->m_pCurrentDoc);
		if(pContext->m_pCurrentDoc)	{
			//	Must be a CGenericDoc.
			ASSERT(pContext->m_pCurrentDoc->IsKindOf(RUNTIME_CLASS(CGenericDoc)));
			pDoc = (CGenericDoc *)pContext->m_pCurrentDoc;

			//	Get the context.
			ASSERT(pDoc->GetContext()->IsFrameContext());
			CWinCX *pCX = (CWinCX *)pDoc->GetContext();

			//	Take over being the frame.
			m_pProxy2Frame = pCX->m_pFrame;
			CMainFrame* pFrame = (CMainFrame* )m_pProxy2Frame->GetFrameWnd();
			pFrame->AddRef();
//			pCX->m_pFrame = (CFrameGlue *)this;

			//	Set our current and active context.
			SetMainContext(pCX);
			SetActiveContext(pCX);

			//	We'll need to unhook ourselves when we close down.
		}
	}
	BOOL bRetVal = COleIPFrameWnd::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext);
	return bRetVal;

}

/////////////////////////////////////////////////////////////////////////////
// CInPlaceFrame diagnostics

#ifdef _DEBUG
void CInPlaceFrame::AssertValid() const
{
	COleIPFrameWnd::AssertValid();
}

void CInPlaceFrame::Dump(CDumpContext& dc) const
{
	COleIPFrameWnd::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CInPlaceFrame commands

CFrameWnd *CInPlaceFrame::GetFrameWnd()	{
	return((CFrameWnd *)this);
}

void CInPlaceFrame::UpdateHistoryDialog()	{
	//	Pass it off.
	if(m_pProxy2Frame)	{
		m_pProxy2Frame->UpdateHistoryDialog();
	}
}

// OnCreateControlBars is called by the framework to create control bars on the
//  container application's windows.  pWndFrame is the top level frame window of
//  the container and is always non-NULL.  pWndDoc is the doc level frame window
//  and will be NULL when the container is an SDI application.  A server
//  application can place MFC control bars on either window.
BOOL CInPlaceFrame::OnCreateControlBars(CWnd* pWndFrame, CWnd* pWndDoc) 
{
	TRACE("CInPlaceFrame::OnCreateControlBars called\n");
	//	Set the toolbar style.
//    BOOL bCreate = m_wndToolBar.Create(pWndFrame);
//	ASSERT(bCreate);
	CGenericDoc* pDoc = (CGenericDoc*)GetActiveDocument( );
	CNetscapeSrvrItem *pItem = pDoc->GetEmbeddedItem();
	if (pItem->IsShowUI()) {

		// Now that the application palette has been created (if 
		//   appropriate) we can create the url bar.  The animation 
		CCustToolbar *pCustToolbar = m_pProxy2Frame->GetChrome()->GetCustomizableToolbar();
		pCustToolbar->SetNewParentFrame((CFrameWnd*)pWndFrame);
		pCustToolbar->SetOwner(m_pProxy2Frame->GetFrameWnd());
#ifdef XP_WIN32
		// this is an undocumented function that adds a control bar
		// to the parent frame's control bar list. SetParent doesn't do this and
		// this only occurs when the control bar is created.  Since our control bar
		// was created before we knew about this parent, we miss out on the only
		// opportunity to do so. So this function is called.
		// This now gets taken care of in CCustToolbar.
		//((CFrameWnd*)pWndFrame)->AddControlBar(pCustToolbar);
#endif
	//	AddBookmarksMenu();

		//   might need custom colors so we need the palette to be around
		m_iShowURLBar = TRUE;
		m_iShowStarterBar = TRUE;
		m_iCSID = INTL_DefaultDocCharSetID(0); // Set Global default.

	//    SetBarType(m_iShowStarterBar, m_iShowURLBar);


        ASSERT( m_pProxy2Frame );    
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pProxy2Frame->GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if( pIStatusBar ) 
		{
            CNetscapeStatusBar *pStatusBar = ((CGenericStatusBar *)pIStatusBar)->GetNetscapeStatusBar();
            
            if( !pStatusBar )
            {
			    pIStatusBar->Create( pWndFrame, 0, 0, TRUE, FALSE ); // No task bar
            }
            else
            {
                pStatusBar->m_pProxy2Frame = m_pProxy2Frame;            
                
                pStatusBar->SetParent( pWndFrame );
              #ifdef XP_WIN32
                ((CFrameWnd *)pWndFrame)->AddControlBar( pStatusBar );
              #endif
                
            }
		 //   pIStatusBar->Show( TRUE );            
			pIStatusBar->Release();
		}
    
		RecalcLayout();

		//	We've created the controls.
		m_bCreatedControls = TRUE;
	}
    //  Success so far.  Also, accept drag and drop files from
    //      the file manager.
    //
    DragAcceptFiles();
	return TRUE;
}

void CInPlaceFrame::ShowControlBars(CFrameWnd* pFrameWnd, BOOL bShow)
{

	CCustToolbar *pCustToolbar = m_pProxy2Frame->GetChrome()->GetCustomizableToolbar();

	if(pCustToolbar)
	{
		if(pFrameWnd == pCustToolbar->GetParentFrame())
			pCustToolbar->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
	}

	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pProxy2Frame->GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if( pIStatusBar ) 
	{
        CNetscapeStatusBar *pStatusBar = ((CGenericStatusBar *)pIStatusBar)->GetNetscapeStatusBar();

		if(pStatusBar && pFrameWnd == pStatusBar->GetParentFrame())
		{
			pStatusBar->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
		}

		pIStatusBar->Release();
	}


}

void CInPlaceFrame::ReparentControlBars(void)
{
	CCustToolbar *pCustToolbar = m_pProxy2Frame->GetChrome()->GetCustomizableToolbar();

	if(pCustToolbar)
	{
		pCustToolbar->SetNewParentFrame(m_pProxy2Frame->GetFrameWnd());
		pCustToolbar->ShowWindow(SW_SHOW);
	}


	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pProxy2Frame->GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if( pIStatusBar ) 
	{
        CNetscapeStatusBar *pStatusBar = ((CGenericStatusBar *)pIStatusBar)->GetNetscapeStatusBar();

		if(pStatusBar)
		{
			pStatusBar->SetParent( m_pProxy2Frame->GetFrameWnd() );
#ifdef XP_WIN32
			m_pProxy2Frame->GetFrameWnd()->AddControlBar( pStatusBar );
#endif
			pIStatusBar->Show(TRUE);

		}
		pIStatusBar->Release();
	}


}


//JOKI
void CInPlaceFrame::DestroyResizeBar()
{
	//I had to resort to this rather than not creating m_wndResizeBar in ::OnCreate()
	// Reason : I could not get hold of GetActiveDocument(in OnCreate) which is used to get 
	//the pNetscapeSrvrItem in turn used to call Get_ShowInplaceUI()
	if(::IsWindow(m_wndResizeBar.m_hWnd))
			m_wndResizeBar.DestroyWindow();

	RecalcLayout();
}

BOOL CInPlaceFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	//	Pump through any active child first.
	//	This will catch active control bars, etc.
	CWinCX *pWinCX = NULL;
	CWnd *pFocus = GetFocus();
	BOOL bFrameChild;

	if(pFocus != NULL)	{
		//	Make sure it's a child of us, and not a CGenericView (we handle those below).
		//	Also make sure it's not a child of our main view.
		//		Those are handled by the view, not us.
		bFrameChild = IsChild(pFocus);
		BOOL bGenericView = pFocus->IsKindOf(RUNTIME_CLASS(CGenericView));
		BOOL bViewChild = FALSE;
		pWinCX = GetMainWinContext();
		if(pWinCX && pWinCX->GetPane())	{
            bViewChild = ::IsChild(pWinCX->GetPane(), pFocus->m_hWnd);
		}

		if(bFrameChild == TRUE && bGenericView == FALSE && bViewChild == FALSE)	{
			//	Try an OnCmdMessage, probably a URL bar, or message list.
			//	Walk up the list of parents until we reach ourselves.
			CWnd *pTarget = pFocus;
			while(pTarget != NULL && pTarget != (CWnd *)this)	{
				if(pTarget->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))	{
					TRACE("IPFr WM_CMD:%u through CWnd:%p (control bar%s?)\n", nID, pTarget, (nCode == CN_COMMAND ? "" : " ui"));
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
	if(m_pProxy2Frame->GetFrameWnd() != NULL && (nID != ID_TOOLS_WEB)) {
		if(m_pProxy2Frame->GetFrameWnd()->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
			return(TRUE);
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

        // walk down frame list till we get to the one we wanted
        while((nID >= ID_WINDOW_WINDOW_0) && (pFrame)) {

            switch(pFrame->GetMainContext()->GetContext()->type) {
            case MWContextMail:
            case MWContextNews:
                // these don't get added to the list so they don't count
                break;
            case MWContextMessageComposition:
            case MWContextBrowser:
                nID--;
                break;
            default:
                // by default windows aren't added to the list
                break;
            }

            // whether this window counted or not we need to move to the next one
            if(pFrame && nID >= ID_WINDOW_WINDOW_0) {
                pFrame = pFrame->m_pNext;
            }
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

	//	Call the base.
	//	Redundant on the view, but can't risk different implementations
	//		at this time.
	return COleIPFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}



void CInPlaceFrame::OnSetFocus(CWnd* pOldWnd) 
{
	COleIPFrameWnd::OnSetFocus(pOldWnd);
	
	//	Set ourselves on the active frame stack.
	SetAsActiveFrame();
}

void CInPlaceFrame::OnHotlistView() 
{
	// Needs to have NavCenter pop up eventually.
}

BOOL CInPlaceFrame::OnCommand(WPARAM wParam, LPARAM lParam) 
{
    if(COleIPFrameWnd::OnCommand(wParam, lParam))	{
    	return(TRUE);
	}

	CMainFrame *pMainFrame = (CMainFrame*)m_pProxy2Frame->GetFrameWnd();

	if(pMainFrame)
		if(pMainFrame->SendMessage(WM_COMMAND, wParam, lParam))
			return(TRUE);

	return(CommonCommand(wParam, lParam));
}

LRESULT CInPlaceFrame::OnFindReplace(WPARAM wParam, LPARAM lParam) 
{
    CWinCX *pContext = GetActiveWinContext();

    if(!pContext)
        pContext = GetMainWinContext();

    if(pContext && pContext->GetPane())
        return(pContext->GetView()->OnFindReplace(wParam, lParam));

    return(FALSE);
}


BOOL CInPlaceFrame::BuildSharedMenu()
{
	CGenericDoc* pDoc = (CGenericDoc*)GetActiveDocument( );
	CNetscapeSrvrItem *pItem = pDoc->GetEmbeddedItem();

	if (pItem->IsShowUI()) {
#ifdef XP_WIN32
		HMENU hMenu = GetInPlaceMenu();
#else
		// get runtime class from the doc template
		CDocTemplate* pTemplate = pDoc->GetDocTemplate();
		ASSERT_VALID(pTemplate);
		HMENU hMenu = pTemplate->m_hMenuInPlaceServer;
#endif

		// create shared menu
		ASSERT(m_hSharedMenu == NULL);
		if ((m_hSharedMenu = ::CreateMenu()) == NULL)
			return FALSE;

		// start out by getting menu from container
		memset(&m_menuWidths, 0, sizeof m_menuWidths);
		if (m_lpFrame->InsertMenus(m_hSharedMenu, &m_menuWidths) != S_OK)
		{
			::DestroyMenu(m_hSharedMenu);
			m_hSharedMenu = NULL;
			return FALSE;
		}
		// container shouldn't touch these
		ASSERT(m_menuWidths.width[1] == 0);
		ASSERT(m_menuWidths.width[3] == 0);
		ASSERT(m_menuWidths.width[5] == 0);

		// only copy the popups if there is a menu loaded
		if (hMenu == NULL)
			return TRUE;
#ifdef XP_WIN32
		// insert our menu popups amongst the container menus
		AfxMergeMenus(m_hSharedMenu, hMenu, &m_menuWidths.width[0], 1);
#else
	// insert our menu popups amongst the container menus
		_AfxMergeMenus(CMenu::FromHandle(m_hSharedMenu),
		CMenu::FromHandle(hMenu), &m_menuWidths.width[0], 1);
#endif
		// finally create the special OLE menu descriptor
		m_hOleMenu = ::OleCreateMenuDescriptor(m_hSharedMenu, &m_menuWidths);
		return m_hOleMenu != NULL;
	}
	else
		return TRUE;
}

void CInPlaceFrame::OnInitMenuPopup(CMenu * pPopup, UINT nIndex, BOOL bSysMenu) 
{
	CMainFrame *pMainFrame = (CMainFrame*)m_pProxy2Frame->GetFrameWnd();

	pMainFrame->SendMessage(WM_INITMENUPOPUP, (WPARAM)pPopup->m_hMenu, MAKELPARAM(nIndex, bSysMenu));

}

void CInPlaceFrame::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (nFlags == 0xFFFF) {
		HMENU hMenu = m_hSharedMenu;

		// Not all top-level windows have a menu bar (e.g. the chromeless ones)
		// Also, Editor frame doesn't have these submenus
		if (hMenu) {
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

	COleIPFrameWnd::OnMenuSelect(nItemID, nFlags, hSysMenu);
}

void CInPlaceFrame::OnToolsWeb()
{
	CAbstractCX *pCX = FEU_GetLastActiveFrameContext(MWContextAny);
	
	if(pCX)
		pCX->NewWindow();

}

// Replace our current bookmarks menu with the mainframe's bookmarks menu and add
// Calendar and 3270 menus
void CInPlaceFrame::AddBookmarksMenu(void)
{

	CMenu *pMenu=GetMenu();

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
		//Bookmark is the First Popup in the Communicator Menu so loop through to find it.
		int nBookmarksPosition = 0;
		while(!pWindowMenu->GetSubMenu(nBookmarksPosition) && nBookmarksPosition < pWindowMenu->GetMenuItemCount())
			nBookmarksPosition++;

		if(nBookmarksPosition < pWindowMenu->GetMenuItemCount())
		{
			CMenu *pOldBookmarksMenu = pWindowMenu->GetSubMenu(nBookmarksPosition);
			char bookmarksStr[30]; 

			if(pOldBookmarksMenu)
			{
				// get the mainframe's bookmark menu
				CMainFrame *pMainFrame = (CMainFrame*)m_pProxy2Frame->GetFrameWnd();

				CTreeMenu *pBookmarksMenu = pMainFrame->GetBookmarksMenu();

				if(pBookmarksMenu)
				{
					pWindowMenu->DeleteMenu(nBookmarksPosition, MF_BYPOSITION);
					pWindowMenu->GetMenuString(nBookmarksPosition, bookmarksStr, 30, MF_BYPOSITION);
					pWindowMenu->InsertMenu(nBookmarksPosition, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT)pBookmarksMenu->GetSafeHmenu(), bookmarksStr);
				}

			}
		}

	}



}

BOOL CInPlaceFrame::OnQueryNewPalette() 
{
	CCustToolbar *pCustToolbar = m_pProxy2Frame->GetChrome()->GetCustomizableToolbar();
	CWnd* parent = pCustToolbar->GetParent( );

	pCustToolbar->SendMessageToDescendants(WM_PALETTECHANGED, (WPARAM)this->GetSafeHwnd());

	return COleIPFrameWnd::OnQueryNewPalette();
}

void CInPlaceFrame::OnPaletteChanged(CWnd* pFocusWnd) 
{
	CCustToolbar *pCustToolbar = m_pProxy2Frame->GetChrome()->GetCustomizableToolbar();
	CWnd* parent = pCustToolbar->GetParent( );
	pCustToolbar->SendMessageToDescendants(WM_PALETTECHANGED, (WPARAM)pFocusWnd->GetSafeHwnd());
	COleIPFrameWnd::OnPaletteChanged(pFocusWnd);
}



HMENU CInPlaceFrame::GetInPlaceMenu()
{
	// get active document associated with this frame window
	CDocument* pDoc = GetActiveDocument();
	ASSERT_VALID(pDoc);

	// get in-place menu from the doc template
	CDocTemplate* pTemplate = pDoc->GetDocTemplate();
	if (pTemplate)
		return pTemplate->m_hMenuInPlaceServer;
	else
		return NULL;
}
