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

#include <stdlib.h>
#include <string.h>

#include <new.h>			// for placement new

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


/*	prototypes	*/

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
:	mNextHeapZone(nil)
,	mZoneHandle(nil)
,	mChunkCount(0)
,	mHeapZone(nil)
#if DEBUG_HEAP_INTEGRITY
,	mSignature(kHeapZoneSignature)
#endif
// This ctor is used for zones in our heap, which are allocated with
// NewPtr
//--------------------------------------------------------------------
{
	SetupHeapZone(zonePtr, ptrSize);
}


//--------------------------------------------------------------------
nsHeapZoneHeader::nsHeapZoneHeader(Handle zoneHandle, Size handleSize)
:	mNextHeapZone(nil)
,	mZoneHandle(zoneHandle)
,	mChunkCount(0)
,	mHeapZone(nil)
#if DEBUG_HEAP_INTEGRITY
,	mSignature(kHeapZoneSignature)
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
	Ptr		zoneStart = zonePtr + sizeof(nsHeapZoneHeader);
	Ptr		endZone = zonePtr + ptrSize;
	
	::InitZone(nil, kHeapZoneMasterPointers, endZone, zoneStart);
	
	mHeapZone = (THz)zoneStart;
	
	// set the current zone back to the application zone, because InitZone changes it
	::SetZone(::ApplicationZone());
}


//--------------------------------------------------------------------
Ptr nsHeapZoneHeader::AllocateZonePtr(Size ptrSize)
//--------------------------------------------------------------------
{
	::SetZone(mHeapZone);
	Ptr		thePtr = ::NewPtr(ptrSize);
	::SetZone(::ApplicationZone());

	mChunkCount += (thePtr != nil);
	
	return thePtr;
}


//--------------------------------------------------------------------
void nsHeapZoneHeader::DisposeZonePtr(Ptr thePtr, Boolean &outWasLastChunk)
//--------------------------------------------------------------------
{
	MEM_ASSERT(::PtrZone(thePtr) == mHeapZone, "Ptr disposed from wrong zone!");
	::DisposePtr(thePtr);
	mChunkCount --;
	outWasLastChunk = (mChunkCount == 0);
}


//--------------------------------------------------------------------
/* static */ nsHeapZoneHeader* nsHeapZoneHeader::GetZoneFromPtr(Ptr subheapPtr)
//--------------------------------------------------------------------
{
	THz		ptrZone = ::PtrZone(subheapPtr);
	MEM_ASSERT(ptrZone && (::MemError() == noErr), "Problem getting zone from ptr");
	
	return (nsHeapZoneHeader *)((char *)ptrZone - sizeof(nsHeapZoneHeader));
}



#pragma mark -


const SInt32	nsAllocatorManager::kNumMasterPointerBlocks = 30;
const SInt32	nsAllocatorManager::kApplicationStackSizeIncrease = (32 * 1024);
const SInt32	nsAllocatorManager::kHeapZoneHeapPercentage = 60;

nsAllocatorManager* 	nsAllocatorManager::sAllocatorManager = nil;

