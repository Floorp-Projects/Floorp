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
#include <OSUtils.h>
#include <Gestalt.h>
#include <Processes.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "prlog.h"


#include "TypesAndSwitches.h"
#include "MacMemAllocator.h"


#if STATS_MAC_MEMORY
#ifdef XP_MAC
#include <Types.h>
#include <unistd.h>
#include <fcntl.h>
#endif /* XP_MAC */

#include "prglobal.h"
#include "prfile.h"

static void WriteString ( PRFileHandle file, const char * string );
extern void DumpAllocatorMemoryStats(PRFileHandle outFile);		/* this is in LowLevel.c */
#endif /* STATS_MAC_MEMORY */

#include "MemoryTracker.h"

#define DEBUG_ALLOCATION_CHUNKS 0
#define DEBUG_MAC_ALLOCATORS    0
#define DEBUG_DONT_TRACK_MALLOC 0
#define DEBUG_TRACK_MACOS_MEMS  1

#define GROW_SMALL_BLOCKS 1

//##############################################################################
//##############################################################################
#pragma mark malloc DECLARATIONS

static Boolean	gInsideCacheFlush = false;

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark malloc DEBUG DECLARATIONS

#if STATS_MAC_MEMORY
#define ALLOCATION_TABLE_SIZE		1024
#define WRITE_TABLES_FLAT			false

UInt32 gSmallAllocationTotalCountTable[ALLOCATION_TABLE_SIZE + 1];
UInt32 gSmallAllocationActiveCountTable[ALLOCATION_TABLE_SIZE + 1];
UInt32 gSmallAllocationMaxCountTable[ALLOCATION_TABLE_SIZE + 1];
#endif

#if DEBUG_HEAP_INTEGRITY

enum {
	kFreeBlockHeaderTag		= 'FREE',
	kFreeBlockTrailerTag	= 'free',
	kUsedBlockHeaderTag		= 'USED',
	kUsedBlockTrailerTag	= 'used',
	kRefdBlockHeaderTag		= 'REFD',
	kRefdBlockTrailerTag	= 'refd',
	kFreeMemoryFillPattern 	= 0xEF,
	kUsedMemoryFillPattern	= 0xDB
};

MemoryBlockHeader *	gFirstAllocatedBlock = NULL;
MemoryBlockHeader *	gLastAllocatedBlock = NULL;

#endif

extern void	*		gOurApplicationHeapBase;
extern void	*		gOurApplicationHeapMax;

#if DEBUG_MAC_MEMORY || DEBUG_HEAP_INTEGRITY || STATS_MAC_MEMORY

UInt32				gOutstandingPointers = 0;
UInt32				gOutstandingHandles = 0;

#if DEBUG_MAC_MEMORY || STATS_MAC_MEMORY

#if DEBUG_MAC_MEMORY

#define	kMaxInstructionScanDistance	4096

AllocationSet *		gFixedSizeAllocatorSet = NULL;
AllocationSet *		gSmallHeapAllocatorSet = NULL;
AllocationSet *		gLargeBlockAllocatorSet = NULL;

void		*		gMemoryTop;

static void PrintStackCrawl ( void ** stackCrawl, char * name );
static void CatenateRoutineNameFromPC( char * string, void *currentPC );
static void PrintHexNumber( char * string, UInt32 hexNumber );
#endif /* DEBUG_MAC_MEMORY */

pascal void TrackerExitToShellPatch(void);

/* patch on exit to shell to always print out our log */
UniversalProcPtr	gExitToShellPatchCallThru = NULL;
#if GENERATINGCFM
enum {
	uppExitToShellProcInfo 				= kPascalStackBased
};

RoutineDescriptor 	gExitToShellPatchRD = BUILD_ROUTINE_DESCRIPTOR(uppExitToShellProcInfo, &TrackerExitToShellPatch);
#else
#define gExitToShellPatchRD ExitToShellPatch
#endif

#endif

#endif

#if STATS_MAC_MEMORY
void DumpCurrentMemoryStatistics(char *operation);
void PrintEfficiency(PRFileHandle file, UInt32 current, UInt32 total);


#endif

/*
asm void AsmFree(void *);
asm void *AsmMalloc(size_t);
*/

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark malloc DEBUG CONTROL VARIABLES

#if DEBUG_HEAP_INTEGRITY

UInt32						gVerifyHeapOnEveryMalloc			= 0;
UInt32						gVerifyHeapOnEveryFree				= 0;
UInt32						gFillUsedBlocksWithPattern			= 0;
UInt32						gFillFreeBlocksWithPattern			= 1;
UInt32						gOnMallocFailureReturnDEADBEEF		= 0;
SInt32						gFailToAllocateAfterNMallocs		= -1;

SInt32						gTagReferencedBlocks				= 0;			// for best effect, turn on gFillFreeBlocksWithPattern also

#endif

#if DEBUG_MAC_MEMORY || DEBUG_HEAP_INTEGRITY
UInt32						gDontActuallyFreeMemory				= 0;
UInt32						gNextBlockNum = 0;
#endif

//##############################################################################
//##############################################################################

#if STATS_MAC_MEMORY

UInt32		gTotalFixedSizeBlocksAllocated = 0;
UInt32		gTotalFixedSizeBlocksUsed = 0;
UInt32		gTotalFixedSizeBlocksMaxUsed = 0;
UInt32		gTotalFixedSizeAllocated = 0;
UInt32		gTotalFixedSizeUsed = 0;
UInt32		gTotalFixedSizeMaxUsed = 0;

UInt32		gTotalSmallHeapChunksAllocated = 0;				// the number of chunks (sub-heaps)
UInt32		gTotalSmallHeapChunksMaxAllocated = 0;			// max no. chunks

UInt32		gTotalSmallHeapTotalChunkSize = 0;				// total space allocated in small heaps
UInt32		gTotalSmallHeapMaxTotalChunkSize = 0;			// max space allocated

UInt32		gTotalSmallHeapBlocksUsed = 0;					// the number of blocks used
UInt32		gTotalSmallHeapBlocksMaxUsed = 0;				// max blocks used

UInt32		gTotalSmallHeapUsed = 0;						// space used in small heaps
UInt32		gTotalSmallHeapMaxUsed = 0;						// max space used


UInt32		gTotalLargeHeapChunksAllocated = 0;				// the number of chunks (sub-heaps)
UInt32		gTotalLargeHeapChunksMaxAllocated = 0;			// max no. chunks

UInt32		gTotalLargeHeapTotalChunkSize = 0;				// total space allocated in small heaps
UInt32		gTotalLargeHeapMaxTotalChunkSize = 0;			// max space allocated

UInt32		gTotalLargeHeapBlocksUsed = 0;					// the number of blocks used
UInt32		gTotalLargeHeapBlocksMaxUsed = 0;				// max blocks used

UInt32		gTotalLargeHeapUsed = 0;						// space used in small heaps
UInt32		gTotalLargeHeapMaxUsed = 0;						// max space used

#endif


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark malloc AND free

static void* LargeBlockGrow(LargeBlockHeader* blockHeader, size_t newSize, void* refcon); // forward
static void* LargeBlockShrink(LargeBlockHeader* blockHeader, size_t newSize, void* refcon); // forward
static void* SmallBlockGrow(void* address, size_t newSize, void* refcon); // forward
static void* SmallBlockShrink(void* address, size_t newSize, void* refcon); // forward

/*
 * These are kinda ugly as all the DEBUG code is ifdefed inside here, but I really
 * don't like the idea of having multiple implementations of the basic malloc
 */
 
void *malloc(size_t blockSize)
{
	void					*		newBlock;
	AllocMemoryBlockDescriptor *	whichAllocator;
#if DEBUG_HEAP_INTEGRITY
	MemoryBlockHeader *				newBlockHeader;
	MemoryBlockTrailer *			newBlockTrailer;	
#endif
#if DEBUG_HEAP_INTEGRITY || DEBUG_MAC_MEMORY || STATS_MAC_MEMORY
	size_t							newCompositeSize = blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;
	size_t							blockSizeCopy = blockSize;
#endif
#if DEBUG_MAC_MEMORY
	void *							stackCrawl[ kStackDepth ];
#endif

	/*
	 * I hate sticking this here, but static initializers may allocate memory and so
	 * we cannot initialize memory through our normal execution path. We could put it in
	 * more efficient places inside the allocators, but then it needs to be added to all
	 * the allocators and that seems rather bug-prone...
	 */
	if ( gMemoryInitialized == false )
	{
		MacintoshInitializeMemory();
	}

	PR_ASSERT((SInt32)blockSize >= 0);		// if the high bit is set, someone probably passed in a -ve blocksize
	
#ifdef MALLOC_IS_NEWPTR
	return NewPtr(blockSize);
#endif

#if DEBUG_HEAP_INTEGRITY
	if (gVerifyHeapOnEveryMalloc)
		VerifyMallocHeapIntegrity();
		
	if (gFailToAllocateAfterNMallocs != -1) {
		if (gFailToAllocateAfterNMallocs-- == 0)
			return NULL;
	}
#endif

#if STATS_MAC_MEMORY

	if (blockSizeCopy >= ALLOCATION_TABLE_SIZE)
		blockSizeCopy = ALLOCATION_TABLE_SIZE;

	gSmallAllocationTotalCountTable[blockSizeCopy]++;
	gSmallAllocationActiveCountTable[blockSizeCopy]++;
	if (gSmallAllocationActiveCountTable[blockSizeCopy] > gSmallAllocationMaxCountTable[blockSizeCopy])
		gSmallAllocationMaxCountTable[blockSizeCopy] = gSmallAllocationActiveCountTable[blockSizeCopy];

#endif

	/* THE REAL MALLOC CODE */
	
	/* which allocator should we use? */
	if (blockSize <= kMaxTableAllocatedBlockSize)
		{
		whichAllocator = gFastMemSmallSizeAllocators + ((blockSize + 3) >> 2);
		}
	else
		{
		/* large blocks come out of entry 0 */
		whichAllocator = &gFastMemSmallSizeAllocators[ 0 ];
		}
	
	newBlock = (whichAllocator->blockAllocRoutine)(blockSize, whichAllocator->root);

	if ( newBlock == NULL )
	{
		/* if we don't have it, try allocating a new chunk */
		if ( (whichAllocator->chunkAllocRoutine)( blockSize, whichAllocator->root ) != NULL )
		{
			newBlock = (char *)(whichAllocator->blockAllocRoutine)(blockSize, whichAllocator->root);
		}
		else
		{
			/* We're in deep doo-doo. We failed to allocate a new sub-heap, so let's */
			/* flush everything that we can. */
			/* We need to ensure no one else tries to flush cache's while we do */
			/* This may mean that we allocate temp mem while flushing the cache that we would */
			/* have had space for after the flush, but life sucks... */
			
			if ( gInsideCacheFlush == false )
			{			
				/* Flush some caches and try again */
				gInsideCacheFlush = true;
				CallCacheFlushers ( blockSize );
				gInsideCacheFlush = false;
				
				/* We may have freed up some space, so let's try allocating again */
				newBlock = (char *)(whichAllocator->blockAllocRoutine)(blockSize, whichAllocator->root);
			}
		}
		
		/*
		 * If that fails, we may be able to succeed by choosing the next biggest block size.
		 * We should signal the user that things are sucking though. We will never be able
		 * to allocate a large block at this point. We might be able to get a small block.
		 */
		if ( newBlock == NULL )
		{
			if ( blockSize <= kMaxTableAllocatedBlockSize )
			{
				UInt32	allocatorIndex;
				
				allocatorIndex = ((blockSize + 3) >> 2);
				
				/* of course, this will only work for small blocks */
				while ( ++allocatorIndex <= (( kMaxTableAllocatedBlockSize >> 2 ) + 1 ))
				{
					if ( allocatorIndex <= ( kMaxTableAllocatedBlockSize >> 2 ) )
					{
						/* try another small block allocator */
						whichAllocator = &gFastMemSmallSizeAllocators[ allocatorIndex ];
					}
					else
					{
						/* try the large block allocator */
						whichAllocator = &gFastMemSmallSizeAllocators[ 0 ];
					}
						
					newBlock = (char *)(whichAllocator->blockAllocRoutine)(blockSize, whichAllocator->root);
					if ( newBlock != NULL )
					{
						break;
					}
				}
			}
			
			/* tell the FE */
			CallFE_LowMemory();
		}
		
		/* now, if we don't have any memory, then we really suck */
		if ( newBlock == NULL )
		{
	#if DEBUG_HEAP_INTEGRITY
			if (gOnMallocFailureReturnDEADBEEF)
			{
				return (void *)0xDEADBEEF;
			}
	#endif
			return NULL;
		}
	}
	
	/* END REAL MALLOC CODE */

#if DEBUG_HEAP_INTEGRITY || DEBUG_MAC_MEMORY
	newBlock = (char *) newBlock - sizeof(MemoryBlockHeader);

#if DEBUG_HEAP_INTEGRITY
	//	Fill in the blocks header.  This includes adding the block to the used list.
	newBlockHeader = (MemoryBlockHeader *)newBlock;

	newBlockHeader->blockSize = blockSize;
	newBlockHeader->headerTag = kUsedBlockHeaderTag;

	newBlockHeader->next = gFirstAllocatedBlock;
	newBlockHeader->prev = NULL;	

	if (gFirstAllocatedBlock != NULL)
		gFirstAllocatedBlock->prev = newBlockHeader;
	else
		gLastAllocatedBlock = newBlockHeader;
	gFirstAllocatedBlock = newBlockHeader;
		
	//	Fill in the trailer
	newBlockTrailer = (MemoryBlockTrailer *)((char *)newBlock + newCompositeSize - sizeof(MemoryBlockTrailer));
	newBlockTrailer->trailerTag = kUsedBlockTrailerTag;
	
	//	Fill with the used memory pattern
	if (gFillUsedBlocksWithPattern)
		memset((char *)newBlock + sizeof(MemoryBlockHeader), kUsedMemoryFillPattern, blockSize);
#endif

#if DEBUG_MAC_MEMORY && GENERATINGPOWERPC
	GetCurrentNativeStackTrace ( stackCrawl );

#if TRACK_EACH_ALLOCATOR
	EnableAllocationSet ( whichAllocator->root->set );
	TrackItem ( newBlock, blockSize, 0, kMallocBlock, stackCrawl );
	DisableAllocationSet ( whichAllocator->root->set );
#elif !DEBUG_DONT_TRACK_MALLOC
	TrackItem ( newBlock, blockSize, 0, kMallocBlock, stackCrawl );
#endif

#endif
	
	return (void *)((char *)newBlock + sizeof(MemoryBlockHeader));
#else
	return newBlock;
#endif
}

