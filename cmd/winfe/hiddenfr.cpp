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

#include "hiddenfr.h"
#include "template.h"
#ifdef MOZ_MAIL_NEWS
#include "wfemsg.h"
#endif
#include "timer.h"
#ifdef MOZ_MAIL_NEWS
#include "addrfrm.h"
#endif
#include "feselect.h"
#include "postal.h" 
#include "ssl.h"

#ifdef MOZ_MAIL_NEWS
#include "nscpmapi.h"         // rhp - for MAPI
#include "mapihook.h"         // rhp - for MAPI
#include "mapismem.h"
#endif 

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CHiddenFrame

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CHiddenFrame, CFrameWnd)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#define EXITING -10  //multi-instance code return values
#define RUNNING  10

const UINT NEAR msg_FoundDNS = RegisterWindowMessage("NetscapeDNSFound");
const UINT NEAR msg_ExitStatus = RegisterWindowMessage("ExitingNetscape");

UINT NEAR msg_ForceIOSelect = (UINT)-1;
UINT NEAR msg_NetActivity = (UINT)-1;
const UINT NEAR msg_AltMailBiffNotification = RegisterWindowMessage("Netscape Mail System"); 

//  Net dike.
int gNetFloodStage = 0;

CHiddenFrame::CHiddenFrame()
{
    //  Attempt to make the registration of this as late as possible as to
    //      get the highest possible ID (see NSPumpMessage).
    msg_ForceIOSelect = RegisterWindowMessage("NetscapeForceIOSelect");
    msg_NetActivity = RegisterWindowMessage("NetscapeSocketSelect");
}

CHiddenFrame::~CHiddenFrame()
{
}

LONG CHiddenFrame::OnRequestExitStatus(WPARAM wParam/*0*/,LPARAM lParam/*0*/)
{
	return (theApp.m_bExitStatus ? EXITING :RUNNING );
}


BEGIN_MESSAGE_MAP(CHiddenFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CHiddenFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
    ON_REGISTERED_MESSAGE(msg_NetActivity, OnNetworkActivity)
    ON_REGISTERED_MESSAGE(msg_FoundDNS, OnFoundDNS)
    ON_REGISTERED_MESSAGE(msg_ForceIOSelect, OnForceIOSelect)
	ON_REGISTERED_MESSAGE(msg_ExitStatus, OnRequestExitStatus)
	ON_MESSAGE(MSG_TASK_NOTIFY, OnTaskNotify)
	ON_WM_DESTROY()
    ON_WM_ENDSESSION()
	//}}AFX_MSG_MAP
#ifdef MOZ_MAIL_NEWS
    ON_REGISTERED_MESSAGE(msg_AltMailBiffNotification, OnAltMailBiffNotification) 
#endif
    ON_WM_QUERYENDSESSION() //~~av

#ifdef MOZ_MAIL_NEWS
    ON_MESSAGE(WM_COPYDATA, OnProcessIPCHook)    // rhp - for MAPI
#endif

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHiddenFrame message handlers

#define HIDDENTIME 333
struct HiddenMessage   {
    UINT m_uMsg;
    WPARAM m_wParam;
    LPARAM m_lParam;
};
void wfe_HiddenRepost(void *pData)
{
    HiddenMessage *pMsg = (HiddenMessage *)pData;
    if(pMsg)    {
        if(theApp.m_pMainWnd && theApp.m_pMainWnd->GetSafeHwnd())   {
            theApp.m_pMainWnd->PostMessage(pMsg->m_uMsg, pMsg->m_wParam, pMsg->m_lParam);
        }

        memset(pMsg, 0, sizeof(*pMsg));
        delete pMsg;
        pMsg = NULL;
    }
}
void wfe_PostDelayedMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, uint32 uMilli = HIDDENTIME)
{
    HiddenMessage *pMsg = new HiddenMessage;
    if(pMsg)    {
        memset(pMsg, 0, sizeof(*pMsg));
        pMsg->m_uMsg = uMsg;
        pMsg->m_wParam = wParam;
        pMsg->m_lParam = lParam;
        void *pSet = FE_SetTimeout(wfe_HiddenRepost, (void *)pMsg, uMilli);
        if(NULL == pSet)    {
            delete pMsg;
            pMsg = NULL;
        }
    }
}

