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
#include "msgcom.h"
#include "intl_csi.h"
#include "feutil.h"
#include "netsvw.h"
#include "fldrfrm.h"
#include "thrdfrm.h"
#include "srchfrm.h"
#include "msgfrm.h"
#include "wfemsg.h"
#include "msgview.h"
#include "filter.h"
#include "mailmisc.h"
#include "template.h"
#include "custom.h"
#include "subnews.h"
#include "mailpriv.h"
#include "mailqf.h"
#include "dspppage.h"
#include "addrfrm.h" //for MOZ_NEWADDR

#ifdef DEBUG_WHITEBOX
#include "qa.h"
#endif 


#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(C3PaneMailFrame, CMsgListFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

static C3PaneMailFrame *g_pLast3PaneMailFrame = NULL;

///////////////////////////////////////////////////////////////////////////
// C3PaneMailFrame

static BOOL s_bHintNews = FALSE;
static BOOL s_bGetMail = FALSE;

UINT MailCodes[10] = {
	ID_FILE_GETNEWMAIL,
	ID_FILE_NEWMESSAGE,
	ID_MESSAGE_REPLYBUTTON,
	ID_MESSAGE_FORWARDBUTTON,
	ID_MESSAGE_FILE,
    ID_MESSAGE_NEXTUNREAD,
	ID_FILE_PRINT,
	ID_SECURITY,
	ID_EDIT_DELETE_3PANE,
	ID_NAVIGATE_INTERRUPT
};

int MailIndices[10] = { 0, 1, 2, 3, 4, 5, 6, 10, 11, 13 };

UINT NewsCodes[11] = {
	ID_FILE_GETNEWMAIL,
    ID_NEWS_POSTNEW,
	ID_MESSAGE_REPLYBUTTON,
	ID_MESSAGE_FORWARDBUTTON,
	ID_MESSAGE_FILE,
    ID_MESSAGE_NEXTUNREAD,
    ID_FILE_PRINT,
	ID_SECURITY,
	ID_MESSAGE_MARKBUTTON,
	ID_EDIT_DELETE_3PANE,
	ID_NAVIGATE_INTERRUPT
};

int NewsIndices[11] = { 0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 14 };

//status bar format
static const UINT BASED_CODE indicators[] =
{
	IDS_EXPANDO,
	IDS_SECURITY_STATUS,
	IDS_SIGNED_STATUS,
    IDS_TRANSFER_STATUS,    
    ID_SEPARATOR,
	IDS_ONLINE_STATUS,
	IDS_TASKBAR
};

C3PaneMailFrame::C3PaneMailFrame()
{
	m_bNews = FALSE;
	m_bWantToGetMail = FALSE;
	m_pMessagePane = NULL;
	m_pOutlinerParent = NULL;
	m_pFolderPane = NULL;
	m_pFolderOutliner = NULL;
	m_pFolderOutlinerParent = NULL;
	m_pFolderSplitter = NULL;
	m_pThreadSplitter = NULL;
	m_nLoadingFolder = 0;
	m_bDragCopying = FALSE;
	m_bBlockingFolderSelection = FALSE;

	m_bNoScrollHack = FALSE;

	m_actionOnLoad = actionSelectFirst;
	m_navPending = MSG_FirstMessage;
	m_selPending = -1L;

	m_pFocusWnd = NULL;

	// All our favorite hotkeys
	LoadAccelerators( IDR_ONEKEYMESSAGE );
}

C3PaneMailFrame::~C3PaneMailFrame()
{
	delete m_pOutlinerParent;
	delete m_pFolderOutlinerParent;
}

void C3PaneMailFrame::UIForFolder( MSG_FolderInfo *folderInfo )
{
	MSG_FolderLine folderLine;
	if (MSG_GetFolderLineById(WFE_MSGGetMaster(), folderInfo, &folderLine)) {

		SetIsNews( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ? TRUE : FALSE );

		MSG_FolderInfo *parentFolder = folderInfo;
									
		int16 doc_csid = MSG_GetFolderCSID( parentFolder );
		if (!doc_csid)
			doc_csid = INTL_DefaultWinCharSetID(0);

		RefreshNewEncoding( doc_csid, FALSE );

		CString csFullString, csTitle, cs;
		if ( m_bNews ) {
			csFullString.LoadString( IDR_NEWSTHREAD );
		} else {
			csFullString.LoadString( IDR_MAILTHREAD );
		}

		AfxExtractSubString( csTitle, csFullString, 0 );

		cs = folderLine.prettyName && folderLine.prettyName[0] ? 
			 folderLine.prettyName :
		     folderLine.name;

		cs += _T(" - ");
		cs += csTitle;
		m_pChrome->SetWindowTitle(cs);

		m_pInfoBar->Update();

		// Refresh the headers, which may change
		if ( m_pOutliner )
			m_pOutliner->GetParent()->Invalidate();

		UpdateFolderPane(folderInfo);
	}
}

void C3PaneMailFrame::PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							    MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if ( pane ==m_pFolderPane && notify == MSG_PaneNotifySelectNewFolder ) {
		if ( value >= 0 ) {
			m_pFolderOutliner->SelectItem( CASTINT(value) );
		}
		return;
	}
	if (notify == MSG_PaneNotifySafeToSelectFolder)
	{
		if (m_bBlockingFolderSelection)
		{
			m_bBlockingFolderSelection = FALSE;
			PostMessage(WM_COMMAND, ID_FOLDER_SELECT, 0);
		}
		return;
	}

	if ( notify == MSG_PaneNotifyFolderLoaded ) {

		if (m_nLoadingFolder > 0) {
			// This load was initiated through our LoadFolder method
			m_pOutliner->BlockSelNotify(FALSE);
			m_nLoadingFolder--;
		}
		else
			return; // must be blocked by select top level host

		// Update the UI
		MSG_FolderInfo *folderInfo = MSG_GetCurFolder( m_pPane );

		UIForFolder( folderInfo );

		// Safety Dance
		m_pOutliner->SetTotalLines( (int) MSG_GetNumLines( m_pPane ) );
		m_pOutliner->Invalidate();

		// Select something based on previously set hints.

		switch ( m_actionOnLoad ) {
		case actionSelectFirst: 
			{
				if (m_navPending != MSG_NextUnreadMessage)
				{
					XP_Bool bSelectLastMsg = FALSE;
 					PREF_GetBoolPref("mailnews.remember_selected_message", &bSelectLastMsg);
					MessageKey lastKey = MSG_GetLastMessageLoaded(folderInfo);
					MSG_ViewIndex index = 
						MSG_GetMessageIndexForKey(m_pPane, lastKey, TRUE);
					if (index != MSG_VIEWINDEXNONE && bSelectLastMsg) 
						SelectMessage(lastKey);
					else
						m_pOutliner->SelectItem( m_pOutliner->GetTotalLines() > 0 ? 0 : -1 );
					m_bNoScrollHack = TRUE;
					OnDoneGettingMail();
				}
				else
					m_navPending = MSG_FirstMessage;
			}
			break;

		case actionSelectKey:
			SelectMessage( m_selPending );
			break;

		case actionNavigation:
			switch ( m_navPending ) {
			case MSG_Forward:
			case MSG_Back:
			case MSG_EditUndo:
			case MSG_EditRedo:
				SelectMessage( m_selPending );
				break;

			case MSG_NextFolder:
			case MSG_NextMessage:
				m_pOutliner->SelectItem( -1 );
				DoNavigate(MSG_FirstMessage);
				break;

			case MSG_NextUnreadMessage:
			case MSG_NextUnreadThread:
			case MSG_NextUnreadGroup:
			case MSG_LaterMessage:
				m_navPending = MSG_NextUnreadMessage;

			default:
				m_pOutliner->SelectItem( -1 );
				DoNavigate( m_navPending );
				break;
			}
			break;

		case actionNone:
			break;

		}
		// Sort might have changed, redraw column headers
		m_pOutliner->GetParent()->Invalidate();

		m_actionOnLoad = actionSelectFirst;

		if (m_bWantToGetMail && !NET_IsOffline() &&
			!MSG_GetMailServerIsIMAP4(g_MsgPrefs.m_pMsgPrefs)) {
			PostMessage(WM_COMMAND, (WPARAM) ID_FILE_GETNEWMAIL, (LPARAM) 0);
		}
		m_bWantToGetMail = FALSE;
	} else if ( notify == MSG_PaneNotifyFolderDeleted ) {
		if (!m_pPane)
			CreateThreadPane();
	} else if ( notify == MSG_PaneNotifyMessageLoaded ) {
		MSG_FolderInfo *curFolder = GetCurFolder();
		MSG_FolderInfo *folderInfo = MSG_GetCurFolder(m_pMessagePane);
		MessageKey key = (MessageKey) value;

		if (folderInfo != curFolder) {
			C3PaneMailFrame::Open(folderInfo, key);
		} else {
			MSG_ViewIndex index = MSG_GetMessageIndexForKey(m_pPane, key, TRUE);

			if (index != MSG_VIEWINDEXNONE) {
				m_pOutliner->BlockSelNotify(TRUE); // Tail don't wag dog
				m_pOutliner->SelectItem(CASTINT(index));
				m_pOutliner->BlockSelNotify(FALSE);
			}
		}
		if (MSG_GetBacktrackState(m_pPane) == MSG_BacktrackIdle)
			MSG_AddBacktrackMessage(m_pPane, folderInfo, key);
		else
			MSG_SetBacktrackState(m_pPane, MSG_BacktrackIdle);
	} else if ( notify == MSG_PaneNotifyCopyFinished && m_bDragCopying) {
		// Allow the UI to update and be interacted with
		m_pOutliner->BlockSelNotify(FALSE);
		m_bDragCopying = FALSE;
		// Post message to allow BE to return first.
		PostMessage(WM_COMMAND, (WPARAM) ID_MESSAGE_SELECT, 0);
	}
	// We get notified of message deletes individually, so lets
	// ignore it.
	if ( notify != MSG_PaneNotifyMessageDeleted && m_pPane) {
		m_pInfoBar->Update();
	}
