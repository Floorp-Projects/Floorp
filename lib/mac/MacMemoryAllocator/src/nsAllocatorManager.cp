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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <new.h>            // for placement new

#include <MacTypes.h>
#include <Memory.h>
#include <Errors.h>
#include <Processes.h>
#include <CodeFragments.h>

#if __profile__
#include <Profiler.h>
#endif

#include "nsMemAllocator.h"

#include "nsFixedSizeAllocator.h"
#include "nsSmallHeapAllocator.h"
#include "nsLargeHeapAllocator.h"

#include "nsAllocatorManager.h"


/*  prototypes  */

#ifdef __cplusplus
extern "C" {
#endif

pascal OSErr __initialize(const CFragInitBlock *theInitBlock);
pascal void __terminate(void);

pascal OSErr __MemInitialize(const CFragInitBlock *theInitBlock);
pascal void __MemTerminate(void);

#ifdef __cplusplus
}
#endif


//--------------------------------------------------------------------
nsHeapZoneHeader::nsHeapZoneHeader(Ptr zonePtr, Size ptrSize)
:   mNextHeapZone(nil)
,   mZoneHandle(nil)
,   mChunkCount(0)
,   mHeapZone(nil)
#if DEBUG_HEAP_INTEGRITY
,   mSignature(kHeapZoneSignature)
#endif
// This ctor is used for zones in our heap, which are allocated with
// NewPtr
//--------------------------------------------------------------------
{
    SetupHeapZone(zonePtr, ptrSize);
}


//--------------------------------------------------------------------
nsHeapZoneHeader::nsHeapZoneHeader(Handle zoneHandle, Size handleSize)
:   mNextHeapZone(nil)
,   mZoneHandle(zoneHandle)
,   mChunkCount(0)
,   mHeapZone(nil)
#if DEBUG_HEAP_INTEGRITY
,   mSignature(kHeapZoneSignature)
#endif
// This ctor is used for zones in temporary memory, which are allocated
// with TempNewHandle
//--------------------------------------------------------------------
{
    // handle should be locked here
    SetupHeapZone(*zoneHandle, handleSize);
}

//--------------------------------------------------------------------
nsHeapZoneHeader::~nsHeapZoneHeader()
//--------------------------------------------------------------------
{
    if (mZoneHandle)
        ::DisposeHandle(mZoneHandle);
    else
        ::DisposePtr((Ptr)this);
        
    MEM_ASSERT(::MemError() == noErr, "Error on Mac memory dispose");
}


//--------------------------------------------------------------------
void nsHeapZoneHeader::SetupHeapZone(Ptr zonePtr, Size ptrSize)
//--------------------------------------------------------------------
{
    Ptr     zoneStart = zonePtr + sizeof(nsHeapZoneHeader);
    Ptr     endZone = zonePtr + ptrSize;

#if !TARGET_CARBON  
    ::InitZone(nil, kHeapZoneMasterPointers, endZone, zoneStart);
#endif

    mHeapZone = (THz)zoneStart;

#if !TARGET_CARBON  
    // set the current zone back to the application zone, because InitZone changes it
    ::SetZone(::ApplicationZone());
#endif
}


//--------------------------------------------------------------------
Ptr nsHeapZoneHeader::AllocateZonePtr(Size ptrSize)
//--------------------------------------------------------------------
{
#if !TARGET_CARBON  
    ::SetZone(mHeapZone);
#endif
    Ptr     thePtr = ::NewPtr(ptrSize);
#if !TARGET_CARBON  
    ::SetZone(::ApplicationZone());
#endif
    mChunkCount += (thePtr != nil);
    
    return thePtr;
}


//--------------------------------------------------------------------
void nsHeapZoneHeader::DisposeZonePtr(Ptr thePtr, Boolean &outWasLastChunk)
//--------------------------------------------------------------------
{
#if !TARGET_CARBON  
    MEM_ASSERT(::PtrZone(thePtr) == mHeapZone, "Ptr disposed from wrong zone!");
#endif
    ::DisposePtr(thePtr);
    mChunkCount --;
    outWasLastChunk = (mChunkCount == 0);
}


