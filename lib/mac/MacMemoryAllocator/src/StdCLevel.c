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
#include <OSUtils.h>
#include <Gestalt.h>
#include <Processes.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef NSPR20
#include "prmacos.h"
#include "prthread.h"
#endif

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

static void WriteString ( PRFileHandle file, char * string );
#endif

#include "MemoryTracker.h"

#define	DEBUG_ALLOCATION_CHUNKS	0
#define	DEBUG_MAC_ALLOCATORS	0
#define	DEBUG_DONT_TRACK_MALLOC	0
#define	DEBUG_TRACK_MACOS_MEMS	1

//##############################################################################
//##############################################################################
#pragma mark malloc DECLARATIONS

static Boolean	gInsideCacheFlush = false;

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark malloc DEBUG DECLARATIONS

#if STATS_MAC_MEMORY
UInt32 gSmallAllocationTotalCountTable[1025];
UInt32 gSmallAllocationActiveCountTable[1025];
UInt32 gSmallAllocationMaxCountTable[1025];
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

#if DEBUG_MAC_MEMORY || DEBUG_HEAP_INTEGRITY

UInt32				gOutstandingPointers = 0;
UInt32				gOutstandingHandles = 0;

#if DEBUG_MAC_MEMORY

#define	kMaxInstructionScanDistance	4096

AllocationSet *		gFixedSizeAllocatorSet = NULL;
AllocationSet *		gSmallHeapAllocatorSet = NULL;
AllocationSet *		gLargeBlockAllocatorSet = NULL;

void		*		gMemoryTop;

static void PrintStackCrawl ( void ** stackCrawl, char * name );
static void CatenateRoutineNameFromPC( char * string, void *currentPC );
static void PrintHexNumber( char * string, UInt32 hexNumber );
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

asm void AsmFree(void *);
asm void *AsmMalloc(size_t);

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
UInt32		gTotalFixedSizeAllocated = 0;
UInt32		gTotalFixedSizeUsed = 0;

UInt32		gTotalSmallHeapBlocksAllocated = 0;
UInt32		gTotalSmallHeapBlocksUsed = 0;
UInt32		gTotalSmallHeapAllocated = 0;
UInt32		gTotalSmallHeapUsed = 0;

#endif


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark malloc AND free

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
#if DEBUG_HEAP_INTEGRITY || DEBUG_MAC_MEMORY
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
	
#if DEBUG_HEAP_INTEGRITY
	if (gVerifyHeapOnEveryMalloc)
		VerifyMallocHeapIntegrity();
		
	if (gFailToAllocateAfterNMallocs != -1) {
		if (gFailToAllocateAfterNMallocs-- == 0)
			return NULL;
	}
#endif