#ifdef DEBUG_WHITEBOX
	if ( notify == MSG_PaneNotifyMessageLoaded ) // previous MSG_PaneNotifyMessageLoaded 
	{
		bWaitForInbox = FALSE;
	}
	if ( notify == MSG_PaneNotifyMessageDeleted) 
	{
		QADoDeleteMessageEventHandler2();
	}

#endif
}

void C3PaneMailFrame::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == m_pFolderPane ) {
		if ( m_pFolderOutliner ) {
			m_pFolderOutliner->MysticStuffStarting( asynchronous, notify,
												   where, num );
		}
	} else {
		CMsgListFrame::ListChangeStarting( pane, asynchronous, notify,
										   where, num );
	}
}

void C3PaneMailFrame::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == m_pFolderPane ) {
		if ( m_pFolderOutliner ) {
			m_pFolderOutliner->MysticStuffFinishing( asynchronous, notify,
												   where, num );
		}
	} else {
		CMsgListFrame::ListChangeFinished( pane, asynchronous, notify,
										   where, num );
	}
}

void C3PaneMailFrame::CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
									 MSG_FolderInfo *folderInfo)
{
	ASSERT(pane);
	m_bDragCopying = TRUE;
	m_pOutliner->BlockSelNotify(TRUE);
	MSG_CopyMessagesIntoFolder(pane, indices, count, folderInfo);
}

void C3PaneMailFrame::MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
									 MSG_FolderInfo *folderInfo)
{
	ASSERT(pane);
	m_bDragCopying = TRUE;
	m_pOutliner->BlockSelNotify(TRUE);
	MSG_FolderLine folderLine;
	MSG_GetFolderLineById( WFE_MSGGetMaster(), GetCurFolder(), &folderLine );

	// We want to make file behave for newsgroups
	if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ) {
		MSG_CopyMessagesIntoFolder(pane, indices, count, folderInfo);
	} else {
		MSG_MoveMessagesIntoFolder(pane, indices, count, folderInfo);
	}
}

void C3PaneMailFrame::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							      int *focus)
{
	if ( pane == m_pFolderPane ) {
		if ( m_pFolderOutliner ) {
			m_pFolderOutliner->GetSelection(*indices, *count);
			*focus = m_pFolderOutliner->GetFocusLine();
		}
	} else {
		CMsgListFrame::GetSelection( pane, indices, count, focus );
	}
}

void C3PaneMailFrame::SelectItem( MSG_Pane* pane, int item )
{
	if ( pane == m_pFolderPane ) {
		if ( m_pFolderOutliner ) {
			m_pFolderOutliner->SelectItem(item);
			m_pFolderOutliner->ScrollIntoView(item);
		}
	} else {
		CMsgListFrame::SelectItem( pane, item );
	}
}

//clock wise
void C3PaneMailFrame::CheckFocusWindow(BOOL bUseTab)
{
	CWnd* pNextFocusWnd = NULL;
	CWnd* pMsgBodyView = m_pMessageView ? m_pMessageView->GetMessageBodyView() : NULL;
	BOOL  bFolderClosed = m_pFolderSplitter->IsOnePaneClosed();
	BOOL  bMessageClosed = m_pThreadSplitter->IsOnePaneClosed();

	if (m_pFocusWnd == pMsgBodyView ) 
	{
		if (bUseTab || bMessageClosed)
			pNextFocusWnd = bFolderClosed ? m_pOutliner : m_pFolderOutliner;
		else
			pNextFocusWnd = pMsgBodyView;
	}
	else if (m_pFocusWnd == m_pOutliner)
	{   
		if (bUseTab)
		{
			if (!bMessageClosed)
				pNextFocusWnd = pMsgBodyView;
			else if (!bFolderClosed)
				pNextFocusWnd = m_pFolderOutliner;
			else
				pNextFocusWnd = m_pOutliner;
		}
		else
			pNextFocusWnd = m_pOutliner;
	}
	else 
		pNextFocusWnd = m_pOutliner;

	if (pNextFocusWnd != m_pFocusWnd)
	{
		m_pFocusWnd = pNextFocusWnd;
		m_pFocusWnd->SetFocus();
	}
}

//counter clock wise
void C3PaneMailFrame::CheckShiftKeyFocusWindow()
{
	CWnd* pNextFocusWnd = NULL;
	CWnd* pMsgBodyView = m_pMessageView ? m_pMessageView->GetMessageBodyView() : NULL;
	BOOL  bFolderClosed = m_pFolderSplitter->IsOnePaneClosed();
	BOOL  bMessageClosed = m_pThreadSplitter->IsOnePaneClosed();

	if (m_pFocusWnd == m_pFolderOutliner) 
		pNextFocusWnd = bMessageClosed ? m_pOutliner : pMsgBodyView;
	else if (m_pFocusWnd == m_pOutliner)
	{
		if (!bFolderClosed)
			pNextFocusWnd = m_pFolderOutliner;
		else if (!bMessageClosed)
			pNextFocusWnd = pMsgBodyView;
		else
			pNextFocusWnd = m_pOutliner;
	}
	else 
		pNextFocusWnd = m_pOutliner;

	if (pNextFocusWnd != m_pFocusWnd)
	{
		m_pFocusWnd = pNextFocusWnd;
		m_pFocusWnd->SetFocus();
	}
}

BOOL C3PaneMailFrame::PreTranslateMessage(MSG* pMsg)
{
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_DELETE)
	{
		CWnd* pFocusWnd = GetFocus();
		if (pFocusWnd == m_pFolderOutliner)
		{
			PrepareForDeleteFolder();
			OnDeleteFolder();
		}
		else 
		{
			if (GetKeyState(VK_SHIFT) & 0x8000)
				OnReallyDeleteMessage();
			else
				OnDeleteMessage();
		}
		return TRUE;
	}
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB && 
		 !(GetKeyState(VK_MENU) & 0x8000) && 
		 !(GetKeyState(VK_CONTROL) & 0x8000)) 
	{ 
		if (GetKeyState(VK_SHIFT) & 0x8000) 
			CheckShiftKeyFocusWindow();
		else 
			CheckFocusWindow();

		return TRUE;
	}

	return CMsgListFrame::PreTranslateMessage( pMsg );
}

BOOL C3PaneMailFrame::OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext )
{
	BOOL res = CMsgListFrame::OnCreateClient(lpcs, pContext);
	if (res) {
		m_pMaster = WFE_MSGGetMaster();
		m_bNews = s_bHintNews;

        m_pFolderSplitter = DYNAMIC_DOWNCAST(CMailNewsSplitter, GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE));
        ASSERT(m_pFolderSplitter);

#ifdef _WIN32
		m_pFolderSplitter->ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
#endif

		m_pFolderSplitter->SetNotifyFrame(this); //for setting focus in pane
		
		m_pThreadSplitter = (CMailNewsSplitter *) RUNTIME_CLASS(CMailNewsSplitter)->CreateObject();
        ASSERT(m_pThreadSplitter); 

#ifdef _WIN32
		m_pThreadSplitter->CreateEx(0, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								  0,0,0,0, m_pFolderSplitter->m_hWnd, (HMENU)99, NULL );
#else
		m_pThreadSplitter->Create( NULL, NULL,
								 WS_BORDER|WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								 CRect(0,0,0,0), m_pFolderSplitter, 99, pContext ); 
#endif
		m_pThreadSplitter->SetNotifyFrame(this); //for loading message when pane opens
		m_pThreadSplitter->SetLoadMessage(TRUE); //for loading message when pane opens

		m_pMessageView = (CMessageView *) RUNTIME_CLASS(CMessageView)->CreateObject();
#ifdef _WIN32
		if (sysInfo.m_bWin4)
			m_pMessageView->CreateEx( WS_EX_CLIENTEDGE, NULL, NULL, 
									  WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
									  0,0,0,0, m_pThreadSplitter->m_hWnd, (HMENU) IDW_MESSAGE_VIEW, pContext );
		else
#endif
			m_pMessageView->Create( NULL, NULL, 
									WS_BORDER|WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
									CRect(0,0,0,0), m_pThreadSplitter, IDW_MESSAGE_VIEW, pContext );

		m_pMessageView->SendMessage(WM_INITIALUPDATE);

		CWnd *pView = GetDescendantWindow(IDW_MESSAGE_PANE, TRUE);
		ASSERT(pView);
		CWinCX *pWinCX;

		pWinCX = new CWinCX(DYNAMIC_DOWNCAST(CGenericDoc, pContext->m_pCurrentDoc), 
							this, (CGenericView *)pView);

		SetMainContext(pWinCX);
		SetActiveContext(pWinCX);

		RECT rect;
		GetClientRect(&rect);
		pWinCX->Initialize(FALSE, &rect);
		pWinCX->GetContext()->type = MWContextMail;
		pWinCX->GetContext()->fancyFTP = TRUE;
		pWinCX->GetContext()->fancyNews = TRUE;
		pWinCX->GetContext()->intrupt = FALSE;
		pWinCX->GetContext()->reSize = FALSE;
		        
		m_pOutlinerParent = new CMessageOutlinerParent;
#ifdef _WIN32
		m_pOutlinerParent->CreateEx(WS_EX_CLIENTEDGE, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								  0,0,0,0, m_pThreadSplitter->m_hWnd, (HMENU) IDW_THREAD_PANE, NULL );
#else
		m_pOutlinerParent->Create( NULL, NULL,
								 WS_BORDER|WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								 CRect(0,0,0,0), m_pThreadSplitter, IDW_THREAD_PANE );
#endif
		m_pOutlinerParent->EnableFocusFrame(TRUE);
		CreateFolderOutliner();

		m_pOutliner = DOWNCAST(CMailNewsOutliner, m_pOutlinerParent->m_pOutliner);
		DOWNCAST(CMessageOutliner, m_pOutliner)->SetNews(m_bNews);
		m_pOutlinerParent->CreateColumns ( );

		int32 prefInt = -1;
		PREF_GetIntPref("mailnews.3pane_thread_height", &prefInt);
		m_pThreadSplitter->AddPanes(m_pOutlinerParent, m_pMessageView, prefInt, FALSE);

		prefInt = -1;
		PREF_GetIntPref("mailnews.3pane_folder_width", &prefInt);
		m_pFolderSplitter->AddPanes(m_pFolderOutlinerParent, m_pThreadSplitter, prefInt);

		CreateMessagePane();
		CreateThreadPane();

		m_pOutlinerParent->SetFocus();
		m_pFocusWnd = m_pOutliner;

		// Don't call CMsgListFrame, since we create our list
		// differently
	}
	return res;
}

