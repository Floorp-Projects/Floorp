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

//
// Main frame for mail reading window
//

#include "stdafx.h"

#include "rosetta.h"
#include "intl_csi.h"
#include "netsvw.h"
#include "cxsave.h"
#include "mailfrm.h"
#include "fldrfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "msgview.h"
#include "msgcom.h"
#include "mailmisc.h"
#include "template.h"
#include "subnews.h"
#include "mnprefs.h"
#include "mailpriv.h"
#include "prefapi.h"
#include "ssl.h"
#include "xpgetstr.h"
#include "wfemsg.h"
#include "abcom.h"
#include "addrfrm.h" //for MOZ_NEWADDR

#ifdef DEBUG_WHITEBOX
#include "xp_trace.h"
#include "stdio.h"
#include "qaoutput.h"
#include "qatrace.h"
#endif

extern "C"
{
	extern int MK_NNTP_SERVER_NOT_CONFIGURED;
};

extern "C" MSG_Host *DoAddNewsServer(CWnd* pParent, int nFromWhere);

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

int g_iModalDelay = -1;
int iMailFrameCount  = 0;


MSG_Master *WFE_MSGGetMaster();				

extern "C" void wfe_AttachmentCount(MSG_Pane *messagepane, void* closure,
								    int32 numattachments, XP_Bool finishedloading)
{
	LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( messagepane );
	if (pUnk) {
		LPMAILFRAME pInterface = NULL;
		pUnk->QueryInterface( IID_IMailFrame, (LPVOID *) &pInterface );
		if ( pInterface ) {
			pInterface->AttachmentCount( messagepane, closure, numattachments, finishedloading );
			pInterface->Release();
		}
	}
}

extern "C" void wfe_UserWantsToSeeAttachments(MSG_Pane *messagepane, void* closure)
{
	LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( messagepane );
	if (pUnk) {
		LPMAILFRAME pInterface = NULL;
		pUnk->QueryInterface( IID_IMailFrame, (LPVOID *) &pInterface );
		if ( pInterface ) {
			pInterface->UserWantsToSeeAttachments( messagepane, closure );
			pInterface->Release();
		}
	}
}

MSG_MessagePaneCallbacks MsgPaneCB = {
	wfe_AttachmentCount,
	wfe_UserWantsToSeeAttachments
};

CMailNewsCX::CMailNewsCX(ContextType ctMyType, CFrameWnd *pFrame )    
{
	m_pFrame = pFrame;
	m_pIChrome = NULL;
    m_cxType = ctMyType;
	m_lPercent = 0;
	m_bAnimated = FALSE;
}

CMailNewsCX::~CMailNewsCX()
{
	if (m_pIChrome) {
		m_pIChrome->Release();
	}
}

void CMailNewsCX::SetChrome( LPUNKNOWN pUnk )
{
	LPCHROME pIChrome = NULL;

	if ( pUnk ) {
		pUnk->QueryInterface( IID_IChrome, (LPVOID *) &pIChrome);
		ASSERT(pIChrome);
	}

	if ( m_pIChrome == pIChrome ) {
		return;
	}

	if ( m_pIChrome ) {
		m_pIChrome->Release();
	}

	m_pIChrome = pIChrome;

	if ( m_pIChrome ) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetProgress( CASTINT(m_lPercent) );
			pIStatusBar->SetStatusText( (LPCTSTR) m_csProgress );
			pIStatusBar->Release();
		}
		if ( m_bAnimated ) {
			m_pIChrome->StartAnimation();
		} else {
			m_pIChrome->StopAnimation();
		}
	}
}

void CMailNewsCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent ) {
	//	Ensure the safety of the value.

	lPercent = lPercent < 0 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_lPercent == lPercent ) {
		return;
	}

	m_lPercent = lPercent;
	if (m_pIChrome) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetProgress( CASTINT(lPercent) );
			pIStatusBar->Release();
		}
	}
}

void CMailNewsCX::Progress(MWContext *pContext, const char *pMessage) {
	m_csProgress = pMessage;
	if ( m_pIChrome ) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetStatusText( pMessage );
			pIStatusBar->Release();
		}
	}
}

int32 CMailNewsCX::QueryProgressPercent()	{
	return m_lPercent;
}

void CMailNewsCX::SetDocTitle( MWContext *pContext, char *pTitle )
{
}

void CMailNewsCX::StartAnimation()
{
	m_bAnimated = TRUE;
	if ( m_pIChrome ) {
		m_pIChrome->StartAnimation();
	}
}

void CMailNewsCX::StopAnimation()
{
	m_bAnimated = FALSE;
	if ( m_pIChrome ) {
		m_pIChrome->StopAnimation();
	}
}

void CMailNewsCX::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CStubsCX::AllConnectionsComplete(pContext);

	//	Okay, stop the animation.
	StopAnimation();

	//	Also, we can clear the progress bar now.
	m_lPercent = 0;
	if ( m_pIChrome ) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pIChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetProgress( 0 );
			pIStatusBar->Release();
		}
	}
    if (m_pFrame) {
		m_pFrame->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

void CMailNewsCX::UpdateStopState( MWContext *pContext )
{
    if (m_pFrame) {
		m_pFrame->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

CWnd *CMailNewsCX::GetDialogOwner() const {
	return m_pFrame;
}

/////////////////////////////////////////////////////////////////////////////
// CMailNewsFrame

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CMailNewsFrame, CGenericFrame)
IMPLEMENT_DYNCREATE(CMsgListFrame, CMailNewsFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif


STDMETHODIMP CMailNewsFrame::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) this;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CMailNewsFrame::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CMailNewsFrame::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

// IMailFrame interface

CMailNewsFrame *CMailNewsFrame::GetMailNewsFrame()
{
	return (CMailNewsFrame *) this; 
}

MSG_Pane *CMailNewsFrame::GetPane()
{
	return m_pPane;
}

void CMailNewsFrame::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
}

void CMailNewsFrame::AttachmentCount(MSG_Pane *messagepane, void* closure,
									 int32 numattachments, XP_Bool finishedloading)
{
    if (m_pMessagePane != NULL)
    {
	    m_nAttachmentCount = numattachments;
		// OK, so 64 is arbitrary, but a bunch of menu ranges need to
		// be changed if it changes.
		if (m_nAttachmentCount > 64)
			m_nAttachmentCount = 64;

	    if (m_pAttachmentData)
		    MSG_FreeAttachmentList(m_pMessagePane, m_pAttachmentData);
	    m_pAttachmentData = NULL;
    	MSG_GetViewedAttachments(m_pMessagePane, &m_pAttachmentData, NULL);

	    if (m_pMessageView) {
			if (numattachments == 0)
			    m_pMessageView->ShowAttachments(FALSE);
		    m_pMessageView->SetAttachments( numattachments, m_pAttachmentData );
		}
    }
}

void CMailNewsFrame::UserWantsToSeeAttachments(MSG_Pane *messagepane, void* closure)
{
    if (m_pMessagePane != NULL)
    {
	    if (m_pMessageView)
		    m_pMessageView->ShowAttachments(!m_pMessageView->AttachmentsVisible());
    }
}

CMailNewsFrame::CMailNewsFrame()
{
	m_bNews = FALSE;
	m_bCategory = FALSE;

	// Folder Frame is the only one that's different
	m_iMessageMenuPos = 4;
	m_iMoveMenuPos = 7;
	m_iCopyMenuPos = 8;
	m_iAddAddressBookMenuPos = 10;

	m_iFileMenuPos = 0;
	m_iAttachMenuPos = 1;

	m_pPane = NULL;
	m_pMessagePane = NULL;
	m_pMaster = NULL;
	m_ulRefCount = 1; // Don't auto delete
	m_pOutliner = NULL;
	m_pInfoBar = NULL;

	m_hAccel = NULL;

	m_pAttachmentData = NULL;
	m_nAttachmentCount = 0;
	m_pMessageView = NULL;
}

//
// The menu gets cleaned up on window destruction but we need
//   to delete our accelerator table ourselves
//
CMailNewsFrame::~CMailNewsFrame()
{
    if(m_hAccelTable != NULL)   {
#ifdef _WIN32
		VERIFY(::DestroyAcceleratorTable( m_hAccelTable ));
#else
        ::FreeResource((HGLOBAL)m_hAccelTable);
#endif
        m_hAccelTable = NULL;
    }

	delete m_pInfoBar;

	if ( m_hAccel )
#ifdef _WIN32
		::DestroyAcceleratorTable( m_hAccel );
#else
        ::FreeResource( (HGLOBAL)m_hAccel );
#endif
}                          

C3PaneMailFrame *CMailNewsFrame::GetLastThreadFrame(CFrameWnd *pExclude)
{
	CFrameGlue *pGlue = NULL;

	//	Loop through all active frames.
	POSITION rIndex = m_cplActiveFrameStack.GetHeadPosition();

	while( rIndex != NULL )	{
		pGlue = (CFrameGlue *) m_cplActiveFrameStack.GetNext( rIndex );
		
		if( pGlue->GetFrameWnd() ) {
			if ( pGlue->GetFrameWnd()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)) ) {
				// Win16 somehow gets the newest frame on the stack before
				// I want it to, so I need to force it to be skipped
				if (pGlue->GetFrameWnd() != pExclude)
					return (C3PaneMailFrame *) pGlue->GetFrameWnd();
			}
		}
	}
	return NULL;
}

CMessageFrame *CMailNewsFrame::GetLastMessageFrame(CFrameWnd *pExclude)
{
	CFrameGlue *pGlue = NULL;

	//	Loop through all active frames.
	POSITION rIndex = m_cplActiveFrameStack.GetHeadPosition();

	while( rIndex != NULL )	{
		pGlue = (CFrameGlue *) m_cplActiveFrameStack.GetNext( rIndex );
		
		if( pGlue->GetFrameWnd() ) {
			if ( pGlue->GetFrameWnd()->IsKindOf(RUNTIME_CLASS(CMessageFrame)) ) {
				// Win16 somehow gets the newest frame on the stack before
				// I want it to, so I need to force it to be skipped
				if (pGlue->GetFrameWnd() != pExclude)
					return (CMessageFrame *) pGlue->GetFrameWnd();
			}
		}
	}
	return NULL;
}


extern int iLowerColors;
extern int iLowerSystemColors;

BOOL CMailNewsFrame::LoadAccelerators( UINT nID )
{
	return LoadAccelerators(MAKEINTRESOURCE(nID));
}

BOOL CMailNewsFrame::LoadAccelerators( LPCSTR lpszResource )
{
	BOOL bSuccess = FALSE;

	VERIFY( m_hAccel = ::LoadAccelerators(AfxGetResourceHandle(), lpszResource ));
	if ( m_hAccel )
		bSuccess = TRUE;

	return bSuccess;
}

void CMailNewsFrame::ActivateFrame( int nCmdShow )
{
	nCmdShow = (nCmdShow == -1) ? (IsIconic() ? SW_RESTORE : SW_SHOW) : nCmdShow;

#ifdef _WIN32
	SetForegroundWindow();
#else
	SetActiveWindow();
#endif

	CGenericFrame::ActivateFrame( nCmdShow );
}

BOOL CMailNewsFrame::PreCreateWindow( CREATESTRUCT& cs )
{
	BOOL res = CGenericFrame::PreCreateWindow(cs);

	if (!sysInfo.m_bWin4) {
		if (m_bPreCreated) {
			cs.lpszClass = AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW, 
											   ::LoadCursor(NULL, IDC_ARROW),
											   (HBRUSH)(COLOR_WINDOW+1),
											   NULL);
			cs.hInstance = AfxGetInstanceHandle();
		}
	}

	return res;
}

BOOL CMailNewsFrame::PreTranslateMessage( MSG *pMsg )
{
	if ( pMsg->message == WM_KEYDOWN) {
		CWinCX*	pContext = GetActiveWinContext();

		// Determine if keystroke is destined for a contained window
		HWND hwnd = pMsg->hwnd;
		LONG lStyle = GetWindowLong(hwnd, GWL_STYLE);
		while ((lStyle & WS_CHILD) && (hwnd = ::GetParent(hwnd))) {
			lStyle = GetWindowLong(hwnd, GWL_STYLE);
		}

		if (hwnd == m_hWnd && pContext) {
			HWND hwndFocus = ::GetFocus();
			HWND hwndView = pContext->GetPane();

			if ( !::IsChild( hwndView, hwndFocus ) ) {
				// Give accelerators first shot
				if ( TranslateAccelerator( m_hWnd, m_hAccel, pMsg ) ) {
					return TRUE;
				}
			}
		}
	}

	return CGenericFrame::PreTranslateMessage( pMsg );
}

void CMailNewsFrame::GetMessageString( UINT nID, CString& rMessage ) const
{
	MSG_FolderInfo *folderInfo = GetCurFolder();
	MSG_FolderLine folderLine;
	folderLine.flags = 0;
	if ( folderInfo ) {
		MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );
	}

	CString csFull = "";

	switch (nID) {
	case ID_EDIT_DELETEMESSAGE:
		if (m_bNews)
			csFull.LoadString( IDS_STATUS_CANCELMESSAGE );
		break;

	case ID_EDIT_DELETEFOLDER:
	case ID_POPUP_DELETEFOLDER:
		if ( folderLine.flags & MSG_FOLDER_FLAG_MAIL ) {
			csFull.LoadString( ID_EDIT_DELETEFOLDER );
		} else if ( folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST ) {
			rMessage.LoadString( IDS_STATUS_DELETENEWSHOST );
		} else if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ) {
			rMessage.LoadString( IDS_STATUS_DELETENEWSGROUP );
		} else {
			rMessage.LoadString( IDS_STATUS_DELETESELECTION );
		}
		break;

	case ID_FILE_GETNEXT:
		{
			int32 iDownloadMax = 0;
			PREF_GetIntPref("news.max_articles", &iDownloadMax);
			csFull.Format(szLoadString(ID_FILE_GETNEXT), (int) iDownloadMax );
		}
		break;

	case ID_FILE_NEWFOLDER:
		if (m_bNews) {
			if (m_bCategory)
				csFull.LoadString(ID_FILE_NEWCATEGORY);
			else
				csFull.LoadString(ID_FILE_NEWNEWSGROUP);
		} else {
			csFull.LoadString(ID_FILE_NEWFOLDER);
		}
		break;

	default:
		break;
	}

	if (nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID)
		csFull.LoadString(IDS_STATUS_MOVEMESSAGE);
	else if (nID >= FIRST_COPY_MENU_ID && nID <= LAST_COPY_MENU_ID)
		csFull.LoadString(IDS_STATUS_COPYMESSAGE);

	if (!csFull.IsEmpty()) {
		AfxExtractSubString( rMessage, csFull, 0 );
	} else {
		CGenericFrame::GetMessageString( nID, rMessage );
	}
}

