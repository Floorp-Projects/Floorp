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

//by Abe Jarrett
//Purpose:  Hard Wired DDE conversation for command line processing.
//			Used to launch specific components the app.
//NOTE:		Most if not all of the code and frame work herein was taken
//			from dde.cpp.  Thanks to GAB for his assistance!



#ifndef __DDECMD_H
#define __DDECMD_H

//	Required Includes
//
#ifndef __DDE_H

#ifndef WIN32
#include "ddeml2.h"
#else
#include <ddeml.h>
#endif // WIN32

#endif //just in case they take out the other dde includes

//	Constants
//
//	Change this on each revision.  Hiword is major, Loword is minor.
const DWORD gdwDDEVersion = 0x00010001UL;

//  Careful, 32bit bools will byte you.


//	Structures
//
class CDDECMDWrapper	{
	//	Enumeration values which index into our static HSZ array.
	//	Faster than list lookups.
	

public:

	CDDECMDWrapper(HSZ hszService, HSZ hszTopic, HCONV hConv);

	~CDDECMDWrapper();

	enum
	{
		m_MinHSZ = 0,
		m_ServiceName = 0,
		m_TopicStart = 1,	//	Where topics begin, services end
		m_OpenURL = 1,
		m_ProcessCmdLine,
		m_MaxHSZ,	//	Where all hsz strings end, and where topics end
		m_Timeout = 30000,	//	Timeout value, in milliseconds, that the we will wait as a client.
	};
	
	static DWORD m_dwidInst;	//	Our DDEML instance.  Only 1 ever.
	static BOOL m_bDDEActive;	//	Wether or not DDEML was initialized.
	static HSZ m_hsz[m_MaxHSZ];	//	Array of HSZs to be used by all
	static CMapPtrToPtr m_ConvList;	//	Map of current conversations
	static DWORD m_dwafCmd;	//	array of command and filter flags
	static FARPROC m_pfnCallBack;	//	Call back function after MakeProcIntance
	
	
	//	Conversation instance specific members
	HSZ m_hszService;	//	The service this object represent
	int m_iService;	//	The enumerated service number; useful.
	HSZ m_hszTopic;	//	The topic this object represents.
	int m_iTopic;	//	The enumerated topic number; very useful.
	HCONV m_hConv;	//	The conversation this object represents.
	CString m_csProgressApp;	//	The service that we will update.

	//General members for informational lookup
	static CDDECMDWrapper *GetConvObj(HCONV hConv);
	static int EnumTopic(HSZ& hsz);

	//	Client connection establisher.
	static CDDECMDWrapper *ClientConnect(const char *cpService,
		HSZ& hszTopic);
};

//	Global variables
//

//	Macros
//

//	Function declarations
//
void DDEInitCmdLineConversation();
void DDEShutdown();
HDDEDATA 
CALLBACK 
#ifndef XP_WIN32
_export 
#endif
CmdLineDdeCallBack(UINT type, UINT fmt,
	HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1,
	DWORD dwData2);

#endif //__DDECMD_H
