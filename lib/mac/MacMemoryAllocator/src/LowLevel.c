/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#pragma error "This file is obsolete, but remains for reference reasons"

#include <Memory.h>
#include <Processes.h>

#include "stdlib.h"

#include "TypesAndSwitches.h"
#include "MacMemAllocator.h"

#include "prlog.h"

#if STATS_MAC_MEMORY
#include "prglobal.h"
#include "prfile.h"

#include <unistd.h>
#include <string.h>
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

#define	kMinLargeBlockHeapSize			(65 * 1024)		/* Min size of the heaps in temp mem try to allocate */
#define	kMaxLargeBlockHeapSize			(1024 * 1024)	/* Max size of the heaps in temp mem try to allocate */
#define	kLargeBlockInitialPercentage	(65)			/* This is the % of the free space in the heap used for the
															large block allocation heap */
															
#define	kMallocLowTempMemoryBoundary	(256 * 1024)	/* This is a boundary point at which we assume we're
															running low on temp memory */

#define kHeapEmergencyReserve			(64 * 1024)		/* Keep 64k free in the app heap for emergencies */


#if GENERATINGCFM
RoutineDescriptor 	gMallocGrowZoneProcRD = BUILD_ROUTINE_DESCRIPTOR(uppGrowZoneProcInfo, &MallocGrowZoneProc);
#else
#define gMallocGrowZoneProcRD MallocGrowZoneProc
#endif

//##############################################################################
//##############################################################################
#pragma mark FIXED-SIZE ALLOCATION DECLARATIONS

/* The format here is:

	DeclareFixedSizeAllocator(blockSize lower bound, # blocks in a heap chunk, # blocks in a tempmem chunk);

	DeclareSmallHeapAllocator(first chunk heap size, temp mem chunk heap size );
	
*/

// the real declarators
DeclareFixedSizeAllocator(4, 2600, 1000);
DeclareFixedSizeAllocator(8, 2000, 1000);
DeclareFixedSizeAllocator(12, 5000, 1000);
DeclareFixedSizeAllocator(16, 4000, 1000);
DeclareFixedSizeAllocator(20, 7000, 3000);
DeclareFixedSizeAllocator(24, 9500, 3000);
DeclareFixedSizeAllocator(28, 3200, 1000);
DeclareFixedSizeAllocator(32, 3400, 1000);
DeclareFixedSizeAllocator(36, 2500, 500);
DeclareFixedSizeAllocator(40, 9300, 5000);
DeclareSmallHeapAllocator(512 * 1024, 256 * 1024);
DeclareLargeBlockAllocator(kLargeBlockInitialPercentage, kMaxLargeBlockHeapSize, kMinLargeBlockHeapSize);

