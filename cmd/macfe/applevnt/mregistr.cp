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

// mregistr.cp
// Registry for AppleEvent notifiers
// Pretty clumsy right now, but separating this functionality out of uapp seems
// to be the right thing.
// It is just a collection of routines

// MacNetscape
#include "mregistr.h"
#include "macutil.h"
#include "CAppleEventHandler.h"
#include "resae.h"
#include "resgui.h"
#include "ufilemgr.h"
#include "uprefd.h"
#include "CNSContext.h"

#ifndef MOZ_MAIL_NEWS
#include "InternetConfig.h"
#endif

// xp
#include "client.h"

static LArray sURLEchoHandlers(sizeof(ProcessSerialNumber));
static LArray sProtocolHandlers;

/************************************************************************************
 * class CProtocolHelper
 * Holds the information about protocol helpers, and knows how to launch them
 ************************************************************************************/

class CProtocolHelper	{
public:
	char * fProtocolInfo;	// String that specifies the protocol
	OSType fApplSig;		// Application to launch. Do not use these unless in saving/restoring
	
	// ее constructors
	CProtocolHelper(char * protocolInfo, OSType applSig);
	virtual ~CProtocolHelper();
	// ее access
	Boolean	AttemptLaunch(URL_Struct *url, MWContext *context);
	Boolean EqualTo(char * protocolInfo, OSType applSig);
	Boolean operator==(CProtocolHelper * p);
	static void AddNewHelper(CProtocolHelper * helper);
	
};

CProtocolHelper::CProtocolHelper(char * protocolInfo, OSType applSig)
{
	fProtocolInfo = protocolInfo;
	fApplSig = applSig;
}

CProtocolHelper::~CProtocolHelper()
{
	if (fProtocolInfo)
		XP_FREE(fProtocolInfo);
}

Boolean CProtocolHelper::operator==(CProtocolHelper * p)
{
	if (fProtocolInfo && p->fProtocolInfo)
		return (strcmp(fProtocolInfo, p->fProtocolInfo) == 0);
	return false;
}

// This is used for the helper removal
// It returns true if we do not have the protocol info
Boolean CProtocolHelper::EqualTo(char * protocolInfo, OSType applSig)
{
	if (applSig != fApplSig)
		return false;
	if (protocolInfo && fProtocolInfo)
		if (strcmp(protocolInfo, fProtocolInfo) == 0)
			return true;
		else
			return false;
	else
		return true;
	return false;
}

// Finds the running helper application
// Tries to send a OpenURL event to the registered protocol handler
// If this does not work, sends the standard GetURL event
Boolean CProtocolHelper::AttemptLaunch(URL_Struct *url, MWContext */*context*/)
{
	if (!url->address)
		return false;
	if (strncasecomp(url->address, fProtocolInfo, strlen(fProtocolInfo)) != 0)
		return false;

	ProcessSerialNumber psn;
	FSSpec dummy;
	OSErr err = FindProcessBySignature(fApplSig,'APPL',psn,&dummy);
	if (err != noErr)
	{
		FSSpec appSpec;
		err = CFileMgr::FindApplication(fApplSig, appSpec);
		if (err != noErr)
			return false;
	
		LaunchParamBlockRec launchParams;
		launchParams.launchBlockID = extendedBlock;
		launchParams.launchEPBLength = extendedBlockLen;
		launchParams.launchFileFlags = 0;
		launchParams.launchControlFlags = launchContinue + launchNoFileFlags;
		launchParams.launchAppSpec = &appSpec;
		launchParams.launchAppParameters = NULL;
		err = LaunchApplication(&launchParams);
		if (err != noErr)
			return false;
		err = FindProcessBySignature(fApplSig,'APPL',psn,&dummy);
		if (err != noErr)
			return false;
	}
	Try_	// Try the old Spyglass AE suite way first
	{
		AppleEvent event;
		
		err = AEUtilities::CreateAppleEvent(AE_spy_send_suite, AE_spy_openURL, event, psn);
		ThrowIfOSErr_(err);
		// put in the URL
		StAEDescriptor urlDesc(typeChar, url->address, url->address ? strlen(url->address) : 0);
		err = ::AEPutParamDesc(&event,keyDirectObject,&urlDesc.mDesc);
		ThrowIfOSErr_(err);
		// Send it
		AppleEvent reply;
		Try_
		{
			err = ::AESend(&event, &reply, kAEWaitReply,kAENormalPriority,60,nil, nil);
			AEDisposeDesc(&event);
			err = AEUtilities::EventHasErrorReply(reply);
			ThrowIfOSErr_(err);
			AEDisposeDesc(&reply);
		}
		Catch_(inErr)
		{
			AEDisposeDesc(&reply);
			
			// Bug #86055
			// A -1 means the handler didn't want the event, not that it didn't handle it.
			// In this case we should just return that the helper can't handle the protocol
			// and Communicator/Navigator should handle it rather than also sending a GURL
			// event to the helper app.  This works around a problem under MacOS 8 where
			// sending a GURL event to an app that didn't handle it could result in an infinite
			// loop when the OS decided to re-direct the GURL back to us and we promptly sent
			// it back to the handler that didn't handle it.
			if (err == -1)
				return false;
			else
				Throw_(inErr);			
		}
		EndCatch_
	}
	Catch_(inErr)	// old Spyglass AE suite way failed, try the standard event
	{
		AppleEvent reply;
		Try_
		{
			AppleEvent event;
			err = AEUtilities::CreateAppleEvent(AE_url_suite, AE_url_getURL, event, psn);
			// put in the URL
			StAEDescriptor urlDesc(typeChar, url->address, url->address ? strlen(url->address) : 0);
			err = ::AEPutParamDesc(&event,keyDirectObject,&urlDesc.mDesc);
			ThrowIfOSErr_(err);
			err = ::AESend(&event, &reply, kAEWaitReply,kAENormalPriority,60,nil, nil);
			AEDisposeDesc(&event);
			ThrowIfOSErr_(AEUtilities::EventHasErrorReply(reply));
			AEDisposeDesc(&reply);
		}
		Catch_(inErr)
		{
			AEDisposeDesc(&reply);
			return false;
		}
		EndCatch_
	}
	EndCatch_
	return true;
}

