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
#include "shcut.h"
#include "msgcom.h"
#include "msg_srch.h"
#include "prefapi.h"
#include "msgtmpl.h"
#include "netsdoc.h"
#include "mailmisc.h"
#include "mailpriv.h"
#include "msgview.h"
#include "fldrfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "wfemsg.h"
#include "intl_csi.h"
#include "fegui.h"
#include "addrfrm.h" //for MOZ_NEWADDR
#include "rdfglobal.h"

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CFolderView, COutlinerView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#define COL_LEFT_MARGIN	((m_cxChar+1)/2)
#define EXTRATEXT_SIZE	128
 
extern "C" const char* FE_GetFolderDirectory(MWContext *pContext);

#ifdef _WIN32
extern "C" BOOL UpdateMapiDll(void);
#endif

int WFE_MSGTranslateFolderIcon( uint8 level, int32 iFlags, BOOL bOpen )
{
	int folderTable[] = { IDX_MAILFOLDERCLOSED, IDX_MAILFOLDEROPEN,
						  IDX_MAILFOLDERCLOSEDNEW, IDX_MAILFOLDEROPENNEW,
						// 4
						  IDX_NEWSGROUP, IDX_NEWSGROUP,
						  IDX_NEWSGROUP, IDX_NEWSGROUP,
						// 8
  						  IDX_LOCALMAIL, IDX_LOCALMAIL,
  						  IDX_LOCALMAIL, IDX_LOCALMAIL,
						// 12
						  IDX_REMOTEMAIL, IDX_REMOTEMAIL,
						  IDX_REMOTEMAIL, IDX_REMOTEMAIL,
						// 16
						  IDX_NEWSHOST, IDX_NEWSHOST,
						  IDX_NEWSHOST, IDX_NEWSHOST,
						// 20
						  IDX_INBOXCLOSED, IDX_INBOXOPEN,
						  IDX_INBOXCLOSEDNEW, IDX_INBOXOPENNEW,
						// 24
						  IDX_OUTBOXCLOSED, IDX_OUTBOXOPEN,
						  IDX_OUTBOXCLOSED, IDX_OUTBOXOPEN,
						// 28
						  IDX_SENTCLOSED, IDX_SENTOPEN,
						  IDX_SENTCLOSED, IDX_SENTOPEN,
						// 32
						  IDX_DRAFTSCLOSED, IDX_DRAFTSOPEN,
						  IDX_DRAFTSCLOSED, IDX_DRAFTSOPEN,
						// 36
						  IDX_TRASH, IDX_TRASHOPEN,
						  IDX_TRASH, IDX_TRASHOPEN,
						// 40
						  IDX_TEMPLATECLOSE, IDX_TEMPLATEOPEN,
						  IDX_TEMPLATECLOSE, IDX_TEMPLATEOPEN,
						// 44
						  IDX_SHARECLOSED, IDX_SHAREOPEN,
						  IDX_SHARECLOSED, IDX_SHAREOPEN,
						// 48
						  IDX_PUBLICCLOSED, IDX_PUBLICOPEN,
						  IDX_PUBLICCLOSED, IDX_PUBLICOPEN,
						};

	int idx = 0;

	if (iFlags & MSG_FOLDER_FLAG_NEWSGROUP)
		idx = 4;
	else if (level < 2)
	{
		if (iFlags & MSG_FOLDER_FLAG_NEWS_HOST)
			idx = 16;
		else if (iFlags & MSG_FOLDER_FLAG_IMAPBOX)
			idx = 12;
		else
			idx = 8;
	}
	else
	{
		if (iFlags & MSG_FOLDER_FLAG_INBOX)
			idx = 20;
		else if (iFlags & MSG_FOLDER_FLAG_QUEUE)
			idx = 24;
		else if (iFlags & MSG_FOLDER_FLAG_DRAFTS)
			idx = 32;
		else if (iFlags & MSG_FOLDER_FLAG_TRASH )
			idx = 36;
		else if (iFlags & MSG_FOLDER_FLAG_SENTMAIL )
			idx = 28;
		else if (iFlags & MSG_FOLDER_FLAG_TEMPLATES )
			idx = 40;
		else if (iFlags & MSG_FOLDER_FLAG_PERSONAL_SHARED )
			idx = 44;
		else if (iFlags & MSG_FOLDER_FLAG_IMAP_OTHER_USER )
			idx = 44;
		else if (iFlags & MSG_FOLDER_FLAG_IMAP_PUBLIC )
			idx = 48;
	}
		
	idx += bOpen ? 1 : 0;
	idx += iFlags & MSG_FOLDER_FLAG_GOT_NEW ? 2 : 0;

	return folderTable[idx];
}


void WFE_MSGBuildMessagePopup( HMENU hmenu, BOOL bNews, BOOL bInHeaders, MWContext *pContext )
{
	::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_REPLY, szLoadString( IDS_MENU_REPLY ) );
	if ( bNews ) {
		::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_POSTREPLY, szLoadString( IDS_MENU_POSTREPLY ) );
		::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_POSTANDREPLY, szLoadString( IDS_MENU_POSTMAILREPLY ) );
	} else {
		::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_REPLYALL, szLoadString( IDS_MENU_REPLYALL ) );
	}
	::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_FORWARD, szLoadString( IDS_MENU_FORWARD ) );
	::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_FORWARDQUOTED, szLoadString( IDS_MENU_FORWARDQUOTED ) );
	::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_FORWARDINLINE, szLoadString(IDS_MENU_FORWARDINLINE));
	::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
	 
#ifdef MOZ_NEWADDR
	WFE_MSGBuildAddAddressBookPopups(hmenu, ::GetMenuItemCount(hmenu), TRUE, pContext); 

#else
	HMENU hAddMenu = ::CreatePopupMenu();
	::AppendMenu( hAddMenu, MF_STRING, ID_MESSAGE_ADDSENDER, szLoadString( IDS_POPUP_ADDSENDER ) );
	::AppendMenu( hAddMenu, MF_STRING, ID_MESSAGE_ADDALL, szLoadString( IDS_POPUP_ADDALL ) );
	::AppendMenu( hmenu, MF_STRING|MF_POPUP, (UINT) hAddMenu, szLoadString( IDS_POPUP_ADDTOADDRESSBOOK ) );
#endif
	::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
 
	if (bInHeaders) {
		::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_KILL, szLoadString( IDS_POPUP_IGNORETHREAD ) );
		::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_WATCH, szLoadString( IDS_POPUP_WATCHTHREAD ) );
		::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
		if (!bNews) {
			 
			HMENU hPriorityMenu = ::CreatePopupMenu();
			::AppendMenu( hPriorityMenu, MF_STRING, ID_PRIORITY_LOWEST, szLoadString( IDS_POPUP_LOWEST ) );
			::AppendMenu( hPriorityMenu, MF_STRING, ID_PRIORITY_LOW, szLoadString( IDS_POPUP_LOW ) );
			::AppendMenu( hPriorityMenu, MF_STRING, ID_PRIORITY_NORMAL, szLoadString( IDS_POPUP_NORMAL ) );
			::AppendMenu( hPriorityMenu, MF_STRING, ID_PRIORITY_HIGH, szLoadString( IDS_POPUP_HIGH ) );
			::AppendMenu( hPriorityMenu, MF_STRING, ID_PRIORITY_HIGHEST, szLoadString( IDS_POPUP_HIGHEST ) );
			::AppendMenu( hmenu, MF_STRING|MF_POPUP, (UINT) hPriorityMenu, szLoadString( IDS_POPUP_PRIORITY ) );
			::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
		}
	}
 
	UINT nID = FIRST_MOVE_MENU_ID;
	HMENU hFileMenu = CreatePopupMenu();
	CMailNewsFrame::UpdateMenu( NULL, hFileMenu, nID );
	::AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT) hFileMenu, szLoadString( IDS_POPUP_FILE ) );
	::AppendMenu( hmenu, MF_STRING, CASTUINT(ID_EDIT_DELETEMESSAGE), 
				  szLoadString( CASTUINT(bNews ? IDS_POPUP_CANCELMESSAGE : IDS_POPUP_DELETEMESSAGE )) );
	::AppendMenu( hmenu, MF_STRING, ID_FILE_SAVEMESSAGES, szLoadString( IDS_POPUP_SAVEMESSAGE ) );
	::AppendMenu( hmenu, MF_STRING, ID_FILE_PRINT, szLoadString( IDS_POPUP_PRINTMESSAGE ) );
}

//////////////////////////////////////////////////////////////////////////////
// CBiffCX

class CBiffCX: public CStubsCX
{
public:
	CBiffCX(): CStubsCX(Network, MWContextBiff) {};
	
	virtual void DestroyContext();
};

void CBiffCX::DestroyContext()
{
	MSG_BiffCleanupContext(GetContext());

	CStubsCX::DestroyContext();
}

//////////////////////////////////////////////////////////////////////////////
// CMessagePrefs

CMsgPrefs g_MsgPrefs;

#ifdef _WIN32
static TCHAR lpszEventMailBeep[] = _T("AppEvents\\EventLabels\\MailBeep");
static TCHAR lpszSchemeMailBeep[] = _T("AppEvents\\Schemes\\Apps\\.Default\\MailBeep");
static TCHAR lpszCurrent[] = _T(".current");
static TCHAR lpszChord[] = _T("chord.wav");
#endif
static char lpszSounds[] = "Sounds";
static char lpszMailBeep[] = "MailBeep";

void WFE_MSGInit()
{
#ifdef _WIN32
	if (sysInfo.m_bWin4) {
		TCHAR szBuf[_MAX_PATH];
		LONG lBufSize = _MAX_PATH;

		if ( ::RegQueryValue(HKEY_CURRENT_USER, lpszEventMailBeep, szBuf, &lBufSize) !=
			 ERROR_SUCCESS ) {
			::RegSetValue(HKEY_CURRENT_USER, lpszEventMailBeep, REG_SZ,
						  szLoadString(IDS_MAILSOUND), _tcslen(szLoadString(IDS_MAILSOUND)));
		}

		HKEY hkey;
		if ( ::RegCreateKey(HKEY_CURRENT_USER, lpszSchemeMailBeep, &hkey) == 
			 ERROR_SUCCESS ) {
			
			lBufSize = _MAX_PATH;
			if ( ::RegQueryValue(hkey, lpszCurrent, szBuf, &lBufSize) !=
				 ERROR_SUCCESS ) {
				::RegSetValue(hkey, lpszCurrent, REG_SZ,
							  lpszChord, _tcslen(lpszChord));
			}
			::RegCloseKey(hkey);
		}
	}
	else
#endif
	{
		char szBuf[_MAX_PATH * 2];
		LONG lBufSize = _MAX_PATH * 2;
		::GetProfileString(lpszSounds, lpszMailBeep, szLoadString(IDS_MAILSOUND16), szBuf, lBufSize );
		::WriteProfileString(lpszSounds, lpszMailBeep, szBuf);
	}	
	g_MsgPrefs.Init();
}

void WFE_MSGShutdown()
{
    // Don't do anything if we never called WFE_MSGInit
    if (g_MsgPrefs.m_bInitialized)
    {
		MSG_DestroyMaster(WFE_MSGGetMaster());
		MSG_ShutdownMsgLib();
		g_MsgPrefs.Shutdown();
	}
}

// returns the master.  If there isn't one then it
// creates one.
MSG_Master* WFE_MSGGetMaster()
{
	return g_MsgPrefs.GetMaster();
}

// returns the current value of the master.
MSG_Master* WFE_MSGGetMasterValue()
{
	return g_MsgPrefs.GetMasterValue();

}

CMsgPrefs::CMsgPrefs()
{
	m_pMaster = NULL;
	m_bInitialized = FALSE;
}

PR_CALLBACK cbMsgPrefs(const char *prefName, void *pData)
{
	XP_Bool bPref;
	char *pszPref = NULL;

	switch ((int) pData) {
	case 0:
		bPref= TRUE;
		PREF_GetBoolPref("mailnews.reuse_thread_window", &bPref);
		g_MsgPrefs.m_bThreadReuse = bPref;
		break;

	case 1:
		bPref= TRUE;
		PREF_GetBoolPref("mailnews.reuse_message_window", &bPref);
		g_MsgPrefs.m_bMessageReuse = bPref;
		break;

	case 2:
		bPref= TRUE;
		PREF_GetBoolPref("mailnews.message_in_thread_window", &bPref);
		g_MsgPrefs.m_bThreadPaneMaxed = !bPref;
		break;

	case 3:
		PREF_CopyCharPref("mail.directory", &pszPref);
		if (pszPref)
			g_MsgPrefs.m_csMailDir = pszPref;
		break;

	case 4:
		PREF_CopyCharPref("network.hosts.pop_server", &pszPref);
		if (pszPref) {
			g_MsgPrefs.m_csMailHost = pszPref;
		}
		break;

	case 5:
		PREF_CopyCharPref("news.directory", &pszPref);
		if (pszPref)
			g_MsgPrefs.m_csNewsDir = pszPref;
		break;

	case 6:
		PREF_CopyCharPref("network.hosts.nntp_server", &pszPref);
		if (pszPref) {
		    NET_SetNewsHost(pszPref);
			g_MsgPrefs.m_csNewsHost = pszPref;
		}
		break;

	case 7:
		PREF_CopyCharPref("mail.identity.useremail", &pszPref);
		if (pszPref)
			g_MsgPrefs.m_csUsersEmailAddr = pszPref;
		break;

	case 8:
		PREF_CopyCharPref("mail.identity.username", &pszPref);
		if (pszPref)
			g_MsgPrefs.m_csUsersFullName = pszPref;
		break;

	case 9:
		PREF_CopyCharPref("mail.identity.organization", &pszPref);
		if (pszPref)
			g_MsgPrefs.m_csUsersOrganization = pszPref;
		break;

	case 10:
		PREF_CopyCharPref("mail.signature_file", &pszPref);
		if (pszPref) {
			XP_FREEIF(g_MsgPrefs.m_pszUserSig);
			g_MsgPrefs.m_pszUserSig = wfe_ReadUserSig(pszPref);
		}
		break;
	case 11:
		break;

	case 12:
		{
			for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext)
				f->PostMessage(WM_COMMAND, (WPARAM) ID_DONEGOINGOFFLINE, (LPARAM) 0);
		}
		break;
	case 13:
#ifdef _WIN32
		UpdateMapiDll();
#endif
		break;
	case 14:
		bPref= TRUE;
		PREF_GetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches", &bPref);
		g_MsgPrefs.m_bShowCompletionPicker = bPref;
		break;
	default:
		ASSERT(0);
	}

	XP_FREEIF(pszPref);

	return PREF_NOERROR;
}