void free(void *deadBlock)
{
	MemoryBlockHeader			*deadBlockHeader;
#if DEBUG_HEAP_INTEGRITY
	MemoryBlockTrailer			*deadBlockTrailer;
#endif
#if DEBUG_HEAP_INTEGRITY 	//|| STATS_MAC_MEMORY
	size_t						deadBlockSize;	
#endif
	char						*deadBlockBase;
	FreeMemoryBlockDescriptor	*descriptor;
	
#if DEBUG_HEAP_INTEGRITY
	if (gVerifyHeapOnEveryFree)
		VerifyMallocHeapIntegrity();
#endif
		
	if (deadBlock == NULL)
		return;

#ifdef MALLOC_IS_NEWPTR
	DisposePtr(deadBlock);
	return;
#endif

	deadBlockBase = ((char *)deadBlock - sizeof(MemoryBlockHeader));

	deadBlockHeader = (MemoryBlockHeader *)deadBlockBase;

#if DEBUG_HEAP_INTEGRITY		// || STATS_MAC_MEMORY
	deadBlockSize = deadBlockHeader->blockSize;
#endif
#if DEBUG_HEAP_INTEGRITY
	deadBlockTrailer = (MemoryBlockTrailer *)(deadBlockBase + deadBlockSize + sizeof(MemoryBlockHeader));
#endif
	
#if STATS_MAC_MEMORY
	{
		UInt32		blockSize = memsize(deadBlock);			/*	This doesn't work, beacuse it always returns
																the 4-byte rounded value */
		if (blockSize >= ALLOCATION_TABLE_SIZE)
			blockSize = ALLOCATION_TABLE_SIZE;
		gSmallAllocationActiveCountTable[blockSize]--;
	}
#endif
	
#if DEBUG_HEAP_INTEGRITY
	if (deadBlockHeader->headerTag == kFreeBlockHeaderTag) {
		DebugStr("\pfastmem: double dispose of malloc block");
		return;
	}
	else
		if ((deadBlockHeader->headerTag != kUsedBlockHeaderTag) || (deadBlockTrailer->trailerTag != kUsedBlockTrailerTag)) {
			DebugStr("\pfastmem: attempt to dispose illegal block");
			return;
		}
	
	//	Remove the block from the list of used blocks.
	
	if (deadBlockHeader->next != NULL)
		deadBlockHeader->next->prev = deadBlockHeader->prev;
	else
		gLastAllocatedBlock = deadBlockHeader->prev;
		
	if (deadBlockHeader->prev != NULL)
		deadBlockHeader->prev->next = deadBlockHeader->next;
	else
		gFirstAllocatedBlock = deadBlockHeader->next;
	
	//	Change the header and tailer tags to be the free ones.
	
	deadBlockHeader->headerTag = kFreeBlockHeaderTag;
	deadBlockTrailer->trailerTag = kFreeBlockTrailerTag;
#endif

#if DEBUG_MAC_MEMORY
	ReleaseItem ( deadBlockBase );
#endif

#if DEBUG_HEAP_INTEGRITY
	//	Fill with the free memory pattern
	if (gFillFreeBlocksWithPattern)
		memset(deadBlock, kFreeMemoryFillPattern, deadBlockSize);
#endif

	descriptor = deadBlockHeader->blockFreeRoutine;
	
#if DEBUG_MAC_MEMORY || DEBUG_HEAP_INTEGRITY
	if (!gDontActuallyFreeMemory)
		{
		(descriptor->freeRoutine)(deadBlock, descriptor->refcon);
		}
#else
	(descriptor->freeRoutine)(deadBlock, descriptor->refcon);
#endif
}

size_t memsize ( void * block )
{
	MemoryBlockHeader			*blockHeader;
#if DEBUG_HEAP_INTEGRITY
	MemoryBlockTrailer			*blockTrailer;
	size_t						blockSize;	
#endif
	char						*blockBase;
	FreeMemoryBlockDescriptor	*descriptor;
			
	if (block == NULL)
		return 0;
	
#ifdef MALLOC_IS_NEWPTR
	return GetPtrSize(block);
#endif

	blockBase = ((char *)block - sizeof(MemoryBlockHeader));
	blockHeader = (MemoryBlockHeader *)blockBase;

#if DEBUG_HEAP_INTEGRITY
	// make sure we're looking at a real block
	blockSize = blockHeader->blockSize;
	blockTrailer = (MemoryBlockTrailer *)(blockBase + blockSize + sizeof(MemoryBlockHeader));
	
	if (blockHeader->headerTag == kFreeBlockHeaderTag) {
		DebugStr("\pfastmem: attempt to size free block");
		return 0;
	}
	else
		if ((blockHeader->headerTag != kUsedBlockHeaderTag) || (blockTrailer->trailerTag != kUsedBlockTrailerTag)) {
			DebugStr("\pfastmem: attempt to size illegal block");
			return 0;
		}
#endif

	descriptor = blockHeader->blockFreeRoutine;
	return (descriptor->sizeRoutine)(block, descriptor->refcon);
}


void memtotal ( size_t blockSize, FreeMemoryStats * stats )
{
	AllocMemoryBlockDescriptor *	whichAllocator;

#ifdef MALLOC_IS_NEWPTR
	// make up some bogus stats
	//stats->totalHeapSize = ApplLimit - ApplZone;
	stats->totalFreeBytes = FreeMem();
	stats->maxBlockSize = MaxBlock();		// this should be the largest block allocated
	return;
#endif

	/* which allocator should we use? */
	if (blockSize <= kMaxTableAllocatedBlockSize)
	{
		whichAllocator = gFastMemSmallSizeAllocators + ((blockSize + 3) >> 2);
	}
	else
	{
		/* large blocks come out of entry 0 */
		whichAllocator = &gFastMemSmallSizeAllocators[ 0 ];
	}
	
	whichAllocator->heapFreeSpaceRoutine ( blockSize, stats, whichAllocator->root );
}

//----------------------------------------------------------------------------------------
static Boolean resize_block(void* block, size_t newSize)
//      Returning false is a signal to the caller to use other means - it does not
//      mean that mem is full, it may just mean that we "don't know".
//----------------------------------------------------------------------------------------
{
#ifdef MALLOC_IS_NEWPTR

        SetPtrSize(block);
        return ::MemError() == noErr;

#else

        char                                            *blockBase = ((char *)block - sizeof(MemoryBlockHeader));
        MemoryBlockHeader                       *blockHeader = (MemoryBlockHeader *)blockBase;
#if DEBUG_HEAP_INTEGRITY        //|| STATS_MAC_MEMORY
        size_t                                          oldBlockSize = blockHeader->blockSize;
#endif
        FreeMemoryBlockDescriptor       *descriptor     = blockHeader->blockFreeRoutine;

        if (descriptor->freeRoutine == LargeBlockFree)
        {
                LargeBlockHeader *              largeBlockHeader
                        = (LargeBlockHeader *) ((char *)block - sizeof(LargeBlockHeader));

#if DEBUG_HEAP_INTEGRITY
                if (blockHeader->headerTag == kFreeBlockHeaderTag)
                {
                        DebugStr("\pfastmem: attempt to grow a freed block");
                        return false;
                }
                else
                {
                        MemoryBlockTrailer* blockTrailer = (MemoryBlockTrailer *)((char*)block + oldBlockSize);
                        
                        if ((blockHeader->headerTag != kUsedBlockHeaderTag) || (blockTrailer->trailerTag != kUsedBlockTrailerTag))
                        {
                                DebugStr("\pfastmem: attempt to grow illegal block");
                                return false;
                        }
                }
#endif

                if (newSize > largeBlockHeader->logicalSize)
                        block = LargeBlockGrow(largeBlockHeader, newSize, descriptor->refcon);
                else if (newSize < largeBlockHeader->logicalSize)
                        block = LargeBlockShrink(largeBlockHeader, newSize, descriptor->refcon);
                else
                        return true;
        }
        else if (descriptor->freeRoutine == FixedSizeFree)
        {
                /*      If the new size would require a "large block", give up */
                if (newSize > kMaxTableAllocatedBlockSize)
                        return false;
                
                /*      If the new size could not be allocated within the same block
                        size, then give up */
                if (((newSize + 3) >> 2) != ((memsize(block) + 3) >> 2))
                        return false;
                
                /* Otherwise, we can reuse the same block! */
        }
#ifdef GROW_SMALL_BLOCKS
        else if (descriptor->freeRoutine == SmallHeapFree)
        {
                size_t newAllocSize = ((newSize + 3) & 0xFFFFFFFC);
                size_t oldAllocSize = ((memsize(block) + 3) & 0xFFFFFFFC);
                if (newAllocSize > oldAllocSize)
                {
                        /*      If the new size would require a "large block", give up */
                        if (newAllocSize > kMaxTableAllocatedBlockSize)
                                return false;
                        block = SmallBlockGrow(block, newSize, descriptor->refcon);
                }
                else if (newAllocSize < oldAllocSize)
                {
                        /* If the new size should be handled as a fixed block, give up */
                        AllocMemoryBlockDescriptor* newAllocator = gFastMemSmallSizeAllocators + (newAllocSize >> 2);
                        if (newAllocator->heapFreeSpaceRoutine != SmallBlockHeapFree)
                                return false;
                        block = SmallBlockShrink(block, newSize, descriptor->refcon);
                }

                /* Otherwise, we can reuse the same block! */
        }
#endif // GROW_SMALL_BLOCKS
        else
        {
                return false;
        }
                
#if DEBUG_HEAP_INTEGRITY
        if (block)
        {
                //      Fix the size in the block's header, and fill in the trailer.
                MemoryBlockTrailer* newBlockTrailer = (MemoryBlockTrailer *)((char *)block + newSize);

                blockHeader->blockSize = newSize;
                newBlockTrailer->trailerTag = kUsedBlockTrailerTag;

                //      Fill with the used memory pattern
                if (gFillUsedBlocksWithPattern && newSize > oldBlockSize)
                        memset(
                                (char *)block + oldBlockSize,
                                kUsedMemoryFillPattern,
                                newSize - oldBlockSize);
        }
#endif
        return (block != NULL);

#endif /*MALLOC_IS_NEWPTR*/
} /* resize_block */

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark realloc AND calloc

void* reallocSmaller(void* block, size_t newSize)
{
#pragma unused (newSize)

        return realloc(block, newSize);
}

void* realloc(void* block, size_t newSize)
{
        void* newBlock = NULL;
        
        if (!block)
                newBlock = malloc(newSize);
        else if (resize_block(block, newSize))
                newBlock = block;
        else
        {
#ifdef MALLOC_IS_NEWPTR
                size_t                          oldSize = GetPtrSize(block);
#else
                size_t                          oldSize = memsize(block);
#endif

                newBlock = malloc(newSize);
                
                if (newBlock)
                {       
					                        BlockMoveData(block, newBlock, newSize < oldSize ? newSize : oldSize);
							free(block);
	}
        }
	return newBlock;
}

