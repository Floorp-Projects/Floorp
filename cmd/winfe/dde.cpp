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
//	File to handle DDE remote control commands.

#include "stdafx.h"

#include "genframe.h"

#include "wfedde.h"
#include "ddectc.h"
#include "regproto.h"
#include "winclose.h"
#include "presentm.h"
#include "urlecho.h"
#include "mainfrm.h"
#include "prefapi.h"    // Added so that prefs can be referenced (Dave Hyatt - 8/13/97)
#include "net.h"        // Added so cache can be referenced (8/18/97)
#include "mkcache.h"    // Ditto
#include "cxprint.h"    // The printing context... used for PrintWindow and PrintURL (8/27/97)

//	Our DDE instance identifier.
DWORD CDDEWrapper::m_dwidInst;

//	Wether or not DDE was successfully initialized.
BOOL CDDEWrapper::m_bDDEActive;

//	Our array of hsz strings.
HSZ CDDEWrapper::m_hsz[CDDEWrapper::m_MaxHSZ];

//	A list of all current conversations.  The PtrToPtr map was used since
//		the pointers are known to be 32 bits long, and the HCONV is a 
//		DWORD or possibly a pointer, and WordToPtr just wouldn't cut it
//		with 16 bits and all.
CMapPtrToPtr CDDEWrapper::m_ConvList;

//	Command filter for DDEML
DWORD CDDEWrapper::m_dwafCmd;

//	Call back function for use with DDEML
FARPROC CDDEWrapper::m_pfnCallBack;

//	Array of acceptable clipboard formats in our CallBack
//	Null terminate, please.
UINT CDDEWrapper::m_cfFmts[] = { CF_TEXT, 0 };

//	Purpose:	Initialize our DDE layer
//	Arguments:	void
//	Returns:	void
//	Comments:	Sets all CDDEWrapper static members and makes our DDE
//					service available to the DDEML.
//				We won't warn the user on an invalid initialization,
//					as they probably won't be using this DDE layer if
//					they have a screwed up setup....
//	Revision History:
//		12-28-94	created GAB
void DDEStartup()
{
	//	Initialize all CDDEWrapper static members here.
	//	No CDDEWrapper instances exist at this point.
	//	Note that m_cfFmts is already intialized, m_ConvList should
	//		be initially already empty.
	CDDEWrapper::m_dwidInst = 0L;
	CDDEWrapper::m_bDDEActive = FALSE;
	for(int i_counter = 0; i_counter < CDDEWrapper::m_MaxHSZ;
		i_counter++)	{
		CDDEWrapper::m_hsz[i_counter] = NULL;
	}
	CDDEWrapper::m_dwafCmd = APPCLASS_STANDARD
        | CBF_FAIL_SELFCONNECTIONS;
	CDDEWrapper::m_pfnCallBack = NULL;

#ifdef XP_WIN16
	//	Find out if we can even use DDEML (must be in protected mode).
	//	Undoubtedly, this is the case since we must be in protected
	//		mode to use WINSOCK services, but just to be anal....
	//  GetWinFlags() has been removed in MSVC 2.0 and the analog doesn't
	//      look like it does the same thing.  chouck 29-Dec-94
	if(!(GetWinFlags() & WF_PMODE))	{
		//	Not in protected mode, can not continue with DDEML
		return;
	}
#endif
	
	//	Set up our callback function to be registered with DDEML.
	CDDEWrapper::m_pfnCallBack = (FARPROC)NetscapeDDECallBack;
		
	//	DDEML initialization, don't do anything on failure.
	if(DdeInitialize(&CDDEWrapper::m_dwidInst,
		(PFNCALLBACK)CDDEWrapper::m_pfnCallBack,
		CDDEWrapper::m_dwafCmd, 0L))	{
		//	Unable to initialize, don't continue.
		return;
	}
	
	//	We're DDEed.
	CDDEWrapper::m_bDDEActive = TRUE;
	
	//	Load in all frequently used HSZs.
	//	Unfortunately, there is really no good way to detect any errors
	//		on these string intializations, so let the blame land
	//		where it will if something fails; we could actually just
	//		call shutdown and return if need be....
	// Ugh.  I hate CStrings reason #59:  passing it as a (char *) requires
	//   a nasty cast like the one below.  Chouck 29-Dec-94
	CString CS;
	
	CS.LoadString(IDS_DDE_SERVICE_NAME);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ServiceName] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_OPENURL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_OpenURL] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_SHOWFILE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ShowFile] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_ACTIVATE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Activate] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_LISTWINDOWS);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ListWindows] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_GETWINDOWINFO);
	CDDEWrapper::m_hsz[CDDEWrapper::m_GetWindowInfo] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_PARSEANCHOR);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ParseAnchor] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_EXIT);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Exit] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_REGISTERPROTOCOL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_RegisterProtocol] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_UNREGISTERPROTOCOL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_UnRegisterProtocol] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_REGISTERVIEWER);
	CDDEWrapper::m_hsz[CDDEWrapper::m_RegisterViewer] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_QUERYVIEWER);
	CDDEWrapper::m_hsz[CDDEWrapper::m_QueryViewer] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_VIEWDOCFILE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ViewDocFile] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_UNREGISTERVIEWER);
	CDDEWrapper::m_hsz[CDDEWrapper::m_UnRegisterViewer] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_REGISTERURLECHO);
	CDDEWrapper::m_hsz[CDDEWrapper::m_RegisterURLEcho] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_URLECHO);
	CDDEWrapper::m_hsz[CDDEWrapper::m_URLEcho] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_UNREGISTERURLECHO);
	CDDEWrapper::m_hsz[CDDEWrapper::m_UnRegisterURLEcho] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_REGISTERWINDOWCHANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_RegisterWindowChange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_WINDOWCHANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_WindowChange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_UNREGISTERWINDOWCHANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_UnRegisterWindowChange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_BEGINPROGRESS);
	CDDEWrapper::m_hsz[CDDEWrapper::m_BeginProgress] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_SETPROGRESSRANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_SetProgressRange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_MAKINGPROGRESS);
	CDDEWrapper::m_hsz[CDDEWrapper::m_MakingProgress] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_ENDPROGRESS);
	CDDEWrapper::m_hsz[CDDEWrapper::m_EndProgress] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_ALERT);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Alert] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_VERSION);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Version] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_CANCELPROGRESS);
	CDDEWrapper::m_hsz[CDDEWrapper::m_CancelProgress] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_QUERYURLFILE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_QueryURLFile] =
	    DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
	    (char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_LISTFRAMECHILDREN);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ListFrameChildren] =
	    DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
	    (char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_GETFRAMEPARENT);
	CDDEWrapper::m_hsz[CDDEWrapper::m_GetFrameParent] =
	    DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
	    (char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_REGISTERSTATUSBARCHANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_RegisterStatusBarChange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_STATUSBARCHANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_StatusBarChange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_UNREGISTERSTATUSBARCHANGE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_UnRegisterStatusBarChange] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_NAVIGATEBACK);
	CDDEWrapper::m_hsz[CDDEWrapper::m_NavigateBack] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_NAVIGATEFORWARD);
	CDDEWrapper::m_hsz[CDDEWrapper::m_NavigateForward] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_RELOAD);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Reload] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_STOP);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Stop] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_GETDOCWIDTH);
	CDDEWrapper::m_hsz[CDDEWrapper::m_GetDocumentWidth] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_GETDOCHEIGHT);
	CDDEWrapper::m_hsz[CDDEWrapper::m_GetDocumentHeight] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_USERAGENT);
	CDDEWrapper::m_hsz[CDDEWrapper::m_UserAgent] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
	CS.LoadString(IDS_DDE_CACHE_CLEARCACHE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Cache_ClearCache] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_CACHE_FILENAME);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Cache_Filename] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_CACHE_INCACHE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Cache_InCache] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_CACHE_REMOVEURL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Cache_RemoveURL] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_CACHE_ADDURL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_Cache_AddURL] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_HISTORY_REMOVEURL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_History_RemoveURL] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_HISTORY_ADDURL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_History_AddURL] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_HISTORY_INHISTORY);
	CDDEWrapper::m_hsz[CDDEWrapper::m_History_InHistory] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_HISTORY_CLEARHISTORY);
	CDDEWrapper::m_hsz[CDDEWrapper::m_History_ClearHistory] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_HISTORY_NUMENTRIES);
	CDDEWrapper::m_hsz[CDDEWrapper::m_History_NumEntries] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_HISTORY_GETENTRY);
	CDDEWrapper::m_hsz[CDDEWrapper::m_History_GetEntry] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);

    CS.LoadString(IDS_DDE_GETWINDOWID);
	CDDEWrapper::m_hsz[CDDEWrapper::m_GetWindowID] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_SUPPORTSMIMETYPE);
	CDDEWrapper::m_hsz[CDDEWrapper::m_SupportsMimeType] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_EXECUTEJAVASCRIPT);
	CDDEWrapper::m_hsz[CDDEWrapper::m_ExecuteJavaScript] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_PRINTWINDOW);
	CDDEWrapper::m_hsz[CDDEWrapper::m_PrintWindow] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);
    CS.LoadString(IDS_DDE_PRINTURL);
	CDDEWrapper::m_hsz[CDDEWrapper::m_PrintURL] =
		DdeCreateStringHandle(CDDEWrapper::m_dwidInst,
		(char *)(const char *)CS, CP_WINANSI);

	//	Register our Name Service with DDEML; we are prepared to rock.
	//	Ignore any error, we could call shutdown if need really be....
	DdeNameService(CDDEWrapper::m_dwidInst,
		CDDEWrapper::m_hsz[CDDEWrapper::m_ServiceName], NULL,
		DNS_REGISTER);
	
	TRACE("DDE started, forgive me, father....\n");
}

//	Purpose:	Shut down any intitialization that occurred previously
//	Arguments:	void
//	Returns:	void
//	Comments:	Won't call any DDE functions if there is no need.
//	Revision History:
//		12-28-94	created GAB
void DDEShutdown()
{
	//	First see if we're DDEed.
        if(CDDEWrapper::m_bDDEActive == TRUE) {

		//	Turn off all our name services.
		DdeNameService(CDDEWrapper::m_dwidInst,
            CDDEWrapper::m_hsz[CDDEWrapper::m_ServiceName], NULL,
			DNS_UNREGISTER);
		
		//	Get rid of all our HSZ strings.
		//	We have to finish up all DDE calls before we call DdeUnInitialize
		//		(that's the current theory), so just do this and don't
		//		worry about the side effects if we can't unitialize.
		for(int i_counter = 0; i_counter < CDDEWrapper::m_MaxHSZ;
			i_counter++)	{
			if(CDDEWrapper::m_hsz[i_counter] != NULL)	{
				//	Ignore errors.  No proper way to handle them on wind
				//		down.  Will not hurt initialization at a later
				//		time as Windows recycles these strings.
				DdeFreeStringHandle(CDDEWrapper::m_dwidInst,
					CDDEWrapper::m_hsz[i_counter]);
				CDDEWrapper::m_hsz[i_counter] = NULL;
			}
		}		
	
		//	We have no real way of recognizing why we can't deinitialize
		//		our DDE layer, so just return if unable....
		if(DdeUninitialize(CDDEWrapper::m_dwidInst) == 0)	{
			//	System won't let us unitialize.
			//	Just return without disabling the callback and current
			//		conversations; it will be up to the system to clean
			//		this up and handle the abscense of any missing HSZs...
			TRACE("Couldn't shut down DDE.\n");
			return;
		}
		
		//	Reset value for consistancy....  Someone, one day, might
		//		for some reason shut us down, and then restart.  Best
		//		not to keep state between periods.
		CDDEWrapper::m_dwidInst = 0L;
		CDDEWrapper::m_bDDEActive = FALSE;
		CDDEWrapper::m_dwafCmd = 0L;
	}
	
	//	Erase the pointer to the callback.
	CDDEWrapper::m_pfnCallBack = NULL;
	
	//	DDEML and the CallBack are dead.
	//	Kill off any dead conversation objects that we have, unbiased.
	//	Chances are, that if we receive appropriate disconnects when
	//		deinitializing, that we won't have any conversations anyhow,
	//		though I can't guarantee windows is so well thought out in
	//		this regard.
	//	It should be noted here that the list management is actually
	//		maintained by the CDDEWrapper constructor, and destructor,
	//		respectively.
	CDDEWrapper *pWrap;
	void *vp_dump, *vp_wrap;
	POSITION pos;
	while(CDDEWrapper::m_ConvList.IsEmpty() != 0)	{
		pos = CDDEWrapper::m_ConvList.GetStartPosition();
		
		if(pos == NULL)	{
			//	don't know how this would happen.
			//	don't let it happen though.
			break;
		}

		//	Get the valid pointer to the DDEWrapper
		CDDEWrapper::m_ConvList.GetNextAssoc(pos, vp_dump, vp_wrap);
		pWrap = (CDDEWrapper *)vp_wrap;
			
		//	Is this a valid entry anyhow?
		if(pWrap != NULL)	{
			//	Actually remove the object from memory
			delete pWrap;
		}
	}
	
	//	Shutdown complete.
	//	If someone needed to, they could call our startup function
	//		again and start this whole thing over again now.
	TRACE("DDE shutdown is complete.\n");
}