#ifdef MOZ_MAIL_NEWS
LONG CHiddenFrame::OnAltMailBiffNotification(WPARAM wParam, LPARAM lParam)
{
	switch(wParam) {
		case INBOX_STATE_NOMAIL:
			FE_UpdateBiff(MSG_BIFF_NoMail);
			break;

		case INBOX_STATE_UNKNOWN:
			FE_UpdateBiff(MSG_BIFF_Unknown);
			break;

		case INBOX_STATE_NEWMAIL:
			FE_UpdateBiff(MSG_BIFF_NewMail);
			break;

		default:
			break;
	}

	return(1); 
}
#endif // MOZ_MAIL_NEWS

LONG CHiddenFrame::OnForceIOSelect(WPARAM wParam, LPARAM lParam)
{
    //  Net dike. (see hiddenfr.h)
    //  This code protects external message loops from the flood
    //      of network messages we would normally handle, thus
    //      keeping the app responsive while in a different
    //      message loop.  External message loops being loops
    //      other than CNetscapeApp::NSPumpMessage.
    BOOL bDoHandle = TRUE;
    if(::GetMessageTime() != (LONG)theApp.GetMessageTime())   {
        //  Message came from different message loop than one
        //      found in CNetscapeApp::NSPumpMessage.
        BOOL bReposted = FALSE;
        
        //  In the CNetscapeApp::NSPumpMessage this variable is
        //      usually incremented on EVERY MESSAGE.  Note the
        //      difference here that we are incrementing it only
        //      on receipt of a network message, causing a much
        //      smaller window for these messages to get through.
        //  A small counter-balance is that we are going to
        //      repost the message, which causes this function
        //      to be entered again at some later time (thus
        //      incrementing the counter, etc).
        //  CNetscapeApp::NSPumpMessage only peeks and
        //      does not generate the overhead of yet another
        //      message, unlike this suboptimal code.
        gNetFloodStage++;
        
        //  1 : NET_FLOWCONTROL, we force a network event to be
        //      handled regardless of the queue state.
        if((gNetFloodStage % NET_FLOWCONTROL) != 0) {
            //  See if there are messages not our own in the queue.
            MSG msg;
            if(::PeekMessage(&msg, NULL, 0, NET_MESSAGERANGE, PM_NOREMOVE)) {
                //  There are, repost till next time.
                //  We use a 1 millisecond timeout, based on what we know
                //      timers actually do.  Timers don't fire until the
                //      queue is empty; thus the message will repost once
                //      all other stuff is done.
                bDoHandle = FALSE;
                wfe_PostDelayedMessage(msg_ForceIOSelect, wParam, lParam, 1);
            }
        }
    }

    if(bDoHandle) {
        SelectType stType = (SelectType)wParam;
        PRFileDesc *iFD = (PRFileDesc *)lParam;

        int iNetlibType = NET_SOCKET_FD;
        if(stType == FileSelect)    {
            iNetlibType = NET_LOCAL_FILE_FD;
        }
        else if(stType == NetlibSelect) {
            iNetlibType = NET_EVERYTIME_TYPE;
        }

        //  Must be socket or have in select list to continue.
        //  We don't check sockets here, as it is too difficult to
        //      segregate them out into their different select NetLib
        //      categories (see feselect.cpp).
		//
		//  LJM removed call to HasSelect for nspr20 port 
		//   if(iNetlibType == NET_SOCKET_FD || selecttracker.HasSelect(stType, iFD))    {
        if(iNetlibType == NET_SOCKET_FD)    {
            int iWantMore = 1;
            BOOL bCalledNetlib = FALSE;
        
            if(winfeInProcessNet == FALSE)  {
                winfeInProcessNet = TRUE;
                iWantMore = NET_ProcessNet(iFD, iNetlibType);
                bCalledNetlib = TRUE;
                winfeInProcessNet = FALSE;
            }

            if(bCalledNetlib == FALSE)  {
                //  Wait to repost message, as we are in the callstack of netlib.
                //  If a dialog is up, then we don't want to max the CPU by posting
                //      of the same message over and over and the user can't hit
                //      a button.
                wfe_PostDelayedMessage(msg_ForceIOSelect, wParam, lParam);
            }
			// LJM removed call to SSL_DataPending for NSPR20 port
            else if(0 != iWantMore && (NET_SOCKET_FD != iNetlibType))   {
                //  Sockets don't need reposting if actually called netlib,
                //      as they will post messages themselves when data is ready.
                //      This is not true if they have SSL data buffered.
                //  However, we do repost for files, as they are ready for more.
                PostMessage(msg_ForceIOSelect, wParam, lParam);
            }
        }
    }

    //  Always handled.
    return(1);
}