BEGIN_MESSAGE_MAP(CMailNewsFrame, CGenericFrame)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
	ON_WM_INITMENUPOPUP()
	ON_MESSAGE(NSBUTTONMENUOPEN, OnButtonMenuOpen)

	// File Menu Commands
	ON_COMMAND(ID_FILE_NEWMESSAGE, OnNew)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWMESSAGE, OnUpdateNew)
	ON_COMMAND(ID_FILE_NEWFOLDER, OnNewFolder)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWFOLDER, OnUpdateNewFolder)
	ON_COMMAND(ID_FILE_NEWNEWSGROUP, OnNewNewsgroup)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWNEWSGROUP, OnUpdateNewNewsgroup)
	ON_COMMAND(ID_FILE_NEWNEWSHOST, OnNewNewshost)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWNEWSHOST, OnUpdateNewNewshost)
	ON_COMMAND(ID_FILE_NEWCATEGORY, OnNewCategory)	   
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWCATEGORY, OnUpdateNewCategory)
    ON_COMMAND(ID_FILE_SAVEMESSAGES, OnSave)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVEMESSAGES, OnUpdateSave)
    ON_COMMAND(ID_FILE_EDITMESSAGE, OnEditMessage)
    ON_UPDATE_COMMAND_UI(ID_FILE_EDITMESSAGE, OnUpdateEditMessage)
    ON_COMMAND(ID_FILE_ADDNEWSGROUP, OnAddNewsGroup)
    ON_UPDATE_COMMAND_UI(ID_FILE_ADDNEWSGROUP, OnUpdateAddNewsGroup)
    ON_COMMAND(ID_FILE_SUBSCRIBE, OnSubscribe)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBSCRIBE, OnUpdateSubscribe)
    ON_COMMAND(ID_FILE_UNSUBSCRIBE, OnUnsubscribe)
    ON_UPDATE_COMMAND_UI(ID_FILE_UNSUBSCRIBE, OnUpdateUnsubscribe)
#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE( FIRST_ATTACH_MENU_ID, LAST_ATTACH_MENU_ID, OnOpenAttach )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_ATTACH_MENU_ID, LAST_ATTACH_MENU_ID, OnUpdateOpenAttach )
	ON_COMMAND_RANGE( FIRST_SAVEATTACH_MENU_ID, LAST_SAVEATTACH_MENU_ID, OnSaveAttach )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_SAVEATTACH_MENU_ID, LAST_SAVEATTACH_MENU_ID, OnUpdateSaveAttach )
	ON_COMMAND_RANGE( FIRST_ATTACHPROP_MENU_ID, LAST_ATTACHPROP_MENU_ID, OnAttachProperties )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_ATTACHPROP_MENU_ID, LAST_ATTACHPROP_MENU_ID, OnUpdateAttachProperties )
#endif

	// Edit Menu Commands
    ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
    ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_COMMAND(ID_EDIT_DELETEFOLDER, OnDeleteFolder)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETEFOLDER, OnUpdateDeleteFolder)
	ON_COMMAND(ID_POPUP_DELETEFOLDER, OnDeleteFolder)
	ON_UPDATE_COMMAND_UI(ID_POPUP_DELETEFOLDER, OnUpdateDeleteFolder)
	ON_COMMAND(ID_EDIT_DELETEMESSAGE, OnDeleteMessage)
	ON_COMMAND(ID_EDIT_REALLYDELETEMESSAGE, OnReallyDeleteMessage)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETEMESSAGE, OnUpdateDeleteMessage)
	ON_COMMAND(ID_EDIT_SELECTALLMESSAGES, OnSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALLMESSAGES, OnUpdateSelectAll)
	ON_COMMAND(ID_EDIT_SELECTALL, OnSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALL, OnUpdateSelectAll)
	ON_COMMAND(ID_EDIT_SEARCH, OnSearch)
	ON_COMMAND(ID_EDIT_FILTER, OnFilter)
	ON_COMMAND(ID_EDIT_MANAGEMAILACCOUNT, OnSetupWizard)
	ON_UPDATE_COMMAND_UI(ID_EDIT_MANAGEMAILACCOUNT, OnUpdateSetupWizard)
	ON_COMMAND(ID_EDIT_SERVER, OnServerStuff)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SERVER, OnUpdateServerStuff)
	ON_COMMAND(ID_VIEW_PROPERTIES, OnEditProperties)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PROPERTIES, OnUpdateProperties)

	// View Menu Items
    ON_COMMAND(ID_MAIL_SORTAGAIN, OnSortAgain)
    ON_UPDATE_COMMAND_UI(ID_MAIL_SORTAGAIN, OnUpdateSortAgain)
	ON_COMMAND(ID_SORT_ASCENDING, OnAscending)
	ON_UPDATE_COMMAND_UI(ID_SORT_ASCENDING, OnUpdateAscending)
	ON_COMMAND(ID_SORT_DESCENDING, OnDescending)
	ON_UPDATE_COMMAND_UI(ID_SORT_DESCENDING, OnUpdateDescending)
    ON_COMMAND(ID_SORT_BYDATE, OnSortDate)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYDATE, OnUpdateSortDate)
    ON_COMMAND(ID_SORT_BYSUBJECT, OnSortSubject)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYSUBJECT, OnUpdateSortSubject)
    ON_COMMAND(ID_SORT_BYSENDER, OnSortSender)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYSENDER, OnUpdateSortSender)
    ON_COMMAND(ID_SORT_BYMESSAGENUMBER, OnSortNumber)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYMESSAGENUMBER, OnUpdateSortNumber)
	ON_COMMAND(ID_SORT_BYTHREAD, OnThread)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYTHREAD, OnUpdateThread)
	ON_COMMAND(ID_SORT_BYPRIORITY, OnSortPriority)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYPRIORITY, OnUpdateSortPriority)
	ON_COMMAND(ID_SORT_BYSTATUS, OnSortStatus)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYSTATUS, OnUpdateSortStatus)
	ON_COMMAND(ID_SORT_BYSIZE, OnSortSize)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYSIZE, OnUpdateSortSize)
	ON_COMMAND(ID_SORT_BYFLAG, OnSortFlag)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYFLAG, OnUpdateSortFlag)
	ON_COMMAND(ID_SORT_BYUNREAD, OnSortUnread)
    ON_UPDATE_COMMAND_UI(ID_SORT_BYUNREAD, OnUpdateSortUnread)
	ON_COMMAND(ID_VIEW_ALLTHREADS, OnViewAll)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ALLTHREADS, OnUpdateViewAll)
	ON_COMMAND(ID_VIEW_KILLEDTHREADS, OnViewKilled)
	ON_UPDATE_COMMAND_UI(ID_VIEW_KILLEDTHREADS, OnUpdateViewKilled)
	ON_COMMAND(ID_VIEW_WATCHEDTHREADS, OnViewWatched)
	ON_UPDATE_COMMAND_UI(ID_VIEW_WATCHEDTHREADS, OnUpdateViewWatched)
	ON_COMMAND(ID_VIEW_NEWTHREADS, OnViewNew)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NEWTHREADS, OnUpdateViewNew)
	ON_COMMAND(ID_VIEW_NEWONLY, OnViewNewOnly)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NEWONLY, OnUpdateViewNewOnly)
	ON_COMMAND(ID_VIEW_HEADERS_MICRO, OnHeadersMicro)
	ON_UPDATE_COMMAND_UI(ID_VIEW_HEADERS_MICRO, OnUpdateHeadersMicro)
	ON_COMMAND(ID_VIEW_HEADERS_SHORT, OnHeadersShort)
	ON_UPDATE_COMMAND_UI(ID_VIEW_HEADERS_SHORT, OnUpdateHeadersShort)
	ON_COMMAND(ID_VIEW_HEADERS_ALL, OnHeadersAll)
	ON_UPDATE_COMMAND_UI(ID_VIEW_HEADERS_ALL, OnUpdateHeadersAll)
    ON_COMMAND(ID_VIEW_ATTACHINLINE,OnViewInline)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ATTACHINLINE,OnUpdateViewInline)
    ON_COMMAND(ID_VIEW_ATTACHLINKS,OnViewAsLinks)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ATTACHLINKS,OnUpdateViewAsLinks)

	// Navigation Commands
	ON_COMMAND(ID_MESSAGE_NEXT, OnNext)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_NEXT, OnUpdateNext)
	ON_COMMAND(ID_MESSAGE_NEXTNEWSGROUP, OnNextFolder)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_NEXTNEWSGROUP, OnUpdateNextFolder)
	ON_COMMAND(ID_MESSAGE_NEXTUNREAD, OnNextUnread)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_NEXTUNREAD, OnUpdateNextUnread)
	ON_COMMAND(ID_MESSAGE_PREVIOUS, OnPrevious)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_PREVIOUS, OnUpdatePrevious)
	ON_COMMAND(ID_MESSAGE_PREVIOUSUNREAD, OnPreviousUnread)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_PREVIOUSUNREAD, OnUpdatePreviousUnread)
	ON_COMMAND(ID_MESSAGE_FIRSTUNREAD, OnFirstUnread)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_FIRSTUNREAD, OnUpdateFirstUnread)
	ON_COMMAND(ID_MESSAGE_NEXTUNREADTHREAD, OnNextUnreadThread)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_NEXTUNREADTHREAD, OnUpdateNextUnreadThread)
	ON_COMMAND(ID_MESSAGE_NEXTUNREADNEWSGROUP, OnNextUnreadFolder)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_NEXTUNREADNEWSGROUP, OnUpdateNextUnreadFolder)
	ON_COMMAND(ID_MESSAGE_PREVIOUSFLAGGED, OnPreviousFlagged)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_PREVIOUSFLAGGED, OnUpdatePreviousFlagged)
	ON_COMMAND(ID_MESSAGE_NEXTFLAGGED, OnNextFlagged)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_NEXTFLAGGED, OnUpdateNextFlagged)
	ON_COMMAND(ID_MESSAGE_FIRSTFLAGGED, OnFirstFlagged)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_FIRSTFLAGGED, OnUpdateFirstFlagged)

	/* hack for message backtracking */
	ON_COMMAND(ID_NAVIGATE_MSG_BACK, OnGoBack)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_MSG_BACK, OnUpdateGoBack)
	ON_COMMAND(ID_NAVIGATE_MSG_FORWARD, OnGoForward)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_MSG_FORWARD, OnUpdateGoForward)

	// Message menu items
 	ON_COMMAND(ID_FILE_GETNEWMAIL, OnGetMail)
 	ON_UPDATE_COMMAND_UI(ID_FILE_GETNEWMAIL, OnUpdateGetMail)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_REPLYBUTTON, OnUpdateReply)
	ON_COMMAND(ID_MESSAGE_REPLYBUTTON, OnReplyButton)
 	ON_COMMAND(ID_FILE_GETMOREMESSAGES, OnGetMail)
 	ON_UPDATE_COMMAND_UI(ID_FILE_GETMOREMESSAGES, OnUpdateGetMail)
	ON_COMMAND(ID_FILE_GETNEXT, OnGetNext)
	ON_UPDATE_COMMAND_UI(ID_FILE_GETNEXT, OnUpdateGetNext)
    ON_COMMAND(ID_FILE_DELIVERMAILNOW,OnDeliverNow)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_SENDUNSENT,OnUpdateDeliverNow)
	ON_COMMAND(ID_MESSAGE_SENDUNSENT, OnDeliverNow)
    ON_UPDATE_COMMAND_UI(ID_FILE_DELIVERMAILNOW,OnUpdateDeliverNow)
	ON_COMMAND(ID_NEWS_POSTNEW, OnPostNew)
	ON_UPDATE_COMMAND_UI(ID_NEWS_POSTNEW, OnUpdatePostNew)
    ON_COMMAND(ID_MESSAGE_REPLY, OnReply)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_REPLY, OnUpdateReply)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_MARKBUTTON, OnUpdateMarkAllRead )
    ON_COMMAND(ID_MESSAGE_REPLYALL, OnReplyAll)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_REPLYALL, OnUpdateReplyAll)
	ON_COMMAND(ID_MESSAGE_FORWARDBUTTON, OnForwardButton)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_FORWARDBUTTON, OnUpdateForwardButton)
    ON_COMMAND(ID_MESSAGE_FORWARD, OnForward)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_FORWARD, OnUpdateForward)
    ON_COMMAND(ID_MESSAGE_FORWARDQUOTED, OnForwardQuoted)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_FORWARDQUOTED, OnUpdateForwardQuoted)
	ON_COMMAND(ID_MESSAGE_FORWARDINLINE, OnForwardInline)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_FORWARDINLINE, OnUpdateForwardInline)
	ON_COMMAND(ID_MESSAGE_POSTREPLY, OnPostReply)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_POSTREPLY, OnUpdatePostReply)
	ON_COMMAND(ID_MESSAGE_POSTANDREPLY, OnPostAndMailReply)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_POSTANDREPLY, OnUpdatePostAndMailReply)
	ON_COMMAND(ID_MESSAGE_KILL, OnKill )
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_KILL, OnUpdateKill )
	ON_COMMAND(ID_MESSAGE_RETRIEVESELECTED, OnRetrieveSelected)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_RETRIEVESELECTED, OnUpdateRetrieveSelected)
	ON_COMMAND(ID_MESSAGE_WATCH, OnWatch )
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_WATCH, OnUpdateWatch )
	ON_COMMAND(ID_MESSAGE_MARKASREAD, OnMarkMessagesRead)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_MARKASREAD, OnUpdateMarkMessagesRead)
	ON_COMMAND(ID_MESSAGE_MARKASUNREAD, OnMarkMessagesUnread)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_MARKASUNREAD, OnUpdateMarkMessagesUnread)
	ON_COMMAND(ID_MESSAGE_MARKTHREADREAD, OnMarkThreadRead )
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_MARKTHREADREAD, OnUpdateMarkThreadRead )
	ON_COMMAND(ID_MESSAGE_MARKALLREAD, OnMarkAllRead )
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_MARKALLREAD, OnUpdateMarkAllRead )
	ON_COMMAND(ID_MESSAGE_CATCHUP, OnCatchup)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_CATCHUP, OnUpdateCatchup)
	ON_COMMAND(ID_MESSAGE_MARKASLATER, OnMarkMessagesLater)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_MARKASLATER, OnUpdateMarkMessagesLater)
	ON_COMMAND(ID_MESSAGE_RETRIEVEOFFLINE, OnRetrieveOffline)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_RETRIEVEOFFLINE, OnUpdateRetrieveOffline)
	ON_COMMAND(ID_MESSAGE_MARK, OnMarkMessages )
	ON_UPDATE_COMMAND_UI( ID_MESSAGE_MARK, OnUpdateMarkMessages )
	ON_COMMAND(ID_MESSAGE_UNMARK, OnUnmarkMessages )
	ON_UPDATE_COMMAND_UI( ID_MESSAGE_UNMARK, OnUpdateUnmarkMessages )
	ON_COMMAND(ID_EDIT_CANCELMESSAGE, OnCancelMessage)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CANCELMESSAGE, OnUpdateCancelMessage)
	ON_COMMAND(ID_NAVIGATE_INTERRUPT, OnInterrupt)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_INTERRUPT, OnUpdateInterrupt)
	ON_COMMAND(ID_MAIL_WRAPLONGLINES,OnWrapLongLines)
	ON_UPDATE_COMMAND_UI(ID_MAIL_WRAPLONGLINES,OnUpdateWrapLongLines)
	ON_COMMAND(ID_VIEW_ROT13,OnViewRot13)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ROT13,OnUpdateViewRot13)

	// Organize Menu
	ON_COMMAND(ID_FILE_COMPRESSTHISFOLDER, OnCompressFolder)
	ON_UPDATE_COMMAND_UI(ID_FILE_COMPRESSTHISFOLDER, OnUpdateCompressFolder)
	ON_COMMAND(ID_FOLDER_RENAME, OnRenameFolder)
	ON_UPDATE_COMMAND_UI(ID_FOLDER_RENAME, OnUpdateRenameFolder)
	ON_COMMAND(ID_FILE_COMPRESSALLFOLDERS, OnCompressAll)
	ON_UPDATE_COMMAND_UI(ID_FILE_COMPRESSALLFOLDERS, OnUpdateCompressAll)
	ON_COMMAND(ID_FILE_EMPTYTRASHFOLDER, OnEmptyTrash)
	ON_UPDATE_COMMAND_UI(ID_FILE_EMPTYTRASHFOLDER, OnUpdateEmptyTrash)
    ON_COMMAND(ID_MESSAGE_ADDSENDER, OnAddToAddressBook)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_ADDSENDER, OnUpdateAddToAddressBook)
    ON_COMMAND(ID_MESSAGE_ADDALL, OnAddAllToAddressBook)
    ON_UPDATE_COMMAND_UI(ID_MESSAGE_ADDALL, OnUpdateAddAllToAddressBook)
