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
#include "prefapi.h"
#include "template.h"
#include "fldrfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "msgcom.h"
#include "wfemsg.h"
#include "mailpriv.h"
#include "prefs.h"
#include "dspppage.h"

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CFolderFrame, CMsgListFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

CFolderFrame *g_pFolderFrame = NULL;

const UINT FolderCodes[] = {
	ID_FILE_GETNEWMAIL,
	ID_FILE_NEWMESSAGE,
	ID_FILE_NEWFOLDER,
	ID_FILE_SUBSCRIBE,
	ID_NAVIGATE_INTERRUPT
};

/////////////////////////////////////////////////////////////////////////////
// CFolderFrame

CFolderFrame::CFolderFrame()
{
	m_iMessageMenuPos = 2;
	m_iMoveMenuPos = 2;
	m_iCopyMenuPos = -1;
	m_iAddAddressBookMenuPos = -1;

	m_iFileMenuPos = -1;
	m_iAttachMenuPos = -1;
}

void CFolderFrame::SelectFolder( MSG_FolderInfo *folderInfo )
{
	ASSERT( m_pPane && m_pOutliner );
	if ( m_pPane && m_pOutliner ) {
		MSG_ViewIndex index = MSG_GetFolderIndexForInfo( m_pPane, folderInfo, TRUE );
		if ( index != MSG_VIEWINDEXNONE ) {
			m_pOutliner->SelectItem( CASTINT(index) );
		}
	}
}

void CFolderFrame::PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
								MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if ( notify == MSG_PaneNotifySelectNewFolder ) {
		if ( value >= 0 ) {
			m_pOutliner->SelectItem( CASTINT(value) );
		}
	}
}

void CFolderFrame::GetMessageString( UINT nID, CString& rMessage ) const
{
	if (nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID) {
		rMessage.LoadString(IDS_STATUS_MOVEFOLDER);
	} else {
		CMsgListFrame::GetMessageString(nID, rMessage);
	}
}

BOOL CFolderFrame::OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext )
{
	BOOL res = CMsgListFrame::OnCreateClient( lpcs, pContext );
	if (res) {
		CMailNewsCX *pCX;

		pCX = new CMailNewsCX( MailCX, this );

		SetMainContext(pCX);
		SetActiveContext(pCX);

		pCX->GetContext()->fancyFTP = TRUE;
		pCX->GetContext()->fancyNews = TRUE;
		pCX->GetContext()->intrupt = FALSE;
		pCX->GetContext()->reSize = FALSE;
		pCX->GetContext()->type = MWContextMail;

		m_pMaster = WFE_MSGGetMaster();
		m_pPane = MSG_CreateFolderPane( GetMainContext()->GetContext(), m_pMaster );

        CFolderView *pView = (CFolderView *) GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
        ASSERT(pView && pView->IsKindOf(RUNTIME_CLASS(CFolderView)));

		m_pOutliner = (CMailNewsOutliner *) pView->m_pOutlinerParent->m_pOutliner;
		pView->m_pOutlinerParent->CreateColumns ( );
		m_pOutliner->SetPane(m_pPane);

		MSG_SetFEData( m_pPane, (LPVOID) (LPUNKNOWN) (LPMSGLIST) this );

		m_pOutliner->SelectItem(0);
	}
	return res;
}

BEGIN_MESSAGE_MAP( CFolderFrame, CMsgListFrame )
	ON_WM_SETFOCUS()
	ON_WM_QUERYNEWPALETTE()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()

	ON_COMMAND(ID_FILE_NEWFOLDER, OnNew)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWFOLDER, OnUpdateNew)

	ON_COMMAND(ID_FILE_UPDATECOUNTS, OnUpdateView)
	ON_UPDATE_COMMAND_UI(ID_FILE_UPDATECOUNTS, OnUpdateUpdateView)
	// Organize Menu
#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE(FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnMove )
	ON_COMMAND_RANGE(FIRST_COPY_MENU_ID, LAST_COPY_MENU_ID, OnCopy )
