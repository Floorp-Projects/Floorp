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

#include <new.h>        // for placement new
#include <MacMemory.h>

#include "nsMemAllocator.h"
#include "nsAllocatorManager.h"
#include "nsLargeHeapAllocator.h"


 const UInt32 LargeBlockHeader::kLargeBlockOverhead = sizeof(LargeBlockHeader) + MEMORY_BLOCK_TAILER_SIZE;
 
//--------------------------------------------------------------------
nsLargeHeapAllocator::nsLargeHeapAllocator(size_t minBlockSize, size_t maxBlockSize)
:   nsMemAllocator(eAllocatorTypeLarge, minBlockSize, maxBlockSize)
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
    nsLargeHeapChunk*   chunk = (nsLargeHeapChunk *)mFirstChunk;
    LargeBlockHeader    *theBlock = nil;

    UInt32              allocSize = GetPaddedBlockSize(blockSize);

    // walk through all of our chunks, trying to allocate memory from somewhere
    while (chunk)
    {
        if (chunk->GetLargestFreeBlock() >= allocSize)
        {
          theBlock = chunk->GetSpaceForBlock(blockSize);
        if (theBlock)
            break;
        }
        
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
        
#if STATS_MAC_MEMORY
        theBlock->header.logicalBlockSize = blockSize;      // yes, it is stored in 2 places in this allocator
        AccountForNewBlock(blockSize);
#endif

        return &(theBlock->memory);
    }

    return nil;
}


//--------------------------------------------------------------------
void *nsLargeHeapAllocator::AllocatorResizeBlock(void *block, size_t newSize)
//--------------------------------------------------------------------
{
    LargeBlockHeader    *blockHeader = LargeBlockHeader::GetBlockHeader(block);
    nsLargeHeapChunk    *chunk = blockHeader->GetOwningChunk();
    
#if DEBUG_HEAP_INTEGRITY
    MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header on realloc");
    MEM_ASSERT(blockHeader->HasTrailerTag(blockHeader->GetBlockSize(), kUsedBlockTrailerTag), "Bad block trailer on realloc");
    MEM_ASSERT(blockHeader->CheckPaddingBytes(), "Block has overwritten its bounds");
#endif

    UInt32      newAllocSize = (newSize + 3) & ~3;

    // we can resize this block to any size, provided it fits.
    
    if (newAllocSize < blockHeader->GetBlockSize())                 // shrinking
    {
        return chunk->ShrinkBlock(blockHeader, newSize);
    }
    else if (newAllocSize > blockHeader->GetBlockSize())            // growing
    {
        return chunk->GrowBlock(blockHeader, newSize);
    }
    else
    {
        return chunk->ResizeBlockInPlace(blockHeader, newSize);
    }
    
    return nil;
}


//--------------------------------------------------------------------
void nsLargeHeapAllocator::AllocatorFreeBlock(void *freeBlock)
//--------------------------------------------------------------------
{
    LargeBlockHeader    *blockHeader = LargeBlockHeader::GetBlockHeader(freeBlock);

#if DEBUG_HEAP_INTEGRITY
    MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header on free");
    MEM_ASSERT(blockHeader->HasTrailerTag(blockHeader->GetBlockSize(), kUsedBlockTrailerTag), "Bad block trailer on free");
    MEM_ASSERT(blockHeader->CheckPaddingBytes(), "Block overwrote bounds");

    blockHeader->ZapBlockContents(kFreeMemoryFillPattern);
#endif

#if STATS_MAC_MEMORY
    AccountForFreedBlock(blockHeader->header.logicalBlockSize);
#endif

    nsLargeHeapChunk    *chunk = blockHeader->GetOwningChunk();
    chunk->ReturnBlock(blockHeader);
    
#if DEBUG_HEAP_INTEGRITY
    blockHeader->SetHeaderTag(kFreeBlockHeaderTag);
#endif

    // if this chunk is completely empty and it's not the first chunk then free it
    if (chunk->IsEmpty() && chunk != mFirstChunk)
        FreeChunk(chunk);
}


//--------------------------------------------------------------------
size_t nsLargeHeapAllocator::AllocatorGetBlockSize(void *thisBlock)
//--------------------------------------------------------------------
{
    LargeBlockHeader*   blockHeader = (LargeBlockHeader *)((char *)thisBlock - sizeof(LargeBlockHeader));

    return blockHeader->GetLogicalSize();
}