void CProtocolHelper::AddNewHelper(CProtocolHelper* helper)
{
	if (helper == NULL)
		return;

	LArrayIterator iter(sProtocolHandlers);
	CProtocolHelper * otherHelper;
	while (iter.Next(&otherHelper))	// Delete duplicate registration for this protocol
		if (*helper == otherHelper)
		{
			delete otherHelper;
			sProtocolHandlers.Remove(&otherHelper);
		}
	sProtocolHandlers.InsertItemsAt(1,1, &helper);
	NET_AddExternalURLType(helper->fProtocolInfo);
	CPrefs::SetModified();
}

// Called from preferences, saves all the protocol  handlers
void CNotifierRegistry::ReadProtocolHandlers()
{	
	// Add the bolo handler
	CProtocolHelper *helper = new CProtocolHelper(strdup("bolo"), 'BOLO');
	
	CProtocolHelper::AddNewHelper(helper);
	CPrefs::UsePreferencesResFile();
	
	Handle stringListHandle = ::Get1Resource('STR#', PROT_HANDLER_PREFS_RESID);
	
	if (stringListHandle && *stringListHandle)
	{
		if (::GetHandleSize(stringListHandle) < sizeof(short))
		{
			::RemoveResource(stringListHandle);
			::DisposeHandle(stringListHandle);
			return;
		}
	}
	
	CStringListRsrc		stringRsrc(PROT_HANDLER_PREFS_RESID);
	Int16 howMany = stringRsrc.CountStrings();
	if (howMany == 0)
		return;
	//  Each protocol handler is represented by 2 strings
	// 1 - the application sig
	// 2 - the protocol string
	for (int i=1; i < howMany; i=i+2)	// Increment by 2.
	{
		CStr255 applSigStr, protocol;
		stringRsrc.GetString(i, applSigStr);
		if (ResError()) return;
		stringRsrc.GetString(i+1, protocol);
		if (ResError()) return;

		OSType appSig;
		LString::PStrToFourCharCode(applSigStr, appSig);
		CProtocolHelper * newHelper = new CProtocolHelper(XP_STRDUP((char*)protocol), appSig);
		CProtocolHelper::AddNewHelper(newHelper);
	}
}

// Called from preferences, writes all the protocol  handlers
void CNotifierRegistry::WriteProtocolHandlers()
{
	Int32 howMany = sProtocolHandlers.GetCount();
	if (howMany <= 1)
		return;
	
	Handle stringListHandle = ::Get1Resource('STR#', PROT_HANDLER_PREFS_RESID);

	if (!stringListHandle) {
		stringListHandle = ::NewHandle(0);
		::AddResource(stringListHandle, 'STR#',
			PROT_HANDLER_PREFS_RESID, CStr255::sEmptyString);
	}
	
	if (stringListHandle && *stringListHandle)
	{
		SInt8 flags = ::HGetState(stringListHandle);
		::HNoPurge(stringListHandle);
		
		CStringListRsrc		stringRsrc(PROT_HANDLER_PREFS_RESID);
		stringRsrc.ClearAll();
		for (int i=1; i<=howMany - 1; i++)
		{
			CProtocolHelper * helper = NULL;
			if (sProtocolHandlers.FetchItemAt(i, &helper))
			{
				CStr255 protocol(helper->fProtocolInfo);
				Str255 sig;
				LString::FourCharCodeToPStr(helper->fApplSig, sig);
				stringRsrc.AppendString(sig);
				stringRsrc.AppendString(protocol);
			}
		}
		::WriteResource(stringListHandle);
		::HSetState(stringListHandle, flags);
	}
}

