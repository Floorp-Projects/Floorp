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
// msgfrm.cpp : implementation file
//

#include "stdafx.h"
#include "intl_csi.h"
#include "msgcom.h"
#include "prefapi.h"
#include "apimsg.h"
#include "wfemsg.h"
#include "mailmisc.h"
#include "mailpriv.h"
#include "netsvw.h"
#include "mailfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "msgview.h"
#include "template.h"
#include "mailqf.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CMessageFrame, CMailNewsFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CMessageFrame

CMessageFrame::CMessageFrame()
{
	m_pPane = NULL;
	m_pMessagePane = NULL;

	m_bNavPending = FALSE;
	m_navPending = MSG_FirstMessage;

   // All our favorite hotkeys
   LoadAccelerators( IDR_ONEKEYMESSAGE );

	m_iAttachMenuPos = 2;
}

CMessageFrame::~CMessageFrame()
{
	// Menu's destroyed for us

    if(m_hAccelTable != NULL)   {
        ::FreeResource((HGLOBAL)m_hAccelTable);
        m_hAccelTable = NULL;
    }
}

/////////////////////////////////////////////////////////////////////////////
// CMessageFrame Overloaded methods

extern UINT MailCodes[10];
extern int MailIndices[10];

extern UINT NewsCodes[10];
extern int NewsIndices[10];

BOOL CMessageFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	// Skip up to CGenericFrame, since CMailNewsFrame doesn't call it
	BOOL res = CGenericFrame::OnCreateClient( lpcs, pContext );
	if (res) {
		CWnd *pView = GetDescendantWindow(IDW_MESSAGE_PANE, TRUE);
		ASSERT(pView);

		CWinCX *pWinCX;
		pWinCX = new CWinCX((CGenericDoc *) pContext->m_pCurrentDoc, 
							this,
							(CGenericView *)pView);

		pView = GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
		m_pMessageView = DYNAMIC_DOWNCAST(CMessageView, pView);
		ASSERT(m_pMessageView);

		SetMainContext(pWinCX);
		SetActiveContext(pWinCX);

		RECT rect;
		GetClientRect(&rect);
		pWinCX->Initialize(FALSE, &rect);
		pWinCX->GetContext()->type = MWContextMailMsg;
		pWinCX->GetContext()->fancyFTP = TRUE;
		pWinCX->GetContext()->fancyNews = TRUE;
		pWinCX->GetContext()->intrupt = FALSE;
		pWinCX->GetContext()->reSize = FALSE;

		m_pMaster = WFE_MSGGetMaster();
		m_pPane = MSG_CreateMessagePane( pWinCX->GetContext(), m_pMaster );
		m_pMessagePane = m_pPane;

		MSG_SetFEData(m_pPane, (LPVOID) (LPUNKNOWN) this );
		MSG_SetMessagePaneCallbacks(m_pMessagePane, &MsgPaneCB, NULL);
	}

	return res;
}

void CMessageFrame::DoUpdateNavigate( CCmdUI* pCmdUI, MSG_MotionType msgCommand )
{
	XP_Bool enable = FALSE;

	if ( m_pMessagePane ) {
		MSG_FolderInfo* folder;
		MessageKey		key;
		MSG_ViewIndex	viewIndex;

		MSG_GetCurMessage( m_pMessagePane,
						   &folder,
						   &key,
						   &viewIndex);

		if ( key != MSG_MESSAGEKEYNONE ) {
			if ((int) viewIndex >= 0) {
				MSG_NavigateStatus(m_pMessagePane, msgCommand, viewIndex, &enable, NULL);
			}
		}
	}

	pCmdUI->Enable(enable);
}