//	Purpose:	A callback function for the DDEML DLL, and to possibly
//					multiplex custom calls in from our own application.
//	Arguments:	type	Specifies the type of the current transation.
//				fmt		Specifies the format in which data is to be sent
//							or received
//				hconv	Identifies the conversation associated with the
//							current transaction
//				hsz1	Identifies a string, the context of which is
//							decided by the current transaction type
//				hsz2	ditto
//				hData	Identifies some DDE data, the context of which is
//							decided by the current transaction type
//				dwData1	Specifies tranaction specific data
//				dwData2	ditto
//	Returns:	HDDEDATA	The actual return type is relative to the
//								type of the transaction.
//	Comments:	Multiplexes all conversations to their appropriate
//					CDDEWrapper instance, or ignores them.
//				Due to the nature of Callbacks, and exported functions,
//					DO NOT USE TRACES HERE, or GPF.
//				The code will have to be air tight first time through,
//					be careful.
//	Revision History:
//		12-28-94	created GAB
//		12-30-94	Began fleshing out else ifs and switch statement
//						to multiplex DDE transactions.
//					This is likely to be one of the most monstrous
//						functions I'll ever write.
HDDEDATA 
CALLBACK 
#ifndef XP_WIN32
_export 
#endif
NetscapeDDECallBack(UINT type, UINT fmt,
	HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1,
	DWORD dwData2)
{
	//	First, are we correctly initialized?
	if(CDDEWrapper::m_bDDEActive == FALSE)	{
		return(NULL);
	}
	
	//	Do we support the format they are requesting?
	//	Fall through on not specified.
	if(fmt)	{
		//	Look through our list of supported formats, until we hit
		//		NULL at the end.
		for(int i_counter = 0; 1; i_counter++)	{
			if(CDDEWrapper::m_cfFmts[i_counter] == 0)	{
				//	End of list, and no match, fail.
				return(NULL);
			}
			if(CDDEWrapper::m_cfFmts[i_counter] == fmt)	{
				//	Matched, so break.
				break;
			}
		}
	}
	
	//	Depending on the class of transaction, we return different data.
	if(type & XCLASS_BOOL)	{
		//	Must return (HDDEDATA)TRUE or (HDDEDATA)FALSE
		switch(type)	{
		case XTYP_ADVSTART:	{
			//	We are the server.
			//	A client specified the XTYP_ADVSTART in a call to
			//		DdeClientTransaction
			break;
		}
		case XTYP_CONNECT:	{
			//	We are the server.
			//	A client call the DdeConnect specifying a service and
			//		topic name which we support.
			HSZ& hszTopic = hsz1;
			HSZ& hszService = hsz2;
			
			//	Deny the connection if the service name is not the
			//		one we are taking connections for.
			if(hszService !=
				CDDEWrapper::m_hsz[CDDEWrapper::m_ServiceName])	{
				return((HDDEDATA)FALSE);
			}
			
			//	Now, the topic can be NULL, or it can be any one of our
			//		topic names to be accepted.
			if(hszTopic == NULL)	{
				return((HDDEDATA)TRUE);
			}
			
			//	Go through all our topics, see if we match.
			if(0 != CDDEWrapper::EnumTopic(hszTopic))	{
				return((HDDEDATA)TRUE);
			}
			
			//	Topic not supported
			return((HDDEDATA)FALSE);
		}
		default:
			//	unknown
			return((HDDEDATA)FALSE);
		}
		
		//	Break handled here
		return((HDDEDATA)FALSE);
	}
	else if(type & XCLASS_DATA)	{
		//	Must return DDE data handle, CBR_BLOCK, or NULL
		switch(type)	{
		case XTYP_ADVREQ:	{
			//	We are the server.
			//	We called DdePostAdvise indicating that the data of
			//		an item in the advise loop had changed.
			break;
		}
		case XTYP_REQUEST:	{
			//	We are the server.
			//	A client said XTYP_REQUEST in DdeClientTransaction
			HSZ& hszTopic = hsz1;
			HSZ& hszItem = hsz2;
			
			//	Get the object associated with this conversation,
			//		and let it handle the request.
			CDDEWrapper *pWrap = CDDEWrapper::GetConvObj(hconv);
			if(pWrap != NULL)	{
				return(pWrap->RequestHandler(hszTopic, hszItem));
			}			
			break;
		}
		case XTYP_WILDCONNECT:	{
			//	We are the server
			//	A client called DdeConnect or DdeConnectList specifying
			//		NULL for the service name, the topic name, or both
			HSZ& hszTopic = hsz1;
			HSZ& hszService = hsz2;
			
			//	One way we can deny this type of connection is
			//		if the service name is not null, and is not our
			//		service name.
			if(hszService != NULL)	{
				if(hszService !=
					CDDEWrapper::m_hsz[CDDEWrapper::m_ServiceName])	{
					return(NULL);
				}
			}
			
			//	The only other way we can deny this type of connection
			//		is if the topic name is not one that we support.
			//	Skip the service name.
			if(hszTopic != NULL)	{
				if(0 == CDDEWrapper::EnumTopic(hszTopic))	{
					//	Topic wasn't matched.
					return(NULL);
				}
			}
			
			//	Obviously, we're going to be accepting this connection.
			//	We have to return all matching service/topic pairs
			//		that were given to us.
			//	Get an array of HSZPAIRs that is big enough to take
			//		all our topics.
			HSZPAIR ahszpair[CDDEWrapper::m_MaxHSZ + 1 -
				CDDEWrapper::m_TopicStart];
				
			//	Match up all service/topic matches.
			int i_depth = 0;
			for(int i_counter = CDDEWrapper::m_TopicStart; i_counter <
				CDDEWrapper::m_MaxHSZ; i_counter++)	{				
				//	See if we are over an appropriate topic.
				if(hszTopic == NULL || hszTopic ==
					CDDEWrapper::m_hsz[i_counter])	{
					//	Assign stuff over, don't care about service name,
					//		we only have one.
					ahszpair[i_depth].hszSvc =
						CDDEWrapper::m_hsz[CDDEWrapper::m_ServiceName];
					ahszpair[i_depth].hszTopic =
						CDDEWrapper::m_hsz[i_counter];
						
					//	We are one further in the array.
					i_depth++;
				}
			}
			
			//	Cap off the list with some NULLs, i_depth is correctly
			//		set.
			ahszpair[i_depth].hszSvc = NULL;
			ahszpair[i_depth].hszTopic = NULL;
			i_depth++;
			
			//	Create the array in the DDE way.
			HDDEDATA pData = DdeCreateDataHandle(CDDEWrapper::m_dwidInst,
				(unsigned char *) &ahszpair, 
				sizeof(HSZPAIR) * i_depth, 
				0L, 
				NULL,
				fmt,
				0);				
			
			//	Return it.  Don't care on failure, as it will be NULL
			//		anyhow.
			return(pData);
		}
		default:
			//	unknown
			return(NULL);
		}
		
		//	Break handled here
		return(NULL);
	}
	else if(type & XCLASS_FLAGS)	{
		//	Must return DDE_FACK, DDE_BUSY, or DDE_FNOTPROCESSED
		switch(type)	{
		case XTYP_ADVDATA:	{
			//	We are the client.
			//	The server gave us a data handle.
			break;
		}
		case XTYP_EXECUTE:	{
			//	We are the server.
			//	A client said XTYP_EXECUTE in DdeClientTransaction
			break;
		}
		case XTYP_POKE:	{
			//	We are the Server
			//	A client said XTYP_POKE in DdeClientTransaction
			HSZ& hszTopic = hsz1;
			HSZ& hszItem = hsz2;
			HDDEDATA& hDataPoke = hData;

			//	Get the object associated with this conversation,
			//		and let it handle the request.
			CDDEWrapper *pWrap = CDDEWrapper::GetConvObj(hconv);
			if(pWrap != NULL)	{
				return(pWrap->PokeHandler(hszTopic, hszItem, hDataPoke));
			}			
			break;
		}
		default:
			//	unknown
			return(DDE_FNOTPROCESSED);
		}
		
		//	Break handled here
		return(DDE_FNOTPROCESSED);
	}
	else if(type & XCLASS_NOTIFICATION)	{
		//	Must return NULL, as the return value is ignored
		switch(type)	{
		case XTYP_ADVSTOP:	{
			//	We are the server
			//	A client said XTYP_ADVSTOP in DdeClientTransaction
			break;
		}
		case XTYP_CONNECT_CONFIRM:	{
			//	We are the server.
			//	We returned 1 to a XTYP_CONNECT transaction.
			
			//	This callback is mainly to inform us of the conversation
			//		handle that now exists, but we will actually be
			//		creating an instance of an object to handle this
			//		conversation from now on.
			HSZ& hszTopic = hsz1;
			HSZ& hszService = hsz2;
			
			//	Create the object, correctly initialized so that
			//		it understands the service, topic, and conversation
			//		it is handling.
			//	It will add itself to the conversation list.
			CDDEWrapper *pObject = new CDDEWrapper(hszService, hszTopic,
				hconv);
			break;
		}
		case XTYP_DISCONNECT:	{
			//	We are either client/server
			//	The partner in the conversation called DdeDisconnect
			//		causing both client/server to receive this.
			
			//	Find out which conversation object we are dealing with.
			CDDEWrapper *pWrapper = CDDEWrapper::GetConvObj(hconv);
			
			//	Simply delete it.
			//	The object takes care of removing itself from the list.
			if(pWrapper != NULL)	{
				delete pWrapper;
			}
			break;
		}
		case XTYP_ERROR:	{
			//	We are either client/server
			//	A serious error has occurred.
			//	DDEML doesn't have any resources left to guarantee
			//		good communication.
			break;
		}
		case XTYP_REGISTER:	{
			//	We are either client/server
			//	A server application used DdeNameService to register
			//		a new service name.
			break;
		}
		case XTYP_UNREGISTER:	{
			//	We are either client/server
			//	A server applciation used DdeNameService to unregister
			//		an old service name.
			break;
		}
		case XTYP_XACT_COMPLETE:	{
			//	We are the client
			//	An asynchronous tranaction, sent when the client specified
			//		the TIMEOUT_ASYNC flag in DdeClientTransaction has
			//		concluded.
			break;
		}
		default:
			//	unknown
			return(NULL);
		}
		return(NULL);
	}

	//	Unknown class type
	return(NULL);
}


//	Purpose:	Construct a DDEWrapper object
//	Arguments:	hszService	The name of the service we are handling.
//				hszTopic	The name of the topic we are handling.
//				hConv		The handle of the conversation we are
//								handling.
//	Returns:	nothing
//	Comments:	Handles all created objects internally through a map.
//	Revision History:
//		12-30-94	created GAB
CDDEWrapper::CDDEWrapper(HSZ hszService, HSZ hszTopic, HCONV hConv)
{
	//	Set our members passed in.
	m_hszService = hszService;
	m_hszTopic = hszTopic;
	m_hConv = hConv;
	
	//	Figure out our enumerated service number.
	//	We know this anyhow, we only have one service name at this
	//		point.
	m_iService = m_ServiceName;
	
	//	Figure out our enumerated topic number.
	m_iTopic = EnumTopic(hszTopic);
	
	//	Add ourselves to the current map of conversations.
	m_ConvList.SetAt((void *)hConv, this);
}

//	Purpose:	Destory a CDDEWrapper object
//	Arguments:	void
//	Returns:	nothing
//	Comments:	Removes us from the internally handled list
//	Revision History:
//		12-30-94	created GAB
CDDEWrapper::~CDDEWrapper()
{
	//	Remove ourselves from the list of current conversations.
	m_ConvList.RemoveKey((void *)m_hConv);
}

//	Purpose:	Figure out which wrapper is associated with a conversation.
//	Arguments:	hConv	The conversation to find out about.
//	Returns:	CDDEWrapper *	A pointer to the CDDEWrapper that is
//									handling the conversation, or NULL
//									if none is present.
//	Comments:	Shouldn't ever return NULL really.
//	Revision History:
//		12-30-94	created GAB
CDDEWrapper *CDDEWrapper::GetConvObj(HCONV hConv)
{
	//	Query our static map of conversations for the object.
	void *pWrap;
	
	if(m_ConvList.Lookup((void *)hConv, pWrap) == 0)	{
		return(NULL);
	}
	return((CDDEWrapper *)pWrap);
}

//	Purpose:	Return the offset of the hsz topic string in our static
//					m_hsz array
//	Arguments:	hsz	The HSZ string to find in our array
//	Returns:	int	The offset into the array, also correlating to
//						it's enumerated value.  If not found, the
//						returned failure value is 0.
//	Comments:	Mainly coded to remove redundant lookups.  Modularity...
//	Revision History:
//		12-30-94	created GAB
int CDDEWrapper::EnumTopic(HSZ& hsz)
{
	int i_retval = 0;
	int i_counter;
	
	//	Just start looking for the HSZ string in our static array.
	for(i_counter = m_TopicStart; i_counter < m_MaxHSZ; i_counter++)
	{
		if(m_hsz[i_counter] == hsz)	{
			i_retval = i_counter;
			break;
		}
	}
	
	return(i_retval);
}

//	Purpose:	Scan in the arguments with a specified format.
//	Arguments:	hszArgs	The string containing the arguments, in string
//							format.
//				pFormat	The format (order) that the strings will be
//							scanned in.
//				...		Variable number of parameters, decided by
//							pFormat, that the values will be stored in.
//	Returns:	void
//	Comments:	Data should always be an exact match, no need for
//					errors, do it right the first time.
//				See below, as there will be some funky heuristics
//					going on regarding some parameters and how they
//					are represented.
//				Of course, only pointers should be passed in as the
//					variable number of arguments.
//				->Appropriate type casting here is a must, as we could
//					possibly overwrite, or underwrite, data.  This
//					also means that all the parameters passed in must
//					be correct!
//				Never call this function with an empty format!
//	Revision History:
//		12-31-94	created GAB
void CDDEWrapper::ScanArgs(HSZ& hszArgs, const char *pFormat, ...)
{
	//	Initialize variable number of argumetns.
	va_list VarList;
	va_start(VarList, pFormat);	
	
	//	Set up some variables we are going to use.
	int i_ArgNum = 0;
	char *pScan = (char *)pFormat;
	char *pExtract;
	
	//	Loop through the arguments we are going to parse.
	while(pScan && *pScan)	{
		//	What argument are we currently looking for?
		//	Extract it's value from our HSZ
		i_ArgNum++;
		pExtract = ExtractArg(hszArgs, i_ArgNum);
	
		//	Check it for currently supported types
		if(0 == strncmp(pScan, "DW", 2))	{
			//	Why, it's a DWORD.
			//	Take the pointer to it off our argument list.
			DWORD *pWord;
			pWord = va_arg(VarList, DWORD *);
			
			//	If there is nothing to scan, use a default value.
			if(pExtract == NULL)	{
				*pWord = 0x0;
			}
			else	{
				//	Take the value.
                *pWord = strtoul(pExtract, NULL, 0);
				//sscanf(pExtract, "%lu", pWord);
			}
		}
		else if(0 == strncmp(pScan, "QCS", 3))	{
			//	A quoted CString
			CString *pCS = va_arg(VarList, CString *);
			
			if(pExtract == NULL)	{
				pCS->Empty();
			}
			else	{
				//	Fun thing about a qouted string, is that we need
				//		to compress and '\"' into '"'.
				//	Extractions took off the leading and ending quotes.
				char *pCopy = pExtract;
				while(*pCopy)	{
					if(*pCopy == '\\' && *(pCopy + 1) == '\"')	{
						pCopy++;
					}
					
					*pCS += *pCopy;
					pCopy++;
				}
			}
		}
		else if(0 == strncmp(pScan, "CS", 2))	{
			//	A CString
			CString *pCS = va_arg(VarList, CString *);

			if(pExtract == NULL)	{
				pCS->Empty();
			}
			else	{
				//	Just copy.
				*pCS = pExtract;
			}
		}
		else if(0 == strncmp(pScan, "BL", 2))	{
			//	A boolean
			TwoByteBool *pBool = va_arg(VarList, TwoByteBool *);
			
			if(pExtract == NULL)	{
				*pBool = FALSE;
			}
			else	{
				//	Compare for a TRUE or a FALSE
				if(0 == stricmp(pExtract, "TRUE"))	{
					*pBool = TRUE;
				}
				else	{
					*pBool = FALSE;
				}
			}
		}
		else	{
			//	Doh!, not supported, just break out of the while loop.
			ASSERT(0);
			break;
		}
		
		//	Go on to the next argument in our format string.
		pScan = SkipToNextFormat(pScan);
		
		//	Free the memory that was used during extraction.
		if(pExtract != NULL)	{
			delete pExtract;
		}
	}
	
	//	Done with variable number of arguments
	va_end(VarList);
}


//	Purpose:	Scan in the arguments with a specified format.
//	Arguments:	hArgs	The string containing the arguments, in string
//							format (must convert to HSZ first).
//				pFormat	The format (order) that the strings will be
//							scanned in.
//				...		Variable number of parameters, decided by
//							pFormat, that the values will be stored in.
//	Returns:	void
//	Comments:	Data should always be an exact match, no need for
//					errors, do it right the first time.
//				See below, as there will be some funky heuristics
//					going on regarding some parameters and how they
//					are represented.
//				Of course, only pointers should be passed in as the
//					variable number of arguments.
//				->Appropriate type casting here is a must, as we could
//					possibly overwrite, or underwrite, data.  This
//					also means that all the parameters passed in must
//					be correct!
//				Never call this function with an empty format!
//	Revision History:
//		01-05-95	created GAB
void CDDEWrapper::ScanDataArgs(HDDEDATA& hArgs, const char *pFormat, ...)
{
	char *pDataArgs = (char *)DdeAccessData(hArgs, NULL);
	
	//	Initialize variable number of argumetns.
	va_list VarList;
	va_start(VarList, pFormat);	
	
	//	With DATA args, it will be possible that with only one argument,
	//		that we are receiving raw data.
	//	Use this method.
	if(strchr(pFormat, ',') == NULL)	{
		//	Only one argument.
		if(strcmp(pFormat, "DW") == 0)	{
			DWORD *pWord;
			pWord = va_arg(VarList, DWORD *);
			*pWord = *(DWORD *)pDataArgs;
			DdeUnaccessData(hArgs);
			va_end(VarList);
			return;
		}
		else if(strcmp(pFormat, "BL") == 0)	{
			TwoByteBool *pBool;
			pBool = va_arg(VarList, TwoByteBool *);
			*pBool = *(TwoByteBool *)pDataArgs;
			DdeUnaccessData(hArgs);
			va_end(VarList);
			return;
		}
	}

	//	Convert our hArgs to HSZ format.
	HSZ hszArgs = DdeCreateStringHandle(m_dwidInst, pDataArgs,
		CP_WINANSI);
	DdeUnaccessData(hArgs);	

	//	Set up some variables we are going to use.
	int i_ArgNum = 0;
	char *pScan = (char *)pFormat;
	char *pExtract;
	
	//	Loop through the arguments we are going to parse.
	while(pScan && *pScan)	{
		//	What argument are we currently looking for?
		//	Extract it's value from our HSZ
		i_ArgNum++;
		pExtract = ExtractArg(hszArgs, i_ArgNum);
	
		//	Check it for currently supported types
		if(0 == strncmp(pScan, "DW", 2))	{
			//	Why, it's a DWORD.
			//	Take the pointer to it off our argument list.
			DWORD *pWord;
			pWord = va_arg(VarList, DWORD *);
			
			//	If there is nothing to scan, use a default value.
			if(pExtract == NULL)	{
				*pWord = 0x0;
			}
			else	{
				//	Take the value.
                *pWord = strtoul(pExtract, NULL, 0);
				//sscanf(pExtract, "%lu", pWord);
			}
		}
		else if(0 == strncmp(pScan, "QCS", 3))	{
			//	A quoted CString
			CString *pCS = va_arg(VarList, CString *);
			
			if(pExtract == NULL)	{
				pCS->Empty();
			}
			else	{
				//	Fun thing about a qouted string, is that we need
				//		to compress and '\"' into '"'.
				//	Extractions took off the leading and ending quotes.
				char *pCopy = pExtract;
				while(*pCopy)	{
					if(*pCopy == '\\' && *(pCopy + 1) == '\"')	{
						pCopy++;
					}
					
					*pCS += *pCopy;
					pCopy++;
				}
			}
		}
		else if(0 == strncmp(pScan, "CS", 2))	{
			//	A CString
			CString *pCS = va_arg(VarList, CString *);

			if(pExtract == NULL)	{
				pCS->Empty();
			}
			else	{
				//	Just copy.
				*pCS = pExtract;
			}
		}
		else if(0 == strncmp(pScan, "BL", 2))	{
			//	A boolean
			TwoByteBool *pBool = va_arg(VarList, TwoByteBool *);
			
			if(pExtract == NULL)	{
				*pBool = FALSE;
			}
			else	{
				//	Compare for a TRUE or a FALSE
				if(0 == stricmp(pExtract, "TRUE"))	{
					*pBool = TRUE;
				}
				else	{
					*pBool = FALSE;
				}
			}
		}
		else	{
			//	Doh!, not supported, just break out of the while loop.
			ASSERT(0);
			break;
		}
		
		//	Go on to the next argument in our format string.
		pScan = SkipToNextFormat(pScan);
		
		//	Free the memory that was used during extraction.
		if(pExtract != NULL)	{
			delete pExtract;
		}
	}
	
	//	Done with variable number of arguments
	va_end(VarList);
	
	//	Free off our created HSZ string.
	DdeFreeStringHandle(m_dwidInst, hszArgs);
}


