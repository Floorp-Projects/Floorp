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

//Created Abe Jarrett:  Most of the methods in this class were taken from
//Garrett's code in dde.cpp.  This file is intended only as
//a means of detecting seconds instance criteria and handling processing
//of it and  any command line content as well.

#include "stdafx.h"

#include "ddecmd.h"
#include "ddectc.h"


//	Our DDE instance identifier.
DWORD CDDECMDWrapper::m_dwidInst;

//	Wether or not DDE was successfully initialized.
BOOL CDDECMDWrapper::m_bDDEActive;

//	Our array of hsz strings.
HSZ CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_MaxHSZ];

//	A list of all current conversations.  The PtrToPtr map was used since
//		the pointers are known to be 32 bits long, and the HCONV is a 
//		DWORD or possibly a pointer, and WordToPtr just wouldn't cut it
//		with 16 bits and all.  
//	Sounds good to me!
CMapPtrToPtr CDDECMDWrapper::m_ConvList;

//	Command filter for DDEML
DWORD CDDECMDWrapper::m_dwafCmd;

//	Call back function for use with DDEML
FARPROC CDDECMDWrapper::m_pfnCallBack;


//This kicks it all off.
void DDEInitCmdLineConversation()
{

	CDDECMDWrapper::m_pfnCallBack = NULL;

#ifdef XP_WIN16
	//	Find out if we can even use DDEML (must be in protected mode).
	//	Undoubtedly, this is the case since we must be in protected
	//		mode to use WINSOCK services, but just to be anal....
	//  GetWinFlags() has been removed in MSVC 2.0 and the analog doesn't
	//      look like it does the same thing.  chouck 29-Dec-94
	if(!(GetWinFlags() & WF_PMODE))	{
		//	Not in protected mode, can not continue with DDEML
		return ;
	}
#endif
	
	//	Set up our callback function to be registered with DDEML.
	CDDECMDWrapper::m_pfnCallBack = (FARPROC)CmdLineDdeCallBack;

			
	if(DdeInitialize(&CDDECMDWrapper::m_dwidInst,
		(PFNCALLBACK)CDDECMDWrapper::m_pfnCallBack,
		CDDECMDWrapper::m_dwafCmd, 0L))	{
		//	Unable to initialize, don't continue.
		return ; 
	}


	CDDECMDWrapper::m_bDDEActive = TRUE;
	
	//	Load in all HSZs.
	//	Unfortunately, there is really no good way to detect any errors
	//		on these string intializations, so let the blame land
	//		where it will if something fails.
	CString strCS;
	
	strCS.LoadString(IDS_DDE_CMDLINE_SERVICE_NAME);
	CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ServiceName] =
		DdeCreateStringHandle(CDDECMDWrapper::m_dwidInst,
		(char *)(const char *)strCS, CP_WINANSI);

	strCS.LoadString(IDS_DDE_PROCESS_CMDLINE);
	CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ProcessCmdLine] =
		DdeCreateStringHandle(CDDECMDWrapper::m_dwidInst,
		(char *)(const char *)strCS, CP_WINANSI);
	
}


//	Purpose:	Construct a DDECMDWrapper object
//	Arguments:	hszService	The name of the service we are handling.
//				hszTopic	The name of the topic we are handling.
//				hConv		The handle of the conversation we are
//								handling.
//	Returns:	nothing
//	Comments:	Handles all created objects internally through a map.
//	Revision History:
//
CDDECMDWrapper::CDDECMDWrapper(HSZ hszService, HSZ hszTopic, HCONV hConv)
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


//	Purpose:	Destory a CDDECMDWrapper object
//	Arguments:	void
//	Returns:	nothing
//	Comments:	Removes us from the internally handled list
//	Revision History:
//	

CDDECMDWrapper::~CDDECMDWrapper()
{
	//	Remove ourselves from the list of current conversations.
	m_ConvList.RemoveKey((void *)m_hConv);
}



