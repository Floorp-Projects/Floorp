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

#ifndef __GCHeap__
#define __GCHeap__


namespace MMgc
{
	/**
	 * GCHeap is a heap manager for the Flash Player's garbage collector.
	 *
	 * Memory is allocated from the operating system in large chunks
	 * aligned on page boundaries (4096-byte aligned).  All allocation
	 * requests are specified in numbers of 4096-byte pages.  The first
	 * 4 bytes of each page is reserved for GCHeap's use.
	 *
	 * Allocations may fail if there is no free block large enough
	 * to satisfy the request.  When this occurs, the GC class will
	 * choose to either garbage collect or expand the heap, and then
	 * call GCHeap again to retry the allocation.
	 *
	 * When blocks are freed, they are coalesced with their neighbors
	 * when possible to reduce fragmentation.  A "boundary tag" scheme
	 * similar to Doug Lea's malloc is used to make coalescing fast.
	 * To keep things simple, coalescing is always performed immediately
	 * when a block is freed, and not deferred.
	 *
	 * Allocations are performed using multiple free lists.  Blocks that
	 * are <= kUniqueThreshold pages in size get their own free list.
	 * Blocks above the threshold get mapped to more granular free lists,
	 * and all blocks above kHugeThreshold get mapped to the last free list.
	 * This approach is borrowed from Hans Boehm's C/C++ Garbage Collector.
	 * 
	 * Free lists are doubly-linked lists sorted in size order.  The
	 * sorting is done to ensure best-fit allocation.  Each free list
	 * is itself a list node, and the lists are circular, in a technique
	 * also borrowed from Doug Lea's malloc.  This makes for very simple
	 * logic to insert/remove nodes.
	 *
	 * Only Windows is currently supported, although this should be easy
	 * to get working on other platforms.  Memory is allocated in large
	 * (64K minimum) chunks using the VirtualAlloc API.  Every time the
	 * heap is expanded, a new heap region is created corresponding to
	 * the address space returned by VirtualAlloc.  Heap regions are
	 * allocated contiguously if possible to reduce fragmentation.
	 */
	class GCHeap : public GCAllocObject
	{
	public:
		// -- Constants
		
		/** Size of a block */
		const static int kBlockSize = 4096;

		/** Default size of address space reserved per region */
		const static int kDefaultReserve = 4096;
		
		/** Sizes up to this many blocks each have their own free list. */
		const static int kUniqueThreshold = 16;

		/**
		 * Sizes of at least this many heap blocks are mapped to a
		 * single free list.
		 */
		const static int kHugeThreshold = 128;

		/** In between sizes map this many distinct sizes to a single bin. */
		const static int kFreeListCompression = 8;

		/** Calculated number of free lists */
		const static int kNumFreeLists = (kHugeThreshold-kUniqueThreshold)/kFreeListCompression+kUniqueThreshold;

		/** Minimum heap increment, in blocks */
		const static int kMinHeapIncrement = 32;

		/** if this much of the heap stays free for kDecommitThresholdMillis decommit some memory */
		const static int kDecommitThresholdPercentage = 25;

		
		const static int kDecommitThresholdMillis = 1000;

		/** The native VM page size (in bytes) for the current architecture */
		int kNativePageSize;

		bool heapVerbose;

#ifdef _DEBUG
		/**
		 * turn on memory profiling
		 */
		bool enableMemoryProfiling;
#endif

		/**
		 * Init must be called to set up the GCHeap singleton
		 */
		static void Init(GCMallocFuncPtr malloc = NULL, GCFreeFuncPtr free = NULL, int initialSize=128);

		/**
		 * Destroy the GCHeap singleton
		 */
		static void Destroy();

		/**
		 * Get the GCHeap singleton
		 */
		static GCHeap *GetGCHeap() { GCAssert(instance != NULL); return instance; }

		/**
		 * Allocates a block from the heap.
		 * @param size the number of pages (kBlockSize bytes apiece)
		 *             to allocate.
		 * @return pointer to beginning of block, or NULL if failed.
		 */
		void *Alloc(int size, bool expand=true, bool zero=true);

		/**
		 * Frees a block.
		 * @param item the block to free.  This must be the same
		 *             pointer that was previously returned by
		 *             a call to Alloc.
		 */
		void Free(void *item);