void CMsgPrefs::Init()
{
	m_bInitialized = TRUE;

    m_pFolderTemplate = new CFolderTemplate(IDR_MAILFRAME,  
            RUNTIME_CLASS(CNetscapeDoc),
            RUNTIME_CLASS(CFolderFrame),     // mail window
            RUNTIME_CLASS(CFolderView));
	m_pThreadTemplate = new CThreadTemplate(IDR_MAILTHREAD,  
			RUNTIME_CLASS(CNetscapeDoc),
			RUNTIME_CLASS(C3PaneMailFrame),  // 3 pane mail window
			RUNTIME_CLASS(CMailNewsSplitter));
    m_pMsgTemplate = new CMessageTemplate(IDR_MESSAGEFRAME,  
            RUNTIME_CLASS(CNetscapeDoc),
            RUNTIME_CLASS(CMessageFrame),     // mail message window
            RUNTIME_CLASS(CMessageView));

    theApp.AddDocTemplate(m_pFolderTemplate);
    theApp.AddDocTemplate(m_pThreadTemplate);
    theApp.AddDocTemplate(m_pMsgTemplate);

	int32 prefInt = 0;
	char *prefStr = NULL;
	
    // Set up mail directory (must be before creation of msg prefs)
	CString msg = theApp.m_UserDirectory;
    msg += "\\mail";
	PREF_SetDefaultCharPref("mail.directory", msg);
	PREF_CopyCharPref("mail.directory", &prefStr);
	if (prefStr)
		m_csMailDir = prefStr;
	XP_FREEIF(prefStr);

	// Initialize the message library 
	MSG_InitMsgLib();

	// Create the MSG lib pref structure
	m_pMsgPrefs = MSG_CreatePrefs();

	//
	// Read in user identity stuff
	//

	PREF_CopyCharPref("mail.identity.username",&prefStr);
	m_csUsersFullName = prefStr;
	XP_FREEIF(prefStr);        
    
	PREF_CopyCharPref("mail.identity.useremail",&prefStr);
	m_csUsersEmailAddr = prefStr;
	XP_FREEIF(prefStr);        
    
	PREF_CopyCharPref("mail.identity.organization",&prefStr);
	m_csUsersOrganization = prefStr;
	XP_FREEIF(prefStr);        

	PREF_CopyCharPref("mail.signature_file", &prefStr);
    m_pszUserSig = wfe_ReadUserSig(prefStr);
	XP_FREEIF(prefStr);        

	//
	// Set up the POP3 server
	//

	PREF_CopyCharPref("network.hosts.pop_server", &prefStr);
	if (prefStr) {
		g_MsgPrefs.m_csMailHost = prefStr;
	}
	XP_FREEIF(prefStr);

	XP_Bool prefBool= FALSE;
	PREF_GetBoolPref("mail.remember_password",&prefBool);
	prefStr = NULL;
	PREF_CopyCharPref("mail.pop_password",&prefStr);
    if(prefStr && prefBool) {
		NET_SetPopPassword(prefStr);
	}
	XP_FREEIF(prefStr);

	//
	// Set up the SMTP server
	//

	PREF_CopyCharPref("network.hosts.smtp_server", &prefStr);
    NET_SetMailRelayHost(prefStr);
	XP_FREEIF(prefStr);    

	PREF_SetBoolPref("news.show_pretty_names", TRUE);

	//
	// Set up the NNTP server
	//

	PREF_CopyCharPref("network.hosts.nntp_server", &prefStr);
    NET_SetNewsHost(prefStr);
	m_csNewsHost = prefStr;
	XP_FREEIF(prefStr);

	// Set up news directory
	msg = theApp.m_UserDirectory;
    msg += "\\news";
	PREF_SetDefaultCharPref("news.directory",msg);
	PREF_CopyCharPref("news.directory",&prefStr);
	m_csNewsDir = prefStr;
	XP_FREEIF(prefStr);    

    // Check that the news directory exists
	// Don't show error messagebox according bug#37398
    if(FEU_SanityCheckDir(m_csNewsDir)) 
	{
		NET_ReadNewsrcFileMappings();
	}

	//
	// Compose prefs
	//

	prefBool = TRUE;
	PREF_GetBoolPref("mail.strictly_mime", &prefBool);
    MIME_ConformToStandard(prefBool); 

	//
	// Set up window prefs
	//
	prefBool= TRUE;
	PREF_GetBoolPref("mailnews.reuse_thread_window", &prefBool);
	m_bThreadReuse = prefBool;

	prefBool= TRUE;
	PREF_GetBoolPref("mailnews.reuse_message_window", &prefBool);
	m_bMessageReuse = prefBool;

	prefBool= TRUE;
	PREF_GetBoolPref("mailnews.message_in_thread_window", &prefBool);
	m_bThreadPaneMaxed = !prefBool;

	prefBool = TRUE;
	PREF_GetBoolPref("ldap_1.autoComplete.showDialogForMultipleMatches", &prefBool);
	m_bShowCompletionPicker = prefBool;

	PREF_RegisterCallback("mailnews.reuse_thread_window", cbMsgPrefs, (void *) 0);
	PREF_RegisterCallback("mailnews.reuse_message_window", cbMsgPrefs, (void *) 1);
	PREF_RegisterCallback("mailnews.message_in_thread_window", cbMsgPrefs, (void *) 2);
	// The BE should probably be handling most of these
	PREF_RegisterCallback("mail.directory", cbMsgPrefs, (void *) 3);
	PREF_RegisterCallback("network.hosts.pop_server", cbMsgPrefs, (void *) 4);
	PREF_RegisterCallback("news.directory", cbMsgPrefs, (void *) 5);
	PREF_RegisterCallback("network.hosts.nntp_server", cbMsgPrefs, (void *) 6);
	PREF_RegisterCallback("mail.identity.useremail", cbMsgPrefs, (void *) 7);
	PREF_RegisterCallback("mail.identity.username", cbMsgPrefs, (void *) 8);
	PREF_RegisterCallback("mail.identity.organization", cbMsgPrefs, (void *) 9);
	PREF_RegisterCallback("mail.signature_file", cbMsgPrefs, (void *) 10);
	PREF_RegisterCallback("mail.leave_on_server", cbMsgPrefs, (void *) 11);
	PREF_RegisterCallback("network.online", cbMsgPrefs, (void *) 12);
	PREF_RegisterCallback("mail.use_mapi_server", cbMsgPrefs, (void *) 13);
	//autocomplete
	PREF_RegisterCallback("ldap_1.autoComplete.showDialogForMultipleMatches", cbMsgPrefs, (void*) 14);

	// START THE BIFF CONTEXT
	CBiffCX *pCX = new CBiffCX();
    m_pBiffContext = pCX->GetContext();

    MSG_BiffInit(m_pBiffContext, m_pMsgPrefs);
}

void CMsgPrefs::Shutdown()
{
}

CMsgPrefs::~CMsgPrefs()
{
	XP_FREEIF(m_pszUserSig);
}

BOOL CMsgPrefs::IsValid() const
{
	char buffer[256];
	int nLen = 255;

	buffer[0] = '\0';

	long prefLong = MSG_Pop3;
	PREF_GetIntPref("mail.server_type",&prefLong);
	if (prefLong == MSG_Imap4 && 
		PREF_NOERROR == PREF_GetCharPref("network.hosts.imap_servers", buffer, &nLen))
	{
		if (strlen(buffer))
			return TRUE;
		else
			return FALSE;
	}
	else
	{
		if (PREF_NOERROR == PREF_GetCharPref("mail.pop_name", buffer, &nLen))
		{
			if (strlen(buffer) && !m_csMailHost.IsEmpty())
				return TRUE;
			else
				return FALSE;
		}
		else
			return FALSE;
	}
}

MSG_Master *CMsgPrefs::GetMaster()
{
	// Lazy loading of the one-and-only master object for news
	// * Must be lazy loaded so theApp.m_pMsgPrefs is initialized

	if (!m_pMaster)
	{
		if (!m_pMsgPrefs)
			m_pMsgPrefs = MSG_CreatePrefs();
		m_pMaster = MSG_InitializeMail(m_pMsgPrefs);
	}
	return m_pMaster;
}

MSG_Master *CMsgPrefs::GetMasterValue()
{
	return m_pMaster;
}

//////////////////////////////////////////////////////////////////////////////
// CMailNewsOutliner

CMailNewsOutliner::CMailNewsOutliner()
{
	m_iSelBlock = 0;
	m_bSelChanged = FALSE;
	m_bExpandOrCollapse = FALSE;
	m_pPane = NULL;
	m_iMysticPlane = 0;
	m_nCurrentSelected = -1;
}

CMailNewsOutliner::~CMailNewsOutliner()
{
}

void CMailNewsOutliner::SetPane(MSG_Pane *pane)
{
	m_pPane = pane;

	if (m_pPane) {
		SetTotalLines( (int) MSG_GetNumLines(m_pPane));
		Invalidate();
		UpdateWindow();
	}
}

const char *aszNotifications[] = {
	"None",
	"InsOrDel",
	"Changed",
	"Scramble",
	"All"
};
	 
void CMailNewsOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	TipHide();

	if ( 0 == m_iMysticPlane++  ) {
		m_bSelChanged = FALSE;
	}

	if ( notify == MSG_NotifyInsertOrDelete && m_bExpandOrCollapse ) {
	}
}

void CMailNewsOutliner::MysticStuffFinishing( XP_Bool asynchronous,
											 MSG_NOTIFY_CODE notify, 
											 MSG_ViewIndex where,
											 int32 num )
{
	switch ( notify ) {
	case MSG_NotifyNone:
		break;
	case MSG_NotifyAll:
		ClearSelection();
		m_iSelection = -1;
		m_iTopLine = 0;
		m_bSelChanged = TRUE;
		Invalidate();
		break;
	case MSG_NotifyInsertOrDelete:
		if ( m_bExpandOrCollapse && where > 0 && num < 0 ) {
			// We're collapsing, so if our primary selection is
			// in the collapsing area, we need to select the
			// parent
			// Assume that where - 1 is the parent

			if ( m_iSelection >= (int32) where && 
				 m_iSelection < (int32) (where - num) ) {
				m_iSelection = CASTINT(where - 1);
				m_iFocus = CASTINT(where - 1);
				AddSelection( m_iSelection );
				m_bSelChanged = TRUE;
			}
			if ( m_iShiftAnchor >= (int32) where &&
				 m_iShiftAnchor < (int32) (where - num ) ) {
				m_iShiftAnchor = CASTINT(where - 1);
			}
		}

		if ( num < 0 ) {
			m_bSelChanged |= HandleDelete( where, -num );
		} else if (num > 0) {
			m_bSelChanged |= HandleInsert( where, num );
		} else {
			m_bSelChanged = TRUE;
		}
		break;
	case MSG_NotifyChanged:
		InvalidateLines( (int) where, (int) num );
		break;
	case MSG_NotifyScramble:
		ScrollIntoView(m_iFocus);
		Invalidate();
		break;
	}

	if ( !--m_iMysticPlane && m_pPane ) {
		SetTotalLines( (int) MSG_GetNumLines( m_pPane ) );
		UpdateWindow();

		if (m_bSelChanged) {
			m_bSelChanged = FALSE;
			OnSelChanged();
		}
	}
}

int CMailNewsOutliner::ToggleExpansion( int iLine )
{
	int32 numChanged;

	m_bExpandOrCollapse = TRUE;
    MSG_ToggleExpansion( m_pPane, iLine, &numChanged );
	m_bExpandOrCollapse = FALSE;

	m_iLastSelected = m_iSelection;

	return (int) numChanged;
}

int CMailNewsOutliner::Expand( int iLine )
{
	if (MSG_ExpansionDelta( m_pPane, iLine ) > 0) {
	    return ToggleExpansion( iLine );
	}
	return 0;
}

int CMailNewsOutliner::Collapse( int iLine )
{
	if (MSG_ExpansionDelta( m_pPane, iLine ) < 0) {
	    return ToggleExpansion( iLine );
	}
	return 0;
}

BEGIN_MESSAGE_MAP( CMailNewsOutliner, CMSelectOutliner )
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
END_MESSAGE_MAP()

BOOL CMailNewsOutliner::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{

	if ( (nHitTest == HTCLIENT) && (m_iSelBlock > 0)) {
		::SetCursor( ::LoadCursor( NULL, IDC_WAIT ) );
		return TRUE;
	}

	return CMSelectOutliner::OnSetCursor( pWnd, nHitTest, message );
}

void CMailNewsOutliner::OnLButtonDown( UINT nFlags, CPoint point )
{
	if (m_iSelBlock <= 0)
		COutliner::OnLButtonDown(nFlags, point);
	else 
		MessageBeep((UINT) -1);
}

void CMailNewsOutliner::OnRButtonDown( UINT nFlags, CPoint point )
{
	if (m_iSelBlock <= 0)
		COutliner::OnRButtonDown(nFlags, point);
	else 
		MessageBeep((UINT) -1);
}

void CMailNewsOutliner::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	if (m_iSelBlock <= 0)
		COutliner::OnKeyDown(nChar, nRepCnt, nFlags);
	else 
		MessageBeep((UINT) -1);
}

void CMailNewsOutliner::OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    if (m_iSelection != m_iLastSelected) {
		if (m_iIndicesCount == 1) {
			COutliner::OnKeyUp(nChar, nRepCnt, nFlags);
		} else {
			int flags = 0;
			if (GetKeyState(VK_SHIFT)&0x8000)
				flags |= MK_SHIFT;
			if (GetKeyState(VK_CONTROL)&0x8000)
				flags |= MK_CONTROL;
			SelectItem(m_iSelection, OUTLINER_TIMER, flags);
		}
	}
}


//////////////////////////////////////////////////////////////////////////////
// CFolderOutliner

#define DOUBLE_CLICK_TIMER   9999

CFolderOutliner::CFolderOutliner ( )
{
	m_pUnkUserImage = NULL;
	ApiApiPtr(api);
	m_pUnkUserImage = api->CreateClassInstance(
	  APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_MAILNEWS);
	m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
	ASSERT(m_pIUserImage);
	if (!m_pIUserImage->GetResourceID())
	  m_pIUserImage->Initialize(IDB_MAILNEWS,16,16);

    m_pszExtraText = new _TCHAR[EXTRATEXT_SIZE];
	m_pMysticStuff = NULL;
	m_pAncestor = NULL;

	m_uTimer = 0;
	m_dwPrevTime = 0;
	m_bDoubleClicked = FALSE;
	m_b3PaneParent = FALSE;
	m_bRButtonDown = FALSE;
}

CFolderOutliner::~CFolderOutliner ( )
{
    if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

    delete [] m_pszExtraText;
	delete [] m_pMysticStuff;
}

void CFolderOutliner::MysticStuffStarting( XP_Bool asynchronous,
										  MSG_NOTIFY_CODE notify, 
										  MSG_ViewIndex where,
										  int32 num )
{
	CMailNewsOutliner::MysticStuffStarting( asynchronous, notify,
										   where, num );

	if ( notify == MSG_NotifyScramble && m_pPane ) {
		MSG_ViewIndex *indices;
		int i, count;
		GetSelection(indices, count);

		m_pMysticStuff = new MSG_FolderInfo*[count];

		for (i = 0; i < count; i++) {
			m_pMysticStuff[i] = MSG_GetFolderInfo(m_pPane, indices[i]);
		}

		m_MysticFocus = MSG_GetFolderInfo( m_pPane, m_iFocus );
		m_MysticSelection = MSG_GetFolderInfo( m_pPane, m_iSelection );
	}
}

void CFolderOutliner::MysticStuffFinishing( XP_Bool asynchronous,
										   MSG_NOTIFY_CODE notify, 
										   MSG_ViewIndex where,
										   int32 num )
{
	if ( notify == MSG_NotifyScramble && m_pPane ) {
		MSG_ViewIndex *indices;
		int i, count;
		GetSelection(indices, count);

		ClearSelection();

		for ( i = 0; i < count; i++ ) {
			int index = (int) MSG_GetFolderIndex(m_pPane, m_pMysticStuff[i]);
			if ( index != MSG_VIEWINDEXNONE ) {
				AddSelection(index);
			} else {
				m_bSelChanged = TRUE;
			}
		}

		delete [] m_pMysticStuff;
		m_pMysticStuff = NULL;

		if ( m_MysticSelection == NULL ) {
			m_iSelection = -1;
		} else {
			int iSelection = (int) MSG_GetFolderIndex(m_pPane, m_MysticSelection);
			if (iSelection == -1 ) {
				iSelection = GetTotalLines() > 0 ? 0 : -1;
				if (iSelection != -1)
					AddSelection( iSelection );
			}
			m_iSelection = iSelection;
		}

		if ( m_MysticFocus == NULL ) {
			m_iFocus = m_iSelection;
		} else {
			int iFocus = (int) MSG_GetFolderIndex(m_pPane, m_MysticFocus);
			if ( iFocus == -1 ) {
				iFocus = m_iSelection > 0 ? m_iSelection : 0;
			}
			m_iFocus = iFocus;
		}
		m_iShiftAnchor = m_iFocus;
	}

	CMailNewsOutliner::MysticStuffFinishing( asynchronous, notify,
											where, num );
}

// Tree Info stuff
int CFolderOutliner::GetDepth( int iLine )
{
	MSG_FolderLine folderLine;
	MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &folderLine );
	return folderLine.level - 1;
}

int CFolderOutliner::GetNumChildren( int iLine )
{
	MSG_FolderLine folderLine;
	MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &folderLine );
	return folderLine.numChildren;
}

BOOL CFolderOutliner::IsCollapsed( int iLine )
{
	MSG_FolderLine folderLine;
	MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &folderLine );
	return folderLine.flags & MSG_FOLDER_FLAG_ELIDED ? TRUE : FALSE;
}

void CFolderOutliner::PropertyMenu ( int iSel, UINT flags )	
{
	HMENU hmenu = CreatePopupMenu();
	
	if ( !hmenu )
		return;  

	MSG_ViewIndex *indices;
	int count;
	GetSelection( indices, count );

    if ( iSel < m_iTotalLines && count == 1 ) {
		MSG_FolderLine folderLine;
		MSG_GetFolderLineByIndex( m_pPane, indices[0], 1, &folderLine );

		BOOL bReuse = g_MsgPrefs.m_bThreadReuse;
		if (GetKeyState(VK_MENU) & 0x8000)
			bReuse = !bReuse;

		if ( folderLine.flags & MSG_FOLDER_FLAG_MAIL ) { 
			if (folderLine.level > 1) {
				if (bReuse) {
					::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERREUSE, szLoadString( IDS_POPUP_OPENFOLDER ) );
					::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERNEW, szLoadString( IDS_POPUP_OPENFOLDERNEWWINDOW ) );
				} else {
					::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERNEW, szLoadString( IDS_POPUP_OPENFOLDERNEWWINDOW ) );
					::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERREUSE, szLoadString( IDS_POPUP_OPENFOLDER ) );
				}
				::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
				 
				::AppendMenu( hmenu, MF_STRING, ID_FILE_NEWFOLDER, szLoadString( IDS_POPUP_NEWSUBFOLDER ) );
				::AppendMenu( hmenu, MF_STRING, ID_EDIT_DELETEFOLDER, szLoadString( IDS_POPUP_DELETEFOLDER ) );
				::AppendMenu( hmenu, MF_STRING, ID_FOLDER_RENAME, szLoadString( IDS_POPUP_RENAMEFOLDER ) );
				::AppendMenu( hmenu, MF_STRING, ID_FILE_COMPRESSTHISFOLDER, szLoadString( IDS_POPUP_COMPRESSFOLDER ) );
				::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
			 
				::AppendMenu( hmenu, MF_STRING, ID_FILE_NEWMESSAGE, szLoadString( IDS_POPUP_NEWMESSAGE ) );
				::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
			 
				::AppendMenu( hmenu, MF_STRING, ID_EDIT_SEARCH, szLoadString( IDS_POPUP_SEARCH ) );
				::AppendMenu( hmenu, MF_STRING, ID_VIEW_PROPERTIES, szLoadString( IDS_POPUP_FOLDERPROP ) );
			} else {
				::AppendMenu( hmenu, MF_STRING, ID_FILE_NEWFOLDER, szLoadString( IDS_POPUP_NEWFOLDER ) );
				::AppendMenu( hmenu, MF_STRING, ID_FILE_UPDATECOUNTS, szLoadString( IDS_POPUP_UPDATEMESSAGECOUNT ) );
				::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
			 
				::AppendMenu( hmenu, MF_STRING, ID_EDIT_SEARCH, szLoadString( IDS_POPUP_SEARCH ) );
				::AppendMenu( hmenu, MF_STRING, ID_VIEW_PROPERTIES, szLoadString( IDS_POPUP_MAILSERVERPROP) );
			}
		} else if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ) {
			BOOL bReuse = g_MsgPrefs.m_bThreadReuse;
			if (bReuse) {
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERREUSE, szLoadString( IDS_POPUP_OPENNEWSGROUP ) );
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERNEW, szLoadString( IDS_POPUP_OPENNEWSGROUPNEWWINDOW ) );
			} else {
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERNEW, szLoadString( IDS_POPUP_OPENNEWSGROUPNEWWINDOW ) );
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENFOLDERREUSE, szLoadString( IDS_POPUP_OPENNEWSGROUP ) );
			}
			::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
		 
			::AppendMenu( hmenu, MF_STRING, ID_FILE_NEWMESSAGE, szLoadString( IDS_POPUP_NEWMESSAGE ) );
			::AppendMenu( hmenu, MF_STRING, ID_MESSAGE_MARKALLREAD, szLoadString( IDS_POPUP_MARKNEWSGROUPREAD ) );
			::AppendMenu( hmenu, MF_STRING, ID_POPUP_DELETEFOLDER, szLoadString( IDS_POPUP_REMOVENEWSGROUP ) );
			::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
		 
			::AppendMenu( hmenu, MF_STRING, ID_EDIT_SEARCH, szLoadString( IDS_POPUP_SEARCH ) );
			::AppendMenu( hmenu, MF_STRING, ID_VIEW_PROPERTIES, szLoadString( IDS_POPUP_NEWSGROUPPROP ) );
		} else if ( folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST ) {
			::AppendMenu( hmenu, MF_STRING, ID_FILE_SUBSCRIBE, szLoadString( IDS_POPUP_JOINNEWSGROUP ) );
			::AppendMenu( hmenu, MF_STRING, ID_FILE_UPDATECOUNTS, szLoadString( IDS_POPUP_UPDATEMESSAGECOUNT ) );
			::AppendMenu( hmenu, MF_STRING, ID_POPUP_DELETEFOLDER, szLoadString( IDS_POPUP_REMOVENEWSHOST ) );
			::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
		 
			::AppendMenu( hmenu, MF_STRING, ID_EDIT_SEARCH, szLoadString( IDS_POPUP_SEARCH ) );
			::AppendMenu( hmenu, MF_STRING, ID_VIEW_PROPERTIES, szLoadString( IDS_POPUP_NEWSHOSTPROP) );
		}
	}
	//	Track the popup now.
	POINT pt = m_ptHit;
	ClientToScreen(&pt);

	::TrackPopupMenu( hmenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0,
					  GetParentFrame()->GetSafeHwnd(), NULL);

	VERIFY(::DestroyMenu( hmenu ));
}

