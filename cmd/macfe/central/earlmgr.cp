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

// earlmgr.cp, Mac front end

#include "earlmgr.h"
	// macfe
#include "ufilemgr.h"
#include "ulaunch.h"
#include "uprefd.h"
#include "uerrmgr.h"
#include "BufferStream.h"
#include "uapp.h"
#include "CNSContext.h"
	// Network
#ifndef NSPR20
#include "CNetwork.h"
#endif
	// Netscape
#include "secrng.h"
	// utilities
#include "PascalString.h"

#ifdef JAVA
extern "C" {
#include "nsn.h"
}
#endif

#include "resgui.h"

#ifdef NSPR20
PR_BEGIN_EXTERN_C
PR_EXTERN(PRThread*) PR_GetPrimaryThread(void);
PR_END_EXTERN_C
#endif

/*-----------------------------------------------------------------------------
	Earl Manager
-----------------------------------------------------------------------------*/

EarlManager TheEarlManager;

int EarlManagerNetTicklerCallback(void);

//
// Construction
//

EarlManager::EarlManager ()
{
	fInterruptContext = NULL;
#ifdef JAVA
	nsn_InstallTickleHookProc(&EarlManagerNetTicklerCallback);
#endif
}

//
// ее URL loading interface
//

void EarlManager::DoCancelLoad (MWContext *context)
{
	// Cancels all loads for a given window
	NET_InterruptWindow (context);
}

int EarlManager::StartLoadURL (URL_Struct * request, MWContext *context,
					FO_Present_Types output_format)
{
	if (request->referer && (strlen(request->referer) == 0))
	{
		XP_FREE(request->referer);
		request->referer = NULL;
	}
	int err = NET_GetURL (request, output_format, context, DispatchFinishLoadURL);	
	return err;
}

void EarlManager::SpendTime( const EventRecord& /*inMacEvent*/)
{
	//	Make sure that we only call NET_ProcessNet at an
	//	appropriate time, and make sure that our priority is
	//	low so that other java threads an run.
	
#ifndef NSPR20
	// If we don't have a gNetDriver then we better not call it
	if (!gNetDriver)
		return;
	
	PR_SetThreadPriority(PR_CurrentThread(), 0);
	gNetDriver->SpendTime();
#else
	PR_SetThreadPriority(PR_CurrentThread(), PR_PRIORITY_LOW);
#endif

	unsigned char dst;
	RNG_GenerateGlobalRandomBytes(&dst, 1);
#ifdef NSPR20_DISABLED
	CSelectObject socketToCallWith;
	if (gNetDriver->CanCallNetlib(socketToCallWith))
		NET_ProcessNet(MacToStdSocketID(socketToCallWith.fID), SocketSelectToFD_Type(socketToCallWith.fType));
#else
	NET_PollSockets();
#endif
	if ( fInterruptContext )
	{
		NET_InterruptWindow( fInterruptContext );
		fInterruptContext = NULL;
	}
}

void EarlManager::InterruptThis(MWContext * context)
{
	fInterruptContext = context;
}

void EarlManager::FinishLoadURL(URL_Struct * request, int status, MWContext *context)
{
	CNSContext* nsContext = ExtractNSContext(context);
	if (nsContext)
		nsContext->CompleteLoad(request, status);
	if (status != MK_CHANGING_CONTEXT)
	    NET_FreeURLStruct (request);
}

void EarlManager::DispatchFinishLoadURL (URL_Struct *request, int status, MWContext *context)
{
	// let the front end know the NetGetUrl is complete. This should go
	// into NetLib at some point! LAM	
	
	TheEarlManager.FinishLoadURL (request, status, context);
}




/*------------------------------------------------------------------------------
 FE Misc functions
 - Launching of telnet/tn3270 sessions
 ------------------------------------------------------------------------------*/
// ее Prototypes