void C3PaneMailFrame::GetMessageString( UINT nID, CString& rMessage ) const
{
	switch (nID) {
	case ID_MESSAGE_KILL:
		rMessage.LoadString(ID_MESSAGE_KILL);
		break;

	default:
		CMsgListFrame::GetMessageString( nID, rMessage );
	}
}

BEGIN_MESSAGE_MAP(C3PaneMailFrame, CMailNewsFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_SETFOCUS()
 
	// File Menu Items
	ON_COMMAND(ID_FILE_EMPTYTRASHFOLDER, OnEmptyTrash)

	// Edit Menu Items
    ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_COMMAND(ID_EDIT_SELECTTHREAD, OnSelectThread)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTTHREAD, OnUpdateSelectThread)
	ON_COMMAND(ID_EDIT_SELECTMARKEDMESSAGES, OnSelectFlagged)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTMARKEDMESSAGES, OnUpdateSelectFlagged)
	ON_COMMAND(ID_EDIT_SELECTALL, OnSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALL, OnUpdateSelectAll)
	ON_COMMAND(ID_EDIT_DELETE_3PANE, OnDeleteFrom3Pane)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE_3PANE, OnUpdateDeleteFrom3Pane)

	// View Menu Items
	ON_COMMAND(ID_VIEW_MESSAGE, OnViewMessage)		 
	ON_UPDATE_COMMAND_UI(ID_VIEW_MESSAGE, OnUpdateViewMessage)
	ON_COMMAND(ID_VIEW_FOLDER, OnViewFolder)	
	ON_UPDATE_COMMAND_UI(ID_VIEW_FOLDER, OnUpdateViewFolder)

	// Message Menu Items
#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE(FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnMove )
	ON_COMMAND_RANGE(FIRST_COPY_MENU_ID, LAST_COPY_MENU_ID, OnCopy )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnUpdateFile )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_COPY_MENU_ID, LAST_COPY_MENU_ID, OnUpdateFile )
#endif
	ON_COMMAND(ID_MESSAGE_KILL, OnIgnore)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_KILL, OnUpdateIgnore)

	// Non Menu Items	
	ON_COMMAND( ID_MESSAGE_CONTINUE, OnContinue )
	ON_COMMAND( ID_MESSAGE_SELECT, OnSelect )
	ON_COMMAND( ID_FOLDER_SELECT, OnSelectFolder )
	ON_COMMAND( ID_FILE_OPENMESSAGE, OnOpen )
	ON_UPDATE_COMMAND_UI( ID_FILE_OPENMESSAGE, OnUpdateOpen)
	ON_COMMAND( ID_FILE_OPENMESSAGENEW, OnOpenNew )
	ON_UPDATE_COMMAND_UI( ID_FILE_OPENMESSAGENEW, OnUpdateOpen)
	ON_COMMAND( ID_FILE_OPENMESSAGEREUSE, OnOpenReuse )
	ON_UPDATE_COMMAND_UI( ID_FILE_OPENMESSAGEREUSE, OnUpdateOpen)
	ON_COMMAND(ID_NAVIGATE_CONTAINER, OnContainer )
	ON_UPDATE_COMMAND_UI( ID_MESSAGE_FILE, OnUpdateFile )
	ON_COMMAND(ID_FILE_OPENFOLDER, OnOpenNewFrame )

	ON_COMMAND(ID_HOTLIST_ADDCURRENTTOHOTLIST, OnFileBookmark)
	ON_UPDATE_COMMAND_UI(ID_HOTLIST_ADDCURRENTTOHOTLIST, OnUpdateFileBookmark)

	ON_COMMAND(ID_PRIORITY_LOWEST, OnPriorityLowest)
	ON_UPDATE_COMMAND_UI(ID_PRIORITY_LOWEST, OnUpdatePriority)
	ON_COMMAND(ID_PRIORITY_LOW, OnPriorityLow)
	ON_UPDATE_COMMAND_UI(ID_PRIORITY_LOW, OnUpdatePriority)
	ON_COMMAND(ID_PRIORITY_NORMAL, OnPriorityNormal)
	ON_UPDATE_COMMAND_UI(ID_PRIORITY_NORMAL, OnUpdatePriority)
	ON_COMMAND(ID_PRIORITY_HIGH, OnPriorityHigh)
	ON_UPDATE_COMMAND_UI(ID_PRIORITY_HIGH, OnUpdatePriority)
	ON_COMMAND(ID_PRIORITY_HIGHEST, OnPriorityHighest)
	ON_UPDATE_COMMAND_UI(ID_PRIORITY_HIGHEST, OnUpdatePriority)

	ON_COMMAND(ID_DONEGETTINGMAIL, OnDoneGettingMail)

	ON_MESSAGE(TB_FILLINTOOLTIP, OnFillInToolTip)
	ON_MESSAGE(TB_FILLINSTATUS, OnFillInToolbarButtonStatus)

END_MESSAGE_MAP()

void C3PaneMailFrame::HandleGetNNNMessageMenuItem()
{
	//News needs to have Get Next N messages and mail doesn't.
	CMenu *pMainMenu = GetMenu();

	if(pMainMenu)
	{
		CMenu *pFileMenu = pMainMenu->GetSubMenu(0);

		if(pFileMenu)
		{
			if(!m_bNews)
			{
				pFileMenu->RemoveMenu(ID_FILE_GETNEXT, MF_BYCOMMAND);
			}
			else
			{
				int nGetNNNMessagePosition = WFE_FindMenuItem(pFileMenu, ID_FILE_GETNEXT);
				if(nGetNNNMessagePosition == -1)
				{
					int nGetMessagePosition = WFE_FindMenuItem(pFileMenu, ID_FILE_GETNEWMAIL);
					if(nGetMessagePosition != -1)
					{
						pFileMenu->InsertMenu(nGetMessagePosition + 1, MF_BYPOSITION, ID_FILE_GETNEXT);

					}
				}
			}
		}
	}
}
void C3PaneMailFrame::SwitchUI( )
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
					case ID_MESSAGE_MARKBUTTON:
						dwButtonStyle |= TB_HAS_IMMEDIATE_MENU;
						break;

					case ID_MESSAGE_REPLYBUTTON:
					case ID_MESSAGE_NEXTUNREAD:
						dwButtonStyle |= TB_HAS_TIMED_MENU;
						break;
					case ID_EDIT_DELETE_3PANE:
						dwButtonStyle |= TB_DYNAMIC_TOOLTIP | TB_DYNAMIC_STATUS;
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



void C3PaneMailFrame::SetIsNews( BOOL bNews )
{
	if ( bNews != m_bNews ) {
		m_bNews = bNews;
		SwitchUI();
	}
}

void C3PaneMailFrame::SetSort( int idSort )
{
	if ( m_pOutliner ) {
		m_pOutliner->GetParent()->Invalidate();
		m_pOutliner->ScrollIntoView(m_pOutliner->GetFocusLine());
	}
}

