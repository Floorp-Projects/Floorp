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


#include <Types.h>
#include <Memory.h>
#include <stdlib.h>

#define DEBUG_MAC_MEMORY 0
#define STATS_MAC_MEMORY 0

//##############################################################################
//##############################################################################

enum {
	kMaxTableAllocatedBlockSize		= 256,
	kRecordingDepthOfStackLevels	= 3
};

typedef void *(* AllocMemoryBlockProcPtr)(size_t blockSize, void *refcon);
typedef void (* FreeMemoryBlockProcPtr)(void *freeBlock, void *refcon);

struct AllocMemoryBlockDescriptor {
	AllocMemoryBlockProcPtr		allocRoutine;
	void						*refcon;		
};

typedef struct AllocMemoryBlockDescriptor AllocMemoryBlockDescriptor;

struct FreeMemoryBlockDescriptor {
	FreeMemoryBlockProcPtr		freeRoutine;
	void						*refcon;		
};

typedef struct FreeMemoryBlockDescriptor FreeMemoryBlockDescriptor;

//##############################################################################
//##############################################################################

#if DEBUG_MAC_MEMORY
typedef	UInt32 MemoryBlockTag;
#endif

typedef struct MemoryBlockHeader MemoryBlockHeader;

struct MemoryBlockHeader {
	FreeMemoryBlockDescriptor		*blockFreeRoutine;
#if DEBUG_MAC_MEMORY || STATS_MAC_MEMORY
	size_t							blockSize;
#endif
#if DEBUG_MAC_MEMORY
	MemoryBlockHeader				*next;
	MemoryBlockHeader				*prev;
	UInt32							blockNum;
	void							*whoAllocated[kRecordingDepthOfStackLevels];
	MemoryBlockTag					headerTag;
#endif
};

#if DEBUG_MAC_MEMORY
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
#pragma mark STANDARD (SLOW!) ALLOCATOR DEFINITIONS

void *StandardAlloc(size_t blockSize, void *refcon);
void StandardFree(void *block, void *refcon);

//##############################################################################
//##############################################################################
#pragma mark FIXED SIZED (COMPACT) ALLOCATOR DEFINITIONS

typedef struct FixedSizeCompactAllocationChunk FixedSizeCompactAllocationChunk;

typedef struct FixedSizeCompactAllocationRoot FixedSizeCompactAllocationRoot;

struct FixedSizeCompactAllocationChunk {
	FixedSizeCompactAllocationChunk			*next;
	FixedSizeCompactAllocationRoot			*root;
	UInt32									chunkUsage;
};


struct FixedSizeCompactAllocationRoot {
	FixedSizeCompactAllocationChunk			*firstChunk;
	UInt32									blockSize;
	FreeMemoryBlockDescriptor				*freeDescriptorTable;
};

void *FixedSizeCompactAlloc(size_t blockSize, void *refcon);
void FixedSizeCompactFree(void *block, void *refcon);

