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

#include <Types.h>
#include <Memory.h>
#include <stdlib.h>

#pragma error "This file is obsolete, but remains for reference reasons"

#include "MacMemAllocator.h"

/*	You probably only want to use this for debugging. It's useful to see leaks
	with ZoneRanger.
*/
/* #define MALLOC_IS_NEWPTR		1	*/

/*	Turn this on to check for block overwrites, heap corruption, bad frees, etc */ 
#define	DEBUG_HEAP_INTEGRITY	DEBUG

/*	Turn this on to track memory allocations. If you do this, you will have to
	allocate a lot more memory to the app (symptoms of too little are bogus
	shared library errors on startup)
*/
#define DEBUG_MAC_MEMORY		0

/* This setting is really obsoleted by STATS_MAC_MEMORY, which gives better stats */ 
#define	TRACK_EACH_ALLOCATOR	0

/*	Turn this on to track amount of memory allocated and performance of the allocators
	If you turn this on, it's better to not define DEBUG_HEAP_INTEGRITY, because
	that setting causes each allocation to be 8-12 bytes larger, which messes
	up the stats. I've been careful with STATS_MAC_MEMORY to not have it affect
	block size at all.
	
	STATS_MAC_MEMORY can be turned on independently of the other debugging settings.
	It will cause the creation of a text file, "MemoryStats.txt", in the app
	directory, on quit. Its overhead is very low.
*/
#define STATS_MAC_MEMORY		0

/*	Turn this on to use Apple's Instrumentation Lib to instrument block allocations
	Currently, it collects histogram data on the frequency of allocations of
	different sizes for the small and large allocators.
*/
/* #define INSTRUMENT_MAC_MEMORY	1 */

#if INSTRUMENT_MAC_MEMORY
#include "Instrumentation.h"
#include "InstrumentationMacros.h"
#endif

#if DEBUG_MAC_MEMORY
#include "xp_tracker.h"
#endif

//##############################################################################
//##############################################################################


typedef struct SubHeapAllocationRoot SubHeapAllocationRoot;
typedef struct SubHeapAllocationChunk SubHeapAllocationChunk;

enum {
	kMaxTableAllocatedBlockSize		= 256
};

typedef void *(* AllocMemoryBlockProcPtr)(size_t blockSize, void *refcon);
typedef void (* FreeMemoryBlockProcPtr)(void *freeBlock, void *refcon);
typedef size_t (* BlockSizeProcPtr)(void *freeBlock, void *refcon);
typedef void (* HeapFreeSpaceProcPtr)(size_t blockSize, FreeMemoryStats * stats, void *refcon);
typedef SubHeapAllocationChunk * (* AllocMemoryChunkProcPtr)(size_t blockSize, void *refcon);

struct AllocMemoryBlockDescriptor {
	AllocMemoryBlockProcPtr		blockAllocRoutine;
	AllocMemoryChunkProcPtr		chunkAllocRoutine;
	HeapFreeSpaceProcPtr		heapFreeSpaceRoutine;
	SubHeapAllocationRoot		*root;		
};

typedef struct AllocMemoryBlockDescriptor AllocMemoryBlockDescriptor;

struct FreeMemoryBlockDescriptor {
	FreeMemoryBlockProcPtr		freeRoutine;
	BlockSizeProcPtr			sizeRoutine;
	void						*refcon;		
};

typedef struct FreeMemoryBlockDescriptor FreeMemoryBlockDescriptor;

//##############################################################################
//##############################################################################

#if DEBUG_HEAP_INTEGRITY
typedef	UInt32 MemoryBlockTag;
#endif

typedef struct MemoryBlockHeader MemoryBlockHeader;

struct MemoryBlockHeader {
	FreeMemoryBlockDescriptor		*blockFreeRoutine;
#if DEBUG_HEAP_INTEGRITY	// || STATS_MAC_MEMORY -- see comment in DumpMemoryStats()
	size_t							blockSize;
#endif
#if DEBUG_HEAP_INTEGRITY
	MemoryBlockTag					headerTag;
	MemoryBlockHeader				*next;
	MemoryBlockHeader				*prev;
#endif
};

#if DEBUG_HEAP_INTEGRITY
typedef struct MemoryBlockTrailer MemoryBlockTrailer; 

struct MemoryBlockTrailer {
	MemoryBlockTag		trailerTag;
};
#define MEMORY_BLOCK_TAILER_SIZE sizeof(struct MemoryBlockTrailer)
#else
#define MEMORY_BLOCK_TAILER_SIZE 0
#endif