void CMessageFrame::DoNavigate( MSG_MotionType msgCommand )
{
	if ( m_pMessagePane ) {
		MSG_FolderInfo* folder, *newFolder = NULL;
		MessageKey		key;
		MSG_ViewIndex	viewIndex;

		MSG_GetCurMessage( m_pMessagePane,
						   &folder,
						   &key,
						   &viewIndex);

		if ( key != MSG_MESSAGEKEYNONE ) {
			MessageKey		resultId = MSG_MESSAGEKEYNONE;
			MSG_ViewIndex	resultIndex = viewIndex;
			MSG_ViewIndex	threadIndex;

			MSG_ViewNavigate( m_pMessagePane, msgCommand,
							  viewIndex, 
							  &resultId, &resultIndex, &threadIndex, &newFolder);
				
			if ( resultId != MSG_MESSAGEKEYNONE ) {
				if ( ( resultId != key ) ) {
					if (newFolder && folder != newFolder)
						LoadMessage(newFolder, resultId);
					else
						LoadMessage(folder, resultId);
				}
			} else if ( newFolder != NULL) {
				m_bNavPending = TRUE;
				m_navPending = msgCommand;
				MSG_LoadFolder(m_pPane, newFolder);
			}
		}
		m_pInfoBar->Update();
	}
}

BEGIN_MESSAGE_MAP(CMessageFrame, CMailNewsFrame)
	ON_WM_CLOSE()
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_MESSAGE(TB_FILLINTOOLTIP, OnFillInToolTip)

	// Edit Menu Items
    ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, OnEditRedo)

	// Message Menu Items

	ON_COMMAND(ID_MESSAGE_STOP, OnStop)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_STOP, OnUpdateStop )
	ON_COMMAND(ID_MESSAGE_REUSE, OnReuse)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_REUSE, OnUpdateReuse)

	ON_COMMAND(ID_MESSAGE_CONTINUE, OnContinue)
	ON_COMMAND(ID_NAVIGATE_CONTAINER, OnContainer)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_CONTAINER, OnUpdateContainer )

#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE(FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnMove )
	ON_COMMAND_RANGE(FIRST_COPY_MENU_ID, LAST_COPY_MENU_ID, OnCopy )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnUpdateFile )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_COPY_MENU_ID, LAST_COPY_MENU_ID, OnUpdateFile )
#endif

	ON_UPDATE_COMMAND_UI( ID_MESSAGE_FILE, OnUpdateFile )
END_MESSAGE_MAP()

#ifndef ON_COMMAND_RANGE

BOOL CMessageFrame::OnCommand( WPARAM wParam, LPARAM lParam )
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

BOOL CMessageFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{ 
	if (nCode == CN_UPDATE_COMMAND_UI) {
		CCmdUI* pCmdUI = (CCmdUI*)pExtra;
		if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
			OnUpdateFile( pCmdUI );
			return TRUE;
		}
		if ( nID >= FIRST_COPY_MENU_ID && nID <= LAST_COPY_MENU_ID ) {
			OnUpdateFile( pCmdUI );
			return TRUE;
		}
	}
	return CMailNewsFrame::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

#endif

/////////////////////////////////////////////////////////////////////////////
// CMessageFrame message handlers

static const UINT BASED_CODE indicators[] =
{
	IDS_SECURITY_STATUS,
	IDS_SIGNED_STATUS,
    IDS_TRANSFER_STATUS,    
    ID_SEPARATOR,
	IDS_TASKBAR
};

int CMessageFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	int res = CMailNewsFrame::OnCreate(lpCreateStruct);

	// Set up toolbar
	m_pChrome->SetWindowTitle(XP_AppName);

	//I'm hardcoding string since I don't want it translated.
	m_pChrome->CreateCustomizableToolbar("Messages"/*ID_MESSAGES*/, 3, TRUE);
	CButtonToolbarWindow *pWindow;
	BOOL bOpen, bShowing;

	int32 nPos;

	//I'm hardcoding because I don't want this translated
	m_pChrome->LoadToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"), nPos, bOpen, bShowing);

	LPNSTOOLBAR pIToolBar;
	m_pChrome->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
	if ( pIToolBar ) {
		pIToolBar->Create( this, WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE|CBRS_TOP );
		pIToolBar->SetToolbarStyle( theApp.m_pToolbarStyle);
		SwitchUI();
		pWindow = new CButtonToolbarWindow(CWnd::FromHandlePermanent(pIToolBar->GetHWnd()), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
		m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_NAVIGATION_TOOLBAR, pWindow,nPos, 50, 37, 0, CString(szLoadString(ID_NAVIGATION_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
		m_pChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, bShowing);

		pIToolBar->Release();
	}

	m_pInfoBar = new CMessageInfoBar;
	m_pInfoBar->Create( this, m_pPane );

	//I'm hardcoding because I don't want this translated
	m_pChrome->LoadToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"), nPos, bOpen, bShowing);

	CToolbarWindow *pToolWindow = new CToolbarWindow(m_pInfoBar, theApp.m_pToolbarStyle, 27, 27, eSMALL_HTAB);
	m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_LOCATION_TOOLBAR, pToolWindow,nPos, 27, 27, 0, CString(szLoadString(ID_LOCATION_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
	m_pChrome->ShowToolbar(ID_LOCATION_TOOLBAR, bShowing);

	m_barStatus.Create(this, TRUE, TRUE);
	m_barStatus.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));
	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->Attach(&m_barStatus);
		pIStatusBar->Release();
	}

	return res;
}

