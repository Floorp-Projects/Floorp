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
// NativeCodeCache
//
// Scott M. Silver
//

#include "NativeCodeCache.h"
#include "StringUtils.h"
#include "ClassWorld.h"
#include "FieldOrMethod.h"
#include "NativeMethodDispatcher.h"
#include "JavaVM.h"

UT_DEFINE_LOG_MODULE(NativeCodeCache);

#ifdef _WIN32
Uint8* CacheEntry::asynchVersion(Pool& pool, Uint8*& ip)
{
	Uint8* src = start.getFunctionAddress();
	Int32 sz = end-src;
	Uint8* dest = new(pool) Uint8[sz];
	Uint32* asynchPoints = eTable->getAsynchPoints();
	int numberofAPoints = eTable->getNrOfAsynchPoints();
	// loop through code and for each byte corresponding to an asynchronous
	// checkpoint, insert 'int 3' (0xCC).
	memcpy((void*)dest,(const void*)src,sz);
	for (int j=0; j<numberofAPoints; j++)
		dest[asynchPoints[j]] = 0xcc;
	ip = dest + (ip-src);
	return dest;
}
#else
Uint8* CacheEntry::asynchVersion(Pool& /*pool*/, Uint8*& /*ip*/)
{
	assert(false);
	return NULL;
}
#endif


// there is one of these per VM instantiation
NativeCodeCache NativeCodeCache::sNativeCodeCache;

// Access to the database of classes
extern ClassWorld world;

#ifdef DEBUG_LOG
static void printFrame(Method *method);
#endif

// acquireMemory
//
// Reserve inBytes of data for (potentially) a new method, or data
Uint8* NativeCodeCache::
acquireMemory(const size_t inBytes)
{
	// is Pool new thread safe?
	return ((Uint8*) new(mPool) char[inBytes]);
}


// mapMemoryToMethod
//
// Assign a "name" to a method.  When other users ask the cache
// for a given method, this is the entry point returned.  The
// first call for a specific MethodDescriptor M and function f,
// will assign f to M.
//
// FIX-ME do all HashTable entrypoints need monitors?
void NativeCodeCache::
mapMemoryToMethod(	const MethodDescriptor& inMethodDescriptor, 
					PlatformTVector inWhere, 
					Uint8* inEnd, 
					ExceptionTable* pExceptionTable,
					StackFrameInfo& inPolicy)
{
	CacheEntry*			existingEntry = NULL;
	TemporaryStringCopy	key(inMethodDescriptor.method->toString());
	
	mIndexByDescriptor.get(key, &existingEntry);

	if (!existingEntry)
	{
		CacheEntry&	cacheEntry  = *new(mPool) CacheEntry(inMethodDescriptor);
		cacheEntry.start = inWhere;
		cacheEntry.end = inEnd;
		cacheEntry.eTable = pExceptionTable;
		cacheEntry.policy = inPolicy;
		
		mIndexByDescriptor.add(key, &cacheEntry);
		mIndexByRange.append(&cacheEntry);
	}
	else 
	{
		existingEntry->start = inWhere;
		existingEntry->end = inEnd;
		existingEntry->eTable = pExceptionTable;
		existingEntry->policy = inPolicy;
	}
}


// lookupByRange
//
// Purpose:	used for stack crawling / debugging purposes
// In:		pointer to memory
// Out:		pointer to cache entry if in code cache (NULL otherwise)
//
// Notes:	currently we do a linear search of a vector of methods, eventually
//			we'll want a better search strategy
CacheEntry* NativeCodeCache::
lookupByRange(Uint8* inAddress)
{
	for(CacheEntry** ce= mIndexByRange.begin(); ce < mIndexByRange.end(); ce++)
	{
#ifdef GENERATE_FOR_X86
		if ((*ce)->stub <= inAddress && (*ce)->stub + 10 > inAddress)
			return *ce;
		else
#endif
		if(((*ce)->start.getFunctionAddress() <= inAddress) && (inAddress < (*ce)->end))
			return *ce;
	}
	return NULL;
}