void C3PaneMailFrame::DoNavigate( MSG_MotionType msgCommand )
{
	ASSERT( m_pPane && m_pOutliner );
	if ( !m_pPane || !m_pOutliner )
		return;

	if ( m_nLoadingFolder > 0 )
		return;

	MSG_ViewIndex	viewIndex = (MSG_ViewIndex) m_pOutliner->GetFocusLine();
	// Back end really wants -1 if there are no messages.
	if ( m_pOutliner->GetTotalLines() < 1 ) {
		viewIndex = (MSG_ViewIndex)-1;
	}

	MSG_ViewIndex	resultIndex = viewIndex;
	MessageKey		key = MSG_GetMessageKey( m_pPane, viewIndex );
	MessageKey		resultId = key;
	MSG_ViewIndex	threadIndex;
	MSG_FolderInfo	*pFolderInfo = NULL;

	if ((int) viewIndex >= -1) {
		// We don't want to be informed of a selection change if the BE does 
		// something weird like collapse a thread as part of navigation, since
		// we're probably going to select something ourselves.

		XP_Bool enable = FALSE;
		MSG_NavigateStatus(m_pPane, msgCommand, viewIndex, &enable, NULL);
		if (!enable)
			return;

		m_pOutliner->BlockSelNotify(TRUE);

		MSG_ViewNavigate(m_pPane, msgCommand, viewIndex,
						 &resultId, &resultIndex, &threadIndex, &pFolderInfo);

		m_pOutliner->BlockSelNotify(FALSE);

		if ( pFolderInfo )
		{
			C3PaneMailFrame *pFrame = C3PaneMailFrame::FindFrame( pFolderInfo );

			if (pFrame) {
				pFrame->ActivateFrame();

				if (pFolderInfo != pFrame->GetCurFolder()) {
					// We must be a category, since we found a frame,
					// but our folderInfos don't match
					pFrame->m_navPending = msgCommand;
					
					switch ( msgCommand ) {
					case MSG_Forward:
					case MSG_Back:
						pFrame->m_selPending = resultId;
						break;
					default:
						break;
					}
					pFrame->LoadFolder(pFolderInfo, MSG_MESSAGEKEYNONE, actionNavigation);
				} else {
					switch ( msgCommand ) {
					case MSG_Forward:
					case MSG_Back:
					case MSG_EditUndo:
					case MSG_EditRedo:
						pFrame->SelectMessage( resultId );
						break;

					case MSG_NextFolder:
					case MSG_NextMessage:
						pFrame->m_pOutliner->SelectItem( -1 );
						pFrame->DoNavigate(MSG_FirstMessage);
						break;

					case MSG_NextUnreadMessage:
					case MSG_NextUnreadThread:
					case MSG_NextUnreadGroup:
					case MSG_LaterMessage:
						pFrame->m_pOutliner->SelectItem( -1 );
						pFrame->DoNavigate( msgCommand );

					default:
						break;
					}
				}
			} else {
				m_navPending = msgCommand;
				
				switch ( msgCommand ) {
				case MSG_NextFolder:
				case MSG_NextUnreadGroup:
					break;
				case MSG_Forward:
				case MSG_Back:
					m_selPending = resultId;
					break;
				default:
					break;
				}

				LoadFolder(pFolderInfo, MSG_MESSAGEKEYNONE, actionNavigation);
			}
		} else if ( resultId != key && (int) resultIndex >= 0 &&
					(int) resultIndex < m_pOutliner->GetTotalLines() ) {
			m_pOutliner->SelectItem( CASTINT(resultIndex) );
			m_pOutliner->ScrollIntoView( CASTINT(resultIndex) );
		}
		else if ( resultId != key && resultId != MSG_MESSAGEKEYNONE) {
			SelectMessage(resultId);
		}
	}
}

void C3PaneMailFrame::DoUpdateNavigate( CCmdUI* pCmdUI, MSG_MotionType msgCommand )
{
	ASSERT( m_pPane && m_pOutliner );
	if ( !m_pPane || !m_pOutliner )
		return;

	XP_Bool	enable = FALSE;
	MSG_ViewIndex viewIndex = (MSG_ViewIndex) m_pOutliner->GetFocusLine();

	// Back end really wants -1 if there are no messages.
	if ( m_pOutliner->GetTotalLines() < 1 ) {
		viewIndex = (MSG_ViewIndex) -1;
	}

	if ((int) viewIndex >= -1) {
		MSG_NavigateStatus(m_pPane, msgCommand, viewIndex, &enable, NULL);
	}

	pCmdUI->Enable( enable );
}

void C3PaneMailFrame::LoadFrameMenu(CMenu *pPopup, UINT nIndex)
{
	CGenericFrame::LoadFrameMenu(pPopup, nIndex);
	//if it's the file menu we want to make sure it's correct
	HandleGetNNNMessageMenuItem();

}

int C3PaneMailFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// Add menus to genframe's menu map in order to load these menus
	// when they are accessed rather than when frame is first created.
	AddToMenuMap(0, IDM_MAILTHREADFILEMENU);
	AddToMenuMap(1, IDM_MAILTHREADEDITMENU);
	AddToMenuMap(2, IDM_MAILTHREADVIEWMENU);
	AddToMenuMap(3, IDM_MAILTHREADGOMENU);
	AddToMenuMap(4, IDM_MAILTHREADMESSAGEMENU);

	int res = CMailNewsFrame::OnCreate(lpCreateStruct);


	if ( res != -1) {
		m_pChrome->SetWindowTitle(XP_AppName);
	
		//I'm hardcoding string since I don't want it translated.
		m_pChrome->CreateCustomizableToolbar("Messenger", 3, TRUE);
		UINT nID = CASTUINT(ID_NAVIGATION_TOOLBAR);
		CButtonToolbarWindow *pWindow;
		BOOL bOpen, bShowing;

		int32 nPos;

		//I'm hardcoding because I don't want this translated
		m_pChrome->LoadToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"), nPos, bOpen, bShowing);

		// Create tool bar
		LPNSTOOLBAR pIToolBar;
		m_pChrome->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
		if ( pIToolBar ) {
			pIToolBar->Create( this, WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE|CBRS_TOP );
			pIToolBar->SetToolbarStyle( theApp.m_pToolbarStyle );
			SwitchUI();
			pWindow = new CButtonToolbarWindow(CWnd::FromHandlePermanent(pIToolBar->GetHWnd()), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
			m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_NAVIGATION_TOOLBAR, pWindow,nPos, 50, 37, 0, CString(szLoadString(nID)),theApp.m_pToolbarStyle, bOpen, FALSE);
			m_pChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, bShowing);

			pIToolBar->Release();
		}

		m_pInfoBar = new CFolderInfoBar;
		m_pInfoBar->Create( this, m_pPane );

		//I'm hardcoding because I don't want this translated
		m_pChrome->LoadToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"), nPos, bOpen, bShowing);

		CToolbarWindow *pToolWindow = new CToolbarWindow(m_pInfoBar, theApp.m_pToolbarStyle, 27, 27, eSMALL_HTAB);
		m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_LOCATION_TOOLBAR, pToolWindow,nPos, 27, 27, 0, CString(szLoadString(ID_LOCATION_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
		m_pChrome->ShowToolbar(ID_LOCATION_TOOLBAR, bShowing);

		m_barStatus.Create( this );
		m_barStatus.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->Attach( &m_barStatus );
			pIStatusBar->Release();
		}

		RecalcLayout();
	}

	if ( res != -1) {
		g_pLast3PaneMailFrame = this;
	} else {
		g_pLast3PaneMailFrame = NULL;
	}

	return res;
}
#ifndef ON_COMMAND_RANGE

BOOL C3PaneMailFrame::OnCommand( WPARAM wParam, LPARAM lParam )
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
	return CMsgListFrame::OnCommand( wParam, lParam );
}

BOOL C3PaneMailFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
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
	return CMsgListFrame::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

#endif

void C3PaneMailFrame::OnClose()
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

	PREF_SetRectPref("mailnews.thread_window_rect", left, top, width, height);
	PREF_SetIntPref("mailnews.thread_window_showwindow", prefInt);

	PREF_SetIntPref("mailnews.3pane_folder_width", m_pFolderSplitter->GetPaneSize());
	PREF_SetIntPref("mailnews.3pane_thread_height", m_pThreadSplitter->GetPaneSize());

	//I'm hardcoding because I don't want this translated
	m_pChrome->SaveToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"));
	m_pChrome->SaveToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"));

	CMailNewsFrame::OnClose();
}

void C3PaneMailFrame::OnDestroy()
{
	if (m_pAttachmentData)
		MSG_FreeAttachmentList(m_pMessagePane, m_pAttachmentData);
	m_pAttachmentData = NULL;

	DestroyMessagePane();

	if ( m_pFolderPane ) {
		// Ditto...
		MSG_Pane *pTemp = m_pFolderPane;
		m_pFolderPane = NULL;
		//otherwise folder pane sometimes gets accessed on quit
		m_pFolderOutliner->SetPane(NULL);
		MSG_DestroyPane( pTemp );
	}

	CView *pView = (CView *) GetDescendantWindow(IDW_MESSAGE_PANE, TRUE);

	ASSERT(pView && pView->IsKindOf(RUNTIME_CLASS(CNetscapeView)));

	if(pView)
		((CNetscapeView *)pView)->FrameClosing();

	CMsgListFrame::OnDestroy();
}

void C3PaneMailFrame::OnSetFocus(CWnd* pOldWnd)
{
	CMsgListFrame::OnSetFocus(pOldWnd);
	if (!m_pFocusWnd)
		m_pFocusWnd = m_pOutliner;
	m_pFocusWnd->SetFocus();
}

// Edit Menu Items

void C3PaneMailFrame::OnSelectThread()
{
	( (CMessageOutliner *) m_pOutliner)->SelectThread( -1 );
}

void C3PaneMailFrame::OnUpdateSelectThread ( CCmdUI* pCmdUI )
{
	BOOL bEnable = TRUE;

	bEnable &= MSG_GetToggleStatus( m_pPane, MSG_SortByThread, NULL, 0) == MSG_Checked;

	MSG_ViewIndex *indices;
	int count;	
	m_pOutliner->GetSelection( indices, count );

	bEnable &= (count > 0);

	pCmdUI->Enable( bEnable );
}