DROPEFFECT CFolderOutliner::DropSelect( int iLineNo, COleDataObject *pDataObject )
{
	DROPEFFECT res = DROPEFFECT_NONE;
	MSG_DragEffect requireEffect = MSG_Default_Drag;
	MSG_DragEffect effect = MSG_Drag_Not_Allowed;
	HGLOBAL hContent = NULL;
	MailNewsDragData *pDragData = NULL;

	if ((GetKeyState(VK_SHIFT) & 0x8000) && (GetKeyState(VK_CONTROL) & 0x8000))
		requireEffect = MSG_Default_Drag;
	else if (GetKeyState(VK_SHIFT) & 0x8000)
		requireEffect = MSG_Require_Move;
	else if (GetKeyState(VK_CONTROL) & 0x8000)
		requireEffect = MSG_Require_Copy;

// The commented out code has the effect of always doing a move, if we get called
// with the same iLineNo as last time!
//	if (iLineNo != m_iDragSelection)
	{
		if ( m_iDragSelection != -1 )
			InvalidateLine (m_iDragSelection);

		m_iDragSelection = -1;

		MSG_FolderLine folderLine;

		if (MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLineNo, 1, &folderLine )) 
		{
			if ( pDataObject->IsDataAvailable( m_cfMessages ) ) 
			{
				hContent = pDataObject->GetGlobalData (m_cfMessages);
				pDragData = (MailNewsDragData *) GlobalLock(hContent);
				effect = MSG_DragMessagesIntoFolderStatus
						(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
						folderLine.id, requireEffect);
				GlobalUnlock (hContent); //do we have to GlobalFree it too?
			}
			else if ( pDataObject->IsDataAvailable( m_cfFolders ) ) 
			{
				hContent = pDataObject->GetGlobalData (m_cfFolders);
				pDragData = (MailNewsDragData *) GlobalLock(hContent);
				effect = MSG_DragFoldersIntoStatus
					(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
					folderLine.id, requireEffect);
				GlobalUnlock (hContent); //do we have to GlobalFree it too?
			}
			else if ( pDataObject->IsDataAvailable( m_cfSearchMessages ) ) 
			{
				hContent = pDataObject->GetGlobalData (m_cfSearchMessages);
				pDragData = (MailNewsDragData *) GlobalLock(hContent);
				effect = MSG_DragMessagesIntoFolderStatus
					(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
					folderLine.id, requireEffect);
				GlobalUnlock (hContent); //do we have to GlobalFree it too?
			}
			if (MSG_Drag_Not_Allowed != effect) 
			{
				InvalidateLine (iLineNo);
				m_iDragSelection = iLineNo;
				if (effect == MSG_Require_Copy)
					res = DROPEFFECT_COPY;
				else
					res = DROPEFFECT_MOVE;
			} 
		}
	}
//	else  
//		res = DROPEFFECT_MOVE;

	return res;
}

void CFolderOutliner::AcceptDrop( int iLineNo, COleDataObject *pDataObject,
								 DROPEFFECT dropEffect )
{
    if (iLineNo != -1) {
		MSG_FolderInfo *pFolder = MSG_GetFolderInfo(m_pPane,iLineNo);
		ASSERT(pFolder);

		MSG_DragEffect requireEffect = MSG_Default_Drag;
		if (dropEffect & DROPEFFECT_MOVE)
			requireEffect = MSG_Require_Move;
		else if (dropEffect & DROPEFFECT_COPY)
			requireEffect = MSG_Require_Copy;

		if ( pDataObject->IsDataAvailable( m_cfMessages ) && pFolder ) 
		{
			HGLOBAL hContent = pDataObject->GetGlobalData (m_cfMessages);
			MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock(hContent);

			MSG_DragEffect effect =  MSG_DragMessagesIntoFolderStatus
					(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
					pFolder, requireEffect);

			LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pDragData->m_pane );
			if (pUnk) {
				LPMSGLIST pInterface = NULL;
				pUnk->QueryInterface( IID_IMsgList, (LPVOID *) &pInterface );
				if ( pInterface ) {
					switch (effect) {
					case MSG_Require_Move:
						pInterface->MoveMessagesInto( pDragData->m_pane, pDragData->m_indices, 
													  pDragData->m_count, pFolder);
						break;
					case MSG_Require_Copy:
						pInterface->CopyMessagesInto( pDragData->m_pane, pDragData->m_indices, 
													  pDragData->m_count, pFolder);
						break;
					default:
						ASSERT(0);
						break;
					}
					pInterface->Release();
				}
			}
			GlobalUnlock (hContent); //do we have to GlobalFree it too?
		}

		if ( pDataObject->IsDataAvailable( m_cfSearchMessages ) && pFolder ) 
		{
			HGLOBAL hContent = pDataObject->GetGlobalData (m_cfSearchMessages);
			MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock(hContent);

			MSG_DragEffect effect =  MSG_DragMessagesIntoFolderStatus
					(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
					pFolder, requireEffect);

			LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pDragData->m_pane );
			if (pUnk) {
				LPMSGLIST pInterface = NULL;
				pUnk->QueryInterface( IID_IMsgList, (LPVOID *) &pInterface );
				if ( pInterface ) {
					switch (effect) {
					case MSG_Require_Move:
						pInterface->MoveMessagesInto( pDragData->m_pane, pDragData->m_indices, 
													  pDragData->m_count, pFolder);
						break;
					case MSG_Require_Copy:
						pInterface->CopyMessagesInto( pDragData->m_pane, pDragData->m_indices, 
													  pDragData->m_count, pFolder);
						break;
					default:
						ASSERT(0);
						break;
					}
					pInterface->Release();
				}
			}
			GlobalUnlock (hContent); //do we have to GlobalFree it too?
		}


 		if ( pDataObject->IsDataAvailable( m_cfFolders ) && pFolder ) 
		{
			HGLOBAL hContent = pDataObject->GetGlobalData (m_cfFolders);
			MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock(hContent);

			MSG_DragEffect effect = MSG_DragFoldersIntoStatus
					(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
					pFolder, MSG_Default_Drag);

			if (effect == MSG_Require_Copy || effect == MSG_Require_Move)
			{
				MSG_MoveFoldersInto( pDragData->m_pane, pDragData->m_indices, 
									 pDragData->m_count, pFolder);
				if (IsParent3PaneFrame())
				{
					C3PaneMailFrame* pFrame = C3PaneMailFrame::FindFrame(pFolder);
					if (pFrame) 
						pFrame->ActivateFrame();
					((C3PaneMailFrame*)GetParentFrame())->BlockFolderSelection (TRUE);
				}
			}
			GlobalUnlock (hContent); //do we have to GlobalFree it too?
		}
	}
}

COleDataSource * CFolderOutliner::GetDataSource(void)
{
	MSG_ViewIndex *indices;
	int count;
	GetSelection(indices, count);

    HANDLE hContent = GlobalAlloc(GMEM_ZEROINIT|GMEM_MOVEABLE|GMEM_DDESHARE,sizeof(MailNewsDragData));
	MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock (hContent);

	pDragData->m_pane = m_pPane;
	pDragData->m_indices = indices;
	pDragData->m_count = count;

    GlobalUnlock(hContent);

    COleDataSource * pDataSource = new COleDataSource;  
    pDataSource->CacheGlobalData(m_cfFolders,hContent);

	if ( count == 1 ) {
		MSG_FolderLine folderLine;
		MSG_GetFolderLineByIndex( m_pPane, indices[0], 1, &folderLine );
		URL_Struct *url = MSG_ConstructUrlForFolder( m_pPane, folderLine.id );

		if ( url ) {			
			const char *name = (folderLine.prettyName && folderLine.prettyName[0]) ?
							   folderLine.prettyName : folderLine.name;
			RDFGLOBAL_DragTitleAndURL( pDataSource, name, url->address );
			NET_FreeURLStruct( url );
		}
	}

    return pDataSource;
}

void CFolderOutliner::InitializeClipFormats(void)
{
    m_cfMessages = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT);
	m_cfFolders = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_FOLDER_FORMAT);
	m_cfSearchMessages = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_SEARCH_FORMAT);
}

CLIPFORMAT * CFolderOutliner::GetClipFormatList(void)
{
    static CLIPFORMAT cfFormatList[4];
    cfFormatList[0] = m_cfMessages;
	cfFormatList[1] = m_cfFolders;
    cfFormatList[2] = m_cfSearchMessages;
	cfFormatList[3] = 	0;
    return cfFormatList;
}

int CFolderOutliner::TranslateIcon (void * pData)
{
    ASSERT(pData);
    MSG_FolderLine * pFolder = (MSG_FolderLine*)pData;

	BOOL bOpen = pFolder->numChildren <= 0 ||
				 pFolder->flags & MSG_FOLDER_FLAG_ELIDED ? FALSE : TRUE;

	return WFE_MSGTranslateFolderIcon( pFolder->level, pFolder->flags, bOpen );
}

int CFolderOutliner::TranslateIconFolder (void * pData)
{
    ASSERT(pData);
    MSG_FolderLine * pFolder = (MSG_FolderLine*)pData;

	if (pFolder->numChildren > 0) {
		if (pFolder->flags & MSG_FOLDER_FLAG_ELIDED) {
            return(OUTLINER_CLOSEDFOLDER);
		} else {
            return(OUTLINER_OPENFOLDER);
		}
	}
    return (OUTLINER_ITEM);
}

void * CFolderOutliner::AcquireLineData ( int line )
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;

    m_pszExtraText[ 0 ] = '\0';
	if ( line >= m_iTotalLines)
		return NULL;
	if ( !MSG_GetFolderLineByIndex(m_pPane, line, 1, &m_FolderLine ))
        return NULL;

	return &m_FolderLine;
}

void CFolderOutliner::GetTreeInfo ( int line, uint32 * pFlags, int * pDepth, 
									OutlinerAncestorInfo ** pAncestor )
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;

	if (pAncestor) {
		if ( m_FolderLine.level > 0 ) {
			m_pAncestor = new OutlinerAncestorInfo[m_FolderLine.level];
		} else {
			m_pAncestor = new OutlinerAncestorInfo[1];
		}

		int i = m_FolderLine.level - 1;
		int idx = line + 1;
		while ( i > 0 ) {
			int level;
			if ( idx < m_iTotalLines ) {
				level = MSG_GetFolderLevelByIndex( m_pPane, idx );
				if ( (level - 1) == i ) {
					m_pAncestor[i].has_prev = TRUE;
					m_pAncestor[i].has_next = TRUE;
					i--;
					idx++;
				} else if ( (level - 1) < i ) {
					m_pAncestor[i].has_prev = FALSE;
					m_pAncestor[i].has_next = FALSE;
					i--;
				} else {
					idx++;
				}
			} else {
				m_pAncestor[i].has_prev = FALSE;
				m_pAncestor[i].has_next = FALSE;
				i--;
			}
		}
		m_pAncestor[0].has_prev = FALSE;
		m_pAncestor[0].has_next = FALSE;

        *pAncestor = m_pAncestor;
	}

    if ( pFlags ) *pFlags = m_FolderLine.flags;
    if ( pDepth ) *pDepth = m_FolderLine.level - 1;
}

void CFolderOutliner::ReleaseLineData ( void * )
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;
}

int CFolderOutliner::OnCreate(LPCREATESTRUCT lpcs)
{
	m_b3PaneParent = GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame));
	
	return CMailNewsOutliner::OnCreate(lpcs);
}

void CFolderOutliner::OnDestroy()
{
	if (m_uTimer)
	{
		KillTimer(m_uTimer);
		m_uTimer = 0;
	}
	CMailNewsOutliner::OnDestroy();
}

void CFolderOutliner::OnTimer(UINT nIDEvent)
{
	if (m_uTimer == nIDEvent)
	{
		if (m_bDoubleClicked)
		{
			m_bDoubleClicked = FALSE;
			KillTimer(m_uTimer);
			m_uTimer = 0;
		}
		else
		{
			UINT delta = GetTickCount() - m_dwPrevTime;
			if (delta > GetDoubleClickTime())
			{	// single click
				m_bSelChanged = FALSE;
				if (!m_iSelBlock)
					GetParentFrame()->SendMessage(WM_COMMAND, 
												ID_FOLDER_SELECT, 0);
				KillTimer(m_uTimer);
				m_uTimer = 0;
			}
		}
		m_bLButtonDown = FALSE;
	}
	else
		CMailNewsOutliner::OnTimer(nIDEvent);
}

void CFolderOutliner::OnSelChanged()
{
	if (IsParent3PaneFrame())
	{
		if (m_bRButtonDown)
		{
			((C3PaneMailFrame*)GetParentFrame())->BlankOutRightPanes();
			return;
		}
		SetCurrentSelected(m_iLastSelected);
		BOOL bControlKeydown = ((GetKeyState(VK_CONTROL) & 0x8000) && m_bLButtonDown);
		if (!bControlKeydown && m_bLButtonDown && m_uTimer == 0 && !m_bDoubleClicked)
		{
			m_dwPrevTime = GetTickCount();
			UINT uElapse = GetDoubleClickTime() / 5;
			m_uTimer = SetTimer(DOUBLE_CLICK_TIMER, uElapse, NULL);
			return;
		} 
	}
 	m_bSelChanged = FALSE;
	if (!m_iSelBlock)
		GetParentFrame()->SendMessage(WM_COMMAND, ID_FOLDER_SELECT, 0);
}

void CFolderOutliner::OnSelDblClk()
{
	if (m_bLButtonDown && m_uTimer != 0)
		m_bDoubleClicked = TRUE;

	MSG_FolderLine folderLine;
	if (MSG_GetFolderLineByIndex(m_pPane, m_iFocus, 1, &folderLine)) {
		if (folderLine.level < 2) {
			DoToggleExpansion(m_iFocus);
		}
	}
	GetParentFrame()->SendMessage(WM_COMMAND, ID_FILE_OPENFOLDER, 0);
}

HFONT CFolderOutliner::GetLineFont ( void *pLineData )
{
	MSG_FolderLine *pFolderLine = (MSG_FolderLine *) pLineData;

	if (((pFolderLine->deepUnseen > 0) && (pFolderLine->flags & MSG_FOLDER_FLAG_ELIDED)) ||
		(pFolderLine->unseen > 0) ||
		(pFolderLine->level < 2) )
		return m_hBoldFont;
	else
		return m_hRegFont;
}

LPCTSTR CFolderOutliner::GetColumnText( UINT iColumn, void * pLineData )
{
	m_pszExtraText[0] = '\0';
	LPCTSTR name;

    switch ( iColumn )
    {
        case ID_COLUMN_FOLDER:
			name = (LPCTSTR) m_FolderLine.prettyName;
			if ( !name || !name[0] )
				name = m_FolderLine.name;
            return name;

        case ID_COLUMN_UNREAD:
            if( m_FolderLine.unseen > 0 )
				sprintf( m_pszExtraText, "%ld", m_FolderLine.unseen );
            else if (m_FolderLine.unseen < 0)
                sprintf( m_pszExtraText, "???" );
            return m_pszExtraText;
            
        case ID_COLUMN_COUNT:
            if( m_FolderLine.total > 0 )
				sprintf( m_pszExtraText, "%ld", m_FolderLine.total );
            else if(m_FolderLine.total < 0)
                sprintf( m_pszExtraText, "???" );
            return m_pszExtraText;

    }
    return m_pszExtraText;
}