//	Purpose:	Create an argument list, as HDDEDATA
//	Arguments:	pFormat	The format of the argument list
//				...		The viarables correlating to the format
//	Returns:	HSZ	The argument list (a string, really)
//	Comments:	HSZ is the return type because this will mainly
//					be used to create return values back to the
//					server.
//				Do not screw up and pass in the wrong arguments, or
//					a wrong format.
//				Only pointers are accepted as arguments, because if
//					a NULL is passed in, then it will be considered
//					an optional non-existant argument.  If this is
//					not the case, then pass in a valid pointer,
//					pointing to a valid argument.
//	Revision History:
//		01-05-95	created GAB
HSZ CDDEWrapper::MakeItemArgs(const char *pFormat, ...)
{
	//	Safety dance
	char *pTraverse = (char *)pFormat;	
	if(pTraverse == NULL || *pTraverse == '\0')	{
		return(NULL);
	}

	//	Set up for variable number of arguments.
	va_list VarList;
	va_start(VarList, pFormat);

	//	Real simple.
	//	Go through our format, and append the string representation
	//		to our developing return value.
	char caNumpad[64];
	CString csBuffer;
	CString csRetval;
	
	while(*pTraverse)	{
		//	Erase temp data from our last pass.
		caNumpad[0] = '\0';
		csBuffer.Empty();
	
		//	Compare our current format to the known formats
		if(0 == strncmp(pTraverse, "DW", 2))	{
			//	A DWORD.
			DWORD *pWord = va_arg(VarList, DWORD *);
			
			if(pWord != NULL)	{			
				sprintf(caNumpad, "%lu", *pWord);
				csRetval += caNumpad;
			}
		}
		else if(0 == strncmp(pTraverse, "CS", 2))	{
			//	A CString, not quoted
			CString *pCS = va_arg(VarList, CString *);
			
			if(pCS != NULL)	{
				csRetval += *pCS;
			}
		}
		else if(0 == strncmp(pTraverse, "QCS", 3))	{
			//	A quoted CString
			CString *pQCS = va_arg(VarList, CString *);
			
			if(pQCS != NULL)	{
				csRetval += '\"';
				
				//	Need to escape any '"' to '\"', literally.
				char *pConvert = (char *)(const char *)*pQCS;
				while(*pConvert != '\0')	{
					if(*pConvert == '\"')	{
						csRetval += '\\';
					}
					csRetval += *pConvert;
					pConvert++;
				}
				csRetval += '\"';
			}
		}
		else if(0 == strncmp(pTraverse, "BL", 2))	{
			//	A boolean
			TwoByteBool *pBool = va_arg(VarList, TwoByteBool *);
			
			if(pBool != NULL)	{
				if(*pBool != FALSE)	{
					csRetval += "TRUE";
				}
				else	{
					csRetval += "FALSE";
				}
			}
		}
		else	{
			//	Unhandled format, just get out of loop.
			ASSERT(0);
			break;
		}
		
		//	Go on to next type
		pTraverse = SkipToNextFormat(pTraverse);
		
		//	See if we need a comma
		if(*pTraverse != '\0')	{
			csRetval += ',';
		}
	}

	//	Done with varargs.
	va_end(VarList);
	
	//	Make sure we're atleast returning something.
	if(csRetval.IsEmpty())	{
		return(NULL);
	}

	//	Return our resultant HSZ, created from our string, and
	//		the DDEML will own this once we return it.
	HSZ Final = DdeCreateStringHandle(m_dwidInst, (char *)
		(const char *)csRetval, CP_WINANSI);
	return(Final);
}


//	Purpose:	Create an argument list, as HDDEDATA
//	Arguments:	pFormat	The format of the argument list
//				...		The viarables correlating to the format
//	Returns:	HDDEDATA	The argument list (a string, really)
//	Comments:	HDDEDATA is the return type because this will mainly
//					be used to create return values back to the
//					client.
//				Do not screw up and pass in the wrong arguments, or
//					a wrong format.
//				Only pointers are accepted as arguments, because if
//					a NULL is passed in, then it will be considered
//					an optional non-existant argument.  If this is
//					not the case, then pass in a valid pointer,
//					pointing to a valid argument.
//	Revision History:
//		01-02-95	created GAB
HDDEDATA CDDEWrapper::MakeArgs(const char *pFormat, ...)
{
	//	Safety dance
	char *pTraverse = (char *)pFormat;	
	if(pTraverse == NULL || *pTraverse == '\0')	{
		return(NULL);
	}

	//	Set up for variable number of arguments.
	va_list VarList;
	va_start(VarList, pFormat);

	//	Real simple.
	//	Go through our format, and append the string representation
	//		to our developing return value.
	char caNumpad[64];
	CString csBuffer;
	CString csRetval;
	
	while(*pTraverse)	{
		//	Erase temp data from our last pass.
		caNumpad[0] = '\0';
		csBuffer.Empty();
	
		//	Compare our current format to the known formats
		if(0 == strncmp(pTraverse, "DW", 2))	{
			//	A DWORD.
			DWORD *pWord = va_arg(VarList, DWORD *);
			
			if(pWord != NULL)	{			
				sprintf(caNumpad, "%lu", *pWord);
				csRetval += caNumpad;
			}
		}
		else if(0 == strncmp(pTraverse, "CS", 2))	{
			//	A CString, not quoted
			CString *pCS = va_arg(VarList, CString *);
			
			if(pCS != NULL)	{
				csRetval += *pCS;
			}
		}
		else if(0 == strncmp(pTraverse, "QCS", 3))	{
			//	A quoted CString
			CString *pQCS = va_arg(VarList, CString *);
			
			if(pQCS != NULL)	{
				csRetval += '\"';
				
				//	Need to escape any '"' to '\"', literally.
				char *pConvert = (char *)(const char *)*pQCS;
				while(*pConvert != '\0')	{
					if(*pConvert == '\"')	{
						csRetval += '\\';
					}
					csRetval += *pConvert;
					pConvert++;
				}
				csRetval += '\"';
			}
		}
		else if(0 == strncmp(pTraverse, "BL", 2))	{
			//	A boolean
			TwoByteBool *pBool = va_arg(VarList, TwoByteBool *);
			
			if(pBool != NULL)	{
				if(*pBool != FALSE)	{
					csRetval += "TRUE";
				}
				else	{
					csRetval += "FALSE";
				}
			}
		}
		else	{
			//	Unhandled format, just get out of loop.
			ASSERT(0);
			break;
		}
		
		//	Go on to next type
		pTraverse = SkipToNextFormat(pTraverse);
		
		//	See if we need a comma
		if(*pTraverse != '\0')	{
			csRetval += ',';
		}
	}

	//	Done with varargs.
	va_end(VarList);
	
	//	Make sure we're atleast returning something.
	if(csRetval.IsEmpty())	{
		return(NULL);
	}

	//	Return our resultant HDDEDATA, created from our string, be
	//		sure to copy over the terminating NULL, no offset, no
	//		item association, this is text, and the DDEML will own
	//		this once we return it.
	HDDEDATA Final;
	TRACE("Returning HDDEDATA:  %s\n", (const char *)csRetval);
	Final = DdeCreateDataHandle(m_dwidInst,
#ifdef XP_WIN16
		(void *)(const char *)csRetval, 
#else
		(LPBYTE)(const char *)csRetval,
#endif	 
 		csRetval.GetLength() + 1, 0, m_hsz[m_ServiceName],
		CF_TEXT, 0);

	//	Okay, we're not supposed to know this, but the minimum data size of this
	//		object is in increments of 32.  So we're going to do a little proactive
	//		writing to it if need be.
	//	THIS IS A HACK.
	//	I just recently figured out that we need to return actual raw data to the
	//		calling application, so handle it here for now.
	//	This only happens if we have only one argument, check for commas.
	//	Forget the speed considerations, anyone using DDE is insane anyhow.
	if(strchr(pFormat, ',') == NULL)	{
		if(strcmp(pFormat, "BL") == 0)	{
			//	Scan in the value.
			TwoByteBool bData = FALSE;
			if(csRetval == "TRUE")	{
				bData = TRUE;
			}
			DdeFreeDataHandle(Final);
			
			Final = DdeCreateDataHandle(m_dwidInst, (unsigned char *) &bData, sizeof(TwoByteBool), 0, m_hsz[m_ServiceName], CF_TEXT, 0);			
		}
		else if(strcmp(pFormat, "DW") == 0)	{
			//	Scan in the value.
			DWORD dwData;
            dwData = strtoul(csRetval, NULL, 0);
			//sscanf(csRetval, "%lu", &dwData);
			DdeFreeDataHandle(Final);
			
			//	Reset the value in a binary way.
			Final = DdeCreateDataHandle(m_dwidInst, (unsigned char *) &dwData, sizeof(DWORD), 0, m_hsz[m_ServiceName], CF_TEXT, 0);			
		}
	}

	return(Final);
}


//	Purpose:	Move the pointer passed in to the next argument
//	Arguments:	pFormat	the format string wanting to be incremented
//	Returns:	char *	The next format.
//	Comments:	
//	Revision History:
//		01-01-95	created GAB
char *CDDEWrapper::SkipToNextFormat(char *pFormat)
{
	//	Safety dance
	if(pFormat == NULL || *pFormat == '\0')	{
		return(pFormat);
	}

	//	The next format is directly after a ','
	while(*pFormat != ',' && *pFormat != '\0')	{
		pFormat++;
	}
	if(*pFormat == ',')	{
		pFormat++;
	}
	
	return(pFormat);
}


//	Purpose:	Return an allocated char array conatining the contents
//					of a specified argument.
//	Arguments:	hszArgs	The HSZ of all arguments
//				iArg	Integer specifying which argument to extract.
//	Returns:	char *	The allocated array containing the contents of
//							argument iArg, or NULL if there is no such
//							argument or if the argument was empty.
//	Comments:	Quoted strings are counted as only one argument, though
//					the quotes are not copied into the return string.
//	Revision History:
//		01-01-95	created GAB
char *CDDEWrapper::ExtractArg(HSZ& hszArgs, int iArg)
{
	//	Allocate some memory to retrieve infomation from out HSZ.
	DWORD dwLength = DdeQueryString(m_dwidInst, hszArgs, NULL, 0L,
		CP_WINANSI) + 1;
	char *pTraverse = new char[dwLength];
	char *pRemove = pTraverse;
	if(pTraverse == NULL)	{
		return(NULL);
	}	
	DdeQueryString(m_dwidInst, hszArgs, pTraverse, dwLength, CP_WINANSI);
	
	//	safety dance
	if(*pTraverse == '\0' || iArg < 1)	{
		delete(pRemove);
		return(NULL);
	}
	
	//	Need to decrement the argument we're looking for, as the very
	//		first argument has no ',' at the beginning.
	iArg--;
	
	//	Search through the arguments, seperated by ','.
	while(iArg)	{
		//	Special handling of quoted strings.
		if(*pTraverse == '\"')	{
			//	Find the ending quote.
			while(*pTraverse != '\0')	{
				pTraverse++;
				if(*pTraverse == '\"')	{
					pTraverse++;	//	One beyond, please
					break;
				}
				else if(*pTraverse == '\\')	{
					//	Attempting to embed a quoted, perhaps....
					if(*(pTraverse + 1) == '\"')	{
						pTraverse++;
					}
				}
			}
		}
		//	Find the next comma
		while(*pTraverse != '\0' && *pTraverse != ',')	{
			pTraverse++;
		}

		//	Go beyond a comma
		if(*pTraverse == ',')	{
			pTraverse++;
		}
		
		//	Okay, we're at the next argument, decrement our argument
		//		count.
		iArg--;
		
		//	By chance, if we're at the end of the string, break out of
		//		the loop.
		if(*pTraverse == '\0')	{
			break;
		}
	}
	
	//	Handle empty arguments here.
	if(*pTraverse == ',' || *pTraverse == '\0')	{
		delete(pRemove);
		return(NULL);
	}
	
	//	Count chars up to next comma, plus 1 (end of string stuff)
	int iLength = 1;
	char *pCounter = pTraverse;
	TwoByteBool bQuoted = FALSE;
	
	//	specially handle quoted strings
	if(*pCounter == '\"')	{
		//	Go past initial quote.
		pCounter++;
		bQuoted = TRUE;

		//	Find the ending quote.
		while(*pCounter != '\0')	{
			if(*pCounter == '\"')	{
				break;
			}
			else if(*pCounter == '\\')	{
				//	Attempting to embed a quoted, perhaps....
				if(*(pCounter + 1) == '\"')	{
					pCounter++;
					iLength++;
				}
			}
			
			pCounter++;
			iLength++;
		}
	}
	//	Go to next comma
	while(*pCounter != '\0' && *pCounter != ',')	{
		iLength++;
		pCounter++;
	}
	
	//	Subtrace one to ignore ending quote if we were quoted....
	if(bQuoted == TRUE)	{
		iLength--;
	}
	
	//	Argument's of length 1 are of no interest.
	if(iLength == 1)	{
		delete(pRemove);
		return(NULL);
	}
	
	//	Okay, allocate some memory for the string, we'll copy it over.
	char *pRetVal = new char[iLength];
	
	if(*pTraverse == '\"')	{
		pTraverse++;
	}	
	strncpy(pRetVal, pTraverse, iLength - 1);
	pRetVal[iLength - 1] = '\0';
	
	delete(pRemove);

	//	If pRetval begins with a caret, then detect if this is a file.
	//	We will substitue in file data if this is the case.
	if(pRetVal && *pRetVal == '^')	{
		char *pFilename = pRetVal + 1;

		if(*pFilename)	{
			CFileStatus cfs;
			BOOL bValid = CFile::GetStatus(pFilename, cfs);
			if(bValid)	{
				//	We have a file.
				//	Allocate the number of bytes + 1 (for null termination).
				char *pFileData = NULL;
				TRY	{
					pFileData = new char[cfs.m_size + 1];
				}
				CATCH(CException, e)	{
					pFileData = NULL;
				}
				END_CATCH

				if(pFileData != NULL)	{
					TRY	{
                        //  Leave as shared readable for DDE apps looking into the file early.
						CFile cf(pFilename, CFile::modeRead | CFile::shareDenyWrite);

						//	Read in the data.
						cf.Read(pFileData, CASTSIZE_T(cfs.m_size));

						//	Null terminate.
						pFileData[cfs.m_size] = '\0';
					}
					CATCH(CException, e)	{
						//	Opening or reading of the file failed.
						delete pFileData;
						pFileData = NULL;
					}
					END_CATCH
				}

				//	If we have file data, swap with the return value.
				if(pFileData)	{
					delete(pRetVal);
					pRetVal = pFileData;
				}
			}
		}
	}

	return(pRetVal);
}