#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE(FIRST_ADDSENDER_MENU_ID, LAST_ADDSENDER_MENU_ID, OnAddSenderAddressBook )
	ON_COMMAND_RANGE(FIRST_ADDALL_MENU_ID, LAST_ADDALL_MENU_ID, OnAddAllAddressBook )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_ADDSENDER_MENU_ID, LAST_ADDSENDER_MENU_ID, OnUpdateAddSenderAddressBook )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_ADDALL_MENU_ID, LAST_ADDALL_MENU_ID, OnUpdateAddAllAddressBook )
#endif
	    // Encoding stuff                                                                                                                                           
    ON_COMMAND(ID_OPTIONS_ENCODING_1, OnToggleEncoding1)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_1, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_2, OnToggleEncoding2)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_2, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_3, OnToggleEncoding3)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_3, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_4, OnToggleEncoding4)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_4, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_5, OnToggleEncoding5)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_5, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_6, OnToggleEncoding6)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_6, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_7, OnToggleEncoding7)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_7, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_8, OnToggleEncoding8)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_8, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_9, OnToggleEncoding9)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_9, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_10, OnToggleEncoding10)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_10, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_11, OnToggleEncoding11)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_11, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_12, OnToggleEncoding12)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_12, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_13, OnToggleEncoding13)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_13, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_14, OnToggleEncoding14)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_14, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_15, OnToggleEncoding15)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_15, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_16, OnToggleEncoding16)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_16, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_17, OnToggleEncoding17)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_17, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_18, OnToggleEncoding18)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_18, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_19, OnToggleEncoding19)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_19, OnUpdateEncoding)
    ON_COMMAND(ID_OPTIONS_ENCODING_20, OnToggleEncoding20)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_ENCODING_20, OnUpdateEncoding)

	// Non-MSG Commands
	ON_COMMAND(ID_OPTIONS_TIPS, OnTips)
	ON_COMMAND(ID_MESSAGE_FILE, OnQuickFile)
	ON_UPDATE_COMMAND_UI(ID_MESSAGE_FILE, OnUpdateQuickFile)
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
	ON_UPDATE_COMMAND_UI(ID_SECURITY, OnUpdateSecurity)
	ON_UPDATE_COMMAND_UI(IDS_SECURITY_STATUS, OnUpdateSecureStatus)
	ON_UPDATE_COMMAND_UI(IDS_SIGNED_STATUS, OnUpdateSecureStatus)

END_MESSAGE_MAP()

BOOL CMailNewsFrame::OnCommand( WPARAM wParam, LPARAM lParam )
{
	UINT nID = wParam;

	if (nID == ID_FILE_SUBSCRIBE)
	{
		MSG_Host* pThisHost = (MSG_Host*)lParam;
		DoSubscribe(pThisHost);
		return TRUE;
	}

#ifndef ON_COMMAND_RANGE

	if ( nID >= FIRST_ATTACH_MENU_ID && nID <= LAST_ATTACH_MENU_ID ) {
		OnOpenAttach( nID );
		return TRUE;
	} else if (nID >= FIRST_SAVEATTACH_MENU_ID && nID <= LAST_SAVEATTACH_MENU_ID) {
		OnSaveAttach( nID );
		return TRUE;
	} else if (nID >= FIRST_ATTACHPROP_MENU_ID && nID <= LAST_ATTACHPROP_MENU_ID) {
		OnAttachProperties( nID );
		return TRUE;
	}
	else if(nID >= FIRST_ADDSENDER_MENU_ID && nID <= LAST_ADDSENDER_MENU_ID)
	{
		OnAddSenderAddressBook(nID);
		return TRUE;
	}
	else if(nID >= FIRST_ADDALL_MENU_ID && nID <= LAST_ADDALL_MENU_ID)
	{
		OnAddAllAddressBook(nID);
		return TRUE;
	}

#endif

	return CGenericFrame::OnCommand( wParam, lParam );
}

#ifndef ON_COMMAND_RANGE

BOOL CMailNewsFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{ 
	if (nCode == CN_UPDATE_COMMAND_UI) {
		CCmdUI* pCmdUI = (CCmdUI*)pExtra;
		if ( nID >= FIRST_ATTACH_MENU_ID && nID <= LAST_ATTACH_MENU_ID ) {
			OnUpdateOpenAttach( pCmdUI );
			return TRUE;
		} else if (nID >= FIRST_SAVEATTACH_MENU_ID && nID <= LAST_SAVEATTACH_MENU_ID) {
			OnUpdateSaveAttach( pCmdUI );
			return TRUE;
		} else if (nID >= FIRST_ATTACHPROP_MENU_ID && nID <= LAST_ATTACHPROP_MENU_ID) {
			OnUpdateAttachProperties( pCmdUI );
			return TRUE;
		}
		else if(nID >= FIRST_ADDSENDER_MENU_ID && nID <= LAST_ADDSENDER_MENU_ID)
		{
			OnUpdateAddSenderAddressBook(pCmdUI);
			return TRUE;
		}
		else if(nID >= FIRST_ADDALL_MENU_ID && nID <= LAST_ADDALL_MENU_ID)
		{
			OnUpdateAddAllAddressBook(pCmdUI);
			return TRUE;
		}

	}
	return CGenericFrame::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

#endif


int CMailNewsFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int res = CGenericFrame::OnCreate(lpCreateStruct);

	iMailFrameCount++;

	return res;
}

static void _ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure)
{
	if (::IsWindow(hwnd)) {
		::ShowWindow(hwnd,SW_SHOW);
		::UpdateWindow(hwnd);
	}

	if (pane)
		MSG_CleanupFolders(pane);
}

void CMailNewsFrame::OnPaint()
{
	// To switch icons on < version 4, we have to override paint
	// when the window is iconic
	if (!sysInfo.m_bWin4 && IsIconic()) {
		CPaintDC dc(this);
		UINT nIDIcon = m_bNews ? IDR_NEWSTHREAD : IDR_MAILTHREAD;
		if (IsKindOf(RUNTIME_CLASS(CFolderFrame)))
			nIDIcon = IDR_MAILFRAME;
		HICON hLargeIcon = theApp.LoadIcon(nIDIcon);

		dc.DrawIcon(0, 0, hLargeIcon);
	} else {
		CGenericFrame::OnPaint();
	}
}

HCURSOR CMailNewsFrame::OnQueryDragIcon()
{
	UINT nIDIcon = m_bNews ? IDR_NEWSTHREAD : IDR_MAILTHREAD;
	if (IsKindOf(RUNTIME_CLASS(CFolderFrame)))
		nIDIcon = IDR_MAILFRAME;

	return (HCURSOR) theApp.LoadIcon(nIDIcon);
}

//this is a static function
BOOL CMailNewsFrame::CleanupFolders(MSG_Pane *pPane)
{
	XP_Bool bSelectable = FALSE;
	XP_Bool bPlural = FALSE;
    MSG_COMMAND_CHECK_STATE sState = MSG_Unchecked;

	ASSERT(pPane);
	MSG_CommandStatus(pPane, MSG_DeliverQueuedMessages, 0, NULL,
					  &bSelectable, &sState, NULL, &bPlural);

	
	if (bSelectable && !NET_IsOffline()) {
		CString strTitle;
		VERIFY(strTitle.LoadString(IDS_NETSCAPE_MAIL_TITLE));

		if (::MessageBox(NULL, szLoadString(IDS_UNSENT_MESSAGES), strTitle, MB_YESNO) == IDYES) {
			MSG_Command(pPane, MSG_DeliverQueuedMessages, 0, NULL );
			return FALSE;
		}
	}
	if (MSG_CleanupNeeded(WFE_MSGGetMaster()))
	  new CProgressDialog(NULL, pPane,_ShutDownFrameCallBack,
        NULL,szLoadString(IDS_COMPRESSINGFOLDERS));

	return TRUE;
}

void CMailNewsFrame::OnClose()  
{

	if (1 == iMailFrameCount)	// this seems to be null when you close a message window dmb
	{
		if(!CleanupFolders(m_pPane))
		{
			return;
		}
	}

    CGenericFrame::OnClose();
}

void CMailNewsFrame::OnDestroy()
{
	iMailFrameCount--;

	MSG_DestroyPane(m_pPane);
	m_pPane = NULL;

	CGenericFrame::OnDestroy();
}

void CMailNewsFrame::OnInitMenuPopup( CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu )
{
	AfxLockTempMaps();
	CGenericFrame::OnInitMenuPopup( pPopupMenu, nIndex, bSysMenu );

	HMENU hmenu = ::GetMenu(m_hWnd);
	if ( m_iMessageMenuPos >= 0 && 
		 pPopupMenu->m_hMenu == GetSubMenu(hmenu, m_iMessageMenuPos )) { 
		UpdateFileMenus();
#ifdef MOZ_NEWADDR
		UpdateAddressBookMenus();
#endif
	} else if (m_iFileMenuPos >= 0 &&
			   pPopupMenu->m_hMenu == GetSubMenu(hmenu, m_iFileMenuPos )) {
		UpdateAttachmentMenus();
	}
	AfxUnlockTempMaps();
}