#if STATS_MAC_MEMORY

	if (blockSizeCopy >= 1024)
		blockSizeCopy = 1024;

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
		/* we need to ensure no one else tries to flush cache's while we do */
		/* This may mean that we allocate temp mem while flushing the cache that we would */
		/* have had space for after the flush, but life sucks... */
		
		if ( gInsideCacheFlush == false )
			{			
			/* we didn't get it. Flush some caches and try again */
			gInsideCacheFlush = true;
			CallCacheFlushers ( blockSize );
			gInsideCacheFlush = false;
			
			newBlock = (char *)(whichAllocator->blockAllocRoutine)(blockSize, whichAllocator->root);
			}

		/* if we still don't have it, try allocating a new chunk */
		if ( newBlock == NULL )
			{
			if ( (whichAllocator->chunkAllocRoutine)( blockSize, whichAllocator->root ) != NULL )
				{
				newBlock = (char *)(whichAllocator->blockAllocRoutine)(blockSize, whichAllocator->root);
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
#if DEBUG_HEAP_INTEGRITY || STATS_MAC_MEMORY
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
	
	deadBlockBase = ((char *)deadBlock - sizeof(MemoryBlockHeader));

	deadBlockHeader = (MemoryBlockHeader *)deadBlockBase;

#if DEBUG_HEAP_INTEGRITY || STATS_MAC_MEMORY
	deadBlockSize = deadBlockHeader->blockSize;
#endif
#if DEBUG_HEAP_INTEGRITY
	deadBlockTrailer = (MemoryBlockTrailer *)(deadBlockBase + deadBlockSize + sizeof(MemoryBlockHeader));
#endif
	
#if STATS_MAC_MEMORY
	if (deadBlockSize >= 1024)
		deadBlockSize = 1024;
	gSmallAllocationActiveCountTable[deadBlockSize]--;
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

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark realloc AND calloc

void* reallocSmaller(void* block, size_t newSize)
{
#pragma unused (newSize)

	// This actually doesn't reclaim memory!
	return block;
}

void* realloc(void* block, size_t newSize)
{
	void 		*newBlock = NULL;	
	UInt8		tryAgain = true;
	
	newBlock = malloc(newSize);
	
	if (newBlock != NULL) {	
		BlockMoveData(block, newBlock, newSize);
		free(block);
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

	for (count = 0; count < 1025; count++)
		fprintf(statsFile, "%ld\n", gSmallAllocationTotalCountTable[count]);

	fprintf(statsFile, "Current Allocation Allocations\n");

	for (count = 0; count < 1025; count++)
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
	UInt32 	efficiency = current * 1000 / total;
	UInt32	efficiencyLow = efficiency / 10;
	char	string[ 256 ];
	
	efficiency = efficiency/10;
	
	sprintf(string, "%ld.%ld", efficiency, efficiencyLow);
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
static void WriteString ( PRFileHandle file, char * string )
{
	long	len;
	long	bytesWritten;
	
	len = strlen ( string );
	if ( len >= 1024 ) Debugger();
	bytesWritten = _OS_WRITE ( file, string, len );
	PR_ASSERT(bytesWritten == len);
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

	sprintf(outString, "********************************************************************************\n");	WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	WriteString ( outFile, outString );
	sprintf(outString, "USAGE REPORT (MALLOC HEAP)\n");															WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	WriteString ( outFile, outString );
	sprintf(outString, "TYPE      BLOCKS (A)  BLOCKS (U)  BLOCKS (F)  MEMORY (A)  MEMORY (U)  EFFICIENCY\n");	WriteString ( outFile, outString );
	sprintf(outString, "----      ----------  ----------  ----------  ----------  ----------  ----------\n");	WriteString ( outFile, outString );
	sprintf(outString, "FIXED   ");																			WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalFixedSizeBlocksAllocated);
	sprintf(outString, "%s  ", hexString);																		WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalFixedSizeBlocksUsed);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalFixedSizeBlocksAllocated - gTotalFixedSizeBlocksUsed);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalFixedSizeAllocated);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalFixedSizeUsed);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintEfficiency(outFile, gTotalFixedSizeBlocksUsed, gTotalFixedSizeBlocksAllocated);
	sprintf(outString, "/"); WriteString ( outFile, outString );
	PrintEfficiency(outFile, gTotalFixedSizeUsed, gTotalFixedSizeAllocated);
	sprintf(outString, "  \n");	 WriteString ( outFile, outString );
	sprintf(outString, "HEAP      "); WriteString ( outFile, outString );
	PrintHexNumber(hexString, 0); WriteString ( outFile, outString );
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalSmallHeapBlocksUsed);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, 0);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalSmallHeapAllocated);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintHexNumber(hexString, gTotalSmallHeapUsed);
	sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
	PrintEfficiency(outFile, gTotalSmallHeapUsed, gTotalSmallHeapAllocated);
	sprintf(outString, "  \nTotal efficiency: "); WriteString ( outFile, outString );
	PrintEfficiency(outFile, gTotalSmallHeapUsed + gTotalFixedSizeUsed, 
		gTotalSmallHeapAllocated + gTotalFixedSizeAllocated);
	sprintf(outString, "  \n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "BLOCK DISTRIBUTION (MALLOC HEAP: ACTIVE)\n"); WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n"); WriteString ( outFile, outString );
	sprintf(outString, "SIZE      +0x00       +0x01       +0x02      +0x03        TOTAL\n"); WriteString ( outFile, outString );
	sprintf(outString, "----      ----------  ----------  ----------  ----------  ----------\n"); WriteString ( outFile, outString );
	
	for (currentItem = 0; currentItem < 64; currentItem++) {
		PrintHexNumber(hexString, currentItem * 4);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationActiveCountTable[currentItem * 4]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationActiveCountTable[currentItem * 4 + 1]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationActiveCountTable[currentItem * 4 + 2]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationActiveCountTable[currentItem * 4 + 3]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationActiveCountTable[currentItem * 4] + 
			gSmallAllocationActiveCountTable[currentItem * 4 + 1] +
			gSmallAllocationActiveCountTable[currentItem * 4 + 2] +
			gSmallAllocationActiveCountTable[currentItem * 4 + 3]);
		sprintf(outString, "\n");	 WriteString ( outFile, outString );
	}
	sprintf(outString, "Blocks Over 1K:"); WriteString ( outFile, outString );
	PrintHexNumber(hexString, gSmallAllocationActiveCountTable[1024]);	 WriteString ( outFile, hexString );
	sprintf(outString, "\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "BLOCK DISTRIBUTION (MALLOC HEAP: MAX)\n"); WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n"); WriteString ( outFile, outString );
	sprintf(outString, "SIZE      +0x00       +0x01       +0x02      +0x03        TOTAL\n"); WriteString ( outFile, outString );
	sprintf(outString, "----      ----------  ----------  ----------  ----------  ----------\n"); WriteString ( outFile, outString );
	
	for (currentItem = 0; currentItem < 64; currentItem++) {
		PrintHexNumber(hexString, currentItem * 4);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationMaxCountTable[currentItem * 4]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationMaxCountTable[currentItem * 4 + 1]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationMaxCountTable[currentItem * 4 + 2]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationMaxCountTable[currentItem * 4 + 3]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationMaxCountTable[currentItem * 4] + 
			gSmallAllocationMaxCountTable[currentItem * 4 + 1] +
			gSmallAllocationMaxCountTable[currentItem * 4 + 2] +
			gSmallAllocationMaxCountTable[currentItem * 4 + 3]);
		sprintf(outString, "\n");	 WriteString ( outFile, outString );
	}
	sprintf(outString, "Blocks Over 1K:"); WriteString ( outFile, outString );
	PrintHexNumber(hexString, gSmallAllocationMaxCountTable[1024]);	 WriteString ( outFile, hexString );
	sprintf(outString, "\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "BLOCK DISTRIBUTION (MALLOC HEAP: TOTAL)\n"); WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n");	 WriteString ( outFile, outString );
	sprintf(outString, "********************************************************************************\n"); WriteString ( outFile, outString );
	sprintf(outString, "SIZE        +0x00       +0x01       +0x02      +0x03        TOTAL\n"); WriteString ( outFile, outString );
	sprintf(outString, "----        ----------  ----------  ----------  ----------  ----------\n"); WriteString ( outFile, outString );
	
	for (currentItem = 0; currentItem < 64; currentItem++) {
		PrintHexNumber(hexString, currentItem * 4);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationTotalCountTable[currentItem * 4]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationTotalCountTable[currentItem * 4 + 1]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationTotalCountTable[currentItem * 4 + 2]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationTotalCountTable[currentItem * 4 + 3]);
		sprintf(outString, "%s  ", hexString);	 WriteString ( outFile, outString );
		PrintHexNumber(hexString, gSmallAllocationTotalCountTable[currentItem * 4] + 
			gSmallAllocationTotalCountTable[currentItem * 4 + 1] +
			gSmallAllocationTotalCountTable[currentItem * 4 + 2] +
			gSmallAllocationTotalCountTable[currentItem * 4 + 3]);
		sprintf(outString, "\n");	 WriteString ( outFile, outString );
	}
	sprintf(outString, "Blocks Over 1K:"); WriteString ( outFile, outString );
	PrintHexNumber(hexString, gSmallAllocationTotalCountTable[1024]);
	sprintf(outString, "%s\n", hexString);	 WriteString ( outFile, outString );

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
		blockHeader->size = blockSize;
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


