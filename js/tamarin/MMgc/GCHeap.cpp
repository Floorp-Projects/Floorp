/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include <string.h>

#if defined(DARWIN) || defined(MMGC_ARM)
#include <stdlib.h>
#endif

#include "MMgc.h"

namespace MMgc
{
#ifdef _DEBUG
	// 64bit - Warning, this debug mechanism will fail on 64-bit systems when we 
	// allocate pages across more than 4 GB of memory space.  We no longer have 20-bits
	// to track - we have 52-bits.  
	uint8 GCHeap::m_megamap[1048576];
#endif

	GCHeap *GCHeap::instance = NULL;

	const bool decommit = true;

	// set this to true to disable code that prevents decommmission from 
	// happening too frequently
	const bool decommitStress = false;

	void GCHeap::Init(GCMallocFuncPtr m, GCFreeFuncPtr f, int initialSize)
	{
		GCAssert(instance == NULL);
		instance = new GCHeap(m,f, initialSize);
	}

	void GCHeap::Destroy()
	{
		GCAssert(instance != NULL);
		delete instance;
		instance = NULL;
	}

	GCHeap::GCHeap(GCMallocFuncPtr m, GCFreeFuncPtr f, int initialSize)
		: heapVerbose(false),
		  kNativePageSize(0)
	{
#ifdef _DEBUG
		// dump memory profile after sweeps
		enableMemoryProfiling = false;
#endif

#if defined(_MAC) || defined(MMGC_ARM)
		m_malloc = m ? m : malloc;
		m_free = f ? f : free;		
#else
		(void)m;
		(void)f;
#endif
		#ifdef _DEBUG
		// Initialize the megamap for debugging.
		memset(m_megamap, 0, sizeof(m_megamap));
		#endif
		
		lastRegion  = 0;
		blocksLen   = 0;
		numAlloc    = 0;
		numDecommitted = 0;
		blocks      = NULL;

		// Initialize free lists
		HeapBlock *block = freelists;
		for (int i=0; i<kNumFreeLists; i++) {
			block->baseAddr     = NULL;
			block->size         = 0;
			block->sizePrevious = 0;
			block->prev         = block;
			block->next         = block;
			block->committed    = true;
			block->dirty 	    = true;
			block++;
		}
		
#ifdef MMGC_AVMPLUS
		// get native page size from OS
		kNativePageSize = vmPageSize();
		committedCodeMemory = 0;

		#ifdef WIN32
		// Check OS version to see if PAGE_GUARD is supported
		// (It is not supported on Win9x)
		OSVERSIONINFO osvi;
		::GetVersionEx(&osvi);
		useGuardPages = (osvi.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS);
		#endif
#endif

		// Create the initial heap
		ExpandHeap(initialSize);

		decommitTicks = 0;
		decommitThresholdTicks = kDecommitThresholdMillis * GC::GetPerformanceFrequency() / 1000;
	}

	GCHeap::~GCHeap()
	{
#ifdef MEMORY_INFO
		if(numAlloc != 0)
		{
			for (unsigned int i=0; i<blocksLen; i++) 
			{
				HeapBlock *block = &blocks[i];
				if(block->baseAddr)
				{
					uint32 megamapIndex = ((uint32)(uintptr)block->baseAddr) >> 12;
					GCAssert(m_megamap[megamapIndex] == 0);
				}
				if(block->inUse() && block->baseAddr)
				{
					GCDebugMsg(false, "Block 0x%x not freed\n", block->baseAddr);
					PrintStackTraceByIndex(block->allocTrace);
				}
			}	
			GCAssert(false);
		}
#endif
		FreeAll();
	}