LPCTSTR CFolderOutliner::GetColumnTip( UINT iColumn, void * pLineData )
{
	switch ( iColumn ) {
	case ID_COLUMN_UNREAD:
		if ( m_FolderLine.deepUnseen != m_FolderLine.unseen ) {
            if( m_FolderLine.unseen > 0 ) { 
				sprintf( m_pszExtraText, "(%ld) %ld", 
						 m_FolderLine.deepUnseen, m_FolderLine.unseen );
				return m_pszExtraText;
			} else if (m_FolderLine.unseen == 0) {
                sprintf( m_pszExtraText, "(%ld)", m_FolderLine.deepUnseen );
				return m_pszExtraText;
			}
		}
		break;

	case ID_COLUMN_COUNT:
		if ( m_FolderLine.deepTotal != m_FolderLine.total ) {
            if( m_FolderLine.total > 0 ) { 
				sprintf( m_pszExtraText, "(%ld) %ld", 
						 m_FolderLine.deepTotal, m_FolderLine.total );
				return m_pszExtraText;
			} else if (m_FolderLine.total == 0) {
                sprintf( m_pszExtraText, "(%ld)", m_FolderLine.deepTotal );
				return m_pszExtraText;
			}
		}
		break;
	}
	return NULL;
}

void CFolderOutliner::OnSetFocus(CWnd* pOldWnd)
{
	C3PaneMailFrame* pParent = (C3PaneMailFrame*)GetParentFrame();
	CMailNewsOutliner::OnSetFocus(pOldWnd);
	if (IsParent3PaneFrame())
		pParent->SetFocusWindow(this);
}

void CFolderOutliner::OnRButtonDown (UINT nFlags, CPoint point)
{
	m_bRButtonDown = TRUE;
	CMailNewsOutliner::OnRButtonDown (nFlags, point);
}

void CFolderOutliner::OnRButtonUp(UINT nFlags, CPoint point)
{
	CMailNewsOutliner::OnRButtonUp(nFlags, point);
	m_bRButtonDown = FALSE;

	if (IsParent3PaneFrame())
		((C3PaneMailFrame*)GetParentFrame())->CheckForChangeFocus();
}

BEGIN_MESSAGE_MAP(CFolderOutliner, CMailNewsOutliner)
	ON_WM_CREATE()
	ON_WM_TIMER()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
	ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////
// CMessageOutliner


CMessageOutliner::CMessageOutliner ( )
{
    m_pszExtraText = new _TCHAR[EXTRATEXT_SIZE];
	m_pAncestor = NULL;
	m_bNews = FALSE;
	m_bDrafts = FALSE;

	m_uTimer = 0;
	m_dwPrevTime = 0;
	m_bDoubleClicked = FALSE;

	m_pUnkUserImage = NULL;
	ApiApiPtr(api);
	m_pUnkUserImage = api->CreateClassInstance(
	  APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_MAILNEWS);
	m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
	ASSERT(m_pIUserImage);
	if (!m_pIUserImage->GetResourceID())
	  m_pIUserImage->Initialize(IDB_MAILNEWS,16,16);

	m_pMysticStuff = NULL;
}

CMessageOutliner::~CMessageOutliner ( )
{
    if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

    delete [] m_pszExtraText;
	delete [] m_pMysticStuff;
}

void CMessageOutliner::MysticStuffStarting( XP_Bool asynchronous,
										   MSG_NOTIFY_CODE notify, 
										   MSG_ViewIndex where,
										   int32 num )
{
	CMailNewsOutliner::MysticStuffStarting( asynchronous, notify,
										   where, num);
	if ( notify == MSG_NotifyScramble ) {
		MSG_ViewIndex *indices;
		int i, count;
		GetSelection(indices, count);

		m_pMysticStuff = new MessageKey[count];

		for (i = 0; i < count; i++) {
			m_pMysticStuff[i] = MSG_GetMessageKey(m_pPane, indices[i]);
		}

		m_MysticFocus = MSG_MESSAGEKEYNONE;
		m_MysticSelection = MSG_MESSAGEKEYNONE;

		if ( m_iSelection != -1 && m_iSelection < m_iTotalLines) {
			m_MysticSelection = MSG_GetMessageKey( m_pPane, m_iSelection );
		}
		if ( m_iFocus != -1 && m_iFocus < m_iTotalLines) {
			m_MysticFocus = MSG_GetMessageKey( m_pPane, m_iFocus );
		}
	}
}

void CMessageOutliner::MysticStuffFinishing( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	if ( notify == MSG_NotifyScramble ) {
		MSG_ViewIndex *indices;
		int i, count;
		GetSelection(indices, count);

		ClearSelection();

		for ( i = 0; i < count; i++ ) {
			MSG_ViewIndex index = MSG_GetMessageIndexForKey(m_pPane, m_pMysticStuff[i], TRUE);
			if ( index != MSG_VIEWINDEXNONE ) {
				AddSelection(index);
			} else {
				m_bSelChanged = TRUE;
			}
		}

		delete [] m_pMysticStuff;
		m_pMysticStuff = NULL;

		if ( m_MysticSelection == MSG_MESSAGEKEYNONE ) {
			m_iSelection = -1;
		} else {
			int iSelection = CASTINT(MSG_GetMessageIndexForKey(m_pPane, m_MysticSelection, TRUE));
			if (iSelection == -1 ) {
				iSelection = MSG_GetNumLines( m_pPane ) > 0 ? 0 : -1;
				if (iSelection != -1)
					AddSelection( iSelection );
			}
			m_iSelection = iSelection;
		}

		if ( m_MysticFocus == MSG_MESSAGEKEYNONE ) {
			m_iFocus = m_iSelection;
		} else {
			int iFocus = CASTINT(MSG_GetMessageIndexForKey(m_pPane, m_MysticFocus, TRUE));
			if ( iFocus == -1 ) {
				iFocus = m_iSelection > 0 ? m_iSelection : 0;
			}
			m_iFocus = iFocus;
		}
		m_iShiftAnchor = m_iFocus;
	}

	if ( notify == MSG_NotifyAll ) {
		MSG_FolderInfo *folderInfo = MSG_GetCurFolder( m_pPane );
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( WFE_MSGGetMaster(),
							   folderInfo, &folderLine );
		if ( m_bNews ) {
			SaveXPPrefs("news.thread_columns_win");
		} else {
			SaveXPPrefs("mail.thread_columns_win");
		}

		m_bNews = folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ? TRUE : FALSE;
		m_bDrafts = folderLine.flags & MSG_FOLDER_FLAG_DRAFTS ? TRUE : FALSE;

		if( m_bNews ) {
			LoadXPPrefs("news.thread_columns_win");
		} else {
			LoadXPPrefs("mail.thread_columns_win");
		}
	}

	CMailNewsOutliner::MysticStuffFinishing( asynchronous, notify,
											where, num);
}

// Tree Info stuff
int CMessageOutliner::GetDepth( int iLine )
{
	MSG_MessageLine messageLine;
	MSG_GetThreadLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &messageLine );
	return messageLine.level - 1;
}

int CMessageOutliner::GetNumChildren( int iLine )
{
	MSG_MessageLine messageLine;
	MSG_GetThreadLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &messageLine );
	return messageLine.numChildren;
}

int CMessageOutliner::GetParentIndex( int iLine )
{
	MessageKey key = MSG_GetMessageKey( m_pPane, (MSG_ViewIndex) iLine );
	return CASTINT(MSG_ThreadIndexOfMsg( m_pPane, key ));
}

BOOL CMessageOutliner::IsCollapsed( int iLine )
{
	MSG_MessageLine messageLine;
	MSG_GetThreadLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &messageLine );
	return messageLine.flags & MSG_FLAG_ELIDED ? TRUE : FALSE;
}

void CMessageOutliner::SelectAllMessages()
{
	for (int i = GetTotalLines() - 1; i >= 0; i--) 
		Expand(i);
}

int CMessageOutliner::ExpandAll( int iLine )
{
	MSG_ViewIndex viewIndex = (MSG_ViewIndex) iLine;
	int32 numLines = GetTotalLines();

	MSG_Command(m_pPane, MSG_ExpandAll, &viewIndex, 1);
	ScrollIntoView(m_iFocus);

	return GetTotalLines() - numLines;
}

int CMessageOutliner::CollapseAll( int iLine )
{
	MSG_ViewIndex viewIndex = (MSG_ViewIndex) iLine;
	int32 numLines = GetTotalLines();

	MSG_Command(m_pPane, MSG_CollapseAll, &viewIndex, 1);
	ScrollIntoView(m_iFocus);

	return numLines - GetTotalLines();
}

void CMessageOutliner::SelectThread( int iLine, UINT flags )
{
	if ( MSG_GetToggleStatus( m_pPane, MSG_SortByThread, NULL, 0) != MSG_Checked )
		return;

	MysticStuffStarting( FALSE, MSG_NotifyNone, 0, 0 );
	theApp.DoWaitCursor(1);

	m_bSelChanged = TRUE;

	MessageKey *keys;
	MSG_ViewIndex *indices;
	int i, count;
	if ( iLine >= 0 ) {
		if (!(flags & MK_CONTROL )) {
			ClearSelection();
		}

		InvalidateLine(m_iFocus);
		m_iFocus = m_iSelection = iLine;
		InvalidateLine(m_iFocus);

		if ( flags & MK_SHIFT ) {
			GetSelection( indices, count );
			if ( count <= 0 )
				return;

			keys = new MessageKey[count];

			for ( i = 0; i < count; i++ ) {
				keys[ i ] = MSG_GetMessageKey( m_pPane, indices[ i ] );
			}
		} else {
			count = 1;
			keys = new MessageKey[1];
			keys[0] = MSG_GetMessageKey( m_pPane, (MSG_ViewIndex) iLine );
		}
	} else {
		GetSelection( indices, count );
		if ( count <= 0 )
			return;

		keys = new MessageKey[count];

		for ( i = 0; i < count; i++ ) {
			keys[ i ] = MSG_GetMessageKey( m_pPane, indices[ i ] );
		}
		ClearSelection();
	}

	for ( i = 0; i < count; i++ ) {
		MSG_ViewIndex index = MSG_ThreadIndexOfMsg( m_pPane, keys[ i ] );
		int delta = CASTINT(MSG_ExpansionDelta( m_pPane, index ));
		if ( delta > 0) {
			MSG_ToggleExpansion( m_pPane, index, NULL );
		} else {
			delta = -delta;
		}	
		SelectRange( index, index + delta );
	}

	delete [] keys;

	theApp.DoWaitCursor(-1);
	MysticStuffFinishing( FALSE, MSG_NotifyNone, 0, 0 );
}

void CMessageOutliner::SelectFlagged()
{
	MSG_MessageLine messageLine;

	ClearSelection();
	for (int i = 0; i < m_iTotalLines; i++ ) {
		if ( MSG_GetThreadLineByIndex( m_pPane, i, 1, &messageLine ) ) {
			if ( messageLine.flags & MSG_FLAG_MARKED ) {
				AddSelection(i);
			}
		}
	}
	OnSelChanged();
}

void CMessageOutliner::EnsureFlagsVisible()
{
	int iPos = GetColumnPos(ID_COLUMN_MARK);
	int iVis = CASTINT(GetVisibleColumns());
	if (iPos >= iVis) {
		SetColumnPos(ID_COLUMN_MARK, iVis);
		SetVisibleColumns(CASTUINT(iVis + 1));

		RECT rc;
		GetClientRect(&rc);
		OnSize(0, rc.right, rc.bottom);
		Invalidate();
		GetParent()->Invalidate();
	}
}

void CMessageOutliner::PropertyMenu ( int iSel, UINT flags )	
{
    HMENU hmenu = CreatePopupMenu();

	if ( !hmenu )
		return;  

	if ( iSel < m_iTotalLines ) {
		if (!m_bDrafts) {
			BOOL bReuse = g_MsgPrefs.m_bMessageReuse;
			if (GetKeyState(VK_MENU) & 0x8000)
				bReuse = !bReuse;

			if (bReuse) {
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENMESSAGEREUSE, szLoadString( IDS_POPUP_OPENMESSAGE ) );
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENMESSAGENEW, szLoadString( IDS_POPUP_OPENMESSAGENEWWINDOW ) );
			} else {
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENMESSAGENEW, szLoadString( IDS_POPUP_OPENMESSAGENEWWINDOW ) );
				::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENMESSAGEREUSE, szLoadString( IDS_POPUP_OPENMESSAGE ) );
			}
		} else {
			::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENMESSAGE, szLoadString( IDS_POPUP_OPENMESSAGE ) );
		}
		::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
		//
		WFE_MSGBuildMessagePopup( hmenu, m_bNews, TRUE, MSG_GetContext(m_pPane) );
    }

    //	Track the popup now.
    POINT pt = m_ptHit;
    ClientToScreen(&pt);

   	::TrackPopupMenu( hmenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
					  GetParentFrame()->GetSafeHwnd(), NULL);

    //  Cleanup
    VERIFY(::DestroyMenu( hmenu ));
}

COleDataSource * CMessageOutliner::GetDataSource(void)
{
	MSG_ViewIndex *indices;
	int count;

	GetSelection( indices, count );

    HANDLE hContent = GlobalAlloc(GMEM_ZEROINIT|GMEM_MOVEABLE|GMEM_DDESHARE,sizeof(MailNewsDragData));
	MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock (hContent);
	pDragData->m_pane = m_pPane;
	pDragData->m_indices = indices;
	pDragData->m_count = count;
    GlobalUnlock(hContent);

    COleDataSource * pDataSource = new COleDataSource;  
    pDataSource->CacheGlobalData(m_cfMessages,hContent);

	if ( count == 1 ) {
		MSG_MessageLine messageLine;
		MSG_GetThreadLineByIndex( m_pPane, indices[0], 1, &messageLine );
		URL_Struct *url = MSG_ConstructUrlForMessage( m_pPane, messageLine.messageKey );

		if ( url ) {			
			RDFGLOBAL_DragTitleAndURL( pDataSource, messageLine.subject, url->address );
			NET_FreeURLStruct( url );
		}
	}

	return pDataSource;
}

void CMessageOutliner::InitializeClipFormats(void)
{
    m_cfMessages = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT);
}

CLIPFORMAT * CMessageOutliner::GetClipFormatList(void)
{
    static CLIPFORMAT cfFormatList[2];
    cfFormatList[0] = m_cfMessages;
    cfFormatList[1] = 0;

    return cfFormatList;
}

DROPEFFECT CMessageOutliner::DropSelect( int iLineNo, COleDataObject *pDataObject )
{
	DROPEFFECT res = DROPEFFECT_NONE;

	if ( !m_bNews && pDataObject->IsDataAvailable( m_cfMessages ) )  
	{
		MSG_FolderInfo *pFolderInfo = MSG_GetCurFolder( m_pPane );
		HGLOBAL hContent = pDataObject->GetGlobalData (m_cfMessages);
		MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock(hContent);

		MSG_DragEffect requireEffect = MSG_Default_Drag;
		if ((GetKeyState(VK_SHIFT) & 0x8000) && (GetKeyState(VK_CONTROL) & 0x8000))
			requireEffect = MSG_Default_Drag;
		else if (GetKeyState(VK_SHIFT) & 0x8000)
			requireEffect = MSG_Require_Move;
		else if (GetKeyState(VK_CONTROL) & 0x8000)
			requireEffect = MSG_Require_Copy;

		MSG_DragEffect effect = MSG_DragMessagesIntoFolderStatus
				(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
				pFolderInfo, requireEffect);

		if (effect == MSG_Require_Copy)
			res = DROPEFFECT_COPY;
		else
			res = DROPEFFECT_MOVE;

		GlobalUnlock (hContent); //do we have to GlobalFree it too?
	}   
	return res;
}

void CMessageOutliner::AcceptDrop( int iLineNo, COleDataObject *pDataObject,
								   DROPEFFECT dropEffect )
{
    if ( !m_bNews ) {
		MSG_FolderInfo *pFolder = MSG_GetCurFolder( m_pPane );
		ASSERT(pFolder);

		if ( pDataObject->IsDataAvailable( m_cfMessages ) && pFolder ) {
			HGLOBAL hContent = pDataObject->GetGlobalData (m_cfMessages);
			MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock(hContent);
			
			MSG_DragEffect requireEffect = MSG_Default_Drag;
			if (dropEffect & DROPEFFECT_MOVE)
				requireEffect = MSG_Require_Move;
			else if (dropEffect & DROPEFFECT_COPY)
				requireEffect = MSG_Require_Copy;

			MSG_DragEffect effect = MSG_DragMessagesIntoFolderStatus
					(pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
					pFolder, requireEffect);
			//  need to code this way to make it work 
			LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pDragData->m_pane );
			if (pUnk) {
				LPMSGLIST pInterface = NULL;
				pUnk->QueryInterface( IID_IMsgList, (LPVOID *) &pInterface );
				if ( pInterface ) {
					switch (effect ) {
					case MSG_Require_Move:
						pInterface->MoveMessagesInto( pDragData->m_pane, pDragData->m_indices, 
													  pDragData->m_count, pFolder);
						break;
					case MSG_Require_Copy:
						pInterface->CopyMessagesInto( pDragData->m_pane, pDragData->m_indices, 
													  pDragData->m_count, pFolder);
						break;
					default: //do nothing
						break;
					}
				}
			}  
			GlobalUnlock(hContent); //do we have to GlobalFree it too?
		}
	}
}