void CNotifierRegistry::HandleAppleEvent(const AppleEvent &inAppleEvent, AppleEvent &outAEReply,
									AEDesc &outResult, long	inAENumber)
{
	switch(inAENumber)	{
		case AE_RegisterURLEcho:
			HandleRegisterURLEcho(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
		case AE_UnregisterURLEcho:
			HandleUnregisterURLEcho(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
		case AE_RegisterProtocol:
			HandleRegisterProtocol(inAppleEvent, outAEReply, outResult, inAENumber);
			break;			
		case AE_UnregisterProtocol:
			HandleUnregisterProtocol(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
		default:
			ThrowOSErr_(errAEEventNotHandled);
	}
}

// Always save the PSN
void CNotifierRegistry::HandleRegisterURLEcho(const AppleEvent &inAppleEvent, AppleEvent &outAEReply,
									AEDesc &/*outResult*/, long	/*inAENumber*/)
{
	OSType appSignature;
	ProcessSerialNumber psn;
	
	Size 	realSize;
	OSType	realType;
	
	OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeApplSignature, &realType,
						&appSignature, sizeof(appSignature), &realSize);
	if (err == noErr)	// No parameters, extract the signature from the Apple Event
		psn = GetPSNBySig(appSignature);
	else
		psn = MoreExtractFromAEDesc::ExtractAESender(inAppleEvent);
	// Each application can register only once
	LArrayIterator iter(sURLEchoHandlers);
	ProcessSerialNumber newPSN;
	while (iter.Next(&newPSN))	// If we are already registered, returns 
		if ((newPSN.highLongOfPSN == psn.highLongOfPSN) && (newPSN.lowLongOfPSN == psn.lowLongOfPSN))
			ThrowOSErr_(errAECoercionFail);
	sURLEchoHandlers.InsertItemsAt(1,1, &psn);
	{
		Boolean success = true;
		StAEDescriptor	replyDesc(success);
		err = ::AEPutParamDesc(&outAEReply, keyAEResult, &replyDesc.mDesc);
	}
}



void CNotifierRegistry::HandleUnregisterURLEcho(const AppleEvent &inAppleEvent,
	AppleEvent &/*outAEReply*/, AEDesc &/*outResult*/, long	/*inAENumber*/)
{
	OSType appSignature;
	ProcessSerialNumber psn;
	
	Size 	realSize;
	OSType	realType;

	OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeApplSignature, &realType,
						&appSignature, sizeof(appSignature), &realSize);
	if (err == noErr)	// No parameters, extract the signature from the Apple Event
		psn = GetPSNBySig(appSignature);
	else
		psn = MoreExtractFromAEDesc::ExtractAESender(inAppleEvent);
	LArrayIterator	iter(::sURLEchoHandlers);
	ProcessSerialNumber newPSN;
	while (iter.Next(&newPSN))
		if ((newPSN.highLongOfPSN == psn.highLongOfPSN) && (newPSN.lowLongOfPSN == psn.lowLongOfPSN))
			sURLEchoHandlers.Remove(&newPSN);
}

// Echoing of the URLs. For each registered application, send them the URLEcho AE
void FE_URLEcho(URL_Struct *url, int /*iStatus*/, MWContext *context)
{
	ProcessSerialNumber psn;
	OSErr err;
	LArrayIterator	iter(sURLEchoHandlers);
	while (iter.Next(&psn))
	Try_
	{
	// Create the event, fill in all the arguments, and send it
		AEAddressDesc	target;	// Target the event
		err = AECreateDesc(typeProcessSerialNumber, &psn,sizeof(psn), &target);
		ThrowIfOSErr_(err);
		AppleEvent	echoEvent;
		err = ::AECreateAppleEvent(AE_spy_send_suite, AE_spy_URLecho,
									&target,
									kAutoGenerateReturnID,
									kAnyTransactionID,
									&echoEvent);
		ThrowIfOSErr_(err);
		AEDisposeDesc(&target);
	// Add the URL
		if (url->address)
		{
			err = ::AEPutParamPtr(&echoEvent, keyDirectObject, typeChar, url->address, strlen(url->address)); 
			ThrowIfOSErr_(err);
		}
	// Add the MIME type
		if (url->content_type)
		{
			err = ::AEPutParamPtr(&echoEvent, AE_spy_URLecho_mime, typeChar, url->content_type, strlen(url->content_type)); 
			ThrowIfOSErr_(err);
		}
	// Add the refererer
		if (url->referer)
		{
			err = ::AEPutParamPtr(&echoEvent, AE_spy_URLecho_referer, typeChar, url->referer, strlen(url->referer)); 
			ThrowIfOSErr_(err);
		}	
	// Add the window ID
		CNSContext* nsContext = ExtractNSContext(context);
		ThrowIfNil_(context);
		Int32 windowID = nsContext->GetContextUniqueID();
		err = ::AEPutParamPtr(&echoEvent, AE_spy_URLecho_win, typeLongInteger, &windowID, sizeof(windowID)); 
		ThrowIfOSErr_(err);
		AppleEvent reply;
		err = ::AESend(&echoEvent, &reply, kAENoReply,kAENormalPriority,0,nil, nil);
		AEDisposeDesc(&echoEvent);
		ThrowIfOSErr_(err);
	}
	Catch_(inErr){}
	EndCatch_
}

// Registering the protocol
// The protocol is registered by application signature
void CNotifierRegistry::HandleRegisterProtocol(const AppleEvent &inAppleEvent,
	AppleEvent &/*outAEReply*/, AEDesc &/*outResult*/, long	/*inAENumber*/)
{
	Size realSize;
	DescType realType;
	OSType appSignature;
	char * protocol = nil;
	CProtocolHelper * volatile helper;
	
	Try_
	{
		OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeApplSignature, &realType,
							&appSignature, sizeof(appSignature), &realSize);
		if (err != noErr)	// Signature was not passed appropriately typed, try as type
		{
			OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeType, &realType,
							&appSignature, sizeof(appSignature), &realSize);
			if (err != noErr)	// No signature passed, extract it from the Apple Event
			{
				ProcessSerialNumber psn = MoreExtractFromAEDesc::ExtractAESender(inAppleEvent);
				ProcessInfoRec pir;
				FSSpec dummy;
				pir.processAppSpec = &dummy;
				err = ::GetProcessInformation(&psn, &pir);
				ThrowIfOSErr_(err);
				appSignature = pir.processSignature;
			}
		}
		// Extract the protocol
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_register_protocol_pro, protocol);
		// Have app signature, and protocol, add them to the list
		helper = new CProtocolHelper(protocol, appSignature);
		CProtocolHelper::AddNewHelper(helper);
	}
	Catch_(inErr){}
	EndCatch_
}

