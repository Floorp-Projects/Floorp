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

#include <string.h>

class nsMemAllocator;
class nsFixedSizeHeapChunk;


struct FixedMemoryBlockHeader
{
#if DEBUG_HEAP_INTEGRITY
    UInt16                      blockFlags;         // unused at present
    UInt16                      blockPadding;
#endif

    MemoryBlockHeader           header;             // this must be the last variable before memory
};


struct FixedMemoryBlock
{
    static FixedMemoryBlock*    GetBlockHeader(void *block)
                                            { return (FixedMemoryBlock *)((char *)block - sizeof(FixedMemoryBlockHeader)); }
                                            
    FixedMemoryBlock*           GetNextFree()
                                            { return next; }
    void                        SetNextFree(FixedMemoryBlock *nextFree)
                                            { next = nextFree; }
    
    void                        SetOwningChunk(nsHeapChunk *inOwningChunk)
                                            { blockHeader.header.owningChunk = inOwningChunk; }
    
    nsFixedSizeHeapChunk*       GetOwningChunk()
                                            { return (nsFixedSizeHeapChunk *)blockHeader.header.owningChunk; }
    
#if DEBUG_HEAP_INTEGRITY
    enum {
        kBlockPaddingBytes  = 'ננננ'
    };
    
    void                    SetPaddingBytes(UInt32 padding)     { blockHeader.blockPadding = padding; }
    void                    FillPaddingBytes(UInt32 blockSize)  {
                                                                    long    *lastLong   = (long *)((char *)&memory + blockSize - sizeof(long));
                                                                    UInt32  mask        = (1 << (8 * blockHeader.blockPadding)) - 1;
                                                                    *lastLong &= ~mask;
                                                                    *lastLong |= (mask & kBlockPaddingBytes);
                                                                }

    Boolean                 CheckPaddingBytes(UInt32 blockSize) {
                                                                    long    *lastLong   = (long *)((char *)&memory + blockSize - sizeof(long));
                                                                    UInt32  mask        = (1 << (8 * blockHeader.blockPadding)) - 1;
                                                                    return (*lastLong & mask) == (mask & kBlockPaddingBytes);
                                                                }
    UInt32                  GetPaddingBytes()                   { return blockHeader.blockPadding; }

    void                    ZapBlockContents(UInt32 blockSize, UInt8 pattern)
                                                                {
                                                                    memset(&memory, pattern, blockSize);
                                                                }
    
    
    // inline, so won't crash if this is a bad block
    Boolean                 HasHeaderTag(MemoryBlockTag inHeaderTag)
                                            { return blockHeader.header.headerTag == inHeaderTag; }
    void                    SetHeaderTag(MemoryBlockTag inHeaderTag)
                                            { blockHeader.header.headerTag = inHeaderTag; }
                                            
    void                    SetTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
                                            {
                                                MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
                                                trailer->trailerTag = theTag;
                                            }
    MemoryBlockTag          GetTrailerTag(UInt32 blockSize)
                                            {
                                                MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
                                                return trailer->trailerTag;
                                            }
#endif

    static const UInt32             kFixedSizeBlockOverhead;

    FixedMemoryBlockHeader          blockHeader;
    
    union {
        FixedMemoryBlock*           next;
        void*                       memory;
    };
};


class nsFixedSizeHeapChunk : public nsHeapChunk
{
    public:
    
                                    nsFixedSizeHeapChunk(nsMemAllocator *inOwningAllocator, Size heapSize);
                                    ~nsFixedSizeHeapChunk() {}

        FixedMemoryBlock*           GetFreeList() const { return mFreeList; }
        void                        SetFreeList(FixedMemoryBlock *nextFree) { mFreeList = nextFree; }
                
        FixedMemoryBlock*           FetchFirstFree()
                                    {
                                        FixedMemoryBlock*   firstFree = mFreeList;
                                        mFreeList = firstFree->GetNextFree();
                                        mUsedBlocks ++;
                                        return firstFree;
                                    }

        void                        ReturnToFreeList(FixedMemoryBlock *freeBlock)
                                    {
                                        freeBlock->SetNextFree(mFreeList);
                                        mFreeList = freeBlock;
                                        mUsedBlocks --;
                                    }


    protected:
        
    #if STATS_MAC_MEMORY
        UInt32                      chunkSize;
        UInt32                      numBlocks;
    #endif
        
        FixedMemoryBlock            *mFreeList;
        FixedMemoryBlock            mMemory[];
};



//--------------------------------------------------------------------
class nsFixedSizeAllocator : public nsMemAllocator
{
    private:
    
        typedef nsMemAllocator  Inherited;
    
    public:

                                nsFixedSizeAllocator(size_t minBlockSize, size_t maxBlockSize);
                                ~nsFixedSizeAllocator();

        void *                  AllocatorMakeBlock(size_t blockSize);
        void                    AllocatorFreeBlock(void *freeBlock);
        void *                  AllocatorResizeBlock(void *block, size_t newSize);
        size_t                  AllocatorGetBlockSize(void *thisBlock);
        
        nsHeapChunk*            AllocateChunk(size_t requestedBlockSize);
        void                    FreeChunk(nsHeapChunk *chunkToFree);

        inline nsHeapChunk*     FindChunkWithSpace(size_t blockSize) const;
        
        UInt32                  GetAllocatorBlockSize() { return mMaxBlockSize; }
        
    protected:

        enum {
            kMaxBlockResizeSlop     = 16
        }; 
        
        nsFixedSizeHeapChunk    *mChunkWithSpace;   // cheap optimization
        
        
#if STATS_MAC_MEMORY
        UInt32          mChunksAllocated;
        UInt32          mMaxChunksAllocated;
        
        UInt32          mTotalChunkSize;
        UInt32          mMaxTotalChunkSize;

        UInt32          mBlocksAllocated;
        UInt32          mMaxBlocksAllocated;
        
        UInt32          mBlocksUsed;
        UInt32          mMaxBlocksUsed;
        
        UInt32          mBlockSpaceUsed;
        UInt32          mMaxBlockSpaceUsed;
#endif

};