void *calloc(size_t nele, size_t elesize)
{
	char 		*newBlock = NULL;
	UInt8		tryAgain = true;
	size_t		totalSize = nele * elesize;

	newBlock = (char *) malloc(totalSize);

	if (newBlock != NULL)
		memset(newBlock, 0, totalSize);

	return newBlock;
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark DEBUG MEMORY UTILS

#if STATS_MAC_MEMORY

FILE 	*statsFile = NULL;

void DumpCurrentMemoryStatistics(char *operation)
{
	UInt32	count;
	
	if (statsFile == NULL)
		statsFile = fopen("stats", "w");
	
	fprintf(statsFile, "%s\n", operation);
	fprintf(statsFile, "Total Allocations\n");

	for (count = 0; count < ALLOCATION_TABLE_SIZE + 1; count++)
		fprintf(statsFile, "%ld\n", gSmallAllocationTotalCountTable[count]);

	fprintf(statsFile, "Current Allocation Allocations\n");

	for (count = 0; count < ALLOCATION_TABLE_SIZE + 1; count++)
		fprintf(statsFile, "%ld\n", gSmallAllocationActiveCountTable[count]);

}

#endif


#if DEBUG_HEAP_INTEGRITY

extern void VerifyMallocHeapIntegrity()
{
	MemoryBlockHeader	*currentBlock = gFirstAllocatedBlock;

	//	Walk the used block list, checking all of the headers.

	while (currentBlock != NULL) {
	
		MemoryBlockTrailer	*trailer;
		
		trailer = (MemoryBlockTrailer *)((char *)currentBlock + sizeof(MemoryBlockHeader) + currentBlock->blockSize);
	
		if ((currentBlock->headerTag != kUsedBlockHeaderTag) || (trailer->trailerTag != kUsedBlockTrailerTag))
			DebugStr("\pfastmem: malloc heap is corrupt!");
		
		currentBlock = currentBlock->next;
	
	}
	
}

#endif

#if STATS_MAC_MEMORY
void PrintEfficiency(PRFileHandle file, UInt32 current, UInt32 total)
{
	char	string[ 256 ];
/*
	UInt32 	efficiency = current * 1000 / total;
	UInt32	efficiencyLow = efficiency / 10;
	char	string[ 256 ];
	
	efficiency = efficiency/10;
	
	sprintf(string, "%ld.%ld", efficiency, efficiencyLow);
*/
	if (total > 0)
		sprintf(string, "%10.4f", 100.0 * current / total);
	else
		sprintf(string, "n/a");
		
	WriteString ( file, string );
}
#endif

#if DEBUG_HEAP_INTEGRITY
static void TagReferencedBlocks(void)
{
	char *start = (char *) ApplicationZone();
	char *end = (char *) GetApplLimit();
	
	char *comb = start;
	
	while (comb < end) {
	
		MemoryBlockHeader *curCandidate = (MemoryBlockHeader *) comb;
		
		if (curCandidate->headerTag == kFreeBlockHeaderTag || 
			curCandidate->headerTag == kUsedBlockHeaderTag ||
			curCandidate->headerTag == kRefdBlockHeaderTag) {
			
			// we are pointing at a block header			
			// we don't want to consider the next or prev structure members
			// therefore, skip ahead 8 bytes = 2 * sizeof(MemoryBlockHeader *)
						
			comb += 8;
			
		} else if ((char *) curCandidate->next > start &&
				(char *) curCandidate->next < end) {
				
			MemoryBlockHeader *referencedBlock;
			referencedBlock = (MemoryBlockHeader *) ((char *) curCandidate->next - sizeof(MemoryBlockHeader));
				
			if (referencedBlock->headerTag == kUsedBlockHeaderTag) {
					
				// Bingo!!
				// Marked the referenced block as being referenced.
				// skip ahead 4 bytes	(or should we be conservative and just skip 2 bytes?)
				
				referencedBlock->headerTag = kRefdBlockHeaderTag;	// TODO: insert kRefdBlockTrailerTag as well
				comb += 4;
				
			} else {
			
				// False alarm
				// it looked like a pointer, but it didn't point to the beginning of a valid used block.
				// move forward two bytes and look some more
				
				comb += 2;
			}
				
		} else {
		
			// we aren't looking at anything interesting
			// move forward two bytes and look some more.
			
			comb += 2;
		}
		
	}
	
}
#endif

#if STATS_MAC_MEMORY || DEBUG_MAC_MEMORY
static void PrintHexNumber( char * string, UInt32 hexNumber )
{
	char		hexString[11];
	char		hexMap[] = "0123456789ABCDEF";
	long		digits = 8;
		
	*string = 0;
	hexString[0] = '0';
	hexString[1] = 'x';
	
	while (digits) {
		hexString[digits + 1] = *((hexNumber & 0x0F) + hexMap);
		hexNumber = hexNumber >> 4;
		digits--;
	}
	hexString[10] = 0;
	
	strcat ( string, hexString );
}
#endif

#if STATS_MAC_MEMORY
static void WriteString ( PRFileHandle file, const char * string )
{
	long	len;
	long	bytesWritten;
	
	len = strlen ( string );
	if ( len >= 1024 ) Debugger();
	bytesWritten = _OS_WRITE ( file, string, len );
	PR_ASSERT(bytesWritten == len);
}


static void WriteAllocatationTableHeader(PRFileHandle outFile, const char *titleString, Boolean writeFlat)
{
	
	WriteString ( outFile, "********************************************************************************\n" );
	WriteString ( outFile, "********************************************************************************\n" );
	
	WriteString ( outFile, titleString );
	WriteString ( outFile, "\n" );
	
	WriteString ( outFile, "********************************************************************************\n" );
	WriteString ( outFile, "********************************************************************************\n" );

	if (writeFlat)
	{
		WriteString(outFile, "  Size         Number   \n");
		WriteString(outFile, "------      ----------  \n");
	}
	else
	{
		WriteString(outFile, "  Size          +0         +1          +2          +3            TOTAL\n");
		WriteString(outFile, "------      ----------  ----------  ----------  ----------  ----------\n");	
	}
}


static void WriteAllocationTableData(PRFileHandle outFile, UInt32 allocationTable[1025], Boolean writeFlat)
{
	char		outString[ 1024 ];
	UInt32		currentItem;

	if (writeFlat)
	{
		for (currentItem = 0; currentItem < ALLOCATION_TABLE_SIZE + 1; currentItem++)
		{
			sprintf(outString, "%6d  %10d\n", currentItem, allocationTable[currentItem]);
			WriteString ( outFile, outString );
		}
	
	}
	else
	{
		for (currentItem = 0; currentItem < 64; currentItem++)
		{
			sprintf(outString, "%6d  %10d  %10d  %10d  %10d  %10d\n",
						currentItem * 4,
						allocationTable[currentItem * 4],
						allocationTable[currentItem * 4 + 1],
						allocationTable[currentItem * 4 + 2],
						allocationTable[currentItem * 4 + 3],
						allocationTable[currentItem * 4] + 
							allocationTable[currentItem * 4 + 1] +
							allocationTable[currentItem * 4 + 2] +
							allocationTable[currentItem * 4 + 3]
					);
			
			WriteString ( outFile, outString );
		}
	}
	
	sprintf(outString, "Blocks Over 1K:%8d  \n\n\n",  allocationTable[ALLOCATION_TABLE_SIZE]);
	WriteString ( outFile, outString );
}

static void WriteAllocatorStatsExplanations(PRFileHandle outFile)
{
	WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
	
	WriteString ( outFile, "Description of statistics for the various allocators\n" );
	
	WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
	
	WriteString ( outFile, "Current:                     Current usage at time of data dump\n" );
	WriteString ( outFile, "Max:                         Max usage throughout session\n\n" );
	
	WriteString ( outFile, "Num chunks:                  Number of chunks (sub-heaps) allocated\n" );
	WriteString ( outFile, "Chunk total:                 Total size of chunks allocated\n" );
	WriteString ( outFile, "Num blocks:                  Number of blocks allocated in the allocated chunks\n" );
	WriteString ( outFile, "Block space:                 Space taken up by used blocks. Does not include the block header,\n");
	WriteString ( outFile,"                              but includes rounding up block sizes to 4 byte boundaries\n" );
	WriteString ( outFile, "Blocks used:                 Number of blocks used\n" );
	WriteString ( outFile, "\n" );
	WriteString ( outFile, "%s of used blocks occupied:  Percentage of the blocks allocated which are in use\n" );
	WriteString ( outFile, "%s of chunks occupied:       Percentage of the chunks used by client data (includes 4-byte block rounding)\n" );
	WriteString ( outFile, "(Percentages are calculated from the max usage figures)\n" );
	WriteString ( outFile, "\n\n");
}


static void WriteSmallHeapMemoryUsageData(PRFileHandle outFile)
{
	char			outString[ 1024 ];
	
	WriteString ( outFile, "\n\n--------------------------------------------------------------------------------\n" );
	WriteString(outFile, "Stats for small heap allocator (blocks 44 - 256 bytes)\n");
	WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
	WriteString ( outFile, "                     Current         Max\n" );
	WriteString ( outFile, "                  ----------     -------\n" );
	sprintf( outString,    "Num chunks:       %10d  %10d\n", gTotalSmallHeapChunksAllocated, gTotalSmallHeapChunksMaxAllocated );
	WriteString ( outFile, outString );
	sprintf( outString,    "Chunk total:      %10d  %10d\n", gTotalSmallHeapTotalChunkSize, gTotalSmallHeapMaxTotalChunkSize );
	WriteString ( outFile, outString );
	sprintf( outString,    "Block space:      %10d  %10d\n", gTotalSmallHeapUsed, gTotalSmallHeapMaxUsed );
	WriteString ( outFile, outString );
	sprintf( outString,    "Blocks used:      %10d  %10d\n", gTotalSmallHeapBlocksUsed, gTotalSmallHeapBlocksMaxUsed );
	WriteString ( outFile, outString );
	WriteString ( outFile, "                                 -------\n" );
	sprintf( outString,    "%s of allocated space used:    %10.2f\n", "%", 100.0 * gTotalSmallHeapMaxUsed / gTotalSmallHeapMaxTotalChunkSize );
	WriteString ( outFile, outString );

	WriteString ( outFile, "\n\n");
}

static void WriteLargeHeapMemoryUsageData(PRFileHandle outFile)
{
	char			outString[ 1024 ];
	
	WriteString ( outFile, "\n\n--------------------------------------------------------------------------------\n" );
	WriteString(outFile, "Stats for large heap allocator (blocks > 256 bytes)\n");
	WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
	WriteString ( outFile, "                     Current         Max\n" );
	WriteString ( outFile, "                  ----------     -------\n" );
	sprintf( outString,    "Num chunks:       %10d  %10d\n", gTotalLargeHeapChunksAllocated, gTotalLargeHeapChunksMaxAllocated );
	WriteString ( outFile, outString );
	sprintf( outString,    "Chunk total:      %10d  %10d\n", gTotalLargeHeapTotalChunkSize, gTotalLargeHeapMaxTotalChunkSize );
	WriteString ( outFile, outString );
	sprintf( outString,    "Block space:      %10d  %10d\n", gTotalLargeHeapUsed, gTotalLargeHeapMaxUsed );
	WriteString ( outFile, outString );
	sprintf( outString,    "Blocks used:      %10d  %10d\n", gTotalLargeHeapBlocksUsed, gTotalLargeHeapBlocksMaxUsed );
	WriteString ( outFile, outString );
	WriteString ( outFile, "                                 -------\n" );
	sprintf( outString,    "%s of allocated space used:    %10.2f\n", "%", 100.0 * gTotalLargeHeapMaxUsed / gTotalLargeHeapMaxTotalChunkSize );
	WriteString ( outFile, outString );

	WriteString ( outFile, "\n\n");

}

#endif

void DumpMemoryStats(void)
{

#if STATS_MAC_MEMORY

	UInt32			currentItem;
	char			hexString[11];
	char			outString[ 1024 ];
	PRFileHandle	outFile;
	
	outFile = PR_OpenFile ( "MemoryStats.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644 );
	if ( outFile == NULL )
	{
		return;
	}
	/* 
		There is a problem with the fixed size block data collection. That is that
		to collect these summary statistics, we have to put additional data (the block size)
		in the header for each block, and this adds another 4 bytes to every block! This
		messes up the data.
		
		Now, each fixed size allocator stores its performance data in the heap root global,
		so we can collect memory stats without adding any extra data to allocated blocks.
		But we still don't know how much of each block is used; we just know the blocksize,
		which is the requested size rounded up to a 4-byte boundary. So we can't know about
		wasted space within blocks.
	*/
	
	/*
	WriteString(outFile, "********************************************************************************\n");
	WriteString(outFile, "********************************************************************************\n");
	WriteString(outFile, "USAGE REPORT (MALLOC HEAP)\n");
	WriteString(outFile, "********************************************************************************\n");
	WriteString(outFile, "********************************************************************************\n");
	WriteString(outFile, "TYPE      BLOCKS (A)  BLOCKS (U)  BLOCKS (F)  BLOCKS (Max) MEMORY (A)  MEMORY (U)  MEM (Max)    EFFICIENCY\n");
	WriteString(outFile, "----      ----------  ----------  ----------  ------------ ----------  ----------  ----------   ----------\n");
	sprintf(outString, "FIXED   ");																	WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeBlocksAllocated);									WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeBlocksUsed);										WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeBlocksAllocated - gTotalFixedSizeBlocksUsed);		WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeBlocksMaxUsed);	WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeAllocated);	 	WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeUsed);	 		WriteString ( outFile, outString );
	sprintf(outString, "%10d  ", gTotalFixedSizeMaxUsed);	 	WriteString ( outFile, outString );
	
	PrintEfficiency(outFile, gTotalFixedSizeBlocksUsed, gTotalFixedSizeBlocksAllocated);
	sprintf(outString, "/"); WriteString ( outFile, outString );
	PrintEfficiency(outFile, gTotalFixedSizeUsed, gTotalFixedSizeAllocated);
	sprintf(outString, "  \n");	 WriteString ( outFile, outString );

	WriteAllocatationTableHeader(outFile, "BLOCK DISTRIBUTION (MALLOC HEAP: ACTIVE)", WRITE_TABLES_FLAT);
	WriteAllocationTableData(outFile, gSmallAllocationActiveCountTable, WRITE_TABLES_FLAT);
	
	WriteAllocatationTableHeader(outFile, "BLOCK DISTRIBUTION (MALLOC HEAP: MAX)", WRITE_TABLES_FLAT);
	WriteAllocationTableData(outFile, gSmallAllocationMaxCountTable, WRITE_TABLES_FLAT);
	
	WriteAllocatationTableHeader(outFile, "BLOCK DISTRIBUTION (MALLOC HEAP: TOTAL)", WRITE_TABLES_FLAT);
	WriteAllocationTableData(outFile, gSmallAllocationTotalCountTable, WRITE_TABLES_FLAT);
	*/
	
	WriteAllocatorStatsExplanations(outFile);

	WriteSmallHeapMemoryUsageData(outFile);
	WriteLargeHeapMemoryUsageData(outFile);
	DumpAllocatorMemoryStats(outFile);
	
#endif
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark LARGE BLOCK ALLOCATOR

FreeMemoryBlockDescriptor	StandardFreeDescriptor = { &LargeBlockFree, 0 };

#define	LARGE_BLOCK_FREE(b)		((b)->prev == NULL)
#define	LARGE_BLOCK_SIZE(b)		((UInt32) (b)->next - (UInt32) (b))
#define	LARGE_BLOCK_OVERHEAD	(sizeof(LargeBlockHeader) + MEMORY_BLOCK_TAILER_SIZE)
#define	LARGE_BLOCK_INC(b)		((LargeBlockHeader *) ((UInt32) (b) + LARGE_BLOCK_OVERHEAD))

void *LargeBlockAlloc(size_t blockSize, void *refcon)
{
	LargeBlockAllocationRoot *	root = (LargeBlockAllocationRoot *)refcon;
	LargeBlockAllocationChunk *	chunk = (LargeBlockAllocationChunk *) root->header.firstChunk;
	LargeBlockHeader *			blockHeader;
	LargeBlockHeader *			prevBlock;
	LargeBlockHeader *			freeBlock;
	size_t						freeBlockSize;
	size_t						allocSize;
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
	void *						stackCrawl[ kStackDepth ];
	
	GetCurrentStackTrace(stackCrawl);
#endif

#if INSTRUMENT_MAC_MEMORY
	InstUpdateHistogram(gLargeHeapHistogram, blockSize, 1);
#endif

	/* round the block size up to a multiple of four and add space for the header and trailer */
	allocSize = ( ( blockSize + 3 ) & ~3 ) + LARGE_BLOCK_OVERHEAD;
	
	/* find an allocation chunk that can hold our block */
	while ( chunk != NULL )
		{
		/* scan through the blocks in this chunk looking for a big enough free block */
		/* we never allocate the head block */
		prevBlock = chunk->head;
		blockHeader = prevBlock->next;
		
		do
			{
			if ( LARGE_BLOCK_FREE(blockHeader) )
				{
				freeBlockSize = LARGE_BLOCK_SIZE(blockHeader);
				if ( freeBlockSize >= allocSize )
					{
					break;
					}
				}
			
			prevBlock = blockHeader;
			blockHeader = blockHeader->next;
			}
		while ( blockHeader != NULL );
		
		/* if we found a block, go use it */
		if ( blockHeader != NULL )
			{
			break;
			}
		
		/* otherwise, go look at the next chunk */
		chunk = (LargeBlockAllocationChunk *) chunk->header.next;
		}
	
	if ( blockHeader != NULL )
		{
		chunk->header.usedBlocks++;

#if STATS_MAC_MEMORY
		gTotalLargeHeapBlocksUsed ++;
		if (gTotalLargeHeapBlocksUsed > gTotalLargeHeapBlocksMaxUsed)
			gTotalLargeHeapBlocksMaxUsed = gTotalLargeHeapBlocksUsed;
		
		gTotalLargeHeapUsed += allocSize;
		if (gTotalLargeHeapUsed > gTotalLargeHeapMaxUsed)
			gTotalLargeHeapMaxUsed = gTotalLargeHeapUsed;
#endif

#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
		EnableAllocationSet ( gLargeBlockAllocatorSet );
		TrackItem ( blockHeader, allocSize, 0, kPointerBlock, stackCrawl );
		DisableAllocationSet ( gLargeBlockAllocatorSet );
#endif
		
		/* is there space at the end of this block for a free block? */
		if ( ( freeBlockSize - allocSize ) > LARGE_BLOCK_OVERHEAD )
			{
			freeBlock = (LargeBlockHeader *) ( (char *) blockHeader + allocSize );
			freeBlock->prev = NULL;
			freeBlock->next = blockHeader->next;
			freeBlock->next->prev = freeBlock;
			blockHeader->next = freeBlock;
			}
		
		/* allocate this block */
                blockHeader->logicalSize = blockSize;
		blockHeader->prev = prevBlock;
		blockHeader->header.blockFreeRoutine = &chunk->header.freeDescriptor;
		
		chunk->totalFree -= LARGE_BLOCK_SIZE(blockHeader);
		
		return (void *)((char *)blockHeader + sizeof(LargeBlockHeader));
		}
	else
		{
		return NULL;
		}	
}

/* -------------------------------------------------------------------------------------*/
static void* LargeBlockGrow(LargeBlockHeader* blockHeader, size_t newSize, void* refcon)
{
        LargeBlockAllocationChunk*      chunk = (LargeBlockAllocationChunk *) refcon;
        LargeBlockAllocationRoot*       root = (LargeBlockAllocationRoot *) chunk->header.root;
        LargeBlockHeader*                       freeBlock = blockHeader->next;
        size_t                                          allocSize;
        size_t                                          oldAllocSize;
        size_t                                          freeBlockSize;
        
        /* is the block following this block a free block? */
        if (!LARGE_BLOCK_FREE(freeBlock))
                return NULL;
        
        /* round the block size up to a multiple of four and add space for the header and trailer */
        allocSize = ( ( newSize + 3 ) & ~3 ) + LARGE_BLOCK_OVERHEAD;
        oldAllocSize = LARGE_BLOCK_SIZE(blockHeader);

        /* is it big enough? */
        freeBlockSize = LARGE_BLOCK_SIZE(freeBlock);
        if (freeBlockSize + oldAllocSize < allocSize)
                return NULL;
        
        /* grow this block */
        PR_ASSERT(blockHeader->logicalSize < newSize);
        blockHeader->logicalSize = newSize;

#if STATS_MAC_MEMORY
        gTotalLargeHeapUsed += (allocSize - oldAllocSize);
        if (gTotalLargeHeapUsed > gTotalLargeHeapMaxUsed)
                gTotalLargeHeapMaxUsed = gTotalLargeHeapUsed;
#endif

        chunk->totalFree -= LARGE_BLOCK_SIZE(freeBlock);

        /* is there still space at the end of this block for a free block? */
        if ( freeBlockSize + oldAllocSize - allocSize > LARGE_BLOCK_OVERHEAD )
        {
                LargeBlockHeader* smallFree = (LargeBlockHeader *)((char*)blockHeader + allocSize);
                smallFree->prev = NULL;
                smallFree->next = freeBlock->next;
                smallFree->next->prev = smallFree;
#if DEBUG_HEAP_INTEGRITY
                smallFree->header.headerTag = kFreeBlockHeaderTag;
#endif
                blockHeader->next = smallFree;
                chunk->totalFree += LARGE_BLOCK_SIZE(smallFree);
        }
        else
        {
                blockHeader->next = freeBlock->next;
                freeBlock->next->prev = blockHeader;
        }
        
        return (void *)((char *)blockHeader + sizeof(LargeBlockHeader));
}       /* LargeBlockGrow */


/* -------------------------------------------------------------------------------------*/
static void* LargeBlockShrink(LargeBlockHeader* blockHeader, size_t newSize, void* refcon)
{
        LargeBlockAllocationChunk*      chunk = (LargeBlockAllocationChunk *)refcon;
        LargeBlockAllocationRoot*       root = (LargeBlockAllocationRoot *)chunk->header.root;

        /* round the block size up to a multiple of four and add space for the header and trailer */
        size_t                                          newAllocSize = ( ( newSize + 3 ) & ~3 ) + LARGE_BLOCK_OVERHEAD;
        size_t                                          oldAllocSize = LARGE_BLOCK_SIZE(blockHeader);
        LargeBlockHeader*                       nextBlock = blockHeader->next;
        LargeBlockHeader*                       smallFree = NULL;                       /* Where the recovered freeblock will go */

        /* shrink this block */
        PR_ASSERT(blockHeader->logicalSize > newSize);
        blockHeader->logicalSize = newSize;

#if STATS_MAC_MEMORY
        gTotalLargeHeapUsed -= (oldAllocSize - newAllocSize);
#endif

        /* is the block following this block a free block? */
        if (LARGE_BLOCK_FREE(nextBlock))
        {
                /* coalesce the freed space with the following free block */
                smallFree = (LargeBlockHeader *)((char *)blockHeader + newAllocSize);
                chunk->totalFree -= LARGE_BLOCK_SIZE(nextBlock);
                smallFree->next = nextBlock->next;
        }
        /* or is there enough space at the end of this block for a new free block? */
        else if ( oldAllocSize - newAllocSize > LARGE_BLOCK_OVERHEAD )
        {
                smallFree = (LargeBlockHeader *)((char *)blockHeader + newAllocSize);
                smallFree->next = nextBlock;
        }

        if (smallFree)
        {
                /* Common actions for both cases */
                smallFree->prev = NULL;
                smallFree->next->prev = smallFree;
                blockHeader->next = smallFree;
                chunk->totalFree += LARGE_BLOCK_SIZE(smallFree);
#if DEBUG_HEAP_INTEGRITY
                //      Fill recovered memory with the free memory pattern
                if (gFillFreeBlocksWithPattern)
                        memset(
                                (char *)(smallFree) + sizeof(LargeBlockHeader),
                                kFreeMemoryFillPattern,
                                LARGE_BLOCK_SIZE(smallFree) - sizeof(LargeBlockHeader) - sizeof(MemoryBlockTrailer)
                                );
                smallFree->header.headerTag = kFreeBlockHeaderTag;
#endif
        }
        
        return (void *)((char *)blockHeader + sizeof(LargeBlockHeader));
}       /* LargeBlockShrink */


/* -------------------------------------------------------------------------------------*/
void LargeBlockFree(void *block, void *refcon)
{
	LargeBlockAllocationChunk *	chunk = (LargeBlockAllocationChunk *) refcon;
	LargeBlockAllocationRoot *	root = (LargeBlockAllocationRoot *) chunk->header.root;
	LargeBlockHeader *			blockHeader;
	LargeBlockHeader *			prev;
	LargeBlockHeader *			next;
	
	blockHeader = (LargeBlockHeader *) ((char *)block - sizeof(LargeBlockHeader));

#if STATS_MAC_MEMORY
	gTotalLargeHeapBlocksUsed --;
	gTotalLargeHeapUsed -= LARGE_BLOCK_SIZE(blockHeader);
#endif

#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
	EnableAllocationSet ( gLargeBlockAllocatorSet );
	ReleaseItem ( blockHeader );
	DisableAllocationSet ( gLargeBlockAllocatorSet );
#endif
	
	/* we might want to coalese this block with it's prev or next neighbor */
	prev = blockHeader->prev;
	next = blockHeader->next;

	if ( LARGE_BLOCK_FREE(prev) )
		{
		chunk->totalFree -= LARGE_BLOCK_SIZE(prev);
		prev->next = blockHeader->next;
		blockHeader = prev;
		
		if ( LARGE_BLOCK_FREE(next) )
			{
			chunk->totalFree -= LARGE_BLOCK_SIZE(next);
			blockHeader->next = next->next;
			next->next->prev = blockHeader;
			}
		else
			{
			next->prev = blockHeader;
			}
		}
	else
	if ( LARGE_BLOCK_FREE(next) )
		{
		chunk->totalFree -= LARGE_BLOCK_SIZE(next);
		blockHeader->next = next->next;
		next->next->prev = blockHeader;
		}
	
	blockHeader->prev = NULL;

	chunk->totalFree += LARGE_BLOCK_SIZE(blockHeader);
	
	--chunk->header.usedBlocks;
	
	// if this chunk is completely empty and it's not the first chunk then free it
	if ( chunk->header.usedBlocks == 0 && root->header.firstChunk != (SubHeapAllocationChunk *) chunk )
	{
		FreeSubHeap ( &root->header, &chunk->header );
#if STATS_MAC_MEMORY
		gTotalLargeHeapChunksAllocated --;
		gTotalLargeHeapTotalChunkSize -= chunk->header.heapSize;
#endif
	}
}


SubHeapAllocationChunk * LargeBlockAllocChunk ( size_t blockSize, void * refcon )
{
	LargeBlockAllocationRoot *	root = (LargeBlockAllocationRoot *)refcon;
	UInt32						bestHeapSize;
	UInt32						smallestHeapSize;
	Boolean						useTemp;
	LargeBlockAllocationChunk *	chunk;
	LargeBlockHeader *			freeBlock;
#if DEBUG_ALLOCATION_CHUNKS && DEBUG_MAC_MEMORY
	void *						stackCrawl[ kStackDepth ];
	
	GetCurrentStackTrace(stackCrawl);
#endif
	
	useTemp = root->header.firstChunk != NULL;
	
	if ( useTemp )
		{
		bestHeapSize = root->idealTempChunkSize;
		smallestHeapSize = root->smallestTempChunkSize;
		}
	else
		{
		/*
		 * since we use total free memory here rather than the size of the biggest block, there is a slim
		 * chance this operation could fail. our heap should be very unfragmented at this point (we get called
		 * very early in the boot sequence) so this should be safe.
		 */
                bestHeapSize = ( root->baseChunkPercentage * FreeMem() ) / 100;
                bestHeapSize = ( ( bestHeapSize + 3 ) & ~3 );           // round up to a multiple of 4 bytes
                smallestHeapSize = bestHeapSize;
		}
	
	/* make sure that the heap will be big enough to accomodate our block (we have at least three block headers in a heap) */
        blockSize = ( ( blockSize + 3 ) & ~3 ) + 3 * LARGE_BLOCK_OVERHEAD;
	if ( smallestHeapSize < blockSize )
		{
		smallestHeapSize = blockSize;
		}
	
	if ( bestHeapSize < blockSize )
		{
		bestHeapSize = blockSize;
		}
	
	do	{
		chunk = (LargeBlockAllocationChunk *) AllocateSubHeap ( &root->header, bestHeapSize + sizeof(LargeBlockAllocationChunk),
			useTemp );
		
		/* if we failed, then try a smaller heap */
		if ( chunk == NULL )
			{
			bestHeapSize -= 64 * 1024;
			}
		}
	while ( ( chunk == NULL ) && ( bestHeapSize >= smallestHeapSize ) );
	
	/* if that failed, then try allocating the smallest chunk out of the application heap */
	/* call the fe low memory proc as well, cuz we're running out jim */
	if ( ( chunk == NULL ) && useTemp )
	{
		bestHeapSize = smallestHeapSize;
		
		chunk = (LargeBlockAllocationChunk *) AllocateSubHeap ( &root->header, bestHeapSize + sizeof(LargeBlockAllocationChunk),
			false );
		
		CallFE_LowMemory();
	}
		
	if ( chunk != NULL )
	{

#if STATS_MAC_MEMORY
		gTotalLargeHeapChunksAllocated ++;
		if (gTotalLargeHeapChunksAllocated > gTotalLargeHeapChunksMaxAllocated)
			gTotalLargeHeapChunksMaxAllocated = gTotalLargeHeapChunksAllocated;
			
		gTotalLargeHeapTotalChunkSize += bestHeapSize;
		if (gTotalLargeHeapTotalChunkSize > gTotalLargeHeapMaxTotalChunkSize)
			gTotalLargeHeapMaxTotalChunkSize = gTotalLargeHeapTotalChunkSize;
#endif

#if DEBUG_ALLOCATION_CHUNKS && DEBUG_MAC_MEMORY
		EnableAllocationSet ( gLargeBlockAllocatorSet );
		TrackItem ( chunk, bestHeapSize + sizeof(LargeBlockAllocationChunk), 0, kPointerBlock, stackCrawl );
		DisableAllocationSet ( gLargeBlockAllocatorSet );
#endif
		
		// mark how much we can actually store in the heap
		chunk->header.heapSize = bestHeapSize - 3 * sizeof(LargeBlockHeader);

		chunk->header.freeDescriptor.freeRoutine = LargeBlockFree;
		chunk->header.freeDescriptor.sizeRoutine = LargeBlockSize;
		chunk->header.freeDescriptor.refcon = chunk;
		
		/* the head block is zero size and is never free */
		chunk->head->prev = (LargeBlockHeader *) -1L;
		chunk->head->next = LARGE_BLOCK_INC(chunk->head);
		
		/* we have a free block in the middle */
		freeBlock = LARGE_BLOCK_INC(chunk->head);
		freeBlock->prev = NULL;
		freeBlock->next = (LargeBlockHeader *) ( (UInt32) freeBlock + bestHeapSize - 2 * LARGE_BLOCK_OVERHEAD );
		
		/* and then a zero sized allcated block at the end */
		chunk->tail = freeBlock->next;
		chunk->tail->next = NULL;
		chunk->tail->prev = freeBlock;

		chunk->totalFree = LARGE_BLOCK_SIZE(freeBlock);
		}
	
	return &chunk->header;
}


/* -------------------------------------------------------------------------------------*/
size_t LargeBlockSize(void *block, void *refcon)
{
#pragma unused(refcon)
        LargeBlockHeader*       blockHeader = (LargeBlockHeader *)((char *)block - sizeof(LargeBlockHeader));

        return blockHeader->logicalSize;
}


/* -------------------------------------------------------------------------------------*/
void LargeBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon)
{
#pragma unused(blockSize, refcon)
	LargeBlockAllocationRoot *	root = (LargeBlockAllocationRoot *)refcon;
	LargeBlockAllocationChunk *	chunk;
	uint32						freeBytes;
	uint32						totalBytes;
	uint32						maxBlock;
	uint32						curBlockSize;
	LargeBlockHeader *			blockList;
		
	freeBytes = 0;
	maxBlock = 0;
	totalBytes = 0;
	
	chunk = (LargeBlockAllocationChunk *) root->header.firstChunk;
	while ( chunk != NULL )
		{
		/* count up all the free blocks in this chunk */
		for ( blockList = chunk->head; blockList != NULL; blockList = blockList->next )
			{
			if ( LARGE_BLOCK_FREE(blockList) )
				{
				curBlockSize = LARGE_BLOCK_SIZE(blockList) - LARGE_BLOCK_OVERHEAD;
				
				freeBytes += curBlockSize;
				if ( maxBlock < curBlockSize )
					{
					maxBlock = curBlockSize;
					}
				}
			}
			
		totalBytes += chunk->header.heapSize;
		
		chunk = (LargeBlockAllocationChunk *) chunk->header.next;
		}
		
	stats->maxBlockSize = maxBlock;
	stats->totalHeapSize = totalBytes;
	stats->totalFreeBytes = freeBytes;
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark FIXED SIZED ALLOCATOR

void *FixedSizeAlloc(size_t blockSize, void *refcon)
{
#if !STATS_MAC_MEMORY
#pragma unused (blockSize)
#endif
	FixedSizeAllocationRoot *	root = (FixedSizeAllocationRoot *)refcon;
	FixedSizeAllocationChunk *	chunk = (FixedSizeAllocationChunk *) root->header.firstChunk;
	MemoryBlockHeader *			blockHeader;
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
	void *						stackCrawl[ kStackDepth ];
	
	GetCurrentStackTrace(stackCrawl);
#endif
	
	blockHeader = NULL;
	
	//	Try to find an existing chunk with a free block.
	while ( chunk != NULL )
		{
		if ( chunk->freeList != NULL )
			{
			break;
			}
		
		chunk = (FixedSizeAllocationChunk *) chunk->header.next;
		}
	
	if ( chunk != NULL )
		{
		blockHeader = (MemoryBlockHeader *) chunk->freeList;
		++chunk->header.usedBlocks;
		chunk->freeList = chunk->freeList->next;
		blockHeader->blockFreeRoutine = &chunk->header.freeDescriptor;
	
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
		EnableAllocationSet ( gFixedSizeAllocatorSet );
		TrackItem ( blockHeader, blockSize, 0, kPointerBlock, stackCrawl );
		DisableAllocationSet ( gFixedSizeAllocatorSet );
#endif

#if STATS_MAC_MEMORY
		//blockHeader->blockSize = root->blockSize;
		
		root->blocksUsed ++;
		if (root->blocksUsed > root->maxBlocksUsed)
			root->maxBlocksUsed = root->blocksUsed;
		
		root->blockSpaceUsed += root->blockSize;
		if (root->blockSpaceUsed > root->maxBlockSpaceUsed)
			root->maxBlockSpaceUsed = root->blockSpaceUsed;

		gTotalFixedSizeUsed += root->blockSize;
		
		if (gTotalFixedSizeUsed > gTotalFixedSizeMaxUsed)
			gTotalFixedSizeMaxUsed = gTotalFixedSizeUsed;
			
		gTotalFixedSizeBlocksUsed++;
		
		if (gTotalFixedSizeBlocksUsed > gTotalFixedSizeBlocksMaxUsed)
			gTotalFixedSizeBlocksMaxUsed = gTotalFixedSizeBlocksUsed;
		
#endif
	}
	else
	{
		return NULL;
	}

	return (void *)((char *)blockHeader + sizeof(MemoryBlockHeader));
	
}

void FixedSizeFree(void *block, void *refcon)
{
	FixedMemoryFreeBlockHeader *	blockHeader;
	FixedSizeAllocationChunk *		chunk;
	FixedSizeAllocationRoot *		root;
	
	//	Find the block header and the chunk
	
	blockHeader = (FixedMemoryFreeBlockHeader *)((char *)block - sizeof(MemoryBlockHeader));
	chunk = (FixedSizeAllocationChunk *) refcon;
	root = (FixedSizeAllocationRoot *) chunk->header.root;

#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
	EnableAllocationSet ( gFixedSizeAllocatorSet );
	ReleaseItem ( blockHeader );
	DisableAllocationSet ( gFixedSizeAllocatorSet );
#endif
	
	// put this block back on the free list
	blockHeader->next = chunk->freeList;
	chunk->freeList = blockHeader;
	chunk->header.usedBlocks--;
	
	// if this chunk is completely empty and it's not the first chunk then free it
	if ( chunk->header.usedBlocks == 0 && root->header.firstChunk != (SubHeapAllocationChunk *) chunk )
	{
		FreeSubHeap ( &root->header, &chunk->header );
#if STATS_MAC_MEMORY
		root->chunksAllocated --;
		root->totalChunkSize -= chunk->chunkSize;
		root->blocksAllocated -= chunk->numBlocks;
#endif
	}
	
#if STATS_MAC_MEMORY
	root->blocksUsed --;
	root->blockSpaceUsed -= root->blockSize;
	
	gTotalFixedSizeUsed -= root->blockSize;
	gTotalFixedSizeBlocksUsed--;
#endif
		
}

SubHeapAllocationChunk * FixedSizeAllocChunk ( size_t blockSize, void * refcon )
{
	FixedSizeAllocationRoot *			root = (FixedSizeAllocationRoot *)refcon;
	UInt32								chunkSize;
	FixedSizeAllocationChunk *			chunk;
	FixedMemoryFreeBlockHeader *		freePtr;
	FixedMemoryFreeBlockHeader *		nextFree;
	FixedMemoryFreeBlockHeader *		lastFree;
	UInt32								blockCount;
	UInt32								numBlocks;
	Boolean								useTemp;
#if DEBUG_ALLOCATION_CHUNKS && DEBUG_MAC_MEMORY
	void *								stackCrawl[ kStackDepth ];
	
	GetCurrentStackTrace(stackCrawl);
#endif
	
	chunk = NULL;
	
	/* if this size doesn't have a root yet, use real mems, otherwise do temp */
	if ( root->header.firstChunk == NULL )
		{
		useTemp = false;
		numBlocks = root->baseChunkBlockCount;
		}
	else
		{
		useTemp = true;
		numBlocks = root->tempChunkBlockCount;
		}
		
	blockSize = root->blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;
	chunkSize = blockSize * numBlocks + sizeof(FixedSizeAllocationChunk);
	
	chunk = (FixedSizeAllocationChunk *) AllocateSubHeap ( &root->header, chunkSize, useTemp );
	
	if ( chunk != NULL )
		{
#if DEBUG_ALLOCATION_CHUNKS && DEBUG_MAC_MEMORY
		EnableAllocationSet ( gFixedSizeAllocatorSet );
		TrackItem ( chunk, chunkSize, 0, kPointerBlock, stackCrawl );
		DisableAllocationSet ( gFixedSizeAllocatorSet );
#endif
		chunk->header.freeDescriptor.freeRoutine = FixedSizeFree;
		chunk->header.freeDescriptor.sizeRoutine = FixedBlockSize;
		chunk->header.freeDescriptor.refcon = chunk;
#if STATS_MAC_MEMORY
		chunk->chunkSize = chunkSize;
		chunk->numBlocks = numBlocks;
		
		root->blocksAllocated += numBlocks;
		if (root->blocksAllocated > root->maxBlocksAllocated )
			root->maxBlocksAllocated = root->blocksAllocated;
			 
		root->chunksAllocated ++;
		root->totalChunkSize += chunkSize;
		
		if (root->chunksAllocated > root->maxChunksAllocated)
			root->maxChunksAllocated = root->chunksAllocated;
		
		if (root->totalChunkSize > root->maxTotalChunkSize)
			root->maxTotalChunkSize = root->totalChunkSize;
			
		gTotalFixedSizeAllocated += chunkSize;
		gTotalFixedSizeBlocksAllocated += numBlocks;
#endif
					
		// mark how much we can actually store in the heap
		chunk->header.heapSize = numBlocks * root->blockSize;

		/* build the free list for this chunk */
		blockCount = numBlocks;
		
		freePtr = chunk->memory;
		chunk->freeList = freePtr;
		
		do	{
			lastFree = freePtr;
			nextFree = (FixedMemoryFreeBlockHeader *) ( (UInt32) freePtr + (UInt32) blockSize );
			freePtr->next = nextFree;
			freePtr = nextFree;
			}
		while ( --blockCount > 0 );
		
		lastFree->next = NULL;
		}
	
	return &chunk->header;
}

size_t FixedBlockSize (void *block, void *refcon)
{
#pragma unused(block)
	FixedSizeAllocationChunk *		chunk;
	FixedSizeAllocationRoot *		root;
		
	//	Find the block header and the chunk
	
	chunk = (FixedSizeAllocationChunk *) refcon;
	root = (FixedSizeAllocationRoot *) chunk->header.root;
	
	return root->blockSize;
}

void FixedBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon)
{
#pragma unused(blockSize)
	FixedSizeAllocationRoot *			root = (FixedSizeAllocationRoot *)refcon;
	FixedSizeAllocationChunk *			chunk;
	uint32								freeBlocks;
	uint32								totalBytes;
	FixedMemoryFreeBlockHeader *		freeList;
		
	freeBlocks = 0;
	totalBytes = 0;
	
	chunk = (FixedSizeAllocationChunk *) root->header.firstChunk;
	while ( chunk != NULL )
		{
		/* count up the free blocks */
		for ( freeList = chunk->freeList; freeList != NULL; freeList = freeList->next )
			{
			++freeBlocks;
			}
		
		totalBytes += chunk->header.heapSize;
		
		chunk = (FixedSizeAllocationChunk *) chunk->header.next;
		}
	
	stats->maxBlockSize = root->blockSize;
	stats->totalHeapSize = totalBytes;
	stats->totalFreeBytes = freeBlocks * root->blockSize;
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark SMALL HEAP ALLOCATOR

#define SmallHeapBlockToMemoryPtr(x) ((void *)((Ptr)x + sizeof(SmallHeapBlock)))
#define MemoryPtrToSmallHeapBlock(x) ((SmallHeapBlock *)((Ptr)x - sizeof(SmallHeapBlock)))

#define NextSmallHeapBlock(x) ((SmallHeapBlock *)((Ptr)x + (x->blockSize & ~kBlockInUseFlag) + sizeof(SmallHeapBlock) + MEMORY_BLOCK_TAILER_SIZE))
#define PrevSmallHeapBlock(x) (x->prevBlock)

#define SmallHeapBlockInUse(x) ((x->blockSize & kBlockInUseFlag) != 0)
#define SmallHeapBlockNotInUse(x) ((x->blockSize & kBlockInUseFlag) == 0)

#define TotalSmallHeapBlockUsage(x) ((x->blockSize & ~kBlockInUseFlag) + sizeof(SmallHeapBlock) + MEMORY_BLOCK_TAILER_SIZE)

#define AddSmallHeapBlockToFreeList(r, x)														\
{																								\
	UInt32				blockSize = x->blockSize & ~kBlockInUseFlag;							\
	UInt32				nextBlockBin;															\
	SmallHeapBlock		*tempBlock;																\
	x->blockSize = blockSize		;															\
	nextBlockBin = (blockSize - kDefaultSmallHeadMinSize) >> 2;									\
	x->info.freeInfo.prevFree = NULL;															\
	if (blockSize > kMaximumBinBlockSize) {														\
		tempBlock = r->overflow;																\
		x->info.freeInfo.nextFree = tempBlock;													\
		if (tempBlock != NULL)																	\
			tempBlock->info.freeInfo.prevFree = x;												\
		r->overflow = x;																		\
	}																							\
	else {																						\
		tempBlock = r->bins[nextBlockBin];														\
		x->info.freeInfo.nextFree = tempBlock;													\
		if (tempBlock != NULL)																	\
			tempBlock->info.freeInfo.prevFree = x;												\
		r->bins[nextBlockBin] = x;																\
	}																							\
}

#define RemoveSmallHeapBlockFromFreeList(r, x)													\
{																								\
	SmallHeapBlock		*nextFree,																\
						*prevFree;																\
	UInt32				blockSize = x->blockSize;												\
	UInt32				nextBlockBin;															\
	x->blockSize |= kBlockInUseFlag;															\
	nextBlockBin = (blockSize - kDefaultSmallHeadMinSize) >> 2;									\
	nextFree = x->info.freeInfo.nextFree;														\
	prevFree = x->info.freeInfo.prevFree;														\
	if (nextFree != NULL)																		\
		nextFree->info.freeInfo.prevFree = prevFree;											\
	if (prevFree != NULL)																		\
		prevFree->info.freeInfo.nextFree = nextFree;											\
	else																						\
		if (blockSize > kMaximumBinBlockSize)													\
			r->overflow = nextFree;																\
		else																					\
			r->bins[nextBlockBin] = nextFree;													\
}


#if DEBUG_HEAP_INTEGRITY
void SmallHeapCheck(SmallHeapBlock *fromBlock);
void SmallHeapCheck(SmallHeapBlock *fromBlock)
{
	SmallHeapBlock 		*previousBlock = NULL;
	
	//	Go to the first block in the list
	
	while (fromBlock->prevBlock != NULL)
		fromBlock = fromBlock->prevBlock;
	
	//	Walk the list
	
	while (fromBlock) {
	
		if (fromBlock->blockSize == 0xFFFFFFFF)
			break;
	
		if (previousBlock != NULL) {
			if (previousBlock != PrevSmallHeapBlock(fromBlock)) {
				DebugStr("\pSmall Heap Corrupt");
				break;
			}
		}
		previousBlock = fromBlock;
		fromBlock = NextSmallHeapBlock(fromBlock);
	
	}	

}
#endif

void *SmallHeapAlloc(size_t blockSize, void *refcon)
{
	SmallHeapRoot			*heapRoot = (SmallHeapRoot *)refcon;
	SmallHeapChunk			*chunk = (SmallHeapChunk *) heapRoot->header.firstChunk;
	UInt32					startingBinNum,
							remainingBins;
	SmallHeapBlock			**currentBin;
	SmallHeapBlock			*currentBinValue;
	SmallHeapBlock			*blockToCarve;
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
	void *					stackCrawl[ kStackDepth ];
	
	GetCurrentStackTrace(stackCrawl);
#endif

#if INSTRUMENT_MAC_MEMORY
	InstUpdateHistogram(gSmallHeapHistogram, blockSize, 1);
#endif

	//	Round up allocation to nearest 4 bytes.
	
	blockSize = (blockSize + 3) & 0xFFFFFFFC;

tryAlloc:

	// walk through all of our chunks, trying to allocate memory from somewhere
	while ( chunk != NULL )
		{
		//	Try to find the best fit in one of the bins.
		
		startingBinNum = (blockSize - kDefaultSmallHeadMinSize) >> 2;
		
		currentBin = chunk->bins + startingBinNum;
		currentBinValue = *currentBin;
			
		//	If the current bin has something in it,
		//	then use it for our allocation.
		
		if (currentBinValue != NULL) {
			
			RemoveSmallHeapBlockFromFreeList(chunk, currentBinValue);

			currentBinValue->info.inUseInfo.freeProc.blockFreeRoutine = &chunk->header.freeDescriptor;

	#if STATS_MAC_MEMORY
			gTotalSmallHeapBlocksUsed++;
			if (gTotalSmallHeapBlocksUsed > gTotalSmallHeapBlocksMaxUsed)
				gTotalSmallHeapBlocksMaxUsed = gTotalSmallHeapBlocksUsed;
				
			gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(currentBinValue);
			
			if (gTotalSmallHeapUsed > gTotalSmallHeapMaxUsed)
				gTotalSmallHeapMaxUsed = gTotalSmallHeapUsed;
#endif
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
			EnableAllocationSet ( gSmallHeapAllocatorSet );
			TrackItem ( currentBinValue, blockSize, 0, kPointerBlock, stackCrawl );
			DisableAllocationSet ( gSmallHeapAllocatorSet );
#endif

			chunk->header.usedBlocks ++;
			
			return SmallHeapBlockToMemoryPtr(currentBinValue);

		}

		//	Otherwise, try to carve up an existing larger block.

		remainingBins = kDefaultSmallHeapBins - startingBinNum - 1;
		blockToCarve = NULL;
		
		while (remainingBins--) {
		
			currentBin++;
		
			currentBinValue = *currentBin;
			
			if (currentBinValue != NULL) {
				blockToCarve = currentBinValue;
				break;
			}
			
		}

		//	Try carving up up a block from the overflow bin.
		
		if (blockToCarve == NULL)
			blockToCarve = chunk->overflow;
		
	doCarve:
		
		while (blockToCarve != NULL) {
		
			SInt32	blockToCarveSize = blockToCarve->blockSize;
		
			//	If the owerflow block is big enough to house the
			//	allocation, then use it.
		
			if (blockToCarveSize >= blockSize) {
			
				SInt32				leftovers;
			
				//	Remove the block from the free list... we will
				//	be using it for the allocation...
				
				RemoveSmallHeapBlockFromFreeList(chunk, blockToCarve);

				//	If taking our current allocation out of
				//	the overflow block would still leave enough
				//	room for another allocation out of the 
				//	block, then split the block up.
				
				leftovers = blockToCarveSize - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE - blockSize;
				
				if (leftovers >= kDefaultSmallHeadMinSize) {
				
					SmallHeapBlock		*leftoverBlock;
					SmallHeapBlock		*nextBlock;
				
					nextBlock = NextSmallHeapBlock(blockToCarve);
					
					//	Create a new block out of the leftovers
					
					leftoverBlock = (SmallHeapBlock *)((char *)blockToCarve + 
						sizeof(SmallHeapBlock) + blockSize + MEMORY_BLOCK_TAILER_SIZE); 
				
					//	Add the block to linked list of blocks in this raw
					//	allocation chunk.
				
					nextBlock->prevBlock = leftoverBlock;
					blockToCarve->blockSize = blockSize | kBlockInUseFlag;		
						
					leftoverBlock->prevBlock = blockToCarve;
					leftoverBlock->blockSize = leftovers;
					
					//	And add the block to a free list, which will either be
					//	one of the sized bins or the overflow list, depending
					//	on its size.
				
					AddSmallHeapBlockToFreeList(chunk, leftoverBlock);
							
				}
			
				blockToCarve->info.inUseInfo.freeProc.blockFreeRoutine = &chunk->header.freeDescriptor;

#if STATS_MAC_MEMORY
				gTotalSmallHeapBlocksUsed++;
				if (gTotalSmallHeapBlocksUsed > gTotalSmallHeapBlocksMaxUsed)
					gTotalSmallHeapBlocksMaxUsed = gTotalSmallHeapBlocksUsed;
					
				gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(blockToCarve);
				
				if (gTotalSmallHeapUsed > gTotalSmallHeapMaxUsed)
					gTotalSmallHeapMaxUsed = gTotalSmallHeapUsed;
#endif
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
				EnableAllocationSet ( gSmallHeapAllocatorSet );
				TrackItem ( blockToCarve, blockSize, 0, kPointerBlock, stackCrawl );
				DisableAllocationSet ( gSmallHeapAllocatorSet );
#endif
				chunk->header.usedBlocks ++;
				
				return SmallHeapBlockToMemoryPtr(blockToCarve);
							
			} 
		
			blockToCarve = blockToCarve->info.freeInfo.nextFree;
		}
	
		chunk = (SmallHeapChunk *) chunk->header.next;
        }       // while (chunk != NULL)
                
        return NULL;
}

#ifdef GROW_SMALL_BLOCKS
static void* SmallBlockGrow(void* address, size_t newSize, void* refcon)
{
        SmallHeapChunk                  *chunk = (SmallHeapChunk *)refcon;
        SmallHeapRoot                   *root = (SmallHeapRoot *)chunk->header.root;
        SmallHeapBlock                  *growBlock = MemoryPtrToSmallHeapBlock(address);
        SmallHeapBlock                  *blockToCarve = NextSmallHeapBlock(growBlock);

#if DEBUG_HEAP_INTEGRITY
        {
                MemoryBlockTag headerTag = growBlock->info.inUseInfo.freeProc.headerTag;
                if (headerTag == kFreeBlockHeaderTag)
                {
                        DebugStr("\pfastmem: attempt to grow a freed block");
                        return false;
                }
                else
                {
                        size_t exactOldSize = growBlock->info.inUseInfo.freeProc.blockSize;
                        MemoryBlockTrailer* blockTrailer = (MemoryBlockTrailer *)((char*)address + exactOldSize);
                        
                        if (headerTag != kUsedBlockHeaderTag || blockTrailer->trailerTag != kUsedBlockTrailerTag)
                        {
                                DebugStr("\pfastmem: attempt to grow illegal block");
                                return false;
                        }
                }
        }
#endif
        if (SmallHeapBlockNotInUse(blockToCarve))
        {
                //      Round up allocation to nearest 4 bytes.
                
                size_t                          blockSize = (newSize + 3) & 0xFFFFFFFC;
                size_t                          oldSize = (growBlock->blockSize & ~kBlockInUseFlag);
                size_t                          oldPhysicalSize = TotalSmallHeapBlockUsage(growBlock);
                size_t                          newPhysicalSize = sizeof(SmallHeapBlock) + MEMORY_BLOCK_TAILER_SIZE + blockSize;
                size_t                          blockToCarveSize;
                size_t                          blockToCarvePhysicalSize;
                SInt32                          leftovers;
                
                if (blockSize == oldSize)
                        return address;

                blockToCarveSize = blockToCarve->blockSize; // does not have "in use" flag set.
                blockToCarvePhysicalSize = TotalSmallHeapBlockUsage(blockToCarve);
                leftovers = (SInt32)oldPhysicalSize + blockToCarvePhysicalSize - newPhysicalSize;
                
                // enough space to grow into the free block?
                if (leftovers < 0)
                        return nil;

                //      Remove the block from the free list... we will
                //      be using it for the allocation...
                
                RemoveSmallHeapBlockFromFreeList(chunk, blockToCarve);

                //      If taking our current allocation out of
                //      the overflow block would still leave enough
                //      room for another allocation out of the 
                //      block, then split the block up.
                        
                if (leftovers >= sizeof(SmallHeapBlock) + kDefaultSmallHeadMinSize + MEMORY_BLOCK_TAILER_SIZE)
                {
                        //      Create a new block out of the leftovers
                        SmallHeapBlock* leftoverBlock = (SmallHeapBlock *)((char *)growBlock + newPhysicalSize);
                        SmallHeapBlock* nextBlock = NextSmallHeapBlock(blockToCarve);
                
                        //      Add the block to linked list of blocks in this raw
                        //      allocation chunk.
                
                        nextBlock->prevBlock = leftoverBlock;   
                        growBlock->blockSize = blockSize | kBlockInUseFlag; // rounded-up size, just as alloc does (?!)
                        leftoverBlock->prevBlock = growBlock;
                        leftoverBlock->blockSize = leftovers - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE;
                        
                        //      And add the block to a free list, which will either be
                        //      one of the sized bins or the overflow list, depending
                        //      on its size.
                
                        AddSmallHeapBlockToFreeList(chunk, leftoverBlock);                                      
                }
                else
                {
                        SmallHeapBlock* nextBlock = NextSmallHeapBlock(blockToCarve);
                        nextBlock->prevBlock = growBlock;
                        // If we're using the entire free block, then because growBlock->blockSize
                        // is used to calculate the start of the next block, it must be
                        // adjusted so that still does this.
                        growBlock->blockSize += blockToCarvePhysicalSize;
                }


        #if STATS_MAC_MEMORY                    
                gTotalSmallHeapUsed -= oldPhysicalSize;
                gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(growBlock);
                        
                if (gTotalSmallHeapUsed > gTotalSmallHeapMaxUsed)
                        gTotalSmallHeapMaxUsed = gTotalSmallHeapUsed;

        #endif

                return address;
        }

        return nil;
} // SmallBlockGrow
#endif // GROW_SMALL_BLOCKS

#ifdef GROW_SMALL_BLOCKS
static void* SmallBlockShrink(void* address, size_t newSize, void* refcon)
{
        SmallHeapChunk                  *chunk = (SmallHeapChunk *)refcon;
        SmallHeapRoot                   *root = (SmallHeapRoot *)chunk->header.root;
        SmallHeapBlock                  *shrinkBlock = MemoryPtrToSmallHeapBlock(address);

#if DEBUG_HEAP_INTEGRITY
        {
                MemoryBlockTag headerTag = shrinkBlock->info.inUseInfo.freeProc.headerTag;
                if (headerTag == kFreeBlockHeaderTag)
                {
                        DebugStr("\pfastmem: attempt to shrink a freed block");
                        return false;
                }
                else
                {
                        size_t exactOldSize = shrinkBlock->info.inUseInfo.freeProc.blockSize;
                        MemoryBlockTrailer* blockTrailer = (MemoryBlockTrailer *)((char*)address + exactOldSize);
                        
                        if (headerTag != kUsedBlockHeaderTag || blockTrailer->trailerTag != kUsedBlockTrailerTag)
                        {
                                DebugStr("\pfastmem: attempt to shrink illegal block");
                                return false;
                        }
                }
        }
#endif
        {
                SmallHeapBlock          *nextBlock = NextSmallHeapBlock(shrinkBlock);
                SmallHeapBlock          *leftoverBlock;
                size_t                          blockSize = (newSize + 3) & 0xFFFFFFFC;
                size_t                          oldSize = (shrinkBlock->blockSize & ~kBlockInUseFlag);
                size_t                          oldPhysicalSize;
                size_t                          newPhysicalSize;
                SInt32                          leftovers;

                if (blockSize == oldSize)
                        return address;

                oldPhysicalSize = TotalSmallHeapBlockUsage(shrinkBlock);
                newPhysicalSize = sizeof(SmallHeapBlock) + MEMORY_BLOCK_TAILER_SIZE + blockSize;
                leftovers = (SInt32)oldPhysicalSize - newPhysicalSize;

                PR_ASSERT(leftovers > 0);

                if (SmallHeapBlockNotInUse(nextBlock))
                {
                        // coalesce
                        leftovers += TotalSmallHeapBlockUsage(nextBlock); // augmented leftovers.
                        RemoveSmallHeapBlockFromFreeList(chunk, nextBlock);
                        nextBlock = NextSmallHeapBlock(nextBlock);
                }
                else
                {                       
                        // enough space to turn into a free block?
                        if (leftovers < sizeof(SmallHeapBlock) + kDefaultSmallHeadMinSize + MEMORY_BLOCK_TAILER_SIZE)
                                return address;
                }       

                //      Create a new block out of the leftovers (or augmented leftovers)
                leftoverBlock = (SmallHeapBlock *)((char *)shrinkBlock + newPhysicalSize);
        
                //      Add the block to linked list of blocks in this raw
                //      allocation chunk.
        
                nextBlock->prevBlock = leftoverBlock;   
                shrinkBlock->blockSize = blockSize | kBlockInUseFlag; // rounded-up size, just as alloc does (?!)
                leftoverBlock->prevBlock = shrinkBlock;
                leftoverBlock->blockSize = leftovers - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE;
                
                //      And add the block to a free list, which will either be
                //      one of the sized bins or the overflow list, depending
                //      on its size.
        
                AddSmallHeapBlockToFreeList(chunk, leftoverBlock);                                      

#if STATS_MAC_MEMORY                    
                gTotalSmallHeapUsed -= oldPhysicalSize;
                gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(shrinkBlock);
#endif

                return address;
        }

        return nil;
} // SmallBlockShrink
#endif // GROW_SMALL_BLOCKS

void SmallHeapFree(void *address, void *refcon)
{
	SmallHeapChunk			*chunk = (SmallHeapChunk *)refcon;
	SmallHeapRoot			*root = (SmallHeapRoot *)chunk->header.root;
	SmallHeapBlock			*deadBlock,
							*prevBlock,
							*nextBlock;
				
	deadBlock = MemoryPtrToSmallHeapBlock(address);

#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
	EnableAllocationSet ( gSmallHeapAllocatorSet );
	ReleaseItem ( deadBlock );
	DisableAllocationSet ( gSmallHeapAllocatorSet );
#endif

#if STATS_MAC_MEMORY
	gTotalSmallHeapBlocksUsed--;
	gTotalSmallHeapUsed -= TotalSmallHeapBlockUsage(deadBlock);
#endif

	//	If the block after us is free, then coalesce with it.
	
	nextBlock = NextSmallHeapBlock(deadBlock);
	prevBlock = PrevSmallHeapBlock(deadBlock);
	
	if (SmallHeapBlockNotInUse(nextBlock)) {	
		RemoveSmallHeapBlockFromFreeList(chunk, nextBlock);
		deadBlock->blockSize += TotalSmallHeapBlockUsage(nextBlock);
		nextBlock = NextSmallHeapBlock(nextBlock);
	}
	
	//	If the block before us is free, then coalesce with it.
	
	if (SmallHeapBlockNotInUse(prevBlock)) {
		RemoveSmallHeapBlockFromFreeList(chunk, prevBlock);
		prevBlock->blockSize = ((Ptr)nextBlock - (Ptr)prevBlock) - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE;
		AddSmallHeapBlockToFreeList(chunk, prevBlock);
		deadBlock = prevBlock;
	}
	
	else {	
		AddSmallHeapBlockToFreeList(chunk, deadBlock);
	}
	
	nextBlock->prevBlock = deadBlock;

	-- chunk->header.usedBlocks;
	PR_ASSERT(chunk->header.usedBlocks >= 0);
	
	// if this chunk is completely empty and it's not the first chunk then free it
	if ( chunk->header.usedBlocks == 0 && root->header.firstChunk != (SubHeapAllocationChunk *) chunk )
	{
		FreeSubHeap( &root->header, &chunk->header );
#if STATS_MAC_MEMORY
		gTotalSmallHeapChunksAllocated --;
		gTotalSmallHeapTotalChunkSize -= chunk->header.heapSize;
#endif
	}

}


SubHeapAllocationChunk * SmallHeapAllocChunk ( size_t blockSize, void * refcon )
{
#pragma unused(blockSize)
	SmallHeapRoot *			root = (SmallHeapRoot *)refcon;
	SmallHeapChunk *		chunk;
	Boolean					useTemp;
	Size					heapSize;
	SmallHeapBlock *		newFreeOverflowBlock;
	SmallHeapBlock *		newRawBlockHeader;
	SmallHeapBlock *		newRawBlockTrailer;
	UInt32					count;
#if DEBUG_ALLOCATION_CHUNKS && DEBUG_MAC_MEMORY
	void *					stackCrawl[ kStackDepth ];
	
	GetCurrentStackTrace(stackCrawl);
#endif
	
	// if we have a main chunk allocated already, use temp mem
	useTemp = root->header.firstChunk != NULL;
	
	// we allocate a bigger main chunk the first time
	if ( useTemp )
		{
		heapSize = root->tempChunkHeapSize;
		}
	else
		{
		heapSize = root->baseChunkHeapSize;
		}
	
	chunk = (SmallHeapChunk *) AllocateSubHeap ( &root->header, heapSize + sizeof(SmallHeapChunk), useTemp );
	if ( chunk != NULL )
		{
		// init the overflow and bin ptrs
		chunk->overflow = NULL;
		for ( count = 0; count < kDefaultSmallHeapBins; ++count )
			{
			chunk->bins[ count ] = NULL;
			}
		
		// mark how much we can actually store in the heap
		chunk->header.heapSize = heapSize - 3 * sizeof(SmallHeapBlock);
		
		newRawBlockHeader = chunk->memory;

		chunk->header.freeDescriptor.freeRoutine = SmallHeapFree;
		chunk->header.freeDescriptor.sizeRoutine = SmallBlockSize;
		chunk->header.freeDescriptor.refcon = chunk;

#if STATS_MAC_MEMORY
		gTotalSmallHeapTotalChunkSize += heapSize;
		
		if (gTotalSmallHeapTotalChunkSize > gTotalSmallHeapMaxTotalChunkSize)
			gTotalSmallHeapMaxTotalChunkSize = gTotalSmallHeapTotalChunkSize;
			
		gTotalSmallHeapChunksAllocated ++;
		if (gTotalSmallHeapChunksAllocated > gTotalSmallHeapChunksMaxAllocated)
			gTotalSmallHeapChunksMaxAllocated = gTotalSmallHeapChunksAllocated;
#endif

#if DEBUG_ALLOCATION_CHUNKS && DEBUG_MAC_MEMORY
		EnableAllocationSet ( gSmallHeapAllocatorSet );
		TrackItem ( newRawBlockHeader, heapSize, 0, kPointerBlock, stackCrawl );
		DisableAllocationSet ( gSmallHeapAllocatorSet );
#endif

		//	The first few bytes of the block are a dummy header
		//	which is a block of size zero that is always alloacted.  
		//	This allows our coalesce code to work without modification
		//	on the edge case of coalescing the first real block.

		newRawBlockHeader->prevBlock = NULL;
		newRawBlockHeader->blockSize = kBlockInUseFlag;
		newRawBlockHeader->info.inUseInfo.freeProc.blockFreeRoutine = NULL;
		
		newFreeOverflowBlock = NextSmallHeapBlock(newRawBlockHeader);
		
		newFreeOverflowBlock->prevBlock = newRawBlockHeader;
		newFreeOverflowBlock->blockSize = heapSize - 3 * sizeof(SmallHeapBlock) - 3 * MEMORY_BLOCK_TAILER_SIZE;

		//	The same is true for the last few bytes in the block 
		//	as well.
		
		newRawBlockTrailer = (SmallHeapBlock *)(((Ptr)newRawBlockHeader) + heapSize - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE);
		newRawBlockTrailer->prevBlock = newFreeOverflowBlock;
		newRawBlockTrailer->blockSize = kBlockInUseFlag | 0xFFFFFFFF;
		newRawBlockTrailer->info.inUseInfo.freeProc.blockFreeRoutine = NULL;
		
		AddSmallHeapBlockToFreeList(chunk, newFreeOverflowBlock);
		}
	
	return &chunk->header;
}

size_t SmallBlockSize (void *block, void *refcon)
{
#pragma unused(refcon)
	SmallHeapBlock			*blockHeader;
				
	blockHeader = MemoryPtrToSmallHeapBlock(block);
	
	return ( blockHeader->blockSize & ~kBlockInUseFlag );
}

void SmallBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon)
{
#pragma unused(blockSize)
	SmallHeapRoot *			root = (SmallHeapRoot *)refcon;
	SmallHeapChunk *		chunk;
	uint32					freeBytes;
	uint32					totalBytes;
	uint32					maxBlock;
	uint32					bin;
	uint32					curBlockSize;
	SmallHeapBlock *		freeList;
		
	freeBytes = 0;
	maxBlock = 0;
	totalBytes = 0;
	
	chunk = (SmallHeapChunk *) root->header.firstChunk;
	while ( chunk != NULL )
		{
		/* count up the free blocks in all of the bins */
		for ( bin = 0; bin < kDefaultSmallHeapBins; ++bin )
			{
			for ( freeList = chunk->bins[ bin ]; freeList != NULL; freeList = freeList->info.freeInfo.nextFree )
				{
				curBlockSize = freeList->blockSize & ~kBlockInUseFlag;
				
				freeBytes += curBlockSize;
				if ( maxBlock < curBlockSize )
					{
					maxBlock = curBlockSize;
					}
				}
			}
		
		/* add in the overflow block */
		freeBytes += chunk->overflow->blockSize & ~kBlockInUseFlag;
		
		totalBytes += chunk->header.heapSize;
		
		chunk = (SmallHeapChunk *) chunk->header.next;
		}
		
	stats->maxBlockSize = maxBlock;
	stats->totalHeapSize = totalBytes;
	stats->totalFreeBytes = freeBytes;
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark TRACKING MEMORY MANAGER MEMORY

#if DEBUG_MAC_MEMORY || STATS_MAC_MEMORY
#if GENERATINGPOWERPC

#if DEBUG_MAC_MEMORY
enum {
	uppNewHandleProcInfo = kRegisterBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(Handle)))
		 | REGISTER_RESULT_LOCATION(kRegisterA0)
		 | REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt16)))
		 | REGISTER_ROUTINE_PARAMETER(2, kRegisterD0, SIZE_CODE(sizeof(Size))),
	uppNewPtrProcInfo = kRegisterBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(Handle)))
		 | REGISTER_RESULT_LOCATION(kRegisterA0)
		 | REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt16)))
		 | REGISTER_ROUTINE_PARAMETER(2, kRegisterD0, SIZE_CODE(sizeof(Size))),
	uppDisposeHandleProcInfo = kRegisterBased
		 | REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt16)))
		 | REGISTER_ROUTINE_PARAMETER(2, kRegisterA0, SIZE_CODE(sizeof(Handle))),
	uppDisposePtrProcInfo = kRegisterBased
		 | REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt16)))
		 | REGISTER_ROUTINE_PARAMETER(2, kRegisterA0, SIZE_CODE(sizeof(Ptr)))
};

