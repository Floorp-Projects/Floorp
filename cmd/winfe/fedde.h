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

#ifndef __DDE_H
//	Avoid include redundancy.
//
//
#define __DDE_H

//	Purpose:	Header file for DDE remote control

#ifndef WIN32
#include "ddeml2.h"
#else
#include <ddeml.h>
#endif // WIN32

//	Constants
//
//	Change this on each revision.  Hiword is major, Loword is minor.
const DWORD dwDDEVersion = 0x00030000UL;

//  Careful, 32bit bools will byte you.
typedef short TwoByteBool;

//	Structures
//
struct CDDEWrapper	{
	//	Enumeration values which index into our static HSZ array.
	//	Faster than list lookups.
	enum	{
		m_MinHSZ = 0,
		m_ServiceName = 0,
		m_TopicStart = 1,	//	Where topics begin, services end
		m_OpenURL = 1,
		m_ShowFile,
		m_Activate,
		m_ListWindows,
		m_GetWindowInfo,
		m_ParseAnchor,
		m_Exit,
		m_RegisterProtocol,
		m_UnRegisterProtocol,
		m_RegisterViewer,
		m_QueryViewer,
		m_ViewDocFile,
		m_UnRegisterViewer,
		m_RegisterURLEcho,
		m_URLEcho,
		m_UnRegisterURLEcho,
		m_RegisterWindowChange,
		m_WindowChange,
		m_UnRegisterWindowChange,
		m_BeginProgress,
		m_SetProgressRange,
		m_MakingProgress,
		m_EndProgress,
		m_Alert,
		m_ListOptions,
		m_GetOption,
		m_SetOption,
		m_Version,
		m_CancelProgress,
        m_QueryURLFile,
		m_InternetShortcut,
		m_MaxHSZ,	//	Where all hsz strings end, and where topics end
		m_Timeout = 2000,	//	Timeout value, in milliseconds, that the we will wait as a client.
		m_AlertError = 0,	//	Possible alert box types
		m_AlertWarning = 1,
		m_AlertQuestion = 2,
		m_AlertStatus = 3,
		m_ButtonOk = 0,		//	Possible buttons to show in alert box
		m_ButtonOkCancel = 1,
		m_ButtonYesNo = 2,
		m_ButtonYesNoCancel = 3,
		m_PushedError = 0,
		m_PushedOk = 1,		//	The button pushed in an alert box.
		m_PushedCancel = 2,
		m_PushedNo = 3,
		m_PushedYes = 4
	};
	
	static DWORD m_dwidInst;	//	Our DDEML instance.  Only 1 ever.
	static BOOL m_bDDEActive;	//	Whether or not DDEML was initialized.
	static HSZ m_hsz[m_MaxHSZ];	//	Array of HSZs to be used by all
	static CMapPtrToPtr m_ConvList;	//	Map of current conversations
	static DWORD m_dwafCmd;	//	array of command and filter flags
	static FARPROC m_pfnCallBack;	//	Call back function after MakeProcIntance
	static UINT m_cfFmts[];	//	array of acceptable clipboard formats
	
	CDDEWrapper(HSZ hszService, HSZ hszTopic, HCONV hConv);
	~CDDEWrapper();
	
	//	Conversation instance specific members
	HSZ m_hszService;	//	The service this object represents.
	int m_iService;	//	The enumerated service number; useful.
	HSZ m_hszTopic;	//	The topic this object represents.
	int m_iTopic;	//	The enumerated topic number; very useful.
	HCONV m_hConv;	//	The conversation this object represents.
	CString m_csProgressApp;	//	The service that we will update.

	//	General members for informational lookup
	static CDDEWrapper *GetConvObj(HCONV hConv);
	static int EnumTopic(HSZ& hsz);
	static void ScanArgs(HSZ& hszArgs, const char *pFormat, ...);
	static void ScanDataArgs(HDDEDATA& hArgs, const char *pFormat, ...);
	static HDDEDATA MakeArgs(const char *pFormat, ...);
	static HSZ MakeItemArgs(const char *pFormat, ...);
	static char *SkipToNextFormat(char *pCurrent);
	static char *ExtractArg(HSZ& hszArgs, int iArgNum);
	