//	Purpose:	Handle XTYP_REQUEST calls for this conversation.
//	Arguments:	hszTopic	The topic-name of the conversation
//				hszItem		The item-name under this topic
//	Returns:	HDDEDATA	Mainly, we will just pass back the return
//								values of functions that we call
//								depending upon our topic name.  Look
//								there.
//	Comments:	Second level muliplexor, for topics under an already
//					multiplexed conversation.
//				Server only code, here.
//				I would have used function tables to index into the
//					appropriate function on topic type, but the
//					topics are not neccessarily grouped together in
//					the same range as thier topic number.
//	Revision History:
//		12-30-94	created GAB
HDDEDATA CDDEWrapper::RequestHandler(HSZ& hszTopic, HSZ& hszItem)
{
	//	Ensure this conversation is about the topic we think it is.
	if(m_hsz[m_iTopic] != hszTopic)	{
		return(NULL);
	}

	TRACE("REQUEST topic %d\n", m_iTopic);
	
	//	Switch on all possible request topics, and pass params on down.
	switch(m_iTopic)	{
	case m_OpenURL:
		return(OpenURL(hszItem));
	case m_ShowFile:
		return(ShowFile(hszItem));
	case m_Activate:
		return(Activate(hszItem));
	case m_ListWindows:
		return(ListWindows(hszItem));
	case m_GetWindowInfo:
		return(GetWindowInfo(hszItem));
	case m_ParseAnchor:
		return(ParseAnchor(hszItem));
	case m_RegisterProtocol:
		return(RegisterProtocol(hszItem));
	case m_UnRegisterProtocol:
		return(UnRegisterProtocol(hszItem));
	case m_RegisterViewer:
		return(RegisterViewer(hszItem));
	case m_UnRegisterViewer:
		return(UnRegisterViewer(hszItem));
	case m_RegisterWindowChange:
		return(RegisterWindowChange(hszItem));
	case m_UnRegisterWindowChange:
		return(UnRegisterWindowChange(hszItem));
	case m_BeginProgress:
		return(BeginProgress(hszItem));
	case m_MakingProgress:
		return(MakingProgress(hszItem));
	case m_EndProgress:
		return(EndProgress(hszItem));
	case m_Version:
		return(Version(hszItem));
	case m_QueryURLFile:
	    return(QueryURLFile(hszItem));
	case m_ListFrameChildren:
		return(ListFrameChildren(hszItem));
	case m_GetFrameParent:
		return(GetFrameParent(hszItem));
    // Start of the Netscape 5.0 topics (Dave Hyatt - 8/13/97)
	case m_RegisterStatusBarChange: 
		return(RegisterStatusBarChange(hszItem));
	case m_UnRegisterStatusBarChange:
		return(UnRegisterStatusBarChange(hszItem)); 
	case m_NavigateBack:
		return(NavigateBack(hszItem));	
	case m_NavigateForward:
		return (NavigateForward(hszItem));	
	case m_Stop:
		return (Stop(hszItem));
	case m_Reload:
		return (Reload(hszItem));
    case m_UserAgent:
        return (UserAgent(hszItem));
    case m_Cache_ClearCache:
        return (ClearCache(hszItem));
    case m_Cache_Filename:
        return (CacheFilename(hszItem));
    case m_Cache_InCache:
        return (InCache(hszItem));
    case m_Cache_RemoveURL:
        return (CacheRemoveURL(hszItem));
    case m_Cache_AddURL:
        return (CacheAddURL(hszItem));
    case m_History_ClearHistory:
        return (ClearHistory(hszItem));
    case m_History_AddURL:
        return (HistoryAddURL(hszItem));
    case m_History_RemoveURL:
        return (HistoryRemoveURL(hszItem));
    case m_History_InHistory:
        return (InHistory(hszItem));
    case m_History_NumEntries:
        return (HistoryNumEntries(hszItem));
    case m_History_GetEntry:
        return (HistoryGetEntry(hszItem));
    case m_GetWindowID:
        return (GetWindowID(hszItem));
    case m_SupportsMimeType:
        return (SupportsMimeType(hszItem));
    case m_ExecuteJavaScript:
        return (ExecuteJavaScript(hszItem));
    case m_PrintWindow:
        return (PrintWindow(hszItem));
    case m_PrintURL:
        return (PrintURL(hszItem));

	}

	//	Default, should never reach.
	return(NULL);
}

//	Purpose:	Handle XTYP_POKE calls for this conversation.
//	Arguments:	hszTopic	The topic-name of the conversation
//				hszItem		The item-name under this topic
//				hData		Identifies data the client is sending to
//								this server conversation.
//	Returns:	HDDEDATA	Mainly, we will just pass back the return
//								values of functions that we call
//								depending upon our topic name.  Look
//								there.
//	Comments:	Second level muliplexor, for topics under an already
//					multiplexed conversation.
//				Server only code, here.
//				I would have used function tables to index into the
//					appropriate function on topic type, but the
//					topics are not neccessarily grouped together in
//					the same range as thier topic number.
//	Revision History:
//		12-30-94	created GAB
HDDEDATA CDDEWrapper::PokeHandler(HSZ& hszTopic, HSZ& hszItem,
	HDDEDATA& hData)
{
	//	Ensure this conversation is about the topic we think it is.
	if(m_hsz[m_iTopic] != hszTopic)	{
		return(NULL);
	}
	
	TRACE("POKE topic %d\n", m_iTopic);
	
	//	Switch on all possible poke topics, and pass params on down.
	switch(m_iTopic)	{
	case m_Exit:
		return(Exit(hszItem, hData));
	case m_SetProgressRange:
		return(SetProgressRange(hszItem, hData));
	case m_RegisterURLEcho:
		return(RegisterURLEcho(hszItem, hData));
	case m_UnRegisterURLEcho:
		return(UnRegisterURLEcho(hszItem, hData));
	case m_WindowChange:
		return(WindowChange(hszItem, hData));
	case m_CancelProgress:
		return(CancelProgress(hszItem, hData));
	}

	//	Default, should never reach.
	return((HDDEDATA)DDE_FNOTPROCESSED);
}

//	Purpose:	Bring Netscape to the top of all other applications,
//					and show the windows specified.  This will restore
//					minimized Netscape icons.
//	Arguments:	hszItem	A string representing our real parameters.
//	Returns:	HDDEDATA	Returns the ID of the window that was
//								activated and has focus.
//							NULL on failure.
//	Comments:	Parameters within hszItem are
//					WindowID,Flags
//				WindowID is the handle of the window to restore, or bring
//					to the top.  -1 means don't care, I suppose.
//				Flags is currently reserved for later use, so just
//					ignore it.
//	Revision History:
//		12-31-94	created GAB
HDDEDATA CDDEWrapper::Activate(HSZ& hszItem)
{
	DWORD ReturnID = 0L;

	//	Obtain our arguments.
	DWORD WindowID;
	DWORD Flags;
	ScanArgs(hszItem, "DW,DW", &WindowID, &Flags);
	
	//	If we have a specified WindowID, attempt to activate it.
	if(WindowID != 0xFFFFFFFF && WindowID != 0x0)	{
		//	See if it's a valid Window ID
		CFrameWnd *pWnd = FEU_FindFrameByID(WindowID, MWContextBrowser);
		if(pWnd != NULL)	{
			if(pWnd->IsIconic())	{
				pWnd->ShowWindow(SW_RESTORE);
			}
            ::SetWindowPos(pWnd->GetSafeHwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#ifdef XP_WIN32
            pWnd->SetForegroundWindow();
#endif
			
			//	Success, set the return value.
			ReturnID = WindowID;
        }
	}
	else if(WindowID != 0x0)	{
		//	If we have no windows, then it's time to create a new one.
		//	This is a valid possibility, since we could be doing some
		//		type of background work through OLE.
		if(XP_ContextCount(MWContextBrowser, TRUE) == 0)	{
			CFE_CreateNewDocWindow(NULL, NULL);
		}
	
		//	We're just supposed to bring ourselves to the front.
		//	Return the WindowID of the window up front.
		//	Since if there wasn't an active window, then we created one,
		//		the value returned by the below should never be NULL.
		CFrameWnd *pWnd = FEU_GetLastActiveFrame(MWContextBrowser);
		if(pWnd != NULL)	{
			if(pWnd->IsIconic())	{
				pWnd->ShowWindow(SW_RESTORE);
			}
            ::SetWindowPos(pWnd->GetSafeHwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#ifdef XP_WIN32
            pWnd->SetForegroundWindow();
#endif

			//	Success, set the return value.
			CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(pWnd);
			if(pGlue != NULL)	{
				if(pGlue->GetActiveContext() != NULL)	{
					ReturnID = pGlue->GetActiveContext()->GetContextID();
				}
				else if(pGlue->GetMainContext() != NULL)	{
					ReturnID = pGlue->GetMainContext()->GetContextID();
				}
			}
		}
	}

    if(ReturnID)    {
        //  Activate it for real.
        CAbstractCX *pCX = CAbstractCX::FindContextByID(ReturnID);
        if(pCX && pCX->IsFrameContext() && pCX->GetContext()->type == MWContextBrowser)  {
            CWinCX *pWinCX = (CWinCX *)pCX;
            if(pWinCX->GetFrame() && pWinCX->GetFrame()->GetFrameWnd())   {
                pWinCX->
                    GetFrame()->
                    GetFrameWnd()->
                    SetActiveView(pWinCX->GetView(), TRUE);
            }
        }
    }
	
	// HDDEDATA hData = DdeCreateDataHandle(m_dwidInst, &ReturnID, sizeof(DWORD), 0, NULL, 0, 0);
	HDDEDATA hData = MakeArgs("DW", &ReturnID);
	return(hData);
}


//	Purpose:	Retrieves a URL from the net, and either dumps it into
//					a file, or displays it in a given frame.
//	Arguments:	hszArgs	A string representing our real parameters.
//	Returns:	HDDEDATA	Returns the actual WindowID of the window
//								the performed the open, where 0 means
//								the operation failed, and 0xFFFFFFFF
//								means the data was not of an appropriate
//								MIME type to display in a Web browser....
//	Comments:	Parameters within hszItem are
//					URL,[FileSpec],WindowID,Flags,[FormData],[MIMEType],
//						[ProgressApp]
//						URL is the location to load
//						FileSpec is the file to dump the load to
//						WindowID is the window to load into, 0 meaning
//							a new window, 0xFFFFFFFF means the default
//							window.
//						Flags are:
//							0x1	Ignore document cache
//							0x2	Ignore image cache
//							0x4	Operate in the background (?)
//						FormData allows the POSTing of a form.
//						MIMEType specifies the form data mime type.
//						ProgressApp can be named, which is a DDE server
//							that will handle progress messages....
//						
//	Revision History:
//		01-02-95	created GAB
HDDEDATA CDDEWrapper::OpenURL(HSZ& hszArgs)
{
	DWORD dwReturn = 0L;
	
	//	Obtain our arguments.
	CString csURL;
	CString csFileSpec;
	DWORD dwWindowID;
	DWORD dwFlags;
	CString csFormData;
	CString csMimeType;
	CString csProgressApp;
	ScanArgs(hszArgs, "QCS,QCS,DW,DW,QCS,QCS,CS", &csURL, &csFileSpec,
		&dwWindowID, &dwFlags, &csFormData, &csMimeType,
		&csProgressApp);
		
	//	This is very important.
	//	If the client specified a server callback service, then we need
	//		to save this right now.
	//	It will be used in WWW_Alert, WWW_BeginProgress,
	//		WWW_SetProgressRange, WWW_MakingProgress, and WWW_EndProgress.
	m_csProgressApp = csProgressApp;
	
	//	Get the appropriate window.
	if(dwWindowID == 0)	{
		//	They want a new window.
		if(NULL != CFE_CreateNewDocWindow(NULL, NULL))	{
			//	Window ID is no longer 0.
			dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
		}
	}
	else if(dwWindowID == 0xFFFFFFFF)	{
		//	They want the current frame....
		//	See if we even have one.
		if(XP_ContextCount(MWContextBrowser, TRUE) == 0)	{
			if(NULL != CFE_CreateNewDocWindow(NULL, NULL))	{
				//	Window ID is no longer 0.
				dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
			}
			else	{
				dwWindowID = 0;
			}
		}
		else	{
			if(FEU_GetLastActiveFrame(MWContextBrowser) != NULL)	{
				//	Should be safe.
				dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
			}
			else	{
                //  No last active frame, but more than one window....
                //  Just create a new one.  Can't pick one up arbitrarily.
                if(NULL != CFE_CreateNewDocWindow(NULL, NULL))  {
					dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
                }
                else {
				    dwWindowID = 0;
                }
			}
		}
	}
	else	{
		//	They are requesting a specific frame.
		//	See if we've got it.
		if(FEU_FindFrameByID(dwWindowID, MWContextBrowser) == NULL)	{
			dwWindowID = 0;
		}
	}
	
	//	Get the frame
    CAbstractCX *pCX = CAbstractCX::FindContextByID(dwWindowID);
    if(!pCX || !pCX->IsFrameContext() || !pCX->GetContext() || pCX->GetContext()->type != MWContextBrowser)
    {
        dwWindowID = 0;
    }
	
	//	See if the window exists, a value of 0 means failure at this
	//		point.
	if(dwWindowID == 0)	{
		//	Well, either the window didn't exist, or we couldn't create
		//		a new window.
		return(MakeArgs("DW", &dwWindowID));
	}
	
	//	So far so good.  Our current return value will be the window ID,
	//		until another failure further along.
	dwReturn = dwWindowID;
	
	//	Decide whether or not to force a reload.
	NET_ReloadMethod Reload = NET_DONT_RELOAD;
	if(dwFlags & 0x1 || dwFlags & 0x2)	{
		Reload = NET_NORMAL_RELOAD;
	}
	
	//	Create the URL structure to load up the URL.
	URL_Struct *pURL = NET_CreateURLStruct(csURL,  Reload);
	
	//	See if we've got some form to POST.
	if(csFormData.IsEmpty() != TRUE)	{
		//	We don't allow anything but a post....
		pURL->method = URL_POST_METHOD;
		
		//	We need to fill in the post data, memory is freed by netlib.
		char *cp_FormData = (char *)XP_ALLOC(csFormData.GetLength() + 1);
		strcpy(cp_FormData, csFormData);
		pURL->post_data = cp_FormData;

		//	Set the length of the data, don't include our final NULL
		//		I suppose.
		pURL->post_data_size = csFormData.GetLength();
		
		//	We need to manually fill in the Content-type:
		//	If there's not one specified, we default to
		//		applications/x-www-form-urlencoded
		if(csMimeType.IsEmpty() != TRUE)	{
			StrAllocCopy(pURL->post_headers, "Content-type: ");
			StrAllocCat(pURL->post_headers, csMimeType);			
		}
		else	{
			StrAllocCopy(pURL->post_headers,
				"Content-type: application/x-www-form-urlencoded");
		}
		StrAllocCat(pURL->post_headers, CRLF);

		//	Also now need to fill in content-length....
		char aBuffer[1024];
		sprintf(aBuffer, "Content-length: %d", csFormData.GetLength());
		StrAllocCat(pURL->post_headers, aBuffer);
		StrAllocCat(pURL->post_headers, CRLF);
	}
	
	//	Have the URL load.  We simply can't block our return value until
	//		all connections are completed for the window because we
	//		hose all the messaging.
	MWContext *pContext = pCX->GetContext();
    
	//	Here we should set up all progress callbacks.
	//	So whenever we get an Alert, or any progress messages, we'll
	//		be sending it on down to this callback.
	//	This must be cleared in GetURL_exit_routine
	if(csProgressApp.IsEmpty() == FALSE && pContext != NULL)	{
		//	Set up a progress app for this one.
		CNcapiUrlData *pNcapi = new CNcapiUrlData(ABSTRACTCX(pContext), pURL);
		pNcapi->SetProgressServer(csProgressApp);
	}

	if(pContext != NULL)	{
		if(csFileSpec.IsEmpty())	{
			//	Load up.
			ABSTRACTCX(pContext)->GetUrl(pURL, FO_CACHE_AND_PRESENT);
		}
		else	{
			//	Load to file.		
			//	Protect against re-entrancy
			if(pContext->save_as_name)	{
				XP_FREE(pContext->save_as_name);
				pContext->save_as_name = NULL;
			}
			pContext->save_as_name = strdup(csFileSpec);

			//	Load up.
			ABSTRACTCX(pContext)->GetUrl(pURL, FO_CACHE_AND_SAVE_AS);
	    }
	}
    
	return(MakeArgs("DW", &dwReturn));
}


//	Purpose:	Register a DDE viewer
//	Arguments:	hszItem	The arguement string.
//	Returns:	HDDEDATA	A boolean indicating success or failure of
//								the registration.
//	Comments:	Parameters withing hszItem are
//					qcsApplication	The DDE server which will handle
//										the requests.
//					qcsMIMEType		The mime type that the viewer wants
//										to handle.
//					dwFlags	Flags on how to handle:
//								0x00000001	Platform specific open document command.
//								0x00000002	Use of a QueryViewer DDE command, to ask for a file spec.
//								0x00000004	Use ViewDocFile DDE command.
//								0x00000008	Use ViewDocData, not supported.
//				The only registration we allow will be FO_PRESENT
//				There is a default if the flags is 0, which is
//					ViewDocFile method.
//	Revision History:
//		01-06-94	created GAB
HDDEDATA CDDEWrapper::RegisterViewer(HSZ& hszItem)
{
	//	Scan in our arguments.
	CString csApplication;
	CString csMimeType;
	DWORD dwFlags;
	ScanArgs(hszItem, "QCS,QCS,DW", &csApplication, &csMimeType, &dwFlags);
	
	TwoByteBool bRetval = TRUE;
	
	//	Extract out if we are going to be using the QueryViewer command.
	TwoByteBool bQueryViewer = (dwFlags & 0x2UL) != 0UL ? TRUE : FALSE;
	if(bQueryViewer == TRUE)	{
		dwFlags ^= 0x2;
	}
	
	//	Screen any flags we don't support
	if(dwFlags != 0x1 && dwFlags != 0x4)	{
		bRetval = FALSE;
		return(MakeArgs("BL", &bRetval));
	}
	
	//	Make sure the application and mime type aren't empty.
	if(csApplication.IsEmpty() || csMimeType.IsEmpty())	{
		bRetval = FALSE;
		return(MakeArgs("BL", &bRetval));
	}
	
	//	Build a special data object to be handed to each stream.
	//	Basically this will contain the DDE server to contact once we
	//		are receiving data, and will contain the flag representing
	//		how the data should be handled.
	//	We will also be marking this MimeType as un overrideable, until
	//		it is unregistered.
	CDDEStreamData *pCData = new CDDEStreamData(csApplication, csMimeType,
		dwFlags, bQueryViewer);

	//	Register a special converter for the mime type.
	if(FALSE == WPM_RegisterContentTypeConverter(
		(char *)(const char *)csMimeType, FO_PRESENT,
		(void *)pCData, dde_stream, TRUE))	{

		//	Couldn't do it, another remote control client is already
		//		using this mime type.
		delete pCData;
		bRetval = FALSE;		
	}
	
	return(MakeArgs("BL", &bRetval));
}


//	Purpose:	Determine the URL a temp file came from.
//	Arguments:	hszItem	The arguement string.
//	Returns:	HDDEDATA	The URL, or NULL on failure.
//	Comments:	Parameters withing hszItem are
//					qcsFileName	The file name to look up.
//	Revision History:
//		03-21-95	created GAB
HDDEDATA CDDEWrapper::QueryURLFile(HSZ& hszItem)
{
	//	Scan in our arguments.
	CString csFileName;
	ScanArgs(hszItem, "QCS", &csFileName);
    csFileName.MakeLower();
	
	CString csRetval;
	
    //  Find out if the file name is one we know about.
    csRetval = theApp.GetProfileString("Temporary File URL Resolution", csFileName, "");

    if(csRetval.IsEmpty())  {
        return(NULL);
    }
	
	return(MakeArgs("QCS", &csRetval));
}


//	Purpose:	Unregisters on the fly a DDE content type converter.
//	Arguments:	hszItem	Our arguments, in a string
//	Returns:	HDDEDATA	A boolean indicating the success of the
//								operation, which should always be TRUE.
//	Comments:	Our real arguments inside hszItem are
//					qcsApplication	The DDE server to unregister
//					qcsMimeType		The Mime type to unregister
//				The only registration we allow will be FO_PRESENT
//	Revision History:
//		01-06-95	created GAB
HDDEDATA CDDEWrapper::UnRegisterViewer(HSZ& hszItem)
{
	//	Retrieve our arguments.
	CString csApplication;
	CString csMimeType;
	ScanArgs(hszItem, "QCS,QCS", &csApplication, &csMimeType);
	
	TwoByteBool bRetval = TRUE;

	if(csApplication.IsEmpty() || csMimeType.IsEmpty())	{
		bRetval = FALSE;
		return(MakeArgs("BL", &bRetval));
	}
	
	CDDEStreamData *pCData =
		(CDDEStreamData *)WPM_UnRegisterContentTypeConverter(csApplication,
		csMimeType, FO_PRESENT);
		
	if(pCData == NULL)	{
		bRetval = FALSE;
	}
	else	{
		delete pCData;
	}

	return(MakeArgs("BL", &bRetval));
}


//	Purpose:	Connect to a service using a specific topic
//	Arguments:	cpService	The service name of the DDE server
//				hszTopic	The topic of the conversation to the server
//	Returns:	CDDEWrapper *	The conversation object if established,
//									otherwise NULL.
//	Comments:	Generic connection establishment.
//	Revision History:
//		01-05-95	created GAB
CDDEWrapper *CDDEWrapper::ClientConnect(const char *cpService,
	HSZ& hszTopic)
{
	CDDEWrapper *pConv = NULL;

	//	Make the service name into an HSZ.
	HSZ hszService = DdeCreateStringHandle(m_dwidInst, 
	                                       (char *) cpService,
										   CP_WINANSI);
	if(hszService == NULL)	{
		return(NULL);
	}
	
	//	Establish the connection.
	HCONV hConv = DdeConnect(m_dwidInst, hszService, hszTopic, NULL);
	if(hConv != NULL)	{
		//	We have a connection, all that's left to do is to create
		//		a CDDEWrapper, we'll be creating it with the wrong
		//		service name of course, but client connections no
		//		longer really care about the service connection number,
		//		all they need is the conversation handle.
		pConv = new CDDEWrapper(m_hsz[m_ServiceName], hszTopic, hConv);
	}
		
	//	Free off our created hsz.
	DdeFreeStringHandle(m_dwidInst, hszService);
	
	//	Our return value, sir.
	return(pConv);
}


//	Purpose:	Send the service app a begin progress verb.
//	Arguments:	pNcapi	The url data causing this to occur.
//				pService	The service name to connect to.
//				dwWindowID	The windowID which has progressed.
//				pMessage	The message to show in the server.
//	Returns:	DWORD	The transaction ID that the server sends us
//							to use in all other progress verbs.
//	Comments:	Netscape is a DDE client in this function.
//	Revision History:
//		01-05-95	created GAB
//		10-17-95	modified to use CNcapiUrlData
DWORD CDDEWrapper::BeginProgress(CNcapiUrlData *pNcapi, const char *pService,
	DWORD dwWindowID, const char *pMessage)
{
	DWORD dwTransactionID;

	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(pService, m_hsz[m_BeginProgress]);
	
	//	If we didn't connect, don't let the document try this again
	//		for this progress.
	if(pConv == NULL)	{
		pNcapi->ClearProgressServer();
		return(0L);
	}	

	//	Save our conversation, in case the server decides to disconnect
	//		behind our backs.
	HCONV hSaveConv = pConv->m_hConv;

	//	Create our Item arguemnts.
	CString csTemp = pMessage;
	HSZ hszItem = MakeItemArgs("DW,QCS", &dwWindowID, &csTemp);
	
	//	Do the transaction, expect a transaction ID back.
	HDDEDATA ddeTransaction = DdeClientTransaction(NULL, 0L,
		hSaveConv, hszItem, CF_TEXT, XTYP_REQUEST, m_Timeout, NULL);
	
	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);
	
	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv != NULL)	{
		delete pConv;
	}
	
	if(ddeTransaction == NULL)	{
		//	Server didn't want to process this.
		//	Don't try them again.
		pNcapi->ClearProgressServer();
	
		dwTransactionID = 0L;
	}
	else	{
		//	Scan in the progress ID, and get rid of the handle.
		//	Well, it would seem that they're not following their own
		//		spec.  They send us raw data, get it that way.
		//ScanArgs(ddeTransaction, "DW", &dwTransactionID);
		if(ddeTransaction != NULL)	{
			ScanDataArgs(ddeTransaction, "DW", &dwTransactionID);
			DdeFreeDataHandle(ddeTransaction);
		}
		else	{
			dwTransactionID = 0L;
		}
	}

	//	If the transaction ID is 0, then they don't want progress.
	if(dwTransactionID == 0)	{
		pNcapi->ClearProgressServer();
	}
	
	return(dwTransactionID);
}


