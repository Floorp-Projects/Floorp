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
/*
	NativeCodeCache
*/

#ifndef _NATIVE_CODE_CACHE_
#define _NATIVE_CODE_CACHE_

#include "ExceptionTable.h"
#include "FormatStructures.h"
#include "Pool.h"
#include "Attributes.h"
#include "InfoItem.h"
#include "HashTable.h"
#include "FieldOrMethod.h"

#include "LogModule.h"
#include "prmon.h"

//-----------------------------------------------------------------------------------------------------------
// This is the key for entries in the cache
// Currently it retainst he "fully qualified" name
// of the method.
struct MethodDescriptor
{
	MethodDescriptor(Method& inMethod) :
		method(&inMethod) { };
		
	Method *method;			// interned full name string describing this method
	
	void operator=(const MethodDescriptor& inCopy) { method = inCopy.method; }
};

//-----------------------------------------------------------------------------------------------------------
struct PlatformTVector
{
	Uint8*	mTVector;
	
	void
	setTVector(Uint8* inTVector) { mTVector = inTVector; }

	Uint8* 
	getFunctionAddress() const
	{
	#ifdef GENERATING_FOR_POWERPC
		if (mTVEctor != NULL)
			return (*(Uint8***)mTVector);
		else	
			return (NULL);
	#elif defined(GENERATING_FOR_X86)
		return (mTVector);
	#else
		return (mTVector);	// FIX-ME This probably is not right
	#endif
	}

	Uint8*
	getEnvironment() const
	{
	#ifdef GENERATING_FOR_POWERPC
		return (*((Uint8***)mTVector + 1));
	#else
		assert(false);
		return (NULL);
	#endif
	}
};

//-----------------------------------------------------------------------------------------------------------
// An entry in the CodeCache
// Not intended for use outside of NativeCode Cache
struct CacheEntry
{
	CacheEntry(const MethodDescriptor& inMethodDescriptor) :
		descriptor(inMethodDescriptor), stub(0) { }
		
	MethodDescriptor	descriptor;		// unique descriptor of the method
	PlatformTVector		start;			// default entry into the function
	Uint8*				end;			// pointer to byte _after_ end of function
	Uint8*				stub;			// if non-NULL this is the address of the stub function
	ExceptionTable*		eTable;			// pointer to the exception table
	mutable bool		shouldBackPatch; // If true, specifies that the call site must be backpatched on compile

	// FIX for now the restore policy will be kept in the NativeCodeCache, later we may want to store the restore policies elsewhere
	StackFrameInfo		policy;				// specifies how to restore registers as stack frames are popped
	Uint8* asynchVersion(Pool& pool, Uint8*& ip);
};

//-----------------------------------------------------------------------------------------------------------
class NativeCodeCache
{
	PRMonitor* _mon;
public:
				NativeCodeCache() : 
					mIndexByDescriptor(mPool),
					mIndexByRange()
					{ _mon = PR_NewMonitor(); }
					
	Uint8*		acquireMemory(const size_t inBytes);

	void		mapMemoryToMethod(	const MethodDescriptor& inMethodDescriptor, 
									PlatformTVector inWhere, 
									Uint8* inEnd, 
									ExceptionTable* pExceptionTable,
									StackFrameInfo& inPolicy );

	addr		lookupByDescriptor(const MethodDescriptor& inDescriptor, bool inhibitBackpatch, bool inLookupOnly = false);
	CacheEntry*	lookupByRange(Uint8* inAddress);

protected:
	HashTable<CacheEntry*>	mIndexByDescriptor;	// hashtable of entrypoints to methods
	Vector<CacheEntry*>		mIndexByRange;		// maps range -> method
public:
	Pool					mPool;				// resource for our cache
	static inline NativeCodeCache&	getCache() { return (NativeCodeCache::sNativeCodeCache); }

	static NativeCodeCache sNativeCodeCache;

	#ifdef DEBUG
	void		printMethodTable(LogModuleObject &f);
	#endif

	static void enter() {
		PR_EnterMonitor(sNativeCodeCache._mon);
	}

	static void exit() {
		PR_ExitMonitor(sNativeCodeCache._mon);
	}
};

// inCacheEntry: reference to actual CacheEntry in memory 
// inCache: the cache in question
// creates a bit of code which when called compiles the method
// described by inCacheEntry.descriptor and then calls "compileAndBackPatchMethod"
// then jump to the method returned by compileAndBackPatchMethod

extern void*
generateCompileStub(NativeCodeCache& inCache, const CacheEntry& inCacheEntry);

extern void*
generateNativeStub(NativeCodeCache& inCache, const CacheEntry& inCacheEntry, void *nativeFunction);

extern void *
generateJNIGlue(NativeCodeCache& inCache,
                const CacheEntry& inCacheEntry,
                void *nativeFunction);

extern "C" void *
compileAndBackPatchMethod(const CacheEntry* inCacheEntry, void* inLastPC, void* inUserDefined);

// atomically backpatch a caller of a stub generated with generateCompileStub
// inMethodAddress the address of the callee
// inLastPC the next PC to be executed up return from the callee
// inUserDefined is a (probably) per platform piece of data that will be passed
// on to the backPatchMethod function.
void* backPatchMethod(void* inMethodAddress, void* inLastPC, void* inUserDefined);

void NS_EXTERN printMethodTable();

#endif // _NATIVE_CODE_CACHE_