MSG_FolderInfo *CMailNewsFrame::FolderInfoFromMenuID( MSG_FolderInfo *mailRoot, 
													  UINT &nBase, UINT nID )
{
	int i, iCount;
	MSG_FolderInfo **folderInfos;
	MSG_FolderInfo *res = NULL;
	MSG_FolderLine folderLine;

	if (mailRoot == NULL ) {
		// Loop through top level folderInfos, looking for mail trees.

		iCount = MSG_GetFolderChildren(m_pMaster, NULL, NULL, NULL);
		folderInfos = new MSG_FolderInfo*[iCount];
		if (folderInfos) {
			MSG_GetFolderChildren(m_pMaster, NULL, folderInfos, iCount);

			for (i = 0; i < iCount && !res; i++) {
				if (MSG_GetFolderLineById(m_pMaster, folderInfos[i], &folderLine)) {
					if (folderLine.flags & MSG_FOLDER_FLAG_MAIL) {
						res = FolderInfoFromMenuID( folderInfos[i], nBase, nID);
					}
				}
			}
			delete [] folderInfos;
		}
		return res;
	}

	MSG_GetFolderLineById( m_pMaster, mailRoot, &folderLine );
	if (folderLine.level > 1) { // We've a subfolder
		if ( nID == nBase ) {
			return mailRoot;
		}
		nBase++;
	}

	iCount = MSG_GetFolderChildren( m_pMaster, mailRoot, NULL, NULL );

	folderInfos = new MSG_FolderInfo*[iCount];
	ASSERT( folderInfos );
	if (folderInfos) {
		MSG_GetFolderChildren( m_pMaster, mailRoot, folderInfos, iCount );

		for ( i = 0; i < iCount && !res; i++ ) {
			if ( MSG_GetFolderLineById( m_pMaster, folderInfos[ i ], &folderLine ) ) {
				if ( folderLine.numChildren > 0 ) {
					res = FolderInfoFromMenuID( folderInfos[ i ], nBase, nID );
				} else {
					if ( nID == nBase ) {
						res = folderInfos[ i ];
					} else {
						nBase++;
					}
				}
			}
		}
		delete [] folderInfos;
	}

	return res;
}

MSG_FolderInfo *CMailNewsFrame::FolderInfoFromMenuID( UINT nID )
{
	UINT nBase = 0;
	if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
		nBase = FIRST_MOVE_MENU_ID;
	} else 	if ( nID >= FIRST_COPY_MENU_ID && nID <= LAST_COPY_MENU_ID ) {
		nBase = FIRST_COPY_MENU_ID;
	} else {
		ASSERT(0);
		return NULL;
	}
	return FolderInfoFromMenuID( NULL, nBase, nID );
}

// Update the menu list for the copy and move submenus
//

void CMailNewsFrame::UpdateMenu( MSG_FolderInfo *mailRoot,
								 HMENU hMenu, UINT &nID, int nStart)
{
	// *** If you change the way this menu is built, you also need to
	// change CMailNewsFrame::FolderInfoFromMenuID and make it the same
	// as CMailQFButton::BuildMenu ***

	MSG_Master *pMaster = WFE_MSGGetMaster();
	int i, j;
	MSG_FolderInfo **folderInfos;
	int32 iCount;
	MSG_FolderLine folderLine;
	char buffer[_MAX_PATH];

	j = nStart;

	if (mailRoot == NULL) {
		// Get rid of old top level menu (deletes submenus, too)

		int iMenuCount = ::GetMenuItemCount( hMenu );
		for( i = 0; i < iMenuCount; i++)
			::DeleteMenu( hMenu, 0, MF_BYPOSITION );

		// Loop through top level folderInfos, looking for mail trees.

		iCount = MSG_GetFolderChildren(pMaster, NULL, NULL, NULL);
		folderInfos = new MSG_FolderInfo*[iCount];
		if (folderInfos) {
			int nLevelTotal = 0;
			BOOL bAddSep = FALSE;
			MSG_GetFolderChildren(pMaster, NULL, folderInfos, iCount);

			for (i = 0; i < iCount; i++) {
				if (MSG_GetFolderLineById(pMaster, folderInfos[i], &folderLine)) {
					if (folderLine.flags & MSG_FOLDER_FLAG_MAIL) {
						int nChildTotal = MSG_GetFolderChildren( pMaster, folderInfos[i],
									 NULL, NULL);
						if (bAddSep)  
							::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
 						if (nChildTotal)
							UpdateMenu(folderInfos[i], hMenu, nID, nLevelTotal);
						nLevelTotal += nChildTotal;
						bAddSep = TRUE;
					}
				}
			}
			delete [] folderInfos;
		}
		return;
	}

	MSG_GetFolderLineById( pMaster, mailRoot, &folderLine );

	if ( folderLine.level > 1 ) { // We're a child folder
		sprintf( buffer, "&%d %s", j, szLoadString(IDS_MAIL_THISFOLDER));
		::AppendMenu( hMenu, MF_STRING, nID, buffer);
		::AppendMenu( hMenu, MF_SEPARATOR, NULL, NULL);
		nID++;
		j++;
	}

	iCount = MSG_GetFolderChildren( pMaster, mailRoot, NULL, NULL );

	folderInfos = new MSG_FolderInfo*[iCount];
	if (folderInfos) {
		MSG_GetFolderChildren( pMaster, mailRoot, folderInfos, iCount );

		for ( i = 0; i < iCount; i++) {
			if ( MSG_GetFolderLineById( pMaster, folderInfos[ i ], &folderLine ) ) {
				if (j < 10) {
					sprintf( buffer, "&%d %s", j, folderLine.name);
				} else if ( j < 36 ) {
					sprintf( buffer, "&%c %s", 'a' + j - 10, folderLine.name);
				} else {
					sprintf( buffer, "  %s", folderLine.name);
				}

				if ( folderLine.numChildren > 0 ) {
					HMENU hNewMenu = ::CreatePopupMenu();

					UpdateMenu( folderInfos[ i ], hNewMenu, nID);

					::AppendMenu( hMenu, MF_STRING|MF_POPUP, (UINT) hNewMenu, buffer);
				} else {
					::AppendMenu( hMenu, MF_STRING, nID, buffer);
					nID++;
				}
				j++;
			}
		}
		delete [] folderInfos;
	}
}

void CMailNewsFrame::UpdateFileMenus()
{
	HMENU hFrameMenu = ::GetMenu( m_hWnd );

	ASSERT( hFrameMenu );

    if( hFrameMenu ) {
		HMENU hMessage = ::GetSubMenu( hFrameMenu, m_iMessageMenuPos );

		if ( hMessage ) {
			UINT idMove = FIRST_MOVE_MENU_ID;
			UINT idCopy = FIRST_COPY_MENU_ID;

			HMENU hMove = ::GetSubMenu( hMessage, m_iMoveMenuPos );
			HMENU hCopy = NULL;
			if ( m_iCopyMenuPos > -1 )
				hCopy = ::GetSubMenu( hMessage, m_iCopyMenuPos );
		
			// we don't alway have a copy menu
			if( hMove ) {
				UpdateMenu( NULL, hMove, idMove ); 
			}
			if (hCopy) {
				UpdateMenu( NULL, hCopy, idCopy );
			}
		}
	}
    DrawMenuBar();
}

void CMailNewsFrame::UpdateAddressBookMenus()
{
#ifdef MOZ_NEWADDR
	if(m_iAddAddressBookMenuPos == -1)
		return;

	HMENU hFrameMenu = ::GetMenu( m_hWnd );

	ASSERT( hFrameMenu );

	if( hFrameMenu )
	{
		HMENU hMessage = ::GetSubMenu( hFrameMenu, m_iMessageMenuPos );

		if(hMessage)
		{

			WFE_MSGBuildAddAddressBookPopups(hMessage, m_iAddAddressBookMenuPos,
											 FALSE, GetContext()->GetContext());
		}
	}
    DrawMenuBar();
#endif
}




void CMailNewsFrame::UpdateAttachmentMenus()
{
	HMENU hFrameMenu = ::GetMenu( m_hWnd );

	ASSERT( hFrameMenu );

    if( hFrameMenu ) {
		HMENU hFile = ::GetSubMenu( hFrameMenu, m_iFileMenuPos );

		if ( hFile ) {
			int i;
			char buffer[_MAX_PATH];
			UINT idAttach = FIRST_ATTACH_MENU_ID;
			HMENU hAttach = ::GetSubMenu( hFile, m_iAttachMenuPos );
			
			int iCount = ::GetMenuItemCount( hAttach );
			for( i = 0; i < iCount; i++)
				::DeleteMenu( hAttach, 0, MF_BYPOSITION );

			if (m_pAttachmentData) {
				for ( i = 0; m_pAttachmentData[i].url; i++ ) {
					const char *name = m_pAttachmentData[i].real_name ?
									   m_pAttachmentData[i].real_name :
									   szLoadString(IDS_UNTITLED);
					if (i < 10) {
						sprintf( buffer, "&%d %s", i, name);
					} else {
						strcpy(buffer, name);
					}
					::AppendMenu( hAttach, MF_STRING, idAttach + i, buffer);
				}
			} else {
				::AppendMenu( hAttach, MF_STRING, idAttach, szLoadString(IDS_NOATTACHMENTS));
			}
		}
	}
}

void CMailNewsFrame::ModalStatusBegin( int iModalDelay )
{
	if ( iModalDelay > -1 ) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->ModalStatus( TRUE, iModalDelay );
			pIStatusBar->Release();
		}
	}
}

void CMailNewsFrame::ModalStatusEnd()
{
	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->ModalStatus( FALSE, 0 );
		pIStatusBar->Release();
	}
}

void CMailNewsFrame::DoUpdateCommand( CCmdUI* pCmdUI, MSG_CommandType cmd, BOOL bUseCheck )
{
    XP_Bool bSelectable = FALSE, bPlural = FALSE;
    MSG_COMMAND_CHECK_STATE sState;
	const char *display_string;

	CMailNewsOutliner* pOutliner;

	if ((cmd == MSG_DeleteFolder || cmd == MSG_MarkAllRead ||
		 cmd == MSG_DoRenameFolder) &&
		IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		pOutliner = ((C3PaneMailFrame*)this)->GetFolderOutliner();
		if (cmd == MSG_MarkAllRead)
		{
			MSG_FolderLine folderLine;
			if (((C3PaneMailFrame*)this)->GetSelectedFolder(&folderLine))
			{
				if ( !(folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP) ) 
					pOutliner = m_pOutliner;
			}
		}
	}
	else
		pOutliner = m_pOutliner;

	MSG_Pane *pPane = pOutliner ? pOutliner->GetPane(): m_pPane;
	//XP_ASSERT(pPane);
	if (pPane) {
		MSG_ViewIndex *indices = NULL;
		int count = 0;
		if (pOutliner)
			pOutliner->GetSelection(indices, count);

		MSG_CommandStatus(pPane,
						  cmd,
						  indices, count,
						  &bSelectable,
						  &sState,
						  &display_string,
						  &bPlural);
	}

    pCmdUI->Enable(bSelectable);
    if (bUseCheck)
        pCmdUI->SetCheck(sState == MSG_Checked);
    else
        pCmdUI->SetRadio(sState == MSG_Checked);
}