AllocMemoryBlockDescriptor gFastMemSmallSizeAllocators[] = {
	DeclareLargeBlockHeapDescriptor(),			//	0
	DeclareFixedBlockHeapDescriptor(4),			//	4
	DeclareFixedBlockHeapDescriptor(8),			//	8
	DeclareFixedBlockHeapDescriptor(12),		//	12
	DeclareFixedBlockHeapDescriptor(16),		//	16
	DeclareFixedBlockHeapDescriptor(20),		//	20
	DeclareFixedBlockHeapDescriptor(24),		//	24
	DeclareFixedBlockHeapDescriptor(28),		//	28
	DeclareFixedBlockHeapDescriptor(32),		//	32
	DeclareFixedBlockHeapDescriptor(36),		//	36
	DeclareFixedBlockHeapDescriptor(40),		//	40
	DeclareSmallSmallHeapDescriptor(),			//	44
	DeclareSmallSmallHeapDescriptor(),			//	48
	DeclareSmallSmallHeapDescriptor(),			//	52
	DeclareSmallSmallHeapDescriptor(),			//	56
	DeclareSmallSmallHeapDescriptor(),			//	60
	DeclareSmallSmallHeapDescriptor(),			//	64
	DeclareSmallSmallHeapDescriptor(),			//	68
	DeclareSmallSmallHeapDescriptor(),			//	72
	DeclareSmallSmallHeapDescriptor(),			//	76
	DeclareSmallSmallHeapDescriptor(),			//	80
	DeclareSmallSmallHeapDescriptor(),			//	84
	DeclareSmallSmallHeapDescriptor(),			//	88
	DeclareSmallSmallHeapDescriptor(),			//	92
	DeclareSmallSmallHeapDescriptor(),			//	96
	DeclareSmallSmallHeapDescriptor(),			//	100
	DeclareSmallSmallHeapDescriptor(),			//	104
	DeclareSmallSmallHeapDescriptor(),			//	108
	DeclareSmallSmallHeapDescriptor(),			//	112
	DeclareSmallSmallHeapDescriptor(),			//	116
	DeclareSmallSmallHeapDescriptor(),			//	120
	DeclareSmallSmallHeapDescriptor(),			//	124
	DeclareSmallSmallHeapDescriptor(),			//	128
	DeclareSmallSmallHeapDescriptor(),			//	132
	DeclareSmallSmallHeapDescriptor(),			//	136
	DeclareSmallSmallHeapDescriptor(),			//	140
	DeclareSmallSmallHeapDescriptor(),			//	144
	DeclareSmallSmallHeapDescriptor(),			//	148
	DeclareSmallSmallHeapDescriptor(),			//	152
	DeclareSmallSmallHeapDescriptor(),			//	156
	DeclareSmallSmallHeapDescriptor(),			//	160
	DeclareSmallSmallHeapDescriptor(),			//	164
	DeclareSmallSmallHeapDescriptor(),			//	168
	DeclareSmallSmallHeapDescriptor(),			//	172
	DeclareSmallSmallHeapDescriptor(),			//	176
	DeclareSmallSmallHeapDescriptor(),			//	180
	DeclareSmallSmallHeapDescriptor(),			//	184
	DeclareSmallSmallHeapDescriptor(),			//	188
	DeclareSmallSmallHeapDescriptor(),			//	192
	DeclareSmallSmallHeapDescriptor(),			//	196
	DeclareSmallSmallHeapDescriptor(),			//	200
	DeclareSmallSmallHeapDescriptor(),			//	204
	DeclareSmallSmallHeapDescriptor(),			//	208
	DeclareSmallSmallHeapDescriptor(),			//	212
	DeclareSmallSmallHeapDescriptor(),			//	216
	DeclareSmallSmallHeapDescriptor(),			//	220
	DeclareSmallSmallHeapDescriptor(),			//	224
	DeclareSmallSmallHeapDescriptor(),			//	228
	DeclareSmallSmallHeapDescriptor(),			//	232
	DeclareSmallSmallHeapDescriptor(),			//	236
	DeclareSmallSmallHeapDescriptor(),			//	240
	DeclareSmallSmallHeapDescriptor(),			//	244
	DeclareSmallSmallHeapDescriptor(),			//	248
	DeclareSmallSmallHeapDescriptor(),			//	252
	DeclareSmallSmallHeapDescriptor(),			//	256
};


//##############################################################################
//##############################################################################
#pragma mark-
#pragma mark INSTRUMENTATION

#if INSTRUMENT_MAC_MEMORY


InstHistogramClassRef		gSmallHeapHistogram = 0;
InstHistogramClassRef		gLargeHeapHistogram = 0;


static OSErr CreateInstrumentationClasses()
{
	OSErr	err = noErr;

	err = InstCreateHistogramClass(kInstRootClassRef, "MacMemAllocator:SmallHeap:Histogram",
			 40, 256, 4, kInstEnableClassMask, &gSmallHeapHistogram);
	if (err != noErr) return err;
	
	err = InstCreateHistogramClass(kInstRootClassRef, "MacMemAllocator:LargeHeap:Histogram",
			 256, 10240, 64, kInstEnableClassMask, &gLargeHeapHistogram);
	if (err != noErr) return err;
	
	return err;
}


#endif

//##############################################################################
//##############################################################################
#pragma mark-
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

#if !TARGET_CARBON 
	MaxApplZone();
#endif

	for (i = 1; i <= 30; i++)
		MoreMasters();

	// init our new compact allocators
	
	processInfo.processInfoLength = sizeof(processInfo);
	processInfo.processName = NULL;
	processInfo.processAppSpec = NULL;

	GetProcessInformation(&thisProcess, &processInfo);
	gOurApplicationHeapBase = processInfo.processLocation;
	gOurApplicationHeapMax = (Ptr)gOurApplicationHeapBase + processInfo.processSize;

#if INSTRUMENT_MAC_MEMORY
	if (CreateInstrumentationClasses() != noErr)
	{
		DebugStr("\pFailed to initialize instrumentation classes");
		ExitToShell();
	}
#endif

#if DEBUG_MAC_MEMORY || STATS_MAC_MEMORY
	InstallMemoryManagerPatches();

#if DEBUG_MAC_MEMORY
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

#ifdef MALLOC_IS_NEWPTR
	/* Don't allocate memory pools if just using NewPtr */
	return;
#endif

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

#if STATS_MAC_MEMORY

