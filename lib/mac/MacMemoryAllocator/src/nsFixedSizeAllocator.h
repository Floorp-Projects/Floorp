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


class nsMemAllocator;
class nsFixedSizeHeapChunk;


struct FixedMemoryBlockHeader
{
#if DEBUG_HEAP_INTEGRITY
	UInt16						blockFlags;			// unused at present
	UInt16						blockPadding;
#endif

	MemoryBlockHeader			header;				// this must be the last variable before memory
};


struct FixedMemoryBlock
{
	static FixedMemoryBlock*	GetBlockHeader(void *block)
											{ return (FixedMemoryBlock *)((char *)block - sizeof(FixedMemoryBlockHeader)); }
											
	FixedMemoryBlock*			GetNextFree()
											{ return next; }
	void						SetNextFree(FixedMemoryBlock *nextFree)
											{ next = nextFree; }
	
	void						SetOwningChunk(nsHeapChunk *inOwningChunk)
											{ blockHeader.header.owningChunk = inOwningChunk; }
	
	nsFixedSizeHeapChunk*		GetOwningChunk()
											{ return (nsFixedSizeHeapChunk *)blockHeader.header.owningChunk; }
	
#if DEBUG_HEAP_INTEGRITY
	enum {
		kBlockPaddingBytes	= 'ננננ'
	};
	
	void					SetPaddingBytes(UInt32 padding)		{ blockHeader.blockPadding = padding; }
	void					FillPaddingBytes(UInt32 blockSize)	{
																	long	*lastLong 	= (long *)((char *)&memory + blockSize - sizeof(long));
																	UInt32	mask 		= (1 << (8 * blockHeader.blockPadding)) - 1;
																	*lastLong &= ~mask;
																	*lastLong |= (mask & kBlockPaddingBytes);
																}

	Boolean					CheckPaddingBytes(UInt32 blockSize)	{
																	long	*lastLong 	= (long *)((char *)&memory + blockSize - sizeof(long));
																	UInt32	mask 		= (1 << (8 * blockHeader.blockPadding)) - 1;
																	return (*lastLong & mask) == (mask & kBlockPaddingBytes);
																}
	UInt32					GetPaddingBytes()					{ return blockHeader.blockPadding; }

	// inline, so won't crash if this is a bad block
	Boolean					HasHeaderTag(MemoryBlockTag inHeaderTag)
											{ return blockHeader.header.headerTag == inHeaderTag; }
	void					SetHeaderTag(MemoryBlockTag inHeaderTag)
											{ blockHeader.header.headerTag = inHeaderTag; }
											
	void					SetTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
											{
												MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
												trailer->trailerTag = theTag;
											}
	MemoryBlockTag			GetTrailerTag(UInt32 blockSize)
											{
												MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
												return trailer->trailerTag;
											}
#endif

	static const UInt32				kFixedSizeBlockOverhead;

	FixedMemoryBlockHeader			blockHeader;
	
	union {
		FixedMemoryBlock*			next;
		void*						memory;
	};
};



//--------------------------------------------------------------------
class nsFixedSizeAllocator : public nsMemAllocator
{
	private:
	
		typedef nsMemAllocator	Inherited;
	
	public:

								nsFixedSizeAllocator(THz heapZone, size_t blockSize);
								~nsFixedSizeAllocator();

		virtual void *			AllocatorMakeBlock(size_t blockSize);
		virtual void 			AllocatorFreeBlock(void *freeBlock);
		virtual void *			AllocatorResizeBlock(void *block, size_t newSize);
		virtual size_t 			AllocatorGetBlockSize(void *thisBlock);
		
		virtual nsHeapChunk*	AllocateChunk(size_t requestedBlockSize);
		virtual void			FreeChunk(nsHeapChunk *chunkToFree);

		virtual nsHeapChunk*	FindChunkWithSpace(size_t blockSize) const;
		
		UInt32					GetAllocatorBlockSize() { return mBlockSize; }
		
	protected:

		enum {
			kMaxBlockResizeSlop		= 16
		}; 
		
		UInt32			mBlockSize;		// upper bound for blocks allocated in this heap
										// does not include block overhead


#if STATS_MAC_MEMORY
		UInt32			mChunksAllocated;
		UInt32			mMaxChunksAllocated;
		
		UInt32			mTotalChunkSize;
		UInt32			mMaxTotalChunkSize;

		UInt32			mBlocksAllocated;
		UInt32			mMaxBlocksAllocated;
		
		UInt32			mBlocksUsed;
		UInt32			mMaxBlocksUsed;
		
		UInt32			mBlockSpaceUsed;
		UInt32			mMaxBlockSpaceUsed;
#endif

};



class nsFixedSizeHeapChunk : public nsHeapChunk
{
	public:
	
										nsFixedSizeHeapChunk(nsMemAllocator *inOwningAllocator, Size heapSize, Handle tempMemHandle);
										~nsFixedSizeHeapChunk();

		FixedMemoryBlock*			GetFreeList() const { return mFreeList; }
		void							SetFreeList(FixedMemoryBlock *nextFree) { mFreeList = nextFree; }
				
		FixedMemoryBlock*			FetchFirstFree();
		void							ReturnToFreeList(FixedMemoryBlock *freeBlock);

	protected:
		
	#if STATS_MAC_MEMORY
		UInt32							chunkSize;
		UInt32							numBlocks;
	#endif
		
		FixedMemoryBlock			*mFreeList;
		FixedMemoryBlock			mMemory[];
};

