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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fastmem.h"
#include "prmacos.h"
#include <OSUtils.h>
#include <Processes.h>

//##############################################################################
//##############################################################################
#pragma mark malloc DEBUG DECLARATIONS

#if STATS_MAC_MEMORY
UInt32 gSmallAllocationTotalCountTable[1025];
UInt32 gSmallAllocationActiveCountTable[1025];
UInt32 gSmallAllocationMaxCountTable[1025];
#endif

#if DEBUG_MAC_MEMORY

enum {
	kActiveSiteHashTableSize		= 128,
	kActiveSizeListSize				= 1024
};

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

MemoryBlockHeader			*gFirstAllocatedBlock = NULL;
MemoryBlockHeader			*gLastAllocatedBlock = NULL;

enum {
	kAllocationSiteMalloc			= 0,
	kAllocationSiteNewHandle		= 1,
	kAllocationSiteNewPtr			= 2
};

typedef struct AllocationSite AllocationSite;

struct AllocationSite {
	UInt32							type;
	AllocationSite					*next;
	void							*whoAllocated[kRecordingDepthOfStackLevels];
	UInt32							mallocs;
	UInt32							frees;
	UInt32							totalAllocations;
};

AllocationSite		*gActiveSizeHashTable[kActiveSiteHashTableSize];
AllocationSite		gActiveSizeList[kActiveSizeListSize];
AllocationSite		*gCurrentActiveSize = gActiveSizeList;
UInt32				gRemainingSitesInList = kActiveSizeListSize;

AllocationSite *AddAllocationSite(UInt32 allocationType, void **allocatorStack);
void AddBlockToActiveList(void *newBlock, AllocationSite *alocationSite);
void RemoveBlockFromActiveList(void *deadBlock);

UInt32				gOutstandingPointers = 0;
UInt32				gOutstandingHandles = 0;

#endif

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark malloc DEBUG CONTROL VARIABLES

#if DEBUG_MAC_MEMORY

UInt32						gVerifyHeapOnEveryMalloc			= 0;
UInt32						gVerifyHeapOnEveryFree				= 0;
UInt32						gFillUsedBlocksWithPattern			= 0;
UInt32						gFillFreeBlocksWithPattern			= 1;
UInt32						gTrackLeaks							= 0;
UInt32						gDontActuallyFreeMemory				= 0;
UInt32						gOnMallocFailureReturnDEADBEEF		= 0;
SInt32						gFailToAllocateAfterNMallocs		= -1;

SInt32						gReportActiveBlocks					= 1;
SInt32						gTagReferencedBlocks				= 0;			// for best effect, turn on gFillFreeBlocksWithPattern also
SInt32						gReportLeaks						= 1;

#endif

//##############################################################################
//##############################################################################

#if STATS_MAC_MEMORY

UInt32		gTotalFixedSizeCompactBlocksAllocated = 0;
UInt32		gTotalFixedSizeCompactBlocksUsed = 0;
UInt32		gTotalFixedSizeCompactAllocated = 0;
UInt32		gTotalFixedSizeCompactUsed = 0;

UInt32		gTotalFixedSizeFastBlocksAllocated = 0;
UInt32		gTotalFixedSizeFastBlocksUsed = 0;
UInt32		gTotalFixedSizeFastAllocated = 0;
UInt32		gTotalFixedSizeFastUsed = 0;

UInt32		gTotalSmallHeapBlocksAllocated = 0;
UInt32		gTotalSmallHeapBlocksUsed = 0;
UInt32		gTotalSmallHeapAllocated = 0;
UInt32		gTotalSmallHeapUsed = 0;


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark DEBUG malloc AND free

#endif

#if DEBUG_MAC_MEMORY || STATS_MAC_MEMORY

void		*gOurApplicationHeapBase;
void		*gOurApplicationHeapMax;

#if GENERATINGPOWERPC
asm void *GetCurrentStackPointer() 
{
	mr		r3, sp
	lwz		r3, 0(r3)
	blr
}
#endif

UInt32		gNextBlockNum = 0;

void *malloc(size_t blockSize)
{
	MemoryBlockHeader		*newBlockHeader;
#if DEBUG_MAC_MEMORY
	MemoryBlockTrailer		*newBlockTrailer;	
#endif
	size_t					newCompositeSize = blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;
	size_t					blockSizeCopy = blockSize;
	UInt32					stackIterator;
	void					*newBlock;
	void					*currentSP;

#if DEBUG_MAC_MEMORY

#if GENERATINGPOWERPC
	currentSP = GetCurrentStackPointer();
#endif

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

	if (blockSize <= kMaxTableAllocatedBlockSize) {
		AllocMemoryBlockDescriptor	*whichAllocator;
		whichAllocator = gFastMemSmallSizeAllocators + ((blockSize + 3) >> 2);
		newBlock = (char *)(whichAllocator->allocRoutine)(blockSize, whichAllocator->refcon) - sizeof(MemoryBlockHeader);
	}
	
	else {
		newBlock =  (char *)StandardAlloc(blockSize, 0) - sizeof(MemoryBlockHeader);
	}

#if DEBUG_MAC_MEMORY
	if (((Ptr)newBlock + sizeof(MemoryBlockHeader)) == NULL) {
		DebugStr("\pMALLOC FALUIRE");
		if (gOnMallocFailureReturnDEADBEEF)
			return (void *)0xDEADBEEF;
		else
			return NULL;
	}

	//	Fill in the blocks header.  This includes adding the 
	//	block to the used list.
	
	newBlockHeader = (MemoryBlockHeader *)newBlock;

	memset(newBlockHeader->whoAllocated, 0, sizeof(void *) * kRecordingDepthOfStackLevels);

#if GENERATINGPOWERPC
	for (stackIterator = 0; stackIterator < kRecordingDepthOfStackLevels; stackIterator++) {
		if ((currentSP < gOurApplicationHeapBase) || (currentSP > gOurApplicationHeapMax))
			break;
		newBlockHeader->whoAllocated[stackIterator] = *((void **)((char *)currentSP + 8));
		currentSP = *((void **)currentSP);
	}
#endif

	newBlockHeader->blockSize = blockSize;
	newBlockHeader->headerTag = kUsedBlockHeaderTag;
	newBlockHeader->blockNum = ++gNextBlockNum;
	
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

	//	If leak detection is on, then add this block to the 
	//	tracking list.
	
	if (gTrackLeaks)
		AddAllocationSite(kAllocationSiteMalloc, newBlockHeader->whoAllocated);
	
#endif

	return (void *)((char *)newBlock + sizeof(MemoryBlockHeader));
	
}