LFileBufferStream * CreateTelnetFile(char * hostname, char * port, char * username);
LFileBufferStream * CreateTn3270File(char * hostname, char * port, char * username);

// ее Implementation

LFileBufferStream * CreateTelnetFile(char * hostname, char * port, char * username)
{
	FSSpec fileSpec;

	// Figure out the mime extensions
	CMimeMapper * mime = CPrefs::sMimeTypes.FindMimeType(CMimeList::Telnet);
	if (mime == NULL)
		return NULL;

	// Create the file
	LFileBufferStream * file = NULL;
	CFileMgr::UniqueFileSpec( CPrefs::GetTempFilePrototype(),
		CStr31(::GetCString(NETSCAPE_TELNET)), fileSpec);

	Try_	{	// Not dealing with duplicates yet
		file = new LFileBufferStream(fileSpec);
		file->CreateNewDataFile(mime->GetAppSig(), mime->GetDocType(), 0);
		file->OpenDataFork(fsRdWrPerm);
	}
	Catch_(inErr)
	{
		return NULL;
	} EndCatch_

// Write the arguments out

	WriteCString (file, ::GetCString(NETSCAPE_TELNET_NAME_ARG));	// name="user@hostname"
	if (username != NULL)
	{
		WriteCString (file, username);
		WriteChar (file, '@');
	// ErrorManager::PlainAlert(Login as????
	}
	WriteCString (file, hostname);
	WriteCString (file, "\"\r");
	WriteCString (file, ::GetCString(NETSCAPE_TELNET_HOST_ARG));	// host = "hostname"
	WriteCString (file, hostname);
	WriteCString (file, "\"\r");
	WriteCString (file, ::GetCString(NETSCAPE_TELNET_PORT_ARG));	// port = "port"
	if (port != NULL)
		WriteCString (file, port);
	else
		WriteCString (file, "23\r");
	file->CloseDataFork();
	return file;
}

LFileBufferStream * CreateTn3270File(char * hostname, char * /*port*/, char * /*username*/)
{
	FSSpec fileSpec;

// Figure out the mime extensions
	CMimeMapper * mime = CPrefs::sMimeTypes.FindMimeType(CMimeList::Tn3270);
	if (mime == NULL)
		return NULL;

// Create the file
	LFileBufferStream * file = NULL;
	CFileMgr::UniqueFileSpec( CPrefs::GetTempFilePrototype(),
	 	CStr31(::GetCString(NETSCAPE_TN3270)), fileSpec);

	Try_	{	// Not dealing with duplicates yet
		file = new LFileBufferStream (fileSpec);
		file->CreateNewDataFile(mime->GetAppSig(), mime->GetDocType(), 0);
		file->OpenDataFork(fsRdWrPerm);
	}
	Catch_(inErr)
	{
		return NULL;
	} EndCatch_

// Write the arguments out
// I really do not know what the format of the file is, so I am just guessing here
	WriteCString (file, "0 8 80 20 40 10 80 0 ");	// part 1
	WriteCString (file, hostname);
	// part 2 of indecipherable file format
	WriteCString (file, " 2 1 15 1 1 1 23 0 1 ffff ffff ffff fc00 f37d 52f ffff 7a5d 9f8e 0 c42f eaff 1f21 b793 1431 dd6b 8c2 6a2 4eba 6aea dd6b 0 0 0 2a00 2a00 2a00 24 12 0 1 0 0 4eba 6aea dd6b 1 1 0 default 1 English_(U.S.) 12 80 24 80 System_Alert_Sound 2 19f 142 1 74747874 0");						// part2
	file->CloseDataFork();
	return file;
}