LONG CHiddenFrame::OnNetworkActivity(UINT socket, LONG lParam) 
{
    XP_ASSERT(0);
#ifdef NSPR20_DISABLED
    return(OnForceIOSelect((WPARAM)SocketSelect, (LPARAM)socket));
#endif
	return 0;
}                       

LONG CHiddenFrame::OnFoundDNS(WPARAM wParam, LONG lParam) 
{
	int iError = WSAGETASYNCERROR(lParam);

	//  Go through the DNS cache, find the correct task ID.
	//  The find should always be successful.
    //  Be sure to initalize values.
	POSITION pos = NULL;
	CString key;
	CDNSObj *obj = NULL;
	int i_found = 0;
	LONG return_value = 1;

	for(pos = DNSCacheMap.GetStartPosition(); pos != NULL;) {
		DNSCacheMap.GetNextAssoc(pos, key, (CObject *&)obj);
                
		if(!obj)
			return return_value;
		// Since the handle is not unique for the session only
		// compare handles that are currently in use (i.e. active entries)
		if(!obj->i_finished && obj->m_handle == (HANDLE)wParam) {
	  		i_found = 1;
	  		break;
		}

        //  Clear out object if we didn't break.
        //  That way we don't retain value if we leave the loop.
        obj = NULL;
	}

	if(!obj)
		return return_value;
  
	TRACE("%s error=%d h_name=%d task=%d\n", obj->m_host, iError,
		(obj->m_hostent->h_name != NULL) ? 1 : 0, obj->m_handle);

	//  If by chance we couldn't find it, we have a problem.
	//
	ASSERT(i_found == 1);

	/* temp fix */
	if(!i_found)
		return return_value;

	//  Mark this as completed.
	//
	obj->i_finished = 1;
  
	//  If there was an error, set it.
	if (iError) {
		TRACE("DNS Lookup failed! \n"); 
		obj->m_iError = iError;
		return_value = 0;
	}
  	
	/* call ProcessNet for each socket in the list */
	/* use a for loop so that we don't reference the "obj"
	 * after our last call to processNet.  We need to do
	 * this because the "obj" can get free'd by the call
	 * chain after all the sockets have been removed from
	 * sock_list
	 */
	PRFileDesc *tmp_sock;
	int count = XP_ListCount(obj->m_sock_list);
	for(; count; count--) {

		tmp_sock = (PRFileDesc *) XP_ListRemoveTopObject(obj->m_sock_list);

		//    Make sure we call into the Netlib on this socket in particular,
		//    	NET_SOCKET_FD type.
        OnForceIOSelect((WPARAM)SocketSelect, (LPARAM)tmp_sock);
	}

	return(return_value);
}

void CHiddenFrame::OnDestroy()
{
#ifdef _WIN32
	if (sysInfo.m_bWin4) {
		// Kill this biff taskbar icon
		NOTIFYICONDATA nid;
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = GetSafeHwnd();
		nid.uID = ID_TOOLS_INBOX;
		nid.uFlags = 0;
		Shell_NotifyIcon( NIM_DELETE, &nid );
	}
#endif
	CFrameWnd::OnDestroy();
}

/****************************************************************************
*
*	Task notification handling functions
*
****************************************************************************/

/****************************************************************************
*
*	CHiddenFrame::OnTaskNotify
*
*	PARAMETERS:
*		wParam	- icon ID
*		lParam	- mouse event
*
*	RETURNS:
*		Non-zero if message is processed.
*
*	DESCRIPTION:
*		This is the handler for the notification messages we received from
*		task bar icons.
*
****************************************************************************/