//	Purpose:	Connect to a service using a specific topic
//	Arguments:	cpService	The service name of the DDE server
//				hszTopic	The topic of the conversation to the server
//	Returns:	CDDECMDWrapper *	The conversation object if established,
//									otherwise NULL.
//	Comments:	Generic connection establishment.
//	Revision History:
//		01-05-95	created GAB
CDDECMDWrapper *CDDECMDWrapper::ClientConnect(const char *cpService,
	HSZ& hszTopic)
{
	CDDECMDWrapper *pConv = NULL;

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
		//		a CDDECMDWrapper, we'll be creating it with the wrong
		//		service name of course, but client connections no
		//		longer really care about the service connection number,
		//		all they need is the conversation handle.
		pConv = new CDDECMDWrapper(m_hsz[m_ServiceName], hszTopic, hConv);
	}
		
	//	Free off our created hsz.
	DdeFreeStringHandle(m_dwidInst, hszService);
	

	return(pConv);
}

//	Purpose:	Figure out which wrapper is associated with a conversation.
//	Arguments:	hConv	The conversation to find out about.
//	Returns:	CDDECMDWrapper *	A pointer to the CDDECMDWrapper that is
//									handling the conversation, or NULL
//									if none is present.
//	Comments:	Shouldn't ever return NULL really.
//	Revision History:
//		12-30-94	created GAB
//		12-04-96	reused by AJ.  Probably not necessary, but you never know
//					know when we may have to do more than one conversation.
CDDECMDWrapper *CDDECMDWrapper::GetConvObj(HCONV hConv)
{
	//	Query our static map of conversations for the object.
	void *pWrap;
	
	if(m_ConvList.Lookup((void *)hConv, pWrap) == 0)	{
		return(NULL);
	}
	return((CDDECMDWrapper *)pWrap);
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
//		12-04-96	reused by AJ.  Probably not necessary, but you never know
//					know when we may have to handle more than one topic.
int CDDECMDWrapper::EnumTopic(HSZ& hsz)
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

HDDEDATA 
CALLBACK 
#ifndef XP_WIN32
_export 
#endif

CmdLineDdeCallBack(UINT type, UINT fmt,
	HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1,
	DWORD dwData2)
{
	
	
	//	Depending on the class of transaction, we return different data.
	if(type & XCLASS_BOOL)	{
		//	Must return (HDDEDATA)TRUE or (HDDEDATA)FALSE
		switch(type)	{

		case XTYP_CONNECT:	{
			//	We are the server.
			//	A client call the DdeConnect specifying a service and
			//		topic name which we support.
			HSZ& hszTopic = hsz1;
			HSZ& hszService = hsz2;
			
			//	Deny the connection if the service name is not the
			//		one we are taking connections for.
			if(hszService !=
				CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ServiceName])	{
				return((HDDEDATA)FALSE);
			}
			
			//	Now, the topic can be NULL, or it can be any one of our
			//		topic names to be accepted.
			if(hszTopic == NULL)	{
				return((HDDEDATA)TRUE);
			}
			
			//	Go through all our topics, see if we match.
			if(0 != CDDECMDWrapper::EnumTopic(hszTopic))	{
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
			HDDEDATA& hDataExecute = hData;
			char *pData = (char *)DdeAccessData(hDataExecute, NULL);
			char szCmd[_MAX_PATH+12];
			strcpy ( szCmd, "[cmdline(\"" );
			strcat ( szCmd, pData );
			strcat ( szCmd, "\")]" );
			BOOL bRetval = theApp.OnDDECommand(szCmd);
			
			DdeUnaccessData(hDataExecute);
			return((HDDEDATA) bRetval);
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
			CDDECMDWrapper *pObject = new CDDECMDWrapper(hszService, hszTopic,
				hconv);
			break;
		}
		case XTYP_DISCONNECT:	{
			//	We are either client/server
			//	The partner in the conversation called DdeDisconnect
			//		causing both client/server to receive this.
			
			//	Find out which conversation object we are dealing with.
			CDDECMDWrapper *pWrapper = CDDECMDWrapper::GetConvObj(hconv);
			
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