void LargeBlockFree(void *block, void *refcon)
{
	LargeBlockAllocationChunk *	chunk = (LargeBlockAllocationChunk *) refcon;
	LargeBlockAllocationRoot *	root = (LargeBlockAllocationRoot *) chunk->header.root;
	LargeBlockHeader *			blockHeader;
	LargeBlockHeader *			prev;
	LargeBlockHeader *			next;
	
	blockHeader = (LargeBlockHeader *) ((char *)block - sizeof(LargeBlockHeader));

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
		}
}


SubHeapAllocationChunk * LargeBlockAllocChunk ( size_t blockSize, void * refcon )
{
	LargeBlockAllocationRoot *	root = refcon;
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
		smallestHeapSize = bestHeapSize = ( root->baseChunkPercentage * FreeMem() ) / 100;
		}
	
	/* make sure that the heap will be big enough to accomodate our block (we have at least three block headers in a heap) */
	blockSize += 3 * LARGE_BLOCK_OVERHEAD;
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
		chunk = (LargeBlockAllocationChunk *) AllocateSubHeap ( &root->header, smallestHeapSize + sizeof(LargeBlockAllocationChunk),
			false );
		
		CallFE_LowMemory();
		}
		
	if ( chunk != NULL )
		{
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

size_t LargeBlockSize (void *block, void *refcon)
{
#pragma unused(refcon)
	LargeBlockHeader *			blockHeader;
		
	blockHeader = (LargeBlockHeader *) ((char *)block - sizeof(LargeBlockHeader));

	return blockHeader->size;
}

void LargeBlockHeapFree(size_t blockSize, FreeMemoryStats * stats, void * refcon)
{
#pragma unused(blockSize, refcon)
	LargeBlockAllocationRoot *	root = refcon;
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
#pragma unused (blockSize)
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
		gTotalFixedSizeUsed += root->blockSize;
		gTotalFixedSizeBlocksUsed++;
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
		}
	

#if STATS_MAC_MEMORY
	gTotalFixedSizeUsed -= root->blockSize;
	gTotalFixedSizeBlocksUsed--;
#endif
		
}