#endif
	ON_COMMAND(ID_FOLDER_SELECT, OnSelect)

	ON_COMMAND(ID_FILE_OPENFOLDER, OnOpen )
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENFOLDER, OnUpdateOpen )
	ON_COMMAND(ID_FILE_OPENFOLDERNEW, OnOpenNew )
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENFOLDERNEW, OnUpdateOpen )
	ON_COMMAND(ID_FILE_OPENFOLDERREUSE, OnOpenReuse )
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENFOLDERREUSE, OnUpdateOpen )

	ON_COMMAND(ID_HOTLIST_ADDCURRENTTOHOTLIST, OnFileBookmark)
	ON_UPDATE_COMMAND_UI(ID_HOTLIST_ADDCURRENTTOHOTLIST, OnUpdateFileBookmark)
END_MESSAGE_MAP()

#ifndef ON_COMMAND_RANGE

BOOL CFolderFrame::OnCommand( WPARAM wParam, LPARAM lParam )
{
	UINT nID = wParam;

	if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
		OnMove( nID );
		return TRUE;
	}
	if ( nID >= FIRST_COPY_MENU_ID && nID <= LAST_COPY_MENU_ID ) {
		OnCopy( nID );
		return TRUE;
	}
	return CMailNewsFrame::OnCommand( wParam, lParam );
}

BOOL CFolderFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{ 
	if (nCode == CN_UPDATE_COMMAND_UI) {
		CCmdUI* pCmdUI = (CCmdUI*)pExtra;
		if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
			OnUpdateFile( pCmdUI );
			return TRUE;
		}
	}
	return CMsgListFrame::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

#endif

void CFolderFrame::OnSetFocus( CWnd *pWndOld )
{
	CMsgListFrame::OnSetFocus( pWndOld );
	if (m_pOutliner)
		m_pOutliner->SetFocus();
}