void free(void *deadBlock)
{
	MemoryBlockHeader			*deadBlockHeader;
#if DEBUG_MAC_MEMORY
	MemoryBlockTrailer			*deadBlockTrailer;
#endif
	size_t						deadBlockSize;	
	char						*deadBlockBase;
	FreeMemoryBlockDescriptor	*descriptor;
	
#if DEBUG_MAC_MEMORY
	if (gVerifyHeapOnEveryFree)
		VerifyMallocHeapIntegrity();
#endif
		
	if (deadBlock == NULL)
		return;
	
	deadBlockBase = ((char *)deadBlock - sizeof(MemoryBlockHeader));

	deadBlockHeader = (MemoryBlockHeader *)deadBlockBase;
	deadBlockSize = deadBlockHeader->blockSize;

#if DEBUG_MAC_MEMORY
	deadBlockTrailer = (MemoryBlockTrailer *)(deadBlockBase + deadBlockSize + sizeof(MemoryBlockHeader));
#endif
	
#if STATS_MAC_MEMORY
	if (deadBlockSize >= 1024)
		deadBlockSize = 1024;
	gSmallAllocationActiveCountTable[deadBlockSize]--;
#endif
	
#if DEBUG_MAC_MEMORY
	if (deadBlockHeader->headerTag == kFreeBlockHeaderTag)
		DebugStr("\pfastmem: double dispose of malloc block");
	else
		if ((deadBlockHeader->headerTag != kUsedBlockHeaderTag) || (deadBlockTrailer->trailerTag != kUsedBlockTrailerTag))
			DebugStr("\pfastmem: attempt to dispose illegal block");
	
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

	//	If we are tracking leaks, then decrement this blocks
	//	usage in the tracking table.
	
	if (gTrackLeaks) {

		UInt32				hashBin = ((UInt32)(deadBlockHeader->whoAllocated[0]) >> 2) % kActiveSiteHashTableSize;
		AllocationSite		*currentSite = gActiveSizeHashTable[hashBin];
		
		while (currentSite) {
			UInt32	currentStackLevel;
			for (currentStackLevel = 0; currentStackLevel < kRecordingDepthOfStackLevels; currentStackLevel++) {
				if (currentSite->whoAllocated[currentStackLevel] != deadBlockHeader->whoAllocated[currentStackLevel])
					break;
			}
			if (currentStackLevel == kRecordingDepthOfStackLevels)
				break;
			currentSite = currentSite->next;
		}
		
		if (currentSite != NULL)
			currentSite->frees++;

	}

	//	Fill with the free memory pattern
	
	if (gFillFreeBlocksWithPattern)
		memset(deadBlock, kFreeMemoryFillPattern, deadBlockSize);

	if (!gDontActuallyFreeMemory) {
		descriptor = deadBlockHeader->blockFreeRoutine;
		(descriptor->freeRoutine)(deadBlock, descriptor->refcon);
	}
	
#endif
	
}

#else

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark OPTIMIZED POWERPC malloc AND free

#if (defined(powerc) || defined(_powerc))

void *malloc(size_t blockSize)
{
	return AsmMalloc(blockSize);
}


asm void *AsmMalloc(size_t blockSize)
{
	cmplwi	r3, kMaxTableAllocatedBlockSize
	bgt		largeAlloc
	
	lwz		r6,gFastMemSmallSizeAllocators(RTOC)
	addi	r4,r3,3
	rlwinm	r4,r4,1,0,28
	add 	r6,r6,r4
	
	lwz		r5,0(r6)
	lwz		r5,0(r5)
	mtctr	r5
	lwz		r4,4(r6)

	bcctr	31, 0

largeAlloc:

	b		StandardAlloc
}


void free(void *block)
{
	AsmFree(block);
}


asm void AsmFree(void *)
{
	cmplwi	r3, 0
	beqlr
	
	subi	r5,r3,4
	lwz		r5,0(r5)
	lwz		r6,0(r5)
	lwz		r6,0(r6)
	mtctr	r6
	lwz		r4,4(r5)
	bcctr	31, 0
}

#else

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark STANDARD malloc AND free

void *malloc(size_t blockSize)
{

	if (blockSize <= kMaxTableAllocatedBlockSize) {
		AllocMemoryBlockDescriptor	*whichAllocator;
		whichAllocator = gFastMemSmallSizeAllocators + ((blockSize + 3) >> 2);
		return (whichAllocator->allocRoutine)(blockSize, whichAllocator->refcon);
	}
	
	else {
		return StandardAlloc(blockSize, 0);
	}
	
}

void free(void *block)
{
	MemoryBlockHeader			*blockHeader;
	FreeMemoryBlockDescriptor	*descriptor;
	if (block != NULL) {
		blockHeader = (MemoryBlockHeader *)((char *)block - sizeof(MemoryBlockHeader));
		descriptor = blockHeader->blockFreeRoutine;
		(descriptor->freeRoutine)(block, descriptor->refcon);
	}
}


#endif

#endif

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark realloc AND calloc