/* I hate copy and paste */
static void WriteString ( PRFileHandle file, const char * string )
{
	long	len;
	long	bytesWritten;
	
	len = strlen( string );
	if ( len >= 1024 ) Debugger();
	bytesWritten = _OS_WRITE ( file, string, len );
	PR_ASSERT(bytesWritten == len);
}

static void WriteFixedAllocatorStats(PRFileHandle outFile, const FixedSizeAllocationRoot *root)
{
	char		outString[ 1024 ];
	
	WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
	
	sprintf(outString, "Stats for fixed size allocator for blocks of %d-%d bytes\n", root->blockSize - 3, root->blockSize);
	WriteString ( outFile, outString );
	
	WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
	
	WriteString ( outFile, "                     Current         Max\n" );
	WriteString ( outFile, "                  ----------     -------\n" );
	
	sprintf( outString,    "Num chunks:       %10d  %10d\n", root->chunksAllocated, root->maxChunksAllocated );
	WriteString ( outFile, outString );
	
	sprintf( outString,    "Chunk total:      %10d  %10d\n", root->totalChunkSize, root->maxTotalChunkSize );
	WriteString ( outFile, outString );
	
	sprintf( outString,    "Num blocks:       %10d  %10d\n", root->blocksAllocated, root->maxBlocksAllocated );
	WriteString ( outFile, outString );

	sprintf( outString,    "Block space:      %10d  %10d\n", root->blockSpaceUsed, root->maxBlockSpaceUsed );
	WriteString ( outFile, outString );
	
	sprintf( outString,    "Blocks used:      %10d  %10d\n", root->blocksUsed, root->maxBlocksUsed );
	WriteString ( outFile, outString );
		
	WriteString ( outFile, "                                 -------\n" );
	
	sprintf( outString,    "%s of allocated blocks used:   %10.2f\n", "%", 100.0 * root->maxBlocksUsed / root->maxBlocksAllocated );
	WriteString ( outFile, outString );

	sprintf( outString,    "%s of chunk space used:        %10.2f\n", "%", 100.0 * root->maxBlockSpaceUsed / root->maxTotalChunkSize );
	WriteString ( outFile, outString );
	
	WriteString ( outFile, "\n\n");
}

void DumpAllocatorMemoryStats(PRFileHandle outFile);

void DumpAllocatorMemoryStats(PRFileHandle outFile)
{
	/* fixed size allocators */
	WriteFixedAllocatorStats( outFile, &gFixedSize4Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize8Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize12Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize16Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize20Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize24Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize32Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize36Root );
	WriteFixedAllocatorStats( outFile, &gFixedSize40Root );

/*	
	chunk = SmallHeapAllocChunk ( 0, &gSmallHeapRoot );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;
	
	chunk = LargeBlockAllocChunk ( 0, &gLargeBlockRoot );
	PR_ASSERT(chunk);
	if ( chunk == NULL ) goto fail;
*/

}

#endif /* STATS_MAC_MEMORY */


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

/*	CallCacheFlushers is only called under extreme conditions now, when an attempt to
	allocate a new sub-heap has failed.

	The flush procs called here are set up in fredmem.cp:

	InstallMemoryCacheFlusher(&ImageCacheMemoryFlusher);
	InstallMemoryCacheFlusher(&NetlibCacheMemoryFlusher);
	InstallMemoryCacheFlusher(&LayoutCacheMemoryFlusher);
	InstallMemoryCacheFlusher(&LibNeoCacheMemoryFlusher);

 */
UInt8 CallCacheFlushers(size_t blockSize)
{
	MemoryCacheFlusherProcRec		*currentCacheFlusher = gFirstFlusher;
	UInt8							result = false;

	// we might want to remember which flusher was called last and start
	// at the one after, to avoid always flushing the first one (image cache)
	// first. But since this is a last-ditch effort to free memory, that's
	// probably not worth it.
	
	while (currentCacheFlusher != NULL)
	{
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
		
		if ( tempHandle == NULL ) 
		{
			// failed to make temp handle. Let's try a handle in our heap
			tempHandle = NewHandle( heapSize );
			
			// ensure that enough free mem is available for emergencies
			if ( tempHandle != NULL && MaxBlock() < kHeapEmergencyReserve)
			{
				// we need to keep some contiguous space free for emergencies. Give up.
				DisposeHandle(tempHandle);
				tempHandle = NULL;
			}
		}
		
		if ( tempHandle != NULL && err == noErr )
		{
			HLockHi ( tempHandle );				// lock the handle hi now to reduce system heap fragmentation
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
		heapBlock->refCon = tempHandle;			// so we can dispose the handle when we're done with the subheap
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