int CFolderFrame::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	int res = CMsgListFrame::OnCreate( lpCreateStruct );

	if ( res != -1 ) {
		// Set up toolbar

		CString csFullString, cs;
		csFullString.LoadString( IDR_MAILFRAME );
		AfxExtractSubString( cs, csFullString, 0 );
		m_pChrome->SetWindowTitle(cs);

		//I'm hardcoding string since I don't want it translated.
		m_pChrome->CreateCustomizableToolbar("Folders", 3, TRUE);
		CButtonToolbarWindow *pWindow;
		BOOL bOpen, bShowing;

		int32 nPos;

		//I'm hardcoding because I don't want this translated
		m_pChrome->LoadToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"), nPos, bOpen, bShowing);

		LPNSTOOLBAR pIToolBar;
		m_pChrome->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
		if ( pIToolBar ) {
			pIToolBar->Create( this, WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE|CBRS_TOP );
			pIToolBar->SetButtons( FolderCodes, sizeof(FolderCodes) / sizeof(UINT) );
			pIToolBar->SetSizes( CSize( 29, 27 ), CSize( 23, 21 ) );
			pIToolBar->LoadBitmap( MAKEINTRESOURCE( IDB_FOLDERTOOLBAR ) );
			pIToolBar->SetToolbarStyle( theApp.m_pToolbarStyle );
			pWindow = new CButtonToolbarWindow(CWnd::FromHandlePermanent(pIToolBar->GetHWnd()), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
			m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_NAVIGATION_TOOLBAR, pWindow,nPos, 50, 37, 0, CString(szLoadString(ID_NAVIGATION_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
			m_pChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, bShowing);

			pIToolBar->Release();
		}

		m_pInfoBar = new CContainerInfoBar;
		m_pInfoBar->Create(this, m_pPane);

		//I'm hardcoding because I don't want this translated
		m_pChrome->LoadToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"), nPos, bOpen, bShowing);

		CToolbarWindow *pToolWindow = new CToolbarWindow(m_pInfoBar, theApp.m_pToolbarStyle, 27, 27, eSMALL_HTAB);
		m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_LOCATION_TOOLBAR, pToolWindow,nPos, 27, 27, 0, CString(szLoadString(ID_LOCATION_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
		m_pChrome->ShowToolbar(ID_LOCATION_TOOLBAR, bShowing);

		m_barStatus.Create(this, FALSE, TRUE);

		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->Attach(&m_barStatus);
			pIStatusBar->Release();
		}

		RecalcLayout();

		((CMailNewsCX *) GetMainContext())->SetChrome( m_pChrome );
		g_pFolderFrame = this;

		m_pInfoBar->Update();
	}
	return res;
}

void CFolderFrame::OnClose()
{
	int16 left, top, width, height;
	int32 prefInt;
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(&wp);
	CRect rect(wp.rcNormalPosition);

	left = (int16) rect.left;
	top = (int16) rect.top;
	width = (int16) rect.Width();
	height = (int16) rect.Height();
	prefInt = wp.showCmd;

	PREF_SetRectPref("mailnews.folder_window_rect", left, top, width, height);
	PREF_SetIntPref("mailnews.folder_window_showwindow", prefInt);

	//I'm hardcoding because I don't want this translated
	m_pChrome->SaveToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"));
	m_pChrome->SaveToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"));

	CMsgListFrame::OnClose();
}

void CFolderFrame::OnDestroy()
{
	CMsgListFrame::OnDestroy();

	g_pFolderFrame = NULL;
}

LPCTSTR CFolderFrame::GetWindowMenuTitle()
{
	return szLoadString(IDS_TITLE_COLLECTIONS);
}

CFolderFrame *CFolderFrame::Open()
{
	if (!CheckWizard())
		return NULL;

	if (!g_pFolderFrame) {
		g_MsgPrefs.m_pFolderTemplate->OpenDocumentFile(NULL);
	} 
	
	if (g_pFolderFrame) {
		g_pFolderFrame->ActivateFrame();
	}
	return g_pFolderFrame;
}

CFolderFrame *CFolderFrame::Open( MSG_FolderInfo *folderInfo)
{
	Open();
	if ( g_pFolderFrame ) {
		g_pFolderFrame->SelectFolder( folderInfo );
	}
	return g_pFolderFrame;
}

CFolderFrame *CFolderFrame::OpenNews( )
{
	MSG_FolderInfo *folderInfo = NULL;
	MSG_GetFoldersWithFlag( WFE_MSGGetMaster(),
							MSG_FOLDER_FLAG_NEWS_HOST,
							&folderInfo, 1 );

	CFolderFrame *pFrame = Open( folderInfo );

	return pFrame;
}

void CFolderFrame::OnNew()
{
	MSG_FolderInfo *folderInfo = GetCurFolder();
	MSG_FolderLine folderLine;
	folderLine.flags = 0;
	if (folderInfo)
		MSG_GetFolderLineById(WFE_MSGGetMaster(), folderInfo, &folderLine);

	if (folderLine.flags & (MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_NEWS_HOST)) {
		OnNewNewsgroup();
	} else {
		MWContext *pXPCX = NULL;
		MWContextType saveType;
		if (m_pPane)
		{
			pXPCX = MSG_GetContext( m_pPane );
			saveType = pXPCX->type;
		}
		CNewFolderDialog newFolderDlg( this, m_pPane, MSG_SuggestNewFolderParent(GetCurFolder(), WFE_MSGGetMaster()) );
		newFolderDlg.DoModal();
		if (m_pPane)
			pXPCX->type = saveType;	
	}
}

void CFolderFrame::OnUpdateNew(CCmdUI *pCmdUI)
{
	MSG_FolderInfo *folderInfo = GetCurFolder();
	MSG_FolderLine folderLine;
	folderLine.flags = 0;
	if (folderInfo)
		MSG_GetFolderLineById(WFE_MSGGetMaster(), folderInfo, &folderLine);

	if (folderLine.flags & (MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_NEWS_HOST)) {
		pCmdUI->SetText(szLoadString(IDS_MENU_NEWNEWSGROUP));
		OnUpdateNewNewsgroup(pCmdUI);
	} else {
		pCmdUI->SetText(szLoadString(IDS_MENU_NEWFOLDER));
		pCmdUI->Enable( TRUE );
	}
}

void CFolderFrame::OnUpdateView()
{
	DoCommand(MSG_UpdateMessageCount);
}

void CFolderFrame::OnUpdateUpdateView( CCmdUI *pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_UpdateMessageCount);
}

void _ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure)
{
	if (::IsWindow(hwnd)) {
		::ShowWindow(hwnd,SW_SHOW);
		::UpdateWindow(hwnd);
	}

	if (pane)
		MSG_DownloadFolderForOffline(WFE_MSGGetMaster(), pane, (MSG_FolderInfo *) closure);
}