void CMailNewsFrame::DoCommand( MSG_CommandType msgCommand, int iModalDelay, BOOL bAsync )
{
#ifdef DEBUG_WHITEBOX
	QA_Trace("CMailNewsFrame:Do Command parameters = MSG_CommandType %d, iModalDelay %d, bAsync %d",
		(int) msgCommand, iModalDelay, (int) bAsync);
#endif

	ModalStatusBegin( iModalDelay );

	CMailNewsOutliner* pOutliner;
	if ((msgCommand == MSG_DeleteFolder || msgCommand == MSG_MarkAllRead ||
		 msgCommand == MSG_DoRenameFolder) &&
		IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
		pOutliner = ((C3PaneMailFrame*)this)->GetFolderOutliner();
	else
		pOutliner = m_pOutliner;

	MSG_ViewIndex *indices = NULL;
	int count = 0;
	if (pOutliner)
		pOutliner->GetSelection(indices, count);

#ifdef DEBUG_WHITEBOX
	QA_Trace("Return from GetSelection: count = %d",count);
	
	for (int i = 0; i < count; i++)
	{
		QA_Trace("indices[%d] = %d",i,*(indices+i));
	}
#endif

	if (msgCommand == MSG_PostNew || msgCommand == MSG_PostReply || msgCommand == MSG_PostAndMailReply ||
		msgCommand == MSG_MailNew || msgCommand == MSG_ReplyToSender || msgCommand == MSG_ReplyToAll)
		theApp.m_bReverseSenseOfHtmlCompose = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	MSG_Pane *pPane = pOutliner ? pOutliner->GetPane(): m_pPane;
	MSG_Command(pPane, msgCommand, indices, count);

	theApp.m_bReverseSenseOfHtmlCompose = FALSE;

	if (!bAsync)
		ModalStatusEnd();
}

void CMailNewsFrame::DoUpdateMessageCommand( CCmdUI* pCmdUI, MSG_CommandType tMenuType, BOOL bUseCheck )
{
    XP_Bool bSelectable = FALSE, bPlural = FALSE;
	const char *szTitle;
    MSG_COMMAND_CHECK_STATE sState = MSG_Unchecked;

	if ( m_pMessagePane ) {
		if (m_pOutliner) {
			MsgViewIndex *indices;
			int iCount;
			m_pOutliner->GetSelection( indices, iCount );
			MSG_CommandStatus(m_pMessagePane,
							  tMenuType,
							  indices, iCount,
							  &bSelectable,
							  &sState,
							  &szTitle,
							  &bPlural );
		} else {
			MSG_FolderInfo* folder;
			MessageKey key;
			MSG_ViewIndex viewIndex;

			MSG_GetCurMessage( m_pMessagePane,
							   &folder,
							   &key, &viewIndex );
			if ( key != MSG_MESSAGEKEYNONE ) {
				MSG_CommandStatus(m_pMessagePane,
								  tMenuType,
								  &viewIndex, 1,
								  &bSelectable,
								  &sState,
								  &szTitle,
								  &bPlural );
			}
		}
	}
    pCmdUI->Enable(bSelectable);
    if (bUseCheck)
        pCmdUI->SetCheck(sState == MSG_Checked);
    else
        pCmdUI->SetRadio(sState == MSG_Checked);
}

void CMailNewsFrame::DoMessageCommand(MSG_CommandType msgCommand )
{
	if ( m_pMessagePane ) {
		if (m_pOutliner) {
			MsgViewIndex *indices;
			int iCount;
			m_pOutliner->GetSelection( indices, iCount );
			MSG_Command(m_pMessagePane, msgCommand, indices, iCount);
		} else {
			MSG_FolderInfo* folder;
			MessageKey key;
			MSG_ViewIndex viewIndex;
			MSG_GetCurMessage( m_pMessagePane,
							   &folder,
							   &key,
							   &viewIndex);
			if ( key != MSG_MESSAGEKEYNONE ) {
				MSG_Command(m_pMessagePane, msgCommand, &viewIndex, 1);
			}
		}
	}
}

// Reply button drop down menu
const UINT idArrayReply[] = { ID_MESSAGE_REPLY,
							  ID_MESSAGE_REPLYALL,
							  ID_MESSAGE_POSTREPLY,
							  ID_MESSAGE_POSTANDREPLY };
const UINT strArrayReply[] = { IDS_MENU_REPLY,
							   IDS_MENU_REPLYALL,
							   IDS_MENU_POSTREPLY,
							   IDS_MENU_POSTMAILREPLY };

// Mark button drop down menu
const UINT idArrayMark[] = { ID_MESSAGE_MARKASUNREAD,
							 ID_MESSAGE_MARKASREAD,
							 ID_MESSAGE_MARKTHREADREAD,
							 ID_MESSAGE_MARKALLREAD,
							 ID_MESSAGE_CATCHUP,
							 ID_MESSAGE_MARKASLATER };

const UINT strArrayMark[] = { IDS_POPUP_MARKASUNREAD,
							  IDS_POPUP_MARKASREAD,
							  IDS_POPUP_MARKTHREADREAD,
							  IDS_POPUP_MARKALLREAD,
							  IDS_POPUP_CATCHUP,
							  IDS_POPUP_MARKFORLATER };

const UINT idArrayNext[] = { ID_MESSAGE_NEXT,
							 ID_MESSAGE_NEXTUNREAD,
							 ID_MESSAGE_NEXTFLAGGED,
							 ID_MESSAGE_NEXTUNREADTHREAD,
							 ID_MESSAGE_NEXTNEWSGROUP,
							 ID_MESSAGE_NEXTUNREADNEWSGROUP };

const UINT strArrayNext[] = { IDS_POPUP_NEXTMESSAGE,
							  IDS_POPUP_NEXTUNREADMSG,
							  IDS_POPUP_NEXTFLAGGEDMSG,
							  IDS_POPUP_NEXTUNREADTHREAD,
							  IDS_POPUP_NEXTFOLDER,
							  IDS_POPUP_NEXTUNREADFOLDER };

LRESULT CMailNewsFrame::OnButtonMenuOpen(WPARAM wParam, LPARAM lParam)
{

	HMENU hMenu = (HMENU) lParam;
	UINT nCommand = (UINT) LOWORD(wParam);

	const UINT *idArray = NULL;
	const UINT *strArray = NULL;
	int nSize = 0;

	if(nCommand == ID_MESSAGE_REPLYBUTTON) {
		idArray = idArrayReply;
		strArray = strArrayReply;
		if (m_bNews) {
			nSize = 4;
		} else {
			nSize = 2;
		}
	} else if (nCommand == ID_MESSAGE_MARKBUTTON ) {
		idArray = idArrayMark;
		strArray = strArrayMark;
		nSize = 6;
	} else if (nCommand == ID_MESSAGE_NEXTUNREAD ) {
		idArray = idArrayNext;
		strArray = strArrayNext;
		nSize = 6;
	}

	if ( nSize > 0 ) {
		CString str;

		for(int i = 0; i < nSize; i++)
		{
			str.LoadString(strArray[i]);

			AppendMenu(hMenu, MF_STRING, idArray[i], (const char*) str);
		}
	}
	return 1;
}

// File Menu Items

void CMailNewsFrame::OnAddNewsGroup ( )
{
    DoCommand(MSG_AddNewsGroup);
}

void CMailNewsFrame::OnUpdateAddNewsGroup(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_AddNewsGroup);
}

void CMailNewsFrame::DoSubscribe(MSG_Host* pThisHost)
{ 
	BeginWaitCursor();

	MWContext *pContext = GetMainContext() != NULL ? 
		GetMainContext()->GetContext() : NULL;

	// fix for bug#58453 Change Prefs news dir with no news files 
	// then Subscribe. Crashes.
	MSG_NewsHost* defhost = NULL;
	defhost = MSG_GetDefaultNewsHost(WFE_MSGGetMaster());

	if (defhost == NULL)
	{
		FE_Alert(pContext, XP_GetString(MK_NNTP_SERVER_NOT_CONFIGURED));
		return;
	}

	CString title;
	title.LoadString(IDS_SUBSCRIBE_GROUP);
    CSubscribePropertySheet SubscribeNewsgroup(this, pContext, title);
  
    if (pThisHost)
	{
		SubscribeNewsgroup.SetHost(pThisHost);
	}
	else
	{
		MSG_FolderInfo* pFolderInfo = GetCurFolder();
		if (pFolderInfo)
		{
			MSG_Host *pHost = MSG_GetHostForFolder(pFolderInfo);
			if (pHost)
				SubscribeNewsgroup.SetHost(pHost);
		}
	}

    SubscribeNewsgroup.DoModal();
	EndWaitCursor();
}

void CMailNewsFrame::OnSubscribe ( )
{ 
	DoSubscribe(NULL);
}

void CMailNewsFrame::OnUpdateSubscribe(CCmdUI* pCmdUI)
{
	pCmdUI->Enable (TRUE);
}

void CMailNewsFrame::OnUnsubscribe ( )
{
	DoCommand(MSG_Unsubscribe);
}

void CMailNewsFrame::OnUpdateUnsubscribe(CCmdUI* pCmdUI)
{
	DoUpdateCommand(pCmdUI, MSG_Unsubscribe);
}

void CMailNewsFrame::OnOpenAttach(UINT nID)
{
	int idx = nID - FIRST_ATTACH_MENU_ID;

	if (idx < m_nAttachmentCount)
		GetContext()->NormalGetUrl(m_pAttachmentData[idx].url);
}

void CMailNewsFrame::OnUpdateOpenAttach(CCmdUI *pCmdUI)
{
	int idx = pCmdUI->m_nID - FIRST_ATTACH_MENU_ID;

	if (pCmdUI->m_pSubMenu) {
	    pCmdUI->m_pMenu->EnableMenuItem(pCmdUI->m_nIndex,
										MF_BYPOSITION |
										((m_nAttachmentCount > 0) ? MF_ENABLED : MF_GRAYED));
	} else {
		pCmdUI->Enable(idx < m_nAttachmentCount);
	}
}

void CMailNewsFrame::OnSaveAttach(UINT nID)
{
	int idx = nID - FIRST_SAVEATTACH_MENU_ID;

	if (idx < m_nAttachmentCount)
		CSaveCX::SaveAnchorObject(m_pAttachmentData[idx].url, NULL);
}

void CMailNewsFrame::OnUpdateSaveAttach(CCmdUI *pCmdUI)
{
	int idx = pCmdUI->m_nID - FIRST_SAVEATTACH_MENU_ID;

	pCmdUI->Enable(idx < m_nAttachmentCount);
}

void CMailNewsFrame::OnAttachProperties(UINT nID)
{
	int idx = nID - FIRST_ATTACHPROP_MENU_ID;

	CAttachmentSheet sheet(this,
						   m_pAttachmentData[idx].real_name,
						   m_pAttachmentData[idx].real_type,
						   m_pAttachmentData[idx].description);

	sheet.DoModal();
}

void CMailNewsFrame::OnUpdateAttachProperties(CCmdUI *pCmdUI)
{
	int idx = pCmdUI->m_nID - FIRST_ATTACHPROP_MENU_ID;

	pCmdUI->Enable(idx < m_nAttachmentCount);
}


// Edit Menu Items

void CMailNewsFrame::OnEditUndo()
{
    DoCommand(MSG_Undo);
}

void CMailNewsFrame::OnUpdateEditUndo ( CCmdUI* pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_Undo);
}

void CMailNewsFrame::OnEditRedo()
{
    DoCommand(MSG_Redo);
}

void CMailNewsFrame::OnUpdateEditRedo ( CCmdUI* pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_Redo);
}

// hack for message backtracking

void CMailNewsFrame::OnGoBack()
{
    DoNavigate(MSG_Back);
}

void CMailNewsFrame::OnUpdateGoBack ( CCmdUI* pCmdUI )
{
	DoUpdateNavigate(pCmdUI, MSG_Back);
}

void CMailNewsFrame::OnGoForward()
{
    DoNavigate(MSG_Forward);
}

void CMailNewsFrame::OnUpdateGoForward ( CCmdUI* pCmdUI )
{
	DoUpdateNavigate(pCmdUI, MSG_Forward);
}

void CMailNewsFrame::OnDeleteFolder( )
{
	DoCommand(MSG_DeleteFolder);
}

void CMailNewsFrame::OnUpdateDeleteFolder(CCmdUI* pCmdUI)
{
	MSG_FolderInfo *folderInfo = GetCurFolder();

	if ( folderInfo ) {
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );

		if ( pCmdUI->m_nID == ID_EDIT_DELETEFOLDER || pCmdUI->m_nID == ID_EDIT_DELETE_3PANE) {
			if ( folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST ) {
				pCmdUI->SetText( szLoadString( IDS_MENU_DELETENEWSHOST ) );
			} else if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ) {
				pCmdUI->SetText( szLoadString( IDS_MENU_DELETENEWSGROUP ) );
			} else {
				// We shouldn't use the popup item, except that
				// we're don't want to add a string at this point
				pCmdUI->SetText( szLoadString( IDS_POPUP_DELETEFOLDER ) );
			}
		}
	} else {
		pCmdUI->SetText( szLoadString( IDS_MENU_DELETESELECTION ) );
	}
    DoUpdateCommand(pCmdUI, MSG_DeleteFolder);
}

void CMailNewsFrame::OnDeleteMessage( )
{
	if ( m_bNews ) {
		DoCommand(MSG_CancelMessage, MODAL_DELAY);
	} else {
#ifdef DEBUG_WHITEBOX
		QA_Trace("Entering CMailNewsFrame:DoCommand");
#endif

		DoCommand(MSG_DeleteMessage, MODAL_DELAY);

#ifdef DEBUG_WHITEBOX
		QA_Trace("Finished CMailNewsFrame:DoCommand");
#endif
	}
}

void CMailNewsFrame::OnReallyDeleteMessage( )
{
	if ( m_bNews ) {
		DoCommand(MSG_CancelMessage, MODAL_DELAY);
	} else {
		DoCommand(MSG_DeleteMessageNoTrash, MODAL_DELAY);
	}
}

void CMailNewsFrame::OnUpdateDeleteMessage(CCmdUI* pCmdUI)
{
	if ( m_bNews ) {
		pCmdUI->SetText( szLoadString( IDS_MENU_CANCELMESSAGE ) );
	    DoUpdateCommand(pCmdUI, MSG_CancelMessage);
	} else {
		pCmdUI->SetText( szLoadString( IDS_MENU_DELETEMESSAGE ) );
	    DoUpdateCommand(pCmdUI, MSG_DeleteMessage);
	}
}

void CMailNewsFrame::OnSelectAll()
{
	if (m_pOutliner ) {
		m_pOutliner->SelectRange( 0, -1, TRUE );
	}
}

void CMailNewsFrame::OnUpdateSelectAll ( CCmdUI* pCmdUI )
{
	if (m_pOutliner && m_pOutliner->GetTotalLines() > 0 ) {
		pCmdUI->Enable( TRUE );
	} else {
		pCmdUI->Enable( FALSE );
	}
}

void CMailNewsFrame::OnCancelMessage()
{
    DoCommand(MSG_CancelMessage);
}

void CMailNewsFrame::OnUpdateCancelMessage ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_CancelMessage);
}

// View Menu Items

void CMailNewsFrame::OnSortAgain ( )
{
    DoCommand(MSG_ReSort);
}

void CMailNewsFrame::OnUpdateSortAgain(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_ReSort);
}

void CMailNewsFrame::OnAscending( )
{
	if ( MSG_GetToggleStatus( m_pPane, MSG_SortBackward, NULL, 0 ) == MSG_Checked ) {
	    DoCommand(MSG_SortBackward);
		SetSort(-1);
	}
}

void CMailNewsFrame::OnUpdateAscending(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
#ifdef _WIN32
	pCmdUI->SetRadio(MSG_GetToggleStatus( m_pPane, MSG_SortBackward, NULL, 0  ) != MSG_Checked);
#else
	pCmdUI->SetCheck(MSG_GetToggleStatus( m_pPane, MSG_SortBackward, NULL, 0  ) != MSG_Checked);
#endif
}

void CMailNewsFrame::OnDescending( )
{
	if ( MSG_GetToggleStatus( m_pPane, MSG_SortBackward, NULL, 0  ) != MSG_Checked ) {
	    DoCommand(MSG_SortBackward);
		SetSort(-1);
	}
}

void CMailNewsFrame::OnUpdateDescending(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
#ifdef _WIN32
	pCmdUI->SetRadio(MSG_GetToggleStatus( m_pPane, MSG_SortBackward, NULL, 0  ) == MSG_Checked);
#else
	pCmdUI->SetCheck(MSG_GetToggleStatus( m_pPane, MSG_SortBackward, NULL, 0  ) == MSG_Checked);
#endif
}

void CMailNewsFrame::OnSortDate()
{
    DoCommand(MSG_SortByDate);
	SetSort(SORT_BYDATE);
}

void CMailNewsFrame::OnUpdateSortDate ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortByDate);
}

