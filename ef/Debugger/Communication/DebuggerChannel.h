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
// DebuggerChannel.h
//
// Scott M. Silver

// A simple minded communication stream for
// the debugger pieces client/server

#ifndef _H_DEBUGGERCHANNEL
#define _H_DEBUGGERCHANNEL 

#include "prio.h"
#include "prprf.h"
#include "Fundamentals.h"
#include "prlock.h"
#ifdef assert
#undef assert
#include <assert.h>
#endif

#ifdef XP_PC
#    include <wtypes.h>
#endif

// FIX-ME endianness???
struct DebuggerMessageRequest
{
	Int32  request; 
};

struct DebuggerMessageResponse
{
	Int32 status;  // 0 = succesful, non-zero is an error code
};

const Int32 kAddressToMethod = 120;
const Int32 kMethodToAddress = 121;
const Int32 kRequestNotifyMethodCompiledLoaded = 122;
const Int32 kNotifyMethodCompiledLoaded = 123;
const Int32 kRunClass = 124;
const Int32 kNotifyOnClassLoad = 125;
const Int32 kNotifyOnException = 126;
const Int32 kRequestDebuggerThread = 127;

class NS_EXTERN DebuggerStream
{
	PRFileDesc*		mFileDesc;

public:
			DebuggerStream(PRFileDesc* inCommChannel) :
				mFileDesc(inCommChannel) { }
	        DebuggerStream() :
                mFileDesc(0) { }

	void    writeRequest(Int32 inRequest);
	Int32   readRequest();
	void    writeResponse(Int32 inStatus);
	Int32   readResponse();
	
	Int32	readLength();  // actual binary int32

	void	writeString(const char* inString);
	char*   readString();  // string must be deleted [] by callee

	void*	readPtr();     
	void    writePtr(const void* inPtr);

	// r/w raw buffer dumping
	void	writeDataRaw(const void* inData, Int32 inLength);
	void	readDataRaw(void* outData, Int32 inLength);	

	// r/w length preceded data
	void	writeData(const void* inData, Int32 inLength);
	void*	readData();
};

#ifdef WIN32
void NS_EXTERN  setDebuggerThreadID(DWORD inThreadID);
DWORD NS_EXTERN  getDebuggerThreadID();
#define SET_DEBUGGER_THREAD_ID(inID) \
  PR_BEGIN_MACRO \
  setDebuggerThreadID(inID); \
  PR_END_MACRO
#else
#define SET_DEBUGGER_THREAD_ID(inID) \
  PR_BEGIN_MACRO \
  PR_END_MACRO
#endif

const int kDebuggerPort = 8001;

typedef void (*NotifyCompileOrLoadedMethodHandler)(const char*, void*);

class NS_EXTERN DebuggerClientChannel 
{
	DebuggerStream  mSync;
	DebuggerStream  mAsync;
	NotifyCompileOrLoadedMethodHandler mCompLoadHandler;
	PRLock*         mLock;

public:
	static DebuggerClientChannel*  createClient();
	
	char*   requestAddressToMethod(const void* inAddress, Int32& outOffset);
	void*   requestMethodToAddress(const char* inMethodName);
	void    sendOneCharacterRequest(char inCharRequest);
	void*	requestDebuggerThread();
	Int32   requestNotifyOnMethodCompileLoad(const char* inMethodName);
	Int32   requestRunClass(const char* inClassName);
	Int32   requestNotifyOnClassLoad(const char* inClassName);
	Int32   requestNotifyOnException(const char* inClassName);

	void    setCompileOrLoadMethodHandler(NotifyCompileOrLoadedMethodHandler inHandler) { mCompLoadHandler = inHandler; }

protected:
	DebuggerClientChannel(PRFileDesc* inSync, PRFileDesc* inAsync);

	void    handleCompileOrLoadedMethod(const char* inMethodName, void *inAddress) 
	{ 
		if (mCompLoadHandler) 
			(*mCompLoadHandler)(inMethodName, inAddress); 
	}

	bool    waitForAsyncRequest();
	static  void asyncRequestThread(void* inThisPtr);
	void    handleAsyncRequest(Int32 inRequest);
};

#endif