void CMessageFrame::OnClose() 
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

	PREF_SetRectPref("mailnews.message_window_rect", left, top, width, height);
	PREF_SetIntPref("mailnews.message_window_showwindow", prefInt);

	//I'm hardcoding because I don't want this translated
	m_pChrome->SaveToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"));
	m_pChrome->SaveToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"));

	CMailNewsFrame::OnClose();
}

void CMessageFrame::OnDestroy()
{
	if (m_pAttachmentData)
		MSG_FreeAttachmentList(m_pMessagePane, m_pAttachmentData);
	m_pAttachmentData = NULL;
	m_pMessagePane = NULL;

	CMailNewsFrame::OnDestroy();

	CView *pView = (CView *) GetDescendantWindow(IDW_MESSAGE_PANE, TRUE);

	ASSERT(pView && pView->IsKindOf(RUNTIME_CLASS(CNetscapeView)));

	if(pView)
		((CNetscapeView *)pView)->FrameClosing();
}

LRESULT CMessageFrame::OnFillInToolTip(WPARAM wParam, LPARAM lParam)
{
	HWND hwnd = (HWND)wParam;
	LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam;

	CToolbarButton *pButton = (CToolbarButton *) CWnd::FromHandle(hwnd);
	UINT nCommand = pButton->GetButtonCommand();
	LPCSTR pTipText = pButton->GetToolTipText();
	LPCSTR pText = NULL;
	CString cs;

	if( nCommand == ID_NAVIGATE_CONTAINER ) 
	{
		MSG_FolderLine folderLine;
		MSG_FolderInfo *folderInfo = GetCurFolder();

		// XXX WHS Int'lize
		if ( MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine ) ) {
			cs.Format("Open %s", folderLine.prettyName ? folderLine.prettyName : folderLine.name );
			pText = (LPCSTR) cs;
		}
	}

	lpttt->szText[79] = '\0';
	if( !pText || !pText[0] )
		strncpy(lpttt->szText, pTipText, 79);
	else
		strncpy(lpttt->szText, pText, 79);

	return 1;
}

// Edit Menu Items

void CMessageFrame::OnEditUndo()
{
    DoCommand(MSG_Undo);
	MessageKey key = MSG_MESSAGEKEYNONE;
	MSG_FolderInfo *folder = NULL;

	if ( UndoComplete == MSG_GetUndoStatus(m_pPane) ) {
		if (MSG_GetUndoMessageKey(m_pPane, &folder, &key) && folder) 
			LoadMessage(folder, key);
	}
}

void CMessageFrame::OnEditRedo()
{
    DoCommand(MSG_Redo);
	MessageKey key = MSG_MESSAGEKEYNONE;
	MSG_FolderInfo *folder = NULL;

	if ( UndoComplete == MSG_GetUndoStatus(m_pPane) ) {
		if (MSG_GetUndoMessageKey(m_pPane, &folder, &key) && folder)
			LoadMessage(folder, key);
	}
}

// Message Menu Items

void CMessageFrame::OnContinue()
{
    CWinCX * pCX = GetMainWinContext();

    if(pCX->GetOriginY() + pCX->GetHeight() < pCX->GetDocumentHeight()) {
        pCX->Scroll(SB_VERT, SB_PAGEDOWN, 0, NULL);
	} else if (!pCX->IsLayingOut()) {
		DoNavigate( MSG_NextUnreadMessage );
	}
}