BOOL CMessageOutliner::RenderData( UINT iColumn, CRect &rect, CDC &dc, LPCTSTR text )
{
    switch ( iColumn )
    {
	case ID_COLUMN_THREAD:
		if ((m_MessageLine.level == 0 && m_MessageLine.numChildren > 0) ||
			(m_MessageLine.flags & (MSG_FLAG_WATCHED|MSG_FLAG_IGNORED))) {
			int idxImage[] = { IDX_THREADCLOSED, 
								IDX_THREADOPEN,
								IDX_THREADCLOSEDNEW,
								IDX_THREADOPENNEW,
								IDX_THREADWATCHED,
								IDX_THREADWATCHED,
								IDX_THREADWATCHEDNEW,
								IDX_THREADWATCHEDNEW,
								IDX_THREADIGNOREDCLOSED,
								IDX_THREADIGNOREDOPEN,
								IDX_THREADIGNOREDCLOSED,
								IDX_THREADIGNOREDOPEN };
			int idx = 0;
			idx += m_MessageLine.flags & MSG_FLAG_ELIDED ? 0 : 1;
			idx += m_MessageLine.numNewChildren ? 2 : 0;
			idx += m_MessageLine.flags & MSG_FLAG_WATCHED ? 4 : 0;
			idx += m_MessageLine.flags & MSG_FLAG_IGNORED ? 8 : 0;

            m_pIUserImage->DrawImage ( idxImage[idx],
                rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
		}
		return TRUE;

	case ID_COLUMN_MARK:
		{
			int idxImage = m_MessageLine.flags & MSG_FLAG_MARKED ?
						   IDX_MARKED : IDX_READ;
            m_pIUserImage->DrawImage ( idxImage,
									   rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), 
									   rect.top, &dc, FALSE );
		}
		return TRUE;

    case ID_COLUMN_READ:
        if (!(m_MessageLine.flags & MSG_FLAG_EXPIRED)) {
            int idxImage;
            if (m_MessageLine.flags & MSG_FLAG_READ)
                idxImage = IDX_READ;
            else
                idxImage = IDX_UNREAD;

            m_pIUserImage->DrawImage ( idxImage,
                rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );

        }
        return TRUE;  
		
	case ID_COLUMN_PRIORITY: 
		{
			CRect rectText(rect);
			rectText.left += COL_LEFT_MARGIN;
			rectText.right -= COL_LEFT_MARGIN;

			COLORREF colorPriority[] = { RGB(0x00,0x00,0x00),
										  RGB(0x00,0x00,0x00), 
										  RGB(0xaa,0xaa,0xaa),
										  RGB(0x55,0x55,0x55),
										  RGB(0x00,0x00,0x00),
										  RGB(0x80,0x00,0x00),
										  RGB(0xff,0x00,0x00) };

			UINT dwDTFormat = DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
			COLORREF colorOld = dc.GetTextColor();

			if ( colorOld != GetSysColor( COLOR_HIGHLIGHTTEXT )) { 
				dc.SetTextColor( colorPriority[ m_MessageLine.priority ] );
			}

			DrawColumnText( dc.m_hDC, rectText, text, CropRight, AlignLeft );

			dc.SetTextColor( colorOld );
			return TRUE;
		}
		break;
    }
    return FALSE;
}

int CMessageOutliner::TranslateIcon (void * pData)
{
	int messageTable[] = { IDX_MAILMESSAGE,	   IDX_MAILNEW,		   IDX_MAILREAD,
						   IDX_MAILMSGOFFLINE, IDX_MAILMSGOFFLINE, IDX_MAILREADOFFLINE,
						   IDX_NEWSARTICLE,	   IDX_NEWSNEW,		   IDX_NEWSREAD,
						   IDX_NEWSMSGOFFLINE, IDX_NEWSMSGOFFLINE, IDX_NEWSREADOFFLINE,
						   IDX_DRAFT,		   IDX_DRAFT,		   IDX_DRAFT,
						   IDX_DRAFT,		   IDX_DRAFT,		   IDX_DRAFT };
    ASSERT(pData);
    MSG_MessageLine * pMessage = (MSG_MessageLine *)pData;

	if (pMessage->flags & MSG_FLAG_IMAP_DELETED)
		return IDX_IMAPMSGDELETED;
	if (pMessage->flags & MSG_FLAG_ATTACHMENT)
		return IDX_ATTACHMENTMAIL;

	int idx = 0;

	if (m_bNews)
		idx = 6;
	else if (m_bDrafts)
		idx = 12;

	if ( pMessage->flags & MSG_FLAG_OFFLINE )
		idx += 3;

	if ( pMessage->flags & MSG_FLAG_READ )
		idx += 2;
	else if ( pMessage->flags & MSG_FLAG_NEW )
		idx += 1;

	return messageTable[idx];
}

int CMessageOutliner::TranslateIconFolder (void * pData)
{
    ASSERT(pData);
    MSG_MessageLine * pMessage = (MSG_MessageLine *)pData;
    
	if (pMessage->numChildren > 0) {
		if (pMessage->flags & MSG_FLAG_ELIDED) {
            return ( OUTLINER_CLOSEDFOLDER );
        } else {
            return ( OUTLINER_OPENFOLDER );
		}
	} else {
		return (OUTLINER_ITEM);
    }
}


BOOL CMessageOutliner::ColumnCommand ( int iColumn, int iLine )
{
	MSG_ViewIndex indices[1];
	indices[0] = iLine;
	
	switch ( iColumn )
    {
	case ID_COLUMN_THREAD:
		if ( MSG_GetToggleStatus( m_pPane, MSG_SortByThread, NULL, 0) == MSG_Checked ) {
			SelectThread( iLine );
			SetCapture();//~~~
			return TRUE;
		} else {
			return FALSE;
		}

    case ID_COLUMN_READ:
        if (!(m_MessageLine.flags & MSG_FLAG_EXPIRED)) 
            MSG_Command(m_pPane, MSG_ToggleMessageRead, indices, 1);
        return TRUE;
	case ID_COLUMN_MARK:
		{
			MSG_MessageLine messageLine;
			MSG_ViewIndex index = iLine;

			MSG_GetThreadLineByIndex( m_pPane, index, 1, &messageLine );

			MSG_Command( m_pPane, 
						 ( messageLine.flags & MSG_FLAG_MARKED) ? 
						 MSG_UnmarkMessages : MSG_MarkMessages,
						 &index, 1 );

			return TRUE;
		}
    }

    return FALSE;
}

void * CMessageOutliner::AcquireLineData ( int line )
{
	if ( line >= m_iTotalLines)
		return NULL;

	if ( !MSG_GetThreadLineByIndex(m_pPane, line, 1, &m_MessageLine ) )
		return NULL;

	return &m_MessageLine; 
}

void CMessageOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, 
									 OutlinerAncestorInfo ** pAncestor )
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;

	if ( pAncestor ) {
		m_pAncestor = new OutlinerAncestorInfo[m_MessageLine.level + 1];

		int i = m_MessageLine.level;
		int idx = iLine + 1;
		while ( i > 0 ) {
			if ( idx < m_iTotalLines ) {
				int level = MSG_GetThreadLevelByIndex( m_pPane, idx );
				if ( level == i ) {
					m_pAncestor[i].has_prev = TRUE;
					m_pAncestor[i].has_next = TRUE;
					i--;
					idx++;
				} else if ( level < i ) {
					m_pAncestor[i].has_prev = FALSE;
					m_pAncestor[i].has_next = FALSE;
					i--;
				} else {
					idx++;
				}
			} else {
				m_pAncestor[i].has_prev = FALSE;
				m_pAncestor[i].has_next = FALSE;
				i--;
			}
		}
		m_pAncestor[0].has_prev = FALSE;
		m_pAncestor[0].has_next = FALSE;
        *pAncestor = m_pAncestor;
	}

    if ( pFlags ) *pFlags = m_MessageLine.flags;
    if ( pDepth ) *pDepth = m_MessageLine.level;
}

HFONT CMessageOutliner::GetLineFont ( void * )
{
    if ( m_MessageLine.flags & MSG_FLAG_EXPIRED )
        return m_hRegFont;

    return (!(m_MessageLine.flags & MSG_FLAG_READ) ? m_hBoldFont : m_hRegFont);
}

void CMessageOutliner::ReleaseLineData ( void * )
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;
}

LPCTSTR CMessageOutliner::GetColumnText ( UINT iColumn, void * pLineData )
{
	LPTSTR buf;   // temporary buffer for mail header conversion (I18N)
    m_pszExtraText[ 0 ] = _T('\0');
	m_pszExtraText[ EXTRATEXT_SIZE - 1 ] = _T('\0');

    switch ( iColumn )
    {
        case ID_COLUMN_FROM:
			{
				buf = IntlDecodeMimePartIIStr(m_MessageLine.author, INTL_DocToWinCharSetID(m_iCSID), FALSE);
				if (buf)
				{
					_tcsncpy(m_pszExtraText, buf, EXTRATEXT_SIZE - 1);
					XP_FREE(buf);
					return m_pszExtraText;
				}
				return m_MessageLine.author;
			}
        case ID_COLUMN_SUBJECT:
		  	{ 
				CString cs = _T("");
            	if ( m_MessageLine.flags & MSG_FLAG_HAS_RE ) {
                    cs.LoadString(IDS_MAIL_RE);
				}
                _tcscpy ( m_pszExtraText, cs );
				buf = IntlDecodeMimePartIIStr(m_MessageLine.subject, INTL_DocToWinCharSetID(m_iCSID), FALSE);
				if (buf) {
                	_tcsncat ( m_pszExtraText, buf, 
							   EXTRATEXT_SIZE - cs.GetLength() - 1);
					XP_FREE(buf);
				} else {
                	_tcsncat ( m_pszExtraText, m_MessageLine.subject,
							   EXTRATEXT_SIZE - cs.GetLength() - 1);
				}
				return m_pszExtraText;
			}
        case ID_COLUMN_DATE:
            {
                if ( m_MessageLine.flags & MSG_FLAG_EXPIRED )
                    return _T("");
                _tcscpy ( m_pszExtraText, MSG_FormatDate(m_pPane,m_MessageLine.date));
                return m_pszExtraText;
            }
		case ID_COLUMN_PRIORITY:
			if ( m_MessageLine.priority != MSG_NoPriority && 
				 m_MessageLine.priority != MSG_PriorityNotSet &&
				 m_MessageLine.priority != MSG_NormalPriority) {
				MSG_GetPriorityName( m_MessageLine.priority, m_pszExtraText, MSG_FLAG_EXPIRED);
			}
			return m_pszExtraText;
		case ID_COLUMN_STATUS:
			if (m_MessageLine.flags & MSG_FLAG_NEW) {
				MSG_GetStatusName( MSG_FLAG_NEW,
								   m_pszExtraText, EXTRATEXT_SIZE);
				return m_pszExtraText;
			}
			if ( m_MessageLine.flags & MSG_FLAG_REPLIED &&
				 m_MessageLine.flags & MSG_FLAG_FORWARDED ) {
				MSG_GetStatusName( MSG_FLAG_REPLIED|MSG_FLAG_FORWARDED,
								   m_pszExtraText, EXTRATEXT_SIZE);
				return m_pszExtraText;
			}
			else if ( m_MessageLine.flags & MSG_FLAG_FORWARDED ) {
				MSG_GetStatusName( MSG_FLAG_FORWARDED,
								   m_pszExtraText, EXTRATEXT_SIZE);
				return m_pszExtraText;
			}
			else if ( m_MessageLine.flags & MSG_FLAG_REPLIED ) {
				MSG_GetStatusName( MSG_FLAG_REPLIED,
								   m_pszExtraText, EXTRATEXT_SIZE);
				return m_pszExtraText;
			}
			
			break;
		case ID_COLUMN_LENGTH:
			if (m_bNews) {
				_stprintf( m_pszExtraText, _T("%d"), (int) m_MessageLine.messageLines );
			} else {
				_TCHAR cSuffix = _T(' ');
				int32 iSize = m_MessageLine.messageLines;
				if ( iSize > 1L<<30 ) {
					iSize /= 1L<<30;
					cSuffix = _T('G');
				} else if ( iSize > 1L<<20 ) {
					iSize /= 1L<<20;
					cSuffix = _T('M');
				} else if ( iSize > 1L<<10 ) {
					iSize /= 1L<<10;
					cSuffix = _T('K');
				} else {
					iSize = 1;
					cSuffix = _T('K');
				}
				_stprintf( m_pszExtraText, _T("%d%cB"), (int) iSize, cSuffix );
			}
			return m_pszExtraText;

		case ID_COLUMN_UNREAD:
			if (m_MessageLine.numNewChildren) {
				_stprintf( m_pszExtraText, _T("%d"), (int) m_MessageLine.numNewChildren );
				return m_pszExtraText;
			}
			break;
		case ID_COLUMN_COUNT:
			if (m_MessageLine.numChildren) {
				_stprintf( m_pszExtraText, _T("%d"), (int) m_MessageLine.numChildren + 1 );
				return m_pszExtraText;
			}
			break;
    }
    return ("");
}

void CMessageOutliner::OnDestroy()
{
	if (m_uTimer)
	{
		KillTimer(m_uTimer);
		m_uTimer = 0;
	}
	CMailNewsOutliner::OnDestroy();
}

void CMessageOutliner::OnTimer(UINT nIDEvent)
{
	if (m_uTimer == nIDEvent)
	{
		if (m_bDoubleClicked)
		{
			m_bDoubleClicked = FALSE;
			KillTimer(m_uTimer);
			m_uTimer = 0;
		}
		else
		{
			UINT delta = GetTickCount() - m_dwPrevTime;
			if (delta > GetDoubleClickTime())
			{	// single click
				m_bSelChanged = FALSE;
				if (GetCurrentSelected() != m_iSelection)
				{
					if (!m_iSelBlock)
						GetParentFrame()->PostMessage(WM_COMMAND, ID_MESSAGE_SELECT, 0);
				}
				KillTimer(m_uTimer);
				m_uTimer = 0;
			}
		}
		m_bLButtonDown = FALSE;
	}
	else
		CMailNewsOutliner::OnTimer(nIDEvent);
}

void CMessageOutliner::OnSelChanged()
{
	if (((C3PaneMailFrame*)GetParentFrame())->MessageViewClosed())
	{
		m_bSelChanged = FALSE;   
		if (!m_iSelBlock)
			GetParentFrame()->PostMessage(WM_COMMAND, ID_MESSAGE_SELECT, 0);
	}
	else
	{
		BOOL bControlKeydown = ((GetKeyState(VK_CONTROL) & 0x8000) && m_bLButtonDown);
		if (!bControlKeydown)
			SetCurrentSelected(m_iLastSelected);
		if (!bControlKeydown && m_bLButtonDown && m_uTimer == 0 && !m_bDoubleClicked)
		{
			m_dwPrevTime = GetTickCount();
			UINT uElapse = GetDoubleClickTime() / 5;
			m_uTimer = SetTimer(DOUBLE_CLICK_TIMER, uElapse, NULL);
		}
		else
		{
			if (!m_bDoubleClicked)
			{
				m_bSelChanged = FALSE;   
				if (!m_iSelBlock)
					GetParentFrame()->PostMessage(WM_COMMAND, ID_MESSAGE_SELECT, 0);
			}
		}
	}
}

void CMessageOutliner::OnSelDblClk()
{
	if (m_bLButtonDown && m_uTimer != 0)
		m_bDoubleClicked = TRUE;

	GetParentFrame()->PostMessage(WM_COMMAND, ID_FILE_OPENMESSAGE, 0);
}

void CMessageOutliner::OnSetFocus(CWnd* pOldWnd)
{
	C3PaneMailFrame* pParent = (C3PaneMailFrame*)GetParentFrame();
	CMailNewsOutliner::OnSetFocus(pOldWnd);
	pParent->SetFocusWindow(this);
}

BEGIN_MESSAGE_MAP(CMessageOutliner, CMailNewsOutliner)
	ON_WM_TIMER()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFolderOutlinerParent

CFolderOutlinerParent::CFolderOutlinerParent()
{
	m_pUnkUserImage = NULL;
	ApiApiPtr(api);
	m_pUnkUserImage = api->CreateClassInstance(
		APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_MAILCOL);
	m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
	ASSERT(m_pIUserImage);
	if (!m_pIUserImage->GetResourceID())
		m_pIUserImage->Initialize(IDB_MAILCOL,16,16);
}

CFolderOutlinerParent::~CFolderOutlinerParent()
{
    if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }
}

COutliner * CFolderOutlinerParent::GetOutliner ( void )
{
    return new CFolderOutliner;
}

typedef struct COLUMNDESC {
	UINT idCol;
	UINT idTitle;
	int nWidth;
	BOOL bButton;
    Column_t styleColumn;
    CropType_t styleCrop;
	AlignType_t styleAlign;
} ColumnDesc_t;

static ColumnDesc_t aidFolderColumns[] = 
	{ { ID_COLUMN_FOLDER, IDS_MAIL_NAME,   6000, FALSE, ColumnVariable, CropRight, AlignLeft },
	  { ID_COLUMN_UNREAD, IDS_MAIL_UNREAD, 2000, FALSE, ColumnVariable, CropRight, AlignRight },
	  { ID_COLUMN_COUNT,  IDS_MAIL_TOTAL,  2000, FALSE, ColumnVariable, CropRight, AlignRight } };