SubHeapAllocationChunk * FixedSizeAllocChunk ( size_t blockSize, void * refcon )
{
	FixedSizeAllocationRoot *			root = refcon;
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
	FixedSizeAllocationRoot *			root = refcon;
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
			gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(currentBinValue);
	#endif
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
			EnableAllocationSet ( gSmallHeapAllocatorSet );
			TrackItem ( currentBinValue, blockSize, 0, kPointerBlock, stackCrawl );
			DisableAllocationSet ( gSmallHeapAllocatorSet );
#endif

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
				gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(blockToCarve);
	#endif
#if DEBUG_MAC_ALLOCATORS && DEBUG_MAC_MEMORY
				EnableAllocationSet ( gSmallHeapAllocatorSet );
				TrackItem ( blockToCarve, blockSize, 0, kPointerBlock, stackCrawl );
				DisableAllocationSet ( gSmallHeapAllocatorSet );
#endif
				return SmallHeapBlockToMemoryPtr(blockToCarve);
							
			} 
		
			blockToCarve = blockToCarve->info.freeInfo.nextFree;
		}
	
	chunk = (SmallHeapChunk *) chunk->header.next;
	}
		
	return NULL;
}

void SmallHeapFree(void *address, void *refcon)
{
	SmallHeapChunk			*chunk = (SmallHeapChunk *)refcon;
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

}


SubHeapAllocationChunk * SmallHeapAllocChunk ( size_t blockSize, void * refcon )
{
#pragma unused(blockSize)
	SmallHeapRoot *			root = refcon;
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
	SmallHeapRoot *			root = refcon;
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

#if DEBUG_MAC_MEMORY
#if GENERATINGPOWERPC

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


void InstallMemoryManagerPatches(void)
{
	OSErr				err;
	
	err = Gestalt ( gestaltLogicalRAMSize, (long *) &gMemoryTop );
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
	
	gExitToShellPatchCallThru = GetToolboxTrapAddress(0x01F4);
	SetToolboxTrapAddress((UniversalProcPtr)&gExitToShellPatchRD, 0x01F4);
}

#else

void InstallMemoryManagerPatches(void)
{
	
}

#endif
#endif


#if DEBUG_MAC_MEMORY && GENERATINGPOWERPC

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
		DumpMemoryTrackerState();
		
#if STATS_MAC_MEMORY
		DumpMemoryStats();
#endif
		}

	ExitMemoryTracker();

#if GENERATINGCFM
	CallUniversalProc(gExitToShellPatchCallThru, uppExitToShellProcInfo);
#else 
	{
		ExitToShellProc	*exitProc = (ExitToShellProc *)&gExitToShellPatchCallThru;
		(*exitProc)();
	}
#endif
}


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

	/* make sure we're not out in the weeds */
	if ( (unsigned long) currentPC > (unsigned long) gMemoryTop )
		return;
		
	if ( (unsigned long) currentPC & 0x3 )
		return;
	
	*labelString = 0;
	*lameString = 0;
	
	while (instructionsToLook--) {
	
		if (*currentPCAsLong == 0x4E800020) {
			char	stringLength = *((char *)currentPCAsLong + 21);
			memset( labelString, 0, 512 );
			BlockMoveData( ((char *)currentPCAsLong + 22), labelString, stringLength );
			
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

#endif

#if GENERATINGPOWERPC

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
#endif
}

#endif