//	Purpose:	Tell the DDE server our progress range.
//	Arguments:	pNcapi	The url data initiating this request.
//				pService	The server's service name.
//				dwTransactionID	The transaction ID of this progress.
//				dwMaxRange	The max range that we can achieve.
//	Returns:	void
//	Comments:	Netscape is the DDE client.
//	Revision History:
//		01-05-94	created GAB
//		10-17-95	modified to use CNcapiUrlData
void CDDEWrapper::SetProgressRange(CNcapiUrlData *pNcapi, const char *pService,
	DWORD dwTransactionID, DWORD dwMaxRange)
{
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(pService,
		m_hsz[m_SetProgressRange]);
	
	//	If we didn't connect, don't let the document try this again
	//		for this progress.
	if(pConv == NULL)	{
		pNcapi->ClearProgressServer();
		return;
	}

	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our item arguments.
	HSZ hszItem = MakeItemArgs("DW,DW", &dwTransactionID, &dwMaxRange);

	//	Do the transaction, expect nothing back XTYP_POKE
	if(FALSE == DdeClientTransaction(NULL, 0L, hSaveConv, hszItem,
		CF_TEXT, XTYP_POKE, m_Timeout, NULL))	{
		//	For some reason, this didn't fly.
		//	Don't let the document try this again.
		pNcapi->ClearProgressServer();
	}

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);
	
	pConv = GetConvObj(hSaveConv);
	if(pConv != NULL)	{
		delete pConv;
	}
}


//	Purpose:	Tell the DDE server that we have made progress.
//	Arguments:	pNcapi	The document initiating this request.
//				pServic	The server's service name
//				dwTransactionID	The transaction ID to send with the
//							message, aquired by a begin progress call.
//				pMessage	A message to send to the server, explaining
//								the progress message.
//				dwCurrent	A number, representing a range, which
//								was set by a set progress range call.
//	Returns:	TwoByteBool	TRUE	The server would like to discontinue
//									the download.
//						FALSE	Continue downloading.
//	Comments:	Netscape is a DDE client.
//	Revision History:
//		01-05-95	created GAB
//		10-17-95	modified to use CNcapiUrlData
TwoByteBool CDDEWrapper::MakingProgress(CNcapiUrlData *pNcapi, const char *pService,
	DWORD dwTransactionID, const char *pMessage, DWORD dwCurrent)
{
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(pService,
		m_hsz[m_MakingProgress]);
	
	//	If we didn't connect, don't let the document try this again
	//		for this progress.
	if(pConv == NULL)	{
		pNcapi->ClearProgressServer();
		return(FALSE);
	}

	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our item arguments.
	CString csMessage = pMessage;
	HSZ hszItem = MakeItemArgs("DW,QCS,DW", &dwTransactionID, &csMessage,
		&dwCurrent);

	//	Do the transaction, expect a TwoByteBool back.
	HDDEDATA ddeTransaction = DdeClientTransaction(NULL, 0L,
		hSaveConv, hszItem, CF_TEXT, XTYP_REQUEST, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);
	
	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
	
	TwoByteBool bResult = FALSE;
	
	if(ddeTransaction == NULL)	{
		//	Server didn't want to process this.
		//	Don't try them again.
		pNcapi->ClearProgressServer();
	}
	else	{
		//	Scan in the progress ID, and get rid of the handle.
		if(ddeTransaction != NULL)	{
			ScanDataArgs(ddeTransaction, "BL", &bResult);
			DdeFreeDataHandle(ddeTransaction);
		}
		else	{
			bResult = FALSE;
		}
	}
	
	return(bResult);
}


//	Purpose:	Tell the DDE server that the progress is ending.
//	Arguments:	pNcapi	The document initiating the request
//				pService	The server's service name
//				dwTransactionID	The transaction ID of these progress
//									messages.
//	Returns:	TwoByteBool	A success or failure boolean, we really don't
//							care, and will always end.
//				BUT WAIT, the spec says this, but in reality, it isn't
//							done.
//	Comments:	Netscape is the DDE client.
//	Revision History:
//		01-05-95	created GAB
//		10-17-95	modified to use CNcapiUrlData
void CDDEWrapper::EndProgress(CNcapiUrlData *pNcapi, const char *pService,
	DWORD dwTransactionID)
{
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(pService,
		m_hsz[m_EndProgress]);
	
	//	If we didn't connect, don't let the document try this again
	//		for this progress.
	if(pConv == NULL)	{
		pNcapi->ClearProgressServer();
		return;
	}

	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our item arguments.
	HSZ hszItem = MakeItemArgs("DW", &dwTransactionID);

	//	Do the transaction, expect nothing.
	DdeClientTransaction(NULL, 0L,
		hSaveConv, hszItem, CF_TEXT, XTYP_POKE, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);
	
	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
}


//	Purpose:	Send our progress app an error message.
//	Arguments:	pDoc	The document requesting this action
//				pService	The server's service name
//				pMessage	The message to send
//	Returns:	DWORD	The button pushed by the remote app, or error (0)
//							on failuer.
//	Comments:	All servers should consider this message as a failure to
//					load.
//	Revision History:
//		01-05-95	created GAB
//		10-17-95	modified to use CNcapiUrlData
DWORD CDDEWrapper::AlertProgress(CNcapiUrlData *pNcapi, const char *pService,
	const char *pMessage)
{
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(pService,
		m_hsz[m_Alert]);
	
	//	If we didn't connect, simply return, not everyone will have
	//		this verb.
	if(pConv == NULL)	{
		return(m_PushedError);
	}

	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our item arguments.
	CString csMessage = pMessage;
	DWORD dwType = m_AlertError;	// error box
	DWORD dwButtons = m_ButtonOk;	//	What buttons to show
	HSZ hszItem = MakeItemArgs("QCS,DW,DW", &csMessage, &dwType,
		&dwButtons);

	//	Do the transaction
	HDDEDATA ddeTransaction = DdeClientTransaction(NULL, 0L,
		hSaveConv, hszItem, CF_TEXT, XTYP_REQUEST, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);
	
	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
	
	DWORD dwResult = 0L;
	
	if(ddeTransaction != NULL)	{
		//	Scan in the return value, and get rid of the handle.
		ScanDataArgs(ddeTransaction, "DW", &dwResult);
		DdeFreeDataHandle(ddeTransaction);
	}
	
	return(dwResult);
}

//	Purpose:	Send our progress app a confirmation message.
//	Arguments:	pNcapi	The document requesting this action
//				pService	The server's service name
//				pMessage	The message to send
//	Returns:	DWORD	The button pushed by the remote app, or error (0)
//							on failuer.
//	Comments:
//	Revision History:
//		10-17-95	created
DWORD CDDEWrapper::ConfirmProgress(CNcapiUrlData *pNcapi, const char *pService,
	const char *pMessage)
{
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(pService,
		m_hsz[m_Alert]);
	
	//	If we didn't connect, simply return, not everyone will have
	//		this verb.
	if(pConv == NULL)	{
		return(m_PushedError);
	}

	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our item arguments.
	CString csMessage = pMessage;
	DWORD dwType = m_AlertQuestion;	// error box
	DWORD dwButtons = m_ButtonYesNo;	//	What buttons to show
	HSZ hszItem = MakeItemArgs("QCS,DW,DW", &csMessage, &dwType,
		&dwButtons);

	//	Do the transaction
	HDDEDATA ddeTransaction = DdeClientTransaction(NULL, 0L,
		hSaveConv, hszItem, CF_TEXT, XTYP_REQUEST, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);
	
	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
	
	DWORD dwResult = 0L;
	
	if(ddeTransaction != NULL)	{
		//	Scan in the return value, and get rid of the handle.
		ScanDataArgs(ddeTransaction, "DW", &dwResult);
		DdeFreeDataHandle(ddeTransaction);
	}
	
	return(dwResult);
}


//	Purpose:	Ask a DDE server what file we should save a file under,
//					one that we just downloaded.
//	Arguments:	pDData	Our download data, will hold everything needed
//							to establish the conversation.
//	Returns:	void
//	Comments:	Another topic should be called after this one in order
//		to tell the registered viewer that it's document is now ready.
//	Revision History:
//		01-06-95	created GAB
void CDDEWrapper::QueryViewer(CDDEDownloadData *pDData)
{
	//	Get our parameters to establish the conversation.
	CString csService = pDData->m_pCData->m_csServerName;
	
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(csService, m_hsz[m_QueryViewer]);
		
	if(pConv == NULL)	{
		//	There's not much else we can do here, simply return.
		return;
	}
	
	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our argument list.
	CString csURL;
	CString csMimeType;
	csMimeType = pDData->m_pCData->m_csMimeType;
	csURL = pDData->m_csURL;
	HSZ hszItem = MakeItemArgs("QCS,QCS", &csURL, &csMimeType); 
	
	//	Do the transaction, expect a FileSpec back.
	HDDEDATA ddeTransaction = DdeClientTransaction(NULL, 0L,
		hSaveConv, hszItem, CF_TEXT, XTYP_REQUEST, m_Timeout, NULL);	

	//	Get rid of our string handle, don't need it anymore; same with the
	//		connection.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);

	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}

	if(ddeTransaction == NULL)	{
		//	There's not much else we can do here, simply return.
		return;
	}

	//	Get the file name out of the data returned by the DDE server.
	CString csFileName;
	ScanDataArgs(ddeTransaction, "QCS", &csFileName);
	DdeFreeDataHandle(ddeTransaction);
	
	//	Move it over to where the server wants it.
	TRY	{
		CFile::Rename(pDData->m_csFileName, csFileName);
	}
	CATCH(CFileException, e)	{
		//	Couldn't rename for some reason.
		return;
	}
	END_CATCH

	//	Tell the download data, that it won't be necessary to delete this file.
	//	The viewer will take care of it.
	pDData->m_bDelete = FALSE;
	
	//	Change the name.
	pDData->m_csFileName = csFileName;
	
	//	Done.
	return;
}


//	Purpose: 		Tell a registered viewer that a document of it's type is loaded
//						and in a certain file.
//	Arguments:	pDData	The download instance specific data, contains
//							the file name.
//	Returns:	void
//	Comments:	Function will unregister any viewers that don't respond in
//					a fashion that we deem fit.
//	Revision History:
//		01-08-95	created GAB
void CDDEWrapper::ViewDocFile(CDDEDownloadData *pDData)
{
	//	Get our parameters to establish the conversation.
	CString csService = pDData->m_pCData->m_csServerName;
	
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(csService, m_hsz[m_ViewDocFile]);
		
	if(pConv == NULL)	{
		//	Well, Doh!, the server isn't responding.
		//	Disable it from further being our registered app.
		CString csMimeType = pDData->m_pCData->m_csMimeType;
		WPM_UnRegisterContentTypeConverter(csService, csMimeType,
			FO_PRESENT);
		delete pDData->m_pCData;
		
		//	There's not much else we can do here, simply return.
		return;
	}
	
	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;
	
	//	Create our argument list.
	CString csFileSpec = pDData->m_csFileName;
	CString csURL = pDData->m_csURL;
	CString csMimeType = pDData->m_pCData->m_csMimeType;
	DWORD dwWindowID = pDData->m_dwFrameID;
	HSZ hszItem = MakeItemArgs("QCS,QCS,QCS,DW", &csFileSpec, &csURL,
		&csMimeType, &dwWindowID);
		
	//	Do the transaction, expect nothing.
	DdeClientTransaction(NULL, 0L, hSaveConv, hszItem, CF_TEXT,
		XTYP_POKE, m_Timeout, NULL);		

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);

	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
	
	//	Done here.
	return;
}

