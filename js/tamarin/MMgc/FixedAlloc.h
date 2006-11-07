/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#ifndef __FixedAlloc__
#define __FixedAlloc__


namespace MMgc
{
	/**
	 * This is a fast, fixed-size memory allocator for manually freed
	 * objects.
	 *
	 * Memory is allocated from the system in 4096 byte chunks
	 * via the GCHeap class.  Not that this won't work well for
	 * large objects (>400 bytes).  For that, we have the
	 * FixedAllocLarge allocator which will allocate multiple
	 * pages at a time to minimize waste.
	 */
	class FixedAlloc : public GCAllocObject
	{
		friend class FixedMalloc;
		friend class FastAllocator;
		friend class GC;
	public:
		FixedAlloc(int itemSize, GCHeap* heap);
		~FixedAlloc();

		inline void* Alloc(size_t size)
		{ 
			(void)size;
			
			GCAssertMsg(((size_t)m_itemSize >= size), "allocator itemsize too small");

			if(!m_firstFree) {
				if(CreateChunk() == NULL) {
					GCAssertMsg(false, "Out of memory!");
					return NULL;
				}
			}

			FixedBlock* b = m_firstFree;
			GCAssert(b && !IsFull(b));

			b->numAlloc++;

			// Consume the free list if available
			void *item = NULL;
			if (b->firstFree) {
				item = b->firstFree;
				b->firstFree = *((void**)item);
				// assert that the freelist hasn't been tampered with (by writing to the first 4 bytes)
				GCAssert(b->firstFree == NULL || 
						(b->firstFree >= b->items && 
						(((intptr)b->firstFree - (intptr)b->items) % b->size) == 0 && 
						(intptr) b->firstFree < ((intptr)b & ~0xfff) + GCHeap::kBlockSize));
#ifdef MEMORY_INFO				
				// ensure previously used item wasn't written to
				// -1 because write back pointer space isn't poisoned.
				for(int i=3, n=(b->size>>2)-1; i<n; i++)
				{
					int data = ((int*)item)[i];
					if(data != (int)0xedededed)
					{
						GCDebugMsg(false, "Object 0x%x was written to after it was deleted, allocation trace:", (int*)item+2);
						PrintStackTrace((int*)item+2);
						GCDebugMsg(false, "Deletion trace:");
						PrintStackTrace((int*)item+3);
						GCDebugMsg(true, "Deleted item write violation!");
					}
				}
#endif
			} else {
				// Take next item from end of block
				item = b->nextItem;
				GCAssert(item != 0);
				if(!IsFull(b)) {
					// There are more items at the end of the block
					b->nextItem = (void *) ((intptr)item+m_itemSize);
#ifdef MEMORY_INFO
					// space made in ctor
					item = DebugDecorate(item, size + DebugSize(), 6);
					memset(item, 0xfa, size);
#endif
					return item;
				}
				b->nextItem = 0;
			}


			// If we're out of free items, be sure to remove ourselves from the
			// list of blocks with free items.  
			if (IsFull(b)) {
				m_firstFree = b->nextFree;
				b->nextFree = NULL;
				GCAssert(b->prevFree == NULL);

				if (m_firstFree)
					m_firstFree->prevFree = 0;
				else
					CreateChunk();
			}

#ifdef MEMORY_INFO
			// space made in ctor
			item = DebugDecorate(item, size + DebugSize(), 6);
			memset(item, 0xfa, size);
#endif

			GCAssertMsg(item != NULL, "Out of memory");
			return item;
		}

