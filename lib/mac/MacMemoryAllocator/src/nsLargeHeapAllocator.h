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
class nsLargeHeapChunk;

//--------------------------------------------------------------------
struct LargeBlockHeader
{
	
	static LargeBlockHeader *GetBlockHeader(void *block)	{ return (LargeBlockHeader *)((char *)block - sizeof(LargeBlockHeader));	}
	
	Boolean					IsFreeBlock() 	{ return prev == nil; }
	UInt32					GetBlockSize()	{ return ((UInt32)next - (UInt32)this - kLargeBlockOverhead);	}


	UInt32					GetBlockHeapUsageSize()	{ return ((UInt32)next - (UInt32)this);	}
	
	void					SetLogicalSize(UInt32 inSize) { logicalSize = inSize; }
	UInt32					GetLogicalSize() { return logicalSize; }
	
	LargeBlockHeader*		SkipDummyBlock() { return (LargeBlockHeader *)((UInt32)this + kLargeBlockOverhead); }
	
	
	LargeBlockHeader*		GetNextBlock()	{ return next; }
	LargeBlockHeader*		GetPrevBlock()	{ return prev; }

	void					SetNextBlock(LargeBlockHeader *inNext)	{ next = inNext; }
	void					SetPrevBlock(LargeBlockHeader *inPrev)	{ prev = inPrev; }
	
	void					SetOwningChunk(nsHeapChunk *chunk)		{ header.owningChunk = chunk; }
	nsLargeHeapChunk*		GetOwningChunk() { return (nsLargeHeapChunk *)(header.owningChunk); }
	
#if DEBUG_HEAP_INTEGRITY
	enum {
		kBlockPaddingBytes	= 'ננננ'
	};
	
	void					SetPaddingBytes(UInt32 padding)	{ paddingBytes = padding; }
	void					FillPaddingBytes()				{
																	long	*lastLong 	= (long *)((char *)&memory + GetBlockSize() - sizeof(long));
																	UInt32	mask 		= (1 << (8 * paddingBytes)) - 1;
																	
																	*lastLong &= ~mask;
																	*lastLong |= (mask & kBlockPaddingBytes);
															}
	Boolean					CheckPaddingBytes()				{
																	long	*lastLong 	= (long *)((char *)&memory + GetBlockSize() - sizeof(long));
																	UInt32	mask 		= (1 << (8 * paddingBytes)) - 1;
																	return (*lastLong & mask) == (mask & kBlockPaddingBytes);
															}
	UInt32					GetPaddingBytes()				{ return paddingBytes; }

	// inline, so won't crash if this is a bad block
	Boolean					HasHeaderTag(MemoryBlockTag inHeaderTag)
											{ return header.headerTag == inHeaderTag; }
	void					SetHeaderTag(MemoryBlockTag inHeaderTag)
											{ header.headerTag = inHeaderTag; }
	void					SetTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
											{
												MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
												trailer->trailerTag = theTag;
											}
	Boolean					HasTrailerTag(UInt32 blockSize, MemoryBlockTag theTag)
											{
												MemoryBlockTrailer *trailer = (MemoryBlockTrailer *)((char *)&memory + blockSize);
												return (trailer->trailerTag == theTag);
											}
#endif

	static const UInt32		kLargeBlockOverhead;
	
	LargeBlockHeader		*next;
	LargeBlockHeader 		*prev;

#if DEBUG_HEAP_INTEGRITY
	UInt32					paddingBytes;
#endif

	UInt32					logicalSize;
	MemoryBlockHeader		header;		// this must be the last variable before memory

	char					memory[];
};


//--------------------------------------------------------------------
class nsLargeHeapAllocator : public nsMemAllocator
{
	private:
	
		typedef nsMemAllocator		Inherited;
	
	
	public:
								nsLargeHeapAllocator();
								~nsLargeHeapAllocator();


		virtual void *			AllocatorMakeBlock(size_t blockSize);
		virtual void 			AllocatorFreeBlock(void *freeBlock);
		virtual void *			AllocatorResizeBlock(void *block, size_t newSize);
		virtual size_t 			AllocatorGetBlockSize(void *thisBlock);

		virtual nsHeapChunk*	AllocateChunk(size_t requestedBlockSize);
		virtual void			FreeChunk(nsHeapChunk *chunkToFree);

	protected:

		UInt32					mBaseChunkPercentage;
		UInt32					mBestTempChunkSize;
		UInt32					mSmallestTempChunkSize;

};



//--------------------------------------------------------------------
class nsLargeHeapChunk : public nsHeapChunk
{
	public:
	
							nsLargeHeapChunk(	nsMemAllocator *inOwningAllocator,
												Size 			heapSize);
							~nsLargeHeapChunk();

		LargeBlockHeader*	GetHeadBlock()		{ return mHead;	}
																							
		LargeBlockHeader*	GetSpaceForBlock(UInt32 roundedBlockSize);

		void *				GrowBlock(LargeBlockHeader *growBlock, size_t newSize);
		void *				ShrinkBlock(LargeBlockHeader *shrinkBlock, size_t newSize);
		void *				ResizeBlockInPlace(LargeBlockHeader *theBlock, size_t newSize);
		
		void				ReturnBlock(LargeBlockHeader *deadBlock);
		
	protected:
	
		UInt32				mTotalFree;
		
		LargeBlockHeader*	mTail;
		LargeBlockHeader	mHead[];
};