		size_t Size(const void *item);

		/**
		 * Expands the heap by size pages.
		 *
		 * Expands the heap by "size" blocks, such that a single contiguous
		 * allocation of "size" blocks can be performed.  This method is
		 * also called to create the initial heap.
		 *
		 * On Windows, this uses the VirtualAlloc API to obtain memory.
		 * VirtualAlloc can _reserve_ memory, _commit_ memory or both at
		 * the same time.  Reserved memory is just virtual address space.
		 * It consumes the address space of the process but isn't really
		 * allocated yet; there are no pages committed to it yet.
		 * Memory allocation really occurs when reserved pages are
		 * committed.  Our strategy in GCHeap is to reserve a fairly large
		 * chunk of address space, and then commit pages from it as needed.
		 * By doing this, we're more likely to get contiguous regions in
		 * memory for our heap.
		 *
		 * By default, we reserve 16MB (4096 pages) per heap region.
		 * The amount to reserve by default is controlled by kDefaultReserve.
		 * That shouldn't be a big deal, as the process address space is 2GB.
		 * As we're usually a plug-in, however, we don't want to make it too
		 * big because it's not all our memory.
		 *
		 * The goal of reserving so much address space is so that subsequent
		 * expansions of the heap are able to obtain contiguous memory blocks.
		 * If we can keep the heap contiguous, that reduces fragmentation
		 * and the possibility of many small "Balkanized" heap regions.
		 *
		 * Algorithm: When an allocation is requested,
		 * 1. If there is enough reserved but uncommitted memory in the
		 *    last-created region to satisfy the request, commit that memory
		 *    and exit with success, also check decommitted list
		 * 2. Try to reserve a new region contiguous with the last-created
		 *    region.  Go for a 16MB reservation or the requested size,
		 *    whichever is bigger.
		 * 3. If we tried for 16MB reserved space and it didn't work, try
		 *    to reserve again, but for the requested size.
		 * 4. If we were able to retrieve a contiguous region in Step 2 or 3,
		 *    commit any leftover memory from the last-created region,
		 *    commit the remainer from the newly created region, and exit
		 *    with success.
		 * 5. OK, the contiguous region didn't work out, so allocate a
		 *    non-contiguous region.  Go for 16MB or the requested size
		 *    again, whichever is bigger.
		 * 6. If we tried for 16MB reserved space and it didn't work, try
		 *    to reserve again, but for the requested size.
		 * 7. Commit the requested size out of the newly created region
		 *    and exit with success.
		 *
		 * If we are able to reserve memory but can't commit it, then, well
		 * there isn't enough memory.  We free the reserved memory and
		 * exit with failure.
		 *
		 * @param size the number of pages to expand the heap by
		 */	 
		bool ExpandHeap(int size);


		/**
		 * Returns the used heap size, that is, the total
		 * space actually used by allocated objects.
		 * @return the minimum heap size in pages (kBlockSize bytes apiece)
		 */
		unsigned int GetUsedHeapSize() const { return numAlloc; }

		/**
		 * Returns the "free heap size", that is, the difference in the
		 * total heap size and the used heap size
		 * @return the minimum heap size in pages (kBlockSize bytes apiece)
		 */
		unsigned int GetFreeHeapSize() const { return GetTotalHeapSize()-numAlloc; }

		/**
		 * Returns the total heap size, that is, the total amount
		 * of space managed by the heap, including all used and
		 * free space.
		 * @return the total heap size in pages (kBlockSize bytes apiece)
		 */
		unsigned int GetTotalHeapSize() const { return blocksLen - numDecommitted; }
		
		/**
		 * gives memory back to the OS when there hasn't been any memory activity in a while
		 * and we have lots of free memory
		 */
		void Decommit();
	private:

		// -- Implementation
		static GCHeap *instance;
		GCHeap(GCMallocFuncPtr m, GCFreeFuncPtr f, int initialSize);
		~GCHeap();
		
		// Heap regions
		class Region : public GCAllocObject
		{
		public:
			Region *prev;
			char *baseAddr;
			char *reserveTop;
			char *commitTop;
			int blockId;
		};
		Region *lastRegion;
		