void CMailNewsFrame::OnSortNumber()
{
    DoCommand(MSG_SortByMessageNumber);
	SetSort(SORT_BYNUMBER);
}

void CMailNewsFrame::OnUpdateSortNumber ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortByMessageNumber);
}

void CMailNewsFrame::OnSortSubject()
{
    DoCommand(MSG_SortBySubject);
	SetSort(SORT_BYSUBJECT);
}

void CMailNewsFrame::OnUpdateSortSubject ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortBySubject);
}

void CMailNewsFrame::OnSortSender()
{
    DoCommand(MSG_SortBySender);
	SetSort(SORT_BYSENDER);
}

void CMailNewsFrame::OnUpdateSortSender ( CCmdUI* pCmdUI )
{
	if (MSG_DisplayingRecipients(m_pPane)) {
		CString cs;
		cs.LoadString(IDS_MAIL_BYRECIPIENT);
		pCmdUI->SetText(cs);
	}
    DoUpdateCommand(pCmdUI, MSG_SortBySender);
}

void CMailNewsFrame::OnSortPriority()
{
    DoCommand(MSG_SortByPriority);
	SetSort(SORT_BYPRIORITY);
}

void CMailNewsFrame::OnUpdateSortPriority( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortByPriority);
}

void CMailNewsFrame::OnSortStatus()
{
    DoCommand(MSG_SortByStatus);
	SetSort(SORT_BYSTATUS);
}

void CMailNewsFrame::OnUpdateSortStatus( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortByStatus);
}

void CMailNewsFrame::OnSortSize()
{
    DoCommand(MSG_SortBySize);
	SetSort(SORT_BYSIZE);
}

void CMailNewsFrame::OnUpdateSortSize( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortBySize);
}

void CMailNewsFrame::OnSortFlag()
{
    DoCommand(MSG_SortByFlagged);
	SetSort(SORT_BYFLAG);
}

void CMailNewsFrame::OnUpdateSortFlag( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortByFlagged);
}

void CMailNewsFrame::OnSortUnread()
{
    DoCommand(MSG_SortByUnread);
	SetSort(SORT_BYUNREAD);
}

void CMailNewsFrame::OnUpdateSortUnread( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_SortByUnread);
}

void CMailNewsFrame::OnThread ( )
{
    DoCommand(MSG_SortByThread);
	SetSort(SORT_BYTHREAD);
}

void CMailNewsFrame::OnUpdateThread(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_SortByThread);
}

void CMailNewsFrame::OnViewAll()
{
    DoCommand(MSG_ViewAllThreads);
	if ( m_pOutliner )
		m_pOutliner->GetParent()->Invalidate();
}

void CMailNewsFrame::OnUpdateViewAll( CCmdUI *pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_ViewAllThreads);
}

void CMailNewsFrame::OnViewKilled()
{
    DoCommand(MSG_ViewKilledThreads);
	if ( m_pOutliner )
		m_pOutliner->GetParent()->Invalidate();
}

void CMailNewsFrame::OnUpdateViewKilled( CCmdUI *pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_ViewKilledThreads, TRUE);
}

void CMailNewsFrame::OnViewNew()
{
    DoCommand(MSG_ViewThreadsWithNew);
	if ( m_pOutliner )
		m_pOutliner->GetParent()->Invalidate();
}

void CMailNewsFrame::OnUpdateViewNew( CCmdUI *pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_ViewThreadsWithNew);
}

void CMailNewsFrame::OnViewNewOnly()
{
    DoCommand(MSG_ViewNewOnly);
	if ( m_pOutliner )
		m_pOutliner->GetParent()->Invalidate();
}

void CMailNewsFrame::OnUpdateViewNewOnly( CCmdUI *pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_ViewNewOnly);
}

void CMailNewsFrame::OnViewWatched()
{
    DoCommand(MSG_ViewWatchedThreadsWithNew);
	if ( m_pOutliner )
		m_pOutliner->GetParent()->Invalidate();
}

void CMailNewsFrame::OnUpdateViewWatched( CCmdUI *pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_ViewWatchedThreadsWithNew);
}

void CMailNewsFrame::OnViewInline(void)
{
    DoMessageCommand(MSG_AttachmentsInline);
}

void CMailNewsFrame::OnUpdateViewInline(CCmdUI* pCmdUI)
{
    DoUpdateMessageCommand(pCmdUI, MSG_AttachmentsInline, FALSE);
}

void CMailNewsFrame::OnViewAsLinks(void)
{
    DoMessageCommand(MSG_AttachmentsAsLinks);
}

void CMailNewsFrame::OnUpdateViewAsLinks(CCmdUI* pCmdUI)
{
    DoUpdateMessageCommand(pCmdUI, MSG_AttachmentsAsLinks, FALSE);
}

void CMailNewsFrame::OnHeadersMicro()
{
	DoMessageCommand(MSG_ShowMicroHeaders);
}

void CMailNewsFrame::OnUpdateHeadersMicro(CCmdUI* pCmdUI)
{
	DoUpdateMessageCommand( pCmdUI, MSG_ShowMicroHeaders );
}

void CMailNewsFrame::OnHeadersShort()
{
	DoMessageCommand(MSG_ShowSomeHeaders);
}

void CMailNewsFrame::OnUpdateHeadersShort(CCmdUI* pCmdUI)
{
	DoUpdateMessageCommand( pCmdUI, MSG_ShowSomeHeaders );
}

void CMailNewsFrame::OnHeadersAll()
{
	DoMessageCommand(MSG_ShowAllHeaders);
}

void CMailNewsFrame::OnUpdateHeadersAll(CCmdUI* pCmdUI)
{
	DoUpdateMessageCommand( pCmdUI, MSG_ShowAllHeaders );
}

// Navigation Menu Items

void CMailNewsFrame::OnPreviousUnread ( )
{
	DoNavigate( MSG_PreviousUnreadMessage );
}

void CMailNewsFrame::OnUpdatePreviousUnread(CCmdUI* pCmdUI)
{
	DoUpdateNavigate( pCmdUI, MSG_PreviousUnreadMessage );
}

void CMailNewsFrame::OnPrevious ( )
{
	DoNavigate( MSG_PreviousMessage );
}

void CMailNewsFrame::OnUpdatePrevious(CCmdUI* pCmdUI)
{
	DoUpdateNavigate( pCmdUI, MSG_PreviousMessage );
}

void CMailNewsFrame::OnNextUnread ( )
{
	DoNavigate( MSG_NextUnreadMessage );
}

void CMailNewsFrame::OnUpdateNextUnread(CCmdUI* pCmdUI)
{
	if (pCmdUI->m_pMenu)
		DoUpdateNavigate( pCmdUI, MSG_NextUnreadMessage );
	else // We're the toolbar button, always enable (sigh)
		pCmdUI->Enable(TRUE);
}

void CMailNewsFrame::OnFirstUnread()
{
	//Currently no back end support for this and it asserts

	//	DoNavigate(MSG_FirstUnreadMessage);
}

void CMailNewsFrame::OnUpdateFirstUnread(CCmdUI* pCmdUI)
{
	//Currently no back end support for this and it asserts
	pCmdUI->Enable(FALSE);
//	DoUpdateNavigate(pCmdUI, MSG_FirstUnreadMessage);
}

void CMailNewsFrame::OnNext ( )
{
	DoNavigate( MSG_NextMessage );
}

void CMailNewsFrame::OnUpdateNext(CCmdUI* pCmdUI)
{
	DoUpdateNavigate( pCmdUI, MSG_NextMessage );
}

void CMailNewsFrame::OnNextFolder( )
{
	DoNavigate( MSG_NextFolder );
}

void CMailNewsFrame::OnUpdateNextFolder(CCmdUI* pCmdUI)
{
	if (m_bNews)
		pCmdUI->SetText(szLoadString(IDS_MENU_NEXTNEWSGROUP));
	else
		pCmdUI->SetText(szLoadString(IDS_MENU_NEXTFOLDER));
	DoUpdateNavigate( pCmdUI, MSG_NextFolder );
}

void CMailNewsFrame::OnNextUnreadThread() 
{
	DoNavigate(MSG_NextUnreadThread);
}

void CMailNewsFrame::OnUpdateNextUnreadThread(CCmdUI* pCmdUI) 
{	
	DoUpdateNavigate(pCmdUI, MSG_NextUnreadThread);
}

void CMailNewsFrame::OnNextUnreadFolder() 
{
	DoNavigate( MSG_NextUnreadGroup );
}

void CMailNewsFrame::OnUpdateNextUnreadFolder(CCmdUI* pCmdUI) 
{	
	if (m_bNews)
		pCmdUI->SetText(szLoadString(IDS_MENU_NEXTUNREADNEWSGROUP));
	else
		pCmdUI->SetText(szLoadString(IDS_MENU_NEXTUNREADFOLDER));
	DoUpdateNavigate( pCmdUI, MSG_NextUnreadGroup );
}

void CMailNewsFrame::OnPreviousFlagged() 
{
	DoNavigate( MSG_PreviousFlagged );
}

void CMailNewsFrame::OnUpdatePreviousFlagged(CCmdUI* pCmdUI) 
{	
	DoUpdateNavigate( pCmdUI, MSG_PreviousFlagged );
}

void CMailNewsFrame::OnNextFlagged() 
{
	DoNavigate( MSG_NextFlagged );
}

void CMailNewsFrame::OnUpdateNextFlagged(CCmdUI* pCmdUI) 
{	
	DoUpdateNavigate( pCmdUI, MSG_NextFlagged );
}

void CMailNewsFrame::OnFirstFlagged() 
{
	DoNavigate( MSG_FirstFlagged );
}

void CMailNewsFrame::OnUpdateFirstFlagged(CCmdUI* pCmdUI) 
{	
	DoUpdateNavigate( pCmdUI, MSG_FirstFlagged );
}

// Message Menu Items

void CMailNewsFrame::OnNew( )
{
	if (m_bNews)
	    DoCommand(MSG_PostNew);    
	else
		DoCommand(MSG_MailNew);
}

void CMailNewsFrame::OnUpdateNew(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_MailNew);
}

void CMailNewsFrame::OnNewNewsgroup()
{
	DoCommand(MSG_NewNewsgroup);
}

void CMailNewsFrame::OnUpdateNewNewsgroup(CCmdUI *pCmdUI)
{
	DoUpdateCommand(pCmdUI, MSG_NewNewsgroup);
}

void CMailNewsFrame::OnNewNewshost()
{
	DoAddNewsServer(this, FROM_FOLDERPANE);
}

void CMailNewsFrame::OnUpdateNewNewshost(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void CMailNewsFrame::OnNewCategory()
{
	DoCommand(MSG_NewCategory);
}

void CMailNewsFrame::OnUpdateNewCategory(CCmdUI *pCmdUI)
{
	DoUpdateCommand(pCmdUI, MSG_NewCategory);
}

void CMailNewsFrame::OnKill()
{
	DoNavigate((MSG_MotionType) MSG_ToggleThreadKilled);
}

void CMailNewsFrame::OnUpdateKill(CCmdUI* pCmdUI)
{
	DoUpdateCommand(pCmdUI, MSG_ToggleThreadKilled );
}

void CMailNewsFrame::OnWatch()
{
	DoCommand(MSG_ToggleThreadWatched);
}

void CMailNewsFrame::OnUpdateWatch(CCmdUI* pCmdUI)
{
	DoUpdateCommand(pCmdUI, MSG_ToggleThreadWatched, FALSE);
}


void CMailNewsFrame::OnPostAndMailReply() 
{
	DoCommand(MSG_PostAndMailReply);	
}

void CMailNewsFrame::OnUpdatePostAndMailReply(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_PostAndMailReply);
}

void CMailNewsFrame::OnPostReply() 
{
	DoCommand(MSG_PostReply);	
}

void CMailNewsFrame::OnUpdatePostReply(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_PostReply);
}

void CMailNewsFrame::OnReply() 
{
	DoCommand(MSG_ReplyToSender);
}

void CMailNewsFrame::OnUpdateReply(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_ReplyToSender);
}

/*
 *  Reply button now has to respond on a single click as well
 *  as supply a menu.
 */
void CMailNewsFrame::OnReplyButton()
{
	// this will have to be changed to be based on preferences
	if(m_bNews)
	{
		DoCommand(MSG_PostReply);
	}
	else
	{
		DoCommand(MSG_ReplyToSender);
	}

}

void CMailNewsFrame::OnReplyAll() 
{
	DoCommand(MSG_ReplyToAll);
}

void CMailNewsFrame::OnUpdateReplyAll(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_ReplyToAll);
}

void CMailNewsFrame::OnForwardButton()
{

	DoCommand(MSG_ForwardMessage);
}

void CMailNewsFrame::OnUpdateForwardButton(CCmdUI* pCmdUI)
{
	DoUpdateCommand(pCmdUI, MSG_ForwardMessage);
}

void CMailNewsFrame::OnForward() 
{
	DoCommand(MSG_ForwardMessageAttachment);
}

void CMailNewsFrame::OnUpdateForward(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_ForwardMessageAttachment);
}

void CMailNewsFrame::OnForwardQuoted() 
{
	DoCommand(MSG_ForwardMessageQuoted);
}

void CMailNewsFrame::OnUpdateForwardQuoted(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_ForwardMessageQuoted);
}

void CMailNewsFrame::OnForwardInline() 
{
	DoCommand(MSG_ForwardMessageInline);
}

void CMailNewsFrame::OnUpdateForwardInline(CCmdUI* pCmdUI) 
{
	DoUpdateCommand(pCmdUI, MSG_ForwardMessageInline);
}

void CMailNewsFrame::OnSave ( )
{
    DoCommand(MSG_SaveMessagesAs);
}

void CMailNewsFrame::OnUpdateSave(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_SaveMessagesAs);
}

void CMailNewsFrame::OnEditMessage ( )
{
    DoCommand(MSG_OpenMessageAsDraft);
}