	void* GCHeap::Alloc(int size, bool expand/*=true*/, bool zero/*true*/)
	{
		GCAssert(size > 0);

		char *baseAddr;

		// nested block to keep memset/memory commit out of critical section
		{
#ifdef GCHEAP_LOCK
			GCAcquireSpinlock spinlock(m_spinlock);
#endif /* GCHEAP_LOCK */

			HeapBlock *block = AllocBlock(size, zero);

			if (!block && expand) {
				ExpandHeapPrivate(size);
				block = AllocBlock(size, zero);
			}

			if (!block) {
				return NULL;
			}

			GCAssert(block->size == size);
			
			numAlloc += size;

#ifdef _DEBUG
			// Check for debug builds only:
			// Use the megamap to double-check that we haven't handed
			// any of these pages out already.
			uint32 megamapIndex = ((uint32)(uintptr)block->baseAddr)>>12;
			for (int i=0; i<size; i++) {
				GCAssert(m_megamap[megamapIndex] == 0);
				
				// Set the megamap entry
				m_megamap[megamapIndex++] = 1;
			}
#endif

			// copy baseAddr to a stack variable to fix :
			// http://flashqa.macromedia.com/bugapp/detail.asp?ID=125938
			baseAddr = block->baseAddr;
		}

#ifdef MEMORY_INFO
		// do this outside of the spinlock to prevent deadlock w/ trace table lock
		AddrToBlock(baseAddr)->allocTrace = GetStackTraceIndex(2);
#endif
		// Zero out the memory, if requested to do so
		// FIXME: if we know that this memory was just committed we shouldn't do this
		// maybe zero should pass deeper into the system so that on initial expansions
		// and re-commit we avoid this, should be a noise optimization over all but
		// possibly a big win for startup
		if (zero) {
			memset(baseAddr, 0, size * kBlockSize);
		}
		
		return baseAddr;
	}

	void GCHeap::Free(void *item)
	{
#ifdef GCHEAP_LOCK
		GCAcquireSpinlock spinlock(m_spinlock);
#endif /* GCHEAP_LOCK */

		HeapBlock *block = AddrToBlock(item);
		if (block) {

			#ifdef _DEBUG
			// For debug builds only:
			// Check the megamap to ensure that all the pages
			// being freed are in fact allocated.
			uint32 megamapIndex = ((uint32)(uintptr)block->baseAddr) >> 12;
			for (int i=0; i<block->size; i++) {
				if(m_megamap[megamapIndex] != 1) {
					GCAssertMsg(false, "Megamap is screwed up, are you freeing freed memory?");
					int *data = (int*)item;
					(void)data;	// silence compiler warning
				}

				// Clear the entry
				m_megamap[megamapIndex++] = 0;
			}
			#endif

			// Update metrics
			GCAssert(numAlloc >= (unsigned int)block->size);
			numAlloc -= block->size;
			
			FreeBlock(block);
		}
	}

	
	size_t GCHeap::Size(const void *item)
	{
#ifdef GCHEAP_LOCK
		GCAcquireSpinlock spinlock(m_spinlock);
#endif /* GCHEAP_LOCK */
		HeapBlock *block = AddrToBlock(item);
		return block->size;
	}