void* reallocSmaller(void* block, size_t newSize)
{
//	This actually doesn't reclaim memory!
#if 0
	MemoryBlockHeader			*blockHeader;
#if DEBUG_MAC_MEMORY
	MemoryBlockTrailer			*blockTrailer;
#endif
	if (block != NULL) {
		blockHeader = (MemoryBlockHeader *)((char *)block - sizeof(MemoryBlockHeader));
		if (blockHeader->blockFreeRoutine->freeRoutine == &StandardFree) { 
			size_t		newCompositeSize = newSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;
			newCompositeSize = (newCompositeSize + 3) & 0xFFFFFFFC;
			SetPtrSize((Ptr)blockHeader, newCompositeSize);
#if DEBUG_MAC_MEMORY
			blockHeader->blockSize = newSize;
			blockTrailer = (MemoryBlockTrailer *)((char *)block + newSize + sizeof(MemoryBlockHeader));
			blockTrailer->trailerTag = kUsedBlockTrailerTag;
#endif
		}
	}
#endif
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

	newBlock = (char *)malloc(totalSize);

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

#if DEBUG_MAC_MEMORY || STATS_MAC_MEMORY

void PrintHexNumber(UInt32 hexNumber)
{
	char		hexString[11];
	char		hexMap[] = "0123456789ABCDEF";
	long		digits = 8;
		
	
	hexString[0] = '0';
	hexString[1] = 'x';
	
	while (digits) {
		hexString[digits + 1] = *((hexNumber & 0x0F) + hexMap);
		hexNumber = hexNumber >> 4;
		digits--;
	}
	hexString[10] = 0;
	
	printf(hexString);
	
}

#endif


#if DEBUG_MAC_MEMORY

AllocationSite *
AddAllocationSite(UInt32 allocationType, void **allocatorStack)
{
	UInt32				hashBin = ((UInt32)allocatorStack[0] >> 2) % kActiveSiteHashTableSize;
	AllocationSite		*currentSite = gActiveSizeHashTable[hashBin];
	
	while (currentSite) {
		UInt32	currentStackLevel;
		for (currentStackLevel = 0; currentStackLevel < kRecordingDepthOfStackLevels; currentStackLevel++) {
			if (currentSite->whoAllocated[currentStackLevel] != allocatorStack[currentStackLevel])
				break;
		}
		if (currentStackLevel == kRecordingDepthOfStackLevels)
			break;
		currentSite = currentSite->next;
	}
	
	//	If this is a new site, then create a new entry from the table
	
	if ((currentSite == NULL) && (gRemainingSitesInList != 0)) {
		currentSite = gCurrentActiveSize++;
		gRemainingSitesInList--;
		currentSite->next = gActiveSizeHashTable[hashBin];
		gActiveSizeHashTable[hashBin] = currentSite;
		BlockMoveData(allocatorStack, currentSite->whoAllocated, sizeof(void *) * kRecordingDepthOfStackLevels);
		currentSite->totalAllocations = 0;
		currentSite->mallocs = 0;
		currentSite->frees = 0;
		currentSite->type = allocationType;
	}

	if (currentSite != NULL) {
		currentSite->mallocs++;
		currentSite->totalAllocations++;
	}
	
	return currentSite;
	
}

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

enum {
	kMaxInstructionScanDistance	= 4096
};

void PrintRoutineNameFromPC(void *currentPC)
{
	UInt32		instructionsToLook = kMaxInstructionScanDistance;
	UInt32		*currentPCAsLong = (UInt32 *)currentPC;
	UInt32		offset = 0;
	char		labelBuffer[256];
	
	if (currentPC == NULL)
		return;
	
	while (instructionsToLook--) {
	
		if (*currentPCAsLong == 0x4E800020) {
			char	stringLength = *((char *)currentPCAsLong + 21);
			memset(labelBuffer, 0, 256);
			BlockMoveData(((char *)currentPCAsLong + 22), labelBuffer, stringLength);
			printf(labelBuffer);
			break;
		}
	
		currentPCAsLong++;
		
	}

	instructionsToLook = kMaxInstructionScanDistance;
	currentPCAsLong = (UInt32 *)currentPC;

	while (instructionsToLook--) {
		if (*currentPCAsLong == 0x7C0802A6) {
			printf("+");
			PrintHexNumber(offset - 4);
			break;
		}
		currentPCAsLong--;
		offset += 4;
	}

}

#endif

#if STATS_MAC_MEMORY
void PrintEfficiency(UInt32 current, UInt32 total)
{	
	UInt32 	efficiency = current * 1000 / total;
	UInt32	efficiencyLow = efficiency / 10;
	
	efficiency = efficiency/10;
	
	printf("%ld.%ld", efficiency, efficiencyLow);
	
}
#endif

#if DEBUG_MAC_MEMORY
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

void DumpAllocHeap(void)
{
#if DEBUG_MAC_MEMORY
	MemoryBlockHeader			*currentBlock = gFirstAllocatedBlock;
#endif

#if STATS_MAC_MEMORY

	UInt32						currentItem;
	
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");	
	printf("USAGE REPORT (MALLOC HEAP)\n");
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");
	printf("TYPE      BLOCKS (A)  BLOCKS (U)  BLOCKS (F)  MEMORY (A)  MEMORY (U)  EFFICIENCY\n");
	printf("----      ----------  ----------  ----------  ----------  ----------  ----------\n");
	printf("COMPACT   ");
	PrintHexNumber(gTotalFixedSizeCompactBlocksAllocated);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeCompactBlocksUsed);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeCompactBlocksAllocated - gTotalFixedSizeCompactBlocksUsed);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeCompactAllocated);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeCompactUsed);
	printf("  ");
	PrintEfficiency(gTotalFixedSizeCompactBlocksUsed, gTotalFixedSizeCompactBlocksAllocated);
	printf("/");
	PrintEfficiency(gTotalFixedSizeCompactUsed, gTotalFixedSizeCompactAllocated);
	printf("  \n");	
	printf("FIXED     ");
	PrintHexNumber(gTotalFixedSizeFastBlocksAllocated);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeFastBlocksUsed);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeFastBlocksAllocated - gTotalFixedSizeFastBlocksUsed);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeFastAllocated);
	printf("  ");	
	PrintHexNumber(gTotalFixedSizeFastUsed);
	printf("  ");
	PrintEfficiency(gTotalFixedSizeFastBlocksUsed, gTotalFixedSizeFastBlocksAllocated);
	printf("/");
	PrintEfficiency(gTotalFixedSizeFastUsed, gTotalFixedSizeFastAllocated);
	printf("  \n");	
	printf("HEAP      ");
	PrintHexNumber(0);
	printf("  ");	
	PrintHexNumber(gTotalSmallHeapBlocksUsed);
	printf("  ");	
	PrintHexNumber(0);
	printf("  ");	
	PrintHexNumber(gTotalSmallHeapAllocated);
	printf("  ");	
	PrintHexNumber(gTotalSmallHeapUsed);
	printf("  ");
	PrintEfficiency(gTotalSmallHeapUsed, gTotalSmallHeapAllocated);
	printf("  \nTotal efficiency: ");
	PrintEfficiency(gTotalSmallHeapUsed + gTotalFixedSizeFastUsed + gTotalFixedSizeCompactUsed, 
		gTotalSmallHeapAllocated + gTotalFixedSizeFastAllocated + gTotalFixedSizeCompactAllocated);
	printf("  \n");	
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");	
	printf("BLOCK DISTRIBUTION (MALLOC HEAP: ACTIVE)\n");
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");
	printf("SIZE      +0x00       +0x01       +0x02      +0x03        TOTAL\n");
	printf("----      ----------  ----------  ----------  ----------  ----------\n");
	
	for (currentItem = 0; currentItem < 64; currentItem++) {
		PrintHexNumber(currentItem * 4);
		printf("  ");	
		PrintHexNumber(gSmallAllocationActiveCountTable[currentItem * 4]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationActiveCountTable[currentItem * 4 + 1]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationActiveCountTable[currentItem * 4 + 2]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationActiveCountTable[currentItem * 4 + 3]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationActiveCountTable[currentItem * 4] + 
			gSmallAllocationActiveCountTable[currentItem * 4 + 1] +
			gSmallAllocationActiveCountTable[currentItem * 4 + 2] +
			gSmallAllocationActiveCountTable[currentItem * 4 + 3]);
		printf("\n");	
	}
	printf("Blocks Over 1K:");
	PrintHexNumber(gSmallAllocationActiveCountTable[1024]);
	printf("\n");	
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");	
	printf("BLOCK DISTRIBUTION (MALLOC HEAP: MAX)\n");
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");
	printf("SIZE      +0x00       +0x01       +0x02      +0x03        TOTAL\n");
	printf("----      ----------  ----------  ----------  ----------  ----------\n");
	
	for (currentItem = 0; currentItem < 64; currentItem++) {
		PrintHexNumber(currentItem * 4);
		printf("  ");	
		PrintHexNumber(gSmallAllocationMaxCountTable[currentItem * 4]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationMaxCountTable[currentItem * 4 + 1]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationMaxCountTable[currentItem * 4 + 2]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationMaxCountTable[currentItem * 4 + 3]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationMaxCountTable[currentItem * 4] + 
			gSmallAllocationMaxCountTable[currentItem * 4 + 1] +
			gSmallAllocationMaxCountTable[currentItem * 4 + 2] +
			gSmallAllocationMaxCountTable[currentItem * 4 + 3]);
		printf("\n");	
	}
	printf("Blocks Over 1K:");
	PrintHexNumber(gSmallAllocationMaxCountTable[1024]);
	printf("\n");	
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");	
	printf("BLOCK DISTRIBUTION (MALLOC HEAP: TOTAL)\n");
	printf("********************************************************************************\n");	
	printf("********************************************************************************\n");
	printf("SIZE        +0x00       +0x01       +0x02      +0x03        TOTAL\n");
	printf("----        ----------  ----------  ----------  ----------  ----------\n");
	
	for (currentItem = 0; currentItem < 64; currentItem++) {
		PrintHexNumber(currentItem * 4);
		printf("  ");	
		PrintHexNumber(gSmallAllocationTotalCountTable[currentItem * 4]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationTotalCountTable[currentItem * 4 + 1]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationTotalCountTable[currentItem * 4 + 2]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationTotalCountTable[currentItem * 4 + 3]);
		printf("  ");	
		PrintHexNumber(gSmallAllocationTotalCountTable[currentItem * 4] + 
			gSmallAllocationTotalCountTable[currentItem * 4 + 1] +
			gSmallAllocationTotalCountTable[currentItem * 4 + 2] +
			gSmallAllocationTotalCountTable[currentItem * 4 + 3]);
		printf("\n");	
	}
	printf("Blocks Over 1K:");
	PrintHexNumber(gSmallAllocationTotalCountTable[1024]);
	printf("\n");	