LONG CHiddenFrame::OnTaskNotify(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
		case ID_TOOLS_WEB:
		{
			HandleWebNotification(lParam);
		}
		break;
		
#ifdef MOZ_MAIL_NEWS
		case ID_TOOLS_MAIL:
		{
			HandleMailNotification(lParam);
		}
		break;
		
		case ID_TOOLS_INBOX:
		{
			HandleInboxNotification(lParam);
		}
		break;
#endif /* MOZ_MAIL_NEWS */
#ifdef EDITOR
		case ID_TOOLS_EDITOR:
		{
			HandleEditorNotification(lParam);
		}
		break;
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS		
		case ID_TOOLS_NEWS:
		{
			HandleNewsNotification(lParam);
		}
		break;

		case ID_FILE_NEWMESSAGE:
		{
			HandleComposeNotification(lParam);
		}
		break;

		case ID_TOOLS_ADDRESSBOOK:
		{
			HandleAddressBookNotification(lParam);
		}
		break;
#endif /* MOZ_MAIL_NEWS */
	}
	
	return(1L);
	
} // END OF	FUNCTION CHiddenFrame::OnTaskNotify()

#ifdef MOZ_MAIL_NEWS
 /****************************************************************************
*
*	CHiddenFrame::HandleMailNotification
*
*	PARAMETERS:
*		lMessage	- mouse notification message
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the mouse event notification handler for the mail icon
*		located in the Task Bar.
*
****************************************************************************/

void CHiddenFrame::HandleMailNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
		{
		    if(!theApp.m_bKioskMode) {	// TODO: is this still relevant?
				WFE_MSGOpenInbox();
			}
		}
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}
} // END OF	FUNCTION CHiddenFrame::HandleMailNotification()

 /****************************************************************************
*
*	CHiddenFrame::HandleInboxNotification
*
*	PARAMETERS:
*		lMessage	- mouse notification message
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the mouse event notification handler for the inbox icon
*		located in the Task Bar.
*
****************************************************************************/

void CHiddenFrame::HandleInboxNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
		{
			// Single click on mail icon means bring forth the mail window,
			// check mail too if it wasn't already open.
		    if(!theApp.m_bKioskMode)	// TODO: is this still relevant?
			{
				WFE_MSGOpenInbox(TRUE);
			}
		}
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}
} // END OF	FUNCTION CHiddenFrame::HandleMailNotification()
#endif /* MOZ_MAIL_NEWS */

/****************************************************************************
*
*	CHiddenFrame::HandleWebNotification
*
*	PARAMETERS:
*		lMessage	- mouse notification message
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the mouse event notification handler for the Web icon
*		located in the Task Bar.
*
****************************************************************************/

void CHiddenFrame::HandleWebNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
		{
		/*	CAbstractCX *pCX = FEU_GetLastActiveFrameContext(MWContextAny);
			CFrameWnd * pFrame = FEU_GetLastActiveFrame(MWContextAny);
			if (pFrame && IsKindOf(RUNTIME_CLASS(CGenericFrame))) {
				((CGenericFrame*)pFrame)->OnToolsWeb();
			} else {
				theApp.m_ViewTmplate->OpenDocumentFile( NULL );
			}*/
			BOOL bHandledMessage = FALSE;

			CFrameWnd * pFrame = FEU_GetLastActiveFrame(MWContextAny);
			if(pFrame)
				if(pFrame->SendMessage(WM_COMMAND, WPARAM(ID_TOOLS_WEB), 0))
					bHandledMessage = TRUE;

			if(!bHandledMessage)
				theApp.m_ViewTmplate->OpenDocumentFile( NULL );

		}
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}

} // END OF	FUNCTION CHiddenFrame::HandleWebNotification()

#ifdef MOZ_MAIL_NEWS
/****************************************************************************
*
*	CHiddenFrame::HandleNewsNotification
*
*	PARAMETERS:
*		lMessage	- mouse notification message
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the mouse event notification handler for the news icon
*		located in the Task Bar.
*
****************************************************************************/

void CHiddenFrame::HandleNewsNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
		{
		    if(!theApp.m_bKioskMode) {	// TODO: is this still relevant?
				WFE_MSGOpenNews();
			}
		}
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}
} // END OF	FUNCTION CHiddenFrame::HandleNewsNotification()
#endif /* MOZ_MAIL_NEWS */