extern pascal Handle NewHandlePatch(UInt16 trapWord, Size byteCount);
extern pascal Ptr NewPtrPatch(UInt16 trapWord, Size byteCount);
extern pascal void DisposeHandlePatch(UInt16 trapWord, Handle deadHandle);
extern pascal void DisposePtrPatch(UInt16 trapWord, Ptr deadPtr);

RoutineDescriptor	gNewHandlePatchRD = BUILD_ROUTINE_DESCRIPTOR(uppNewHandleProcInfo, &NewHandlePatch);
RoutineDescriptor	gNewPtrPatchRD = BUILD_ROUTINE_DESCRIPTOR(uppNewPtrProcInfo, &NewPtrPatch);
RoutineDescriptor	gDisposeHandlePatchRD = BUILD_ROUTINE_DESCRIPTOR(uppDisposeHandleProcInfo, &DisposeHandlePatch);
RoutineDescriptor	gDisposePtrPatchRD = BUILD_ROUTINE_DESCRIPTOR(uppDisposePtrProcInfo, &DisposePtrPatch);

UniversalProcPtr	gNewHandlePatchCallThru = NULL;
UniversalProcPtr 	gNewPtrPatchCallThru = NULL;
UniversalProcPtr	gDisposeHandlePatchCallThru = NULL;
UniversalProcPtr 	gDisposePtrPatchCallThru = NULL;