void CMessageFrame::OnContainer()
{
	MSG_FolderInfo* folderInfo;
	MessageKey		key;
	MSG_ViewIndex	viewIndex;

	MSG_GetCurMessage( m_pPane,
					   &folderInfo,
					   &key,
					   &viewIndex);

	C3PaneMailFrame::Open( folderInfo, key );
}

void CMessageFrame::OnUpdateContainer( CCmdUI * pCmdUI )
{
	MSG_FolderInfo* folderInfo;
	MessageKey		key;
	MSG_ViewIndex	viewIndex;

	MSG_GetCurMessage( m_pPane,
					   &folderInfo,
					   &key,
					   &viewIndex);

	pCmdUI->Enable( folderInfo ? TRUE : FALSE );
}

void CMessageFrame::OnStop() 
{
	//      Let the context have it.
	if(GetMainContext()) {
		GetMainContext()->AllInterrupt();
	}
}

void CMessageFrame::OnUpdateStop(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetMainContext()) {
		pCmdUI->Enable(GetMainContext()->CanAllInterrupt());
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

void CMessageFrame::OnMove(UINT nID)
{
	if ( m_pPane ) {
		MSG_FolderInfo *folderInfoCur;
		MSG_ViewIndex idxCur;
		MessageKey idCur;
		MSG_GetCurMessage( m_pPane, &folderInfoCur, &idCur, &idxCur );

		MSG_FolderInfo *folderInfo = FolderInfoFromMenuID( nID );

		ASSERT(folderInfo);
		if (folderInfo) {
			MSG_FolderLine folderLine;
			MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );

			// We want to make file behave for newsgroups
			if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ) {
				MSG_CopyMessagesIntoFolder( m_pPane, &idxCur, 1, folderInfo);
			} else {
				MSG_MoveMessagesIntoFolder( m_pPane, &idxCur, 1, folderInfo);
			}
			ModalStatusBegin( MODAL_DELAY );
		}
	}
}

void CMessageFrame::OnCopy(UINT nID)
{		    
	if ( m_pPane ) {
		MSG_FolderInfo *folderInfoCur;
		MSG_ViewIndex idxCur;
		MessageKey idCur;
		MSG_GetCurMessage( m_pPane, &folderInfoCur, &idCur, &idxCur );

		MSG_FolderInfo *folderInfo = FolderInfoFromMenuID( nID );

		ASSERT(folderInfo);
		if (folderInfo) {
			MSG_CopyMessagesIntoFolder( m_pPane, &idxCur, 1, folderInfo);
			ModalStatusBegin( MODAL_DELAY );
		}
	}
}

void CMessageFrame::OnUpdateFile( CCmdUI *pCmdUI )
{
	MessageKey key = GetCurMessage();
	BOOL bEnable = key != MSG_MESSAGEKEYNONE;
	if (pCmdUI->m_pSubMenu) {
	    pCmdUI->m_pMenu->EnableMenuItem(pCmdUI->m_nIndex,
										MF_BYPOSITION |(bEnable ? MF_ENABLED : MF_GRAYED));
	} else {
		pCmdUI->Enable( bEnable );
	}
}

// IMailFrame override
void CMessageFrame::PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if ( notify == MSG_PaneNotifyLastMessageDeleted ||
		 notify == MSG_PaneNotifyMessageDeleted ) {
		PostMessage( WM_CLOSE, (WPARAM) 0, (LPARAM) 0 );
	} else if (notify == MSG_PaneNotifyFolderLoaded) {
		MSG_FolderInfo *folderInfo = GetCurFolder();
		MessageKey key = MSG_MESSAGEKEYNONE;

		if (m_bNavPending) {
			switch ( m_navPending ) {
			case MSG_Forward:
			case MSG_Back:
				ASSERT(0);
				break;

			case MSG_NextFolder:
			case MSG_NextMessage:
				DoNavigate(MSG_FirstMessage);
				break;

			case MSG_NextUnreadMessage:
			case MSG_NextUnreadThread:
			case MSG_NextUnreadGroup:
			case MSG_LaterMessage:
				m_navPending = MSG_NextUnreadMessage;

			default:
				DoNavigate( m_navPending );
				break;
			}
		}
		m_bNavPending = FALSE;
	} else if (notify == MSG_PaneNotifyMessageLoaded) {
		if (MSG_GetBacktrackState(pane) == MSG_BacktrackIdle)
			MSG_AddBacktrackMessage(pane, GetCurFolder(), (MessageKey) value);
		else
			MSG_SetBacktrackState(pane, MSG_BacktrackIdle);
	}
	m_pInfoBar->Update();
}


