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



// temporary defines
//#define DEBUG_MAC_MEMORY			0
//#define TRACK_EACH_ALLOCATOR		0
//#define DEBUG_HEAP_INTEGRITY		DEBUG
//#define STATS_MAC_MEMORY			0

class nsMemAllocator;
class nsHeapChunk;


enum {
	kFreeBlockHeaderTag		= 'FREE',
	kFreeBlockTrailerTag	= 'free',
	kUsedBlockHeaderTag		= 'USED',
	kUsedBlockTrailerTag	= 'used',
	kRefdBlockHeaderTag		= 'REFD',
	kRefdBlockTrailerTag	= 'refd',
	kFreeMemoryFillPattern 	= 0xEF,
	kUsedMemoryFillPattern	= 0xDB
};


typedef	UInt32	MemoryBlockTag;


struct MemoryBlockHeader
{
	nsHeapChunk					*owningChunk;
#if DEBUG_HEAP_INTEGRITY
	//MemoryBlockHeader			*next;
	//MemoryBlockHeader			*prev;
	UInt32						blockID;
	MemoryBlockTag				headerTag;			// put this as the last thing before memory
#endif

	static MemoryBlockHeader*	GetHeaderFromBlock(void *block)	 { return (MemoryBlockHeader*)((char *)block - sizeof(MemoryBlockHeader)); }

#if DEBUG_HEAP_INTEGRITY	
	// inline, so won't crash if this is a bad block
	Boolean				HasHeaderTag(MemoryBlockTag inHeaderTag) { return (headerTag == inHeaderTag); }
	void				SetHeaderTag(MemoryBlockTag inHeaderTag) { headerTag = inHeaderTag; }
#else
	// stubs
	Boolean				HasHeaderTag(MemoryBlockTag inHeaderTag){ return true; }
	void				SetHeaderTag(MemoryBlockTag inHeaderTag){}
#endif
									
};


struct MemoryBlockTrailer {
	MemoryBlockTag		trailerTag;
};

#if DEBUG_HEAP_INTEGRITY
#define MEMORY_BLOCK_TAILER_SIZE sizeof(struct MemoryBlockTrailer)
#else
#define MEMORY_BLOCK_TAILER_SIZE 0
#endif	/* DEBUG_HEAP_INTEGRITY */



//--------------------------------------------------------------------
class nsHeapChunk
{
	public:
		
								nsHeapChunk(nsMemAllocator *inOwningAllocator, Size heapSize, Handle tempMemHandle);
								~nsHeapChunk();
		
		nsHeapChunk*			GetNextChunk() const	{ return mNextChunk; }
		void					SetNextChunk(nsHeapChunk *inChunk)	{ mNextChunk = inChunk;	}
		
		nsMemAllocator*			GetOwningAllocator() const { return mOwningAllocator; }

		void					IncrementUsedBlocks()	{	mUsedBlocks ++;	}
		void					DecrementUsedBlocks()	{	mUsedBlocks -- ;  MEM_ASSERT(mUsedBlocks >= 0, "Bad chunk block count"); }
		
		Boolean					IsEmpty() const			{ return mUsedBlocks == 0;	}
		
		Handle					GetMemHandle()			{ return mTempMemHandle;	}

#if DEBUG_HEAP_INTEGRITY	
		Boolean					IsGoodChunk()			{ return mSignature == kChunkSignature; }
#endif
		
	protected:
#if DEBUG_HEAP_INTEGRITY
		enum {
			kChunkSignature = 'oink'
		};

		OSType					mSignature;
#endif
		nsMemAllocator			*mOwningAllocator;
		nsHeapChunk				*mNextChunk;
		UInt32					mUsedBlocks;
		UInt32					mHeapSize;		
	
	private:
		Handle					mTempMemHandle;			// if nil, this chunk is a pointer in the heap
};




//--------------------------------------------------------------------
class nsMemAllocator
{
	public:

								nsMemAllocator();
		virtual 				~nsMemAllocator();
		
		static size_t			GetBlockSize(void *thisBlock);
		static nsMemAllocator*	GetAllocatorFromBlock(void *thisBlock);
		
		virtual void *			AllocatorMakeBlock(size_t blockSize) = 0;
		virtual void 			AllocatorFreeBlock(void *freeBlock) = 0;
		// AllocatorResizeBlock can return nil if the block cannot be resized in place.
		// realloc then does a malloc and copy. If this returns a valid block, then it
		// is assumed that no coying needs to be done
		virtual void *			AllocatorResizeBlock(void *block, size_t newSize) = 0;
		virtual size_t 			AllocatorGetBlockSize(void *thisBlock) = 0;
		
		virtual nsHeapChunk*	AllocateChunk(size_t requestedBlockSize) = 0;
		virtual void			FreeChunk(nsHeapChunk *chunkToFree) = 0;
		
		Boolean					IsGoodAllocator() 
#if DEBUG_HEAP_INTEGRITY
									{ return mSignature == kMemAllocatorSignature; }
#else
									{ return true; }
#endif

	protected:
		
		static const Size		kFreeHeapSpace;
		static const Size		kChunkSizeMultiple;
		
		Ptr						DoMacMemoryAllocation(Size preferredSize, Size &outActualSize, Handle *outTempMemHandle);
	
		void					AddToChunkList(nsHeapChunk *inNewChunk);
		void					RemoveFromChunkList(nsHeapChunk *inChunk);
		
		enum {
			kMemAllocatorSignature = 'ARSE'
		};
		
#if DEBUG_HEAP_INTEGRITY
		OSType					mSignature;				// signature for debugging
#endif

#if DEBUG_BLOCK_TRACKING
		AllocationSet			*mAllocationSet;		// allocation set used by xp_tracker code
#endif

		nsHeapChunk				*mFirstChunk;			// pointer to first subheap managed by this allocator
		nsHeapChunk				*mLastChunk;			// pointer to last subheap managed by this allocator

		UInt32					mBaseChunkSize;			// size of subheap allocated at startup
		UInt32					mTempChunkSize;			// size of additional subheaps
};