extern pascal Handle NewHandlePatch(UInt16 trapWord, Size byteCount)
{
	void			*stackCrawl[kStackDepth];
	Handle			resultHandle;
	
	GetCurrentStackTrace(stackCrawl);

	resultHandle = (Handle)CallOSTrapUniversalProc(gNewHandlePatchCallThru, uppNewHandleProcInfo, trapWord, byteCount);

	if (resultHandle && DEBUG_TRACK_MACOS_MEMS) {
		TrackItem ( resultHandle, byteCount, 0, kHandleBlock, stackCrawl );
		gOutstandingHandles++;
	}
	
	return resultHandle;
	
}

extern pascal Ptr NewPtrPatch(UInt16 trapWord, Size byteCount)
{	
	void			*stackCrawl[kStackDepth];
	Ptr				resultPtr;

	GetCurrentStackTrace(stackCrawl);
	
	resultPtr = (Ptr)CallOSTrapUniversalProc(gNewPtrPatchCallThru, uppNewPtrProcInfo, trapWord, byteCount);

	if (resultPtr && DEBUG_TRACK_MACOS_MEMS) {
		TrackItem ( resultPtr, byteCount, 0, kPointerBlock, stackCrawl );
		gOutstandingPointers++;
	}
	
	return resultPtr;
	
}