#endif
	
#if DEBUG_MAC_MEMORY

	currentBlock = gFirstAllocatedBlock;

	//	Report leaks
	
	if (gReportLeaks) {
	
		AllocationSite	*scanSite = gActiveSizeList;
		
		printf("********************************************************************************\n");	
		printf("********************************************************************************\n");	
		printf("LEAKS\n");
		printf("********************************************************************************\n");	
		printf("********************************************************************************\n");
		printf("TYPE    ALLOCS      FREES       LEAKS       STACK\n");
		printf("------  ------      -----       -----       -----\n");
		
		while (scanSite != gCurrentActiveSize) {
			
			if (scanSite->mallocs > (scanSite->frees + 1)) {
		
				UInt32	currentStackLevel;
			
				switch (scanSite->type) {
					case kAllocationSiteMalloc:
						printf("MALLOC  ");
						break;	
					case kAllocationSiteNewHandle:
						printf("HANDLE  ");
						break;	
					case kAllocationSiteNewPtr:
						printf("POINTR  ");
						break;	
					default:
						break;	
				}
			
				PrintHexNumber(scanSite->mallocs);
				printf("\t");	
				PrintHexNumber(scanSite->frees);
				printf("\t");	
				PrintHexNumber(scanSite->totalAllocations - scanSite->frees);
				printf("\t");	

				for (currentStackLevel = 0; currentStackLevel < kRecordingDepthOfStackLevels; currentStackLevel++) {
					PrintRoutineNameFromPC(scanSite->whoAllocated[currentStackLevel]);
					if (currentStackLevel == kRecordingDepthOfStackLevels - 1)
						printf("\n");
					else
						printf(",");
				}
				
			}
			
			scanSite++;
			
		}	
		
		printf("Outstanding Pointers:");
		PrintHexNumber(gOutstandingPointers);
		printf("\n");	

		printf("Outstanding Handles:");
		PrintHexNumber(gOutstandingHandles);
		printf("\n");	

	}
	
	// Tag blocks which are referenced from somewhere in the heap.
	
	if (gTagReferencedBlocks) {
		TagReferencedBlocks();
	}

	//	Dump out active blocks
	
	if (gReportActiveBlocks) {
	
		printf("********************************************************************************\n");	
		printf("********************************************************************************\n");	
		printf("ACTIVE BLOCKS\n");
		printf("********************************************************************************\n");	
		printf("********************************************************************************\n");
		printf("ADDRESS     NUMBER      SIZE        STACK\n");
		printf("-------     ------      ----        -----\n");
		
		while (currentBlock) {
		
			UInt32 	currentStackLevel;

			if (!gTagReferencedBlocks || currentBlock->headerTag == kUsedBlockHeaderTag) {
				PrintHexNumber((UInt32)currentBlock);
				printf("\t");	
				PrintHexNumber((UInt32)(currentBlock->blockNum));
				printf("\t");	
				PrintHexNumber((UInt32)(currentBlock->blockSize));
				printf("\t");

				for (currentStackLevel = 0; currentStackLevel < kRecordingDepthOfStackLevels; currentStackLevel++) {
					PrintRoutineNameFromPC(currentBlock->whoAllocated[currentStackLevel]);
					if (currentStackLevel == kRecordingDepthOfStackLevels - 1)
						printf("\n");
					else
						printf(",");
				}
			}

			currentBlock = currentBlock->next;

		}	

	}