		// Block struct used for free lists and memory traversal
		class HeapBlock : public GCAllocObject
		{
		public:
			char *baseAddr;   // base address of block's memory
			int size;         // size of this block
			int sizePrevious; // size of previous block
			HeapBlock *prev;      // prev entry on free list
			HeapBlock *next;      // next entry on free list
			bool committed;   // is block fully committed?
			bool dirty;		  // needs zero'ing, only valid if committed
#ifdef MEMORY_INFO
			int allocTrace;
#endif
			bool inUse() { return prev == NULL; }
		};

		bool ExpandHeapPrivate(int size);

		// Core data structures
		HeapBlock *blocks;
		unsigned int blocksLen;
		unsigned int numDecommitted;
		HeapBlock freelists[kNumFreeLists];
		unsigned int numAlloc;
		
		// Core methods
		void AddToFreeList(HeapBlock *block);
		void AddToFreeList(HeapBlock *block, HeapBlock* pointToInsert);
		HeapBlock *AllocBlock(int size, bool& zero);
		void FreeBlock(HeapBlock *block);
		void FreeAll();
	
		HeapBlock *Split(HeapBlock *block, int size);

#ifdef DECOMMIT_MEMORY
		void Commit(HeapBlock *block);
#endif

#ifdef _DEBUG
		friend class GC;
#endif
		HeapBlock *AddrToBlock(const void *item) const;
		Region *AddrToRegion(const void *item) const;
		void RemoveRegion(Region *r);

		// only used on mac
		GCMallocFuncPtr m_malloc;
		GCFreeFuncPtr m_free;

		// debug only freelist consistency checks
		void CheckFreelist();
		bool BlocksAreContiguous(void *item1, void *item2);

		// Remove a block from a free list (inlined for speed)
		inline void RemoveFromList(HeapBlock *block)
		{
			GCAssert(!block->inUse());
			block->prev->next = block->next;
			block->next->prev = block->prev;
			block->next = block->prev = 0;
		}			


		// Map a number of blocks to the appropriate large block free list index
		// (inlined for speed)
		inline int GetFreeListIndex(int size)
		{
			if (size <= kUniqueThreshold) {
				return size-1;
			} else if (size >= kHugeThreshold) {
				return kNumFreeLists-1;
			} else {
				return (size-kUniqueThreshold)/kFreeListCompression+kUniqueThreshold-1;
			}
		}

		// used for decommit smoothing
		// millis to wait before decommitting anything
		uint64 decommitTicks;
		uint64 decommitThresholdTicks;

#ifdef GCHEAP_LOCK
		GCSpinLock m_spinlock;
#endif /* GCHEAP_LOCK */

#ifdef MMGC_AVMPLUS
		// OS abstraction to determine native page size
		int vmPageSize();
		size_t committedCodeMemory;

		#ifdef WIN32
		bool useGuardPages;
		#endif
public:
		// support for jit buffers
		void* ReserveCodeMemory(void* address, size_t size);
		void* CommitCodeMemory(void* address, size_t size=0);  // size=0 => 1 page
		void* DecommitCodeMemory(void* address, size_t size=0);  // size=0 => 1 page
		void ReleaseCodeMemory(void* address, size_t size);
		bool SetGuardPage(void *address);
#ifdef AVMPLUS_JIT_READONLY
		void SetExecuteBit(void *address, size_t size, bool executeFlag);
#endif /* AVMPLUS_JIT_READONLY */
		size_t GetCodeMemorySize() const { return committedCodeMemory; }
#endif

#ifdef USE_MMAP
	public:
		char *ReserveMemory(char *address, size_t size);
		bool CommitMemory(char *address, size_t size);
		bool DecommitMemory(char *address, size_t size);
		void ReleaseMemory(char *address, size_t size);


		bool CommitMemoryThatMaySpanRegions(char *address, size_t size);
		bool DecommitMemoryThatMaySpanRegions(char *address, size_t size);		
#else
		char *AllocateMemory(size_t size);
		void ReleaseMemory(char *address);
#endif

private:

#ifdef _DEBUG
		/* m_megamap is a debugging aid for finding bugs in this
		   memory allocator.  It tracks allocated/free pages in
		   the crudest way possible ... a 1MB byte array with a
		   0/1 byte for every page in the 32-bit address space. */
		static uint8 m_megamap[1048576];
#endif
	};
}

#endif /* __GCHeap__ */