pascal void DisposeHandlePatch(UInt16 trapWord, Handle deadHandle)
{
#if DEBUG_TRACK_MACOS_MEMS
	ReleaseItem(deadHandle);
#endif
	gOutstandingHandles--;

	CallOSTrapUniversalProc(gDisposeHandlePatchCallThru, uppDisposeHandleProcInfo, trapWord, deadHandle);
}

pascal void DisposePtrPatch(UInt16 trapWord, Ptr deadPtr)
{
#if DEBUG_TRACK_MACOS_MEMS
	ReleaseItem(deadPtr);
#endif
	gOutstandingPointers--;

	CallOSTrapUniversalProc(gDisposePtrPatchCallThru, uppDisposePtrProcInfo, trapWord, deadPtr);
}

#endif /* DEBUG_MAC_MEMORY */

void InstallMemoryManagerPatches(void)
{
#if DEBUG_MAC_MEMORY
	OSErr err = Gestalt ( gestaltLogicalRAMSize, (long *) &gMemoryTop );
	if ( err != noErr )
		{
		DebugStr ( "\pThis machine has no logical ram?" );
		ExitToShell();
		}

	gNewHandlePatchCallThru = GetOSTrapAddress(0x0122);
	SetOSTrapAddress(&gNewHandlePatchRD, 0x0122);
	
	gNewPtrPatchCallThru = GetOSTrapAddress(0x011E);
	SetOSTrapAddress(&gNewPtrPatchRD, 0x011E);

	gDisposeHandlePatchCallThru = GetOSTrapAddress(0x023);
	SetOSTrapAddress(&gDisposeHandlePatchRD, 0x023);

	gDisposePtrPatchCallThru = GetOSTrapAddress(0x001F);
	SetOSTrapAddress(&gDisposePtrPatchRD, 0x001F);

	/* init the tracker itself and then add our block type decoders */
	InitializeMemoryTracker();
	SetTrackerDataDecoder ( kMallocBlock, PrintStackCrawl );
	SetTrackerDataDecoder ( kHandleBlock, PrintStackCrawl );
	SetTrackerDataDecoder ( kPointerBlock, PrintStackCrawl );

#endif /* DEBUG_MAC_MEMORY */

	gExitToShellPatchCallThru = GetToolboxTrapAddress(0x01F4);
	SetToolboxTrapAddress((UniversalProcPtr)&gExitToShellPatchRD, 0x01F4);
}