void FE_ConnectToRemoteHost(MWContext * ctxt, int url_type, char *
hostname, char * port, char * username)
{
	LFileBufferStream * telnetFile = NULL;
	switch (url_type)	{
		case FE_RLOGIN_URL_TYPE:
		case FE_TELNET_URL_TYPE:
			FE_Progress(ctxt, (char *)GetCString( LAUNCH_TELNET_RESID )); 
			telnetFile = CreateTelnetFile(hostname, port, username);
			break;
		case FE_TN3270_URL_TYPE:
			FE_Progress(ctxt, (char *)GetCString( LAUNCH_TN3720_RESID ));
			telnetFile = CreateTn3270File(hostname, port, username);
	}
	if (telnetFile != NULL)
	{
		LaunchFile(telnetFile);
		CFileMgr::sFileManager.RegisterFile(telnetFile);	// Register file for deletion
	}
	else
		ErrorManager::PlainAlert(TELNET_ERR_RESID);
}

/*------------------=----------------------------------------------------------
	Utilities
--------------------=--------------------------------------------------------*/

CStr255 gSaveAsPrompt;

OSErr GUI_AskForFileSpec (StandardFileReply & reply);
OSErr GUI_AskForFileSpec (StandardFileReply & reply)
{
	ErrorManager::PrepareToInteract();
	
	UDesktop::Deactivate();
	if (gSaveAsPrompt.Length() == 0)
		gSaveAsPrompt = (const char*)GetCString( SAVE_AS_RESID );
	StandardPutFile (gSaveAsPrompt, reply.sfFile.name, &reply);
	UDesktop::Activate();
	
	if (!reply.sfGood)
		return userCanceledErr;
	return noErr;
}

int EarlManagerNetTicklerCallback(void)
{
	//	We need to tickle the network and the UI if we
	//	are stuck in java networking code.
	
	if (PR_CurrentThread() == PR_GetPrimaryThread()) {
		(CFrontApp::GetApplication())->ProcessNextEvent();
		return false;
	}
	
	else {
		return true;	
	}
}


#ifdef NSPR20_DISABLED
int SocketSelectToFD_Type(SocketSelect s)
{
	switch (s)	{
	case eReadSocket:
	case eExceptionSocket:
		return NET_SOCKET_FD;
	case eLocalFileSocket:
		return NET_LOCAL_FILE_FD;
	case eEverytimeSocket:
		return NET_EVERYTIME_TYPE;
	}
	return 0;	// Never reached
}
#endif

// FE Select routines
void FE_SetReadSelect (MWContext * /* context */, int sd)
{
#ifndef NSPR20
	gNetDriver->SetReadSelect(sd);
#endif
}

void FE_ClearReadSelect (MWContext */* context */, int sd)
{
#ifndef NSPR20
	gNetDriver->ClearReadSelect(sd);
#endif
}

void FE_SetConnectSelect (MWContext */* context */, int sd)
{
#ifndef NSPR20
	gNetDriver->SetConnectSelect(sd);
#endif
}

void FE_ClearConnectSelect(MWContext */* context */, int sd)
{
#ifndef NSPR20
	gNetDriver->ClearConnectSelect(sd);
#endif
}


void FE_SetFileReadSelect (MWContext */* context */, int fd)
{
#ifndef NSPR20
	gNetDriver->SetFileReadSelect(fd);
#endif
}

		
void FE_ClearFileReadSelect (MWContext */* context */, int fd)
{
#ifndef NSPR20
	gNetDriver->ClearFileReadSelect(fd);
#endif
}

int FE_StartAsyncDNSLookup (MWContext */* context */, char * host_port, void ** hoststruct_ptr_ptr, int sock)
{
#ifndef NSPR20
	*hoststruct_ptr_ptr = NULL;
	int err = gNetDriver->StartAsyncDNSLookup(host_port, (struct hostent **)hoststruct_ptr_ptr, sock);
	if (err == EAGAIN)	// Because macsock knows nothing of netlib defines
		return MK_WAITING_FOR_LOOKUP;
	else
		return err;
#endif
	return MK_WAITING_FOR_LOOKUP;
}


#ifdef PROFILE
#pragma profile off
#endif

