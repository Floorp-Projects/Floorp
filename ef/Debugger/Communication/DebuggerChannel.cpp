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
// DebuggerChannel.cpp
//
// Scott M. Silver

#include "DebuggerChannel.h"
#include "prio.h"
#include "prthread.h"
#include "prlock.h"
#ifdef assert
#undef assert
#endif
#include <assert.h>
#include <string.h>
#include <ctype.h>

void DebuggerStream::
writeRequest(Int32 inRequest)
{
	DebuggerMessageRequest msgRequest = { inRequest };

	writeDataRaw(&msgRequest, sizeof(msgRequest));
}


Int32 DebuggerStream::
readRequest()
{
	DebuggerMessageRequest msgRequest;

	readDataRaw(&msgRequest, sizeof(msgRequest));

	return (msgRequest.request);
}


void DebuggerStream::
writeResponse(Int32 inStatus)
{
	DebuggerMessageResponse msgResponse = { inStatus };

	writeDataRaw(&msgResponse, sizeof(msgResponse));
}


Int32 DebuggerStream::
readResponse()
{
	DebuggerMessageResponse msgResponse;

	readDataRaw(&msgResponse, sizeof(msgResponse));

	return (msgResponse.status);
}


void DebuggerStream::
writeString(const char* inString)
{
	writeData(inString, strlen(inString));
}


void DebuggerStream::
writeDataRaw(const void* inData, Int32 inLength)
{
	if (PR_Write(mFileDesc, inData, inLength) != inLength)
		trespass("comm channel dead");
}


void DebuggerStream::
writeData(const void* inData, Int32 inLength)
{
	if (PR_Write(mFileDesc, &inLength, 4) != 4)
		trespass("comm channel dead");

	if (PR_Write(mFileDesc, inData, inLength) != inLength)
		trespass("comm channel dead");
}


Int32 DebuggerStream::
readLength()
{
	Int32	length;

	readDataRaw(&length, 4);
	return length;
}


void DebuggerStream::
readDataRaw(void* outData, Int32 inLength)
{
	if (PR_Read(mFileDesc, outData, inLength) != inLength)
		trespass("comm channel dead");
}


void* DebuggerStream::
readData()
{
	Int32 len = readLength();
	void* data = (void*) new char[len];

	readDataRaw(data, len);

	return (data);
}


void DebuggerStream::
writePtr(const void* inPtr)
{
	char* buffer = PR_smprintf("%p", inPtr);
	writeString(buffer);
	PR_smprintf_free(buffer);
}


static PRPtrdiff 
convertAsciiToHex(const char* inString)
{
	const char* lc = inString+strlen(inString);	// point to '\0'
	PRPtrdiff	value = 0;
	PRIntn		multiplier = 0;

	while (--lc >= inString)
	{
		if (isdigit(*lc))
			value += ((*lc - 48) << multiplier);
		else if ((*lc >= 97) && (*lc <= 102))
			value += ((*lc - 87) << multiplier);
		else
			return (value);		// bail

		multiplier += 4;
	}

	return (value);
}


char* DebuggerStream::
readString()
{
	Int32	len = readLength();
	char*	data = new char[len+1];

	readDataRaw(data, len);
	data[len+1-1] = '\0';	// null terminate string	
	
	return (data);
}


void* DebuggerStream::
readPtr()
{
	char* data = readString();
	void* rv;

	rv = (void*) convertAsciiToHex(data);

	delete[] data;

	return (rv);
}

bool DebuggerClientChannel::
waitForAsyncRequest()
{
	for (;;)
	{
		Int32 request = mAsync.readRequest();
		

		handleAsyncRequest(request);
	}

	return (true);
}

void DebuggerClientChannel::
asyncRequestThread(void* inThisPtr)
{
	DebuggerClientChannel* thisChannel = (DebuggerClientChannel*) inThisPtr;

	thisChannel->waitForAsyncRequest();
}