	void GCHeap::Decommit()
	{
#ifdef DECOMMIT_MEMORY

#ifdef GCHEAP_LOCK
		GCAcquireSpinlock spinlock(m_spinlock);
#endif /* GCHEAP_LOCK */

		// commit if > %25 of the heap stays free
		int decommitSize = GetFreeHeapSize() * 100 - GetTotalHeapSize() * kDecommitThresholdPercentage;

		decommitSize /= 100;

		if(!decommitStress)
		{
			if(!decommit || decommitSize < 0) {
				
				decommitTicks = 0;
				return;
			}
	    
			if(decommitTicks == 0) {
				decommitTicks = GC::GetPerformanceCounter();
				return;
			} else if ((GC::GetPerformanceCounter() - decommitTicks) < decommitThresholdTicks) {
				return;
			}
		}

		decommitTicks = 0;

		// don't trifle
		if(decommitSize < kMinHeapIncrement)
			return;

		// search from the end of the free list so we decommit big blocks, if
		// a free block is bigger than 
		HeapBlock *freelist = freelists+kNumFreeLists-1;

		HeapBlock *endOfBigFreelists = &freelists[GetFreeListIndex(kMinHeapIncrement)];

		for (; freelist >= endOfBigFreelists && decommitSize > 0; freelist--)
		{
			HeapBlock *block = freelist;
			while ((block = block->prev) != freelist && decommitSize > 0)
			{
				// decommitting already decommitted blocks doesn't help
				if(!block->committed || block->size == 0)
					continue;

#ifdef USE_MMAP				
				RemoveFromList(block);
				if(block->size > decommitSize)
				{
					HeapBlock *newBlock = block + decommitSize;
					newBlock->baseAddr = block->baseAddr + kBlockSize * decommitSize;

					newBlock->size = block->size - decommitSize;
					newBlock->sizePrevious = decommitSize;
					newBlock->committed = block->committed;
					newBlock->dirty = block->dirty;
					block->size = decommitSize;

					// Update sizePrevious in block after that
					HeapBlock *nextBlock = newBlock + newBlock->size;
					nextBlock->sizePrevious = newBlock->size;

					// Add the newly created block to the
					// free list
					AddToFreeList(newBlock);
				}

				if(DecommitMemoryThatMaySpanRegions(block->baseAddr, block->size * kBlockSize))
				{
					block->committed = false;
					block->dirty = false;
					decommitSize -= block->size;
#ifdef MEMORY_INFO
					if(heapVerbose)
						GCDebugMsg(false, "Decommitted %d page block\n", block->size);
#endif
				}
				else
				{
					GCAssert(false);
				}

				numDecommitted += block->size;

				// merge with previous/next if not in use and not committed
				HeapBlock *prev = block - block->sizePrevious;
				if(block->sizePrevious != 0 && !prev->committed && !prev->inUse()) {
					RemoveFromList(prev);

					prev->size += block->size;

					block->size = 0;
					block->sizePrevious = 0;
					block->baseAddr = 0;

					block = prev;
				}

				HeapBlock *next = block + block->size;
				if(next->size != 0 && !next->committed && !next->inUse()) {
					RemoveFromList(next);

					block->size += next->size;
				
					next->size = 0;
					next->sizePrevious = 0;
					next->baseAddr = 0;
				}

				next = block + block->size;
				next->sizePrevious = block->size;

				// add this block to the back of the bus to make sure we consume committed memory
				// first
				HeapBlock *backOfTheBus = &freelists[kNumFreeLists-1];
				HeapBlock *pointToInsert = backOfTheBus;
				while ((pointToInsert = pointToInsert->next) !=  backOfTheBus) {
					if (pointToInsert->size >= block->size && !pointToInsert->committed) {
						break;
					}
				}
				AddToFreeList(block, pointToInsert);

				// so we keep going through freelist properly
				block = freelist;
#else
				// if we aren't using mmap we can only do something if the block maps to a region
				// that is completely empty
				Region *region = AddrToRegion(block->baseAddr);
				if(block->baseAddr == region->baseAddr && // beginnings match
					region->commitTop == block->baseAddr + block->size*kBlockSize) {

					RemoveFromList(block);

					// we're removing a region so re-allocate the blocks w/o the blocks for this region
					HeapBlock *newBlocks = new HeapBlock[blocksLen - block->size];

					// copy blocks before this block
					memcpy(newBlocks, blocks, (block - blocks) * sizeof(HeapBlock));

					// copy blocks after
					size_t lastChunkSize = (char*)(blocks + blocksLen) - (char*)(block + block->size);
					memcpy(newBlocks + (block - blocks), block + block->size, lastChunkSize);

					// Fix up the prev/next pointers of each freelist.  This is a little more complicated
					// than the similiar code in ExpandHeap because blocks after the one we are free'ing
					// are sliding down by block->size
					HeapBlock *fl = freelists;
					for (int i=0; i<kNumFreeLists; i++) {
						HeapBlock *temp = fl;
						do {
							if (temp->prev != fl) {
								if(temp->prev > block) {
									temp->prev = newBlocks + (temp->prev-blocks-block->size);
								} else {
									temp->prev = newBlocks + (temp->prev-blocks);
								}
							}
							if (temp->next != fl) {
								if(temp->next > block) {
									temp->next = newBlocks + (temp->next-blocks-block->size);
								} else {
									temp->next = newBlocks + (temp->next-blocks);
								}
							}
						} while ((temp = temp->next) != fl);
						fl++;
					}

					// need to decrement blockId for regions in blocks after block
					Region *r = lastRegion;
					while(r) {
						if(r->blockId > region->blockId) {
							r->blockId -= block->size;
						}
						r = r->prev;
					}

					blocksLen -= block->size;
					decommitSize -= block->size;
					
					delete [] blocks;
					blocks = newBlocks;
					RemoveRegion(region);

					// start over at beginning of freelist
					block = freelist;
				}
#endif
			}
		}		
#endif
	}

