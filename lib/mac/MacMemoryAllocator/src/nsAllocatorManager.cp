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

#include <new.h>			// for placement new

#include <MacTypes.h>
#include <Memory.h>
#include <Errors.h>
#include <Processes.h>
#include <CodeFragments.h>


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
class nsAllocatorManager
{
	public:

		static const SInt32			kNumMasterPointerBlocks;
		static const SInt32			kApplicationStackSizeIncrease;


									nsAllocatorManager();
									~nsAllocatorManager();
									
		static OSErr				InitializeMacMemory(SInt32 inNumMasterPointerBlocks,
															SInt32 inAppStackSizeInc);

		
		nsMemAllocator*				GetAllocatorForBlockSize(size_t blockSize);

		static nsAllocatorManager*	GetAllocatorManager();
		
	protected:
	
		static nsAllocatorManager	*sAllocatorManager;		
		
	private:
	
		SInt32						mNumFixedSizeAllocators;
		SInt32						mNumSmallBlockAllocators;
		
		UInt32						mMinSmallBlockSize;			// blocks >= this size come out of the small block allocator
		UInt32						mMinLargeBlockSize;			// blocks >= this size come out of the large allocator
		
		nsMemAllocator**			mFixedSizeAllocators;		// array of pointers to allocator objects
		nsMemAllocator**			mSmallBlockAllocators;		// array of pointers to allocator objects
	
		nsMemAllocator*				mLargeAllocator;
};

//--------------------------------------------------------------------



const SInt32	nsAllocatorManager::kNumMasterPointerBlocks = 30;
const SInt32	nsAllocatorManager::kApplicationStackSizeIncrease = (32 * 1024);

nsAllocatorManager* 	nsAllocatorManager::sAllocatorManager = nil;

//--------------------------------------------------------------------
nsAllocatorManager::nsAllocatorManager() :
	mFixedSizeAllocators(nil),
	mSmallBlockAllocators(nil),
	mLargeAllocator(nil)
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
			
		// placement new
		new (mFixedSizeAllocators[i]) nsFixedSizeAllocator((i + 1) * 4);
	}

	for (SInt32 i = 0; i < mNumSmallBlockAllocators; i ++)
	{
		mSmallBlockAllocators[i] = (nsMemAllocator *)NewPtr(sizeof(nsSmallHeapAllocator));
		if (mSmallBlockAllocators[i] == nil)
			throw((OSErr)memFullErr);
			
		// placement new
		new (mSmallBlockAllocators[i]) nsSmallHeapAllocator();
	}
	
	mLargeAllocator = (nsMemAllocator *)NewPtr(sizeof(nsLargeHeapAllocator));
	if (mLargeAllocator == nil)
		throw((OSErr)memFullErr);
	// placement new
	new (mLargeAllocator) nsLargeHeapAllocator();
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
	else if (blockSize < mMinLargeBlockSize)
		return mSmallBlockAllocators[ ((blockSize + 3) >> 2) - mNumFixedSizeAllocators ];
	else
		return mLargeAllocator;
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
	}
	
	return noErr;
}


//--------------------------------------------------------------------
/* static */ nsAllocatorManager * nsAllocatorManager::GetAllocatorManager()
//--------------------------------------------------------------------
{
	if (sAllocatorManager) return sAllocatorManager;
	
	if (InitializeMacMemory(kNumMasterPointerBlocks, kApplicationStackSizeIncrease) != noErr)
		::ExitToShell();
	
	return sAllocatorManager;
}

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
	so we only initialize the memory if we've not alredy done it.s
	
----------------------------------------------------------------------------*/
pascal OSErr __MemInitialize(const CFragInitBlock *theInitBlock)
{
	OSErr	err = __initialize(theInitBlock);
	if (err != noErr) return err;
	
	return nsAllocatorManager::InitializeMacMemory(nsAllocatorManager::kNumMasterPointerBlocks,
													nsAllocatorManager::kApplicationStackSizeIncrease);
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
	__terminate();
}