//--------------------------------------------------------------------
nsHeapChunk *nsLargeHeapAllocator::AllocateChunk(size_t requestedBlockSize)
//--------------------------------------------------------------------
{
    Size    chunkSize = mBaseChunkSize, actualChunkSize;
    
    size_t  paddedBlockSize = (( requestedBlockSize + 3 ) & ~3) + 3 * LargeBlockHeader::kLargeBlockOverhead + sizeof(nsLargeHeapChunk);
    
    if (paddedBlockSize > chunkSize)
        chunkSize = paddedBlockSize;

    Ptr     chunkMemory = nsAllocatorManager::GetAllocatorManager()->AllocateSubheap(chunkSize, actualChunkSize);
    if (!chunkMemory) return nil;
    
    // use placement new to initialize the chunk in the memory block
    nsHeapChunk     *newHeapChunk = new (chunkMemory) nsLargeHeapChunk(this, actualChunkSize);
    
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
    nsLargeHeapChunk    *thisChunk = (nsLargeHeapChunk *)chunkToFree;
    thisChunk->~nsLargeHeapChunk();
    
    nsAllocatorManager::GetAllocatorManager()->FreeSubheap((Ptr)thisChunk);
}


#pragma mark -

//--------------------------------------------------------------------
nsLargeHeapChunk::nsLargeHeapChunk(
            nsMemAllocator  *inOwningAllocator,
            Size            heapSize) :
    nsHeapChunk(inOwningAllocator, heapSize)
//--------------------------------------------------------------------
{
    heapSize -= sizeof(nsLargeHeapChunk);       // subtract heap overhead

    // mark how much we can actually store in the heap
    mHeapSize = heapSize - 3 * sizeof(LargeBlockHeader);
    
    // the head block is zero size and is never free
    mHead->SetPrevBlock((LargeBlockHeader *) -1L);
    
    // we have a free block in the middle
    LargeBlockHeader    *freeBlock = mHead->SkipDummyBlock();
    
    mHead->SetNextBlock(freeBlock);
  mHead->SetLogicalSize(0);

#if DEBUG_HEAP_INTEGRITY
  mHead->SetPaddingBytes(0);

    mHead->SetHeaderTag(kDummyBlockHeaderTag);
  mHead->header.blockID = -1;
#endif
    
    freeBlock->SetPrevBlock(nil);
    freeBlock->SetNextBlock( (LargeBlockHeader *) ( (UInt32)freeBlock + heapSize - 2 * LargeBlockHeader::kLargeBlockOverhead) );
    
    // and then a zero sized allocated block at the end
    mTail = freeBlock->GetNextBlock();
    mTail->SetNextBlock(nil);
    mTail->SetPrevBlock(freeBlock);

    mTotalFree = mLargestFreeBlock = freeBlock->GetBlockHeapUsageSize();
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
    UInt32              allocSize = ((blockSize + 3) & ~3) + LargeBlockHeader::kLargeBlockOverhead;

    if (allocSize > mLargestFreeBlock) return nil;
    //Boolean               expectFailure = (allocSize > mTotalFree);
    
    /* scan through the blocks in this chunk looking for a big enough free block */
    /* we never allocate the head block */
    LargeBlockHeader    *prevBlock = GetHeadBlock();
    LargeBlockHeader    *blockHeader = prevBlock->GetNextBlock();
    
    do
    {
        if (blockHeader->IsFreeBlock())
        {
            UInt32  freeBlockSize = blockHeader->GetBlockHeapUsageSize();
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
        LargeBlockHeader    *freeBlock = (LargeBlockHeader *) ( (char *) blockHeader + allocSize );
        freeBlock->SetPrevBlock(nil);
        freeBlock->SetNextBlock(blockHeader->GetNextBlock());
        freeBlock->GetNextBlock()->SetPrevBlock(freeBlock);
        blockHeader->SetNextBlock(freeBlock);
    }
    
    // allocate this block
    blockHeader->SetPrevBlock(prevBlock);
    blockHeader->SetOwningChunk(this);

#if DEBUG_HEAP_INTEGRITY
    blockHeader->SetHeaderTag(kUsedBlockHeaderTag);
    blockHeader->SetTrailerTag(blockHeader->GetBlockSize(), kUsedBlockTrailerTag);
    blockHeader->SetPaddingBytes(((blockSize + 3) & ~3) - blockSize);
    blockHeader->ZapBlockContents(kUsedMemoryFillPattern);
    blockHeader->FillPaddingBytes();
#endif
    
    mTotalFree -= blockHeader->GetBlockHeapUsageSize();
    IncrementUsedBlocks();

    UpdateLargestFreeBlock();           // we could optimize this

    //MEM_ASSERT(!expectFailure, "I though this would fail!");
    return blockHeader;

}

//--------------------------------------------------------------------
void *nsLargeHeapChunk::GrowBlock(LargeBlockHeader *growBlock, size_t newSize)
//--------------------------------------------------------------------
{
    LargeBlockHeader*   freeBlock = growBlock->GetNextBlock();

    // is the block following this block a free block?
    if (!freeBlock->IsFreeBlock())
        return nil;
    
    // round the block size up to a multiple of four and add space for the header and trailer
    UInt32      newAllocSize = ( ( newSize + 3 ) & ~3 ) + LargeBlockHeader::kLargeBlockOverhead;
    UInt32      oldAllocSize = growBlock->GetBlockHeapUsageSize();

    /* is it big enough? */
    UInt32      freeBlockSize = freeBlock->GetBlockHeapUsageSize();
    if (freeBlockSize + oldAllocSize < newAllocSize)
        return nil;
    
    // grow this block
#if STATS_MAC_MEMORY
    UInt32      oldLogicalSize = growBlock->GetLogicalSize();
#endif
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
#if DEBUG_HEAP_INTEGRITY
        smallFree->header.headerTag = kFreeBlockHeaderTag;
#endif
    }
    else
    {
        growBlock->SetNextBlock(freeBlock->GetNextBlock());
        freeBlock->GetNextBlock()->SetPrevBlock(growBlock);
    }
    
#if DEBUG_HEAP_INTEGRITY
    growBlock->SetTrailerTag(growBlock->GetBlockSize(), kUsedBlockTrailerTag);
    growBlock->SetPaddingBytes(((newSize + 3) & ~3) - newSize);
    growBlock->FillPaddingBytes();
#endif

#if STATS_MAC_MEMORY
    GetOwningAllocator()->AccountForResizedBlock(oldLogicalSize, newSize);
#endif

    UpdateLargestFreeBlock();           // we could optimize this

    return (void *)(&growBlock->memory);
}