void C3PaneMailFrame::OnSelectFlagged()
{
	( (CMessageOutliner *) m_pOutliner)->SelectFlagged();
}

void C3PaneMailFrame::OnUpdateSelectFlagged( CCmdUI *pCmdUI )
{
	// We should probably only do this when there are
	// flagged messages in the view, but that's probably
	// too expensive
	pCmdUI->Enable( TRUE );
}

#define IS_IN_WINDOW(hParent,hChild)\
	(((hParent) == (hChild)) || ::IsChild(hParent, hChild))

void C3PaneMailFrame::OnSelectAll()
{
	HWND hwndFocus = ::GetFocus();
	HWND hwndOutliner = m_pOutliner ? m_pOutliner->m_hWnd : NULL;
	HWND hwndView = NULL;

	CWinCX*	pContext = GetActiveWinContext();
	if (pContext) {
		hwndView = pContext->GetPane();
	}

	BOOL bEnable = TRUE;
	UINT nID = IDS_MENU_ALL;

	if (IS_IN_WINDOW(hwndOutliner, hwndFocus)) {
		if (MSG_GetToggleStatus(m_pPane, MSG_SortByThread, NULL, 0) == MSG_Checked)
			((CMessageOutliner*)m_pOutliner)->SelectAllMessages();
		m_pOutliner->SelectRange( 0, -1, TRUE );
	} else if (hwndView == hwndFocus){
		LO_SelectAll(pContext->GetDocumentContext());
	}
}

void C3PaneMailFrame::OnUpdateSelectAll( CCmdUI *pCmdUI )
{
	HWND hwndFocus = ::GetFocus();
	HWND hwndOutliner = m_pOutliner ? m_pOutliner->m_hWnd : NULL;
	HWND hwndView = NULL;

	CWinCX*	pContext = GetActiveWinContext();
	if (pContext) {
		hwndView = pContext->GetPane();
	}

	BOOL bEnable = TRUE;
	UINT nID = IDS_MENU_ALL;

	if (IS_IN_WINDOW(hwndOutliner, hwndFocus)) {
		nID = IDS_MENU_ALLMESSAGES;
		bEnable = m_pOutliner->GetTotalLines() > 0;
	} else if (hwndView != hwndFocus){
		bEnable = FALSE;
	}
	pCmdUI->SetText(szLoadString(nID));
	pCmdUI->Enable(bEnable);
}

void C3PaneMailFrame::OnUpdateDeleteFrom3Pane(CCmdUI* pCmdUI)
{
	if(IsThreadFocus() || IsMessageFocus())
	{
		OnUpdateDeleteMessage(pCmdUI);

	}
	else
	{
		OnUpdateDeleteFolder(pCmdUI);
	}
}

void C3PaneMailFrame::OnDeleteFrom3Pane()
{
	if(IsThreadFocus() || IsMessageFocus())
	{
		OnDeleteMessage();
	}
	else
	{
		PrepareForDeleteFolder();
		OnDeleteFolder();
	}
}

// Message Menu Items

void C3PaneMailFrame::OnMove(UINT nID)
{
	if ( m_pPane ) {
		MSG_FolderInfo *folderInfo = FolderInfoFromMenuID( nID );

		ASSERT(folderInfo);
		if (folderInfo) {
			MSG_FolderLine folderLine;
			MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );

			MSG_ViewIndex *indices;
			int count;
			m_pOutliner->GetSelection( indices, count );

			// We want to make file behave for newsgroups
			if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ) {
				MSG_CopyMessagesIntoFolder( m_pPane, indices, count, folderInfo);
			} else {
				MSG_MoveMessagesIntoFolder( m_pPane, indices, count, folderInfo);
			}
			ModalStatusBegin( MODAL_DELAY );
		}
	}
}

void C3PaneMailFrame::OnCopy(UINT nID)
{		    
	if ( m_pPane ) {
		MSG_FolderInfo *folderInfo = FolderInfoFromMenuID( nID );

		ASSERT(folderInfo);
		if (folderInfo) {
			MSG_ViewIndex *indices;
			int count;
			m_pOutliner->GetSelection( indices, count );

			MSG_CopyMessagesIntoFolder( m_pPane, indices, count, folderInfo);
			ModalStatusBegin( MODAL_DELAY );
		}
	}
}

void C3PaneMailFrame::OnUpdateFile( CCmdUI *pCmdUI ) 
{
	//note you can get in here if m_nID == ID_MESSAGE_FILE or
	//if in the move and copy ranges. So we have to make sure it
	//works for all of those scenarios.
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );

		// find the desired effect
	MSG_DragEffect request = (pCmdUI->m_nID >= FIRST_MOVE_MENU_ID && 
			   pCmdUI->m_nID <= LAST_MOVE_MENU_ID) ? 
			   MSG_Require_Move : MSG_Require_Copy;

	// find the id
	int nID = (request == MSG_Require_Move) ? 
			   pCmdUI->m_nID - FIRST_MOVE_MENU_ID :
			   pCmdUI->m_nID - FIRST_COPY_MENU_ID;


	// get the folderInfo
	MSG_FolderInfo *folderInfo = NULL;
	MSG_DragEffect effect = MSG_Drag_Not_Allowed;

	if(pCmdUI->m_nID != ID_MESSAGE_FILE &&
	  (request == MSG_Require_Move && pCmdUI->m_nID >=FIRST_MOVE_MENU_ID 
									&& pCmdUI->m_nID <=LAST_MOVE_MENU_ID) ||
	   (request == MSG_Require_Copy && pCmdUI->m_nID >=FIRST_COPY_MENU_ID
									&& pCmdUI->m_nID <=LAST_COPY_MENU_ID))
	{
		folderInfo = FolderInfoFromMenuID(pCmdUI->m_nID);
	}

	if(pCmdUI->m_nID != ID_MESSAGE_FILE && folderInfo)
	{
		effect = MSG_DragMessagesIntoFolderStatus(m_pPane,
								NULL, 0, folderInfo, request);
	}

	BOOL bEnable = count > 0 && (pCmdUI->m_nID == ID_MESSAGE_FILE || 
								(pCmdUI->m_pSubMenu || effect != MSG_Drag_Not_Allowed));
//	BOOL bEnable = count > 0;
	if (pCmdUI->m_pSubMenu) {
	    pCmdUI->m_pMenu->EnableMenuItem(pCmdUI->m_nIndex,
										MF_BYPOSITION |(bEnable ? MF_ENABLED : MF_GRAYED));
	} else {
		pCmdUI->Enable( bEnable );
	}
}

void C3PaneMailFrame::OnViewMessage()
{
	MSG_FolderInfo *folderInfo = GetCurFolder();

	if ( folderInfo ) {
		if (m_pThreadSplitter->IsOnePaneClosed()) { 
			CFolderFrame::SetFolderPref( folderInfo, MSG_FOLDER_PREF_ONEPANE );
			m_pThreadSplitter->SetPaneSize(m_pOutlinerParent, m_pThreadSplitter->GetPreviousPaneSize());
			SendMessage(WM_COMMAND, ID_MESSAGE_SELECT, 0);

		} else {
			CFolderFrame::ClearFolderPref( folderInfo, MSG_FOLDER_PREF_ONEPANE );
			m_pThreadSplitter->SetPaneSize(m_pMessageView, 0);
		}
	}
}

void C3PaneMailFrame::OnUpdateViewMessage( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck(!m_pThreadSplitter->IsOnePaneClosed());
	pCmdUI->Enable( TRUE );

}

void C3PaneMailFrame::OnViewFolder()
{
	if (m_pFolderSplitter->IsOnePaneClosed()) {
		m_pFolderOutlinerParent->EnableWindow(TRUE);
		m_pFolderOutliner->EnableWindow(TRUE);

		MSG_FolderInfo *pFolderInfo = MSG_GetCurFolder(m_pPane);
		UpdateFolderPane(pFolderInfo);

		m_pFolderSplitter->SetPaneSize(m_pFolderOutlinerParent, m_pFolderSplitter->GetPreviousPaneSize());

	}
	else
	{
		m_pFolderSplitter->SetPaneSize(m_pFolderOutlinerParent , 0);

	}



}


void C3PaneMailFrame::OnUpdateViewFolder( CCmdUI *pCmdUI )
{
	pCmdUI->SetCheck(!m_pFolderSplitter->IsOnePaneClosed());
	pCmdUI->Enable( TRUE );

}

// Message Menu Items

void C3PaneMailFrame::OnIgnore()
{
	DoNavigate((MSG_MotionType) MSG_ToggleThreadKilled);
}

void C3PaneMailFrame::OnUpdateIgnore(CCmdUI *pCmdUI)
{
	pCmdUI->SetText(szLoadString(IDS_MENU_IGNORETHREAD));
	DoUpdateCommand(pCmdUI, MSG_ToggleThreadKilled);
}

