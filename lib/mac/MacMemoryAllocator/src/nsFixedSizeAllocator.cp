/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-  */
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


#include <new.h>		// for placement new
#include <MacMemory.h>

#include "nsMemAllocator.h"
#include "nsAllocatorManager.h"
#include "nsFixedSizeAllocator.h"

const UInt32 FixedMemoryBlock::kFixedSizeBlockOverhead = sizeof(FixedMemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;

//--------------------------------------------------------------------
nsFixedSizeAllocator::nsFixedSizeAllocator(size_t minBlockSize, size_t maxBlockSize)
:	nsMemAllocator(eAllocatorTypeFixed, minBlockSize, maxBlockSize)
,	mChunkWithSpace(nil)
//--------------------------------------------------------------------
{
	mBaseChunkSize = mTempChunkSize = (nsAllocatorManager::kChunkSizeMultiple);
}

//--------------------------------------------------------------------
nsFixedSizeAllocator::~nsFixedSizeAllocator()
//--------------------------------------------------------------------
{
}


//--------------------------------------------------------------------
nsHeapChunk* nsFixedSizeAllocator::FindChunkWithSpace(size_t blockSize) const
//--------------------------------------------------------------------
{
	if (mChunkWithSpace && mChunkWithSpace->GetFreeList())
		return mChunkWithSpace;
	
	nsFixedSizeHeapChunk*	chunk = (nsFixedSizeHeapChunk *)mFirstChunk;

	//	Try to find an existing chunk with a free block.
	while (chunk != nil)
	{
		if (chunk->GetFreeList() != nil)
			return chunk;
		
		chunk = (nsFixedSizeHeapChunk *)chunk->GetNextChunk();
	}

	return nil;
}


//--------------------------------------------------------------------
void *nsFixedSizeAllocator::AllocatorMakeBlock(size_t blockSize)
//--------------------------------------------------------------------
{
	nsFixedSizeHeapChunk*	chunk = (nsFixedSizeHeapChunk *)FindChunkWithSpace(blockSize);

	if (chunk == nil)
	{
		chunk = (nsFixedSizeHeapChunk *)AllocateChunk(blockSize);
		if (!chunk) return nil;
		
		mChunkWithSpace = chunk;
	}

	FixedMemoryBlock*	blockHeader = chunk->FetchFirstFree();
	
#if DEBUG_HEAP_INTEGRITY
	blockHeader->SetHeaderTag(kUsedBlockHeaderTag);
	blockHeader->SetTrailerTag(GetAllocatorBlockSize(), kUsedBlockTrailerTag);
	
	blockHeader->ZapBlockContents(blockSize, kUsedMemoryFillPattern);
	
	UInt32		paddedSize = (blockSize + 3) & ~3;
	blockHeader->SetPaddingBytes(paddedSize - blockSize);
	blockHeader->FillPaddingBytes(mMaxBlockSize);
#endif

#if STATS_MAC_MEMORY
	blockHeader->blockHeader.header.logicalBlockSize = blockSize;
	AccountForNewBlock(blockSize);
#endif

	return (void *)&blockHeader->memory;
}


//--------------------------------------------------------------------
void nsFixedSizeAllocator::AllocatorFreeBlock(void *freeBlock)
//--------------------------------------------------------------------
{
	FixedMemoryBlock*	blockHeader = FixedMemoryBlock::GetBlockHeader(freeBlock);
	
#if DEBUG_HEAP_INTEGRITY
	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header");
	MEM_ASSERT(blockHeader->GetTrailerTag(GetAllocatorBlockSize()) == kUsedBlockTrailerTag, "Bad block trailer");
	MEM_ASSERT(blockHeader->CheckPaddingBytes(GetAllocatorBlockSize()), "Block bounds have been overwritten");

	blockHeader->ZapBlockContents(GetAllocatorBlockSize(), kFreeMemoryFillPattern);
#endif
	
#if STATS_MAC_MEMORY
	AccountForFreedBlock(blockHeader->blockHeader.header.logicalBlockSize);
#endif

	nsFixedSizeHeapChunk*	chunk = blockHeader->GetOwningChunk();

#if DEBUG_HEAP_INTEGRITY
	blockHeader->SetHeaderTag(kFreeBlockHeaderTag);
	blockHeader->SetTrailerTag(GetAllocatorBlockSize(), kFreeBlockTrailerTag);
#endif

	chunk->ReturnToFreeList(blockHeader);
	
	// if this chunk is completely empty and it's not the first chunk then free it
	if ( chunk->IsEmpty() && chunk != mFirstChunk )
	{
		if (chunk == mChunkWithSpace)
			mChunkWithSpace = nil;
			
		FreeChunk(chunk);
	}
	else
	{
		mChunkWithSpace = chunk;			// we know is has some space now
	}
}


//--------------------------------------------------------------------
void *nsFixedSizeAllocator::AllocatorResizeBlock(void *block, size_t newSize)
//--------------------------------------------------------------------
{
	// let blocks shrink to at most 16 bytes below this allocator's block size
	if (newSize > mMaxBlockSize || newSize <= mMaxBlockSize - kMaxBlockResizeSlop)
		return nil;
	
	FixedMemoryBlock*	blockHeader = FixedMemoryBlock::GetBlockHeader(block);

#if DEBUG_HEAP_INTEGRITY
	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header");
	MEM_ASSERT(blockHeader->GetTrailerTag(GetAllocatorBlockSize()) == kUsedBlockTrailerTag, "Bad block trailer");
	MEM_ASSERT(blockHeader->CheckPaddingBytes(mMaxBlockSize), "Block bounds have been overwritten");
	
	// if we shrunk the block to below this allocator's normal size range, then these
	// padding bytes won't be any use. But they are tested using mBlockSize, so we
	// have to udpate them anyway.
	UInt32		paddedSize = (newSize + 3) & ~3;
	blockHeader->SetPaddingBytes(paddedSize - newSize);
	blockHeader->FillPaddingBytes(mMaxBlockSize);
#endif

#if STATS_MAC_MEMORY
	AccountForResizedBlock(blockHeader->blockHeader.header.logicalBlockSize, newSize);
	blockHeader->blockHeader.header.logicalBlockSize = newSize;
#endif

	return block;
}


//--------------------------------------------------------------------
size_t nsFixedSizeAllocator::AllocatorGetBlockSize(void *thisBlock)
//--------------------------------------------------------------------
{
	return mMaxBlockSize;
}



//--------------------------------------------------------------------
nsHeapChunk *nsFixedSizeAllocator::AllocateChunk(size_t requestedBlockSize)
//--------------------------------------------------------------------
{
	Size	actualChunkSize;
	
	// adapt the chunk size if we have already allocated a number of chunks, and it's not above a max size
	if (mNumChunks > 4 && mBaseChunkSize < nsAllocatorManager::kMaxChunkSize)
		mBaseChunkSize *= 2;
	
	Ptr		chunkMemory = nsAllocatorManager::GetAllocatorManager()->AllocateSubheap(mBaseChunkSize, actualChunkSize);
	if (!chunkMemory) return nil;
	
	// use placement new to initialize the chunk in the memory block
	nsHeapChunk		*newHeapChunk = new (chunkMemory) nsFixedSizeHeapChunk(this, actualChunkSize);
	if (newHeapChunk)
		AddToChunkList(newHeapChunk);
		
	return newHeapChunk;
}


//--------------------------------------------------------------------
void nsFixedSizeAllocator::FreeChunk(nsHeapChunk *chunkToFree)
//--------------------------------------------------------------------
{
	RemoveFromChunkList(chunkToFree);
	// we used placement new to make it, so we have to delete like this
	nsFixedSizeHeapChunk	*thisChunk = (nsFixedSizeHeapChunk *)chunkToFree;
	thisChunk->~nsFixedSizeHeapChunk();
	
	nsAllocatorManager::GetAllocatorManager()->FreeSubheap((Ptr)thisChunk);
}


#pragma mark -

//--------------------------------------------------------------------
nsFixedSizeHeapChunk::nsFixedSizeHeapChunk(
			nsMemAllocator 	*inOwningAllocator,
			Size 			heapSize) :
	nsHeapChunk(inOwningAllocator, heapSize)
//--------------------------------------------------------------------
{
	nsFixedSizeAllocator	*allocator = (nsFixedSizeAllocator *)mOwningAllocator;
	UInt32	blockSize = allocator->GetAllocatorBlockSize();
	UInt32	allocBlockSize = blockSize + FixedMemoryBlock::kFixedSizeBlockOverhead;

	// work out how much we can actually store in the heap
	UInt32	numBlocks = (heapSize - sizeof(nsFixedSizeHeapChunk)) / allocBlockSize;
	mHeapSize = numBlocks * allocBlockSize;

	// build the free list for this chunk
	UInt32	blockCount = numBlocks - 1;		// -1 because we do the last one by hand
	
	FixedMemoryBlock *freePtr = mMemory;
	FixedMemoryBlock *nextFree;
	
	mFreeList = freePtr;
	
	do
	{
		nextFree = (FixedMemoryBlock *) ((UInt32)freePtr + allocBlockSize);
		freePtr->SetOwningChunk(this);
		freePtr->SetNextFree(nextFree);

		freePtr = nextFree;
	}
	while (--blockCount);
	
	freePtr->SetOwningChunk(this);
	freePtr->SetNextFree(nil);

}