	GCHeap::Region *GCHeap::AddrToRegion(const void *item) const
	{
		// Linear search of regions list to find this address.
		// The regions list should usually be pretty short.
		for (Region *region = lastRegion;
			 region != NULL;
			 region = region->prev)
		{
			if (item >= region->baseAddr && item < region->reserveTop) {
				return region;
			}
		}
		return NULL;
	}

	GCHeap::HeapBlock* GCHeap::AddrToBlock(const void *item) const
	{
		Region *region = AddrToRegion(item);
		if(region) {
			int index = ((char*)item - region->baseAddr) / kBlockSize;
			HeapBlock *b = blocks + region->blockId + index;
			GCAssert(item >= b->baseAddr && item < b->baseAddr + b->size * GCHeap::kBlockSize);
			return b;
		}
		return NULL;
	}
	
	GCHeap::HeapBlock* GCHeap::AllocBlock(int size, bool& zero)
	{
		int startList = GetFreeListIndex(size);
		HeapBlock *freelist = &freelists[startList];

		HeapBlock *decommittedSuitableBlock = NULL;

		for (int i = startList; i < kNumFreeLists; i++)
		{
			// Search for a big enough block in free list
			HeapBlock *block = freelist;
			while ((block = block->next) != freelist)
			{
				if (block->size >= size && block->committed) 
				{
					// why do we do this?  putting in assert but I think it should be removed
					GCAssert(!block->inUse());	
					if( !block->prev || !block->next ) return NULL;


					RemoveFromList(block);

					if(block->size > size)
					{
						HeapBlock *newBlock = Split(block, size);

						// Add the newly created block to the free list
						AddToFreeList(newBlock);
					}
				
					CheckFreelist();

					zero = block->dirty && zero;

					return block;
				}

#if defined(USE_MMAP) && defined(DECOMMIT_MEMORY)
				// if the block isn't committed see if this request can be met with by committing
				// it and combining it with its neighbors
				if(!block->committed && !decommittedSuitableBlock)
				{
					int totalSize = block->size;

					// first try predecessors
					HeapBlock *firstFree = block;

					// loop because we could have interleaved committed/non-committed blocks
					while(totalSize < size && firstFree->sizePrevious != 0)
					{	
						HeapBlock *prevBlock = firstFree - firstFree->sizePrevious;
						if(!prevBlock->inUse() && prevBlock->size > 0) {
							totalSize += prevBlock->size;
							firstFree = prevBlock;
						} else {
							break;
						}
					}

					if(totalSize > size) {
						decommittedSuitableBlock = firstFree;
					} else {
						// now try successors
						HeapBlock *nextBlock = block + block->size;
						while(nextBlock->size > 0 && !nextBlock->inUse() && totalSize < size) {
							totalSize += nextBlock->size;
							nextBlock = nextBlock + nextBlock->size;
						}

						if(totalSize > size) {
							decommittedSuitableBlock = firstFree;
						}
					}
				}
#endif
			}
			freelist++;
		}	

#if defined(USE_MMAP) && defined(DECOMMIT_MEMORY)
		if(decommittedSuitableBlock)
		{
			// first handle case where its too big
			if(decommittedSuitableBlock->size > size)
			{				
				int toCommit = size > kMinHeapIncrement ? size : kMinHeapIncrement;

				if(toCommit > decommittedSuitableBlock->size)
					toCommit = decommittedSuitableBlock->size;

				RemoveFromList(decommittedSuitableBlock);
				
				// first split off part we're gonna commit
				if(decommittedSuitableBlock->size > toCommit) {
					HeapBlock *newBlock = Split(decommittedSuitableBlock, toCommit);

					// put the still uncommitted part back on freelist
					AddToFreeList(newBlock);
				}
				
				Commit(decommittedSuitableBlock);

				if(toCommit > size) {
					HeapBlock *newBlock = Split(decommittedSuitableBlock, size);
					AddToFreeList(newBlock);
				}
			}
			else // too small
			{
				// need to stitch blocks together committing uncommitted blocks
				HeapBlock *block = decommittedSuitableBlock;
				RemoveFromList(block);

				int amountRecommitted = block->committed ? 0 : block->size;
					
				while(block->size < size)
				{
					HeapBlock *nextBlock = block + block->size;

					RemoveFromList(nextBlock);
						
					// Increase size of current block
					block->size += nextBlock->size;
					amountRecommitted += nextBlock->committed ? 0 : nextBlock->size;

					nextBlock->size = 0;
					nextBlock->baseAddr = 0;
					nextBlock->sizePrevious = 0;

					block->dirty |= nextBlock->dirty;
				}

				GCAssert(amountRecommitted > 0);

				if(!CommitMemoryThatMaySpanRegions(block->baseAddr, block->size * kBlockSize)) 
				{
					GCAssert(false);
				}
#ifdef MEMORY_INFO
				if(heapVerbose)
					GCDebugMsg(false, "Recommitted %d pages\n", amountRecommitted);
#endif
				numDecommitted -= amountRecommitted;
				block->committed = true;

				GCAssert(decommittedSuitableBlock->size >= size);

				// split last block
				if(block->size > size)
				{
					HeapBlock *newBlock = Split(block, size);
					AddToFreeList(newBlock);
				}
			}

			GCAssert(decommittedSuitableBlock->size == size);

			// update sizePrevious in next block
			HeapBlock *nextBlock = decommittedSuitableBlock + size;
			nextBlock->sizePrevious = size;

			CheckFreelist();

			return decommittedSuitableBlock;
		}
#endif
	
		CheckFreelist();
		return 0;
	}