#define DeclareFixedSizeCompactAllocator(blockSize) \
FreeMemoryBlockDescriptor FixedSizeCompact##blockSize##AllocationFreeDescriptors[] = { \
{ &FixedSizeCompactFree, (void *)((0L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 0 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((1L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 1 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((2L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 2 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((3L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 3 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((4L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 4 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((5L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 5 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((6L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 6 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((7L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 7 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((8L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 8 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((9L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 9 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((10L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 10 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((11L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 11 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((12L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 12 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((13L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 13 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((14L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 14 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((15L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 15 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((16L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 16 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((17L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 17 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((18L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 18 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((19L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 19 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((20L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 20 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((21L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 21 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((22L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 22 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((23L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 23 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((24L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 24 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((25L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 25 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((26L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 26 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((27L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 27 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((28L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 28 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((29L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 29 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((30L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 30 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) },\
{ &FixedSizeCompactFree, (void *)((31L << 16) | (sizeof(FixedSizeCompactAllocationChunk) + 31 * (blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE))) }\
};\
FixedSizeCompactAllocationRoot	gFixedSizeCompact##blockSize##Root = { NULL, blockSize, FixedSizeCompact##blockSize##AllocationFreeDescriptors }\

//##############################################################################
//##############################################################################
#pragma mark FIXED SIZED (FAST) ALLOCATOR DEFINITIONS

typedef struct FixedSizeFastMemoryBlockHeader FixedSizeFastMemoryBlockHeader;

struct FixedSizeFastMemoryBlockHeader {
	FixedSizeFastMemoryBlockHeader			*nextFree;
	MemoryBlockHeader						realHeader;
};

typedef struct FixedSizeFastAllocationChunk FixedSizeFastAllocationChunk;

struct FixedSizeFastAllocationChunk {
	FixedSizeFastAllocationChunk			*next;
};

typedef struct FixedSizeFastAllocationRoot FixedSizeFastAllocationRoot;

struct FixedSizeFastAllocationRoot {
	FixedSizeFastAllocationChunk			*firstChunk;
	FixedSizeFastMemoryBlockHeader			*firstFree;
	UInt32									blockSize;
	UInt32									blocksPerChunk;
	FreeMemoryBlockDescriptor				*freeDescriptor;
};

void *FixedSizeFastAlloc(size_t blockSize, void *refcon);
void FixedSizeFastFree(void *block, void *refcon);

#define DeclareFixedSizeFastAllocator(blockSize, blocksPerChunk) \
extern FixedSizeFastAllocationRoot	gFixedSizeFast##blockSize##Root; \
FreeMemoryBlockDescriptor FixedSizeFast##blockSize##AllocationFreeDescriptor = { &FixedSizeFastFree, (void *)&gFixedSizeFast##blockSize##Root}; \
FixedSizeFastAllocationRoot	gFixedSizeFast##blockSize##Root = { NULL, NULL, blockSize, blocksPerChunk, &FixedSizeFast##blockSize##AllocationFreeDescriptor }

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
	kMaximumBinBlockSize		= kDefaultSmallHeadMinSize + 4L * kDefaultSmallHeapBins - 1,
	kSmallHeapSize				= 256 * 1024
};

typedef struct SmallHeapChunk SmallHeapChunk;

typedef struct SmallHeapRoot SmallHeapRoot;

struct SmallHeapRoot {
	FreeMemoryBlockDescriptor	*blockFreeRoutine;
	SmallHeapChunk				*firstChunk;
	SmallHeapBlock				*overflow;
	SmallHeapBlock				*bins[kDefaultSmallHeapBins];
};

struct SmallHeapChunk {
	SmallHeapChunk			*nextChunk;
};

#define DeclareSmallHeapAllocator() \
extern SmallHeapRoot	gSmallHeapRoot; \
FreeMemoryBlockDescriptor SmallHeapAllocationFreeDescriptor = { &SmallHeapFree, (void *)&gSmallHeapRoot}; \
SmallHeapRoot	gSmallHeapRoot = { &SmallHeapAllocationFreeDescriptor }

void *SmallHeapAlloc(size_t blockSize, void *refcon);
void SmallHeapFree(void *address, void *refcon);

//##############################################################################
//##############################################################################

//	Clients must provide the following two entry points.

extern AllocMemoryBlockDescriptor gFastMemSmallSizeAllocators[];
extern Ptr AllocateRawMemory(Size blockSize);
extern void FreeRawMemory(Ptr reclaimedRawBlock);

//	VerifyMallocHeapIntegrity will report any block headers or
//	trailers that have been overwritten.

extern void InstallMemoryManagerPatches();
extern void VerifyMallocHeapIntegrity();
static void TagReferencedBlocks();
extern void DumpAllocHeap();

//##############################################################################
//##############################################################################

#ifdef DEBUG_MAC_MEMORY

extern UInt32						gVerifyHeapOnEveryMalloc;
extern UInt32						gVerifyHeapOnEveryFree;
extern UInt32						gFillUsedBlocksWithPattern;
extern UInt32						gFillFreeBlocksWithPattern;
extern UInt32						gTrackLeaks;
extern UInt32						gDontActuallyFreeMemory;
extern SInt32						gFailToAllocateAfterNMallocs;

extern SInt32						gReportActiveBlocks;
extern SInt32						gReportLeaks;

#endif