void CFolderOutlinerParent::CreateColumns ( void )
{
	ColumnDesc_t *pColumns;
	int nColumns;
	int i;

	nColumns = sizeof( aidFolderColumns ) / sizeof ( ColumnDesc_t );
	pColumns = aidFolderColumns;

    CString cs;
	
	for ( i = 0; i < nColumns; i++ ) {
		int nMin = pColumns[i].styleColumn == ColumnFixed ? 20 : pColumns[i].nWidth;
		int nDefault = pColumns[i].nWidth;

		if ( pColumns[i].idTitle ) {
			cs.LoadString(pColumns[i].idTitle);
		} else {
			cs = _T("");
		}
		m_pOutliner->AddColumn ( cs, 
								 pColumns[i].idCol, 
								 nMin, 0, 
								 pColumns[i].styleColumn,
								 nDefault, 
								 pColumns[i].bButton, 
								 pColumns[i].styleCrop,
								 pColumns[i].styleAlign );
	}

	m_pOutliner->SetImageColumn( ID_COLUMN_FOLDER );

	if (((CFolderOutliner*)m_pOutliner)->IsParent3PaneFrame())
		m_pOutliner->LoadXPPrefs("mailnews.3Pane_folder_columns_win");
	else
		m_pOutliner->LoadXPPrefs("mailnews.folder_columns_win");
}

BEGIN_MESSAGE_MAP(CFolderOutlinerParent,COutlinerParent)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CFolderOutlinerParent::OnDestroy()
{
	if (((CFolderOutliner*)m_pOutliner)->IsParent3PaneFrame())
		m_pOutliner->SaveXPPrefs("mailnews.3Pane_folder_columns_win");
	else
		m_pOutliner->SaveXPPrefs( "mailnews.folder_columns_win" );

	COutlinerParent::OnDestroy();
}

/////////////////////////////////////////////////////////////////////////////
// CMessageOutlinerParent

CMessageOutlinerParent::CMessageOutlinerParent()
{
	m_pUnkUserImage = NULL;
	ApiApiPtr(api);
	m_pUnkUserImage = api->CreateClassInstance(
		APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_MAILCOL);
	m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
	ASSERT(m_pIUserImage);
	if (!m_pIUserImage->GetResourceID())
		m_pIUserImage->Initialize(IDB_MAILCOL,16,16);
}

CMessageOutlinerParent::~CMessageOutlinerParent()
{
    if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }
}

BOOL CMessageOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, LPCTSTR text )
{
	MSG_Pane *pPane = ((CMailNewsOutliner *) m_pOutliner)->GetPane();
	ASSERT(pPane);
	MSG_COMMAND_CHECK_STATE sortType = MSG_Unchecked;

    switch (idColumn) {
        case ID_COLUMN_READ:
		case ID_COLUMN_THREAD:
		case ID_COLUMN_MARK:
			{
				int idxImage;
				switch (idColumn) {
				case ID_COLUMN_READ:
					idxImage = IDX_MAILUNREAD;
					break;
				case ID_COLUMN_THREAD:
					if (MSG_GetToggleStatus(pPane, MSG_SortByThread, NULL, 0) == MSG_Checked) {
						idxImage = IDX_THREAD;
					} else {
						idxImage = IDX_UNTHREAD;
					}
					break;
				case ID_COLUMN_MARK:
					idxImage = IDX_MAILMARKED;
					break;
				}
				m_pIUserImage->DrawTransImage( idxImage,
											   (rect.left + rect.right + 1) / 2 - 8,
											   (rect.top + rect.bottom + 1) / 2 - 8, &dc );
			}
		return TRUE;
        case ID_COLUMN_DATE:
            sortType = MSG_GetToggleStatus(pPane, MSG_SortByDate, NULL, 0);
            break;
        case ID_COLUMN_SUBJECT:
            sortType = MSG_GetToggleStatus(pPane, MSG_SortBySubject, NULL, 0);
            break;
        case ID_COLUMN_FROM:
            sortType = MSG_GetToggleStatus(pPane, MSG_SortBySender, NULL, 0);
            break;
		case ID_COLUMN_PRIORITY:
            sortType = MSG_GetToggleStatus(pPane, MSG_SortByPriority, NULL, 0);
            break;
		case ID_COLUMN_STATUS:
			sortType = MSG_GetToggleStatus(pPane, MSG_SortByStatus, NULL, 0);
			break;
		case ID_COLUMN_LENGTH:
			sortType = MSG_GetToggleStatus(pPane, MSG_SortBySize, NULL, 0);
			break;
		default:
			return FALSE;
    }

	UINT dwDTFormat = DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;


	CString csTitle = text;
	if ( MSG_DisplayingRecipients( pPane ) && idColumn == ID_COLUMN_FROM ) {
		csTitle.LoadString( IDS_MAIL_RECIPIENT );
	}
	if ( idColumn == ID_COLUMN_LENGTH ) {
		dwDTFormat |= DT_RIGHT;
		if (!((CMessageOutliner *)m_pOutliner)->IsNews())
			csTitle.LoadString( IDS_SIZE );
	}

	RECT rectText = rect;
	rectText.left += COL_LEFT_MARGIN;
	rectText.right -= COL_LEFT_MARGIN + 1;

    if (sortType == MSG_Checked) {
		rectText.right -= 14;
		int idxImage;
		if (MSG_GetToggleStatus( pPane,MSG_SortBackward, NULL, 0) == MSG_Checked )
			idxImage = IDX_SORTINDICATORUP;
		else
			idxImage = IDX_SORTINDICATORDOWN;
        m_pIImage->DrawTransImage( idxImage,
								   rectText.right + 4,
								   (rect.top + rect.bottom) / 2 - 4,
								   &dc );
	}

	WFE_DrawTextEx( 0, dc.m_hDC, (LPTSTR) (LPCTSTR) csTitle, -1, 
					&rectText, dwDTFormat, WFE_DT_CROPRIGHT );

    return TRUE;
}

COutliner * CMessageOutlinerParent::GetOutliner ( void )
{
    return new CMessageOutliner;
}

static ColumnDesc_t aidMessageColumns[] = 
	{ { ID_COLUMN_THREAD,	0,				   24,   TRUE, ColumnFixed,    CropRight, AlignLeft },
	  { ID_COLUMN_SUBJECT,	IDS_MAIL_SUBJECT,  2500, TRUE, ColumnVariable, CropRight, AlignLeft },
	  { ID_COLUMN_READ,		0,				   24,   TRUE, ColumnFixed,	   CropRight, AlignLeft },
	  { ID_COLUMN_FROM,		IDS_MAIL_SENDER,   2500, TRUE, ColumnVariable, CropRight, AlignLeft },
	  { ID_COLUMN_DATE,		IDS_MAIL_DATE,	   1500, TRUE, ColumnVariable, CropRight, AlignLeft },
	  { ID_COLUMN_PRIORITY, IDS_MAIL_PRIORITY, 1000, TRUE, ColumnVariable, CropRight, AlignLeft },
	  { ID_COLUMN_MARK,		0,				   24,   TRUE, ColumnFixed,	   CropRight, AlignLeft },
	  { ID_COLUMN_STATUS,	IDS_MAIL_STATUS,   750,  TRUE, ColumnVariable, CropRight, AlignLeft },
	  { ID_COLUMN_LENGTH,	IDS_MAIL_LENGTH,   750,  TRUE, ColumnVariable, CropRight, AlignRight },
	  { ID_COLUMN_UNREAD,	IDS_MAIL_UNREAD,   500,  FALSE, ColumnVariable, CropRight, AlignRight },
	  { ID_COLUMN_COUNT,	IDS_MAIL_TOTAL,    500,  FALSE, ColumnVariable, CropRight, AlignRight } };

void CMessageOutlinerParent::CreateColumns ( void )
{
	ColumnDesc_t *pColumns;
	int nColumns;
	int i;

	nColumns = sizeof( aidMessageColumns ) / sizeof ( ColumnDesc_t );
	pColumns = aidMessageColumns;

    CString cs;
	
	for ( i = 0; i < nColumns; i++ ) {
		int nMin = pColumns[i].styleColumn == ColumnFixed ? 20 : pColumns[i].nWidth;
		int nDefault = pColumns[i].nWidth;

		if ( pColumns[i].idTitle ) {
			cs.LoadString(pColumns[i].idTitle);
		} else {
			cs = _T("");
		}
		m_pOutliner->AddColumn ( cs, 
								 pColumns[i].idCol, 
								 nMin, 0, 
								 pColumns[i].styleColumn,
								 nDefault, 
								 pColumns[i].bButton, 
								 pColumns[i].styleCrop,
								 pColumns[i].styleAlign );
	}

	m_pOutliner->SetVisibleColumns( 6 );
	m_pOutliner->SetImageColumn( ID_COLUMN_SUBJECT );

    if ( ((CMessageOutliner *) m_pOutliner)->IsNews() ) {
		m_pOutliner->LoadXPPrefs("news.thread_columns_win");
	} else {
		m_pOutliner->LoadXPPrefs("mail.thread_columns_win");
	}
 }

BOOL CMessageOutlinerParent::ColumnCommand ( int nColumn )
{
	UINT iCommand = 0;

    switch ( nColumn ) {
    case ID_COLUMN_FROM:
		iCommand = ID_SORT_BYSENDER;
        break;

    case ID_COLUMN_SUBJECT:
		iCommand = ID_SORT_BYSUBJECT;
        break;

    case ID_COLUMN_DATE:
		iCommand = ID_SORT_BYDATE;
        break;

	case ID_COLUMN_THREAD:
		iCommand = ID_SORT_BYTHREAD;
		break;

	case ID_COLUMN_PRIORITY:
		iCommand = ID_SORT_BYPRIORITY;
		break;

	case ID_COLUMN_STATUS:
		iCommand = ID_SORT_BYSTATUS;
		break;

	case ID_COLUMN_LENGTH:
		iCommand = ID_SORT_BYSIZE;
		break;

	case ID_COLUMN_MARK:
		iCommand = ID_SORT_BYFLAG;
		break;

	case ID_COLUMN_READ:
		iCommand = ID_SORT_BYUNREAD;
		break;
    }

	if ( iCommand ) {
	    SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );
		GetParentFrame()->SendMessage(WM_COMMAND, iCommand, 0 );
	    SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
	}

    return 0;
}

BEGIN_MESSAGE_MAP(CMessageOutlinerParent,COutlinerParent)
    ON_WM_DESTROY()
END_MESSAGE_MAP()


void CMessageOutlinerParent::OnDestroy()
{
    if ( ((CMessageOutliner *) m_pOutliner)->IsNews() )
		m_pOutliner->SaveXPPrefs("news.thread_columns_win");
    else
		m_pOutliner->SaveXPPrefs("mail.thread_columns_win");

	COutlinerParent::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////////
// CMarkReadDateDlg

BOOL CMarkReadDateDlg::OnInitDialog( )
{
	BOOL res = CDialog::OnInitDialog();

	wndDateTo.SubclassDlgItem(IDC_STATIC1, this);

	CTime timeTmp = CTime::GetCurrentTime();
	wndDateTo.SetDate( timeTmp );

	return res;
}

void CMarkReadDateDlg::OnOK()
{
	wndDateTo.GetDate(dateTo);
	CDialog::OnOK();
}


/////////////////////////////////////////////////////////////////////////////
// C function shared by CMailFolderHelper and preference

extern "C" void HelperInitFonts( HDC hdc , HFONT *phFont, HFONT *phBoldFont)
{
	uint16 csid = INTL_CharSetNameToID(INTL_ResourceCharSet());

    LOGFONT lf;							   
    memset(&lf,0,sizeof(LOGFONT));

    lf.lfPitchAndFamily = FF_SWISS;
	lf.lfCharSet = IntlGetLfCharset( csid );
	if ( csid == CS_LATIN1 )
 		_tcscpy( lf.lfFaceName, "MS Sans Serif" );
	else
 		_tcscpy( lf.lfFaceName, IntlGetUIPropFaceName( csid ) );
   	lf.lfHeight = -MulDiv( 9, ::GetDeviceCaps( hdc, LOGPIXELSY ), 72);
	*phFont = theApp.CreateAppFont( lf );
    lf.lfWeight = 700;
	*phBoldFont = theApp.CreateAppFont( lf );
}

extern "C" void HelperDoMeasureItem( HWND hWnd, LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
	lpMeasureItemStruct->itemHeight = 16;
}

extern "C" void HelperDoDrawItem
( HWND hWnd, LPDRAWITEMSTRUCT lpDrawItemStruct, HFONT *phFont, HFONT *phBoldFont,
  MSG_Master *pMaster, BOOL bStaticCtl, int iInitialDepth, LPIMAGEMAP pIImageMap, 
  BOOL bNoPrettyName)
{
	HDC hDC = lpDrawItemStruct->hDC;
	RECT rcItem = lpDrawItemStruct->rcItem;
	RECT rcTemp = rcItem;
	RECT rcText;
	DWORD dwItemData = lpDrawItemStruct->itemData;
	HBRUSH hBrushWindow = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
	HBRUSH hBrushHigh = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
	HBRUSH hBrushFill = NULL;

	if ( !*phFont ) {
		HelperInitFonts( hDC, phFont, phBoldFont);
	}

	HFONT hOldFont = NULL;
	if ( *phFont ) {
		hOldFont = (HFONT) ::SelectObject( hDC, *phFont );
	}

	if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
		hBrushFill = hBrushHigh;
		::SetBkColor( hDC, GetSysColor( COLOR_HIGHLIGHT ) );
		::SetTextColor( hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
	} else {
		hBrushFill = hBrushWindow;
		::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
	}

	VERIFY(::FillRect( hDC, &rcItem, hBrushWindow ));

	if ( lpDrawItemStruct->itemID != -1 &&  dwItemData ) {
		MSG_FolderInfo *folderInfo = (MSG_FolderInfo *) dwItemData;
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( pMaster, folderInfo, &folderLine);

		int idxImage = WFE_MSGTranslateFolderIcon( folderLine.level,
												   folderLine.flags, 
												   folderLine.numChildren );

		if ( *phBoldFont && folderLine.unseen > 0 ) {
			::SelectObject( hDC, *phBoldFont );
		}
		int iIndent = 4;

		BOOL bStatic = FALSE;
#ifdef _WIN32
	if ( sysInfo.m_bWin4 )
		bStatic = ( lpDrawItemStruct->itemState & ODS_COMBOBOXEDIT ) ? TRUE : FALSE;
	else 
#endif
		bStatic = bStaticCtl;

		if (!bStatic)
			iIndent += (folderLine.level - iInitialDepth) * 8;

        pIImageMap->DrawImage( idxImage, iIndent, rcItem.top, hDC, FALSE );

		LPCTSTR name;
		if (bNoPrettyName)
			name = folderLine.name;
		else
		{
			name = (LPCTSTR) folderLine.prettyName;
			if ( !name || !name[0] )
				name = folderLine.name;
		}

		::DrawText( hDC, name, -1, &rcTemp, DT_SINGLELINE|DT_CALCRECT|DT_NOPREFIX );
		int iWidth = rcTemp.right - rcTemp.left;

		rcTemp = rcItem;
		rcText = rcItem;
		rcTemp.left = iIndent + 20;
		rcTemp.right = rcTemp.left + iWidth + 4;
		if (rcTemp.right > rcItem.right)
			rcTemp.right = rcItem.right;

		VERIFY(::FillRect( hDC, &rcTemp, hBrushFill ));
		rcText.left = rcTemp.left + 2;
		rcText.right = rcTemp.right - 2;
		::DrawText( hDC, name, -1, &rcText, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );

		if ( lpDrawItemStruct->itemAction & ODA_FOCUS && 
			 lpDrawItemStruct->itemState & ODS_SELECTED ) {
			::DrawFocusRect( hDC, &rcTemp );
		}
	}
	if ( hBrushHigh ) 
		VERIFY( ::DeleteObject( hBrushHigh ));
	if ( hBrushWindow ) 
		VERIFY( ::DeleteObject( hBrushWindow ));

	if ( hOldFont )
		::SelectObject( hDC, hOldFont );
}

extern "C" void HelperSubPopulate
( HWND hWnd, int *pIndex, MSG_FolderInfo *folder, MSG_Master *pMaster, 
  UINT nAddString, UINT nSetItemData)
{
	int32 iLines = MSG_GetFolderChildren (pMaster, folder, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];
	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (pMaster, folder, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (pMaster, ppFolderInfo[i], &folderLine)) {
				if ( !(folderLine.flags & MSG_FOLDER_FLAG_CATEGORY) ||
					  (folderLine.flags & MSG_FOLDER_FLAG_CAT_CONTAINER)) {
					SendMessage( hWnd, nAddString, (WPARAM) 0, (LPARAM) folderLine.name );
					SendMessage( hWnd, nSetItemData, (WPARAM) *pIndex, (LPARAM) folderLine.id );
					(*pIndex)++;
					if ( folderLine.numChildren > 0 ) {
						HelperSubPopulate( hWnd, pIndex, folderLine.id ,pMaster, nAddString, nSetItemData);
					}
				}
			}
		}
		delete [] ppFolderInfo;
	}
}

extern "C" int HelperPopulate
( HWND hWnd, MSG_Master *master, MSG_FolderInfo *mailRoot, MSG_Master **hMaster, 
  UINT nResetContent, int* piInitialDepth, UINT nAddString, UINT nSetItemData)
{
	*hMaster = master;
	int index = 0;

	*piInitialDepth = 1;

	SendMessage( hWnd, nResetContent, (WPARAM) 0, (LPARAM) 0 );

	if ( mailRoot ) {
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( master, mailRoot, &folderLine );
		*piInitialDepth = folderLine.level;
	}

	HelperSubPopulate( hWnd, &index, mailRoot, master, nAddString, nSetItemData);

	return 1;	
}


/////////////////////////////////////////////////////////////////////////////
// CMailFolderCombo, CMailFolderList