//	Purpose:	Use a shell open to open the file name in our download data.
//	Arguments:	pDData	Data specific to this download.
//	Returns:	void
//	Comments:	Support of another RegisterViewer command.
//	Revision History:
//		01-14-95	created GAB
void CDDEWrapper::OpenDocument(CDDEDownloadData *pDData)
{
	//	Well, we don't really do any DDE per se here, but this is where the
	//		funciton logically belongs.
	//	Attempt to have windows open the file specified.
	//	This is a fire and forget mechanism, though we will unregister the
	//		DDE content type converter on an error.
	HINSTANCE hReturns = ::ShellExecute(::GetDesktopWindow(), NULL, pDData->m_csFileName, NULL, NULL, SW_SHOW);
	if(hReturns <= (HINSTANCE)32)	{
		//	There seemed to have been an error.
		//	Unregister the content type converter now.
		CString csService = pDData->m_pCData->m_csServerName;
		CString csMimeType = pDData->m_pCData->m_csMimeType;
		WPM_UnRegisterContentTypeConverter(csService, csMimeType,
			FO_PRESENT);
		delete pDData->m_pCData;
	}
}


//	Purpose:	Exit the Netscape application.
//	Arguments:	hszItem	Our arguments.
//				hData	Any data in particular that the server is sending us.
//	Returns:	HDDEDATA	Wether or not we completed this operation.
//	Comments:	Netscape will not always exit on this command, since we may be automated.
//	Revision History:
//		01-13-95	created GAB
//		10-17-95	modified to send the last active frame, if around, the
//						ID_APP_EXIT WM_COMMAND message.
HDDEDATA CDDEWrapper::Exit(HSZ& hszItem, HDDEDATA& hData)
{
	//	See if any frame windows are around.
	CFrameWnd *pWnd = FEU_GetLastActiveFrame();
	if(pWnd != NULL)	{
		//	Send it the app exit message.
		pWnd->PostMessage(WM_COMMAND, ID_APP_SUPER_EXIT);
		return((HDDEDATA)DDE_FACK);
	}

    return((HDDEDATA)DDE_FNOTPROCESSED);
}


//	Purpose:	Get some misc information about a Netscape window.
//	Arguments:	hstItem	Our arugments, see below.
//	Returns:	HDDEDATA	Contains
//					qcsUrl	The current URL loaded in the window.
//					qcsTitle	The current title of the window.
//	Comments:	This is useless, but implementable.
//	Revision History:
//		01-13-95	created GAB
HDDEDATA CDDEWrapper::GetWindowInfo(HSZ& hszItem)
{
	//	Retrieve our arguments.
	DWORD dwFrameID;
	ScanArgs(hszItem, "DW", &dwFrameID);
	
	//	We'll let dwFrameID be 0xffffffff for the default window.
	if(dwFrameID == 0xFFFFFFFF)	{
		dwFrameID = FEU_GetLastActiveFrameID(MWContextBrowser);
		if(dwFrameID == 0)	{
			//	This is going to fail, no active window.
			return(NULL);
		}
	}
	
	//	Okay, all we have left to do is obtain the current title of the window,
	//		and the current loaded URL.
    CAbstractCX *pCX = CAbstractCX::FindContextByID(dwFrameID);
    if(pCX && pCX->GetContext() && pCX->GetContext()->type != MWContextBrowser) {
        pCX = NULL;
    }

	if(pCX == NULL)	{
		return(NULL);
	}
	
	//	Get the stuff.
	CString csTitle;
	CString csUrl;
    CString csName;

    if(pCX->GetContext())   {
        if(pCX->GetContext()->title)    {
            csTitle = pCX->GetContext()->title;
        }
	    if(pCX->GetContext()->hist.cur_doc_ptr)	{
		    if(pCX->GetContext()->hist.cur_doc_ptr->address)	{
			    csUrl = pCX->GetContext()->hist.cur_doc_ptr->address;
		    }
	    }
        if(pCX->GetContext()->name)   {
            csName = pCX->GetContext()->name;
        }
    }
	
	//	Create our return arguments.
	return(MakeArgs("QCS,QCS,QCS", &csUrl, &csTitle, &csName));
}

//	Purpose:	List all the open Netscape windows.
//	Arguments:	hszItem	ignored.
//	Returns:	HDDEDATA	An array of DWORDs corresponding to the windows.
//	Comments:	We will manually create the return data in this function,
//					as it follows no previously coded standard.
//				We will not return the ID of the minimized window.
//	Revision History:
//		01-13-94	created GAB
HDDEDATA CDDEWrapper::ListWindows(HSZ& hszItem)
{
	//	Count our windows.
	DWORD dwFrames = XP_ContextCount(MWContextBrowser, FALSE);
	
	//	No frames, don't do this.
	if(dwFrames == 0)	{
		return(NULL);
	}
	
	DWORD *pData = new DWORD[dwFrames + 1];
	DWORD dwCounter = 0;
	
	//	Loop through each context, taking only browser style contexts.
	MWContext *pTraverseContext = NULL;
	CAbstractCX *pTraverseCX = NULL;
	XP_List *pTraverse = XP_GetGlobalContextList();
	while (pTraverseContext = (MWContext *)XP_ListNextObject(pTraverse)) {
		if(pTraverseContext != NULL && ABSTRACTCX(pTraverseContext) != NULL)	{
			pTraverseCX = ABSTRACTCX(pTraverseContext);

			if(pTraverseCX->GetContext()->type == MWContextBrowser &&
				pTraverseCX->IsFrameContext() == TRUE &&
				pTraverseCX->IsDestroyed() == FALSE)	{
				CWinCX *pWinCX = (CWinCX *)pTraverseCX;
				if(pWinCX->GetFrame()->GetFrameWnd() != NULL)	{
					//	Looks like this is the type of context we'll list.
					*(pData + dwCounter) = pWinCX->GetContextID();
					dwCounter++;
				}
			}

		}
	}

	//	Null terminate the list.
	*(pData + dwCounter) = (DWORD)0;

	//	allocate some data to return to the caller.
	HDDEDATA hData = DdeCreateDataHandle(m_dwidInst, (unsigned char *)pData, (dwCounter + 1) * sizeof(DWORD), 0, m_hsz[m_ServiceName],
		CF_TEXT, 0);

	delete [] pData;	
	return(hData);
}


//	Purpose:	Parse a main, and relative URL, returning the fully qualified URL.
//	Arguments:	hszItem	The arguments.  See below.
//	Returns:	HDDEDATA	A null terminated string representing the fully qualified URL.
//	Comments:	Nothing special here, folks.
//	Revision History:
//		01-13-95	created GAB
HDDEDATA CDDEWrapper::ParseAnchor(HSZ& hszItem)
{
	//	Get our arguments.
	CString csMainURL;
	CString csRelativeURL;
	ScanArgs(hszItem, "QCS,QCS", &csMainURL, &csRelativeURL);
	
	//	create the URL.
	char *cpURL = NET_MakeAbsoluteURL((char *)(const char *)csMainURL, (char *)(const char *)csRelativeURL);
	CString csURL;
	if(cpURL != NULL)	{
		csURL = cpURL;
		XP_FREE(cpURL);
	}
	
	//	Create our return value.
	return(MakeArgs("QCS", &csURL));
}

//	Purpose:	Report the current DDE API version back to the caller.
//	Arguments:	hszItem	none and ignored.
//	Returns:	HDDEDATA which actually contains the version.
//	Comments:	Version control information for the API.
//	Revision History:
//		01-17-95	created GAB
HDDEDATA CDDEWrapper::Version(HSZ& hszItem)
{
	return(MakeArgs("DW", &dwDDEVersion));
}

//	Purpose:	Register a particular DDE server to handle URLs of a particular type.
//	Arguments:	hszItem	Arguments, see below.
//	Returns:	HDDEDATA	TRUE	registered
//							FALSE	unable to register, another applciation is handling it.
//	Comments:
//	Revision History:
//		01-18-95	created GAB
HDDEDATA CDDEWrapper::RegisterProtocol(HSZ& hszItem)
{
	//	Get our arguments.
	CString csServer;
	CString csProtocol;
	ScanArgs(hszItem, "QCS,QCS", &csServer, &csProtocol);
	
	//	If either is empty, we'll just fail.
	if(csServer.IsEmpty() || csProtocol.IsEmpty())	{
		return(NULL);
	}
	
	//	Need to see if something is already registered to handle this protocol.
	TwoByteBool bRetval = CDDEProtocolItem::DDERegister(csProtocol, csServer);
	
	return(MakeArgs("BL", &bRetval));
}

//	Purpose:	Register a particular DDE server to handle URLs of a particular type.
//	Arguments:	hszItem	Arguments, see below.
//	Returns:	HDDEDATA	TRUE	registered
//							FALSE	unable to register, another applciation is handling it.
//	Comments:
//	Revision History:
//		01-18-95	created GAB
HDDEDATA CDDEWrapper::UnRegisterProtocol(HSZ& hszItem)
{
	//	Get our arguments.
	CString csServer;
	CString csProtocol;
	ScanArgs(hszItem, "QCS,QCS", &csServer, &csProtocol);
	
	//	If either is empty, we'll just fail.
	if(csServer.IsEmpty() || csProtocol.IsEmpty())	{
		return(NULL);
	}

	TwoByteBool bRetval = CDDEProtocolItem::DDEUnRegister(csProtocol, csServer);
	return(MakeArgs("BL", &bRetval));	
}

//	Purpose:	Have an external protocol handler open a URL.
//	Arguments:	csServiceName	The name of the external service application.
//				pURL	The URL to perform the load on; contains possible form data also.
//				pContext	The context to load from (windowID).
//				iFormatOut	Wether or not to save the URL.
//	Returns:	TwoByteBool	TRUE	External application handling the request.
//						FALSE	External application failed to handle.
//	Comments:	Will automatically deregister the external applciation if failure to connect.
//	Revision History:
//		01-18-94	created GAB
TwoByteBool CDDEWrapper::OpenURL(CString csProtocol, CString csServiceName, URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut)
{
	//	Get the conversation going.
	CDDEWrapper *pConv = ClientConnect(csServiceName, m_hsz[m_OpenURL]);

	if(pConv == NULL)	{
		//	Well, Doh!, the server isn't responding.
		//	Disable it from further being our registered app.
		CDDEProtocolItem::DDEUnRegister(csProtocol, csServiceName);
		
		//	There's not much else we can do here, simply return.
		return(FALSE);
	}
	
	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;

	//	See if we need to save the file.
    //  The filename was saved in the context some time ago.
	CString csSaveAs;
    if((iFormatOut & FO_SAVE_AS) == FO_SAVE_AS)   {
	    if(pContext->save_as_name != NULL)	{
		    csSaveAs = pContext->save_as_name;

			//	Steal it from the old context.
			free(pContext->save_as_name);
			pContext->save_as_name = NULL;
	    }
        else    {
            //  There's no name to save it under, so that must mean that the user either
            //      cancelled a save, or we're simply broken.

            //  Be sure to get rid of the conversation too.
	        //	Make sure we still have a conversation, if so, delete the object.
	        DdeDisconnect(hSaveConv);
	        pConv = GetConvObj(hSaveConv);
	        if(pConv)	{
		        delete pConv;
	        }

            return(FALSE);
        }
    }
    //  Screen types we don't want to handle.
    else if((iFormatOut & FO_PRESENT) != FO_PRESENT)    {
        //  Be sure to get rid of the conversation too.
	    //	Make sure we still have a conversation, if so, delete the object.
	    DdeDisconnect(hSaveConv);
	    pConv = GetConvObj(hSaveConv);
	    if(pConv)	{
		    delete pConv;
	    }

        return(FALSE);
    }
	
	//	Create our argument list.
	CString csURL = pURL->address;
	DWORD dwWindowID = FE_GetContextID(pContext);
	DWORD dwFlags = 0UL;
	CString csFormData;
	if(pURL->post_data != NULL)	{
		csFormData = pURL->post_data;
	}
	CString csPostMimeType;
	if(pURL->post_headers != NULL)	{
		//	Need to extract the content type.
		char *pFind = strcasestr(pURL->post_headers, "Content-type:");
		if(pFind != NULL)	{
			while(*pFind != ':')	{
				pFind++;
			}
			pFind++;
			
			while(*pFind != '\0' && isspace(*pFind))	{
				pFind++;
			}
			
			csPostMimeType = pFind;
			csPostMimeType = csPostMimeType.SpanExcluding("\r\n");
		}
	}
	CString csProgressApp;	//	leave empty for now.
	
	HSZ hszItem = MakeItemArgs("QCS,QCS,DW,DW,QCS,QCS,CS", &csURL, &csSaveAs, &dwWindowID, &dwFlags, &csFormData,
		&csPostMimeType, &csProgressApp);
		
	//	Do the transaction, expect return value.
	HDDEDATA hData = DdeClientTransaction(NULL, 0L, hSaveConv, hszItem, CF_TEXT,
		XTYP_REQUEST, m_Timeout, NULL);		

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);

	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
	
	//	See if we've gotten a return value.  Failure to give us a value will cause use to unregister the app.
	if(hData == NULL)	{
		CDDEProtocolItem::DDEUnRegister(csProtocol, csServiceName);	
		return(FALSE);
	}
	
	//	We've got a return value.  Determine what it is.
	DWORD dwRetval;
	ScanDataArgs(hData, "DW", &dwRetval);
	DdeFreeDataHandle(hData);
	
	//	Check for failure.
	if(dwRetval == 0 || dwRetval == 0xFFFFFFFFUL)	{
		return(FALSE);
	}
	
	//	Done.
    //  Be sure to let the frame know it's not saving any file, regardless, since we
    //      may have handled it.
	return(TRUE);
}

//	Purpose:	Register a DDE server for URL echo.
//	Arguments:	hszItem	our arguments.
//				hData	Disregard, not used.
//	Returns:	HDDEDATA	Always DDE_FACK, acknowleding that we processed this.
//	Comments:
//	Revision History:
//		01-18-95	created GAB.
HDDEDATA CDDEWrapper::RegisterURLEcho(HSZ& hszItem, HDDEDATA& hData)
{
	//	Scan in our arguments.
	CString csServiceName;
	ScanArgs(hszItem, "QCS", &csServiceName);

	//	Have the URL echo class handle it.
	CDDEEchoItem::DDERegister(csServiceName);

	return((HDDEDATA)DDE_FACK);
}

//	Purpose:	Unregister a DDE server from URL echo.
//	Arguments:	hszItem	our arugumetns.
//				hData	Disregard.
//	Returns:	HDDEDATA	always DDE_FACK, unless invalid
//	Comments:
//	Revision History:
//		01-18-95	created GAB
HDDEDATA CDDEWrapper::UnRegisterURLEcho(HSZ& hszItem, HDDEDATA& hData)
{
	//	Scan in our arguments.
	CString csServiceName;
	ScanArgs(hszItem, "QCS", &csServiceName);

	//	Have the URL echo class handle it.
	if(CDDEEchoItem::DDEUnRegister(csServiceName) == TRUE)  {
	    return((HDDEDATA)DDE_FACK);
    }

    return((HDDEDATA)DDE_FNOTPROCESSED);
}

//	Purpose:	Echo a URL event to a server.
//	Arguments:	pItem	The Echo object causing this (so we can get our service name out of it, and anything else.
//				csURL	The url loaded.
//				csMimeType	The mime type of the url.
//				dwWindowID	The window performing the load.
//				csReferrer	The referrer URL.
//	Returns:	void
//	Comments:
//	Revision History:
//		01-18-95	created GAB
void CDDEWrapper::URLEcho(CDDEEchoItem *pItem, CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer)
{
	//	Get the server name.
	CString csServiceName = pItem->GetServiceName();
	
	//	Establish the connection to homeworld.
	CDDEWrapper *pConv = ClientConnect(csServiceName, m_hsz[m_URLEcho]);

	if(pConv == NULL)	{
		//	Well, Doh!, the server isn't responding.
		//	Disable it from further being our registered app.
		CDDEEchoItem::DDEUnRegister(csServiceName);
		
		//	There's not much else we can do here, simply return.
		return;
	}
	
	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;

	//	Create our argument list.
	HSZ hszItem = MakeItemArgs("QCS,QCS,DW,QCS", &csURL, &csMimeType, &dwWindowID, &csReferrer);
		
	//	Do the transaction, expect nothing.
	DdeClientTransaction(NULL, 0L, hSaveConv, hszItem, CF_TEXT,
		XTYP_POKE, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);

	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
}