	GCHeap::HeapBlock *GCHeap::Split(HeapBlock *block, int size)
	{
		GCAssert(block->size > size);
		HeapBlock *newBlock = block + size;
		newBlock->baseAddr = block->baseAddr + kBlockSize * size;

		newBlock->size = block->size - size;
		newBlock->sizePrevious = size;
		newBlock->committed = block->committed;
		newBlock->dirty = block->dirty;
		block->size = size;

		// Update sizePrevious in block after that
		HeapBlock *nextBlock = newBlock + newBlock->size;
		nextBlock->sizePrevious = newBlock->size;
	
		return newBlock;
	}

	
#if defined(DECOMMIT_MEMORY) && defined(USE_MMAP)
	void GCHeap::Commit(HeapBlock *block)
	{
		if(!block->committed)
		{
			if(!CommitMemoryThatMaySpanRegions(block->baseAddr, block->size * kBlockSize)) 
			{
				GCAssert(false);
			}
#ifdef MEMORY_INFO
			if(heapVerbose)
				GCDebugMsg(false, "Recommitted %d pages\n", block->size);
#endif
			numDecommitted -= block->size;
			block->committed = true;
			block->dirty = false;
		}
	}
#endif
		

	void GCHeap::CheckFreelist()
	{
#ifdef _DEBUG
		HeapBlock *freelist = freelists;
		for (int i = 0; i < kNumFreeLists; i++)
		{
			HeapBlock *block = freelist;
			while((block = block->next) != freelist)
			{
				GCAssert(block != block->next);
				GCAssert(block != block->next->next || block->next == freelist);
				if(block->sizePrevious)
				{
					HeapBlock *prev = block - block->sizePrevious;
					GCAssert(block->sizePrevious == prev->size);
				}
			}
			freelist++;
		}
#endif
	}

	bool GCHeap::BlocksAreContiguous(void *item1, void *item2)
	{
		Region *r1 = AddrToRegion(item1);
		Region *r2 = AddrToRegion(item2);
		return r1 == r2 || r1->reserveTop == r2->baseAddr;
	}

	void GCHeap::AddToFreeList(HeapBlock *block)
	{
		GCAssert(m_megamap[((uint32)(uintptr)block->baseAddr)>>12] == 0);

		int index = GetFreeListIndex(block->size);
		HeapBlock *freelist = &freelists[index];

		HeapBlock *pointToInsert = freelist;
		
		// Note: We don't need to bother searching for the right
		// insertion point if we know all blocks on this free list
		// are the same size.
		if (block->size >= kUniqueThreshold) {
			while ((pointToInsert = pointToInsert->next) != freelist) {
				if (pointToInsert->size >= block->size) {
					break;
				}
			}
		}

		AddToFreeList(block, pointToInsert);
	}
		