//--------------------------------------------------------------------
/* static */ nsHeapZoneHeader* nsHeapZoneHeader::GetZoneFromPtr(Ptr subheapPtr)
//--------------------------------------------------------------------
{
#if !TARGET_CARBON  
    THz     ptrZone = ::PtrZone(subheapPtr);
    MEM_ASSERT(ptrZone && (::MemError() == noErr), "Problem getting zone from ptr");
    
    return (nsHeapZoneHeader *)((char *)ptrZone - sizeof(nsHeapZoneHeader));
#else
    return NULL;
#endif
}



#pragma mark -


const SInt32    nsAllocatorManager::kNumMasterPointerBlocks = 30;
const SInt32    nsAllocatorManager::kApplicationStackSizeIncrease = (32 * 1024);
const float     nsAllocatorManager::kHeapZoneHeapPercentage = 0.5;
const SInt32    nsAllocatorManager::kSmallHeapByteRange = 16;

nsAllocatorManager*     nsAllocatorManager::sAllocatorManager = nil;

//--------------------------------------------------------------------
nsAllocatorManager::nsAllocatorManager()
:   mFixedSizeAllocators(nil)
,   mSmallBlockAllocators(nil)
,   mLargeAllocator(nil)
,   mFirstHeapZone(nil)
,   mLastHeapZone(nil)
//--------------------------------------------------------------------
{
    mMinSmallBlockSize = 165;       //128;      //44;           // some magic numbers for now
    mMinLargeBlockSize = 261;       //512;      //256;
    
    mNumFixedSizeAllocators = mMinSmallBlockSize / 4;
    mNumSmallBlockAllocators = 1 + (mMinLargeBlockSize - mMinSmallBlockSize) / kSmallHeapByteRange;
}


//--------------------------------------------------------------------
nsAllocatorManager::~nsAllocatorManager()
//--------------------------------------------------------------------
{
    for (SInt32 i = 0; i < mNumFixedSizeAllocators; i ++)
    {
        // because we used NewPtr and placement new, we have to destruct thus
        mFixedSizeAllocators[i]->~nsMemAllocator();
        DisposePtr((Ptr)mFixedSizeAllocators[i]);
    }
    
    DisposePtr((Ptr)mFixedSizeAllocators);
    
    for (SInt32 i = 0; i < mNumSmallBlockAllocators; i ++)
    {
        // because we used NewPtr and placement new, we have to destruct thus
        mSmallBlockAllocators[i]->~nsMemAllocator();
        DisposePtr((Ptr)mSmallBlockAllocators[i]);
    }

    DisposePtr((Ptr)mSmallBlockAllocators);
    
    mLargeAllocator->~nsMemAllocator();
    DisposePtr((Ptr)mLargeAllocator);

}

//--------------------------------------------------------------------
OSErr nsAllocatorManager::InitializeAllocators()
//--------------------------------------------------------------------
{
    //can't use new yet!
    mFixedSizeAllocators = (nsMemAllocator **)NewPtrClear(mNumFixedSizeAllocators * sizeof(nsMemAllocator*));
    if (mFixedSizeAllocators == nil)
        return memFullErr;

    mSmallBlockAllocators = (nsMemAllocator **)NewPtrClear(mNumSmallBlockAllocators * sizeof(nsMemAllocator*));
    if (mSmallBlockAllocators == nil)
        return memFullErr;
        
    for (SInt32 i = 0; i < mNumFixedSizeAllocators; i ++)
    {
        mFixedSizeAllocators[i] = (nsMemAllocator *)NewPtr(sizeof(nsFixedSizeAllocator));
        if (mFixedSizeAllocators[i] == nil)
            return memFullErr;
        new (mFixedSizeAllocators[i]) nsFixedSizeAllocator(i * 4 + (i > 0) * 1, (i + 1) * 4);
    }

    for (SInt32 i = 0; i < mNumSmallBlockAllocators; i ++)
    {
        mSmallBlockAllocators[i] = (nsMemAllocator *)NewPtr(sizeof(nsSmallHeapAllocator));
        if (mSmallBlockAllocators[i] == nil)
            return memFullErr;
        SInt32      minBytes = mMinSmallBlockSize + i * kSmallHeapByteRange;        // lower bound of block size
        new (mSmallBlockAllocators[i]) nsSmallHeapAllocator(minBytes, minBytes + kSmallHeapByteRange - 1);
    }
    
    mLargeAllocator = (nsMemAllocator *)NewPtr(sizeof(nsLargeHeapAllocator));
    if (mLargeAllocator == nil)
        return memFullErr;
    new (mLargeAllocator) nsLargeHeapAllocator(mMinLargeBlockSize, 0x7FFFFFFF);

    // make the heap zone for our subheaps
    UInt32      heapZoneSize;
    
    heapZoneSize = (UInt32)(kHeapZoneHeapPercentage * ::FreeMem());
    heapZoneSize = ( ( heapZoneSize + 3 ) & ~3 );       // round up to a multiple of 4 bytes

    nsHeapZoneHeader    *firstZone = MakeNewHeapZone(heapZoneSize, heapZoneSize);
    if (!firstZone)
        return memFullErr;
        
    return noErr;   
}