//	Purpose:	Register a DDE server to monitor a certain window's close.
//	Arguments:	hszItem	the argumetns, the server and the window
//	Returns:	HDDEDATA	TRUE or FALSE, FALSE there was no window.
//	Comments:
//	Revision History:
//		01-19-95	created GAB
//
//	Purpose:	Register a particular DDE server to handle URLs of a particular type.
//	Arguments:	hszItem	Arguments, see below.
//	Returns:	HDDEDATA    dwWindowID
//	Comments:
//	Revision History:
//		01-18-95	created GAB
HDDEDATA CDDEWrapper::RegisterWindowChange(HSZ& hszItem)
{
	//	Get our arguments.
	CString csServer;
	DWORD dwWindowID;
	ScanArgs(hszItem, "QCS,DW", &csServer, &dwWindowID);
	
	//	If server is empty, we'll just fail.
	if(csServer.IsEmpty())	{
		return(NULL);
	}

	//	Sign up to watch the window, this possibly fails.
	if(FALSE == CDDEWindowChangeItem::DDERegister(csServer, dwWindowID))    {
        dwWindowID = 0;
	}

	
	return(MakeArgs("DW", &dwWindowID));
}

//	Purpose:	Unregister a window to be monitored.
//	Arguments:	hszItem	The arguments, the server ,and the windowID.
//	Returns:	HDDEDATA TRUE or FALSE, FALSE there was no prior registration.
//	Comments:	
//	Revision History:
//		01-19-95	created GAB
HDDEDATA CDDEWrapper::UnRegisterWindowChange(HSZ& hszItem)
{
	//	Get our arguments.
	CString csServer;
	DWORD dwWindowID;
	ScanArgs(hszItem, "QCS,DW", &csServer, &dwWindowID);
	
	//	If server is empty, we'll just fail.
	if(csServer.IsEmpty())	{
		return(NULL);
	}

	//	Sign up to watch the window, this possibly fails.
	TwoByteBool bRetval = CDDEWindowChangeItem::DDEUnRegister(csServer, dwWindowID);	
	
	return(MakeArgs("BL", &bRetval));
}

//	Purpose:	Send a message to the registere monitoring server that the window is chaning.
//	Arguments:	pItem	The registry of window close monitors.
//              iChange The type of change ocurring.
//				bExiting	Wether or not we believe Netscape is actually exiting.
//              dwX The X position of the window.
//              dwY The Y position of the window.
//              dwCX    The width of the window.
//              dwCY    The height of the window.
//	Returns:	void
//	Comments:
//	Revision History:
//		01-19-95	created GAB
//      02-01-95    modified extensively to handle multiple different types of window actions.
void CDDEWrapper::WindowChange(CDDEWindowChangeItem *pItem, int iChange, TwoByteBool bExiting, DWORD dwX, DWORD dwY, DWORD dwCX, DWORD dwCY)
{
	//	Get the server name.
	CString csServiceName = pItem->GetServiceName();
	
	//	Establish the connection to homeworld.
	CDDEWrapper *pConv = ClientConnect(csServiceName, m_hsz[m_WindowChange]);

	if(pConv == NULL)	{
		//	Well, Doh!, the server isn't responding.
		//	There's not much else we can do here, simply return.
		//	Unregistration happens elsewhere.
		return;
	}
	
	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;

    //  Construct the flags for the call, and our calling format.
    DWORD dwFlags = 0;
    const char *pFormat;
    switch(iChange) {
        case CDDEWindowChangeItem::m_Close: {
            dwFlags |= 0x00000010UL;
            if(bExiting == TRUE)    {
                dwFlags |= 0x00010000UL;
            }
            pFormat = "DW,DW";
            break;
        }
        case CDDEWindowChangeItem::m_Size:  {
            dwFlags |= 0x00000001UL;
            pFormat = "DW,DW,DW,DW,DW,DW";
            break;
        }
        case CDDEWindowChangeItem::m_Maximize:  {
            dwFlags |= 0x00000002UL;
            pFormat = "DW,DW";
            break;
        }
        case CDDEWindowChangeItem::m_Minimize:  {
            dwFlags |= 0x00000008UL;
            pFormat = "DW,DW";
            break;
        }
        case CDDEWindowChangeItem::m_Normalize: {
            dwFlags |= 0x00000004UL;
            pFormat = "DW,DW";
            break;
        }
        default:    {
            //  Not handled, this is bad.
            pFormat = "DW,DW";
            ASSERT(0);
            break;
        }
    }

	//	Create our argument list.
	DWORD dwWindowID = pItem->GetWindowID();
	HSZ hszItem = MakeItemArgs(pFormat, &dwWindowID, &dwFlags, &dwX, &dwY, &dwCX, &dwCY);
		
	//	Do the transaction, expect nothing.
	DdeClientTransaction(NULL, 0L, hSaveConv, hszItem, CF_TEXT,
		XTYP_POKE, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);

	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
}

//	Purpose:	Retrieves a file from the local file system and displays it in a given frame.
//	Arguments:	hszArgs	A string representing our real parameters.
//	Returns:	HDDEDATA	Returns the actual WindowID of the window
//								the performed the open, where 0 means
//								the operation failed, and 0xFFFFFFFF
//								means the data was not of an appropriate
//								MIME type to display in a Web browser....
//	Comments:	Parameters within hszItem are
//		qcsFileName	the file on the local file system that Netscape should attempt to load.
//		qcsMimeType is the mime type of the file.
//		dwWindowID is the Netscape window in which to perform the load.
//			A value of 0x0 requests Netscape to load into a new window.
//			A value of 0xFFFFFFFF requests Netscape to load into the last active window.
//		qcsURL is the original URL of the document to reload if necessary.  Will only attempt if can't
//			open the file for reading.
//	Revision History:
//		01-19-95	created GAB
HDDEDATA CDDEWrapper::ShowFile(HSZ& hszArgs)
{
	DWORD dwReturn = 0L;
	
	//	Obtain our arguments.
	CString csFileName;
	CString csMimeType;
	DWORD dwWindowID;
	CString csURL;
	ScanArgs(hszArgs, "QCS,QCS,DW,QCS", &csFileName, &csMimeType, &dwWindowID, &csURL);
		
	//	Get the appropriate window.
	if(dwWindowID == 0)	{
		//	They want a new window.
		if(NULL != CFE_CreateNewDocWindow(NULL, NULL))	{
			//	Window ID is no longer 0.
			dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
		}
	}
	else if(dwWindowID == 0xFFFFFFFF)	{
		//	They want the current frame....
		//	See if we even have one.
		if(XP_ContextCount(MWContextBrowser, TRUE) == 0)	{
			if(NULL != CFE_CreateNewDocWindow(NULL, NULL))	{
				dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
			}
			else	{
				dwWindowID = 0;
			}
		}
		else	{
			if(FEU_GetLastActiveFrame(MWContextBrowser) != NULL)	{
				//	Should be safe.
				dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
			}
			else	{
				dwWindowID = 0;
		        if(NULL != CFE_CreateNewDocWindow(NULL, NULL))	{
			        dwWindowID = FEU_GetLastActiveFrameID(MWContextBrowser);
		        }
			}
		}
	}
	else	{
		//	They are requesting a specific frame.
		//	See if we've got it.
		if(FEU_FindFrameByID(dwWindowID, MWContextBrowser) == NULL)	{
			dwWindowID = 0;
		}
	}
	
	CAbstractCX *pCX = NULL;
    if(dwWindowID)  {
        pCX = CAbstractCX::FindContextByID(dwWindowID);
        if(NULL == pCX ||
            pCX->IsDestroyed() ||
            pCX->IsFrameContext() == FALSE ||
            pCX->GetContext()->type != MWContextBrowser)   {
            dwWindowID = 0;
            pCX = NULL;
        }
    }

	//	See if the window exists, a value of 0 means failure at this
	//		point.
	if(dwWindowID == 0)	{
		//	Well, either the window didn't exist, or we couldn't create
		//		a new window.
		return(MakeArgs("DW", &dwWindowID));
	}
	
	//	So far so good.  Our current return value will be the window ID,
	//		until another failure further along.
	dwReturn = dwWindowID;
	
	//	Create the URL structure to load up the URL.
	//	We do this first by seeing if we can access the file, if so go on, and if
	//		not use the URL that they presented us with.
	URL_Struct *pURL = NULL;
	if(_access(csFileName, 0x4) == -1)	{
		//	Use the URL
		pURL = NET_CreateURLStruct(csURL, NET_DONT_RELOAD);
	}
	else	{
		//	Use the file name.
		CString csURL;
		WFE_ConvertFile2Url(csURL, csFileName);
		pURL = NET_CreateURLStruct(csURL, NET_DONT_RELOAD);	
	}
	
	//	Manually assign in the mime type of the URL struct.
	if(csMimeType.IsEmpty() == FALSE)	{
		pURL->content_type = strdup(csMimeType);
	}
	
	//	Have the URL load.  We simply can't block our return value until
	//		all connections are completed for the window because we
	//		hose all the messaging.
	pCX->GetUrl(pURL, FO_CACHE_AND_PRESENT);
    
	return(MakeArgs("DW", &dwReturn));
}

//	Purpose:    Change our window position/attributes.
//	Arguments:  hszItem Our arguments.
//              hData   Data, ignored.
//	Returns:    HDDEDATA    TRUE or FALSE, depending on successful completion.
//	Comments:   This should probably by XTYP_REQUEST, as they may specify an invalid window ID, but they get the candy they want.
//              They could always check for valid IDs themselves.
//	Revision History:
//      02-01-95    created GAB
HDDEDATA CDDEWrapper::WindowChange(HSZ& hszItem, HDDEDATA& hData)
{
    //  Scan in our argumetnts.
    DWORD dwWindowID;
    DWORD dwFlags;
    DWORD dwX;
    DWORD dwY;
    DWORD dwCX;
    DWORD dwCY;

    ScanArgs(hszItem, "DW,DW,DW,DW,DW,DW", &dwWindowID, &dwFlags, &dwX, &dwY, &dwCX, &dwCY);

    //  Figure out if the frame exists.
    CAbstractCX *pCX = CAbstractCX::FindContextByID(dwWindowID);
    CFrameWnd *pFrame =  FEU_FindFrameByID(dwWindowID);
    if(pFrame == NULL || dwWindowID == 0 || NULL == pCX || pCX->IsGridCell())  {
        return((HDDEDATA)DDE_FNOTPROCESSED);
    }

    TwoByteBool bDidSomething = FALSE;

    //  Figure out, via the flags, what in the hell we are doing.
    if(dwFlags & 0x00000001UL)  {
        //  We're changing size.
        //  X and Y are always significant, though the others may not be....
        if(dwCX == 0)   {
            //  Determine what this should really be.
            RECT rDim;
            pFrame->GetWindowRect(&rDim);
            dwCX = rDim.right - rDim.left;
        }
        if(dwCY == 0)   {
            //  Determine what this should really be.
            RECT rDim;
            pFrame->GetWindowRect(&rDim);
            dwCX = rDim.bottom - rDim.top;
        }

        pFrame->MoveWindow(CASTINT(dwX), CASTINT(dwY), CASTINT(dwCX), CASTINT(dwCY));
        bDidSomething = TRUE;
    }

    if(dwFlags & 0x00000002UL)  {
        //  We're maximizing.
        pFrame->ShowWindow(SW_SHOWMAXIMIZED);
        bDidSomething = TRUE;
    }

    if(dwFlags & 0x00000004UL)  {   
        //  We're normalizing.
        pFrame->ShowWindow(SW_SHOWNORMAL);
        bDidSomething = TRUE;
    }

    if(dwFlags & 0x00000008UL)    {
        //  We're minimizing.
        pFrame->ShowWindow(SW_MINIMIZE);
        bDidSomething = TRUE;
    }

    if(dwFlags & 0x00000010UL)  {
        //  We're closing.
        pFrame->PostMessage(WM_CLOSE);
        bDidSomething = TRUE;
    }

    if(bDidSomething == TRUE)   {
        return((HDDEDATA)DDE_FACK);
    }
    else    {
        return((HDDEDATA)DDE_FNOTPROCESSED);
    }
}

//	Purpose:    Pure hell.
//	Arguments:  hszItem The arguments, the transaction ID to stop loading.
//              hData is ignored.
//	Returns:    HDDEDATA    TRUE, always work.
//	Comments:   We need to traverse all windows, hence all frames, and then documents, to find out the Transaction ID.
//	Revision History:
//      02-01-95    created GAB
//		10-17-95	Modified to search for the transaction ID in a different manner.
HDDEDATA CDDEWrapper::CancelProgress(HSZ& hszItem, HDDEDATA& hData)
{
    //  Get the argument.
    DWORD dwTransactionID;
    ScanArgs(hszItem, "DW", &dwTransactionID);


	//	Go through all contexts, searching for the one with the specific
	//		transaction ID.
	int iTraverseIndex;
	MWContext *pTraverseContext = NULL;
	CAbstractCX *pTraverseCX = NULL;
	XP_List *pTraverse = XP_GetGlobalContextList();
	while (pTraverseContext = (MWContext *)XP_ListNextObject(pTraverse)) {
		if(pTraverseContext != NULL && ABSTRACTCX(pTraverseContext) != NULL)	{
			pTraverseCX = ABSTRACTCX(pTraverseContext);

			if(pTraverseCX->m_pNcapiUrlData != NULL)	{
				if(pTraverseCX->m_pNcapiUrlData->GetTransactionID())	{
					if(pTraverseCX->m_pNcapiUrlData->GetTransactionID() == dwTransactionID)	{
						//	Found it, cancel it.
						pTraverseCX->Interrupt();
				        return((HDDEDATA)DDE_FACK);
					}
				}
			}
		}
	}

	//	Not found, not valid.
    return((HDDEDATA)DDE_FNOTPROCESSED);
}

HDDEDATA CDDEWrapper::ListFrameChildren(HSZ& hszItem)
{
    HDDEDATA hRetval = NULL;
	DWORD dwCXID;
	ScanArgs(hszItem, "DW", &dwCXID);

    CWinCX *pCX = GetContext(dwCXID);
    if(pCX && pCX->IsGridParent()) {
        XP_List *pChildren = pCX->GetContext()->grid_children;
        DWORD dwCount = XP_ListCount(pChildren);
        if(dwCount) {
            //  Allocate a buffer for the children IDs.
            DWORD *pBuf = new DWORD[dwCount + 1];
            if(pBuf)    {
                memset(pBuf, 0, CASTSIZE_T(sizeof(DWORD) * (dwCount + 1)));

                //  Go through the list figuring out the context ID and
                //      assigning it in.
                void *pTraverse;
                DWORD dwIndex = 0;
                while(pTraverse = XP_ListNextObject(pChildren)) {
                    CWinCX *pChildCX = WINCX(((MWContext *)pTraverse));
                    pBuf[dwIndex] = pChildCX->GetContextID();
                    dwIndex++;
                }

                //  Convert
                hRetval = DdeCreateDataHandle(
                    m_dwidInst,
                    (unsigned char *)pBuf,
                    (dwCount + 1) * sizeof(DWORD),
                    0,
                    m_hsz[m_ServiceName],
                    CF_TEXT, 0);

                delete [] pBuf;
                pBuf = NULL;
            }
        }
    }

    return(hRetval);
}

HDDEDATA CDDEWrapper::GetFrameParent(HSZ& hszItem)
{
	DWORD dwCXID;
	ScanArgs(hszItem, "DW", &dwCXID);

    CWinCX *pCX = GetContext(dwCXID);
    if(pCX && pCX->IsGridCell()) {
        DWORD dwParentID = WINCX(pCX->GetParentContext())->GetContextID();
        return(MakeArgs("DW", &dwParentID));
    }

    return(NULL);
}

HDDEDATA CDDEWrapper::RegisterStatusBarChange(HSZ& hszItem)
{
	//	Get our arguments.
	CString csServer;
	DWORD dwWindowID;
	ScanArgs(hszItem, "QCS,DW", &csServer, &dwWindowID);
	
	//	If server is empty, we'll just fail.
	if(csServer.IsEmpty())	{
		return(NULL);
	}

	if(FALSE == CDDEStatusBarChangeItem::DDERegister(csServer, dwWindowID))    {
        dwWindowID = 0;
	}
	
	return(MakeArgs("DW", &dwWindowID));
}

HDDEDATA CDDEWrapper::UnRegisterStatusBarChange(HSZ& hszItem)
{
	//	Get our arguments.
	CString csServer;
	DWORD dwWindowID;
	ScanArgs(hszItem, "QCS,DW", &csServer, &dwWindowID);
	
	//	If server is empty, we'll just fail.
	if(csServer.IsEmpty())	{
		return(NULL);
	}

	TwoByteBool bRetval = CDDEStatusBarChangeItem::DDEUnRegister(csServer, dwWindowID);	
	
	return(MakeArgs("BL", &bRetval));
}