	void GCHeap::AddToFreeList(HeapBlock *block, HeapBlock* pointToInsert)
	{
		CheckFreelist();

		block->next = pointToInsert;
		block->prev = pointToInsert->prev;
		block->prev->next = block;
		pointToInsert->prev = block;

		CheckFreelist();
	}						   

	void GCHeap::FreeBlock(HeapBlock *block)
	{
		GCAssert(block->inUse());

		// Try to coalesce this block with its predecessor
		HeapBlock *prevBlock = block - block->sizePrevious;
		if (!prevBlock->inUse() && prevBlock->committed) {
			GCAssert(m_megamap[((uint32)(uintptr)prevBlock->baseAddr)>>12] == 0);
			// Remove predecessor block from free list
			RemoveFromList(prevBlock);

			// Increase size of predecessor block
			prevBlock->size += block->size;

			block->size = 0;
			block->sizePrevious = 0;
			block->baseAddr = 0;				

			block = prevBlock;
		}

		// Try to coalesce this block with its successor
		HeapBlock *nextBlock = block + block->size;

		if (!nextBlock->inUse() && nextBlock->committed) {
			// Remove successor block from free list
			RemoveFromList(nextBlock);

			// Increase size of current block
			block->size += nextBlock->size;
			nextBlock->size = 0;
			nextBlock->baseAddr = 0;
			nextBlock->sizePrevious = 0;
		}

		// Update sizePrevious in the next block
		nextBlock = block + block->size;
		nextBlock->sizePrevious = block->size;

		// Add this block to the right free list
		block->dirty = true;

		AddToFreeList(block);

		CheckFreelist();
	}

	bool GCHeap::ExpandHeap(int askSize)
	{
#ifdef GCHEAP_LOCK
		// Acquire the spinlock, as this is a publicly
		// accessible API.
		GCAcquireSpinlock spinlock(m_spinlock);
#endif /* GCHEAP_LOCK */
		return ExpandHeapPrivate(askSize);
	}
	 