void CNotifierRegistry::HandleUnregisterProtocol(const AppleEvent &inAppleEvent,
	AppleEvent &/*outAEReply*/, AEDesc &/*outResult*/, long	/*inAENumber*/)
{
	Size realSize;
	DescType realType;
	OSType appSignature;
	char * protocol = nil;
	
	Try_
	{
		OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeApplSignature, &realType,
							&appSignature, sizeof(appSignature), &realSize);
		if (err != noErr)
			err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeType, &realType,
							&appSignature, sizeof(appSignature), &realSize);
		if (err != noErr)	// No signature passed, extract it from the Apple Event
		{
			ProcessSerialNumber psn = MoreExtractFromAEDesc::ExtractAESender(inAppleEvent);
			ProcessInfoRec pir;
			FSSpec dummy;
			pir.processAppSpec = &dummy;
			err = ::GetProcessInformation(&psn, &pir);
			ThrowIfOSErr_(err);
			appSignature = pir.processSignature;
		}
		// Extract the protocol. Not necessary. If we only have the sig, remove all the registered protocols
		Try_
		{
			MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_register_protocol_pro, protocol);
		}
		Catch_(inErr){}
		EndCatch_
		// Delete it from the list
		LArrayIterator iter(sProtocolHandlers);
		CProtocolHelper * helper;
		while (iter.Next(&helper))	// Delete duplicate registration for this protocol
			if (helper->EqualTo(protocol, appSignature))
			{
				delete helper;
				sProtocolHandlers.Remove(&helper);
			}
		if (protocol)
			NET_DelExternalURLType(protocol);
	}
	Catch_(inErr){}
	EndCatch_
}


XP_Bool FE_UseExternalProtocolModule(MWContext *context,
	FO_Present_Types /*iFormatOut*/, URL_Struct *url,
	Net_GetUrlExitFunc */*pExitFunc*/)
{

#ifndef MOZ_MAIL_NEWS	
	if (url->address && CInternetConfigInterface::CurrentlyUsingIC()) {
		ICError err = CInternetConfigInterface::SendInternetConfigURL(url->address);
		if (err == noErr)
			return true;
	}
#endif

	LArrayIterator iter(sProtocolHandlers);
	CProtocolHelper * helper;
	while (iter.Next(&helper))
		if (helper->AttemptLaunch(url, context))
			return true;
	return false;
}