void C3PaneMailFrame::OnSelectFolder()
{
	if (m_bBlockingFolderSelection)
		return;

	MSG_FolderLine folderLine;
	if (GetSelectedFolder(&folderLine))
	{
		C3PaneMailFrame* pFrame = NULL;

		if (!m_pPane)
			 CreateThreadPane();
		ASSERT(m_pPane);
		MSG_FolderInfo *folderInfo = MSG_GetCurFolder(m_pPane);
		if (folderLine.id == folderInfo)
		{
			m_pInfoBar->Update();
			if (folderLine.total == m_pOutliner->GetTotalLines())
				return;
		}
		// check if folder has loaded in other window
		pFrame = FindFrame(folderLine.id);
		if (pFrame) 
		{
			if (pFrame != this)
			{
				pFrame->ActivateFrame();
				// set selection back to original one when doing a double click
				int nIndex = m_pFolderOutliner->GetCurrentSelected();
				if (nIndex != -1)
				{
					m_pFolderOutliner->SelectItem(nIndex);
					m_pFolderOutliner->ScrollIntoView(nIndex);
				}
			}
		}
		else
			LoadFolder(folderLine.id, MSG_MESSAGEKEYNONE, m_actionOnLoad);
	}
	else
	{
		BlankOutThreadPane();
		BlankOutMessagePane(NULL);
	}
}

void C3PaneMailFrame::OnSelect()
{
	if (!m_pPane)
		return; // Abort!

	MessageKey idLoad = MSG_MESSAGEIDNONE;
	MSG_FolderInfo *folderInfo = MSG_GetCurFolder( m_pPane );

	// Make sure we need to load a message
	if ( m_pOutliner ) {
		MSG_MessageLine messageLine;
		MSG_ViewIndex *indices;
		int count;
		
		m_pOutliner->GetSelection( indices, count );
		if ( count == 1  && 
			 MSG_GetThreadLineByIndex( m_pPane, indices[0], 1, &messageLine ) && 
			 !m_pThreadSplitter->IsOnePaneClosed() ) {
			idLoad = messageLine.messageKey;

			if (!m_bNoScrollHack) {
				m_pOutliner->ScrollIntoView( CASTINT(indices[0]) );
			}
		}
	}

	if (!m_pMessagePane)
		return; // Abort!

	// Make sure we aren't already displaying the message
	MessageKey idCur;
	MSG_FolderInfo *folderInfoCur;
	MSG_ViewIndex idxCur;

	MSG_GetCurMessage( m_pMessagePane, &folderInfoCur, &idCur, &idxCur );

	if ( idCur != idLoad || folderInfo != folderInfoCur ) {

		// Our null document doesn't clear the title.
		if (idLoad == MSG_MESSAGEIDNONE)  
			BlankOutMessagePane(folderInfo);
		else
			MSG_LoadMessage( m_pMessagePane, folderInfo, idLoad );
	}

	// Sync our info bar
	m_pInfoBar->Update();

	m_bNoScrollHack = FALSE;
}

void C3PaneMailFrame::DoOpenMessage(BOOL bReuse)
{
	if (m_pOutliner) {
		MSG_ViewIndex *indices;
		int i, count;
		
		m_pOutliner->GetSelection(indices, count);
		for ( i = 0; i < count; i++ ) {
			MSG_FolderInfo *folderInfo = MSG_GetCurFolder( m_pPane );
			MessageKey id = MSG_GetMessageKey(m_pPane, indices[i]);

			if ( folderInfo && id != MSG_MESSAGEKEYNONE ) {
				MSG_FolderLine folderLine;
				MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );
				if (folderLine.flags & 
					(MSG_FOLDER_FLAG_DRAFTS |
					 MSG_FOLDER_FLAG_QUEUE |
					 MSG_FOLDER_FLAG_TEMPLATES)) {
					MSG_OpenDraft (m_pPane, folderInfo, id);
				} else {
					CMessageFrame *pFrame = CMessageFrame::FindFrame( folderInfo, id );

					if ( !pFrame ) {
						if ( bReuse && i == 0 ) {
							pFrame = GetLastMessageFrame();
						}
			
						if ( pFrame ) {
							pFrame->LoadMessage( folderInfo, id );
						} else {
							pFrame = CMessageFrame::Open( folderInfo, id );
						}
					}
					if ( pFrame ) {
						pFrame->ActivateFrame();
					}
				}
				int nPreviousSel = ((CMessageOutliner*)m_pOutliner)->GetCurrentSelected();
				if (-1 != nPreviousSel)
				{
					m_pOutliner->SelectItem(nPreviousSel);
					m_pOutliner->ScrollIntoView(nPreviousSel);
				}
			}
		}
	}
	m_pInfoBar->Update();
}

void C3PaneMailFrame::OnOpen( )
{
	BOOL bReuse = g_MsgPrefs.m_bMessageReuse;
	if (GetKeyState(VK_MENU) & 0x8000)
		bReuse = !bReuse;

	DoOpenMessage(bReuse);
}

void C3PaneMailFrame::OnOpenNew( )
{
	DoOpenMessage(FALSE);
}

void C3PaneMailFrame::OnOpenReuse( )
{
	DoOpenMessage(TRUE);
}

void C3PaneMailFrame::OnUpdateOpen(CCmdUI *pCmdUI)
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);

	pCmdUI->Enable(count > 0);
}

void C3PaneMailFrame::OnOpenNewFrame()
{
	if (GetFocus() == m_pFolderOutliner) 
	{
		MSG_FolderLine folderLine;
		if (GetSelectedFolder(&folderLine))
		{
			if (folderLine.level > 1) 
			{
				Open(folderLine.id);
				if (!g_MsgPrefs.m_bThreadReuse)
				{
					int nIndex = m_pFolderOutliner->GetCurrentSelected();
					if (nIndex != -1)
					{
						m_pFolderOutliner->SelectItem(nIndex);
						m_pFolderOutliner->ScrollIntoView(nIndex);
					}
				}
			}
		}
	}
}

void C3PaneMailFrame::OnContinue()
{
    CWinCX * pCX = GetMainWinContext();

    if(pCX->GetOriginY() + pCX->GetHeight() < pCX->GetDocumentHeight()) {
        pCX->Scroll(SB_VERT, SB_PAGEDOWN, 0, NULL);
	} else if (!pCX->IsLayingOut()) {
		DoNavigate( MSG_NextUnreadMessage ); 
	}
}

void C3PaneMailFrame::OnContainer()
{
	MSG_FolderInfo *folderInfo = MSG_GetCurFolder( m_pPane );
	if ( folderInfo ) {
		CFolderFrame::Open( folderInfo );
	}
}

void C3PaneMailFrame::DoPriority( MSG_PRIORITY priority )
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);
	for (int i = 0; i < count; i++) {
		MSG_SetPriority( m_pPane,
						 MSG_GetMessageKey( m_pPane, indices[i] ),
						 priority );
	}
}

void C3PaneMailFrame::OnPriorityLowest()
{
	DoPriority( MSG_LowestPriority );
}

void C3PaneMailFrame::OnPriorityLow()
{
	DoPriority( MSG_LowPriority );
}

void C3PaneMailFrame::OnPriorityNormal()
{
	DoPriority( MSG_NormalPriority );
}

void C3PaneMailFrame::OnPriorityHigh()
{
	DoPriority( MSG_HighPriority );
}

void C3PaneMailFrame::OnPriorityHighest()
{
	DoPriority( MSG_HighestPriority );
}

void C3PaneMailFrame::OnUpdatePriority(CCmdUI *pCmdUI)
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices,count);

	pCmdUI->Enable( !m_bNews && (count > 0) );
}

void C3PaneMailFrame::OnDoneGettingMail()
{
	MSG_ViewIndex	resultIndex = 0;
	MessageKey		resultId = MSG_MESSAGEKEYNONE;
	MSG_ViewIndex	threadIndex = 0;
	MSG_FolderInfo	*pFolderInfo = NULL;

	// We don't want to be informed of a selection change if the BE does 
	// something weird like collapse a thread as part of navigation, since
	// we're probably going to select something ourselves.
	m_pOutliner->BlockSelNotify(TRUE);

	MSG_ViewNavigate(m_pPane, MSG_FirstNew, 0,
					 &resultId, &resultIndex, &threadIndex, &pFolderInfo);

	m_pOutliner->BlockSelNotify(FALSE);
	if (resultId != MSG_MESSAGEKEYNONE) {
		resultIndex = MSG_GetMessageIndexForKey(m_pPane, resultId, TRUE);
		m_pOutliner->ScrollIntoView(CASTINT(resultIndex));
	}
}


void C3PaneMailFrame::DoUndoNavigate( MSG_MotionType motionCmd )
{
	MessageKey key = MSG_MESSAGEKEYNONE;
	MSG_FolderInfo *folder = NULL;

	if (!m_pPane || !m_pOutliner) return;

	UndoStatus undoStatus = MSG_GetUndoStatus(m_pPane); 

	if ( UndoComplete == undoStatus ) {
		if (MSG_GetUndoMessageKey(m_pPane, &folder, &key) && folder) {
			if (GetCurFolder() == folder) {
				SelectMessage(key);
			}
			else {
				/* need to load new folder */
				m_navPending = motionCmd;
				m_selPending = key;
				LoadFolder(folder, MSG_MESSAGEKEYNONE, actionNavigation);
			}
		}
	}
}

// File Menu Items

void C3PaneMailFrame::OnEmptyTrash()
{
	MSG_FolderLine folderLine;
	MSG_FolderInfo *curFolderInfo = MSG_GetCurFolder( m_pPane );
	if (MSG_GetFolderLineById(WFE_MSGGetMaster(), curFolderInfo, &folderLine)) 
	{
		if (folderLine.flags & MSG_FOLDER_FLAG_TRASH)
		{
			BlankOutThreadPane();
			BlankOutMessagePane(curFolderInfo);
		}
	}

	CMailNewsFrame::OnEmptyTrash();
}

