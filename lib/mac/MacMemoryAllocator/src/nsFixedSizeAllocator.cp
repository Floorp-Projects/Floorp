/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL. You may obtain a copy of the NPL at  
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation. Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights    
 * Reserved. */


#include <new.h>		// for placement new
#include <MacMemory.h>

#include "nsMemAllocator.h"
#include "nsAllocatorManager.h"
#include "nsFixedSizeAllocator.h"

const UInt32 FixedMemoryBlock::kFixedSizeBlockOverhead = sizeof(FixedMemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;

//--------------------------------------------------------------------
nsFixedSizeAllocator::nsFixedSizeAllocator(size_t blockSize)
:	nsMemAllocator()
,	mBlockSize(blockSize)
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
	nsFixedSizeHeapChunk*	chunk = (nsFixedSizeHeapChunk *)mFirstChunk;

	if (mChunkWithSpace && mChunkWithSpace->GetFreeList())
		return mChunkWithSpace;
	
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
	
	UInt32		paddedSize = (blockSize + 3) & ~3;
	blockHeader->SetPaddingBytes(paddedSize - blockSize);
	blockHeader->FillPaddingBytes(mBlockSize);
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
	MEM_ASSERT(blockHeader->CheckPaddingBytes(mBlockSize), "Block bounds have been overwritten");
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
	if (newSize > mBlockSize || newSize <= mBlockSize - kMaxBlockResizeSlop)
		return nil;
	
	FixedMemoryBlock*	blockHeader = FixedMemoryBlock::GetBlockHeader(block);

#if DEBUG_HEAP_INTEGRITY
	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header");
	MEM_ASSERT(blockHeader->GetTrailerTag(GetAllocatorBlockSize()) == kUsedBlockTrailerTag, "Bad block trailer");
	MEM_ASSERT(blockHeader->CheckPaddingBytes(mBlockSize), "Block bounds have been overwritten");
	
	// if we shrunk the block to below this allocator's normal size range, then these
	// padding bytes won't be any use. But they are tested using mBlockSize, so we
	// have to udpate them anyway.
	UInt32		paddedSize = (newSize + 3) & ~3;
	blockHeader->SetPaddingBytes(paddedSize - newSize);
	blockHeader->FillPaddingBytes(mBlockSize);
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
	return mBlockSize;
}



//--------------------------------------------------------------------
nsHeapChunk *nsFixedSizeAllocator::AllocateChunk(size_t requestedBlockSize)
//--------------------------------------------------------------------
{
	Size	actualChunkSize;
	Ptr		chunkMemory = nsAllocatorManager::GetAllocatorManager()->AllocateSubheap(mBaseChunkSize, actualChunkSize);
	
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
	UInt32	blockCount = numBlocks;
	
	FixedMemoryBlock *freePtr = mMemory;
	FixedMemoryBlock *lastFree;
	
	mFreeList = freePtr;
	
	do
	{
		lastFree = freePtr;
		FixedMemoryBlock *nextFree = (FixedMemoryBlock *) ((UInt32)freePtr + allocBlockSize);
		freePtr->SetNextFree(nextFree);
		freePtr = nextFree;
	}
	while (--blockCount);
	
	lastFree->next = nil;
}


//--------------------------------------------------------------------
nsFixedSizeHeapChunk::~nsFixedSizeHeapChunk()
//--------------------------------------------------------------------
{
}


//--------------------------------------------------------------------
FixedMemoryBlock* nsFixedSizeHeapChunk::FetchFirstFree()
//--------------------------------------------------------------------
{
	FixedMemoryBlock*	firstFree = mFreeList;
	mFreeList = firstFree->GetNextFree();
	mUsedBlocks ++;
	firstFree->SetOwningChunk(this);
	return firstFree;
}


//--------------------------------------------------------------------
void nsFixedSizeHeapChunk::ReturnToFreeList(FixedMemoryBlock *freeBlock)
//--------------------------------------------------------------------
{
	freeBlock->SetNextFree(mFreeList);
	mFreeList = freeBlock;
	mUsedBlocks --;
}


