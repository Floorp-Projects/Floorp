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

#include <MacMemory.h>


#include "nsMemAllocator.h"

//--------------------------------------------------------------------
nsHeapChunk::nsHeapChunk(nsMemAllocator *inOwningAllocator, Size heapSize, Handle tempMemHandle)
:	mOwningAllocator(inOwningAllocator)
,	mNextChunk(nil)
,	mHeapSize(heapSize)
,	mUsedBlocks(0)
,	mTempMemHandle(tempMemHandle)
#if DEBUG_HEAP_INTEGRITY	
,	mSignature(kChunkSignature)
#endif
//--------------------------------------------------------------------
{

}


//--------------------------------------------------------------------
nsHeapChunk::~nsHeapChunk()
//--------------------------------------------------------------------
{
}


#pragma mark -

//--------------------------------------------------------------------
nsMemAllocator::nsMemAllocator(THz inHeapZone)
:	mHeapZone(inHeapZone)
,	mFirstChunk(nil)
,	mLastChunk(nil)
#if DEBUG_HEAP_INTEGRITY	
,	mSignature(kMemAllocatorSignature)
#endif
//--------------------------------------------------------------------
{
}

//--------------------------------------------------------------------
nsMemAllocator::~nsMemAllocator()
//--------------------------------------------------------------------
{
	// free up the subheaps
}


#pragma mark -

//--------------------------------------------------------------------
/* static */ nsMemAllocator* nsMemAllocator::GetAllocatorFromBlock(void *thisBlock)
//--------------------------------------------------------------------
{
	MemoryBlockHeader	*blockHeader = MemoryBlockHeader::GetHeaderFromBlock(thisBlock);
#if DEBUG_HEAP_INTEGRITY
	MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header tag");
	MEM_ASSERT(blockHeader->owningChunk->IsGoodChunk(), "Block has bad chunk pointer");
#endif
	return (blockHeader->owningChunk->GetOwningAllocator());
}

//--------------------------------------------------------------------
/* static */ size_t nsMemAllocator::GetBlockSize(void *thisBlock)
//--------------------------------------------------------------------
{
	nsMemAllocator*		allocator = GetAllocatorFromBlock(thisBlock);
#if DEBUG_HEAP_INTEGRITY
	MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "Failed to get allocator for block");
#endif
	return allocator->AllocatorGetBlockSize(thisBlock);
}


#pragma mark -


//--------------------------------------------------------------------
void nsMemAllocator::AddToChunkList(nsHeapChunk *inNewChunk)
//--------------------------------------------------------------------
{
	if (mLastChunk)
		mLastChunk->SetNextChunk(inNewChunk);
	else
		mFirstChunk = inNewChunk;

	mLastChunk = inNewChunk;

#if STATS_MAC_MEMORY
	mCurSubheapCount ++;
	if (mCurSubheapCount > mMaxSubheapCount)
		mMaxSubheapCount = mCurSubheapCount;
#endif
}


//--------------------------------------------------------------------
void nsMemAllocator::RemoveFromChunkList(nsHeapChunk *inChunk)
//--------------------------------------------------------------------
{
	nsHeapChunk		*prevChunk = nil;
	nsHeapChunk		*nextChunk = nil;
	nsHeapChunk		*thisChunk = mFirstChunk;
		
	while (thisChunk)
	{
		nextChunk = thisChunk->GetNextChunk();
		
		if (thisChunk == inChunk)
			break;
		
		prevChunk = thisChunk;
		thisChunk = nextChunk;
	}
	
	if (thisChunk)
	{
		if (prevChunk)
			prevChunk->SetNextChunk(nextChunk);
		
		if (mFirstChunk == thisChunk)
			mFirstChunk = nextChunk;
		
		if (mLastChunk == thisChunk)
			mLastChunk = prevChunk;
	}
	
#if STATS_MAC_MEMORY
	mCurSubheapCount --;
#endif
}


// amount of free space to maintain in the heap. This is used by 
// plugins, GWorlds and other things.
const Size nsMemAllocator::kFreeHeapSpace 		= 512 * 1024;

// block size multiple. All blocks should be multiples of this size,
// to reduce heap fragmentation
const Size nsMemAllocator::kChunkSizeMultiple 	= 2 * 1024;
const Size nsMemAllocator::kMacMemoryPtrOvehead	= 16;

//--------------------------------------------------------------------
Ptr nsMemAllocator::DoMacMemoryAllocation(Size preferredSize, Size &outActualSize,
			Handle *outTempMemHandle)
// This is the routine that does the actual memory allocation.
// Possible results:
//		1.	Allocate pointer in heap. Return ptr, *outTempMemHandle==nil.
//		2.	Allocate handle in temp mem. Return *handle, and put handle in *outTempMemHandle
//		3.	Fail to allocate. Return nil,  *outTempMemHandle==nil.
//
//		outActualSize may be larger than preferredSize because we
//		want to avoid heap fragmentation by keeping all blocks multiples
//		of one smallest block size. outActualSize will not be smaller
//		than preferredSize
//--------------------------------------------------------------------
{
	*outTempMemHandle = nil;
	
	// calculate an ideal chunk size by rounding up
	preferredSize = kChunkSizeMultiple * ((preferredSize + (kChunkSizeMultiple - 1)) / kChunkSizeMultiple);
	
	// take into accound the memory manager's pointer overhead (16 btyes), to avoid fragmentation
	preferredSize += ((preferredSize / kChunkSizeMultiple) - 1) * kMacMemoryPtrOvehead;
	outActualSize = preferredSize;
	
	// try to allocate in our heap zone
	::SetZone(mHeapZone);

	Ptr	resultPtr = ::NewPtr(preferredSize);

	// set the current zone back to the application zone
	::SetZone(::ApplicationZone());

	if (resultPtr != nil)
		return resultPtr;
	
	// that failed, so try temp mem now
	OSErr		err;
	Handle		tempMemHandle = ::TempNewHandle(preferredSize, &err);
	if (tempMemHandle != nil)
	{
		HLock(tempMemHandle);		// may thee remain locked for all time
		*outTempMemHandle = tempMemHandle;
		return *tempMemHandle;
	}

	return nil;			// failure
}


#if STATS_MAC_MEMORY

//--------------------------------------------------------------------
void nsMemAllocator::AccountForNewBlock(size_t logicalSize)
//--------------------------------------------------------------------
{
	mCurBlockCount ++;
	
	if (mCurBlockCount > mMaxBlockCount)
		mMaxBlockCount = mCurBlockCount;
		
	mCurBlockSpaceUsed += logicalSize;
	
	if (mCurBlockSpaceUsed > mMaxBlockSpaceUsed)
		mMaxBlockSpaceUsed = mCurBlockSpaceUsed;

}

//--------------------------------------------------------------------
void nsMemAllocator::AccountForResizedBlock(size_t oldLogicalSize, size_t newLogicalSize)
//--------------------------------------------------------------------
{
	mCurBlockSpaceUsed -= oldLogicalSize;
	mCurBlockSpaceUsed += newLogicalSize;
	
	if (mCurBlockSpaceUsed > mMaxBlockSpaceUsed)
		mMaxBlockSpaceUsed = mCurBlockSpaceUsed;

}

//--------------------------------------------------------------------
void nsMemAllocator::AccountForFreedBlock(size_t logicalSize)
//--------------------------------------------------------------------
{
	mCurBlockCount --;
	mCurBlockSpaceUsed -= logicalSize;
}

#endif