#else

void InstallMemoryManagerPatches(void)
{
	
}

#endif
#endif


#if GENERATINGPOWERPC


#if (DEBUG_MAC_MEMORY || STATS_MAC_MEMORY)

/*
 * Stuff for tracking memory leaks
 */
pascal void TrackerExitToShellPatch(void)
{
static Boolean gBeenHere = false;

	/* if we haven't been here, then dump our memory state, otherwise don't */
	/* this way we don't hose ourselves if we crash in here */
	if ( !gBeenHere )
		{
		gBeenHere = true;
		
#if DEBUG_MAC_MEMORY
		DumpMemoryTrackerState();
#endif

#if STATS_MAC_MEMORY
		DumpMemoryStats();
#endif
	}

#if DEBUG_MAC_MEMORY
	ExitMemoryTracker();
#endif

#if GENERATINGCFM
	CallUniversalProc(gExitToShellPatchCallThru, uppExitToShellProcInfo);
#else 
	{
		ExitToShellProc	*exitProc = (ExitToShellProc *)&gExitToShellPatchCallThru;
		(*exitProc)();
	}
#endif	/* GENERATINGCFM */
}

#if DEBUG_MAC_MEMORY
static void PrintStackCrawl ( void ** stackCrawl, char * name )
{
	unsigned long		currentStackLevel;
	
	*name = 0;
	
	for (currentStackLevel = 0; currentStackLevel < kStackDepth; currentStackLevel++)
		{
		CatenateRoutineNameFromPC ( name, stackCrawl[ currentStackLevel ] );
		
		if (currentStackLevel < kStackDepth - 1)
			strcat( name, "," );
		}
}

