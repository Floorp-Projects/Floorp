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

#include <Memory.h>
#include <Processes.h>

#include "stdlib.h"

#include "TypesAndSwitches.h"
#include "MacMemAllocator.h"

#ifndef NSPR20
#include "prglobal.h"
#include "prmacos.h"
#include "prthread.h"
#include "prgc.h"
#include "swkern.h"
#else
#include "prlog.h"
#endif

#if DEBUG_MAC_MEMORY
#include "MemoryTracker.h"


extern AllocationSet *	gFixedSizeAllocatorSet;
extern AllocationSet *	gSmallHeapAllocatorSet;
extern AllocationSet *	gLargeBlockAllocatorSet;
#endif

void	*		gOurApplicationHeapBase;
void	*		gOurApplicationHeapMax;

Boolean			gMemoryInitialized = false;

//##############################################################################
//##############################################################################
#pragma mark DECLARATIONS AND ENUMERATIONS

typedef struct MemoryCacheFlusherProcRec MemoryCacheFlusherProcRec;

struct MemoryCacheFlusherProcRec {
	MemoryCacheFlusherProc			flushProc;
	MemoryCacheFlusherProcRec		*next;
};

typedef struct PreAllocationProcRec PreAllocationProcRec;

struct PreAllocationProcRec {
	PreAllocationHookProc			preAllocProc;
	PreAllocationProcRec			*next;
};

MemoryCacheFlusherProcRec	*gFirstFlusher = NULL;
PreAllocationProcRec		*gFirstPreAllocator = NULL;
MallocHeapLowWarnProc		gMallocLowProc = NULL;

void CallPreAllocators(void);
void InitializeSubAllocators ( void );
Boolean ReclaimMemory(size_t amountNeeded);
long pascal MallocGrowZoneProc(Size cbNeeded);

#define	kMinLargeBlockHeapSize			(65 * 1024)
#define	kMaxLargeBlockHeapSize			(1024 * 1024)
#define	kLargeBlockInitialPercentage	(50)

// this is a boundary point at which we assume we're running low on temp memory
#define	kMallocLowTempMemoryBoundary	(256 * 1024)

#if GENERATINGCFM
RoutineDescriptor 	gMallocGrowZoneProcRD = BUILD_ROUTINE_DESCRIPTOR(uppGrowZoneProcInfo, &MallocGrowZoneProc);
#else
#define gMallocGrowZoneProcRD MallocGrowZoneProc
#endif

//##############################################################################
//##############################################################################
#pragma mark FIXED-SIZE ALLOCATION DECLARATIONS

// the real declarators
DeclareFixedSizeAllocator(4, 2600, 1000);
DeclareFixedSizeAllocator(8, 2000, 1000);
DeclareFixedSizeAllocator(12, 3200, 1000);
DeclareFixedSizeAllocator(16, 2000, 1000);
DeclareFixedSizeAllocator(20, 7000, 3000);
DeclareFixedSizeAllocator(24, 9500, 3000);
DeclareFixedSizeAllocator(28, 3200, 1000);
DeclareFixedSizeAllocator(32, 3400, 1000);
DeclareFixedSizeAllocator(36, 2500, 500);
DeclareFixedSizeAllocator(40, 9300, 5000);
DeclareSmallHeapAllocator(512 * 1024, 256 * 1024);
DeclareLargeBlockAllocator(kLargeBlockInitialPercentage, kMaxLargeBlockHeapSize, kMinLargeBlockHeapSize);

AllocMemoryBlockDescriptor gFastMemSmallSizeAllocators[] = {
	DeclareLargeBlockHeapDescriptor(),
	DeclareFixedBlockHeapDescriptor(4),
	DeclareFixedBlockHeapDescriptor(8),
	DeclareFixedBlockHeapDescriptor(12),
	DeclareFixedBlockHeapDescriptor(16),
	DeclareFixedBlockHeapDescriptor(20),
	DeclareFixedBlockHeapDescriptor(24),
	DeclareFixedBlockHeapDescriptor(28),
	DeclareFixedBlockHeapDescriptor(32),
	DeclareFixedBlockHeapDescriptor(36),
	DeclareFixedBlockHeapDescriptor(40),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor(),
	DeclareSmallSmallHeapDescriptor()
};

