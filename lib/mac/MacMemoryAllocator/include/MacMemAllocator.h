/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