void CDDEWrapper::StatusBarChange(CDDEStatusBarChangeItem *pItem, LPCSTR lpStatusMsg)
{
	CString csStatusMsg = lpStatusMsg;

	//	Get the server name.
	CString csServiceName = pItem->GetServiceName();
	
	//	Establish the connection to homeworld.
	CDDEWrapper *pConv = ClientConnect(csServiceName, m_hsz[m_StatusBarChange]);

	if(pConv == NULL)	{
		//	The server isn't responding.
		//	There's not much else we can do here, simply return.
		//	Unregistration happens elsewhere.
		return;
	}
	
	//	Save the conversation, in case the DDE server disconnects behind
	//		our backs.
	HCONV hSaveConv = pConv->m_hConv;

    //  Construct the flags for the call, and our calling format.
    DWORD dwFlags = 0;
    const char *pFormat = "DW,QCS";

	//	Create our argument list.
	DWORD dwWindowID = pItem->GetWindowID();
	HSZ hszItem = MakeItemArgs(pFormat, &dwWindowID, &csStatusMsg);
		
	//	Do the transaction, expect nothing.
	DdeClientTransaction(NULL, 0L, hSaveConv, hszItem, CF_TEXT,
		XTYP_POKE, m_Timeout, NULL);

	//	Cut the cord.
	DdeFreeStringHandle(m_dwidInst, hszItem);
	DdeDisconnect(hSaveConv);

	//	Make sure we still have a conversation, if so, delete the object.
	pConv = GetConvObj(hSaveConv);
	if(pConv)	{
		delete pConv;
	}
}

//Returns : 
//		0  - If incorrect WindowID 
//	   -1  - If Can't go back
//	   ID  - Id of window on which the operation was performed
HDDEDATA CDDEWrapper::NavigateBack(HSZ& hszItem)
{
	DWORD ReturnID = 0x0;

	//	Retrieve our arguments.
	DWORD WindowID;
	ScanArgs(hszItem, "DW", &WindowID);

   if (WindowID == 0xFFFFFFFF)
       WindowID = fetchLastActiveWindow();

   if(WindowID != 0x0)	
	{
		//	See if it's a valid Window ID
		CFrameGlue *pFrame = CFrameGlue::FindFrameByID(WindowID, MWContextBrowser);
		if(pFrame != NULL)	
		{
			MWContext *pContext = (pFrame->GetMainContext() != NULL) ? pFrame->GetMainContext()->GetContext() : NULL;
			if(pContext != NULL)	
			{
                if(ABSTRACTCX(pContext)->CanAllBack())
                {
				  ABSTRACTCX(pContext)->AllBack();
				  ReturnID = WindowID;
                }
			}
		}
	}

	HDDEDATA hData = MakeArgs("DW", &ReturnID);
	return(hData);
}

//Returns : 
//		0  - If incorrect WindowID 
//	   -1  - If Can't go forward
//	   ID  - Id of window on which the operation was performed
HDDEDATA CDDEWrapper::NavigateForward(HSZ& hszItem)
{
	DWORD ReturnID = 0x0;

	//	Retrieve our arguments.
	DWORD WindowID;
	ScanArgs(hszItem, "DW", &WindowID);

   if (WindowID == 0xFFFFFFFF)
       WindowID = fetchLastActiveWindow();

   if(WindowID != 0x0)	
	{
		//	See if it's a valid Window ID
		CFrameGlue *pFrame = CFrameGlue::FindFrameByID(WindowID, MWContextBrowser);
		if(pFrame != NULL)	
		{
			MWContext *pContext = (pFrame->GetMainContext() != NULL) ? pFrame->GetMainContext()->GetContext() : NULL;
			if(pContext != NULL)	
			{
				if(ABSTRACTCX(pContext)->CanAllForward())
				{
					ABSTRACTCX(pContext)->AllForward();
					//Success, set the return value.
					ReturnID = WindowID;
				}
				//else
			    // ReturnID = 0xFFFFFFFF;
			}
		}
	}

	HDDEDATA hData = MakeArgs("DW", &ReturnID);
	return(hData);
}

//Returns : 
//		0  - If incorrect WindowID 
//	   -1  - If Can't Stop
//	   ID  - Id of window on which the operation was performed
HDDEDATA CDDEWrapper::Stop(HSZ& hszItem)
{
	DWORD ReturnID = 0L;

	//	Retrieve our arguments.
	DWORD WindowID;
	ScanArgs(hszItem, "DW", &WindowID);

	if (WindowID == 0xFFFFFFFF)
       WindowID = fetchLastActiveWindow();

   if(WindowID != 0x0)	
	{
		//	See if it's a valid Window ID
		CFrameGlue *pFrame = CFrameGlue::FindFrameByID(WindowID, MWContextBrowser);
		if(pFrame != NULL)	
		{
			MWContext *pContext = (pFrame->GetMainContext() != NULL) ? pFrame->GetMainContext()->GetContext() : NULL;
			if(pContext != NULL)	
			{
				if(ABSTRACTCX(pContext)->CanAllInterrupt())
				{
					ABSTRACTCX(pContext)->AllInterrupt();
					//Success, set the return value.
					ReturnID = WindowID;
				}
				else
					ReturnID = 0xFFFFFFFF;
			}
		}
	}

	HDDEDATA hData = MakeArgs("DW", &ReturnID);
	return(hData);
}

//Returns : 
//		0  - If incorrect WindowID 
//	   -1  - If Can't Reload
//	   ID  - Id of window on which the operation was performed
HDDEDATA CDDEWrapper::Reload(HSZ& hszItem)
{
	DWORD ReturnID = 0L;

	//	Retrieve our arguments.
	DWORD WindowID;
	ScanArgs(hszItem, "DW", &WindowID);

	if (WindowID == 0xFFFFFFFF)
       WindowID = fetchLastActiveWindow();

   if(WindowID != 0x0)	
	{
		//	See if it's a valid Window ID
		CFrameGlue *pFrame = CFrameGlue::FindFrameByID(WindowID, MWContextBrowser);
		if(pFrame != NULL)	
		{
			MWContext *pContext = (pFrame->GetMainContext() != NULL) ? pFrame->GetMainContext()->GetContext() : NULL;
			if(pContext != NULL)	
			{
				if(ABSTRACTCX(pContext)->CanAllReload())
				{
					ABSTRACTCX(pContext)->AllReload();
					//Success, set the return value.
					ReturnID = WindowID;
				}
				else
					ReturnID = 0xFFFFFFFF;
			}
		}
	}

	HDDEDATA hData = MakeArgs("DW", &ReturnID);
	return(hData);
}

//Returns : 
//		0  - If incorrect WindowID 
//	   -1  - If Can't Reload
//	   ID  - Id of window on which the operation was performed
HDDEDATA CDDEWrapper::UserAgent(HSZ& hszItem)
{
    CString csVersion = theApp.ResolveAppVersion();
    return MakeArgs("QCS", &csVersion);
}

HDDEDATA CDDEWrapper::ClearCache(HSZ& hszItem)
{
    TwoByteBool returnVal = FALSE;

    int32 nSize;

    // This would fail if the pref were locked, but it's what is done
    // in the prefs dialog
    if (!PREF_PrefIsLocked("browser.cache.disk_cache_size"))
    {
      PREF_GetIntPref("browser.cache.disk_cache_size", &nSize);
      PREF_SetIntPref("browser.cache.disk_cache_size", 0);
	  PREF_SetIntPref("browser.cache.disk_cache_size", nSize);
      returnVal = TRUE;
    }

    return MakeArgs("BL", &returnVal);
}

HDDEDATA CDDEWrapper::InCache(HSZ& hszItem)
{
    TwoByteBool retVal = FALSE;
    CString url;
    ScanArgs(hszItem, "QCS", &url);

    URL_Struct* url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD); 
	int i;
    if (url_s) 
    {
        i = NET_FindURLInCache(url_s, NULL);
    }
    
    retVal = (i != 0);

    NET_FreeURLStruct(url_s);

    return MakeArgs("BL", &retVal);
}

HDDEDATA CDDEWrapper::CacheFilename(HSZ& hszItem)
{
    CString retVal = "";
    CString url;
    ScanArgs(hszItem, "QCS", &url);

    URL_Struct* url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD); 
	if (url_s) 
	{ 
		NET_FindURLInCache(url_s, NULL); 
	}
	if (url_s && url_s->cache_file && *url_s->cache_file)
	{
		retVal = WH_FileName(url_s->cache_file, xpCache);
    }
    
    
    NET_FreeURLStruct(url_s);

    return MakeArgs("QCS", &retVal);
}

HDDEDATA CDDEWrapper::CacheRemoveURL(HSZ& hszItem)
{
    TwoByteBool retVal = FALSE;
    CString url;
    ScanArgs(hszItem, "QCS", &url);

    URL_Struct* url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD); 
	if (url_s) 
    {
        NET_RemoveURLFromCache(url_s);
        retVal = TRUE;
    }

    NET_FreeURLStruct(url_s);


    return MakeArgs("BL", &retVal);
}

HDDEDATA CDDEWrapper::CacheAddURL(HSZ& hszItem)
{
    TwoByteBool retVal = FALSE;
    CString url;

    ScanArgs(hszItem, "QCS", &url);
    CAbstractCX* theContext = CAbstractCX::FindContextByID(fetchLastActiveWindow());
    MWContext* mwContext = theContext->GetContext();
    URL_Struct* url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD); 
	if (url_s) 
    {
        // Fetch and cache only
        ABSTRACTCX(mwContext)->GetUrl(url_s, FO_CACHE_ONLY);
        retVal = TRUE;
    }

    return MakeArgs("BL", &retVal);
}

HDDEDATA CDDEWrapper::ClearHistory(HSZ& hszItem)
{
    TwoByteBool returnVal = FALSE;

    int32 nSize;

    // This would fail if the pref were locked, but it's what is done
    // in the prefs dialog
    if (!PREF_PrefIsLocked("browser.link_expiration"))
    {
      PREF_GetIntPref("browser.link_expiration", &nSize);
	  PREF_SetIntPref("browser.link_expiration", 0);
	  PREF_SetIntPref("browser.link_expiration", nSize);
      returnVal = TRUE;
    }
    return MakeArgs("BL", &returnVal);
}

HDDEDATA CDDEWrapper::HistoryAddURL(HSZ& hszItem)
{
   TwoByteBool retVal = FALSE;
   CString url;

   ScanArgs(hszItem, "QCS", &url);
   URL_Struct* url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD); 
   CAbstractCX *theContext = CAbstractCX::FindContextByID(fetchLastActiveWindow());
   //MWContext *mwContext = theContext->GetContext();
   
    if (url_s) 
    {
        theContext->GetUrl(url_s, FO_CACHE_ONLY);
        GH_UpdateGlobalHistory(url_s);
        GH_UpdateURLTitle(url_s, (char*)(const char*)url, FALSE);
        retVal = TRUE;
    }
    
    return MakeArgs("BL", &retVal);
}

HDDEDATA CDDEWrapper::HistoryRemoveURL(HSZ& hszItem)
{
   TwoByteBool retVal = FALSE;
   CString url;

   ScanArgs(hszItem, "QCS", &url);
   
   GHHANDLE theHistory = GH_GetContext(eGH_NameSort, NULL, NULL, NULL, NULL);
   int recNo = GH_GetRecordNum(theHistory, (char*)((const char*)url));
   if (recNo != -1)
   {
       GH_DeleteRecord(theHistory, recNo, FALSE);
       GH_ReleaseContext(theHistory, FALSE);
       retVal = TRUE;
   }

    return MakeArgs("BL", &retVal);
}

HDDEDATA CDDEWrapper::HistoryNumEntries(HSZ& hszItem)
{

   DWORD retVal = 0;
   GHHANDLE theHistory = GH_GetContext(eGH_NameSort, NULL, NULL, NULL, NULL);
   retVal = GH_GetNumRecords(theHistory);
   return MakeArgs("DW", &retVal);
}

HDDEDATA CDDEWrapper::HistoryGetEntry(HSZ& hszItem)
{
   CString title;
   CString url;
   DWORD lastAccessed = 0;
   DWORD firstAccessed = 0;
   DWORD visitCount = 0;

   DWORD recNo = 0;
   DWORD retVal = 0;

   ScanArgs(hszItem, "DW", &recNo);
   
   GHHANDLE theHistory = GH_GetContext(eGH_NameSort, NULL, NULL, NULL, NULL);
   gh_HistEntry* historyEntry = GH_GetRecord(theHistory, recNo);
   if (historyEntry != NULL)
   {
      title = historyEntry->pszName;
      url = historyEntry->address;
      lastAccessed = historyEntry->last_accessed;
      firstAccessed = historyEntry->first_accessed;
      visitCount = historyEntry->iCount;
      return MakeArgs("QCS,QCS,DW,DW,DW", &url, &title, &firstAccessed, &lastAccessed, &visitCount);

   }
   return MakeArgs("DW", &retVal);
}

HDDEDATA CDDEWrapper::InHistory(HSZ& hszItem)
{
   TwoByteBool retVal = FALSE;
   CString url;

   ScanArgs(hszItem, "QCS", &url);
   
    int i = GH_CheckGlobalHistory((char*)(const char*)url);
    if (i != -1)
           retVal = TRUE;
    
    return MakeArgs("BL", &retVal);
}

HDDEDATA CDDEWrapper::GetWindowID(HSZ& hszItem)
{

    CString name;
    DWORD WindowID;

    ScanArgs(hszItem, "QCS,DW", &name, &WindowID);
    if (WindowID == 0xFFFFFFFF)
       WindowID = fetchLastActiveWindow();

    DWORD retVal = 0x0;

    if(WindowID != 0x0)	
	{
		//	See if it's a valid Window ID
		CFrameGlue *pFrame = CFrameGlue::FindFrameByID(WindowID, MWContextBrowser);
		if(pFrame != NULL)	
		{
			MWContext *pContext = (pFrame->GetMainContext() != NULL) ? pFrame->GetMainContext()->GetContext() : NULL;
			if(pContext != NULL)	
            {
                // We have a reference point.... find the named window
                MWContext* returnContext = XP_FindNamedContextInList(pContext, (char*)(const char*)name);
                if (returnContext != NULL)
                    retVal = WINCX(pContext)->GetContextID();
            }
        }
    }

    return MakeArgs("DW", &retVal);
}

HDDEDATA CDDEWrapper::SupportsMimeType(HSZ& hszItem)
{


  return 0;  
}
HDDEDATA CDDEWrapper::ExecuteJavaScript(HSZ& hszItem)
{


  return 0;
}

HDDEDATA CDDEWrapper::PrintWindow(HSZ& hszItem)
{
 	DWORD ReturnID = 0L;

	//	Retrieve our arguments.
	DWORD WindowID;
	ScanArgs(hszItem, "DW", &WindowID);

	if (WindowID == 0xFFFFFFFF)
       WindowID = fetchLastActiveWindow();

    if(WindowID != 0x0)	
	{
		//	See if it's a valid Window ID
        CWinCX* pContext = GetContext(WindowID);
        if(pContext != NULL)	
		{
				if(pContext->CanPrint())
				{
					pContext->Print();
					//Success, set the return value.
					ReturnID = WindowID;
				}
				else
					ReturnID = 0xFFFFFFFF;
		}
	}

	HDDEDATA hData = MakeArgs("DW", &ReturnID);
	return(hData);
}

HDDEDATA CDDEWrapper::PrintURL(HSZ& hszItem)
{
 TwoByteBool retVal = TRUE;
 CString url;
 CString printer;
 CString driver;
 CString port;

 ScanArgs(hszItem, "QCS,QCS,QCS,QCS", &url, &printer, &driver, &port);

 char* pr = NULL;
 char* dr = NULL;
 char* pt = NULL;
 if (!printer.IsEmpty())
     pr = (char*)(const char*)printer;
 if (!driver.IsEmpty())
     dr = (char*)(const char*)driver;
 if (!port.IsEmpty())
     pt = (char*)(const char*)port; 
 
 CPrintCX::AutomatedPrint((char*)(const char*)url, pr, dr, pt);

 
 return MakeArgs("BL", &retVal);
}

CGenericView *CDDEWrapper::GetView(DWORD dwContextID)
{
    CGenericView *pRetval = NULL;

    CWinCX *pCX = GetContext(dwContextID);
    if(pCX) {
        pRetval = pCX->GetView();
    }

    return(pRetval);
}

CFrameGlue *CDDEWrapper::GetFrame(DWORD dwContextID)
{
    CFrameGlue *pRetval = NULL;

    CWinCX *pCX = GetContext(dwContextID);
    if(pCX) {
        pRetval = pCX->GetFrame();
    }

    return(pRetval);
}

CWinCX *CDDEWrapper::GetContext(DWORD dwContextID)
{
    CWinCX *pRetval = NULL;

    CAbstractCX *pAbstract = CAbstractCX::FindContextByID(dwContextID);
    if(pAbstract && pAbstract->IsDestroyed() == FALSE)   {
        //  Must be a window, and a MWContextBrowser.
        if(pAbstract->IsFrameContext() && pAbstract->GetContext()->type == MWContextBrowser)  {
            pRetval = VOID2CX(pAbstract, CWinCX);
        }
    }

    return(pRetval);
}

DWORD CDDEWrapper::fetchLastActiveWindow()
{
    return FEU_GetLastActiveFrameID(MWContextBrowser);
}