	bool GCHeap::ExpandHeapPrivate(int askSize)
	{
		int size = askSize;
#ifdef _DEBUG
		// Turn this switch on to force non-contiguous heaps.
		bool debug_noncontiguous = false;

		// Turn this switch on to test bridging of contiguous
		// regions.
		bool test_bridging = false;
		int defaultReserve = test_bridging ? (size+kMinHeapIncrement) : kDefaultReserve;
#else
		const int defaultReserve = kDefaultReserve;
#endif
		
		char *baseAddr = NULL;
		char *newRegionAddr = NULL;
		int newRegionSize = 0;
		bool contiguous = false;
		int commitAvail = 0;
		
		// Allocate at least kMinHeapIncrement blocks
		if (size < kMinHeapIncrement) {
			size = kMinHeapIncrement;
		}

		// Round to the nearest kMinHeapIncrement
		size = ((size + kMinHeapIncrement - 1) / kMinHeapIncrement) * kMinHeapIncrement;

		#ifdef USE_MMAP
#ifdef _DEBUG
		if (lastRegion != NULL && !debug_noncontiguous)
#else
		if (lastRegion != NULL)
#endif
		{
			commitAvail = (lastRegion->reserveTop - lastRegion->commitTop) / kBlockSize;
			
			// Can this request be satisfied purely by committing more memory that
			// is already reserved?
			if (size <= commitAvail) {
				if (CommitMemory(lastRegion->commitTop, size * kBlockSize))
				{
					// Succeeded!
					baseAddr = lastRegion->commitTop;
					contiguous = true;

					// Update the commit top.
					lastRegion->commitTop += size*kBlockSize;

					// Go set up the block list.
					goto gotMemory;
				}
				else
				{
					// If we can't commit memory we've already reserved,
					// no other trick is going to work.  Fail.
					return false;
				}
			}

			// Try to reserve a region contiguous to the last region.

			// - Try for the "default reservation size" if it's larger than
			//   the requested block.
			if (defaultReserve > size) {
				newRegionAddr = ReserveMemory(lastRegion->reserveTop,
											  defaultReserve * kBlockSize);
				newRegionSize = defaultReserve;
			}

			// - If the default reservation size didn't work or isn't big
			//   enough, go for the exact amount requested, minus the
			//   committable space in the current region.
			if (newRegionAddr == NULL) {
				newRegionAddr = ReserveMemory(lastRegion->reserveTop,
											  (size - commitAvail)*kBlockSize);
				newRegionSize = size - commitAvail;
			}

			if (newRegionAddr != NULL) {
				// We were able to reserve some space.

				// Commit available space from the existing region.
				if (commitAvail != 0) {
					if (!CommitMemory(lastRegion->commitTop, commitAvail * kBlockSize))
					{
						// We couldn't commit even this space.  We're doomed.
						// Un-reserve the space we just reserved and fail.
						ReleaseMemory(newRegionAddr, newRegionSize);
						return false;
					}
				}

				// Commit needed space from the new region.
				if (!CommitMemory(newRegionAddr, (size - commitAvail) * kBlockSize))
				{
					// We couldn't commit this space.  We can't meet the
					// request.  Un-commit any memory we just committed,
					// un-reserve any memory we just reserved, and fail.
					if (commitAvail != 0) {
						DecommitMemory(lastRegion->commitTop,
									   commitAvail * kBlockSize);
					}
					ReleaseMemory(newRegionAddr,
								  (size-commitAvail)*kBlockSize);
					return false;
				}

				// We successfully reserved a new contiguous region
				// and committed the memory we need.  Finish up.
				baseAddr = lastRegion->commitTop;
				lastRegion->commitTop = lastRegion->reserveTop;
				contiguous = true;
				
				goto gotMemory;
			}
		}

		// We were unable to allocate a contiguous region, or there
		// was no existing region to be contiguous to because this
		// is the first-ever expansion.  Allocate a non-contiguous region.

		// Don't use any of the available space in the current region.
		commitAvail = 0;

		// - Go for the default reservation size unless the requested
		//   size is bigger.
		if (size < defaultReserve) {
			newRegionAddr = ReserveMemory(NULL,
										  defaultReserve*kBlockSize);
			newRegionSize = defaultReserve;
		}

		// - If that failed or the requested size is bigger than default,
		//   go for the requested size exactly.
		if (newRegionAddr == NULL) {
			newRegionAddr = ReserveMemory(NULL,
										  size*kBlockSize);
			newRegionSize = size;
		}

		// - If that didn't work, give up.
		if (newRegionAddr == NULL) {
			return false;
		}

		// - Try to commit the memory.
		if (CommitMemory(newRegionAddr,
						 size*kBlockSize) == 0)
		{
			// Failed.  Un-reserve the memory and fail.
			ReleaseMemory(newRegionAddr, newRegionSize*kBlockSize);
			return false;
		}

		// If we got here, we've successfully allocated a
		// non-contiguous region.
		baseAddr = newRegionAddr;
		contiguous = false;

	  gotMemory:

#else		
		// Allocate the requested amount of space as a new region.
		newRegionAddr = AllocateMemory(size * kBlockSize);
		baseAddr = newRegionAddr;
		newRegionSize = size;

		// If that didn't work, give up.
		if (newRegionAddr == NULL) {
			return false;
		}
#endif

		// If we were able to allocate a contiguous block, remove
		// the old top sentinel.
		if (contiguous) {
			blocksLen--;
		}

		// Expand the block list.
		int newBlocksLen = blocksLen + size;

		// Add space for the "top" sentinel
		newBlocksLen++;

		HeapBlock *newBlocks = new HeapBlock[newBlocksLen];
		if (!newBlocks) {
			// Could not get the memory.
			#ifdef USE_MMAP
			ReleaseMemory(newRegionAddr, newRegionSize);
			#else
			ReleaseMemory(newRegionAddr);
			#endif
			return false;
		}
		
		// Copy all the existing blocks.
		if (blocksLen) {
			memcpy(newBlocks, blocks, blocksLen * sizeof(HeapBlock));

			// Fix up the prev/next pointers of each freelist.
			HeapBlock *freelist = freelists;
			for (int i=0; i<kNumFreeLists; i++) {
				HeapBlock *temp = freelist;
				do {
					if (temp->prev != freelist) {
						temp->prev = newBlocks + (temp->prev-blocks);
					}
					if (temp->next != freelist) {
						temp->next = newBlocks + (temp->next-blocks);
					}
				} while ((temp = temp->next) != freelist);
				freelist++;
			}
			CheckFreelist();
		}

		// Create a single free block for the new space,
		// and add it to the free list.
		HeapBlock *block = newBlocks+blocksLen;
		block->baseAddr = baseAddr;

		block->size = size;
		block->sizePrevious = 0;
		// link up contiguous blocks
		if(blocksLen && contiguous)
		{
			// search backwards for first real block
			HeapBlock *b = &blocks[blocksLen-1];
			while(b->size == 0) 
			{
				b--;
				GCAssert(b >= blocks);
			}
			block->sizePrevious = b->size;
		}
		block->prev = NULL;
		block->next = NULL;
		block->committed = true;
#ifdef USE_MMAP
		block->dirty = false; // correct?
#else
		block->dirty = true;
#endif

#ifdef MEMORY_INFO
		block->allocTrace = 0;
#endif

		AddToFreeList(block);

		// Initialize the rest of the new blocks to empty.
		for (int i=1; i<size; i++) {
			block++;
			block->baseAddr = NULL;
			block->size = 0;
			block->sizePrevious = 0;
			block->prev = NULL;
			block->next = NULL;
			block->committed = false;
			block->dirty = false;
#ifdef MEMORY_INFO
			block->allocTrace = 0;
#endif
		}

		// Fill in the sentinel for the top of the heap.
		block++;
		block->baseAddr     = NULL;
		block->size         = 0;
		block->sizePrevious = size;
		block->prev         = NULL;
		block->next         = NULL;
#ifdef MEMORY_INFO
		block->allocTrace = 0;
#endif

		// Replace the blocks list
		if (blocks) {
			delete [] blocks;
		}
		blocks = newBlocks;
		blocksLen = newBlocksLen;

		// If we created a new region, save the base address so we can free later.
		if (newRegionAddr) {
			Region *newRegion = new Region;
			if (newRegion == NULL) {
				// Ugh, FUBAR.
				return false;
			}
			newRegion->baseAddr   = newRegionAddr;
			newRegion->reserveTop = newRegionAddr+newRegionSize*kBlockSize;
			newRegion->commitTop  = newRegionAddr+(size-commitAvail)*kBlockSize;
			newRegion->blockId    = newBlocksLen-(size-commitAvail)-1;
			newRegion->prev = lastRegion;
			lastRegion = newRegion;

			#ifdef TRACE
			printf("ExpandHeap: new region, %d reserve, %d commit, %s\n",
				   newRegion->reserveTop-newRegion->baseAddr,
				   newRegion->commitTop-newRegion->baseAddr,
				   contiguous ? "contiguous" : "non-contiguous");
			#endif
		}

		CheckFreelist();
		
#ifdef MEMORY_INFO
		if(heapVerbose)
			GCDebugMsg(false, "Heap expanded by %d pages:\n", size);
#endif
			
		// Success!
		return true;
	}

	void GCHeap::RemoveRegion(Region *region)
	{
		Region **next = &lastRegion;
		while(*next != region) 
			next = &((*next)->prev);
		*next = region->prev;
#ifdef USE_MMAP
		ReleaseMemory(region->baseAddr,
				region->reserveTop-region->baseAddr);
#else
		ReleaseMemory(region->baseAddr);
#endif
		delete region;
	}

	void GCHeap::FreeAll()
	{
		// Release all of the heap regions
		while (lastRegion != NULL) {
			Region *region = lastRegion;
			lastRegion = lastRegion->prev;
			#ifdef USE_MMAP
			ReleaseMemory(region->baseAddr,
						  region->reserveTop-region->baseAddr);
			#else
			ReleaseMemory(region->baseAddr);
			#endif
			delete region;
		}
		delete [] blocks;

	}

}
