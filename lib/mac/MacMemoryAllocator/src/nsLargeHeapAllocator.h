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
class nsLargeHeapChunk;

//--------------------------------------------------------------------
struct LargeBlockHeader
{
    
    static LargeBlockHeader *GetBlockHeader(void *block)    { return (LargeBlockHeader *)((char *)block - sizeof(LargeBlockHeader));    }
    
    Boolean                 IsFreeBlock()   { return prev == nil; }
    UInt32                  GetBlockSize()  { return ((UInt32)next - (UInt32)this - kLargeBlockOverhead);   }


    UInt32                  GetBlockHeapUsageSize() { return ((UInt32)next - (UInt32)this); }
    
    void                    SetLogicalSize(UInt32 inSize) { logicalSize = inSize; }
    UInt32                  GetLogicalSize() { return logicalSize; }
    
    LargeBlockHeader*       SkipDummyBlock() { return (LargeBlockHeader *)((UInt32)this + kLargeBlockOverhead); }
    
    
    LargeBlockHeader*       GetNextBlock()  { return next; }
    LargeBlockHeader*       GetPrevBlock()  { return prev; }

    void                    SetNextBlock(LargeBlockHeader *inNext)  { next = inNext; }
    void                    SetPrevBlock(LargeBlockHeader *inPrev)  { prev = inPrev; }
    
    void                    SetOwningChunk(nsHeapChunk *chunk)      { header.owningChunk = chunk; }
    nsLargeHeapChunk*       GetOwningChunk() { return (nsLargeHeapChunk *)(header.owningChunk); }
    
#if DEBUG_HEAP_INTEGRITY
    enum {
        kBlockPaddingBytes  = 'ננננ'
    };
    
    void                    SetPaddingBytes(UInt32 padding) { paddingBytes = padding; }
    void                    FillPaddingBytes()              {
                                                                    long    *lastLong   = (long *)((char *)&memory + GetBlockSize() - sizeof(long));
                                                                    UInt32  mask        = (1 << (8 * paddingBytes)) - 1;
                                                                    
                                                                    *lastLong &= ~mask;
                                                                    *lastLong |= (mask & kBlockPaddingBytes);
                                                            }
    Boolean                 CheckPaddingBytes()             {
                                                                    long    *lastLong   = (long *)((char *)&memory + GetBlockSize() - sizeof(long));
                                                                    UInt32  mask        = (1 << (8 * paddingBytes)) - 1;
                                                                    return (*lastLong & mask) == (mask & kBlockPaddingBytes);
                                                            }
    UInt32                  GetPaddingBytes()               { return paddingBytes; }

    void                    ZapBlockContents(UInt8 pattern) { memset(&memory, pattern, GetBlockSize()); }
    
    // inline, so won't crash if this is a bad block
    Boolean                 HasHeaderTag(MemoryBlockTag inHeaderTag)
                                            { return header.headerTag == inHeaderTag; }
    void                    SetHeaderTag(MemoryBlockTag inHeaderTag)
                                            { header.headerTag = inHeaderTag; }
    void                    SetTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
                                            {
                                                MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
                                                trailer->trailerTag = theTag;
                                            }
    Boolean                 HasTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
                                            {
                                                MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
                                                return (trailer->trailerTag == theTag);
                                            }
#endif

    static const UInt32     kLargeBlockOverhead;
    
    LargeBlockHeader        *next;
    LargeBlockHeader        *prev;

#if DEBUG_HEAP_INTEGRITY
    UInt32                  paddingBytes;
#endif

    UInt32                  logicalSize;
    MemoryBlockHeader       header;     // this must be the last variable before memory

    char                    memory[];
};


//--------------------------------------------------------------------
class nsLargeHeapAllocator : public nsMemAllocator
{
    private:
    
        typedef nsMemAllocator      Inherited;
    
    
    public:
                                nsLargeHeapAllocator(size_t minBlockSize, size_t maxBlockSize);
                                ~nsLargeHeapAllocator();


        void *              AllocatorMakeBlock(size_t blockSize);
        void                AllocatorFreeBlock(void *freeBlock);
        void *              AllocatorResizeBlock(void *block, size_t newSize);
        size_t              AllocatorGetBlockSize(void *thisBlock);

        nsHeapChunk*            AllocateChunk(size_t requestedBlockSize);
        void                    FreeChunk(nsHeapChunk *chunkToFree);

        UInt32                  GetPaddedBlockSize(UInt32 blockSize)  { return ((blockSize + 3) & ~3) + LargeBlockHeader::kLargeBlockOverhead; }

    protected:

        UInt32                  mBaseChunkPercentage;
        UInt32                  mBestTempChunkSize;
        UInt32                  mSmallestTempChunkSize;

};



//--------------------------------------------------------------------
class nsLargeHeapChunk : public nsHeapChunk
{
    public:
    
                            nsLargeHeapChunk(   nsMemAllocator *inOwningAllocator,
                                                Size            heapSize);
                            ~nsLargeHeapChunk();

        LargeBlockHeader*   GetHeadBlock()      { return mHead; }
                                                                                            
        LargeBlockHeader*   GetSpaceForBlock(UInt32 roundedBlockSize);

        void *              GrowBlock(LargeBlockHeader *growBlock, size_t newSize);
        void *              ShrinkBlock(LargeBlockHeader *shrinkBlock, size_t newSize);
        void *              ResizeBlockInPlace(LargeBlockHeader *theBlock, size_t newSize);
        
        void                ReturnBlock(LargeBlockHeader *deadBlock);
        
        UInt32      GetLargestFreeBlock()   { return mLargestFreeBlock; }
        
    protected:
    
        void                UpdateLargestFreeBlock();
        
        UInt32              mTotalFree;
        UInt32              mLargestFreeBlock;      // heap useage for largest block we can allocate
        
        LargeBlockHeader*   mTail;
        LargeBlockHeader    mHead[];
};