//##############################################################################
//##############################################################################
#pragma mark SUB HEAP ALLOCATOR

struct SubHeapAllocationRoot {
	SubHeapAllocationChunk *	firstChunk;
	SubHeapAllocationChunk *	lastChunk;
#if DEBUG_MAC_MEMORY && TRACK_EACH_ALLOCATOR
	AllocationSet *				set;
#endif
};

struct SubHeapAllocationChunk {
	SubHeapAllocationRoot *		root;
	SubHeapAllocationChunk *	next;
	void *						refCon;
	SInt32						usedBlocks;
	UInt32						heapSize;
	FreeMemoryBlockDescriptor	freeDescriptor;
};

// The following two entry points for sub heap allocations
extern SubHeapAllocationChunk * AllocateSubHeap ( SubHeapAllocationRoot * root, Size heapSize, Boolean useTemp );
extern void FreeSubHeap ( SubHeapAllocationRoot * root, SubHeapAllocationChunk * chunk );

#if DEBUG_MAC_MEMORY && TRACK_EACH_ALLOCATOR
#define	DECLARE_SUBHEAP_ROOT()	{ NULL, NULL, NULL }
#else
#define	DECLARE_SUBHEAP_ROOT()	{ NULL, NULL }
#endif

//##############################################################################
//##############################################################################
#pragma mark LARGE BLOCK ALLOCATOR

typedef struct LargeBlockHeader LargeBlockHeader;

struct LargeBlockHeader {
	LargeBlockHeader *		next;
	LargeBlockHeader *		prev;
	size_t					logicalSize;
	MemoryBlockHeader		header;
};

typedef struct LargeBlockAllocationChunk LargeBlockAllocationChunk;

typedef struct LargeBlockAllocationRoot LargeBlockAllocationRoot;

struct LargeBlockAllocationChunk {
	SubHeapAllocationChunk	header;
	UInt32					totalFree;
	LargeBlockHeader *		tail;
	LargeBlockHeader		head[];
};

struct LargeBlockAllocationRoot {
	SubHeapAllocationRoot	header;
	UInt32					baseChunkPercentage;
	UInt32					idealTempChunkSize;
	UInt32					smallestTempChunkSize;
};

void *LargeBlockAlloc(size_t blockSize, void *refcon);
void LargeBlockFree(void *block, void *refcon);
SubHeapAllocationChunk * LargeBlockAllocChunk ( size_t blockSize, void *refcon );
size_t LargeBlockSize (void *freeBlock, void *refcon);
void LargeBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon);

extern LargeBlockAllocationRoot	gLargeBlockRoot;

#define DeclareLargeBlockAllocator(basePercentage,idealTmpSize,smallestTmpSize) \
LargeBlockAllocationRoot	gLargeBlockRoot = {  DECLARE_SUBHEAP_ROOT(), basePercentage, idealTmpSize, smallestTmpSize }

#define	DeclareLargeBlockHeapDescriptor()	\
	{	&LargeBlockAlloc, &LargeBlockAllocChunk, &LargeBlockHeapFree, (SubHeapAllocationRoot *)&gLargeBlockRoot }

//##############################################################################
//##############################################################################
#pragma mark FIXED SIZED ALLOCATOR DEFINITIONS

typedef struct FixedMemoryFreeBlockHeader FixedMemoryFreeBlockHeader;

struct FixedMemoryFreeBlockHeader {
	MemoryBlockHeader				header;
	FixedMemoryFreeBlockHeader *	next;
};

typedef struct FixedSizeAllocationChunk FixedSizeAllocationChunk;

typedef struct FixedSizeAllocationRoot FixedSizeAllocationRoot;

struct FixedSizeAllocationChunk {
	SubHeapAllocationChunk			header;
#if STATS_MAC_MEMORY
	UInt32							chunkSize;
	UInt32							numBlocks;
#endif
	FixedMemoryFreeBlockHeader *	freeList;
	FixedMemoryFreeBlockHeader		memory[];
};

struct FixedSizeAllocationRoot {
	SubHeapAllocationRoot	header;
	UInt32					baseChunkBlockCount;
	UInt32					tempChunkBlockCount;
	UInt32					blockSize;
#if STATS_MAC_MEMORY
	UInt32					chunksAllocated;
	UInt32					maxChunksAllocated;
	
	UInt32					totalChunkSize;
	UInt32					maxTotalChunkSize;

	UInt32					blocksAllocated;
	UInt32					maxBlocksAllocated;
	
	UInt32					blocksUsed;
	UInt32					maxBlocksUsed;
	
	UInt32					blockSpaceUsed;
	UInt32					maxBlockSpaceUsed;
#endif
};