//--------------------------------------------------------------------
nsMemAllocator* nsAllocatorManager::GetAllocatorForBlockSize(size_t blockSize)
//--------------------------------------------------------------------
{
    if (blockSize < mMinSmallBlockSize)
        return mFixedSizeAllocators[ (blockSize == 0) ? 0 : ((blockSize + 3) >> 2) - 1 ];
    
    if (blockSize < mMinLargeBlockSize)
        return mSmallBlockAllocators[ ((blockSize - mMinSmallBlockSize + kSmallHeapByteRange) / kSmallHeapByteRange) - 1 ];
        //return mSmallBlockAllocators[ ((blockSize + (kSmallHeapByteRange - 1)) / kSmallHeapByteRange) - mNumFixedSizeAllocators ];
    
    return mLargeAllocator;
}


//--------------------------------------------------------------------
size_t nsAllocatorManager::GetBlockSize(void *thisBlock)
//--------------------------------------------------------------------
{
    nsMemAllocator*     allocator = GetAllocatorFromBlock(thisBlock);
#if DEBUG_HEAP_INTEGRITY
    MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "Failed to get allocator for block");
#endif

    switch (allocator->GetAllocatorType())
    {
        case nsMemAllocator::eAllocatorTypeFixed:
            return ((nsFixedSizeAllocator*)allocator)->AllocatorGetBlockSize(thisBlock);
            
        case nsMemAllocator::eAllocatorTypeSmall:
            return ((nsSmallHeapAllocator*)allocator)->AllocatorGetBlockSize(thisBlock);
            
        case nsMemAllocator::eAllocatorTypeLarge:
            return ((nsLargeHeapAllocator*)allocator)->AllocatorGetBlockSize(thisBlock);
    }

    return 0;
}


//--------------------------------------------------------------------
nsHeapZoneHeader* nsAllocatorManager::MakeNewHeapZone(Size zoneSize, Size minZoneSize)
//--------------------------------------------------------------------
{
    if (mFirstHeapZone == nil)
    {
        Ptr firstZonePtr = ::NewPtr(zoneSize);
        
        if (!firstZonePtr) return nil;

        mFirstHeapZone = new (firstZonePtr) nsHeapZoneHeader(firstZonePtr, zoneSize);
        mLastHeapZone = mFirstHeapZone;
    }
    else
    {
        OSErr       err;
        Handle      tempMemHandle = ::TempNewHandle(zoneSize, &err);
        
        while (!tempMemHandle && zoneSize > minZoneSize)
        {
            zoneSize -= (128 * 1024);
            tempMemHandle = ::TempNewHandle(zoneSize, &err);
        }
    
        if (!tempMemHandle) return nil;
    
        // first, lock the handle hi
        HLockHi(tempMemHandle);
    
        nsHeapZoneHeader    *newZone = new (*tempMemHandle) nsHeapZoneHeader(tempMemHandle, zoneSize);
        mLastHeapZone->SetNextZone(newZone);
        mLastHeapZone = newZone;
    }

    return mLastHeapZone;
}


// block size multiple. All blocks should be multiples of this size,
// to reduce heap fragmentation
const Size nsAllocatorManager::kChunkSizeMultiple   = 2 * 1024;
const Size nsAllocatorManager::kMaxChunkSize        = 48 * 1024;
const Size nsAllocatorManager::kMacMemoryPtrOvehead = 16;               // this overhead is documented in IM:Memory 2-22
const Size nsAllocatorManager::kTempMemHeapZoneSize = 1024 * 1024;      // 1MB temp handles
const Size nsAllocatorManager::kTempMemHeapMinZoneSize  = 256 * 1024;   // min 256K handle