void CFolderFrame::OnMove(UINT nID)
{		    
	if ( m_pPane ) {
		MSG_FolderInfo *folderInfo = FolderInfoFromMenuID( nID );

		ASSERT(folderInfo);
		if (folderInfo) {
			MSG_ViewIndex *indices;
			int count;
			m_pOutliner->GetSelection( indices, count );

			MSG_MoveFoldersInto( m_pPane, indices, count, folderInfo);
			ModalStatusBegin( MODAL_DELAY );
		}
	}
}

void CFolderFrame::OnCopy(UINT nID)
{		    
}

void CFolderFrame::OnUpdateFile(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE); // WHS folders only
}

void CFolderFrame::OnSelect()
{
}

MSG_FolderInfo *CFolderFrame::GetCurFolder() const
{
	MSG_FolderInfo *res = NULL;
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );

	if ( count == 1 ) {
		res = MSG_GetFolderInfo( m_pPane, indices[0] );
	}
	return res;
}

MSG_FolderInfo *CFolderFrame::FindFolder( LPCSTR lpszName, MSG_FolderInfo *root )
{
	MSG_Master *pMaster = WFE_MSGGetMaster();
	MSG_FolderInfo *res = NULL;
	MSG_FolderInfo **apFolderInfo = NULL;
	int i;
	int iLines  = CASTINT(MSG_GetFolderChildren ( pMaster, root, NULL, 0));

	apFolderInfo = new MSG_FolderInfo*[iLines];
	ASSERT(apFolderInfo);

	if ( apFolderInfo ) {
		MSG_GetFolderChildren ( pMaster, root, apFolderInfo, iLines );
		for ( i = 0; i < iLines && !res; i++ ) {
			MSG_FolderLine folderLine;
			MSG_GetFolderLineById( pMaster, apFolderInfo[i], &folderLine );

			if ( !stricmp( folderLine.name, lpszName ) ||
				 (folderLine.prettyName && !strcmp( folderLine.prettyName, lpszName )) ) {
				res = apFolderInfo[i];
			} else if ( folderLine.numChildren > 0 ) {
				res = FindFolder( lpszName, apFolderInfo[i] );
			}
		}
	}
	delete [] apFolderInfo;

	return res;
}

void CFolderFrame::DoOpenFolder(BOOL bReuse)
{
	MSG_ViewIndex *indices;
	int i, count;
	
	m_pOutliner->GetSelection(indices, count);
	for ( i = 0; i < count; i++ ) 
	{
		MSG_FolderLine folderLine;
		if ( MSG_GetFolderLineByIndex(m_pPane, indices[i], 1, &folderLine) ) 
		{
			C3PaneMailFrame *pFrame = C3PaneMailFrame::FindFrame( folderLine.id );

			if ( !pFrame ) 
			{
				if ( (i > 0) || !bReuse || !(pFrame = GetLastThreadFrame()) ) 
				{
					pFrame = C3PaneMailFrame::Open(folderLine.id);
				}
			}

			if (pFrame)
			{
				pFrame->ActivateFrame();
				pFrame->LoadFolder(folderLine.id);
			}
		}
	}
}

void CFolderFrame::OnOpen( )
{
	if (!m_pOutliner)
		return;

	BOOL bReuse = g_MsgPrefs.m_bThreadReuse;
	if (GetKeyState(VK_MENU) & 0x8000)
		bReuse = !bReuse;

	DoOpenFolder(bReuse);
}

void CFolderFrame::OnOpenNew( )
{
	DoOpenFolder(FALSE);
}

void CFolderFrame::OnOpenReuse( )
{
	DoOpenFolder(TRUE);
}