//##############################################################################
//##############################################################################
#pragma mark INITIALIZATION

void MacintoshInitializeMemory(void)
{
	UInt32				i;
	ProcessSerialNumber	thisProcess = { 0, kCurrentProcess };
	ProcessInfoRec		processInfo;

// Increase the stack space.
// This is because PA_MDL_ParseTag can go into deep recursion when dealing with 
// malformed HTML comments (the only occurrence where we bombed).
#ifndef powerc
	SetApplLimit(GetApplLimit() - 16384);	
#endif
	MaxApplZone();
	for (i = 1; i <= 30; i++)
		MoreMasters();

	// init our new compact allocators
	
	processInfo.processInfoLength = sizeof(processInfo);
	processInfo.processName = NULL;
	processInfo.processAppSpec = NULL;

	GetProcessInformation(&thisProcess, &processInfo);
	gOurApplicationHeapBase = processInfo.processLocation;
	gOurApplicationHeapMax = (Ptr)gOurApplicationHeapBase + processInfo.processSize;
	
#if DEBUG_MAC_MEMORY
	InstallMemoryManagerPatches();

#if 1
	// Create some allocation sets to track our allocators
	gFixedSizeAllocatorSet = NewAllocationSet ( 0, "Fixed Size Compact Allocator" );
	gSmallHeapAllocatorSet = NewAllocationSet ( 0, "Small Heap Allocator" );
	gLargeBlockAllocatorSet = NewAllocationSet ( 0, "Large Block Allocator" );
	 
	// disable them so we don't get random garbage
	DisableAllocationSet ( gFixedSizeAllocatorSet );
	DisableAllocationSet ( gSmallHeapAllocatorSet );
	DisableAllocationSet ( gLargeBlockAllocatorSet );
#endif

#endif
	
	// intialize the sub allocators
	InitializeSubAllocators();
	
	gMemoryInitialized = true;
}