//--------------------------------------------------------------------
Ptr nsAllocatorManager::AllocateSubheap(Size preferredSize, Size &outActualSize)
//--------------------------------------------------------------------
{
    nsHeapZoneHeader        *thisHeapZone = mFirstHeapZone;
    
    // calculate an ideal chunk size by rounding up
    preferredSize = kChunkSizeMultiple * ((preferredSize + (kChunkSizeMultiple - 1)) / kChunkSizeMultiple);
    
    // take into account the memory manager's pointer overhead (16 btyes), to avoid fragmentation
    preferredSize += ((preferredSize / kChunkSizeMultiple) - 1) * kMacMemoryPtrOvehead;
    outActualSize = preferredSize;

    while (thisHeapZone)
    {
        Ptr     subheapPtr = thisHeapZone->AllocateZonePtr(preferredSize);
        if (subheapPtr)
            return subheapPtr;
    
        thisHeapZone = thisHeapZone->GetNextZone();
    }

    // we failed to allocate. Let's make a new heap zone
    UInt32      prefZoneSize = preferredSize + sizeof(nsHeapZoneHeader) + 512;  // for zone overhead
    UInt32      zoneSize = (kTempMemHeapZoneSize > prefZoneSize) ? kTempMemHeapZoneSize : prefZoneSize;
    UInt32      minZoneSize = (kTempMemHeapMinZoneSize > prefZoneSize) ? kTempMemHeapMinZoneSize : prefZoneSize;
    thisHeapZone = MakeNewHeapZone(zoneSize, minZoneSize);
    if (thisHeapZone)
        return thisHeapZone->AllocateZonePtr(preferredSize);
        
    return nil;
}


//--------------------------------------------------------------------
void nsAllocatorManager::FreeSubheap(Ptr subheapPtr)
//--------------------------------------------------------------------
{
    nsHeapZoneHeader        *ptrHeapZone = nsHeapZoneHeader::GetZoneFromPtr(subheapPtr);
    
#if DEBUG_HEAP_INTEGRITY
    MEM_ASSERT(ptrHeapZone->IsGoodZone(), "Got bad heap zone header");
#endif
    
    Boolean                 lastChunk;
    ptrHeapZone->DisposeZonePtr(subheapPtr, lastChunk);
    
    if (lastChunk)
    {
        // remove from the list
        nsHeapZoneHeader        *prevZone = nil;
        nsHeapZoneHeader        *nextZone = nil;
        nsHeapZoneHeader        *thisZone = mFirstHeapZone;
            
        while (thisZone)
        {
            nextZone = thisZone->GetNextZone();
            
            if (thisZone == ptrHeapZone)
                break;
            
            prevZone = thisZone;
            thisZone = nextZone;
        }
        
        if (thisZone)
        {
            if (prevZone)
                prevZone->SetNextZone(nextZone);
            
            if (mFirstHeapZone == thisZone)
                mFirstHeapZone = nextZone;
            
            if (mLastHeapZone == thisZone)
                mLastHeapZone = prevZone;
        }
        
        // dispose it
        ptrHeapZone->~nsHeapZoneHeader();       // this disposes the ptr/handle
    }
}


//--------------------------------------------------------------------
/* static */ OSErr nsAllocatorManager::InitializeMacMemory(SInt32 inNumMasterPointerBlocks,
                                SInt32 inAppStackSizeInc)
//--------------------------------------------------------------------
{
    if (sAllocatorManager) return noErr;
    
    // increase the stack size by 32k. Someone is bound to have fun with
    // recursion
#if !TARGET_CARBON 
    SetApplLimit(GetApplLimit() - inAppStackSizeInc);   

    MaxApplZone();
#endif

    for (SInt32 i = 1; i <= inNumMasterPointerBlocks; i++)
        MoreMasters();

    // initialize our allocator object. We have to do this through NewPtr
    // and placement new, because we can't call new yet.
    OSErr   err;
    Ptr     allocatorManager = NewPtr(sizeof(nsAllocatorManager));
    if (!allocatorManager) return memFullErr;
    
        // use placement new. The constructor can throw
        sAllocatorManager = new (allocatorManager) nsAllocatorManager;
    err = sAllocatorManager->InitializeAllocators();
    if (err != noErr)
        return err;
    
    return noErr;
}