DebuggerClientChannel* DebuggerClientChannel::
createClient()
{
	PRFileDesc* async;
	PRFileDesc* sync;

	// specify loopback
	PRNetAddr netaddr;

	netaddr.inet.family = AF_INET;
	netaddr.inet.port = htons(kDebuggerPort);
	netaddr.inet.ip = htonl(0x7f000001); // 127.0.0.1

	if ((sync = PR_NewTCPSocket()) == NULL) 
		return (0);
	
	if (PR_Connect(sync, &netaddr, PR_INTERVAL_NO_TIMEOUT) < 0)
		return (0);

	if ((async = PR_NewTCPSocket()) == NULL) 
		return (0);
	
	if (PR_Connect(async, &netaddr, PR_INTERVAL_NO_TIMEOUT) < 0)
		return (0);

	DebuggerClientChannel* channel = new DebuggerClientChannel(sync, async);
	PRThread*		thread;

	thread = PR_CreateThread(PR_USER_THREAD,
							 &asyncRequestThread,
							 channel,
							 PR_PRIORITY_NORMAL,
							 PR_GLOBAL_THREAD,
							 PR_JOINABLE_THREAD,
							 0);

	// if we can't create the async thread, this is failure
	if (!thread)
	{
		delete channel;
		return (0);
	}

	return (channel);
}

// Constructor locks the passed in lock
// Destructor unlocks the lock.
// Useful for a one line critical section
class StLocker
{
	PRLock* mLock;

public:
	StLocker(PRLock* inLock) :
		mLock(inLock) { PR_Lock(mLock); }

	~StLocker()
	{
		PRStatus status = PR_Unlock(mLock);
		assert(status == PR_SUCCESS);
	}
};


char* DebuggerClientChannel::
requestAddressToMethod(const void* inAddress, Int32& outOffset)
{
	StLocker locker(mLock);
	mSync.writeRequest(kAddressToMethod);

	// now send the arguments (one hex address)
	mSync.writePtr(inAddress);
	
	Int32 response = mSync.readResponse();

	if (!response)
	{
		char *methodName = mSync.readString();
		outOffset = (Int32) mSync.readPtr();
		return (methodName);
	}
	
	return (NULL);
}

DebuggerClientChannel::
DebuggerClientChannel(PRFileDesc* inSync, PRFileDesc* inAsync) :
	mSync(inSync),
	mAsync(inAsync),
	mCompLoadHandler(0) 
{ 
	mLock = PR_NewLock();
	assert(mLock);
}

void* DebuggerClientChannel::
requestMethodToAddress(const char* inMethodName)
{
	StLocker  locker(mLock);
	mSync.writeRequest(kMethodToAddress);

	// now send the arguments (one hex address)
	mSync.writeString(inMethodName);
	
	Int32 response = mSync.readResponse();
	
	void* address = (response == 0) ? mSync.readPtr() : 0;

	return (address);
}

void DebuggerClientChannel::
sendOneCharacterRequest(char inCharRequest)
{
	StLocker  locker(mLock);
	mSync.writeRequest((Int32) inCharRequest);
}

void DebuggerClientChannel::
handleAsyncRequest(Int32 inRequest)
{
	// method does not get run until 
	if (inRequest == kNotifyMethodCompiledLoaded)
	{
		char* methodName = mAsync.readString();
		void* address = mAsync.readPtr();

		handleCompileOrLoadedMethod(methodName, address);
		delete [] methodName;
		mAsync.writeResponse(0);
	}
	else
		mAsync.writeResponse(-1);
}


Int32 DebuggerClientChannel::
requestNotifyOnMethodCompileLoad(const char* inMethodName)
{
	StLocker  locker(mLock);
	mSync.writeRequest(kRequestNotifyMethodCompiledLoaded);

	// now send the arguments (one string)
	mSync.writeString(inMethodName);
	
	return(mSync.readResponse());
}