CMailFolderHelper::CMailFolderHelper( UINT nAddString, UINT nSetItemData, 
									  UINT nResetContent)
{
	m_bStaticCtl = FALSE;

	m_hFont = NULL;
	m_hBoldFont = NULL;

	m_nAddString = nAddString;
	m_nSetItemData = nSetItemData;
	m_nResetContent = nResetContent;

    ApiApiPtr(api);
    m_pIImageUnk = api->CreateClassInstance(APICLASS_IMAGEMAP);
	ASSERT(m_pIImageUnk);
    if (m_pIImageUnk) {
        m_pIImageUnk->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImageMap);
        ASSERT(m_pIImageMap);
        m_pIImageMap->Initialize(IDB_MAILNEWS,16,16);
    }
}

CMailFolderHelper::~CMailFolderHelper()
{
	if (m_pIImageUnk && m_pIImageMap ) {
		m_pIImageUnk->Release();
	}
	if (m_hFont)
		theApp.ReleaseAppFont(m_hFont);
	if (m_hBoldFont)
		theApp.ReleaseAppFont(m_hBoldFont);
}

void CMailFolderHelper::InitFonts( HDC hdc )
{
	::HelperInitFonts(hdc, &m_hFont, &m_hBoldFont);
}

void CMailFolderHelper::DoMeasureItem( HWND hWnd, LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
	::HelperDoMeasureItem( hWnd, lpMeasureItemStruct );
}

void CMailFolderHelper::DoDrawItem( HWND hWnd, LPDRAWITEMSTRUCT lpDrawItemStruct, BOOL bNoPrettyName)
{
	::HelperDoDrawItem( hWnd, lpDrawItemStruct, &m_hFont, &m_hBoldFont,
					    m_pMaster, m_bStaticCtl, m_iInitialDepth, m_pIImageMap, bNoPrettyName);
}

void CMailFolderHelper::SubPopulate( HWND hWnd, int &index, MSG_FolderInfo *folder )
{
	HelperSubPopulate( hWnd, &index, folder, m_pMaster, m_nAddString, m_nSetItemData);
}

int CMailFolderHelper::Populate( HWND hWnd, MSG_Master *master, MSG_FolderInfo *mailRoot )
{
	return HelperPopulate( hWnd, master, mailRoot, &m_pMaster, m_nResetContent, 
		                   &m_iInitialDepth, m_nAddString, m_nSetItemData);
}

int CMailFolderHelper::PopulateMailServer( HWND hWnd, MSG_Master *master, BOOL bRoots)
{
	m_pMaster = master;
	int index = 0;

	m_iInitialDepth = 1;

	SendMessage( hWnd, m_nResetContent, (WPARAM) 0, (LPARAM) 0 );

	int32 iLines = MSG_GetFolderChildren (m_pMaster, NULL, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];
	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (m_pMaster, NULL, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (m_pMaster, ppFolderInfo[i], &folderLine)) {
				if ( folderLine.flags & MSG_FOLDER_FLAG_MAIL ) {
					if ( bRoots ) {
						SendMessage( hWnd, m_nAddString, (WPARAM) 0, (LPARAM) folderLine.name );
						SendMessage( hWnd, m_nSetItemData, (WPARAM) index, (LPARAM) folderLine.id );
						index++;
					}
				}
			}
		}
		delete [] ppFolderInfo;
	}

	return 1;	
}

int CMailFolderHelper::PopulateMail( HWND hWnd, MSG_Master *master, BOOL bRoots)
{
	m_pMaster = master;
	int index = 0;

	m_iInitialDepth = 1;

	SendMessage( hWnd, m_nResetContent, (WPARAM) 0, (LPARAM) 0 );

	int32 iLines = MSG_GetFolderChildren (m_pMaster, NULL, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];
	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (m_pMaster, NULL, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (m_pMaster, ppFolderInfo[i], &folderLine)) {
				if ( folderLine.flags & MSG_FOLDER_FLAG_MAIL ) {
					if ( bRoots ) {
						SendMessage( hWnd, m_nAddString, (WPARAM) 0, (LPARAM) folderLine.name );
						SendMessage( hWnd, m_nSetItemData, (WPARAM) index, (LPARAM) folderLine.id );
						index++;
					}
					if ( folderLine.numChildren > 0 ) {
						SubPopulate( hWnd, index, ppFolderInfo[i] );
					}
				}
			}
		}
		delete [] ppFolderInfo;
	}

	return 1;	
}

int CMailFolderHelper::PopulateNews( HWND hWnd, MSG_Master *master, BOOL bRoots )
{
	m_pMaster = master;
	int index = 0;

	m_iInitialDepth = 1;

	SendMessage( hWnd, m_nResetContent, (WPARAM) 0, (LPARAM) 0 );

	int32 iLines = MSG_GetFolderChildren (m_pMaster, NULL, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];
	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (m_pMaster, NULL, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (m_pMaster, ppFolderInfo[i], &folderLine)) {
				if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ||
					 folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST) 
				{
					if ( bRoots ) {
						SendMessage( hWnd, m_nAddString, (WPARAM) 0, (LPARAM) folderLine.name );
						SendMessage( hWnd, m_nSetItemData, (WPARAM) index, (LPARAM) folderLine.id );
						index++;
					}
					if ( folderLine.numChildren > 0 ) {
						SubPopulate( hWnd, index, ppFolderInfo[i] );
					}
				}
			}
		}
		delete ppFolderInfo;
	}

	return 1;	
}


#define NAV_BUTTONWIDTH		24
#define NAV_BUTTONHEIGHT	16

CNavCombo::CNavCombo()
{
	m_bFirst = TRUE;
	m_hFont = NULL;
	m_hBigFont = NULL;
}

CNavCombo::~CNavCombo()
{
}

void CNavCombo::SetFont( CFont *pFont, CFont *pBigFont )
{
	m_hFont = (HFONT) pFont->m_hObject;
	m_hBigFont = (HFONT) pBigFont->m_hObject;

	CMailFolderCombo::SetFont(pFont);
}

void CNavCombo::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	BOOL bStatic = FALSE;

#ifdef _WIN32
	if ( sysInfo.m_bWin4 )
		bStatic = ( lpDrawItemStruct->itemState & ODS_COMBOBOXEDIT ) ? TRUE : FALSE;
	else 
#endif
		bStatic = m_bStaticCtl;

	if ( bStatic ) {
		HDC hdc = lpDrawItemStruct->hDC;
		RECT rect = lpDrawItemStruct->rcItem; 

		COLORREF oldText;
		COLORREF oldBk;
		if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
			oldText = ::SetTextColor( hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
			oldBk = ::SetBkColor( hdc, GetSysColor(COLOR_HIGHLIGHT));
		} else {
			oldText = ::SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
			oldBk = ::SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
		}
		HFONT hOldFont = (HFONT) ::SelectObject( hdc, m_hBigFont );

		MSG_FolderInfo *itemData = (MSG_FolderInfo *) lpDrawItemStruct->itemData;

		if ( (lpDrawItemStruct->itemID != (UINT) -1) && itemData ) {
			MSG_FolderLine folderLine;
			MSG_GetFolderLineById( m_pMaster, itemData, &folderLine);
			
			rect.left += 4;

			LPCTSTR name = (LPCTSTR) folderLine.prettyName;
			if ( !name || !name[0] )
				name = folderLine.name;

			CIntlWin::DrawText( 0, hdc, (LPSTR) name, -1, &rect,
								  DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX );
		}

		if ( hOldFont )
			::SelectObject( hdc, hOldFont );

		::SetTextColor( hdc, oldText );
		::SetBkColor(hdc, oldText);
	} else {
		CMailFolderCombo::DrawItem( lpDrawItemStruct );
	}
}

BEGIN_MESSAGE_MAP( CNavCombo, CMailFolderCombo )
	ON_WM_CTLCOLOR()
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CNavCombo::OnPaint()
{
	m_bStaticCtl = TRUE;
	CMailFolderCombo::OnPaint();
	m_bFirst = TRUE;
}

HBRUSH CNavCombo::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	if (nCtlColor == CTLCOLOR_LISTBOX) {
		if (m_bFirst) {
			RECT rcParent;
			GetWindowRect( &rcParent );
			pWnd->GetWindowRect(&m_rcList);
			m_bFirst = FALSE;
			int height = m_rcList.bottom - m_rcList.top;
			if ( (rcParent.bottom + height) > GetSystemMetrics( SM_CYFULLSCREEN ) ) {
				height = GetSystemMetrics( SM_CYFULLSCREEN ) - rcParent.bottom;
			}
			pWnd->MoveWindow( m_rcList.left, rcParent.bottom,
							  m_rcList.right - m_rcList.left, height );
		}
	}

	m_bStaticCtl = (nCtlColor == CTLCOLOR_EDIT);

	return CMailFolderCombo::OnCtlColor( pDC, pWnd, nCtlColor );
}

///////////////////////////////////////////////////////////////////////
// CLinkDropSource

class CLinkDropSource: public COleDropSource
{
protected:
	virtual SCODE GiveFeedback( DROPEFFECT dropEffect );
};

SCODE CLinkDropSource::GiveFeedback( DROPEFFECT dropEffect )
{
	if ( !(dropEffect & (DROPEFFECT_COPY|DROPEFFECT_MOVE)) ) {
		return COleDropSource::GiveFeedback( dropEffect );
	} else {
		::SetCursor( ::LoadCursor( AfxGetResourceHandle(), 
								   MAKEINTRESOURCE( dropEffect & DROPEFFECT_COPY ?
												    IDC_LINK_COPY : IDC_LINK_MOVE ) ) );
		return NOERROR;
	}
}

///////////////////////////////////////////////////////////////////////
// CMailInfoBar

CMailInfoBar::CMailInfoBar()
{
	m_bEraseBackground = TRUE;

	m_idxImage = 0;
	m_pUnkImage = NULL;

	ApiApiPtr(api);
	m_pUnkImage = api->CreateClassInstance( APICLASS_IMAGEMAP,
											NULL, 
											(APISIGNATURE)IDB_MAILNEWS);
	m_pUnkImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImage);
	ASSERT(m_pIImage);
	if (!m_pIImage->GetResourceID())
		m_pIImage->Initialize(IDB_MAILNEWS,16,16);

	m_hbmBanner = NULL;
	m_hFont = NULL;
	m_hBoldFont = NULL;
	m_hIntlFont = NULL;
	m_hBoldIntlFont = NULL;
}

CMailInfoBar::~CMailInfoBar()
{
    if (m_pUnkImage) {
        if (m_pIImage)
            m_pUnkImage->Release();
    }
	if (m_hbmBanner)
		VERIFY(::DeleteObject( m_hbmBanner ));

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
	if (m_hBoldFont) {
		theApp.ReleaseAppFont(m_hBoldFont);
	}
	if (m_hIntlFont) {
		theApp.ReleaseAppFont(m_hIntlFont);
	}
	if (m_hBoldIntlFont) {
		theApp.ReleaseAppFont(m_hBoldIntlFont);
	}
}

void CMailInfoBar::SetCSID( int csid )
{

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
	if (m_hBoldFont) {
		theApp.ReleaseAppFont(m_hBoldFont);
	}
	if (m_hIntlFont) {
		theApp.ReleaseAppFont(m_hIntlFont);
	}
	if (m_hBoldIntlFont) {
		theApp.ReleaseAppFont(m_hBoldIntlFont);
	}

	CClientDC dc(this);
	m_iCSID = csid;
	int16 defCSID = INTL_CharSetNameToID(INTL_ResourceCharSet());

    LOGFONT lf;
    memset( &lf, 0, sizeof(LOGFONT) );

    lf.lfPitchAndFamily = FF_SWISS;
	lf.lfCharSet = IntlGetLfCharset(defCSID);
	if (defCSID == CS_LATIN1)
 		_tcscpy(lf.lfFaceName, "MS Sans Serif");
	else
 		_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(defCSID));

   	lf.lfHeight = -MulDiv( 9, dc.GetDeviceCaps(LOGPIXELSY), 72 );
    lf.lfWeight = 0;
	m_hFont = theApp.CreateAppFont( lf );
   	lf.lfHeight = -MulDiv( 10, dc.GetDeviceCaps(LOGPIXELSY), 72 );
    lf.lfWeight = 700;
	m_hBoldFont = theApp.CreateAppFont( lf );
	lf.lfCharSet = IntlGetLfCharset(csid);
	if (csid == CS_LATIN1)
 		_tcscpy(lf.lfFaceName, "MS Sans Serif");
	else
 		_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(csid));

   	lf.lfHeight = -MulDiv( 9, dc.GetDeviceCaps(LOGPIXELSY), 72 );
    lf.lfWeight = 0;
	m_hIntlFont = theApp.CreateAppFont( lf );
   	lf.lfHeight = -MulDiv( 10, dc.GetDeviceCaps(LOGPIXELSY), 72 );
    lf.lfWeight = 700;
	m_hBoldIntlFont = theApp.CreateAppFont( lf );

	Invalidate();
}

BOOL CMailInfoBar::Create( CWnd *pWnd, MSG_Pane *pPane )
{
	m_pPane = pPane;

	ASSERT( pWnd && pWnd->IsKindOf( RUNTIME_CLASS(CFrameWnd) ));

	DWORD dwStyle = WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|CBRS_TOP;
	UINT nID = AFX_IDW_CONTROLBAR_FIRST + 0x7e;
	
	LPCTSTR lpszClass = AfxRegisterWndClass( 0, ::LoadCursor( NULL, IDC_ARROW) );

#ifdef _WIN32
	m_dwStyle = dwStyle;
#endif

	BOOL res = CControlBar::Create( lpszClass, _T("InfoBar"), dwStyle, 
									CRect(0,0,0,0), pWnd, nID);

	if (res) {
		SetCSID( INTL_DefaultWinCharSetID(0) );
#ifndef _WIN32
		m_sizeFixedLayout = CalcFixedLayout(TRUE, TRUE);
#endif
	}
	return res;
}

void CMailInfoBar::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{
}

CSize CMailInfoBar::CalcFixedLayout( BOOL bStretch, BOOL bHorz )
{
	ASSERT( bHorz ); // No vertical, No way

	HDC hDC = ::GetDC( m_hWnd );
	HFONT hOldFont = (HFONT) ::SelectObject( hDC, m_hBoldFont);

	TEXTMETRIC tm;
	GetTextMetrics( hDC, &tm);
	
	::SelectObject( hDC, hOldFont );
	::ReleaseDC( m_hWnd, hDC );

	int iHeight = (tm.tmHeight > 20 ? tm.tmHeight : 20 ) + 4;

	return CSize( 32767, iHeight );
}

BOOL CMailInfoBar::PreTranslateMessage( MSG *pMsg )
{
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
		m_wndToolTip.RelayEvent( pMsg );

    return CControlBar::PreTranslateMessage( pMsg );
}

void CMailInfoBar::PaintBackground( HDC hDC )
{
	RECT rect;
	GetClientRect( &rect );

	::FillRect( hDC, &rect, sysInfo.m_hbrBtnFace );
}

void CMailInfoBar::DrawInfoText( int iCSID, HDC hDC, LPCSTR lpszText, LPRECT lpRect )
{
	WFE_DrawTextEx( iCSID, hDC, (LPSTR) lpszText, -1, lpRect,
					DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,
					WFE_DT_CROPRIGHT );
}

void CMailInfoBar::DrawInfoText( HDC hDC, LPCSTR lpszText, LPRECT lpRect )
{
	int16 resCsid = INTL_CharSetNameToID(INTL_ResourceCharSet());
	DrawInfoText( resCsid, hDC, lpszText, lpRect );
}

void CMailInfoBar::MeasureInfoText( int iCSID, HDC hDC, LPCSTR lpszText, LPRECT lpRect )
{
	CIntlWin::DrawText( iCSID, hDC, (LPSTR) lpszText, -1, lpRect, DT_LEFT|DT_SINGLELINE|DT_CALCRECT );
}

void CMailInfoBar::MeasureInfoText( HDC hDC, LPCSTR lpszText, LPRECT lpRect )
{
	int16 resCsid = INTL_CharSetNameToID(INTL_ResourceCharSet());
	MeasureInfoText( resCsid, hDC, lpszText, lpRect );
}

void CMailInfoBar::DragProxie()
{
}

BEGIN_MESSAGE_MAP( CMailInfoBar, CControlBar )
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

int CMailInfoBar::OnCreate(LPCREATESTRUCT lpcs)
{
	int res = CControlBar::OnCreate(lpcs);
	if (res != -1) {
		m_wndToolTip.Create(this, TTS_ALWAYSTIP);
		m_wndToolTip.Activate(TRUE);
	}
	return res;
}
void CMailInfoBar::OnLButtonDown( UINT nFlags, CPoint point )
{
	RECT rect;
	GetClientRect(&rect);
	rect.right = 22;

	if (PtInRect(&rect, point)) {
		DragProxie();
	}
	else{
		// Send this message to the customizable toolbar for dragging
		MapWindowPoints(GetParent(), &point, 1);
		GetParent()->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
	}

}

void CMailInfoBar::OnLButtonUp( UINT nFlags, CPoint point )
{
	RECT rect;
	GetClientRect(&rect);
	rect.right = 22;

	if (!PtInRect(&rect, point)) {
		// Send this message to the customizable toolbar for dragging
		MapWindowPoints(GetParent(), &point, 1);
		GetParent()->SendMessage(WM_LBUTTONUP, nFlags, MAKELPARAM(point.x, point.y));

	}
	
}

void CMailInfoBar::OnMouseMove( UINT nFlags, CPoint point )
{
	RECT rect;
	GetClientRect(&rect);
	rect.right = 22;

	if (!PtInRect(&rect, point)) {
		// Send this message to the customizable toolbar for dragging
		MapWindowPoints(GetParent(), &point, 1);
		GetParent()->SendMessage(WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x, point.y));
	}

}