void CMailNewsFrame::OnUpdateEditMessage(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_OpenMessageAsDraft);
}


void CMailNewsFrame::OnPostAndMail()
{
    DoCommand(MSG_PostAndMailReply);
}

void CMailNewsFrame::OnUpdatePostAndMail ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_PostAndMailReply);
}

void CMailNewsFrame::OnMarkMessagesRead()
{
    DoCommand(MSG_MarkMessagesRead);
}

void CMailNewsFrame::OnRetrieveSelected()
{
	DoCommand(MSG_RetrieveSelectedMessages);
}

void CMailNewsFrame::OnUpdateRetrieveSelected( CCmdUI *pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_RetrieveSelectedMessages);
}

void CMailNewsFrame::OnUpdateMarkMessagesRead ( CCmdUI* pCmdUI )
{
	DoUpdateCommand( pCmdUI, MSG_MarkMessagesRead );
}

void CMailNewsFrame::OnMarkMessagesUnread()
{
	DoCommand( MSG_MarkMessagesUnread );
}

void CMailNewsFrame::OnUpdateMarkMessagesUnread ( CCmdUI* pCmdUI )
{
	DoUpdateCommand( pCmdUI, MSG_MarkMessagesUnread );
}

void CMailNewsFrame::OnMarkThreadRead()
{
	DoCommand( MSG_MarkThreadRead );
}

void CMailNewsFrame::OnUpdateMarkThreadRead( CCmdUI *pCmdUI )
{
	DoUpdateCommand( pCmdUI, MSG_MarkThreadRead );
}

void CMailNewsFrame::OnMarkAllRead()
{
	DoCommand( MSG_MarkAllRead );
}

void CMailNewsFrame::OnUpdateMarkAllRead( CCmdUI *pCmdUI )
{
	DoUpdateCommand( pCmdUI, MSG_MarkAllRead );
}

void CMailNewsFrame::OnCatchup()
{
	CMarkReadDateDlg dlg(CMarkReadDateDlg::IDD, this);
	if (dlg.DoModal()) {
		MSG_MarkReadByDate ( m_pPane, 0, dlg.dateTo.GetTime() );
	}
}

void CMailNewsFrame::OnUpdateCatchup( CCmdUI *pCmdUI )
{
	// We use MarkAllRead because it's enabled state
	// should be the same
    DoUpdateCommand(pCmdUI, MSG_MarkAllRead);
}

void CMailNewsFrame::OnMarkMessagesLater()
{
	DoNavigate( MSG_LaterMessage );	
}

void CMailNewsFrame::OnUpdateMarkMessagesLater( CCmdUI *pCmdUI )
{
	DoUpdateNavigate( pCmdUI, MSG_LaterMessage);
}

void CMailNewsFrame::OnRetrieveOffline()
{
	DoCommand(MSG_RetrieveMarkedMessages);
}

void CMailNewsFrame::OnUpdateRetrieveOffline( CCmdUI *pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_RetrieveMarkedMessages);
}

void CMailNewsFrame::OnMarkMessages()
{
	DoCommand( MSG_MarkMessages );
	if (m_pOutliner)
		DOWNCAST(CMessageOutliner, m_pOutliner)->EnsureFlagsVisible();
}

void CMailNewsFrame::OnUpdateMarkMessages( CCmdUI *pCmdUI )
{
	DoUpdateCommand( pCmdUI, MSG_MarkMessages );
}

void CMailNewsFrame::OnUnmarkMessages()
{
	DoCommand( MSG_UnmarkMessages );
	if (m_pOutliner)
		DOWNCAST(CMessageOutliner, m_pOutliner)->EnsureFlagsVisible();
}

void CMailNewsFrame::OnUpdateUnmarkMessages( CCmdUI *pCmdUI )
{
	DoUpdateCommand( pCmdUI, MSG_UnmarkMessages );
}

void CMailNewsFrame::OnPostNew ( )
{
    DoCommand(MSG_PostNew);
}

void CMailNewsFrame::OnUpdatePostNew(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_PostNew);
}

void CMailNewsFrame::OnWrapLongLines ( )
{
	DoMessageCommand(MSG_WrapLongLines);
}
 
void CMailNewsFrame::OnUpdateWrapLongLines ( CCmdUI * pCmdUI )
{
	DoUpdateMessageCommand( pCmdUI, MSG_WrapLongLines );
}

void CMailNewsFrame::OnViewRot13 ( )
{
	DoMessageCommand(MSG_Rot13Message);
}
 
void CMailNewsFrame::OnUpdateViewRot13 ( CCmdUI * pCmdUI )
{
	DoUpdateMessageCommand( pCmdUI, MSG_Rot13Message );
}

static BOOL s_bGettingMail = FALSE;

static void _GetMailCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
   if (pane != NULL)
   {
	   s_bGettingMail = TRUE;
	   MSG_Command( pane, MSG_GetNewMail, NULL, 0 );
	   ::ShowWindow(hwnd, SW_SHOW);
   }
}

static void _GetMailDoneCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
	s_bGettingMail = FALSE;
	for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext)
		f->PostMessage(WM_COMMAND, (WPARAM) ID_DONEGETTINGMAIL, (LPARAM) 0);
}

static void _GetNextCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
   if (pane != NULL)
   {
	   XP_Bool bEnabled, bPlural;
	   MSG_COMMAND_CHECK_STATE stateSelected;
	   LPCSTR lpszLabel;
	   MSG_CommandStatus( pane, MSG_GetNextChunkMessages, NULL, 0, 
					      &bEnabled, &stateSelected,
					      &lpszLabel, &bPlural );

	   if ( !bEnabled ) {
         ::PostMessage(hwnd,WM_DESTROY,0,0);
	   }

   	MSG_Command( pane, MSG_GetNextChunkMessages, NULL, 0 );
   }
}

static void _CompressFoldersCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
   if (pane != NULL)
   {
	   MSG_Command( pane, MSG_CompressAllFolders, NULL, 0 );
	   ::ShowWindow(hwnd, SW_SHOW);
   }
}

typedef struct {
	MSG_ViewIndex *indices;
	int count;
} CompressFolderClosure;

static void _CompressFolderCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
   CompressFolderClosure *cfc = (CompressFolderClosure *) closure;

   if (pane != NULL)
   {
       MSG_Command(pane, MSG_CompressFolder, cfc->indices, cfc->count);
	   ::ShowWindow(hwnd, SW_SHOW);
   }
}

static void _EmptyTrashCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
   if (pane != NULL)
   {
	   MSG_Command( pane, MSG_EmptyTrash, NULL, 0 );
	   ::ShowWindow(hwnd, SW_SHOW);
   }
}

/////////////////////////////////////////////////////////////////////////////
void CMailNewsFrame::OnGetMail()
{
	// prompt user about going online. Make sure get new mail is checked
	if (NET_IsOffline())
	{
		PREF_SetBoolPref("offline.download_mail", TRUE);
		OnGoOffline();
	}
	else
		new CProgressDialog(this, m_pPane, _GetMailCallback, NULL, NULL, _GetMailDoneCallback);
}

void CMailNewsFrame::OnUpdateGetMail(CCmdUI *pCmdUI)
{
	if (!s_bGettingMail)
	    DoUpdateCommand(pCmdUI, MSG_GetNewMail);
	else
		pCmdUI->Enable(FALSE);
}

void CMailNewsFrame::OnGetNext()
{
	new CProgressDialog(this, m_pPane, _GetNextCallback);
}

void CMailNewsFrame::OnUpdateGetNext(CCmdUI *pCmdUI)
{
	int32 iDownloadMax = 0;
	PREF_GetIntPref("news.max_articles", &iDownloadMax);

	CString cs;
	cs.Format(szLoadString(IDS_MENU_GETNEXT), (int) iDownloadMax);

	pCmdUI->SetText(cs);

    DoUpdateCommand(pCmdUI, MSG_GetNextChunkMessages);
}

void CMailNewsFrame::OnDeliverNow()
{
    DoCommand(MSG_DeliverQueuedMessages);
}

void CMailNewsFrame::OnUpdateDeliverNow(CCmdUI *pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_DeliverQueuedMessages);
}

void CMailNewsFrame::OnNewFolder ( )
{
	if (m_bNews) {
		if (m_bCategory)
			OnNewCategory();
		else
			OnNewNewsgroup();
	} else {
		MSG_FolderInfo *pFolderInfo	= NULL;
		if (IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
		{
			MSG_FolderLine folderLine;
			if (((C3PaneMailFrame*)this)->GetSelectedFolder(&folderLine))
				pFolderInfo = folderLine.id;
		}
		else
			pFolderInfo = GetCurFolder();
		MWContext *pXPCX = NULL;
		MWContextType saveType;
		if (m_pPane)
		{
			pXPCX = MSG_GetContext( m_pPane );
			saveType = pXPCX->type;
		}
		CNewFolderDialog newFolderDlg( this, m_pPane, MSG_SuggestNewFolderParent(pFolderInfo, WFE_MSGGetMaster()) );
		newFolderDlg.DoModal();
		if (m_pPane)
			pXPCX->type = saveType;	
	}
}

void CMailNewsFrame::OnUpdateNewFolder(CCmdUI* pCmdUI)
{
	if (m_bNews) {
		if (m_bCategory) {
			pCmdUI->SetText(szLoadString(IDS_MENU_NEWCATEGORY));
			OnUpdateNewCategory(pCmdUI);
		} else {
			pCmdUI->SetText(szLoadString(IDS_MENU_NEWNEWSGROUP));
			OnUpdateNewNewsgroup(pCmdUI);
		}
	} else {
		pCmdUI->SetText(szLoadString(IDS_MENU_NEWFOLDER));
		pCmdUI->Enable( TRUE );
	}
}

void CMailNewsFrame::OnCompressFolder ( )
{
    DoCommand(MSG_CompressFolder, 0);
}

void CMailNewsFrame::OnUpdateCompressFolder(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_CompressFolder);
}

void CMailNewsFrame::OnRenameFolder ( )
{
    DoCommand(MSG_DoRenameFolder);
}

void CMailNewsFrame::OnUpdateRenameFolder(CCmdUI* pCmdUI)
{
    DoUpdateCommand(pCmdUI, MSG_DoRenameFolder);
}

void CMailNewsFrame::OnCompressAll()
{
	MSG_FolderInfo *folderInfo = GetCurFolder();
	
	if (folderInfo) {
		int32 lDeleteModel = 1;

		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( WFE_MSGGetMaster(), GetCurFolder(), &folderLine );
		if ( folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX ) {
			MSG_Host *host = MSG_GetHostForFolder(folderInfo);
			if(host != NULL)
			{
				const char * hostName = MSG_GetHostName(host);
				if(hostName != NULL)
				{
					IMAP_GetIntPref(hostName, INT_DELETE_MODEL, &lDeleteModel);
				}
			}
		}

		if (lDeleteModel != 1) {
			CompressFolderClosure cfc;
			cfc.indices = NULL;
			cfc.count = 0;

			if (MSG_GetPaneType(m_pPane) == MSG_FOLDERPANE && m_pOutliner) {
				m_pOutliner->GetSelection(cfc.indices, cfc.count);
			}
			new CProgressDialog(this, m_pPane, _CompressFolderCallback,
								&cfc, szLoadString(IDS_COMPRESSINGFOLDERS));
		} else {
			new CProgressDialog(this, m_pPane, _CompressFoldersCallback,
								NULL, szLoadString(IDS_COMPRESSINGFOLDERS));
		}
	}
}

void CMailNewsFrame::OnUpdateCompressAll ( CCmdUI* pCmdUI )
{
	MSG_FolderInfo *folderInfo = GetCurFolder();
	
	if (folderInfo) {
		int32 lDeleteModel = 1;

		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( WFE_MSGGetMaster(), GetCurFolder(), &folderLine );
		if ( folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX ) {
			MSG_Host *host = MSG_GetHostForFolder(folderInfo);
			if(host != NULL)
			{
				const char * hostName = MSG_GetHostName(host);
				if(hostName != NULL)
				{
					IMAP_GetIntPref(hostName, INT_DELETE_MODEL, &lDeleteModel);
				}
			}
		}

		if (lDeleteModel != 1) {
			pCmdUI->SetText(szLoadString(IDS_MENU_COMPRESSFOLDER));
			DoUpdateCommand(pCmdUI, MSG_CompressFolder);
		} else {
			pCmdUI->SetText(szLoadString(IDS_MENU_COMPRESSFOLDERS));
			DoUpdateCommand(pCmdUI, MSG_CompressAllFolders);
		}
	} else {
		pCmdUI->Enable(FALSE);
	}
}

void CMailNewsFrame::OnEmptyTrash()
{
    new CProgressDialog(this, m_pPane, _EmptyTrashCallback,
						NULL, szLoadString(IDS_EMPTYINGTRASH));
}

void CMailNewsFrame::OnUpdateEmptyTrash ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_EmptyTrash);
}

void CMailNewsFrame::OnAddSenderAddressBook(UINT nID)
{	
	OnAddAddressBook(nID, MSG_AddSender);	
}

void CMailNewsFrame::OnAddAllAddressBook(UINT nID)
{	
	OnAddAddressBook(nID, MSG_AddAll);	
}

void CMailNewsFrame::OnAddAddressBook(UINT nID, MSG_CommandType command)
{
	int nIndex = (command == MSG_AddSender) ? nID - FIRST_ADDSENDER_MENU_ID :
											  nID - FIRST_ADDALL_MENU_ID;

	XP_List *addressBooks = AB_AcquireAddressBookContainers(GetContext()->GetContext()); 

	if(addressBooks)
	{
		AB_ContainerInfo *pInfo = (AB_ContainerInfo*)XP_ListGetObjectNum(addressBooks, nIndex);
																		 

		if(m_pPane)
		{
			MSG_ViewIndex *indices = NULL;
			int count = 0;
			if(m_pOutliner)
				m_pOutliner->GetSelection( indices, count );

			MSG_AddToAddressBook(m_pPane, command, indices, count, pInfo); 
		}

		AB_ReleaseContainersList(addressBooks);
	}
	  
}