// lookupByDescriptor
//
// Looks up method inMethodDescriptor, if inMethodDescriptor
// is not in the cache, this generates a new "stub" function
// (which will compile inMethodDescriptor and backpatch the caller)
// Since we have two entries in the CacheEntry, one for stub and
// the other for function, we return the function if it exists
// otherwise we return the stub.
// inLookupOnly - do not create a stub if the lookup fails
addr NativeCodeCache::
lookupByDescriptor(const MethodDescriptor& inMethodDescriptor, bool /*inhibitBackpatch*/, bool inLookupOnly)
{	
    Method *method = inMethodDescriptor.method;
	TemporaryStringCopy	key(method->toString());
	
	CacheEntry*	cacheEntry = NULL;
		
	mIndexByDescriptor.get(key, &cacheEntry);
	
	// short circuit if inLookupOnly 
	if (inLookupOnly && !cacheEntry)
		return (functionAddress(NULL));

	if (!cacheEntry)
	{
		CacheEntry&	newEntry = *new(mPool) CacheEntry(inMethodDescriptor);
		
		newEntry.stub = (unsigned char*) generateCompileStub(*this, newEntry);
		newEntry.start.setTVector(NULL);
		newEntry.end = NULL;
		newEntry.eTable = NULL;

		// FIX set shouldBackPatch to always false. Note that this
		// means that we compile evry function every time
		newEntry.shouldBackPatch = false;

		//newEntry.shouldBackPatch = !method->getDynamicallyDispatched();
		
		cacheEntry = &newEntry;		
		mIndexByDescriptor.add(key, cacheEntry);
		mIndexByRange.append(cacheEntry);
	}

    bool inhibitBackpatching = false;
    inhibitBackpatching = VM::theVM.getInhibitBackpatching();

	if (cacheEntry->start.getFunctionAddress() && !inhibitBackpatching)		
		return (functionAddress((void (*)())cacheEntry->start.getFunctionAddress()));
	else
	{
		if (!cacheEntry->stub)
            cacheEntry->stub = (unsigned char*) generateCompileStub(*this, *cacheEntry);
		return (functionAddress((void (*)())cacheEntry->stub));
	}
}


UT_DEFINE_LOG_MODULE(NativeCodeCacheMonitor);

// compileOrLoadMethod
//
// If the method pointed to by inCacheEntry is a native method, resolve and load 
// the native code library.  Otherwise compile the method's bytecode into
// native code.  Either way, return a pointer to the resulting native code.
// Sets shouldBackPatch to true if it is neccessary to back-patch the method.
static void* 
compileOrLoadMethod(const CacheEntry* inCacheEntry, bool &shouldBackPatch)
{
	Method* method;
  
	// if this function is already compiled then just return it
	if (inCacheEntry->start.getFunctionAddress() != NULL) 
	{
		shouldBackPatch = inCacheEntry->shouldBackPatch;
		goto ExitReturnFunc;
	}
	else
	{
		NativeCodeCache::enter();
		// maybe we slept while someone else was compiling, if so
		// don't do any work.
		if (inCacheEntry->start.getFunctionAddress() != NULL)
			goto ExitReturnFunc;
    
		method = inCacheEntry->descriptor.method;

		// If this is a native method, then resolve it appropriately
		if (method->getModifiers() & CR_METHOD_NATIVE) 
		{
			addr a = NativeMethodDispatcher::resolve(*method);

			if (!a) 
			{
				NativeCodeCache::exit();
				UT_LOG(NativeCodeCache, PR_LOG_ERROR, ("\tCould not resolve native method %s\n", method->getName()));
				runtimeError(RuntimeError::linkError);
			}
			else
				UT_LOG(NativeCodeCache, PR_LOG_DEBUG, ("\tResolved native method %s\n", method->getName()));


			NativeCodeCache &cache = NativeCodeCache::getCache();
			void* code = generateNativeStub(cache, *inCacheEntry, addressFunction(a));

			PlatformTVector tVector;
			tVector.setTVector((Uint8*)code);
			int codeLength = 0;  // We don't know the length of native methods yet

			/*ExceptionTable* pExceptionTable = NULL;	*/ // FIX native methods don't have exception tables yet
			StackFrameInfo policy;			// FIX we don't know the stack layout policies yet, so pass in junk

			cache.mapMemoryToMethod(inCacheEntry->descriptor, tVector, (Uint8*)code + codeLength, NULL, policy);
		} 
		else 
			method->compile();
	}

ExitReturnFunc:
	NativeCodeCache::exit();
	assert(inCacheEntry->start.getFunctionAddress());
	shouldBackPatch = inCacheEntry->shouldBackPatch;
	EventBroadcaster::broadcastEvent(gCompileOrLoadBroadcaster, kEndCompileOrLoad, inCacheEntry->descriptor.method);
	return (inCacheEntry->start.getFunctionAddress());
/*
ExitMonitorThrowCompileException:
  PR_CExitMonitor((void*)inCacheEntry);
  assert(false);
  return (NULL);
*/
}


