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


#include <MacTypes.h>

#if STATS_MAC_MEMORY
#include "prio.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t blockSize);
void free(void *deadBlock);
void* realloc(void* block, size_t newSize);
void *calloc(size_t nele, size_t elesize);


#if STATS_MAC_MEMORY
void WriteString(PRFileDesc *file, const char * string);
#endif


#ifdef __cplusplus
}
#endif


//--------------------------------------------------------------------
class nsHeapZoneHeader
{
    public:
    
                                nsHeapZoneHeader(Ptr zonePtr, Size ptrSize);
                                nsHeapZoneHeader(Handle zoneHandle, Size handleSize);
                                ~nsHeapZoneHeader();
                                
        nsHeapZoneHeader *      GetNextZone()                           { return mNextHeapZone; }
        void                    SetNextZone(nsHeapZoneHeader *nextZone) { mNextHeapZone = nextZone; }

        Ptr                     AllocateZonePtr(Size ptrSize);
        void                    DisposeZonePtr(Ptr thePtr, Boolean &outWasLastChunk);
        
#if DEBUG_HEAP_INTEGRITY
        Boolean                 IsGoodZone()    { return (mSignature == kHeapZoneSignature); }
#endif

        static nsHeapZoneHeader*    GetZoneFromPtr(Ptr subheapPtr);
        
    protected:

        void                    SetupHeapZone(Ptr zonePtr, Size zoneSize);      
        
        enum
        {
            kHeapZoneMasterPointers = 24            // this number doesn't really matter, because we never
                                                    // allocate handles in our heap zones
        };

        
#if DEBUG_HEAP_INTEGRITY
        enum {
            kHeapZoneSignature = 'HZne'
        };

        OSType                  mSignature;
#endif
        
        nsHeapZoneHeader        *mNextHeapZone;
        Handle                  mZoneHandle;        // the handle containing the zone. Nil if Ptr in app heap
        UInt32                  mChunkCount;        // how many chunks are allocated in this zone
        THz                     mHeapZone;
};




class nsAllocatorManager
{
    public:

        static const SInt32         kNumMasterPointerBlocks;
        static const SInt32         kApplicationStackSizeIncrease;
        
        static const float          kHeapZoneHeapPercentage;
        static const SInt32         kTempMemHeapZoneSize;
        static const SInt32         kTempMemHeapMinZoneSize;
        
        static const Size           kChunkSizeMultiple;
        static const Size           kMaxChunkSize;
        
        static const SInt32         kSmallHeapByteRange;
        
        static nsAllocatorManager*  GetAllocatorManager() { return sAllocatorManager ? sAllocatorManager : CreateAllocatorManager(); }

                                    nsAllocatorManager();
                                    ~nsAllocatorManager();
                                    
        OSErr                       InitializeAllocators();
        
        static OSErr                InitializeMacMemory(SInt32 inNumMasterPointerBlocks,
                                                            SInt32 inAppStackSizeInc);
        
        inline nsMemAllocator*      GetAllocatorForBlockSize(size_t blockSize);

        static nsMemAllocator*      GetAllocatorFromBlock(void *thisBlock)
                                    {
                                        MemoryBlockHeader   *blockHeader = MemoryBlockHeader::GetHeaderFromBlock(thisBlock);
                                    #if DEBUG_HEAP_INTEGRITY
                                        MEM_ASSERT(blockHeader->HasHeaderTag(kUsedBlockHeaderTag), "Bad block header tag");
                                        MEM_ASSERT(blockHeader->owningChunk->IsGoodChunk(), "Block has bad chunk pointer");
                                    #endif
                                        return (blockHeader->owningChunk->GetOwningAllocator());
                                    }
        
        
        static size_t               GetBlockSize(void *thisBlock);

        
        Ptr                         AllocateSubheap(Size preferredSize, Size &outActualSize);
        void                        FreeSubheap(Ptr subheapPtr);        

#if STATS_MAC_MEMORY
        void                        DumpMemoryStats();
#endif

        static nsAllocatorManager*  CreateAllocatorManager();

    protected:
    

        static const Size           kMacMemoryPtrOvehead;

        nsHeapZoneHeader *          MakeNewHeapZone(Size zoneSize, Size minZoneSize);
        
    private:
    
        SInt32                      mNumFixedSizeAllocators;
        SInt32                      mNumSmallBlockAllocators;
        
        UInt32                      mMinSmallBlockSize;         // blocks >= this size come out of the small block allocator
        UInt32                      mMinLargeBlockSize;         // blocks >= this size come out of the large allocator
        
        nsMemAllocator**            mFixedSizeAllocators;       // array of pointers to allocator objects
        nsMemAllocator**            mSmallBlockAllocators;      // array of pointers to allocator objects
    
        nsMemAllocator*             mLargeAllocator;

        nsHeapZoneHeader*           mFirstHeapZone;             // first of a linked list of heap zones
        nsHeapZoneHeader*           mLastHeapZone;              // last of a linked list of heap zones
        
        THz                         mHeapZone;                  // the heap zone for our memory heaps

        static nsAllocatorManager   *sAllocatorManager;     

};

