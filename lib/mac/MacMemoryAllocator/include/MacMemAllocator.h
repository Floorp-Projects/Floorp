/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __MACMEMALLOCATOR__
#define	__MACMEMALLOCATOR__

#include "prtypes.h"

#include <stddef.h>

#include <Types.h>

#warning "This file should not be used"

typedef struct FreeMemoryStats FreeMemoryStats;

struct FreeMemoryStats {
	uint32	totalHeapSize;
	uint32	totalFreeBytes;
	uint32	maxBlockSize;
};

typedef void (*MallocHeapLowWarnProc)(void);

NSPR_BEGIN_EXTERN_C

typedef unsigned char (*MemoryCacheFlusherProc)(size_t size);
typedef void (*PreAllocationHookProc)(void);

extern void InstallPreAllocationHook(PreAllocationHookProc newHook);
extern void InstallMemoryCacheFlusher(MemoryCacheFlusherProc newFlusher);

// Entry into the memory system's cache flushing
extern UInt8 CallCacheFlushers(size_t blockSize);

extern void* reallocSmaller(void* block, size_t newSize);

void				memtotal ( size_t blockSize, FreeMemoryStats * stats );

size_t				memsize ( void * block );

extern Boolean		gMemoryInitialized;

void 				MacintoshInitializeMemory(void);
void				CallFE_LowMemory(void);
Boolean 			Memory_ReserveInMacHeap(size_t spaceNeeded);
Boolean 			Memory_ReserveInMallocHeap(size_t spaceNeeded);
Boolean 			InMemory_ReserveInMacHeap();
size_t 				Memory_FreeMemoryRemaining();
void 				InstallGarbageCollectorCacheFlusher(const MemoryCacheFlusherProc inFlusher);
void				InstallMallocHeapLowProc( MallocHeapLowWarnProc proc );

NSPR_END_EXTERN_C

#endif