#ifdef EDITOR
/****************************************************************************
*
*	CHiddenFrame::HandleEditorNotification
*
*	PARAMETERS:
*		lMessage	- mouse notification message
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the mouse event notification handler for the Compose icon
*		located in the Task Bar.
*
****************************************************************************/

void CHiddenFrame::HandleEditorNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
		{
			// Find last active editor/browser and find or launch new editor from there

            // 2/17/96 - fixed bug 45611. Was creating disabled editor window when invoked from a
            // Mail window. We need to look for any active frame, not just Browser frame.
		// We send a message instead of assuming that it's a generic frame because the
		// the inplace frame could be the last active frame.
		CFrameWnd * pFrame = FEU_GetLastActiveFrame(MWContextAny, TRUE);
			if (pFrame) {
			pFrame->SendMessage(WM_COMMAND, WPARAM(ID_TOOLS_EDITOR), 0);
			} else {
                // there should always be an active frame.
                ASSERT(FALSE);          // FE_CreateNewEditWindow(NULL, NULL);
			}
		}
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}
} // END OF	FUNCTION CHiddenFrame::HandleWebNotification()
#endif

#ifdef MOZ_MAIL_NEWS
/****************************************************************************
*
*	CHiddenFrame::HandleComposeNotification
*
*	PARAMETERS:
*		lMessage	- mouse notification message
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the mouse event notification handler for the compose icon
*		located in the Task Bar.
*
****************************************************************************/

void CHiddenFrame::HandleComposeNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
		{
			CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();
			ASSERT(pFrame != NULL);
			if (pFrame != NULL)
			{
				CAbstractCX * pCX = pFrame->GetMainContext();

				if (pCX)
				{
					MSG_Mail ( pCX->GetContext()  );
				}
			}
		}
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}
} // END OF	FUNCTION CHiddenFrame::HandleNewsNotification()

  void CHiddenFrame::HandleAddressBookNotification(LONG lMessage)
{
	switch (lMessage)
	{
		case WM_LBUTTONUP:
			CAddrFrame::Open();
		break;
		
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		break;
	}
} // END OF	FUNCTION CHiddenFrame::HandleAddressBookNotification()
#endif /* MOZ_MAIL_NEWS */

//~~av
// added this message handler not to allow it to fall through the default
// MFC processing. Needed for bug 44748 fix when it crashed on system shutdown
// if plugin was running
BOOL CHiddenFrame::OnQueryEndSession() 
{
	return TRUE;
}

void CHiddenFrame::OnEndSession(BOOL bEnding)
{
    if(bEnding) {
#ifdef MOZ_MAIL_NEWS
	    WFE_MSGShutdown();
#endif /* MOZ_MAIL_NEWS */
        //  Close all frames, and we'll exit.
        PostMessage(WM_COMMAND, ID_APP_SUPER_EXIT, 0);
    }
}

// rhp - 12/5/97
// This will simply turn around and call the function
// LONG ProcessNetscapeMAPIHook(WPARAM wParam, LPARAM lParam);
// and return the result. This is all for MAPI support in 
// Communicator
//
#ifdef MOZ_MAIL_NEWS    // No mail - no MAPI - for now, but this could live
                    // for IPC reasons in the future without MAPI

LONG CHiddenFrame::OnProcessIPCHook(WPARAM wParam, LPARAM lParam)
{
  PCOPYDATASTRUCT	  pcds = (PCOPYDATASTRUCT) lParam;

  if (!pcds)
    return(-1);

  // Now check for what type of IPC message this really is?
  if ((pcds->dwData > NSCP_MAPIStartRequestID) && (pcds->dwData < NSCP_MAPIEndRequestID))
  {
      return ( ProcessNetscapeMAPIHook(wParam, lParam) );
  }

  return(-1);
}

// rhp - new stuff for Address call...
void CHiddenFrame::AddressDialog(LPSTR winText, 
                                 MAPIAddressCallbackProc mapiCB,
                                 MAPIAddressGetAddrProc getProc)
{
    CAddrDialog AddressDialog(this, TRUE, winText, mapiCB, getProc);
    AddressDialog.DoModal();
}

#endif /* MOZ_MAIL_NEWS */