void InitializeSubAllocators ( void )
{
	SubHeapAllocationChunk *	chunk;
	
	/* fixed size allocators */
	chunk = FixedSizeAllocChunk ( 0, &gFixedSize4Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;
	
	chunk = FixedSizeAllocChunk ( 0, &gFixedSize8Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize12Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize16Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize20Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize24Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize28Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize32Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize36Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

	chunk = FixedSizeAllocChunk ( 0, &gFixedSize40Root );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;
	
	chunk = SmallHeapAllocChunk ( 0, &gSmallHeapRoot );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;
	
	chunk = LargeBlockAllocChunk ( 0, &gLargeBlockRoot );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;

#if DEBUG_MAC_MEMORY && TRACK_EACH_ALLOCATOR
	gFixedSize4Root.header.set = NewAllocationSet ( 0, "Fixed Block 4" );
	DisableAllocationSet ( gFixedSize4Root.header.set );
	
	gFixedSize8Root.header.set = NewAllocationSet ( 0, "Fixed Block 8" );
	DisableAllocationSet ( gFixedSize8Root.header.set );
	
	gFixedSize12Root.header.set = NewAllocationSet ( 0, "Fixed Block 12" );
	DisableAllocationSet ( gFixedSize12Root.header.set );
	
	gFixedSize16Root.header.set = NewAllocationSet ( 0, "Fixed Block 16" );
	DisableAllocationSet ( gFixedSize16Root.header.set );
	
	gFixedSize20Root.header.set = NewAllocationSet ( 0, "Fixed Block 20" );
	DisableAllocationSet ( gFixedSize20Root.header.set );
	
	gFixedSize24Root.header.set = NewAllocationSet ( 0, "Fixed Block 24" );
	DisableAllocationSet ( gFixedSize24Root.header.set );
	
	gFixedSize28Root.header.set = NewAllocationSet ( 0, "Fixed Block 28" );
	DisableAllocationSet ( gFixedSize28Root.header.set );
	
	gFixedSize32Root.header.set = NewAllocationSet ( 0, "Fixed Block 32" );
	DisableAllocationSet ( gFixedSize32Root.header.set );
	
	gFixedSize36Root.header.set = NewAllocationSet ( 0, "Fixed Block 36" );
	DisableAllocationSet ( gFixedSize36Root.header.set );
	
	gFixedSize40Root.header.set = NewAllocationSet ( 0, "Fixed Block 40" );
	DisableAllocationSet ( gFixedSize40Root.header.set );
	
	gSmallHeapRoot.header.set = NewAllocationSet ( 0, "Small Block" );
	DisableAllocationSet ( gSmallHeapRoot.header.set );
	
	gLargeBlockRoot.header.set = NewAllocationSet ( 0, "Large Block" );
	DisableAllocationSet ( gLargeBlockRoot.header.set );
#endif

	return;
	
fail:
	/* We couldn't initialize one of the sub allocators, so we're screwed */
	/* I don't think we need an alert here as we should never hit this case unless */
	/* a user really mucks up our heap partition */
	ExitToShell();
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark INSTALLING MEMORY MANAGER HOOKS

void InstallPreAllocationHook(PreAllocationHookProc newHook)
{
	PreAllocationProcRec			*preAllocatorRec;

	preAllocatorRec = (PreAllocationProcRec *)(NewPtr(sizeof(PreAllocationProcRec)));

	if (preAllocatorRec != NULL) {
	
		preAllocatorRec->next = gFirstPreAllocator;
		preAllocatorRec->preAllocProc = newHook;
		gFirstPreAllocator = preAllocatorRec;		
	
	}

}

void InstallMemoryCacheFlusher(MemoryCacheFlusherProc newFlusher)
{	
	MemoryCacheFlusherProcRec		*previousFlusherRec = NULL;
	MemoryCacheFlusherProcRec		*cacheFlusherRec = gFirstFlusher;
	
	while (cacheFlusherRec != NULL) {
		previousFlusherRec = cacheFlusherRec;
		cacheFlusherRec = cacheFlusherRec->next;
	}

	cacheFlusherRec =  (MemoryCacheFlusherProcRec *)NewPtrClear(sizeof(MemoryCacheFlusherProcRec));
	
	if (cacheFlusherRec == NULL)
		return;

	cacheFlusherRec->flushProc = newFlusher;

	if (previousFlusherRec != NULL) {
		previousFlusherRec->next = cacheFlusherRec;
	}
	
	else {
		gFirstFlusher = cacheFlusherRec;
	}
	
}

void CallPreAllocators(void)
{
	PreAllocationProcRec		*currentPreAllocator = gFirstPreAllocator;

	while (currentPreAllocator != NULL) {
		(*(currentPreAllocator->preAllocProc))();
		currentPreAllocator = currentPreAllocator->next;
	}
	
}

static MemoryCacheFlusherProc sGarbageCollectorCacheFlusher = NULL;

UInt8 CallCacheFlushers(size_t blockSize)
{
	MemoryCacheFlusherProcRec		*currentCacheFlusher = gFirstFlusher;
	UInt8							result = false;

	while (currentCacheFlusher != NULL) {
		result |= (*(currentCacheFlusher->flushProc))(blockSize);
		currentCacheFlusher = currentCacheFlusher->next;
	}
	
	//	We used to try calling the GC if malloc failed, but that's
	//  a waste of time since the GC never frees segments (bug?)
	//  and thus won't increase heap space.
		
	return result;
	
}

void InstallGarbageCollectorCacheFlusher(const MemoryCacheFlusherProc inFlusher)
{
	sGarbageCollectorCacheFlusher = inFlusher;
}

void InstallMallocHeapLowProc( MallocHeapLowWarnProc proc )
{
	gMallocLowProc = proc;
}

void CallFE_LowMemory(void)
{
	if ( gMallocLowProc != NULL )
		{
		gMallocLowProc();
		}
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark SUB HEAP ALLOCATION

SubHeapAllocationChunk * AllocateSubHeap ( SubHeapAllocationRoot * root, Size heapSize, Boolean useTemp )
{
	SubHeapAllocationChunk *	heapBlock;
	Handle						tempHandle;
	OSErr						err;
	
	heapBlock = NULL;
	tempHandle = NULL;
	
#if DEBUG_MAC_MEMORY
	DisableMemoryTracker();
#endif

	if ( useTemp )
		{
		tempHandle = TempNewHandle ( heapSize, &err );
		if ( tempHandle != NULL && err == noErr )
			{
			HLock ( tempHandle );
			heapBlock = *(SubHeapAllocationChunk **) tempHandle;
			}
		}
	else
		{
		heapBlock = (SubHeapAllocationChunk *) NewPtr ( heapSize );
		}
	
	if ( heapBlock != NULL )
		{
		heapBlock->root = root;
		heapBlock->refCon = tempHandle;
		heapBlock->next = NULL;
		heapBlock->usedBlocks = 0;
		heapBlock->freeDescriptor.freeRoutine = NULL;
		heapBlock->freeDescriptor.refcon = NULL;
		
		// whack this on the root's chunk list
		if ( root->lastChunk == NULL )
			{
			root->firstChunk = heapBlock;
			}
		else
			{
			root->lastChunk->next = heapBlock;
			}
		
		root->lastChunk = heapBlock;
		}
	
#if DEBUG_MAC_MEMORY
	EnableMemoryTracker();
#endif
	
	return heapBlock;
}


void FreeSubHeap ( SubHeapAllocationRoot * root, SubHeapAllocationChunk * chunk )
{
	Handle						tempHandle;
	SubHeapAllocationChunk *	list;
	SubHeapAllocationChunk *	prev;
	SubHeapAllocationChunk *	next;
	
	if ( chunk != NULL )
		{
		// run through the root's chunk list and remove our block
		prev = NULL;
		list = root->firstChunk;
		
		while ( list != NULL )
			{
			next = list->next;
			
			if ( list == chunk )
				{
				break;
				}
			
			prev = list;
			list = next;
			}
		
		if ( list != NULL )
			{
			if ( prev != NULL )
				{
				prev->next = next;
				}
			
			if ( root->firstChunk == list )
				{
				root->firstChunk = next;
				}
			
			if ( root->lastChunk == list )
				{
				root->lastChunk = prev;
				}
			}
			
		tempHandle = (Handle) chunk->refCon;
		if ( tempHandle != NULL )
			{
			DisposeHandle ( tempHandle );
			}
		else
			{
			DisposePtr ( (Ptr) chunk );
			}
		}
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark MEMORY UTILS

Boolean gInMemory_ReserveInMacHeap = false;

Boolean InMemory_ReserveInMacHeap()
{
	return gInMemory_ReserveInMacHeap;
}

Boolean ReclaimMemory(size_t amountNeeded)
{
	Boolean			result;
			
	result = CallCacheFlushers(amountNeeded);
		
	return result;

}

Boolean Memory_ReserveInMacHeap(size_t spaceNeeded)
{
	Boolean		result = true;
	
	gInMemory_ReserveInMacHeap = true;
	
	if (MaxBlock() < spaceNeeded)
		result = ReclaimMemory(spaceNeeded);
	
	gInMemory_ReserveInMacHeap = false;

	return result;
	
}

Boolean Memory_ReserveInMallocHeap(size_t spaceNeeded)
{
	Boolean		result = true;
	Size		freeMem;
	
	gInMemory_ReserveInMacHeap = true;
	
	freeMem = MaxBlock();
	
	if (freeMem < spaceNeeded)
		result = ReclaimMemory(spaceNeeded);

	gInMemory_ReserveInMacHeap = false;

	return result;
	
}

size_t Memory_FreeMemoryRemaining()
{
	size_t		mainHeap;
	size_t		mallocHeap;
	
	mainHeap = FreeMem();
	
	mallocHeap = FreeMem();
	
	return (mainHeap < mallocHeap) ? mainHeap : mallocHeap; 

}



