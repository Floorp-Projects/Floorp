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
#include "nsLargeHeapAllocator.h"


 const UInt32 LargeBlockHeader::kLargeBlockOverhead = sizeof(LargeBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;
 
//--------------------------------------------------------------------
nsLargeHeapAllocator::nsLargeHeapAllocator()
//--------------------------------------------------------------------
{
	mBaseChunkSize = mTempChunkSize = (64 * 1024);
}


//--------------------------------------------------------------------
nsLargeHeapAllocator::~nsLargeHeapAllocator()
//--------------------------------------------------------------------
{

}

//--------------------------------------------------------------------
void * nsLargeHeapAllocator::AllocatorMakeBlock(size_t blockSize)
//--------------------------------------------------------------------
{
	nsLargeHeapChunk*	chunk = (nsLargeHeapChunk *)mFirstChunk;
	LargeBlockHeader	*theBlock = nil;

	// walk through all of our chunks, trying to allocate memory from somewhere
	while (chunk)
	{
		theBlock = chunk->GetSpaceForBlock(blockSize);
		
		if (theBlock)
			break;
		
		chunk = (nsLargeHeapChunk *)chunk->GetNextChunk();
	}

	if (!theBlock)
	{
		chunk = (nsLargeHeapChunk *)AllocateChunk(blockSize);
		if (!chunk) return nil;
		
		theBlock = chunk->GetSpaceForBlock(blockSize);
	}
	
	if (theBlock)
	{
		theBlock->SetLogicalSize(blockSize);
		return &(theBlock->memory);
	}

	return nil;
}


//--------------------------------------------------------------------
void *nsLargeHeapAllocator::AllocatorResizeBlock(void *block, size_t newSize)
//--------------------------------------------------------------------
{
	return nil;
}


//--------------------------------------------------------------------
void nsLargeHeapAllocator::AllocatorFreeBlock(void *freeBlock)
//--------------------------------------------------------------------
{
	LargeBlockHeader	*blockHeader = (LargeBlockHeader *)((char *)freeBlock - sizeof(LargeBlockHeader));
	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header on free");
	MEM_ASSERT(blockHeader->HasTrailerTag(blockHeader->GetBlockSize(), kUsedBlockTrailerTag), "Bad block trailer on free");
	MEM_ASSERT(blockHeader->CheckPaddingBytes(), "Block overwrote bounds");
	nsLargeHeapChunk	*chunk = blockHeader->GetOwningChunk();

	chunk->ReturnBlock(blockHeader);
	blockHeader->SetHeaderTag(kFreeBlockHeaderTag);
	// if this chunk is completely empty and it's not the first chunk then free it
	if (chunk->IsEmpty() && chunk != mFirstChunk)
		FreeChunk(chunk);
}


//--------------------------------------------------------------------
size_t nsLargeHeapAllocator::AllocatorGetBlockSize(void *thisBlock)
//--------------------------------------------------------------------
{
	LargeBlockHeader*	blockHeader = (LargeBlockHeader *)((char *)thisBlock - sizeof(LargeBlockHeader));

	return blockHeader->GetLogicalSize();
}


//--------------------------------------------------------------------
nsHeapChunk *nsLargeHeapAllocator::AllocateChunk(size_t requestedBlockSize)
//--------------------------------------------------------------------
{
	Size	chunkSize = mBaseChunkSize, actualChunkSize;
	Handle	tempMemHandle;
	
	size_t	paddedBlockSize = (( requestedBlockSize + 3 ) & ~3) + 3 * LargeBlockHeader::kLargeBlockOverhead + sizeof(nsLargeHeapChunk);
	
	if (paddedBlockSize > chunkSize)
		chunkSize = paddedBlockSize;

	Ptr		chunkMemory = DoMacMemoryAllocation(chunkSize, actualChunkSize, &tempMemHandle);
	
	// use placement new to initialize the chunk in the memory block
	nsHeapChunk		*newHeapChunk = new (chunkMemory) nsLargeHeapChunk(this, actualChunkSize, tempMemHandle);
	
	if (newHeapChunk)
		AddToChunkList(newHeapChunk);
	
	return newHeapChunk;
}


//--------------------------------------------------------------------
void nsLargeHeapAllocator::FreeChunk(nsHeapChunk *chunkToFree)
//--------------------------------------------------------------------
{
	RemoveFromChunkList(chunkToFree);
	// we used placement new to make it, so we have to delete like this
	nsLargeHeapChunk	*thisChunk = (nsLargeHeapChunk *)chunkToFree;
	thisChunk->~nsLargeHeapChunk();
	
	Handle	tempMemHandle = thisChunk->GetMemHandle();
	if (tempMemHandle)
		DisposeHandle(tempMemHandle);
	else
		DisposePtr((Ptr)thisChunk);
}


#pragma mark -

//--------------------------------------------------------------------
nsLargeHeapChunk::nsLargeHeapChunk(
			nsMemAllocator 	*inOwningAllocator,
			Size 			heapSize,
			Handle 			tempMemHandle) :
	nsHeapChunk(inOwningAllocator, heapSize, tempMemHandle)
//--------------------------------------------------------------------
{
	heapSize -= sizeof(nsLargeHeapChunk);		// subtract heap overhead

	// mark how much we can actually store in the heap
	mHeapSize = heapSize - 3 * sizeof(LargeBlockHeader);
	
	// the head block is zero size and is never free
	mHead->SetPrevBlock((LargeBlockHeader *) -1L);
	
	// we have a free block in the middle
	LargeBlockHeader	*freeBlock = mHead->SkipDummyBlock();
	
	mHead->SetNextBlock(freeBlock);
	
	freeBlock->SetPrevBlock(nil);
	freeBlock->SetNextBlock( (LargeBlockHeader *) ( (UInt32)freeBlock + heapSize - 2 * LargeBlockHeader::kLargeBlockOverhead) );
	
	// and then a zero sized allcated block at the end
	mTail = freeBlock->GetNextBlock();
	mTail->SetNextBlock(nil);
	mTail->SetPrevBlock(freeBlock);

	mTotalFree = freeBlock->GetBlockHeapUsageSize();
}

//--------------------------------------------------------------------
nsLargeHeapChunk::~nsLargeHeapChunk()
//--------------------------------------------------------------------
{
}

//--------------------------------------------------------------------
LargeBlockHeader* nsLargeHeapChunk::GetSpaceForBlock(UInt32 blockSize)
//--------------------------------------------------------------------
{
	UInt32				allocSize = ((blockSize + 3) & ~3) + LargeBlockHeader::kLargeBlockOverhead;

	/* scan through the blocks in this chunk looking for a big enough free block */
	/* we never allocate the head block */
	LargeBlockHeader	*prevBlock = GetHeadBlock();
	LargeBlockHeader	*blockHeader = prevBlock->GetNextBlock();
	
	do
	{
		if (blockHeader->IsFreeBlock())
		{
			UInt32	freeBlockSize = blockHeader->GetBlockHeapUsageSize();
			if (freeBlockSize >= allocSize)
				break;
		}
		
		prevBlock = blockHeader;
		blockHeader = blockHeader->GetNextBlock();
	}
	while (blockHeader);
	
	// if we failed to find a block, return nil
	if (!blockHeader)
		return nil;

	// is there space at the end of this block for a free block?
	if ( ( blockHeader->GetBlockHeapUsageSize() - allocSize ) > LargeBlockHeader::kLargeBlockOverhead )
	{
		LargeBlockHeader	*freeBlock = (LargeBlockHeader *) ( (char *) blockHeader + allocSize );
		freeBlock->SetPrevBlock(nil);
		freeBlock->SetNextBlock(blockHeader->GetNextBlock());
		freeBlock->GetNextBlock()->SetPrevBlock(freeBlock);
		blockHeader->SetNextBlock(freeBlock);
	}
	
	// allocate this block
	blockHeader->SetPrevBlock(prevBlock);
	blockHeader->SetOwningChunk(this);

	blockHeader->SetHeaderTag(kUsedBlockHeaderTag);
	blockHeader->SetTrailerTag(blockHeader->GetBlockSize(), kUsedBlockTrailerTag);
	blockHeader->SetPaddingBytes(((blockSize + 3) & ~3) - blockSize);
	blockHeader->FillPaddingBytes();
	
	mTotalFree -= blockHeader->GetBlockHeapUsageSize();
	IncrementUsedBlocks();
	
	return blockHeader;

}

//--------------------------------------------------------------------
void *nsLargeHeapChunk::GrowBlock(LargeBlockHeader *growBlock, size_t newSize)
//--------------------------------------------------------------------
{
	LargeBlockHeader* 	freeBlock = growBlock->GetNextBlock();

	// is the block following this block a free block?
	if (!freeBlock->IsFreeBlock())
		return nil;
	
	// round the block size up to a multiple of four and add space for the header and trailer
	UInt32		newAllocSize = ( ( newSize + 3 ) & ~3 ) + LargeBlockHeader::kLargeBlockOverhead;
	UInt32		oldAllocSize = growBlock->GetBlockHeapUsageSize();

	/* is it big enough? */
	UInt32		freeBlockSize = freeBlock->GetBlockHeapUsageSize();
	if (freeBlockSize + oldAllocSize < newAllocSize)
		return nil;
	
	// grow this block
	MEM_ASSERT(growBlock->logicalSize < newSize, "Wrong block size on grow block");
	growBlock->SetLogicalSize(newSize);

	mTotalFree -= freeBlock->GetBlockHeapUsageSize();

	// is there still space at the end of this block for a free block?
	if ( freeBlockSize + oldAllocSize - newAllocSize > LargeBlockHeader::kLargeBlockOverhead )
	{
		LargeBlockHeader* smallFree = (LargeBlockHeader *)((char*)growBlock + newAllocSize);
		smallFree->SetPrevBlock(nil);
		smallFree->SetNextBlock(freeBlock->GetNextBlock());
		smallFree->GetNextBlock()->SetPrevBlock(smallFree);
		growBlock->SetNextBlock(smallFree);
		mTotalFree += smallFree->GetBlockHeapUsageSize();
	}
	else
	{
		growBlock->SetNextBlock(freeBlock->GetNextBlock());
		freeBlock->GetNextBlock()->SetPrevBlock(growBlock);
	}
	
	return (void *)(&growBlock->memory);

}


//--------------------------------------------------------------------
void *nsLargeHeapChunk::ShrinkBlock(LargeBlockHeader *growBlock, size_t newSize)
//--------------------------------------------------------------------
{
	// round the block size up to a multiple of four and add space for the header and trailer
	UInt32 				newAllocSize = ((newSize + 3) & ~3) + LargeBlockHeader::kLargeBlockOverhead;
	size_t 				oldAllocSize = growBlock->GetBlockHeapUsageSize();

	LargeBlockHeader*	nextBlock = growBlock->GetNextBlock();
	LargeBlockHeader* 	smallFree = nil;			// Where the recovered freeblock will go

	// shrink this block
	MEM_ASSERT(growBlock->logicalSize > newSize, "Wrong bock size on shrink block");
	growBlock->SetLogicalSize(newSize);

	// is the block following this block a free block?
	if (nextBlock->IsFreeBlock())
	{
		// coalesce the freed space with the following free block
		smallFree = (LargeBlockHeader *)((char *)growBlock + newAllocSize);
		mTotalFree -= nextBlock->GetBlockHeapUsageSize();
		smallFree->SetNextBlock(nextBlock->GetNextBlock());
	}
	// or is there enough space at the end of this block for a new free block?
	else if ( oldAllocSize - newAllocSize > LargeBlockHeader::kLargeBlockOverhead )
	{
		smallFree = (LargeBlockHeader *)((char *)growBlock + newAllocSize);
		smallFree->SetNextBlock(nextBlock);
	}

	if (smallFree)
	{
		// Common actions for both cases
		smallFree->SetPrevBlock(nil);
		smallFree->GetNextBlock()->SetPrevBlock(smallFree);
		growBlock->SetNextBlock(smallFree);
		mTotalFree += smallFree->GetBlockHeapUsageSize();
#if DEBUG_HEAP_INTEGRITY
		smallFree->header.headerTag = kFreeBlockHeaderTag;
#endif
	}
	
	return (void *)(&growBlock->memory);
}

//--------------------------------------------------------------------
void nsLargeHeapChunk::ReturnBlock(LargeBlockHeader *deadBlock)
//--------------------------------------------------------------------
{
	// we might want to coalesce this block with it's prev or next neighbor
	LargeBlockHeader	*prev = deadBlock->prev;
	LargeBlockHeader	*next = deadBlock->next;

	if (prev->IsFreeBlock())
	{
		mTotalFree -= prev->GetBlockHeapUsageSize();
		prev->next = deadBlock->next;
		deadBlock = prev;
		
		if (next->IsFreeBlock())
		{
			mTotalFree -= next->GetBlockHeapUsageSize();
			deadBlock->next = next->next;
			next->next->prev = deadBlock;
		}
		else
		{
			next->prev = deadBlock;
		}
	}
	else if (next->IsFreeBlock() )
	{
		mTotalFree -= next->GetBlockHeapUsageSize();
		deadBlock->next = next->next;
		next->next->prev = deadBlock;
	}
	
	deadBlock->prev = nil;

	mTotalFree += deadBlock->GetBlockHeapUsageSize();
	DecrementUsedBlocks();
}