void *FixedSizeAlloc(size_t blockSize, void *refcon);
void FixedSizeFree(void *block, void *refcon);
SubHeapAllocationChunk * FixedSizeAllocChunk ( size_t blockSize, void *refcon );
size_t FixedBlockSize (void *freeBlock, void *refcon);
void FixedBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon);

/* When STATS_MAC_MEMORY, the extra fields in the FixedSizeAllocationRoot will be zeroed on initialization */
#define DeclareFixedSizeAllocator(blockSize,baseCount,tempCount) \
extern FixedSizeAllocationRoot	gFixedSize##blockSize##Root;	\
FixedSizeAllocationRoot	gFixedSize##blockSize##Root = {	DECLARE_SUBHEAP_ROOT(), baseCount, tempCount, blockSize }

#define	DeclareFixedBlockHeapDescriptor(blockSize)	\
	{	&FixedSizeAlloc, &FixedSizeAllocChunk, &FixedBlockHeapFree, (SubHeapAllocationRoot *)&gFixedSize##blockSize##Root }

//##############################################################################
//##############################################################################
#pragma mark SMALL HEAP ALLOCATOR DEFINITIONS

typedef struct SmallHeapBlock SmallHeapBlock;

struct SmallHeapBlock {
	SmallHeapBlock			*prevBlock;
	UInt32					blockSize;
	union {
		struct {
			SmallHeapBlock	*nextFree;
			SmallHeapBlock	*prevFree;
		}					freeInfo;
		struct {
			UInt32			filler;
			MemoryBlockHeader freeProc;	
		}					inUseInfo;
	}						info;
};

enum {
	kBlockInUseFlag				= 0x80000000,
	kDefaultSmallHeadMinSize	= 4L,
	kDefaultSmallHeapBins 		= 64L,
	kMaximumBinBlockSize		= kDefaultSmallHeadMinSize + 4L * kDefaultSmallHeapBins - 1
};

typedef struct SmallHeapChunk SmallHeapChunk;

typedef struct SmallHeapRoot SmallHeapRoot;

struct SmallHeapRoot {
	SubHeapAllocationRoot		header;
	UInt32						baseChunkHeapSize;
	UInt32						tempChunkHeapSize;
};

struct SmallHeapChunk {
	SubHeapAllocationChunk	header;
	SmallHeapChunk *		nextChunk;
	SmallHeapBlock *		overflow;
	SmallHeapBlock *		bins[kDefaultSmallHeapBins];
	SmallHeapBlock			memory[];
};

#define DeclareSmallHeapAllocator(baseSize, tempSize) \
extern SmallHeapRoot	gSmallHeapRoot; \
SmallHeapRoot	gSmallHeapRoot = {  DECLARE_SUBHEAP_ROOT(), baseSize, tempSize }

#define	DeclareSmallSmallHeapDescriptor()	\
	{	&SmallHeapAlloc, &SmallHeapAllocChunk, &SmallBlockHeapFree, (SubHeapAllocationRoot *)&gSmallHeapRoot }
	
void *SmallHeapAlloc(size_t blockSize, void *refcon);
void SmallHeapFree(void *address, void *refcon);
SubHeapAllocationChunk * SmallHeapAllocChunk ( size_t blockSize, void *refcon );
size_t SmallBlockSize (void *freeBlock, void *refcon);
void SmallBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon);

//##############################################################################
//##############################################################################
#pragma mark LOW LEVEL ALLOCATORS

// Client Provides a table to small block allocators
extern AllocMemoryBlockDescriptor gFastMemSmallSizeAllocators[];

//	VerifyMallocHeapIntegrity will report any block headers or
//	trailers that have been overwritten.
extern void InstallMemoryManagerPatches();
extern void VerifyMallocHeapIntegrity();
static void TagReferencedBlocks();
extern void DumpMemoryStats();

//##############################################################################
//##############################################################################

#ifdef DEBUG_HEAP_INTEGRITY

extern UInt32						gVerifyHeapOnEveryMalloc;
extern UInt32						gVerifyHeapOnEveryFree;
extern UInt32						gFillUsedBlocksWithPattern;
extern UInt32						gFillFreeBlocksWithPattern;
extern UInt32						gDontActuallyFreeMemory;
extern SInt32						gFailToAllocateAfterNMallocs;

extern SInt32						gTagReferencedBlocks;
#endif

#if INSTRUMENT_MAC_MEMORY
extern InstHistogramClassRef		gSmallHeapHistogram;
extern InstHistogramClassRef		gLargeHeapHistogram;
#endif