// compileAndBackPatchMethod
//
// compile the method pointed to by inCacheEntry and then back patch
// the caller.  
//
// inLastPC = address of next native instruction to be executed
// if the callee executed and returned. (where callee is the method that
// is to be compiled)
extern "C" void* 
compileAndBackPatchMethod(const CacheEntry* inCacheEntry, void* inLastPC, void* inUserDefined)
{
  bool shouldBackPatch;

  void *nativeCode = compileOrLoadMethod(inCacheEntry, shouldBackPatch);

  if (VM::theVM.getInhibitBackpatching())
    shouldBackPatch = false;

#ifdef DEBUG
  if (VM::theVM.getTraceAllMethods()) 
  {
      Method *method = inCacheEntry->descriptor.method;  
      printFrame(method);
  }
#endif

  /* Back-patch method only if method was resolved statically */
  return (shouldBackPatch) ? 
    (backPatchMethod(nativeCode, inLastPC, inUserDefined)) : nativeCode;
}


#ifdef DEBUG_LOG
// C-entry point so we can call this from
// the MSVC debugger
void NS_EXTERN
printMethodTable()
{
	NativeCodeCache::getCache().printMethodTable(UT_LOG_MODULE(NativeCodeCache));
}

// printMethodTable
//
// Print out all the methods currently in the cache, with their
// ExceptionTables
void NativeCodeCache::
printMethodTable(LogModuleObject &f)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n__________________Method Table___________________\n"));
	for(CacheEntry** ce= mIndexByRange.begin(); ce < mIndexByRange.end(); ce++)
	{
		Uint8* pStart = (*ce)->start.getFunctionAddress();
		Uint8* pEnd = (*ce)->end;
		const char* pName= (*ce)->descriptor.method->toString();		
		ExceptionTable* eTable= (*ce)->eTable;

		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("[%#010x-%#010x] %s\n", Uint32(pStart), Uint32(pEnd), pName));
		if(eTable)
			eTable->printShort(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n\n"));
}

UT_EXTERN_LOG_MODULE(StackWalker);

#include "StackWalker.h"

// printFrame
//
// Print a frame of a given method (which is
// the caller).  This can be called from a debugger
static void printFrame(Method *method)
{
    UT_SET_LOG_LEVEL(StackWalker, PR_LOG_DEBUG);

    Frame frame;
    frame.moveToPrevFrame();
    frame.moveToPrevFrame();

    // Indent the frame according to stack depth
    int stackDepth = Frame::getStackDepth();
    int i;

    // Discount the stack frames used for internal EF native code
    stackDepth -= 7;
    if (stackDepth < 0)
        stackDepth = 0;

    for (i = 0; i < stackDepth; i++)
            UT_LOG(StackWalker, PR_LOG_ALWAYS, ("  "));

    Frame::printWithArgs(UT_LOG_MODULE(StackWalker), (Uint8*)(frame.getBase()), method);
	UT_LOG(StackWalker, PR_LOG_ALWAYS, ("\n"));

    UT_LOG_MODULE(StackWalker).flushLogFile();
}

#endif	// DEBUG_LOG