void CMessageFrame::SwitchUI( )
{
	LPNSTOOLBAR pIToolBar;
	m_pChrome->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
	if ( pIToolBar ) {
		CWnd *pToolWnd = CWnd::FromHandlePermanent( pIToolBar->GetHWnd() );

		if ( pToolWnd ) {
			pIToolBar->RemoveAllButtons();

			UINT *aidButtons = m_bNews ? NewsCodes : MailCodes;
			int *aidxButtons = m_bNews ? NewsIndices : MailIndices;

			int nButtons = (m_bNews ? sizeof(NewsCodes) : sizeof(MailCodes)) /sizeof(UINT);
			for(int i = 0; i < nButtons; i++)
			{
				if ( aidButtons[i] == ID_MESSAGE_FILE ) {
					CMailQFButton *pFileButton = new CMailQFButton;
					pFileButton->Create( CRect(0,0,0,0), pToolWnd, ID_MESSAGE_FILE);
					pIToolBar->AddButton(pFileButton, i);
				} else {
					CString statusStr, toolTipStr, textStr;
					CCommandToolbarButton *pCommandButton = new CCommandToolbarButton;
				 
					WFE_ParseButtonString( aidButtons[i], statusStr, toolTipStr, textStr );
					DWORD dwButtonStyle = 0;

					switch (aidButtons[i]) {
					case ID_MESSAGE_REPLYBUTTON:
					case ID_MESSAGE_MARKBUTTON:
						dwButtonStyle |= TB_HAS_IMMEDIATE_MENU;
						break;

					case ID_MESSAGE_NEXTUNREAD:
						dwButtonStyle |= TB_HAS_TIMED_MENU;
						break;

					case ID_NAVIGATE_MSG_BACK:
						break;
					default:
						break;
					}


					pCommandButton->Create( pToolWnd, theApp.m_pToolbarStyle, 
											CSize(44, 37), CSize(25, 25),
											textStr, toolTipStr, statusStr,
											m_bNews ? IDR_NEWSTHREAD : IDR_MAILTHREAD,
											aidxButtons[i], CSize(23,21), 
											aidButtons[i], -1, dwButtonStyle);

					pIToolBar->AddButton(pCommandButton, i);
				}
			}
		}
		pIToolBar->Release();
	}

	CString csFullString, cs;
	csFullString.LoadString( m_bNews ? IDR_NEWSMSGFRAME : IDR_MESSAGEFRAME );
	AfxExtractSubString( cs, csFullString, 0 );
	m_pChrome->SetWindowTitle(cs);

#ifdef _WIN32
	if (sysInfo.m_bWin4) {
		UINT nIDIcon = m_bNews ? IDR_NEWSTHREAD : IDR_MAILTHREAD;
		HINSTANCE hInst = AfxFindResourceHandle(MAKEINTRESOURCE(nIDIcon), RT_GROUP_ICON);
		HICON hLargeIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(nIDIcon));
		SetIcon(hLargeIcon, TRUE);

		HICON hSmallIcon = (HICON) ::LoadImage(hInst, 
											   MAKEINTRESOURCE(nIDIcon),
											   IMAGE_ICON, 
											   GetSystemMetrics(SM_CXSMICON),
											   GetSystemMetrics(SM_CYSMICON),
											   LR_SHARED);
		SetIcon(hSmallIcon, FALSE);
	}
#endif
}

void CMessageFrame::SetIsNews( BOOL bNews )
{
	if ( bNews != m_bNews ) {
		m_bNews = bNews;
		SwitchUI();
	}
}