//--------------------------------------------------------------------
nsAllocatorManager::nsAllocatorManager()
:	mFixedSizeAllocators(nil)
,	mSmallBlockAllocators(nil)
,	mLargeAllocator(nil)
,	mFirstHeapZone(nil)
,	mLastHeapZone(nil)
//--------------------------------------------------------------------
{
	mMinSmallBlockSize = 44;			// some magic numbers for now
	mMinLargeBlockSize = 256;
	
	mNumFixedSizeAllocators = mMinSmallBlockSize / 4;
	mNumSmallBlockAllocators = 1 + (mMinLargeBlockSize - mMinSmallBlockSize) / 4;

	//can't use new yet!
	mFixedSizeAllocators = (nsMemAllocator **)NewPtrClear(mNumFixedSizeAllocators * sizeof(nsMemAllocator*));
	if (mFixedSizeAllocators == nil)
		throw((OSErr)memFullErr);

	mSmallBlockAllocators = (nsMemAllocator **)NewPtrClear(mNumSmallBlockAllocators * sizeof(nsMemAllocator*));
	if (mSmallBlockAllocators == nil)
		throw((OSErr)memFullErr);
		
	for (SInt32 i = 0; i < mNumFixedSizeAllocators; i ++)
	{
		mFixedSizeAllocators[i] = (nsMemAllocator *)NewPtr(sizeof(nsFixedSizeAllocator));
		if (mFixedSizeAllocators[i] == nil)
			throw((OSErr)memFullErr);
		new (mFixedSizeAllocators[i]) nsFixedSizeAllocator((i + 1) * 4);
	}

	for (SInt32 i = 0; i < mNumSmallBlockAllocators; i ++)
	{
		mSmallBlockAllocators[i] = (nsMemAllocator *)NewPtr(sizeof(nsSmallHeapAllocator));
		if (mSmallBlockAllocators[i] == nil)
			throw((OSErr)memFullErr);
		new (mSmallBlockAllocators[i]) nsSmallHeapAllocator();
	}
	
	mLargeAllocator = (nsMemAllocator *)NewPtr(sizeof(nsLargeHeapAllocator));
	if (mLargeAllocator == nil)
		throw((OSErr)memFullErr);
	new (mLargeAllocator) nsLargeHeapAllocator();
	
	// make the heap zone for our subheaps
	UInt32		heapZoneSize;
	
	heapZoneSize = ( kHeapZoneHeapPercentage * ::FreeMem() ) / 100;
	heapZoneSize = ( ( heapZoneSize + 3 ) & ~3 );		// round up to a multiple of 4 bytes

	nsHeapZoneHeader	*firstZone = MakeNewHeapZone(heapZoneSize, heapZoneSize);
	if (!firstZone)
		throw((OSErr)memFullErr);
	
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
nsMemAllocator* nsAllocatorManager::GetAllocatorForBlockSize(size_t blockSize)
//--------------------------------------------------------------------
{
	if (blockSize < mMinSmallBlockSize)
		return mFixedSizeAllocators[ (blockSize == 0) ? 0 : ((blockSize + 3) >> 2) - 1 ];
	
	if (blockSize < mMinLargeBlockSize)
		return mSmallBlockAllocators[ ((blockSize + 3) >> 2) - mNumFixedSizeAllocators ];
	
	return mLargeAllocator;
}

//--------------------------------------------------------------------
nsHeapZoneHeader* nsAllocatorManager::MakeNewHeapZone(Size zoneSize, Size minZoneSize)
//--------------------------------------------------------------------
{
	if (mFirstHeapZone == nil)
	{
		Ptr	firstZonePtr = ::NewPtr(zoneSize);
		
		if (!firstZonePtr) return nil;

		mFirstHeapZone = new (firstZonePtr) nsHeapZoneHeader(firstZonePtr, zoneSize);
		mLastHeapZone = mFirstHeapZone;
	}
	else
	{
		OSErr		err;
		Handle		tempMemHandle = ::TempNewHandle(zoneSize, &err);
		
		while (!tempMemHandle && zoneSize > minZoneSize)
		{
			zoneSize -= (128 * 1024);
			tempMemHandle = ::TempNewHandle(zoneSize, &err);
		}
	
		if (!tempMemHandle) return nil;
	
		// first, lock the handle hi
		HLockHi(tempMemHandle);
	
		nsHeapZoneHeader	*newZone = new (*tempMemHandle) nsHeapZoneHeader(tempMemHandle, zoneSize);
		mLastHeapZone->SetNextZone(newZone);
		mLastHeapZone = newZone;
	}

	return mLastHeapZone;
}


// block size multiple. All blocks should be multiples of this size,
// to reduce heap fragmentation
const Size nsAllocatorManager::kChunkSizeMultiple 	= 2 * 1024;
const Size nsAllocatorManager::kMacMemoryPtrOvehead	= 16;							// this overhead is documented in IM:Memory 2-22
const Size nsAllocatorManager::kTempMemHeapZoneSize	= 1024 * 1024;		// 1MB temp handles
const Size nsAllocatorManager::kTempMemHeapMinZoneSize	= 256 * 1024;	// min 256K handle


//--------------------------------------------------------------------
Ptr nsAllocatorManager::AllocateSubheap(Size preferredSize, Size &outActualSize)
//--------------------------------------------------------------------
{
	nsHeapZoneHeader		*thisHeapZone = mFirstHeapZone;
	
	// calculate an ideal chunk size by rounding up
	preferredSize = kChunkSizeMultiple * ((preferredSize + (kChunkSizeMultiple - 1)) / kChunkSizeMultiple);
	
	// take into accound the memory manager's pointer overhead (16 btyes), to avoid fragmentation
	preferredSize += ((preferredSize / kChunkSizeMultiple) - 1) * kMacMemoryPtrOvehead;
	outActualSize = preferredSize;

	while (thisHeapZone)
	{
		Ptr		subheapPtr = thisHeapZone->AllocateZonePtr(preferredSize);
		if (subheapPtr)
			return subheapPtr;
	
		thisHeapZone = thisHeapZone->GetNextZone();
	}

	// we failed to allocate. Let's make a new heap zone
	thisHeapZone = MakeNewHeapZone(kTempMemHeapZoneSize, kTempMemHeapMinZoneSize);
	if (thisHeapZone)
		return thisHeapZone->AllocateZonePtr(preferredSize);
		
	return nil;
}


//--------------------------------------------------------------------
void nsAllocatorManager::FreeSubheap(Ptr subheapPtr)
//--------------------------------------------------------------------
{
	nsHeapZoneHeader		*ptrHeapZone = nsHeapZoneHeader::GetZoneFromPtr(subheapPtr);
	
#if DEBUG_HEAP_INTEGRITY
	MEM_ASSERT(ptrHeapZone->IsGoodZone(), "Got bad heap zone header");
#endif
	
	Boolean					lastChunk;
	ptrHeapZone->DisposeZonePtr(subheapPtr, lastChunk);
	
	if (lastChunk)
	{
		// remove from the list
		nsHeapZoneHeader		*prevZone = nil;
		nsHeapZoneHeader		*nextZone = nil;
		nsHeapZoneHeader		*thisZone = mFirstHeapZone;
			
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
		ptrHeapZone->~nsHeapZoneHeader();		// this disposes the ptr/handle
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
	SetApplLimit(GetApplLimit() - inAppStackSizeInc);	

#if !TARGET_CARBON 
	MaxApplZone();
#endif

	for (SInt32 i = 1; i <= inNumMasterPointerBlocks; i++)
		MoreMasters();

	// initialize our allocator object. We have to do this through NewPtr
	// and placement new, because we can't call new yet.
	Ptr		allocatorManager = NewPtr(sizeof(nsAllocatorManager));
	if (!allocatorManager) return memFullErr;
	
	try
	{
		// use placement new. The constructor can throw
		sAllocatorManager = new (allocatorManager) nsAllocatorManager;
	}
	catch (OSErr err)
	{
		return err;
	}
	catch (...)
	{
		return paramErr;
	}
	
	return noErr;
}


//--------------------------------------------------------------------
/* static */ nsAllocatorManager * nsAllocatorManager::CreateAllocatorManager()
//--------------------------------------------------------------------
{
	if (sAllocatorManager) return sAllocatorManager;
	
	if (InitializeMacMemory(kNumMasterPointerBlocks, kApplicationStackSizeIncrease) != noErr)
		::ExitToShell();
	
	return sAllocatorManager;
}



#if STATS_MAC_MEMORY
//--------------------------------------------------------------------
void nsAllocatorManager::DumpMemoryStats()
//--------------------------------------------------------------------
{
	UInt32			i;
	char			outString[ 1024 ];
	PRFileDesc		*outFile;
	
	// Enter a valid, UNIX-style full path on your system to get this
	// to work.
	outFile = PR_Open("/System/MemoryStats.txt", PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0644);
	if ( outFile == NULL )
	{
		return;
	}

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
	long	len;
	long	bytesWritten;
	
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
	nsMemAllocator	*allocator = nsAllocatorManager::GetAllocatorManager()->GetAllocatorForBlockSize(blockSize);
	MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "This allocator ain't no good");

#if DEBUG_HEAP_INTEGRITY
	void		*newBlock = allocator->AllocatorMakeBlock(blockSize);
	if (newBlock)
	{
		MemoryBlockHeader	*blockHeader = MemoryBlockHeader::GetHeaderFromBlock(newBlock);
		
		static UInt32		sBlockID = 0;
		blockHeader->blockID = sBlockID++;
	}
	return newBlock;
#else
	return allocator->AllocatorMakeBlock(blockSize);
#endif
}


//--------------------------------------------------------------------
void free(void *deadBlock)
//--------------------------------------------------------------------
{
	if (!deadBlock) return;
	nsMemAllocator	*allocator = nsMemAllocator::GetAllocatorFromBlock(deadBlock);
	MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "Can't get block's allocator on free. The block is hosed");
	allocator->AllocatorFreeBlock(deadBlock);
}


//--------------------------------------------------------------------
void* realloc(void* block, size_t newSize)
//--------------------------------------------------------------------
{
	void	*newBlock = nil;
	
	if (block)
	{
		nsMemAllocator	*allocator = nsMemAllocator::GetAllocatorFromBlock(block);
		MEM_ASSERT(allocator && allocator->IsGoodAllocator(), "Can't get block's allocator on realloc. The block is hosed");
		newBlock = allocator->AllocatorResizeBlock(block, newSize);
		if (newBlock) return newBlock;
	} 
	
	newBlock = ::malloc(newSize);
	if (!newBlock) return nil;
	
	if (block)
	{
		size_t		oldSize = nsMemAllocator::GetBlockSize(block);
		BlockMoveData(block, newBlock, newSize < oldSize ? newSize : oldSize);
		::free(block);
	}
	return newBlock;
}


//--------------------------------------------------------------------
void *calloc(size_t nele, size_t elesize)
//--------------------------------------------------------------------
{
	size_t	space = nele * elesize;
	void	*newBlock = ::malloc(space);
	if (newBlock)
		BlockZero(newBlock, space);
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
	OSErr	err = __initialize(theInitBlock);
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