		static inline void Free(void *item)
		{
			FixedBlock *b = (FixedBlock*) ((uint32)item & ~0xFFF);

#ifdef MEMORY_INFO
			item = DebugFree(item, 0xED, 6);
#endif

#ifdef _DEBUG
			// ensure that we are freeing a pointer on a item boundary
			GCAssert(((intptr)item - (intptr)b->items) % b->alloc->m_itemSize == 0);
#endif

			// Add this item to the free list
			*((void**)item) = b->firstFree;
			b->firstFree = item;

			// We were full but now we have a free spot, add us to the free block list.
			if (b->numAlloc == b->alloc->m_itemsPerBlock)
			{
				GCAssert(!b->nextFree && !b->prevFree);
				b->nextFree = b->alloc->m_firstFree;
				if (b->alloc->m_firstFree)
					b->alloc->m_firstFree->prevFree = b;
				b->alloc->m_firstFree = b;
			}
#ifdef _DEBUG
			else // we should already be on the free list
			{
				GCAssert ((b == b->alloc->m_firstFree) || b->prevFree);
			}
#endif

			b->numAlloc--;

			if(b->numAlloc == 0) {
				b->alloc->FreeChunk(b);
			}
		}

		size_t Allocated();

		size_t GetItemSize() const;
		int GetMaxAlloc() const { return m_maxAlloc; }

		static FixedAlloc *GetFixedAlloc(void *item)
		{
			FixedBlock *b = (FixedBlock*) ((uint32)item & ~0xFFF);
#ifdef _DEBUG
			// Attempt to sanity check this ptr: numAllocs * size should be less than kBlockSize
			GCAssertMsg(((b->numAlloc * b->size) < GCHeap::kBlockSize), "Size called on ptr not part of FixedBlock");
#endif
			return b->alloc;
		}

	private:

		struct FixedBlock
		{
			void*  firstFree;        // first item on free list
			void*  nextItem;         // next free item
			FixedBlock* next;
			FixedBlock* prev;
			uint16 numAlloc;
			uint16 size;
			FixedBlock *prevFree;
			FixedBlock *nextFree;
			FixedAlloc *alloc;
			char   items[1];
		};

		GCHeap *m_heap;
		unsigned int    m_itemsPerBlock;
		size_t    m_itemSize;

		// The list of chunk blocks
		FixedBlock* m_firstBlock; 
		FixedBlock* m_lastBlock;

		// The lowest priority block that has free items		
		FixedBlock* m_firstFree;

		int    m_maxAlloc;

		bool IsFull(FixedBlock *b) const { return b->numAlloc == m_itemsPerBlock; }
		FixedBlock* CreateChunk();
		void FreeChunk(FixedBlock* b);

		static inline size_t Size(const void *item)
		{
			FixedBlock *b = (FixedBlock*) ((uint32)item & ~0xFFF);
#ifdef _DEBUG
			// Attempt to sanity check this ptr: numAllocs * size should be less than kBlockSize
			GCAssertMsg(((b->numAlloc * b->size) < GCHeap::kBlockSize), "Size called on ptr not part of FixedBlock");
#endif
			return b->size;
		}
	};

	class FixedAllocSafe : public FixedAlloc
	{
	public:
		FixedAllocSafe(int itemSize, GCHeap* heap) : FixedAlloc(itemSize, heap) {}
		
		void* Alloc(size_t size)
		{ 		
#ifdef GCHEAP_LOCK
			GCAcquireSpinlock lock(m_spinlock);
#endif
			return FixedAlloc::Alloc(size); 
		}

		void Free(void *ptr)
		{
#ifdef GCHEAP_LOCK
			GCAcquireSpinlock lock(m_spinlock);
#endif
			FixedAlloc::Free(ptr);
		}

		static FixedAllocSafe *GetFixedAllocSafe(void *item)
		{
			return (FixedAllocSafe*) FixedAlloc::GetFixedAlloc(item);
		}

	private:

#ifdef GCHEAP_LOCK
		GCSpinLock m_spinlock;
#endif
	};

	/**
	 * classes that need fast lock free allocation should subclass this and pass
	 * a FixedAlloc * to the new parameter.  One new/delete are lock free, scalar
	 * allocations use the normal locked general size allocator.
	 */
	class FastAllocator 
	{		
	public:
		static void *operator new(size_t size, FixedAlloc *alloc)
		{
			return alloc->Alloc(size);
		}
		
		static void operator delete (void *item)
		{
			FixedAlloc::Free(item);
		}

		// allow array allocation  as well		
		static void *operator new[](size_t size);		
		static void operator delete [](void *item);
	};
}

#endif /* __FixedAlloc__ */
