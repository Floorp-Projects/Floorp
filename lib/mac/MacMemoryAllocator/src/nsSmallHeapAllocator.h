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
class nsSmallHeapChunk;


struct SmallHeapBlock
{
    
    enum {
        kBlockPaddingBytes  = 'ננננ',
        kBlockInUseFlag     = 0x8000,
        kBlockPaddingMask   = 0x0003
    };

    static const UInt32         kBlockOverhead;
    
    static SmallHeapBlock*      GetBlockHeader(void *block) { return (SmallHeapBlock *)((char *)block - sizeof(SmallHeapBlock)); }
    
    SmallHeapBlock*             GetNextBlock()      { return (SmallHeapBlock *)((UInt32)this + GetBlockHeapUsage()); }
    SmallHeapBlock*             GetPrevBlock()      { return prevBlock; }
    
    void                        SetPrevBlock(SmallHeapBlock *prev)      { prevBlock = prev; }
    void                        SetBlockSize(UInt32 inBlockSize)        { blockSize = inBlockSize;  }

    UInt32                      GetBlockSize()      { return blockSize; }
    UInt32                      GetBlockHeapUsage() { return (GetBlockSize() + kBlockOverhead); }
    
    Boolean                     IsBlockUsed()       { return (blockFlags & kBlockInUseFlag) != 0; }

    void                        SetBlockUsed()      { blockFlags = 0; blockFlags |= kBlockInUseFlag; }
    void                        SetBlockUnused()    { blockFlags &= ~kBlockInUseFlag; }
    
    void                        SetNextFree(SmallHeapBlock *next)   { info.freeInfo.nextFree = next; }
    void                        SetPrevFree(SmallHeapBlock *prev)   { info.freeInfo.prevFree = prev; }
    
    SmallHeapBlock*             GetNextFree()       { return info.freeInfo.nextFree; }
    SmallHeapBlock*             GetPrevFree()       { return info.freeInfo.prevFree; }
    
    
    void                        SetOwningChunk(nsHeapChunk *inOwningChunk) { info.inUseInfo.freeProc.owningChunk = inOwningChunk; }
    nsSmallHeapChunk*           GetOwningChunk()    { return (nsSmallHeapChunk *)info.inUseInfo.freeProc.owningChunk; }
    
#if DEBUG_HEAP_INTEGRITY
    void                        SetPaddingBytes(UInt32 padding) { blockFlags &= ~kBlockPaddingMask; blockFlags |= padding; }
    void                        FillPaddingBytes()              {
                                                                    UInt32  padding     = blockFlags & kBlockPaddingMask;
                                                                    long    *lastLong   = (long *)((char *)&memory + blockSize - sizeof(long));
                                                                    UInt32  mask        = (1 << (8 * padding)) - 1;
                                                                    *lastLong &= ~mask;
                                                                    *lastLong |= (mask & kBlockPaddingBytes);
                                                                }

    Boolean                     CheckPaddingBytes()             {
                                                                    UInt32  padding     = blockFlags & kBlockPaddingMask;
                                                                    long    *lastLong   = (long *)((char *)&memory + blockSize - sizeof(long));
                                                                    UInt32  mask        = (1 << (8 * padding)) - 1;
                                                                    return (*lastLong & mask) == (mask & kBlockPaddingBytes);
                                                                }

    UInt32                      GetPaddingBytes()               { return (blockFlags & kBlockPaddingMask); }

    void                        ZapBlockContents(UInt8 pattern) { memset(&memory, pattern, blockSize); }

    // inline, so won't crash if this is a bad block
    Boolean                     HasHeaderTag(MemoryBlockTag inHeaderTag)
                                        { return info.inUseInfo.freeProc.headerTag == inHeaderTag; }

    void                        SetHeaderTag(MemoryBlockTag inHeaderTag)
                                        { info.inUseInfo.freeProc.headerTag = inHeaderTag; }

    void                        SetTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
                                            {
                                                MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
                                                trailer->trailerTag = theTag;
                                            }
    Boolean                     HasTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
                                            {
                                                MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
                                                return (trailer->trailerTag == theTag);
                                            }
#endif

#if STATS_MAC_MEMORY
    size_t                      GetLogicalBlockSize()                   { return info.inUseInfo.freeProc.logicalBlockSize; }
    void                        SetLogicalBlockSize(size_t blockSize)   { info.inUseInfo.freeProc.logicalBlockSize = blockSize; }
#endif
    
    private:
    
        SmallHeapBlock              *prevBlock;
        UInt16                      blockFlags;         // the top bit is the in use flag, the bottom 3 bits padding bytes
        UInt16                      blockSize;
        union {
            struct {
                SmallHeapBlock      *nextFree;
                SmallHeapBlock      *prevFree;
            }                       freeInfo;
            struct {
                UInt32              filler;
                MemoryBlockHeader   freeProc;       // this must be the last variable before memory
            }                       inUseInfo;
        }                           info;
    
    public:
        char                        memory[];
};


//--------------------------------------------------------------------
class nsSmallHeapAllocator : public nsMemAllocator
{
    friend class nsSmallHeapChunk;

    private:
    
        typedef nsMemAllocator  Inherited;

    public:
            
                                nsSmallHeapAllocator(size_t minBlockSize, size_t maxBlockSize);
                                ~nsSmallHeapAllocator();

        void *                  AllocatorMakeBlock(size_t blockSize);
        void                    AllocatorFreeBlock(void *freeBlock);
        void *                  AllocatorResizeBlock(void *block, size_t newSize);
        size_t                  AllocatorGetBlockSize(void *thisBlock);
        
        nsHeapChunk*            AllocateChunk(size_t blockSize);
        void                    FreeChunk(nsHeapChunk *chunkToFree);


    protected:

        nsSmallHeapChunk*       mChunkWithSpace;        // cheap optimization
};


//--------------------------------------------------------------------
class nsSmallHeapChunk : public nsHeapChunk
{
    public:
        
                                nsSmallHeapChunk(nsMemAllocator *inOwningAllocator, Size heapSize);
                                ~nsSmallHeapChunk();
        
        void *                  GetSpaceForBlock(UInt32 roundedBlockSize);
        void                    ReturnBlock(SmallHeapBlock *deadBlock);
        
        void *                  GrowBlock(SmallHeapBlock *growBlock, size_t newSize);
        void *                  ShrinkBlock(SmallHeapBlock *shrinkBlock, size_t newSize);
        void *                  ResizeBlockInPlace(SmallHeapBlock *shrinkBlock, size_t newSize);
        
    protected:
    
        enum {
            kDefaultSmallHeadMinSize    = 4L,
            kDefaultSmallHeapBins       = 128L,
            kMaximumBinBlockSize        = kDefaultSmallHeadMinSize + 4L * kDefaultSmallHeapBins - 1,
            kMaximumBlockSize           = 0xFFFF
        };

        SmallHeapBlock**        GetBins(UInt32 binIndex)
                                                {
                                                    MEM_ASSERT(binIndex < kDefaultSmallHeapBins, "Bad bin index!");
                                                    return mBins + binIndex;
                                                }

        SmallHeapBlock*         GetOverflow()   { return mOverflow; }

        void                    RemoveBlockFromFreeList(SmallHeapBlock *removeBlock);
        void                    AddBlockToFreeList(SmallHeapBlock *addBlock);

        UInt32                  mTotalFree;
#if DEBUG
        UInt32                  mInitialFree;
#endif
        SmallHeapBlock*         mOverflow;
        SmallHeapBlock*         mBins[kDefaultSmallHeapBins];
        SmallHeapBlock          mMemory[];

};