//--------------------------------------------------------------------
/* static */ nsAllocatorManager * nsAllocatorManager::CreateAllocatorManager()
//--------------------------------------------------------------------
{
    if (sAllocatorManager) return sAllocatorManager;
    
    if (InitializeMacMemory(kNumMasterPointerBlocks, kApplicationStackSizeIncrease) != noErr)
    {
#ifdef DEBUG
        ::DebugStr("\pAllocator Manager initialization failed");
#endif
        ::ExitToShell();
    }
    
    return sAllocatorManager;
}



#if STATS_MAC_MEMORY
//--------------------------------------------------------------------
void nsAllocatorManager::DumpMemoryStats()
//--------------------------------------------------------------------
{
    UInt32          i;
    PRFileDesc      *outFile;
    
    // Enter a valid, UNIX-style full path on your system to get this
    // to work.
    outFile = PR_Open("MemoryStats.txt", PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0644);
    if ( outFile == NULL )
    {
        return;
    }

    WriteString(outFile, "\n\n--------------------------------------------------------------------------------\n");
    WriteString(outFile, "Max heap usage chart (* = 1024 bytes)\n");
    WriteString(outFile, "--------------------------------------------------------------------------------\n\n");

    UInt32          totHeapUsed = 0;

    for (i = 0; i < mNumFixedSizeAllocators; i ++)
    {
        mFixedSizeAllocators[i]->DumpHeapUsage(outFile);
        totHeapUsed += mFixedSizeAllocators[i]->GetMaxHeapUsage();
    }

    for (i = 0; i < mNumSmallBlockAllocators; i ++)
    {
        mSmallBlockAllocators[i]->DumpHeapUsage(outFile);
        totHeapUsed += mSmallBlockAllocators[i]->GetMaxHeapUsage();
    }

    char    outString[256];
    sprintf(outString, "Total heap space used by allocators: %ldk\n", totHeapUsed / 1024);

    WriteString(outFile, "--------------------------------------------------------------------------------\n");
    WriteString(outFile, outString);
    WriteString(outFile, "--------------------------------------------------------------------------------\n\n");

    for (i = 0; i < mNumFixedSizeAllocators; i ++)
    {
        mFixedSizeAllocators[i]->DumpMemoryStats(outFile);
    }

    for (i = 0; i < mNumSmallBlockAllocators; i ++)
    {
        mSmallBlockAllocators[i]->DumpMemoryStats(outFile);
    }
    
    mLargeAllocator->DumpMemoryStats(outFile);
    
    PR_Close(outFile);
}


//--------------------------------------------------------------------
void WriteString(PRFileDesc *file, const char * string)
//--------------------------------------------------------------------
{
    long    len;
    long    bytesWritten;
    
    len = strlen ( string );
    if ( len >= 1024 ) Debugger();
    bytesWritten = PR_Write(file, string, len);
}


#endif

#pragma mark -


//--------------------------------------------------------------------
void *malloc(size_t blockSize)
//--------------------------------------------------------------------
{
    // local static copy is slightly faster
    static nsAllocatorManager* sManager;
    if (!sManager)
      sManager = nsAllocatorManager::GetAllocatorManager();
        
    nsMemAllocator  *allocator = sManager->GetAllocatorForBlockSize(blockSize);
    
    MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "This allocator ain't no good");

#if DEBUG_HEAP_INTEGRITY
    void        *newBlock;

    // we are switching here, instead of using virtual methods, for performance
    switch (allocator->GetAllocatorType())
    {
        case nsMemAllocator::eAllocatorTypeFixed:
            newBlock = ((nsFixedSizeAllocator*)allocator)->AllocatorMakeBlock(blockSize);
            break;
            
        case nsMemAllocator::eAllocatorTypeSmall:
            newBlock = ((nsSmallHeapAllocator*)allocator)->AllocatorMakeBlock(blockSize);
            break;
            
        case nsMemAllocator::eAllocatorTypeLarge:
            newBlock = ((nsLargeHeapAllocator*)allocator)->AllocatorMakeBlock(blockSize);
            break;
    }

    if (newBlock)
    {
        MemoryBlockHeader   *blockHeader = MemoryBlockHeader::GetHeaderFromBlock(newBlock);
        
        static UInt32       sBlockID = 0;
        blockHeader->blockID = sBlockID++;
    }
#if 0
    else
    {
      DebugStr("\pAllocation failure");
    }
#endif
    return newBlock;