void CMailNewsFrame::OnUpdateAddSenderAddressBook(CCmdUI *pCmdUI)
{

	OnUpdateAddAddressBook(pCmdUI, MSG_AddSender);

}

void CMailNewsFrame::OnUpdateAddAllAddressBook(CCmdUI *pCmdUI)
{

	OnUpdateAddAddressBook(pCmdUI, MSG_AddAll);
}

void CMailNewsFrame::OnUpdateAddAddressBook(CCmdUI *pCmdUI, MSG_CommandType command)
{
    XP_Bool selectable = FALSE; 
    MSG_COMMAND_CHECK_STATE selected; 
    const char *displayString = NULL;
    XP_Bool plural = FALSE; 
	MSG_ViewIndex *indices = NULL;
	int count = 0;
	if(m_pOutliner)
		m_pOutliner->GetSelection( indices, count );

	int nIndex = (command == MSG_AddSender) ? pCmdUI->m_nID - FIRST_ADDSENDER_MENU_ID :
											  pCmdUI->m_nID - FIRST_ADDALL_MENU_ID;

	XP_List *addressBooks = AB_AcquireAddressBookContainers(GetContext()->GetContext()); 

	if(addressBooks)
	{
		AB_ContainerInfo *pInfo = (AB_ContainerInfo*)XP_ListGetObjectNum(addressBooks, nIndex);
																		 

		MSG_AddToAddressBookStatus(m_pPane, command, indices, count, &selectable, 
								   &selected, &displayString, &plural, pInfo); 

		pCmdUI->Enable(selectable);

		AB_ReleaseContainersList(addressBooks);
	}

}

void CMailNewsFrame::OnAddToAddressBook()
{
	theApp.DoWaitCursor(1);
	DoCommand(MSG_AddSender);	
	theApp.DoWaitCursor(-1);
}

void CMailNewsFrame::OnUpdateAddToAddressBook ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_AddSender);
}

void CMailNewsFrame::OnAddAllToAddressBook()
{
	theApp.DoWaitCursor(1);
	DoCommand(MSG_AddAll);	
	theApp.DoWaitCursor(-1);
}

void CMailNewsFrame::OnUpdateAddAllToAddressBook ( CCmdUI* pCmdUI )
{
    DoUpdateCommand(pCmdUI, MSG_AddAll);
}

// Non-MSG Items

void CMailNewsFrame::OnTips()
{
	if ( m_pOutliner ) {
		m_pOutliner->EnableTips( TRUE );
	}
}

void CMailNewsFrame::OnQuickFile()
{
}

void CMailNewsFrame::OnUpdateQuickFile( CCmdUI *pCmdUI )
{
	if (m_pOutliner) {
		MSG_ViewIndex *indices;
		int count;
		m_pOutliner->GetSelection( indices, count );
		pCmdUI->Enable( count > 0 );
	} else {
		pCmdUI->Enable( TRUE );
	}
}

// replace with Manage Mail Account
void CMailNewsFrame::OnSetupWizard()
{
	DoCommand(MSG_ManageMailAccount);
}

void CMailNewsFrame::OnUpdateSetupWizard( CCmdUI *pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_ManageMailAccount);
}

void CMailNewsFrame::OnServerStuff()
{
	DoCommand(MSG_ModerateNewsgroup);
}

void CMailNewsFrame::OnUpdateServerStuff( CCmdUI *pCmdUI )
{
	DoUpdateCommand(pCmdUI, MSG_ModerateNewsgroup);
}

void CMailNewsFrame::OnInterrupt()
{
	//      Let the context have it.
	if(GetMainContext()) {
		GetMainContext()->AllInterrupt();
	}
}

void CMailNewsFrame::OnUpdateInterrupt( CCmdUI *pCmdUI )
{
	//      Defer to the context's wisdom.
	pCmdUI->Enable( GetMainContext() && GetMainContext()->CanAllInterrupt() );
}

void CMailNewsFrame::OnPriorityLowest()
{
	MSG_SetPriority( m_pPane,
					 GetCurMessage(),
					 MSG_LowestPriority );
}

void CMailNewsFrame::OnPriorityLow()
{
	MSG_SetPriority( m_pPane,
					 GetCurMessage(),
					 MSG_LowPriority );
}

void CMailNewsFrame::OnPriorityNormal()
{
	MSG_SetPriority( m_pPane,
					 GetCurMessage(),
					 MSG_NormalPriority );
}

void CMailNewsFrame::OnPriorityHigh()
{
	MSG_SetPriority( m_pPane,
					 GetCurMessage(),
					 MSG_HighPriority );
}

void CMailNewsFrame::OnPriorityHighest()
{
	MSG_SetPriority( m_pPane,
					 GetCurMessage(),
					 MSG_HighestPriority );
}

void CMailNewsFrame::OnUpdatePriority(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetCurMessage() != MSG_MESSAGEKEYNONE);
}

#define UNSEC_SIGNED_INDEX	7
#define UNSECURE_INDEX		8
#define SEC_SIGNED_INDEX	9
#define SECURE_INDEX		10

void CMailNewsFrame::OnUpdateSecurity(CCmdUI *pCmdUI)
{
	HG92611
}

void CMailNewsFrame::OnUpdateSecureStatus(CCmdUI *pCmdUI)
{
	HG11173
}

void CMailNewsFrame::SetCSID( int csid ) {
	if (m_iCSID != csid)
	{
		m_iCSID = csid;
		if (m_pOutliner) {
			m_pOutliner->SetCSID( csid );
		}

		if (m_pInfoBar) {
			m_pInfoBar->SetCSID( csid );
		}
        GetContext()->NiceReload();
		if ( m_pMessageView ) {
			m_pMessageView->SetCSID( csid );
		}
	}
}

void CMailNewsFrame::RefreshNewEncoding(int16 doc_csid, BOOL bSave )
{
	int16 win_csid = INTL_DocToWinCharSetID(doc_csid);
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo( GetMainContext()->GetContext() );

	INTL_SetCSIDocCSID( c, doc_csid );	
	INTL_SetCSIWinCSID( c, win_csid );
	
	SetCSID( doc_csid );

	if ( bSave ) {
		MSG_FolderInfo *folderInfo = GetCurFolder();
		if (m_bCategory)
			folderInfo = MSG_GetCategoryContainerForCategory(folderInfo);

		if (folderInfo) {
			MSG_SetFolderCSID( folderInfo, doc_csid );
		}
	}
}

void CMailNewsFrame::OnToggleEncoding1()  // Latin1
{
    RefreshNewEncoding(CS_LATIN1);
}

void CMailNewsFrame::OnToggleEncoding2()  // Latin2
{
    RefreshNewEncoding(CS_LATIN2);
}

void CMailNewsFrame::OnToggleEncoding3()  // Japanese(Auto)
{
    RefreshNewEncoding(CS_SJIS_AUTO);
}

void CMailNewsFrame::OnToggleEncoding4()  // Japanese(ShiftJIS)
{
    RefreshNewEncoding(CS_SJIS);
}

void CMailNewsFrame::OnToggleEncoding5()  // Japanese(EUC)
{
    RefreshNewEncoding(CS_EUCJP);
}

void CMailNewsFrame::OnToggleEncoding6()  // Chinese(BIG5)
{
    RefreshNewEncoding(CS_BIG5);
}

void CMailNewsFrame::OnToggleEncoding7()  // Chinese (EUC)
{
    RefreshNewEncoding(CS_CNS_8BIT);
}

void CMailNewsFrame::OnToggleEncoding8()  // Simplified Chinese
{
    RefreshNewEncoding(CS_GB_8BIT);
}

void CMailNewsFrame::OnToggleEncoding9()    // Korean ( Auto )
{
    RefreshNewEncoding(CS_KSC_8BIT | CS_AUTO);
}

void CMailNewsFrame::OnToggleEncoding10()
{
    RefreshNewEncoding(CS_USER_DEFINED_ENCODING);
}

void CMailNewsFrame::OnToggleEncoding11()
{ 
    RefreshNewEncoding(CS_CP_1250);
}

void CMailNewsFrame::OnToggleEncoding12()
{
    RefreshNewEncoding(CS_CP_1251);
}

void CMailNewsFrame::OnToggleEncoding13()
{
    RefreshNewEncoding(CS_8859_5);
}

void CMailNewsFrame::OnToggleEncoding14()
{
    RefreshNewEncoding(CS_KOI8_R);
}

void CMailNewsFrame::OnToggleEncoding15()
{
    RefreshNewEncoding(CS_8859_7);
}

void CMailNewsFrame::OnToggleEncoding16()
{
    RefreshNewEncoding(CS_CP_1253);
}

void CMailNewsFrame::OnToggleEncoding17()
{
    RefreshNewEncoding(CS_8859_9);
}

void CMailNewsFrame::OnToggleEncoding18()
{
    RefreshNewEncoding(CS_UTF8);
}

void CMailNewsFrame::OnToggleEncoding19()
{
    RefreshNewEncoding(CS_UCS2);
}

void CMailNewsFrame::OnToggleEncoding20()
{
    RefreshNewEncoding(CS_UTF7);
}

void CMailNewsFrame::OnUpdateEncoding(CCmdUI* pCmdUI)
{
    BOOL bCheck;
    bCheck = FALSE;
    switch (pCmdUI->m_nID) {
    case ID_OPTIONS_ENCODING_1:    // Western(Latin1)
        if(m_iCSID == CS_LATIN1)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_2:     // East European(latin2)
        if(m_iCSID == CS_LATIN2)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_3:     // Japanese(Auto)
        if(m_iCSID == CS_SJIS_AUTO)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_4:     // Japanese(ShiftJIS)
        if(m_iCSID == CS_SJIS)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_5:     // Japanese(EUC)
        if(m_iCSID == CS_EUCJP)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_6:     // Traditional Chinese(Big5)
        if(m_iCSID == CS_BIG5)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_7:     // Traditional Chinese(EUC)
        if(m_iCSID == CS_CNS_8BIT)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_8:     // Simplified Chinese
        if(m_iCSID == CS_GB_8BIT)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_9:     // Korean
        if((m_iCSID & ~CS_AUTO) == CS_KSC_8BIT)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_10:        // User defined 
        if(m_iCSID == CS_USER_DEFINED_ENCODING)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_11:        // Windows Central European(latin2)
        if(m_iCSID == CS_CP_1250)
            bCheck = TRUE;
        break; 
    case ID_OPTIONS_ENCODING_12:        // Cyrillic (KOI8-R)
        if(m_iCSID == CS_CP_1251)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_13:        // Cyrillic (ISO8859-5)
        if(m_iCSID == CS_8859_5)
            bCheck = TRUE;
        break;
	case ID_OPTIONS_ENCODING_14:
        if(m_iCSID == CS_KOI8_R)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_15:        // Greek (8859_7)
        if(m_iCSID == CS_8859_7)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_16:        // Greek (cp1253)
        if(m_iCSID == CS_CP_1253)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_17:        // Turkish (8859_9) 
        if(m_iCSID == CS_8859_9)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_18:            // Unicode (UTF8)
        if(m_iCSID == CS_UTF8)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_19:            // Unicode (UCS2)
        if(m_iCSID == CS_UCS2 || m_iCSID == CS_UCS2_SWAP)
            bCheck = TRUE;
        break;
    case ID_OPTIONS_ENCODING_20:            // Unicode (UTF7)
        if(m_iCSID == CS_UTF7)
            bCheck = TRUE;
        break;
    default:                // Illegal ID is wrong.
        return;
    }
 
    if (bCheck)
        pCmdUI->SetCheck(TRUE);
    else
        pCmdUI->SetCheck(FALSE);
}

// CMsgListFrame
STDMETHODIMP CMsgListFrame::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) this;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CMsgListFrame::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CMsgListFrame::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

CMsgListFrame::CMsgListFrame()
{
}

// IMsgList Interface

void CMsgListFrame::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == m_pPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffStarting( asynchronous, notify,
												   where, num );
		}
	} else {
		ASSERT(0);
	}
}

void CMsgListFrame::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == m_pPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffFinishing( asynchronous, notify,
												    where, num );
		}
	} else {
		ASSERT(0);
	}
}

void CMsgListFrame::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							      int *focus)
{
	if ( pane == m_pPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->GetSelection(*indices, *count);
			*focus = m_pOutliner->GetFocusLine();
		}
	} else {
		ASSERT(0);
	}
}

void CMsgListFrame::SelectItem( MSG_Pane* pane, int item )
{
	if ( pane == m_pPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->SelectItem(item);
			m_pOutliner->ScrollIntoView(item);
		}
	} else {
		ASSERT(0);
	}
}

void CMsgListFrame::CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
									  MSG_FolderInfo *folderInfo)
{
	ASSERT(0);
}

void CMsgListFrame::MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
									  MSG_FolderInfo *folderInfo)
{
	ASSERT(0);
}

BEGIN_MESSAGE_MAP( CMsgListFrame, CMailNewsFrame)
	ON_WM_CREATE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


int CMsgListFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int res = CMailNewsFrame::OnCreate( lpCreateStruct );

	return res;
}

void CMsgListFrame::OnDestroy()
{
	// Destroy my own damn context
	if(GetMainContext() && !GetMainContext()->IsDestroyed()) {
		CAbstractCX *pCX = GetMainContext();
		ClearContext(pCX);
		pCX->DestroyContext();
	}

	CMailNewsFrame::OnDestroy();
}

BOOL WFE_MSGCheckWizard(CWnd *pParent)
{
	return CMailNewsFrame::CheckWizard(pParent);
}