// Edit Menu Items

void C3PaneMailFrame::OnEditUndo()
{
    DoCommand(MSG_Undo);
	DoUndoNavigate(MSG_EditUndo);
}

void C3PaneMailFrame::OnEditRedo()
{
    DoCommand(MSG_Redo);
	DoUndoNavigate(MSG_EditRedo);
}

//lpttt->szText can only hold 80 characters
#define MAX_TOOLTIP_CHARS 79
//status can only hold 1000 characters 
#define MAX_STATUS_CHARS 999

LRESULT C3PaneMailFrame::OnFillInToolTip(WPARAM wParam, LPARAM lParam)
{
	HWND hwnd = (HWND)wParam;
	LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam;

	CToolbarButton *pButton = (CToolbarButton *)CWnd::FromHandle(hwnd);
	UINT nCommand = pButton->GetButtonCommand();

	const char * pText = "";
	if(nCommand == ID_EDIT_DELETE_3PANE)
	{
		if(IsThreadFocus() || IsMessageFocus())
		{
			pText = szLoadString(IDS_DELETEMESSAGE_TOOLTIP);
		}
		else
		{
			pText = szLoadString(IDS_DELETEFOLDER_TOOLTIP);
		}

	}

	strncpy(lpttt->szText, pText, MAX_TOOLTIP_CHARS);

	return 1;
}

LRESULT C3PaneMailFrame::OnFillInToolbarButtonStatus(WPARAM wParam, LPARAM lParam)
{
	UINT nCommand = LOWORD(wParam);
	char *pStatus = (char*)lParam;

	const char * pText = "";

	if(nCommand == ID_EDIT_DELETE_3PANE)
	{
		if(IsThreadFocus() || IsMessageFocus())
		{
			pText = szLoadString(IDS_DELETEMESSAGE_STATUS);
		}
		else
		{
			pText = szLoadString(IDS_DELETEFOLDER_STATUS);
		}

	}

	strncpy(pStatus, pText, MAX_TOOLTIP_CHARS);
	return 1;
}

void C3PaneMailFrame::SelectMessage( MessageKey key )
{
	MSG_ViewIndex index = 
		MSG_GetMessageIndexForKey( m_pPane, key, TRUE );
	if ( index != MSG_VIEWINDEXNONE ) {
		m_pOutliner->SelectItem( CASTINT(index) );
		m_pOutliner->ScrollIntoView(CASTINT(index));
	}
}

void C3PaneMailFrame::CreateThreadPane()
{
	m_pPane = MSG_CreateThreadPane(GetMainContext()->GetContext(), m_pMaster);
	if (m_pPane)
	{
		if (m_pOutliner)
			m_pOutliner->SetPane(m_pPane);
		MSG_SetFEData( m_pPane, (LPVOID) (LPUNKNOWN) (LPMSGLIST) this );
#ifdef MOZ_NEWADDR
		AB_SetShowPropertySheetForEntryFunc(m_pPane, ShowPropertySheetForEntry);
#endif
		if (m_pInfoBar)
			m_pInfoBar->SetPane(m_pPane);
	}
}

void C3PaneMailFrame::CreateMessagePane()
{
	m_pMessagePane = MSG_CreateMessagePane(GetMainContext()->GetContext(), m_pMaster );

	if (m_pMessagePane)
	{
		MSG_SetFEData( m_pMessagePane, (LPVOID) (LPUNKNOWN) (LPMSGLIST) this );
		MSG_SetMessagePaneCallbacks(m_pMessagePane, &MsgPaneCB, NULL);
	}
}

void C3PaneMailFrame::CreateFolderOutliner()
{
	m_pFolderOutlinerParent = new CFolderOutlinerParent;
#ifdef _WIN32
	m_pFolderOutlinerParent->CreateEx(WS_EX_CLIENTEDGE, NULL, NULL,
								 WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								 0,0,0,0, m_pFolderSplitter->m_hWnd, (HMENU) IDW_FOLDER_PANE, NULL );
#else
	m_pFolderOutlinerParent->Create( NULL, NULL,
								 WS_BORDER|WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								 CRect(0,0,0,0), m_pFolderSplitter, IDW_FOLDER_PANE );
#endif

	m_pFolderOutlinerParent->EnableFocusFrame(TRUE);
	m_pFolderPane = MSG_CreateFolderPane( GetMainContext()->GetContext(), m_pMaster );
	m_pFolderOutliner = DOWNCAST(CFolderOutliner, m_pFolderOutlinerParent->m_pOutliner);
	m_pFolderOutlinerParent->CreateColumns ( );
	m_pFolderOutliner->SetPane(m_pFolderPane);

	MSG_SetFEData( m_pFolderPane, (LPVOID) (LPUNKNOWN) (LPMSGLIST) this );
}

void C3PaneMailFrame::UpdateFolderPane(MSG_FolderInfo *pFolderInfo)
{
	if (m_pFolderOutliner && m_pFolderPane)
	{
		MSG_ViewIndex *indices;
		int count;	
		m_pFolderOutliner->GetSelection( indices, count );   

		MSG_ViewIndex index = MSG_GetFolderIndexForInfo(m_pFolderPane, 
								pFolderInfo, TRUE);

		if (index != MSG_VIEWINDEXNONE && indices[0] != index)
			m_pFolderOutliner->SelectItem( CASTINT(index) );
	}
}

BOOL C3PaneMailFrame::GetSelectedFolder(MSG_FolderLine* pFolderLine)
{
	if (m_pFolderOutliner && m_pFolderPane)
	{
		MSG_ViewIndex *indices;
		int count = 0;

		m_pFolderOutliner->GetSelection(indices, count);
		if (count == 1 &&
			MSG_GetFolderLineByIndex(m_pFolderPane, indices[0], 1, pFolderLine)) 
			return TRUE;
		else
		{
			int nIndex = m_pFolderOutliner->GetCurrentSelected();
			if (count == 0)
			{
				indices[0] = nIndex;
				if (MSG_GetFolderLineByIndex(m_pFolderPane, indices[0], 1, pFolderLine)) 
					return TRUE;
			}
			else
			{
				ASSERT(m_pPane);
				if (m_pPane)
				{
					MSG_FolderInfo *pFolderInfo = MSG_GetCurFolder( m_pPane );
					if (pFolderInfo && MSG_GetFolderLineById(m_pMaster, pFolderInfo, pFolderLine)) 
						return TRUE;
				}
			}
		}
	}
	return FALSE;
}

void C3PaneMailFrame::PrepareForDeleteFolder()
{
	MSG_FolderLine folderLine;
	if (GetSelectedFolder(&folderLine))
	{
		BlankOutMessagePane(folderLine.id);
		DestroyMessagePane();
		CreateMessagePane();

		BlankOutThreadPane();
		MSG_Pane *pTemp = m_pPane;
		m_pPane = NULL;
		MSG_DestroyPane(pTemp);
		CreateThreadPane();

		m_bBlockingFolderSelection = TRUE;
	}
}

void C3PaneMailFrame::BlankOutThreadPane()
{
	m_pOutliner->SelectItem(-1);
	m_pOutliner->SetTotalLines(0);
	m_pOutliner->Invalidate();
	m_pOutliner->UpdateWindow();
}

void C3PaneMailFrame::BlankOutMessagePane(MSG_FolderInfo *pFolderInfo)
{
	if (m_pMessagePane)
	{
		MSG_LoadMessage(m_pMessagePane, pFolderInfo, MSG_MESSAGEKEYNONE);
		m_pChrome->SetDocumentTitle("");
		m_pMessageView->SetAttachments(NULL, 0);
	}
}

void C3PaneMailFrame::DestroyMessagePane()
{
	if (m_pMessagePane) {
		// Since MSG_DestroyPane can result in notifications that
		// we may attempt to act upon with the dying pane, NULL
		// it out first
		MSG_Pane *pTemp = m_pMessagePane;
		m_pMessagePane = NULL;
		MSG_DestroyPane( pTemp );
	}
}

void C3PaneMailFrame::BlankOutRightPanes()
{
	MSG_FolderInfo *pCurFolderInfo = MSG_GetCurFolder( m_pPane );
	if (pCurFolderInfo)
		BlankOutMessagePane(pCurFolderInfo);
	DestroyMessagePane();
	CreateMessagePane();

	BlankOutThreadPane();
	MSG_Pane *pTemp = m_pPane;
	m_pPane = NULL;
	MSG_DestroyPane(pTemp);
	CreateThreadPane();
}

void C3PaneMailFrame::CheckForChangeFocus()
{
	MSG_FolderInfo* pFolderInfo = NULL;
	MSG_FolderLine folderLine;

	if (m_pPane)
		pFolderInfo = MSG_GetCurFolder(m_pPane);
	if (m_pFolderOutliner && m_pFolderPane)
	{
		MSG_ViewIndex *indices;
		int count = 0;

		m_pFolderOutliner->GetSelection(indices, count);
		if (count == 1)
			MSG_GetFolderLineByIndex(m_pFolderPane, indices[0], 1, &folderLine);
	}
	if (!pFolderInfo || (pFolderInfo != folderLine.id))
		m_pOutliner->SetFocus();
}