void CFolderFrame::OnUpdateOpen( CCmdUI *pCmdUI )
{
	MSG_ViewIndex *indices;
	int count;

	m_pOutliner->GetSelection( indices, count );

	if (pCmdUI->m_nID == ID_FILE_OPENFOLDER) {
		CString cs;
		if (count == 1) {
			MSG_FolderLine folderLine;
			if ( MSG_GetFolderLineByIndex(m_pPane, indices[0], 1, &folderLine) ) {
				if (folderLine.flags & MSG_FOLDER_FLAG_MAIL) {
					cs.LoadString(IDS_POPUP_OPENFOLDER);
				} else {
					cs.LoadString(IDS_POPUP_OPENNEWSGROUP);
				}
			}
		}
		if (cs.IsEmpty()) {
			cs.LoadString(IDS_POPUP_OPENSELECTION);
		}
		cs += szLoadString(IDS_OPENACCEL);
		pCmdUI->SetText(cs);
	}

	BOOL bEnable = count > 0;
	for (int i = 0; i < count; i++) {
		MSG_FolderLine folderLine;
		if ( MSG_GetFolderLineByIndex(m_pPane, indices[0], 1, &folderLine) ) {
			if (folderLine.level < 2)
				bEnable = FALSE;
		} else {
			bEnable = FALSE;
		}
	}
	pCmdUI->Enable( bEnable );
}

#define WFE_PREF_BITS		0x40000002
#define WFE_PREF_DEFAULT	0x00000000

void CFolderFrame::SetFolderPref( MSG_FolderInfo *folderInfo, uint32 flag )
{
	uint32 flags = MSG_GetFolderPrefFlags( folderInfo );
	if ( !(flags & MSG_FOLDER_PREF_FEVALID) ) {
		flags |= WFE_PREF_DEFAULT|MSG_FOLDER_PREF_FEVALID;	
	}
	flags |= flag;
	MSG_SetFolderPrefFlags( folderInfo, flags );
}

void CFolderFrame::ClearFolderPref( MSG_FolderInfo *folderInfo, uint32 flag )
{
	uint32 flags = MSG_GetFolderPrefFlags( folderInfo );
	if ( !(flags & MSG_FOLDER_PREF_FEVALID) ) {
		flags |= WFE_PREF_DEFAULT|MSG_FOLDER_PREF_FEVALID;	
	}
	flags &= ~flag;
	MSG_SetFolderPrefFlags( folderInfo, flags );
}

BOOL CFolderFrame::IsFolderPrefSet( MSG_FolderInfo *folderInfo, uint32 flag )
{
	uint32 flags = MSG_GetFolderPrefFlags( folderInfo );
	if ( !(flags & MSG_FOLDER_PREF_FEVALID ) ) {
		flags |= WFE_PREF_DEFAULT;
	}
	return (flags & flag) ? TRUE : FALSE;
}

void CFolderFrame::OnFileBookmark()
{
	FileBookmark();
}

void CFolderFrame::OnUpdateFileBookmark(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
}

BOOL CFolderFrame::FileBookmark()
{
	BOOL res = FALSE;
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);

	if (count < 1) {
		HT_AddBookmark("mailbox:", (char*)(szLoadString(IDS_TITLE_COLLECTIONS))); 
		// Updated to Aurora (Dave H.)
		res = TRUE;
	} else {
		// Add bookmark to each selected folder
		MSG_FolderLine folderLine;
		for (int i = 0; i < count; i++) {
			if (MSG_GetFolderLineByIndex(m_pPane, indices[i], 1, &folderLine)) {
				URL_Struct *url = MSG_ConstructUrlForFolder( m_pPane, folderLine.id );

				if ( url ) {			
					const char *name = (folderLine.prettyName && folderLine.prettyName[0]) ?
									   folderLine.prettyName : folderLine.name;

					HT_AddBookmark(url->address, (char*)name); // updated to Aurora (Dave H.)
					NET_FreeURLStruct( url );
					res = TRUE;
				}
			}
		}
	}

	return res;
}

CFolderFrame *WFE_MSGOpenFolders()
{
	return CFolderFrame::Open();
}

CFolderFrame *WFE_MSGOpenNews()
{
	return CFolderFrame::OpenNews();
}