Int32 DebuggerClientChannel::
requestRunClass(const char* inClassName)
{
	StLocker  locker(mLock);
	mSync.writeRequest(kRunClass);

	// now send the arguments (one string)
	mSync.writeString(inClassName);

	return (mSync.readResponse());
}

Int32 DebuggerClientChannel::
requestNotifyOnClassLoad(const char* inClassName)
{
	StLocker  locker(mLock);
	mSync.writeRequest(kNotifyOnClassLoad);

	// now send the arguments (one string)
	mSync.writeString(inClassName);
	
	return(mSync.readResponse());
}

Int32 DebuggerClientChannel::
requestNotifyOnException(const char* inClassName)
{
	StLocker  locker(mLock);
	mSync.writeRequest(kNotifyOnException);

	// now send the arguments (one string)
	mSync.writeString(inClassName);
	
	return(mSync.readResponse());
}

void* DebuggerClientChannel::
requestDebuggerThread()
{
	StLocker  locker(mLock);
	mSync.writeRequest(kRequestDebuggerThread);

   if (!mSync.readResponse())
	   return (mSync.readPtr());
   else
	   return (0);
}

#ifdef WIN32
// The DLL entry-point function sets up shared memory using 
// a named file-mapping object. 

#include <windows.h> 
#include <memory.h> 
 
static DWORD* sThreadID = NULL; // pointer to shared memory
 
BOOL WINAPI DllEntryPoint(HINSTANCE hinstDLL,  // DLL module handle
    DWORD fdwReason,                    // reason called 
    LPVOID lpvReserved)                 // reserved 
{ 
    HANDLE hMapObject = NULL;  // handle to file mapping
    BOOL fInit, fIgnore; 
 
    switch (fdwReason) 
    { 
        // The DLL is loading due to process 
        // initialization or a call to LoadLibrary. 
		
	    case DLL_PROCESS_ATTACH: 
            // Create a named file mapping object.
            hMapObject = CreateFileMapping( 
                (HANDLE) 0xFFFFFFFF, // use paging file
                NULL,                // no security attributes
                PAGE_READWRITE,      // read/write access
                0,                   // size: high 32-bits
                sizeof(*sThreadID),           // size: low 32-bits
                "debuggerThreadID");    // name of map object
            if (hMapObject == NULL) 
                return FALSE; 
 
            // The first process to attach initializes memory.
            fInit = (GetLastError() != ERROR_ALREADY_EXISTS); 
 
            // Get a pointer to the file-mapped shared memory.
            sThreadID = (DWORD*) MapViewOfFile( 
                hMapObject,     // object to map view of
                FILE_MAP_WRITE, // read/write access
                0,              // high offset:  map from
                0,              // low offset:   beginning
                0);             // default: map entire file
            if (sThreadID == NULL) 
                return FALSE; 
 
            // Initialize memory if this is the first process.
            if (fInit) 
                memset(sThreadID, 0, sizeof(*sThreadID));
 
            break; 
        // The attached process creates a new thread. 
        case DLL_THREAD_ATTACH: 
            break; 
		// The thread of the attached process terminates.
        case DLL_THREAD_DETACH: 
            break; 
        // The DLL is unloading from a process due to 
        // process termination or a call to FreeLibrary. 
        case DLL_PROCESS_DETACH: 
             // Unmap shared memory from the process's address space.
             fIgnore = UnmapViewOfFile(sThreadID); 
 
            // Close the process's handle to the file-mapping object.
 
            fIgnore = CloseHandle(hMapObject); 
 
            break; 
 
        default: 
          break; 
     } 
 
    return TRUE; 
    UNREFERENCED_PARAMETER(hinstDLL); 
    UNREFERENCED_PARAMETER(lpvReserved); 
} 
 
void setDebuggerThreadID(DWORD inThreadID)
{
	*sThreadID = inThreadID;
}

DWORD getDebuggerThreadID()
{
	return (*sThreadID);
}
#endif