	//	DDE Callback handlers for a conversation
	HDDEDATA PokeHandler(HSZ& hszTopic, HSZ& hszItem, HDDEDATA& hData);
	HDDEDATA RequestHandler(HSZ& hszTopic, HSZ& hszItem);
	
	//	Server Requests
	HDDEDATA OpenURL(HSZ& hszItem);
	HDDEDATA ShowFile(HSZ& hszItem);
	HDDEDATA Activate(HSZ& hszItem);
	HDDEDATA ListWindows(HSZ& hszItem);
	HDDEDATA GetWindowInfo(HSZ& hszItem);
	HDDEDATA ParseAnchor(HSZ& hszItem);
	HDDEDATA RegisterProtocol(HSZ& hszItem);
	HDDEDATA UnRegisterProtocol(HSZ& hszItem);
	HDDEDATA RegisterViewer(HSZ& hszItem);
	HDDEDATA UnRegisterViewer(HSZ& hszItem);
	HDDEDATA RegisterWindowChange(HSZ& hszItem);
	HDDEDATA UnRegisterWindowChange(HSZ& hszItem);
	HDDEDATA BeginProgress(HSZ& hszItem)	{ return(NULL); }
	HDDEDATA MakingProgress(HSZ& hszItem)	{ return(NULL); }
	HDDEDATA EndProgress(HSZ& hszItem)	{ return(NULL); }
	HDDEDATA ListOptions(HSZ& hszItem);
	HDDEDATA GetOption(HSZ& hszItem);
	HDDEDATA Version(HSZ& hszItem);
    HDDEDATA QueryURLFile(HSZ& hszItem);
	
	//	Server Pokes
	HDDEDATA Exit(HSZ& hszItem, HDDEDATA& hData);
	HDDEDATA SetProgressRange(HSZ& hszItem, HDDEDATA& hData)	{ return(NULL); }
	HDDEDATA SetOption(HSZ& hszItem, HDDEDATA& hData);
	HDDEDATA RegisterURLEcho(HSZ& hszItem, HDDEDATA& hData);
	HDDEDATA UnRegisterURLEcho(HSZ& hszItem, HDDEDATA& hData);
	HDDEDATA WindowChange(HSZ& hszItem, HDDEDATA& hData);
	HDDEDATA CancelProgress(HSZ& hszItem, HDDEDATA& hData);
	
	//	Client connection establisher.
	static CDDEWrapper *ClientConnect(const char *cpService,
		HSZ& hszTopic);
	
	//	Client Progress
	static DWORD BeginProgress(CNetscapeDoc *pDoc, const char *pService,
		DWORD dwWindowID, const char *pMessage);
	static void SetProgressRange(CNetscapeDoc *pDoc, const char *pService,
		DWORD dwTransactionID, DWORD dwMaxRange);
	static TwoByteBool MakingProgress(CNetscapeDoc *pDoc, const char *pService,
		DWORD dwTransactionID, const char *pMessage, DWORD dwCurrent);
	static void EndProgress(CNetscapeDoc *pDoc, const char *pService,
		DWORD dwTransactionID);
	static DWORD AlertProgress(CNetscapeDoc *pDoc, const char *pService,
		const char *pMessage);
		
	//	Client Viewer Commands
	static void QueryViewer(CDDEDownloadData *pDData);
	static void ViewDocFile(CDDEDownloadData *pDData);
	static void OpenDocument(CDDEDownloadData *pDData);
	
	//	Client Protocol Commands.
	static TwoByteBool OpenURL(CString csProtocol, CString csServiceName, URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut);
	
	//	Client Echo commands.
	static void URLEcho(CDDEEchoItem *pItem, CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer);
	
	//	Client Window change commands.
	static void WindowChange(CDDEWindowChangeItem *pItem, int iChange, TwoByteBool bExiting = FALSE, DWORD dwX = 0, DWORD dwY = 0,
	    DWORD dwCX = 0, DWORD cwCY = 0);
};

//	Function declarations
//
void DDEStartup();
void DDEShutdown();
HDDEDATA 
CALLBACK 
#ifndef XP_WIN32 
_export 
#endif // no _export in windows 32
NetscapeDDECallBack(UINT type, UINT fmt,
	HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1,
	DWORD dwData2);

#endif // __DDE_H