void CMailInfoBar::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground |= bShow;
}

BOOL CMailInfoBar::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		PaintBackground( pDC->m_hDC );
		m_bEraseBackground = FALSE;
	}

	return TRUE;
}

BOOL CMailInfoBar::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
	POINT pt;
	RECT rect;
	GetWindowRect(&rect);
	if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
		rect.left += 30;
	rect.right = rect.left + 22;
	GetCursorPos( &pt );

	if ( nHitTest == HTCLIENT && ::PtInRect( &rect, pt ) ) {
		::SetCursor( ::LoadCursor( AfxGetResourceHandle(), MAKEINTRESOURCE(IDC_HANDOPEN) ) );
		return TRUE;
	}

	return CControlBar::OnSetCursor( pWnd, nHitTest, message );
}

/////////////////////////////////////////////////////////////////////
//
// CContainerInfoBar
//
// Info Bar for the Folder Window

void CContainerInfoBar::Update()
{
	LPCSTR szUser = FE_UsersFullName();
	if ( szUser ) {
		m_csBanner.Format( szLoadString( IDS_MAIL_MYCONTAINERS ), szUser);
	} else {
		m_csBanner.LoadString(IDS_MAIL_CONTAINERS);
	}
}

void CContainerInfoBar::DragProxie()
{
	URL_Struct *url = MSG_ConstructUrlForPane( m_pPane );
	if ( url ) {
		COleDataSource *pDataSource = new COleDataSource;
		RDFGLOBAL_DragTitleAndURL( pDataSource, m_csBanner, url->address );
		COleDropSource *pDropSource = new CLinkDropSource;
		DROPEFFECT res = pDataSource->DoDragDrop( DROPEFFECT_COPY|DROPEFFECT_LINK|
												  DROPEFFECT_MOVE|DROPEFFECT_SCROLL,
												  CRect( 0, 0, 0, 0),
												  pDropSource );
		pDataSource->Empty();
		delete pDataSource;
		delete pDropSource;
		NET_FreeURLStruct( url );
	}
}

BEGIN_MESSAGE_MAP( CContainerInfoBar, CMailInfoBar )
	ON_WM_CREATE()
	ON_WM_PAINT()
END_MESSAGE_MAP()

int CContainerInfoBar::OnCreate(LPCREATESTRUCT lpcs)
{
	int res = CMailInfoBar::OnCreate(lpcs);
	if (res != -1) {
		RECT rect;
		SetRectEmpty(&rect);
		m_wndToolTip.AddTool(this, IDS_PROXIE_FOLDERS, &rect, IDS_PROXIE_FOLDERS);
	}
	return res;
}
void CContainerInfoBar::OnPaint( )
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect( &rect );

	PaintBackground( dc.m_hDC );

    m_pIImage->DrawTransImage( IDX_COLLECTION,
							   2,
							   rect.bottom / 2 - 8,
							   &dc );

	RECT rcTool;
	::SetRect(&rcTool, 2, rect.bottom / 2 - 8, 18, rect.bottom / 2 + 8);
	m_wndToolTip.SetToolRect(this, IDS_PROXIE_FOLDERS, &rcTool);

	int oldMode = dc.SetBkMode( TRANSPARENT );
	HFONT hOldFont = (HFONT) dc.SelectObject( m_hBoldFont );

	int iWidth = rect.right - rect.left;
	rect.left += 22;

	DrawInfoText( dc.m_hDC, m_csBanner, &rect );

	dc.SetBkMode( oldMode );
	dc.SelectObject( hOldFont );
}

///////////////////////////////////////////////////////////////////////
//
// CFolderInfoBar
//
// Info Bar for the Thread Window

CFolderInfoBar::CFolderInfoBar()
{
	m_folderOld = NULL;
}

CFolderInfoBar::~CFolderInfoBar()
{
}

void CFolderInfoBar::SetCSID( int csid )
{
	CMailInfoBar::SetCSID( csid );
	m_wndNavButton.SetFont( CFont::FromHandle( m_hFont ), CFont::FromHandle( m_hBoldFont ) );
}

void CFolderInfoBar::Update()
{
	if (m_pPane) {
		MSG_FolderLine folderLine;
		MSG_FolderInfo *folderInfo = NULL;

		if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
		{
			if (((C3PaneMailFrame*)GetParentFrame())->GetSelectedFolder(&folderLine))
				folderInfo = folderLine.id;
		}
		if (!folderInfo)
			folderInfo = MSG_GetCurFolder( m_pPane );
		if (folderInfo) {
			MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );
			if (folderLine.flags & MSG_FOLDER_FLAG_CATEGORY) {
				folderInfo = MSG_GetCategoryContainerForCategory(folderInfo);
				MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );
			}

			if ( folderInfo != m_folderOld ) {
				m_wndNavButton.Populate( WFE_MSGGetMaster(), NULL );
				m_wndNavButton.SetCurSel(0);
				for ( int i = 0; i < m_wndNavButton.GetCount(); i++ ) {
					if ( (MSG_FolderInfo *) m_wndNavButton.GetItemData(i) == folderInfo ) {
						m_wndNavButton.SetCurSel(i);
						break;
					}
				}
			}

			m_csFolderName = folderLine.prettyName && folderLine.prettyName[0] ?
							 folderLine.prettyName : folderLine.name;

			if (folderLine.flags & MSG_FOLDER_FLAG_CATEGORY) {
				if (folderLine.deepUnseen >= 0) {
					m_csFolderCounts.Format(szLoadString(IDS_FOLDERCOUNTS),
											(int32) folderLine.deepTotal, 
											(int32) folderLine.deepUnseen);
				} else {
					m_csFolderCounts.LoadString(IDS_FOLDERUNKNOWNCOUNTS);
				}
			} else {
				if (folderLine.unseen >= 0) {
					m_csFolderCounts.Format(szLoadString(IDS_FOLDERCOUNTS),
											(int32) folderLine.total, 
											(int32) folderLine.unseen);
				} else {
					m_csFolderCounts.LoadString(IDS_FOLDERUNKNOWNCOUNTS);
				}
			}

			m_idxImage = WFE_MSGTranslateFolderIcon( folderLine.level, 
													 folderLine.flags,
													 FALSE );
			m_folderOld = folderInfo;
		} else {
			m_csFolderName = "";
			m_csFolderCounts = "";
		}
	}
	Invalidate();
	UpdateWindow();
}

void CFolderInfoBar::DragProxie()
{
	URL_Struct *url = MSG_ConstructUrlForPane( m_pPane );
	if ( url ) {
		
		COleDataSource *pDataSource = new COleDataSource;
		RDFGLOBAL_DragTitleAndURL( pDataSource, m_csFolderName, url->address );
		COleDropSource *pDropSource = new CLinkDropSource;
		DROPEFFECT res = pDataSource->DoDragDrop( DROPEFFECT_COPY|DROPEFFECT_LINK|
												  DROPEFFECT_MOVE|DROPEFFECT_SCROLL,
												  CRect( 0, 0, 0, 0),
												  pDropSource );
		pDataSource->Empty();
		delete pDataSource;
		delete pDropSource;
		NET_FreeURLStruct( url );
	}
}

BEGIN_MESSAGE_MAP( CFolderInfoBar, CMailInfoBar )
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_CBN_DROPDOWN(101, OnDropDown)
	ON_CBN_SELENDOK(101, OnCloseUp)
	ON_BN_CLICKED(102, OnContainer )
	ON_COMMAND( ID_BUTTON_CONTAINER, OnContainer )
END_MESSAGE_MAP()

int CFolderInfoBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	int res = CMailInfoBar::OnCreate( lpCreateStruct );

	if ( res != -1 ) {
		DWORD dwStyle = 0;

		dwStyle = WS_CHILD|WS_VISIBLE|WS_VSCROLL|CBS_OWNERDRAWFIXED|CBS_DROPDOWNLIST;

		m_wndNavButton.Create( dwStyle, CRect(22,3,250,480), this, 101 );
		
		CString csFull, csLabel, csTip, csStatus;
		WFE_ParseButtonString( ID_NAVIGATE_CONTAINER, csStatus, csTip, csLabel );

		m_wndBackButton.Create( this, FALSE, CSize(20,20), CSize(20,20), 
								csLabel, csTip, csStatus,
								IDB_BACK, 0, CSize(16,16), 
								ID_BUTTON_CONTAINER, 0, 0 );

		m_wndBackButton.SetActionMessageOwner(this);
		RECT rect;
		SetRectEmpty(&rect);
		m_wndToolTip.AddTool(this, IDS_PROXIE_FOLDERS, &rect, IDS_PROXIE_FOLDERS);
	}
	return res;
}	

void CFolderInfoBar::OnSize( UINT nType, int cx, int cy )
{
	if ( nType != SIZE_MINIMIZED ) {
		m_wndBackButton.MoveWindow( cx - 22, 3, 22, cy - 6 );
	}
}

void CFolderInfoBar::OnDropDown()
{
	// Folder Structure might have changed, rebuild.
	m_wndNavButton.Populate( WFE_MSGGetMaster(), NULL );
	m_wndNavButton.SetCurSel(0);
	for ( int i = 0; i < m_wndNavButton.GetCount(); i++ ) {
		if ( (MSG_FolderInfo *) m_wndNavButton.GetItemData(i) == m_folderOld ) {
			m_wndNavButton.SetCurSel(i);
			break;
		}
	}
}

void CFolderInfoBar::OnCloseUp()
{
	int iCurSel = m_wndNavButton.GetCurSel();
	m_wndNavButton.m_bFirst = TRUE;

	MSG_FolderInfo *folderInfo = (MSG_FolderInfo *) m_wndNavButton.GetItemData(iCurSel);
	MSG_FolderLine folderLine;
	if ( m_folderOld != folderInfo ) {

		CMsgListFrame* pFrame = C3PaneMailFrame::FindFrame( folderInfo );
		if ( pFrame ) {
			pFrame->ActivateFrame();
		} else {
			if ( !MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine ) ) {
				ASSERT(0); // Shouldn't happen
				return;
			}

			// User selected a folder
			pFrame = (CMsgListFrame *) GetParentFrame();
			ASSERT(pFrame && pFrame->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)));
			if (pFrame->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
			{
				// bounce focus back to frame
				((C3PaneMailFrame*)pFrame)->UpdateFolderPane(folderInfo);
				pFrame->SetFocus();
				return;
			} 
			else 
			{
				if ( folderLine.level > 1 )  
					CFolderFrame::Open( folderInfo );
			}
		}

		// Since we didn't change, reselect our folder
		m_wndNavButton.SetCurSel(0);
		for ( int i = 0; i < m_wndNavButton.GetCount(); i++ ) {
			if ( (MSG_FolderInfo *) m_wndNavButton.GetItemData(i) == m_folderOld ) {
				m_wndNavButton.SetCurSel(i);
				break;
			}
		}
	}
}

void CFolderInfoBar::OnContainer()
{
	GetParentFrame()->PostMessage( WM_COMMAND, (WPARAM) ID_NAVIGATE_CONTAINER );
}

void CFolderInfoBar::OnPaint( )
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect( &rect );

	PaintBackground( dc.m_hDC );

	m_pIImage->DrawTransImage( m_idxImage,
							   2,
							   rect.bottom / 2 - 8,
							   &dc );

	RECT rcTool;
	::SetRect(&rcTool, 2, rect.bottom / 2 - 8, 18, rect.bottom / 2 + 8);
	m_wndToolTip.SetToolRect(this, IDS_PROXIE_FOLDERS, &rcTool);

	int oldMode = dc.SetBkMode( TRANSPARENT );
	HFONT hOldFont = (HFONT) dc.SelectObject( m_hBoldFont );

	dc.SelectObject( m_hFont );

	RECT rcText;
	MeasureInfoText( dc.m_hDC, m_csFolderCounts, &rcText);
	int iCountsWidth = rcText.right - rcText.left;

	rect.left = rect.right - iCountsWidth - 32;
	rect.right -= 32;

	DrawInfoText( dc.m_hDC, m_csFolderCounts, &rect );

	dc.SetBkMode( oldMode );
	dc.SelectObject( hOldFont );

	m_wndNavButton.UpdateWindow();
}

///////////////////////////////////////////////////////////////////////
//
// CMessageInfoBar
//
// Info Bar for the Message Window

CMessageInfoBar::CMessageInfoBar()
{
	m_csFolderTip = "";
	m_csFolderStatus = "";
}

CMessageInfoBar::~CMessageInfoBar()
{
}

void CMessageInfoBar::SetCSID( int csid )
{
	CMailInfoBar::SetCSID( csid );
	Update();
}

void CMessageInfoBar::Update()
{
	if (m_pPane) {
		MSG_FolderInfo *folderInfo;
		MessageKey key;
		MSG_ViewIndex index;
		MSG_GetCurMessage( m_pPane, &folderInfo, &key, &index);
		if (folderInfo) {
			MSG_FolderLine folderLine;
			MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );
			m_idxImage = folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ? 
						 IDX_NEWSARTICLE :
						 IDX_MAILREAD;
			m_csFolderTip = folderLine.name;
			m_csFolderStatus.Format(szLoadString(IDS_OPEN_STRING), folderLine.name);
		} else {
			m_idxImage = 0;
		}
		if ( key != MSG_MESSAGEKEYNONE ) {
			LPTSTR buf; 
			MSG_MessageLine messageLine;
			MSG_GetThreadLineById( m_pPane, key, &messageLine );
			buf = IntlDecodeMimePartIIStr( messageLine.subject, INTL_DocToWinCharSetID(m_iCSID), FALSE );
			if ( buf ) {
				m_csMessageName = buf;
				XP_FREE(buf);
			} else {
				m_csMessageName = messageLine.subject;
			}
			buf = IntlDecodeMimePartIIStr( messageLine.author, INTL_DocToWinCharSetID(m_iCSID), FALSE );
			if ( buf ) {
				m_csMessageAuthor = buf;
				XP_FREE(buf);
			} else {
				m_csMessageAuthor = messageLine.author;
			}
		} else {
			m_csMessageName = "";
			m_csMessageAuthor = "";
		}
	}
	Invalidate();
}

void CMessageInfoBar::DragProxie()
{
	URL_Struct *url = MSG_ConstructUrlForPane( m_pPane );

	if ( url ) {
		
		COleDataSource *pDataSource = new COleDataSource;
		RDFGLOBAL_DragTitleAndURL( pDataSource, m_csMessageName, url->address );
		COleDropSource *pDropSource = new CLinkDropSource;
		DROPEFFECT res = pDataSource->DoDragDrop( DROPEFFECT_COPY|DROPEFFECT_LINK|
												  DROPEFFECT_MOVE|DROPEFFECT_SCROLL,
												  CRect( 0, 0, 0, 0),
												  pDropSource );
		pDataSource->Empty();
		delete pDataSource;
		delete pDropSource;

		NET_FreeURLStruct( url );
	}
}

BEGIN_MESSAGE_MAP( CMessageInfoBar, CMailInfoBar )
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_BN_CLICKED( 102, OnContainer )
	ON_COMMAND( ID_BUTTON_CONTAINER, OnContainer )
END_MESSAGE_MAP()

int CMessageInfoBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	int res = CMailInfoBar::OnCreate( lpCreateStruct );
	if ( res != -1 ) {
		CString csFull, csLabel, csTip, csStatus;
		WFE_ParseButtonString( ID_NAVIGATE_MSGCONTAINER, csStatus, csTip, csLabel );
		m_wndBackButton.Create( this, FALSE, CSize(20,20), CSize(20,20), 
								csLabel, csTip, csStatus,
								IDB_BACK, 0, CSize(16,16), 
								ID_BUTTON_CONTAINER, 0, TB_DYNAMIC_TOOLTIP );

		m_wndBackButton.SetActionMessageOwner(this);

		RECT rect;
		SetRectEmpty(&rect);
		m_wndToolTip.AddTool(this, IDS_PROXIE_FOLDERS, &rect, IDS_PROXIE_FOLDERS);
	}

	return res;
}
		
void CMessageInfoBar::OnSize( UINT nType, int cx, int cy )
{
	if ( nType != SIZE_MINIMIZED ) {
		m_wndBackButton.MoveWindow( cx - 22, 3, 22, cy - 6 );
	}
}

void CMessageInfoBar::OnPaint( )
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect( &rect );

	PaintBackground( dc.m_hDC );

    m_pIImage->DrawTransImage( m_idxImage,
							   2,
							   rect.bottom / 2 - 8,
							   &dc );

	RECT rcTool;
	::SetRect(&rcTool, 2, rect.bottom / 2 - 8, 18, rect.bottom / 2 + 8);
	m_wndToolTip.SetToolRect(this, IDS_PROXIE_FOLDERS, &rcTool);

	int oldMode = dc.SetBkMode( TRANSPARENT );
	HFONT hOldFont = (HFONT) dc.SelectObject( m_hBoldIntlFont );

	int iWidth = rect.right - rect.left;
	rect.left += 22;
	rect.right = 22 + iWidth / 2;

	DrawInfoText( m_iCSID, dc.m_hDC, m_csMessageName, &rect );

	dc.SelectObject( m_hIntlFont );
	rect.left = rect.right;
	rect.right = iWidth;

	DrawInfoText( m_iCSID, dc.m_hDC, m_csMessageAuthor, &rect );

	dc.SetBkMode( oldMode );
	dc.SelectObject( hOldFont );
}

void CMessageInfoBar::OnContainer()
{
	GetParentFrame()->PostMessage( WM_COMMAND, (WPARAM) ID_NAVIGATE_CONTAINER );
}

///////////////////////////////////////////////////////////////////////