#else
    
    // we are switching here, instead of using virtual methods, for performance
    switch (allocator->GetAllocatorType())
    {
        case nsMemAllocator::eAllocatorTypeFixed:
            return ((nsFixedSizeAllocator*)allocator)->AllocatorMakeBlock(blockSize);
            
        case nsMemAllocator::eAllocatorTypeSmall:
            return ((nsSmallHeapAllocator*)allocator)->AllocatorMakeBlock(blockSize);
            
        case nsMemAllocator::eAllocatorTypeLarge:
            return ((nsLargeHeapAllocator*)allocator)->AllocatorMakeBlock(blockSize);
    }
    
    return nil;
    
    //return allocator->AllocatorMakeBlock(blockSize);
#endif
}


//--------------------------------------------------------------------
void free(void *deadBlock)
//--------------------------------------------------------------------
{
    if (!deadBlock) return;
    nsMemAllocator  *allocator = nsAllocatorManager::GetAllocatorFromBlock(deadBlock);
    MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "Can't get block's allocator on free. The block is hosed");

    switch (allocator->GetAllocatorType())
    {
        case nsMemAllocator::eAllocatorTypeFixed:
            ((nsFixedSizeAllocator*)allocator)->AllocatorFreeBlock(deadBlock);
            break;
            
        case nsMemAllocator::eAllocatorTypeSmall:
            ((nsSmallHeapAllocator*)allocator)->AllocatorFreeBlock(deadBlock);
            break;
            
        case nsMemAllocator::eAllocatorTypeLarge:
            ((nsLargeHeapAllocator*)allocator)->AllocatorFreeBlock(deadBlock);
            break;
    }
}


//--------------------------------------------------------------------
void* realloc(void* block, size_t newSize)
//--------------------------------------------------------------------
{
    void    *newBlock = nil;
    
    if (block)
    {
        nsMemAllocator  *allocator = nsAllocatorManager::GetAllocatorFromBlock(block);
        MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "Can't get block's allocator on realloc. The block is hosed");

        switch (allocator->GetAllocatorType())
        {
            case nsMemAllocator::eAllocatorTypeFixed:
                newBlock = ((nsFixedSizeAllocator*)allocator)->AllocatorResizeBlock(block, newSize);
                break;
                
            case nsMemAllocator::eAllocatorTypeSmall:
                newBlock = ((nsSmallHeapAllocator*)allocator)->AllocatorResizeBlock(block, newSize);
                break;
                
            case nsMemAllocator::eAllocatorTypeLarge:
                newBlock = ((nsLargeHeapAllocator*)allocator)->AllocatorResizeBlock(block, newSize);
                break;
        }

        if (newBlock) return newBlock;
    } 
    
    newBlock = ::malloc(newSize);
    if (!newBlock) return nil;
    
    if (block)
    {
        size_t      oldSize = nsAllocatorManager::GetBlockSize(block);
        BlockMoveData(block, newBlock, newSize < oldSize ? newSize : oldSize);
        ::free(block);
    }
    return newBlock;
}


//--------------------------------------------------------------------
void *calloc(size_t nele, size_t elesize)
//--------------------------------------------------------------------
{
    size_t  space = nele * elesize;
    void    *newBlock = ::malloc(space);
    if (newBlock)
        memset(newBlock, 0, space);
    return newBlock;
}



#pragma mark -

/*----------------------------------------------------------------------------
    __MemInitialize 
    
    Note the people can call malloc() or new() before we come here,
    so we can't rely on this being called before we do allocation.
    
----------------------------------------------------------------------------*/
pascal OSErr __MemInitialize(const CFragInitBlock *theInitBlock)
{
    OSErr   err = __initialize(theInitBlock);
    if (err != noErr) return err;
    
#if __profile__
    if (ProfilerInit(collectDetailed, bestTimeBase, 500, 20) != noErr)
        ExitToShell();
#endif

    return noErr;
}


/*----------------------------------------------------------------------------
    __MemTerminate 
    
    Code frag Terminate routine. We could do more tear-down here, but we
    cannot be sure that anyone else doesn't still need to reference 
    memory that we are managing. So we can't just free all the heaps.
    We rely on the Proess Manager to free handles that we left in temp
    mem (see IM: Memory).
    
----------------------------------------------------------------------------*/

pascal void __MemTerminate(void)
{
#if __profile__
    ProfilerDump("\pMemory Tester.prof");
    ProfilerTerm();
#endif

#if STATS_MAC_MEMORY
    nsAllocatorManager::GetAllocatorManager()->DumpMemoryStats();
#endif
    
    __terminate();
}