void C3PaneMailFrame::LoadFolder( MSG_FolderInfo *folderInfo, MessageKey key, int action )
{
	MSG_FolderInfo *curFolderInfo = MSG_GetCurFolder( m_pPane );
	if (curFolderInfo != folderInfo && m_pPane) 
	{
		//remember last message
		MessageKey lastKey = GetCurMessage();
		MSG_SetLastMessageLoaded(curFolderInfo, lastKey); 

		if ( key == MSG_MESSAGEKEYNONE ) 
		{
			m_actionOnLoad = action;
		} 
		else 
		{
			m_actionOnLoad = actionSelectKey;
			m_selPending = key;
		}

		BlankOutMessagePane(folderInfo);

		UIForFolder( folderInfo );

		if (m_nLoadingFolder == 0) 
		{
			m_nLoadingFolder++; // FYI
			m_pOutliner->BlockSelNotify(TRUE);
		} 
		MSG_LoadFolder( m_pPane, folderInfo );
	}
}

MessageKey C3PaneMailFrame::GetCurMessage() const
{
	MessageKey res = MSG_MESSAGEKEYNONE;
	MSG_ViewIndex *indices;
	int count;

	m_pOutliner->GetSelection( indices, count );

	if ( count == 1 ) {
		res = MSG_GetMessageKey( m_pPane, indices[0] );
	}
	return res;
}

MSG_FolderInfo *C3PaneMailFrame::GetCurFolder() const
{
	return m_pPane ? MSG_GetCurFolder( m_pPane ) : NULL;
}

LPCTSTR C3PaneMailFrame::GetWindowMenuTitle()
{
	static CString cs;

	cs.LoadString(m_bNews ? IDS_NEWSGROUPCOLON : IDS_FOLDERCOLON);
	CString csTitle;
	csTitle.LoadString(IDS_NOTITLE);

	MSG_FolderLine folderLine;
	MSG_FolderInfo *folderInfo;
	folderInfo = MSG_GetCurFolder(m_pPane);

	if ( folderInfo && MSG_GetFolderLineById( m_pMaster, folderInfo, &folderLine ) ) {
		csTitle = folderLine.prettyName && folderLine.prettyName[0] ? 
				  folderLine.prettyName :
				  folderLine.name;
	}

	cs += csTitle;

	return cs;
}

void C3PaneMailFrame::OnFileBookmark()
{
	FileBookmark();
}

void C3PaneMailFrame::OnUpdateFileBookmark(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetCurFolder() != NULL);
}

BOOL C3PaneMailFrame::FileBookmark()
{
	BOOL res = FALSE;
	MSG_ViewIndex *indices = NULL;
	int count = 0;
	m_pOutliner->GetSelection(indices, count);

	if (count < 1) {
		// Add bookmark to folder
		MSG_FolderLine folderLine;
		MSG_FolderInfo *folderInfo = MSG_GetCurFolder(m_pPane);

		if ( folderInfo && MSG_GetFolderLineById( m_pMaster, folderInfo, &folderLine ) ) {
			URL_Struct *url = MSG_ConstructUrlForPane(m_pPane);
			if (url) {
				const char *name = (folderLine.prettyName && folderLine.prettyName[0]) ?
								   folderLine.prettyName : folderLine.name;

				HT_AddBookmark(url->address, (char*)name); // Updated to Aurora (Dave H.)
				NET_FreeURLStruct( url );
				res = TRUE;
			}
		}
	} else {
			// Add bookmark to each selected message
		MSG_MessageLine messageLine;
		for (int i = 0; i < count; i++) {
			if (MSG_GetThreadLineByIndex(m_pPane, indices[i], 1, &messageLine)) {
				URL_Struct *url = MSG_ConstructUrlForMessage( m_pPane, messageLine.messageKey );

				char *buf = IntlDecodeMimePartIIStr(messageLine.subject, INTL_DocToWinCharSetID(m_iCSID), FALSE);
				char *name = (buf && buf[0]) ? buf : messageLine.subject;

				if ( url ) {			
					HT_AddBookmark(url->address, (char*)name); // Updated to Aurora (Dave H.) 
					NET_FreeURLStruct( url );
					res = TRUE;
				}
				XP_FREEIF(buf);
			}
		}
	}

	return res;
}

void C3PaneMailFrame::SetFocusWindowBackToFrame()
{
	CWnd* pWnd = GetFocus();
	if (m_pFocusWnd != pWnd)
		m_pFocusWnd->SetFocus();
}

BOOL C3PaneMailFrame::MessageViewClosed()
{
	return m_pThreadSplitter->IsOnePaneClosed();
}

C3PaneMailFrame *C3PaneMailFrame::FindFrame( MSG_FolderInfo *folderInfo )
{
	CGenericFrame *pFrame = theApp.m_pFrameList;
	while (pFrame) {
		if (pFrame->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame))) {
			C3PaneMailFrame *pThreadFrame = DYNAMIC_DOWNCAST(C3PaneMailFrame, pFrame);
			MSG_FolderInfo *folderInfo2 = pThreadFrame->GetCurFolder();

			if (folderInfo == folderInfo2)
				return pThreadFrame;
		}
		pFrame = pFrame->m_pNext;
	}
	return NULL;
}

C3PaneMailFrame *C3PaneMailFrame::Open( )
{
	g_MsgPrefs.m_pThreadTemplate->OpenDocumentFile( NULL );

	return g_pLast3PaneMailFrame;
}

C3PaneMailFrame *C3PaneMailFrame::Open
( MSG_FolderInfo *folderInfo, MessageKey key, BOOL* pContinue )
{
	BOOL bReuse = g_MsgPrefs.m_bThreadReuse;
	if (GetKeyState(VK_MENU) & 0x8000)
		bReuse = !bReuse;

	C3PaneMailFrame *pFrame = FindFrame( folderInfo );
	MSG_FolderInfo *pCurrentFolder = NULL;
	if (pFrame)
		pCurrentFolder = MSG_GetCurFolder( pFrame->m_pPane );

	if ( pFrame && (bReuse || (folderInfo == pCurrentFolder))) 
	{
		if (folderInfo != MSG_GetCurFolder( pFrame->m_pPane )) 
			pFrame->LoadFolder(folderInfo, key);
		else 
		{
			if ( key != MSG_MESSAGEKEYNONE )
				pFrame->SelectMessage( key );
			else
			{
				if (pFrame->m_nLoadingFolder && pContinue)
					*pContinue = TRUE;
			}
		}
		if (s_bGetMail)
			pFrame->PostMessage(WM_COMMAND, (WPARAM) ID_FILE_GETNEWMAIL, (LPARAM) 0);
		if (pContinue && *pContinue)
			return 	pFrame;
	} 
	else 
	{
		if ( !bReuse || !(pFrame = CMailNewsFrame::GetLastThreadFrame()) ) 
		{
			// Try to bring up the right UI the first time.
			MSG_FolderLine folderLine;
			MSG_GetFolderLineById(WFE_MSGGetMaster(), folderInfo, &folderLine);
			s_bHintNews = !(folderLine.flags & MSG_FOLDER_FLAG_MAIL);

			pFrame = C3PaneMailFrame::Open();
			if (pFrame)
			{
				if (pFrame->m_pFolderOutliner && pFrame->m_pFolderPane)
				{
					MSG_ViewIndex index = MSG_GetFolderIndexForInfo(pFrame->m_pFolderPane, 
										   folderInfo, TRUE);
					if (index != MSG_VIEWINDEXNONE) 
						pFrame->m_pFolderOutliner->SelectItem( CASTINT(index) );
				}
				pFrame->m_pOutliner->Invalidate();
				pFrame->m_pFolderOutliner->Invalidate();
				pFrame->m_pMessageView->Invalidate();
				pFrame->UpdateWindow();
			}
		}

		if (pFrame) 
		{
			if (s_bGetMail)
				pFrame->m_bWantToGetMail = TRUE;

			pFrame->LoadFolder( folderInfo, key );
		}
	}
	if ( pFrame ) 
		pFrame->ActivateFrame();

	return pFrame;
}

C3PaneMailFrame *C3PaneMailFrame::OpenInbox( BOOL bGetNew )
{
	if(theApp.m_hPostalLib)
	{		
		FEU_AltMail_ShowMailBox(); 
		return NULL;
	}
	else
	{
		if (!CheckWizard())
			return NULL;

		C3PaneMailFrame *pFrame = NULL;
		MSG_FolderInfo *folderInfo = NULL;
		MSG_GetFoldersWithFlag( WFE_MSGGetMaster(),
								MSG_FOLDER_FLAG_INBOX,
								&folderInfo, 1 );

		s_bGetMail = bGetNew;

		if (folderInfo)
		{
			pFrame = Open( folderInfo );

			if (pFrame->m_pFolderOutliner && pFrame->m_pFolderPane)
			{
				MSG_ViewIndex index = MSG_GetFolderIndexForInfo(pFrame->m_pFolderPane, 
					                   folderInfo, TRUE);
				if (index != MSG_VIEWINDEXNONE) 
					pFrame->m_pFolderOutliner->SelectItem( CASTINT(index) );
			}
		}
		s_bGetMail = FALSE;

		return pFrame;
	}
}


C3PaneMailFrame *WFE_MSGOpenInbox(BOOL bGetNew)
{
	return C3PaneMailFrame::OpenInbox(bGetNew);
}


#ifdef DEBUG_WHITEBOX
BOOL C3PaneMailFrame::WhiteBox_DoesMessageExist( MessageKey key )
{
	MSG_ViewIndex index = MSG_GetMessageIndexForKey( m_pPane, key, TRUE );
	if ( index != MSG_VIEWINDEXNONE )
		return TRUE;

	return FALSE;
}
#endif