#endif

}

FreeMemoryBlockDescriptor	StandardFreeDescriptor = { &StandardFree, 0 };

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark STANDARD ALLOCATOR (SLOW!)

void *StandardAlloc(size_t blockSize, void *refcon)
{
	MemoryBlockHeader	*newBlockHeader = (MemoryBlockHeader*)AllocateRawMemory(((blockSize + 3) & 0xFFFFFFFC) + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE);
	if (newBlockHeader == NULL)
		return NULL;
	newBlockHeader->blockFreeRoutine = &StandardFreeDescriptor;
	return (void *)((char *)newBlockHeader + sizeof(MemoryBlockHeader));
}

void StandardFree(void *block, void *refcon)
{
	MemoryBlockHeader *blockHeader = (MemoryBlockHeader *)((char *)block - sizeof(MemoryBlockHeader));
	FreeRawMemory((Ptr)blockHeader);
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark FIXED SIZED ALLOCATOR (COMPACT)

UInt32 CountLeadingZeros(UInt32 value);

#if defined(powerc) || defined(_powerc)
asm UInt32 CountLeadingZeros(UInt32 value) 
{
	cntlzw	r3, r3
	blr
}
#else
UInt32 CountLeadingZeros(UInt32 value) 
{
	UInt32	whichZero = 0;
	while ((value & 0x80000000) == 0) {
		whichZero++;
		value = value << 1;
	}
	return whichZero;
}
#endif

void *FixedSizeCompactAlloc(size_t blockSize, void *refcon)
{
	FixedSizeCompactAllocationRoot		*sizeRoot = (FixedSizeCompactAllocationRoot *)refcon;
	FixedSizeCompactAllocationChunk		*currentChunk = sizeRoot->firstChunk;
	FreeMemoryBlockDescriptor			*whichFreeDescriptor;
	UInt32								whichBlockIsFree,
										blockOffset;
	MemoryBlockHeader					*blockHeader;
	
	//	Try to find an existing chunk with a free block.
	
	while ((currentChunk != NULL) && (currentChunk->chunkUsage == 0))
		currentChunk = currentChunk->next;

	//	If there is not chunk with a free block, then create
	//	a new one and put it at the beginning of the chunk list.
	
	if (currentChunk == NULL) {

		currentChunk = (FixedSizeCompactAllocationChunk *)AllocateRawMemory(sizeof(FixedSizeCompactAllocationChunk) + 
			(32 * (sizeRoot->blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE)));

		if (currentChunk == NULL) {
			return NULL;
		}

#if STATS_MAC_MEMORY
		gTotalFixedSizeCompactAllocated += sizeof(FixedSizeCompactAllocationChunk) + 
			(32 * (sizeRoot->blockSize + sizeof(MemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE));
		gTotalFixedSizeCompactBlocksAllocated += 32;
#endif

		currentChunk->root = sizeRoot;
		currentChunk->chunkUsage = 0xFFFFFFFF;
	
		currentChunk->next = sizeRoot->firstChunk;
		sizeRoot->firstChunk = currentChunk;
		
	}
	
	//	Find the block in the chunk that is free.
	
	whichBlockIsFree = CountLeadingZeros(currentChunk->chunkUsage);
	
	//	Claim the block
	
	currentChunk->chunkUsage &= ~(0x80000000 >> whichBlockIsFree);
	
	//	Install the free descriptor
	
	whichFreeDescriptor = sizeRoot->freeDescriptorTable + whichBlockIsFree;
	
	blockOffset = (UInt32)(whichFreeDescriptor->refcon) & 0x0000FFFF;
	
	blockHeader = (MemoryBlockHeader *)((char *)currentChunk + blockOffset);
	blockHeader->blockFreeRoutine = whichFreeDescriptor;
	
#if STATS_MAC_MEMORY
	gTotalFixedSizeCompactUsed += sizeRoot->blockSize;
	gTotalFixedSizeCompactBlocksUsed++;
#endif

	return (void *)((char *)blockHeader + sizeof(MemoryBlockHeader));
	
}

void FixedSizeCompactFree(void *block, void *refcon)
{
	MemoryBlockHeader					*blockHeader;
	UInt32								blockNumber;
	UInt32								blockOffset;
	FixedSizeCompactAllocationChunk		*thisChunk;
	FixedSizeCompactAllocationChunk		*currentChunk,
										**previousChunk = NULL;
	FixedSizeCompactAllocationRoot		*root;
	
	blockNumber = (UInt32)refcon >> 16;
	blockOffset = (UInt32)refcon & 0xFFFF;
	
	//	Find the block header and the 
	
	blockHeader = (MemoryBlockHeader *)((char *)block - sizeof(MemoryBlockHeader));
	thisChunk = (FixedSizeCompactAllocationChunk *)((char *)blockHeader - blockOffset);

	//	Mark the block as unused.
	 
	thisChunk->chunkUsage |= (0x80000000 >> blockNumber);

	//	If the block is completely unused, then reclaim it

	root = thisChunk->root;
	currentChunk = root->firstChunk;

	if ((thisChunk->chunkUsage == 0xFFFFFFFF) && (currentChunk != thisChunk)) {
		
		while (currentChunk != thisChunk) {
			previousChunk = &currentChunk->next;
			currentChunk = currentChunk->next;
		}
		
		*previousChunk = currentChunk->next;
		
		FreeRawMemory((Ptr)thisChunk);
			
	}

#if STATS_MAC_MEMORY
	gTotalFixedSizeCompactUsed -= root->blockSize;
	gTotalFixedSizeCompactBlocksUsed--;
#endif
		
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark FIXED SIZED ALLOCATOR (FAST)

void *FixedSizeFastAlloc(size_t blockSize, void *refcon)
{
	FixedSizeFastAllocationRoot		*sizeRoot = (FixedSizeFastAllocationRoot *)refcon;
	FixedSizeFastAllocationChunk	*currentChunk;
	FixedSizeFastMemoryBlockHeader	*ourBlock;
	
	//	If the free list is empty, then we have to allocate another chunk.

	if (sizeRoot->firstFree == NULL) {
	
		FixedSizeFastMemoryBlockHeader	*currentBlock;
		UInt32							currentBlockNum;

#if STATS_MAC_MEMORY
		gTotalFixedSizeFastAllocated += sizeof(FixedSizeFastAllocationChunk) + 
			(sizeRoot->blocksPerChunk * (sizeRoot->blockSize + sizeof(FixedSizeFastMemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE));
		gTotalFixedSizeFastBlocksAllocated += sizeRoot->blocksPerChunk;
#endif

		currentChunk = (FixedSizeFastAllocationChunk *)AllocateRawMemory(sizeof(FixedSizeFastAllocationChunk) + 
			(sizeRoot->blocksPerChunk * (sizeRoot->blockSize + sizeof(FixedSizeFastMemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE)));
		if (currentChunk == NULL) {
			return NULL;
		}
	
		currentChunk->next = sizeRoot->firstChunk;
		sizeRoot->firstChunk = currentChunk;
	
		//	Thread all of the chunks blocks onto the free list.
	
		currentBlock = (FixedSizeFastMemoryBlockHeader *)((char *)currentChunk + sizeof(FixedSizeFastAllocationChunk));
	
		for (currentBlockNum = 0; currentBlockNum < sizeRoot->blocksPerChunk; currentBlockNum++) {
		
			currentBlock->nextFree = sizeRoot->firstFree;
			currentBlock->realHeader.blockFreeRoutine = sizeRoot->freeDescriptor;
			
			sizeRoot->firstFree = currentBlock;
			
			currentBlock = (FixedSizeFastMemoryBlockHeader *)((char *)currentBlock + 
				sizeof(FixedSizeFastMemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE + sizeRoot->blockSize);
			
		}
	
	}

	//	Take the first block off of the free list.  It is the one
	//	we will use.
	
	ourBlock = sizeRoot->firstFree;
	sizeRoot->firstFree = ourBlock->nextFree;
	
#if STATS_MAC_MEMORY
	gTotalFixedSizeFastBlocksUsed++;
	gTotalFixedSizeFastUsed += sizeRoot->blockSize;
#endif
	return (void *)((char *)ourBlock + sizeof(FixedSizeFastMemoryBlockHeader));

}

void FixedSizeFastFree(void *block, void *refcon)
{
	FixedSizeFastMemoryBlockHeader	*deadBlock;
	FixedSizeFastAllocationRoot		*sizeRoot = (FixedSizeFastAllocationRoot *)refcon;
	
	deadBlock = (FixedSizeFastMemoryBlockHeader *)((char *)block - sizeof(FixedSizeFastMemoryBlockHeader));
	
	//	Put the block on the free list.
	
	deadBlock->nextFree = sizeRoot->firstFree;
	sizeRoot->firstFree = deadBlock;

#if STATS_MAC_MEMORY
	gTotalFixedSizeFastBlocksUsed--;
	gTotalFixedSizeFastUsed -= sizeRoot->blockSize;
#endif
	
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


#if DEBUG_MAC_MEMORY
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
	UInt32					startingBinNum,
							remainingBins;
	SmallHeapBlock			**currentBin;
	SmallHeapBlock			*currentBinValue;
	SmallHeapBlock			*newFreeOverflowBlock,
							*blockToCarve,
							*newRawBlockHeader,
							*newRawBlockTrailer;

	//	Round up allocation to nearest 4 bytes.
	
	blockSize = (blockSize + 3) & 0xFFFFFFFC;

	//	Try to find the best fit in one of the bins.
	
	startingBinNum = (blockSize - kDefaultSmallHeadMinSize) >> 2;
	
	currentBin = heapRoot->bins + startingBinNum;
	currentBinValue = *currentBin;
		
	//	If the current bin has something in it,
	//	then use it for our allocation.
	
	if (currentBinValue != NULL) {
		
		RemoveSmallHeapBlockFromFreeList(heapRoot, currentBinValue);

		currentBinValue->info.inUseInfo.freeProc.blockFreeRoutine = heapRoot->blockFreeRoutine;

#if STATS_MAC_MEMORY
		gTotalSmallHeapBlocksUsed++;
		gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(currentBinValue);
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
		blockToCarve = heapRoot->overflow;
	
doCarve:
	
	while (blockToCarve != NULL) {
	
		SInt32	blockToCarveSize = blockToCarve->blockSize;
	
		//	If the owerflow block is big enough to house the
		//	allocation, then use it.
	
		if (blockToCarveSize >= blockSize) {
		
			SInt32				leftovers;
		
			//	Remove the block from the free list... we will
			//	be using it for the allocation...
			
			RemoveSmallHeapBlockFromFreeList(heapRoot, blockToCarve);

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
			
				AddSmallHeapBlockToFreeList(heapRoot, leftoverBlock);
						
			}
		
			blockToCarve->info.inUseInfo.freeProc.blockFreeRoutine = heapRoot->blockFreeRoutine;

#if STATS_MAC_MEMORY
			gTotalSmallHeapBlocksUsed++;
			gTotalSmallHeapUsed += TotalSmallHeapBlockUsage(blockToCarve);
#endif
			return SmallHeapBlockToMemoryPtr(blockToCarve);
						
		} 
	
		blockToCarve = blockToCarve->info.freeInfo.nextFree;
	
	}
	
	//	No existing block was suitable.  We need to allocate
	//	some raw memory to try again.
	
#if STATS_MAC_MEMORY
	gTotalSmallHeapAllocated += kSmallHeapSize;
#endif

	newRawBlockHeader = (SmallHeapBlock *)AllocateRawMemory(kSmallHeapSize);
	if (newRawBlockHeader == NULL) {
		//	If all else fails, try allocating out of the NewPtr heap.
		return StandardAlloc(blockSize, 0);
	}

	//	The first few bytes of the block are a dummy header
	//	which is a block of size zero that is always alloacted.  
	//	This allows our coalesce code to work without modification
	//	on the edge case of coalescing the first real block.

	newRawBlockHeader->prevBlock = NULL;
	newRawBlockHeader->blockSize = kBlockInUseFlag;
	newRawBlockHeader->info.inUseInfo.freeProc.blockFreeRoutine = NULL;
	
	newFreeOverflowBlock = NextSmallHeapBlock(newRawBlockHeader);
	
	newFreeOverflowBlock->prevBlock = newRawBlockHeader;
	newFreeOverflowBlock->blockSize = kSmallHeapSize - 3 * sizeof(SmallHeapBlock) - 3 * MEMORY_BLOCK_TAILER_SIZE;

	//	The same is true for the last few bytes in the block 
	//	as well.
	
	newRawBlockTrailer = (SmallHeapBlock *)(((Ptr)newRawBlockHeader) + kSmallHeapSize - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE);
	newRawBlockTrailer->prevBlock = newFreeOverflowBlock;
	newRawBlockTrailer->blockSize = kBlockInUseFlag | 0xFFFFFFFF;
	newRawBlockTrailer->info.inUseInfo.freeProc.blockFreeRoutine = NULL;
	
	AddSmallHeapBlockToFreeList(heapRoot, newFreeOverflowBlock);
	
	blockToCarve = newFreeOverflowBlock;
	
	//	Try again.
	
	goto doCarve;

}

void SmallHeapFree(void *address, void *refcon)
{
	SmallHeapRoot			*heapRoot = (SmallHeapRoot *)refcon;
	SmallHeapBlock			*deadBlock,
							*prevBlock,
							*nextBlock;
				
	deadBlock = MemoryPtrToSmallHeapBlock(address);

#if STATS_MAC_MEMORY
	gTotalSmallHeapBlocksUsed--;
	gTotalSmallHeapUsed -= TotalSmallHeapBlockUsage(deadBlock);
#endif

	//	If the block after us is free, then coalesce with it.
	
	nextBlock = NextSmallHeapBlock(deadBlock);
	prevBlock = PrevSmallHeapBlock(deadBlock);
	
	if (SmallHeapBlockNotInUse(nextBlock)) {	
		RemoveSmallHeapBlockFromFreeList(heapRoot, nextBlock);
		deadBlock->blockSize += TotalSmallHeapBlockUsage(nextBlock);
		nextBlock = NextSmallHeapBlock(nextBlock);
	}
	
	//	If the block before us is free, then coalesce with it.
	
	if (SmallHeapBlockNotInUse(prevBlock)) {
		RemoveSmallHeapBlockFromFreeList(heapRoot, prevBlock);
		prevBlock->blockSize = ((Ptr)nextBlock - (Ptr)prevBlock) - sizeof(SmallHeapBlock) - MEMORY_BLOCK_TAILER_SIZE;
		AddSmallHeapBlockToFreeList(heapRoot, prevBlock);
		deadBlock = prevBlock;
	}
	
	else {	
		AddSmallHeapBlockToFreeList(heapRoot, deadBlock);
	}
	
	nextBlock->prevBlock = deadBlock;

}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark TRACKING MEMORY MANAGER MEMORY

#if DEBUG_MAC_MEMORY
#if GENERATINGPOWERPC

enum {
	kFigmentBlockHashTableSize			= 128,
	kFigmentBlockTotalTrackedBlocks		= 8192
};

typedef struct FigmentBlockInformation FigmentBlockInformation;

struct FigmentBlockInformation {
	FigmentBlockInformation			*next;
	void							*allocation;
	AllocationSite					*allocator;
};

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

Ptr							figmentAllocationList = NULL;
FigmentBlockInformation		*figmentAllocationListHash[kFigmentBlockHashTableSize];
FigmentBlockInformation		*figmentAllocationFreeList = NULL;

void AddBlockToActiveList(void *newBlock, AllocationSite *alocationSite) 
{

	//	If we don't already have a block to store the list
	//	of our active figment blocks, then create one.

	if (figmentAllocationList == NULL) {

		UInt32	currentBlock;

		figmentAllocationList = (Ptr)CallOSTrapUniversalProc(gNewPtrPatchCallThru, uppNewPtrProcInfo, 0x031E, kFigmentBlockTotalTrackedBlocks * sizeof(FigmentBlockInformation));

		//	Thread all of the new blocks onto the free list.

		for (currentBlock = 0; currentBlock < kFigmentBlockTotalTrackedBlocks; currentBlock++) {

			FigmentBlockInformation	*currentBlockPtr;

			currentBlockPtr = (FigmentBlockInformation * )figmentAllocationList + currentBlock;
			currentBlockPtr->next = figmentAllocationFreeList;
			figmentAllocationFreeList = currentBlockPtr;

		}

	}
	
	//	If we have a free block on the free list, use it
	//	to store the information about this allocation.
	
	if (figmentAllocationFreeList != NULL) {
	
		UInt32						hashKey = ((UInt32)newBlock >> 2) % kFigmentBlockHashTableSize;
		FigmentBlockInformation		*blockToUse;
	
		//	Remove the block from the free list.
	
		blockToUse = figmentAllocationFreeList;
		figmentAllocationFreeList = blockToUse->next;

		//	Add it to the hash table of active blocks in the appropriate place.
		
		blockToUse->next = figmentAllocationListHash[hashKey];
		figmentAllocationListHash[hashKey] = blockToUse;
		
		//	And fill in its relevant information.
		
		blockToUse->allocation = newBlock;
		blockToUse->allocator = alocationSite;
	
	}
	
}

void RemoveBlockFromActiveList(void *deadBlock) 
{
	UInt32						hashKey = ((UInt32)deadBlock >> 2) % kFigmentBlockHashTableSize;
	FigmentBlockInformation		*blockToUse;
	FigmentBlockInformation		**previousBlockToUse = NULL;
	
	blockToUse = figmentAllocationListHash[hashKey];
	previousBlockToUse = figmentAllocationListHash + hashKey;
	
	while (blockToUse) {
	
		if (blockToUse->allocation == deadBlock) {
			if (blockToUse->allocator != NULL)
				blockToUse->allocator->frees++;
			*previousBlockToUse = blockToUse->next;
			blockToUse->next = figmentAllocationFreeList;
			figmentAllocationFreeList = blockToUse;
			break;
		}
	
		previousBlockToUse = &(blockToUse->next);
		blockToUse = blockToUse->next;
	
	}
	
}

void GetCurrentNativeStackTrace(void **stackCrawl)
{
	void	*currentSP;
	void	*interfaceLibSP;
	void	*callerFirstStackFrame;
	void	*callerSP;
	UInt32	stackFrameLevel;
	UInt8	isPowerPC = true;
	
	memset(stackCrawl, 0, sizeof(void *) * kRecordingDepthOfStackLevels);

#if !GENERATINGPOWERPC
	return;
#endif
 
	//	If the current SP is not in our heap, then bail (OT is evil).
	
#if GENERATINGPOWERPC
	currentSP = GetCurrentStackPointer();
#endif

	if ((currentSP > gOurApplicationHeapMax) || (currentSP < gOurApplicationHeapBase))
		return;
		
	interfaceLibSP = *((void **)currentSP);
	
	callerFirstStackFrame = interfaceLibSP;
	
	//	Walk up the stack until we get the first frame
	//	which is actually in our executable's heap... note that
	//	this only works if VM is OFF!
	
	while (1) {

		void	*nextFrame;

		if ((nextFrame > gOurApplicationHeapMax) || (nextFrame < gOurApplicationHeapBase))
			return;
		
		//	Walk the stack differently whether we are at a
		//	PowerPC or 68K frame...

		if (isPowerPC) {

			void	*framePC;

			//	If we are PowerPC, check to see if the PC
			//	corresponding to the current frame is in the
			//	the app's code space.  If so, then we are
			//	done and we break out.
			
			framePC = *((void **)callerFirstStackFrame + 2);
			if ((framePC < gOurApplicationHeapMax) && (framePC > gOurApplicationHeapBase))
				break;
				
			//	Otherwise, check the pointer to the next
			//	stack frame.  If its low bit is set, then
			//	it is a mixed-mode switch frame, and 
			//	we need to switch to 68K frames.

			nextFrame = *((void **)callerFirstStackFrame);
			if (((UInt32)nextFrame & 0x00000001) != 0) {
				callerFirstStackFrame = (void *)((UInt32)nextFrame & 0xFFFFFFFE);
				isPowerPC = false;
			} else
				callerFirstStackFrame = nextFrame;

		}
	
		else {
			
			//	68K case:  If the location immediately above the stack
			//	frame pointer is -1, then we are at a switch frame,
			//	move back to PowerPC.

			if (*((UInt32 *)callerFirstStackFrame - 1) == 0xFFFFFFFF)
				isPowerPC = true;
			callerFirstStackFrame = *((void **)callerFirstStackFrame);

		}
	
	}
	
	callerSP = callerFirstStackFrame;
	
	for (stackFrameLevel = 0; stackFrameLevel < kRecordingDepthOfStackLevels; stackFrameLevel++) {
	
		if ((callerSP > gOurApplicationHeapMax) || (callerSP < gOurApplicationHeapBase))
			return;
			
		stackCrawl[stackFrameLevel] = *(((void **)callerSP) + 2);
		
		callerSP = *((void **)callerSP);
	
	}

}

extern pascal Handle NewHandlePatch(UInt16 trapWord, Size byteCount)
{
	void			*stackCrawl[kRecordingDepthOfStackLevels];
	Handle			resultHandle;
	AllocationSite	*allocationSite;
	
	if (gTrackLeaks)
		GetCurrentNativeStackTrace(stackCrawl);

	resultHandle = (Handle)CallOSTrapUniversalProc(gNewHandlePatchCallThru, uppNewHandleProcInfo, trapWord, byteCount);

	if (gTrackLeaks && resultHandle) {
		allocationSite = AddAllocationSite(kAllocationSiteNewHandle, stackCrawl);
		AddBlockToActiveList((void *)resultHandle, allocationSite);
		gOutstandingHandles++;
	}
	
	return resultHandle;
	
}

extern pascal Ptr NewPtrPatch(UInt16 trapWord, Size byteCount)
{	
	void			*stackCrawl[kRecordingDepthOfStackLevels];
	Ptr				resultPtr;
	AllocationSite	*allocationSite;

	if (gTrackLeaks)
		GetCurrentNativeStackTrace(stackCrawl);
	
	resultPtr = (Ptr)CallOSTrapUniversalProc(gNewPtrPatchCallThru, uppNewPtrProcInfo, trapWord, byteCount);

	if (gTrackLeaks && resultPtr) {
		allocationSite = AddAllocationSite(kAllocationSiteNewPtr, stackCrawl);
		AddBlockToActiveList((void *)resultPtr, allocationSite);
		gOutstandingPointers++;
	}
	
	return resultPtr;
	
}

pascal void DisposeHandlePatch(UInt16 trapWord, Handle deadHandle)
{
	if (gTrackLeaks) {
		RemoveBlockFromActiveList(deadHandle);
		gOutstandingHandles--;
	}
	CallOSTrapUniversalProc(gDisposeHandlePatchCallThru, uppDisposeHandleProcInfo, trapWord, deadHandle);
}

pascal void DisposePtrPatch(UInt16 trapWord, Ptr deadPtr)
{
	if (gTrackLeaks) {
		RemoveBlockFromActiveList(deadPtr);
		gOutstandingPointers--;
	}
	CallOSTrapUniversalProc(gDisposePtrPatchCallThru, uppDisposePtrProcInfo, trapWord, deadPtr);
}


void InstallMemoryManagerPatches(void)
{
	ProcessSerialNumber	thisProcess = { 0, kCurrentProcess };
	ProcessInfoRec		processInfo;

	processInfo.processInfoLength = sizeof(processInfo);
	processInfo.processName = NULL;
	processInfo.processAppSpec = NULL;

	GetProcessInformation(&thisProcess, &processInfo);

	gOurApplicationHeapBase = processInfo.processLocation;
	gOurApplicationHeapMax = (Ptr)gOurApplicationHeapBase + processInfo.processSize;

	gNewHandlePatchCallThru = GetOSTrapAddress(0x0122);
	SetOSTrapAddress(&gNewHandlePatchRD, 0x0122);
	
	gNewPtrPatchCallThru = GetOSTrapAddress(0x011E);
	SetOSTrapAddress(&gNewPtrPatchRD, 0x011E);

	gDisposeHandlePatchCallThru = GetOSTrapAddress(0x023);
	SetOSTrapAddress(&gDisposeHandlePatchRD, 0x023);

	gDisposePtrPatchCallThru = GetOSTrapAddress(0x001F);
	SetOSTrapAddress(&gDisposePtrPatchRD, 0x001F);

}

#else

void InstallMemoryManagerPatches(void)
{
	
}

#endif
#endif



