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
#include "nsFixedSizeAllocator.h"

const UInt32 FixedMemoryBlock::kFixedSizeBlockOverhead = sizeof(FixedMemoryBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;

//--------------------------------------------------------------------
nsFixedSizeAllocator::nsFixedSizeAllocator(size_t blockSize) :
	mBlockSize(blockSize)
//--------------------------------------------------------------------
{
	mBaseChunkSize = mTempChunkSize = (nsMemAllocator::kChunkSizeMultiple);
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

	//	Try to find an existing chunk with a free block.
	while ( chunk != NULL )
	{
		if ( chunk->GetFreeList() != nil )
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
	}

	FixedMemoryBlock*	blockHeader = chunk->FetchFirstFree();
	// these get stripped by the compiler in optimized builds
	blockHeader->SetHeaderTag(kUsedBlockHeaderTag);
	blockHeader->SetTrailerTag(GetAllocatorBlockSize(), kUsedBlockTrailerTag);
	
#if DEBUG_HEAP_INTEGRITY
	UInt32		paddedSize = (blockSize + 3) & ~3;
	blockHeader->SetPaddingBytes(paddedSize - blockSize);
	blockHeader->FillPaddingBytes(mBlockSize);
#endif

	return (void *)&blockHeader->memory;
}


//--------------------------------------------------------------------
void nsFixedSizeAllocator::AllocatorFreeBlock(void *freeBlock)
//--------------------------------------------------------------------
{
	FixedMemoryBlock*	blockHeader = FixedMemoryBlock::GetBlockHeader(freeBlock);
	
	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header");
	MEM_ASSERT(blockHeader->GetTrailerTag(GetAllocatorBlockSize()) == kUsedBlockTrailerTag, "Bad block trailer");
	MEM_ASSERT(blockHeader->CheckPaddingBytes(mBlockSize), "Block bounds have been overwritten");
	
	nsFixedSizeHeapChunk*	chunk = blockHeader->GetOwningChunk();

	// these get stripped by the compiler in optimized builds
	blockHeader->SetHeaderTag(kFreeBlockHeaderTag);
	blockHeader->SetTrailerTag(GetAllocatorBlockSize(), kFreeBlockTrailerTag);

	chunk->ReturnToFreeList(blockHeader);
	
	// if this chunk is completely empty and it's not the first chunk then free it
	if ( chunk->IsEmpty() && chunk != mFirstChunk )
	{
		FreeChunk(chunk);
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

	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header");
	MEM_ASSERT(blockHeader->GetTrailerTag(GetAllocatorBlockSize()) == kUsedBlockTrailerTag, "Bad block trailer");
	MEM_ASSERT(blockHeader->CheckPaddingBytes(mBlockSize), "Block bounds have been overwritten");
	
#if DEBUG_HEAP_INTEGRITY
	// if we shrunk the block to below this allocator's normal size range, then these
	// padding bytes won't be any use. But they are tested using mBlockSize, so we
	// have to udpate them anyway.
	UInt32		paddedSize = (newSize + 3) & ~3;
	blockHeader->SetPaddingBytes(paddedSize - newSize);
	blockHeader->FillPaddingBytes(mBlockSize);
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
	Handle	tempMemHandle;
	Ptr		chunkMemory = DoMacMemoryAllocation(mBaseChunkSize, actualChunkSize, &tempMemHandle);
	
	// use placement new to initialize the chunk in the memory block
	nsHeapChunk		*newHeapChunk = new (chunkMemory) nsFixedSizeHeapChunk(this, actualChunkSize, tempMemHandle);
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
	
	Handle	tempMemHandle = thisChunk->GetMemHandle();
	if (tempMemHandle)
		DisposeHandle(tempMemHandle);
	else
		DisposePtr((Ptr)thisChunk);
}


#pragma mark -

//--------------------------------------------------------------------
nsFixedSizeHeapChunk::nsFixedSizeHeapChunk(
			nsMemAllocator 	*inOwningAllocator,
			Size 			heapSize,
			Handle 			tempMemHandle) :
	nsHeapChunk(inOwningAllocator, heapSize, tempMemHandle)
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