//--------------------------------------------------------------------
void *nsLargeHeapChunk::ShrinkBlock(LargeBlockHeader *growBlock, size_t newSize)
//--------------------------------------------------------------------
{
    // round the block size up to a multiple of four and add space for the header and trailer
    UInt32              newAllocSize = ((newSize + 3) & ~3) + LargeBlockHeader::kLargeBlockOverhead;
    size_t              oldAllocSize = growBlock->GetBlockHeapUsageSize();

    LargeBlockHeader*   nextBlock = growBlock->GetNextBlock();
    LargeBlockHeader*   smallFree = nil;            // Where the recovered freeblock will go

    // shrink this block
#if STATS_MAC_MEMORY
    UInt32      oldLogicalSize = growBlock->GetLogicalSize();
#endif
    MEM_ASSERT(oldAllocSize > newAllocSize, "Wrong bock size on shrink block");
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
    
#if DEBUG_HEAP_INTEGRITY
    growBlock->SetTrailerTag(growBlock->GetBlockSize(), kUsedBlockTrailerTag);
    growBlock->SetPaddingBytes(((newSize + 3) & ~3) - newSize);
    growBlock->FillPaddingBytes();
#endif

#if STATS_MAC_MEMORY
    GetOwningAllocator()->AccountForResizedBlock(oldLogicalSize, newSize);
#endif

    UpdateLargestFreeBlock();           // we could optimize this

    return (void *)(&growBlock->memory);
}


//--------------------------------------------------------------------
void* nsLargeHeapChunk::ResizeBlockInPlace(LargeBlockHeader *theBlock, size_t newSize)
//--------------------------------------------------------------------
{
    theBlock->SetLogicalSize(newSize);

#if DEBUG_HEAP_INTEGRITY
    UInt32      newAllocSize = (newSize + 3) & ~3;
    
    theBlock->SetPaddingBytes(newAllocSize - newSize);
    theBlock->FillPaddingBytes();
#endif

#if STATS_MAC_MEMORY
    GetOwningAllocator()->AccountForResizedBlock(theBlock->header.logicalBlockSize, newSize);
    theBlock->header.logicalBlockSize = newSize;
#endif

    UpdateLargestFreeBlock();           // we could optimize this

    return (void *)(&theBlock->memory);
}


//--------------------------------------------------------------------
void nsLargeHeapChunk::ReturnBlock(LargeBlockHeader *deadBlock)
//--------------------------------------------------------------------
{
    // we might want to coalesce this block with it's prev or next neighbor
    LargeBlockHeader    *prev = deadBlock->prev;
    LargeBlockHeader    *next = deadBlock->next;

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
    
    UpdateLargestFreeBlock();           // we could optimize this
    
    DecrementUsedBlocks();
}


//--------------------------------------------------------------------
void nsLargeHeapChunk::UpdateLargestFreeBlock()
//--------------------------------------------------------------------
{
    LargeBlockHeader    *thisBlock = mHead->GetNextBlock();   // head block is a dummy block
    UInt32              curMaxSize = 0;
    
    while (thisBlock != mTail)
    {
      if (thisBlock->IsFreeBlock())
      {
        UInt32      blockSize = thisBlock->GetBlockHeapUsageSize();
        
        if (blockSize > curMaxSize)
            curMaxSize = blockSize;
        }
        thisBlock = thisBlock->GetNextBlock();
    }

    mLargestFreeBlock = curMaxSize;
}