static void CatenateRoutineNameFromPC( char * string, void *currentPC )
{
	UInt32		instructionsToLook = kMaxInstructionScanDistance;
	UInt32		*currentPCAsLong = (UInt32 *)currentPC;
	UInt32		offset = 0;
	char		labelString[ 512 ];
	char		lameString[ 512 ];
	
	if (currentPC == NULL)
		return;

	/* make sure we're not out in the weeds 
	if ( (unsigned long) currentPC > (unsigned long) gMemoryTop )
		return;
	*/
	
	if (GetPageState(currentPC) != kPageInMemory)
		return;
	
	if ( (unsigned long) currentPC & 0x3 )
		return;
	
	*labelString = 0;
	*lameString = 0;
	
	while (instructionsToLook--) {
	
		if (*currentPCAsLong == 0x4E800020) {
			char	stringLength = *((char *)currentPCAsLong + 21);
			
			if (stringLength > 0)
			{
				BlockMoveData( ((char *)currentPCAsLong + 22), labelString, stringLength );
				labelString[stringLength] = '\0';
			}
			// if there was no routine name, then just put down the address.
			if ( *labelString == 0 )
				{
				PrintHexNumber( lameString, (unsigned long) currentPC );
				goto bail;
				}
			else
				{
				strcat ( lameString, labelString );
				strcat ( lameString, "+" );
				}
			break;
		}
	
		currentPCAsLong++;
	}
	
	instructionsToLook = kMaxInstructionScanDistance;
	currentPCAsLong = (UInt32 *)currentPC;
	
	*labelString = 0;
		
	while (instructionsToLook--) {
		if (*currentPCAsLong == 0x7C0802A6) {

			PrintHexNumber( labelString, offset - 4 );
			strcat ( lameString, labelString );
			break;
		}
		currentPCAsLong--;
		offset += 4;
	}

bail:	
	/* make sure we don't end up being too big */
	lameString[ kNameMaxLength ] = 0;
	strcat ( string, lameString );
}
#endif	/* DEBUG_MAC_MEMORY */

#endif	/* (DEBUG_MAC_MEMORY || STATS_MAC_MEMORY) */


asm void *GetCurrentStackPointer()
{
	mr		r3, sp
	lwz		r3, 0(r3)
	blr
}


void GetCurrentStackTrace(void **stackCrawl)
{
#if DEBUG_MAC_MEMORY
	void *		currentSP;
	void *		interfaceLibSP;
	void *		callerFirstStackFrame;
	void *		callerSP;
	UInt32		stackFrameLevel;
	UInt8		isPowerPC = true;
	void *		nextFrame;
	long		nativeFrames;
	PRThread *	curThread;
	void *		stackMin;
	void *		stackMax;
		
	memset(stackCrawl, 0, sizeof(void *) * kStackDepth);
	
#if !GENERATINGPOWERPC
	return;
#endif

	curThread = PR_CurrentThread();
	if ( curThread != NULL && curThread->stack->stackBottom != 0 )
		{
		stackMin = curThread->stack->stackBottom;
		stackMax = curThread->stack->stackTop;
		}
	else
		{
		stackMin = gOurApplicationHeapBase;
		stackMax = gOurApplicationHeapMax;
		}
 
	//	If the current SP is not in our heap, then bail (OT ).
	
#if GENERATINGPOWERPC
	currentSP = GetCurrentStackPointer();
#endif

	if ((currentSP > gOurApplicationHeapMax) || (currentSP < gOurApplicationHeapBase))
		return;
		
	interfaceLibSP = *((void **)currentSP);
	
	callerFirstStackFrame = interfaceLibSP;
	nextFrame = callerFirstStackFrame;
		
	//	What we really want to do here is walk up the stack until we get to a native frame that is in
	//	one of our Shared libraries... Since they can live in temp mem, this gets whacky.
	
	//	So, for now, I just walk up until I hit two native frames in a row...
	nativeFrames = 0;
	
	while (1) {
		// if this frame is outside of our thread's stack, then bail
		if ( ( nextFrame > stackMax ) || ( nextFrame < stackMin ) )
			{
			return;
			}
				
		//	Walk the stack differently whether we are at a
		//	PowerPC or 68K frame...

		if (isPowerPC) {

#if 0
			void	*framePC;

			//	If we are PowerPC, check to see if the PC
			//	corresponding to the current frame is in the
			//	the app's code space.  If so, then we are
			//	done and we break out.
			
			framePC = *((void **)callerFirstStackFrame + 2);
			if ((framePC < gOurApplicationHeapMax) && (framePC > gOurApplicationHeapBase))
				break;
#endif
				
			//	Otherwise, check the pointer to the next
			//	stack frame.  If its low bit is set, then
			//	it is a mixed-mode switch frame, and 
			//	we need to switch to 68K frames.

			nextFrame = *((void **)callerFirstStackFrame);
			
			isPowerPC = ((UInt32)nextFrame & 0x00000001) == 0;			
			callerFirstStackFrame = (void *)((UInt32)nextFrame & 0xFFFFFFFE);
			if ( isPowerPC != false )
				{
				if ( ++nativeFrames >= 1 )
					{
					break;
					}
				}
		}
	
		else {
			
			nativeFrames = 0;
			
			//	68K case:  If the location immediately above the stack
			//	frame pointer is -1, then we are at a switch frame,
			//	move back to PowerPC.

			if (*((UInt32 *)callerFirstStackFrame - 1) == 0xFFFFFFFF)
				isPowerPC = true;
			callerFirstStackFrame = *((void **)callerFirstStackFrame);
			nextFrame = callerFirstStackFrame;
		}
	
	}
	
	callerSP = callerFirstStackFrame;
	
	for (stackFrameLevel = 0; stackFrameLevel < kStackDepth; stackFrameLevel++) {
	
		if ( ( callerSP > stackMax ) || ( callerSP < stackMin ) )
			{
			return;
			}
			
		stackCrawl[stackFrameLevel] = *(((void **)callerSP) + 2);
		
		callerSP = *((void **)callerSP);
	}
#else
#pragma unused(stackCrawl)
#endif
}


void GetCurrentNativeStackTrace ( void ** stackCrawl )
{
#if DEBUG_MAC_MEMORY
	void *		currentSP;
	UInt32		stackFrameLevel;
	PRThread *	curThread;
	void *		stackMin;
	void *		stackMax;
	
	memset(stackCrawl, 0, sizeof(void *) * kStackDepth);
	
#if GENERATINGPOWERPC
	currentSP = GetCurrentStackPointer();
#else
	return;
#endif

	curThread = PR_CurrentThread();
	if ( curThread != NULL && curThread->stack->stackBottom != 0 )
		{
		stackMin = curThread->stack->stackBottom;
		stackMax = curThread->stack->stackTop;
		}
	else
		{
		stackMin = gOurApplicationHeapBase;
		stackMax = gOurApplicationHeapMax;
		}
		
	for (stackFrameLevel = 0; stackFrameLevel < kStackDepth; stackFrameLevel++) {
		if ( ( currentSP > stackMax ) || ( currentSP < stackMin ) )
			{
			return;
			}
			
		stackCrawl[stackFrameLevel] = *((void **)((char *)currentSP + 8));
		currentSP = *((void **)currentSP);
	}
#else
#pragma unused(stackCrawl)
#endif	/* DEBUG_MAC_MEMORY */
}

#endif /* GENERATINGPOWERPC */