void CMessageFrame::LoadMessage( MSG_FolderInfo *folderInfo, MessageKey id )
{
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo( GetMainContext()->GetContext() );
	int16 cur_csid = INTL_GetCSIDocCSID( c );
	int16 doc_csid = MSG_GetFolderCSID( folderInfo );

	if (!doc_csid)
		doc_csid = INTL_DefaultWinCharSetID(0);

	if ( cur_csid != doc_csid )
		RefreshNewEncoding( doc_csid, FALSE );

	MSG_LoadMessage( m_pPane, folderInfo, id );

	if (id == MSG_MESSAGEKEYNONE)
		m_pMessageView->SetAttachments(NULL, 0);
	else
		MSG_AddBacktrackMessage( m_pPane, folderInfo, id );

	MSG_FolderLine folderLine;
	MSG_MessageLine messageLine;
	MSG_GetFolderLineById( m_pMaster, folderInfo, &folderLine );
	MSG_GetThreadLineById( m_pPane, id, &messageLine );

	SetIsNews( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ? TRUE : FALSE );
	m_bCategory = (folderLine.flags & MSG_FOLDER_FLAG_CATEGORY ? TRUE : FALSE);

	m_pInfoBar->Update();
}

void CMessageFrame::OnReuse()
{
}

void CMessageFrame::OnUpdateReuse(CCmdUI* pCmdUI)
{
}

MessageKey CMessageFrame::GetCurMessage() const
{
	MessageKey key = MSG_MESSAGEKEYNONE;
	if (m_pPane) {
		MSG_ViewIndex index = (MSG_ViewIndex) -1;
		MSG_FolderInfo *folderInfo = NULL;

		MSG_GetCurMessage( m_pPane, &folderInfo, &key, &index );
	}
	return key;
}

MSG_FolderInfo *CMessageFrame::GetCurFolder() const
{
	MessageKey key = MSG_MESSAGEKEYNONE;
	MSG_ViewIndex index = (MSG_ViewIndex) -1;
	MSG_FolderInfo *folderInfo = NULL;

	MSG_GetCurMessage( m_pPane, &folderInfo, &key, &index );

	return folderInfo;
}

LPCTSTR CMessageFrame::GetWindowMenuTitle()
{
	static CString cs;
	CString csTitle = szLoadString( IDS_TITLE_NOSUBJECT );
	cs = m_bNews ? szLoadString( IDS_TITLE_ARTICLE ) : szLoadString( IDS_TITLE_MESSAGE );
	
	MWContext *pXPCX = GetMainContext()->GetContext();
	if ( pXPCX && pXPCX->title) {
		csTitle = fe_MiddleCutString(pXPCX->title, 40);
	}
	cs += csTitle;

	return cs;
}

CMessageFrame *CMessageFrame::FindFrame( MSG_FolderInfo *folderInfo, MessageKey key )
{
	CGenericFrame *pFrame = theApp.m_pFrameList;

	while ( pFrame  ) {
		if ( pFrame->IsKindOf( RUNTIME_CLASS( CMessageFrame ) ) ) {
			MSG_Pane *pPane = ((CMessageFrame *) pFrame)->GetPane();
			ASSERT( MSG_GetPaneType( pPane ) == MSG_MESSAGEPANE );

			MSG_FolderInfo *folderInfo2;
			MessageKey key2;
			MSG_ViewIndex index;
			MSG_GetCurMessage( pPane, &folderInfo2, &key2, &index );

			if ( (folderInfo == folderInfo2) && ( key == key2 ) ) {
				return (CMessageFrame *) pFrame;
			}
		}
		pFrame = pFrame->m_pNext;
	}
	return NULL;
}

CMessageFrame *CMessageFrame::Open()
{
	if (!CheckWizard())
		return NULL;

	CMessageFrame *pFrame = NULL;
	CGenericDoc *pDoc;
	pDoc = (CGenericDoc *) g_MsgPrefs.m_pMsgTemplate->OpenDocumentFile(NULL);

	if ( pDoc )
	{
		CWinCX * pContext = ((CWinCX*)pDoc->GetContext());

		if ( pContext ) {
			pFrame = (CMessageFrame *) pContext->GetFrame()->GetFrameWnd();
		}
	}

	return pFrame;
}

CMessageFrame *CMessageFrame::Open( MSG_FolderInfo *folderInfo, MessageKey key )
{
	CMessageFrame *pFrame = FindFrame( folderInfo, key );

	if (!pFrame) {
		pFrame = CMessageFrame::Open();
		if ( pFrame ) {
			pFrame->LoadMessage( folderInfo, key );
		}
	}
	return pFrame;
}
