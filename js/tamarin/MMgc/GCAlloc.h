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

#ifndef __GCAlloc__
#define __GCAlloc__


namespace MMgc
{
	/**
	 *
	 * This is a fast, fixed-size memory allocator for garbage-collected
	 * objects.
	 *
	 * Memory is allocated from the system on 4096-byte aligned boundaries,
	 * which corresponds to the size of an OS page in Windows XP.  Allocation
	 * of pages is performed via the GCPageAlloc class.
	 *
	 * In each 4096-byte block, there is a block header with marked bits,
	 * finalize bits, the pointer to the next free item and "recycling"
	 * free item linked list.
	 *
	 * The bits of the "marked" bitmap are controlled by the SetMark method.
	 *
	 * The bits of the "finalize" bitmap are set when an item is
	 * allocated.  The value for the finalize bit is passed in as a
	 * parameter to the allocation call.
	 *
	 * When the Sweep method is invoked, all objects that are not marked
	 * with the specified mark flag are disposed of.  If the corresponding
	 * finalize bit is set, the GCObject destructor is invoked on that
	 * item.
	 *
	 * When an allocation is requested and there are no more free
	 * entries, GCAlloc will request that a garbage collection take
	 * place.  It will allocate new blocks if more than 20% of its
	 * blocks are used after the collection, targeting a 5:1
	 * heap size / minimim heap size ratio.
	 * 
	 */
	class GCAlloc : public GCAllocObject
	{
		friend class GC;
	public:
		enum ItemBit { kMark=1, kQueued=2, kFinalize=4, kHasWeakRef=8, kFreelist=kMark|kQueued };

		GCAlloc(GC* gc, int itemSize, bool containsPointers, bool isRC, int sizeClassIndex);
		~GCAlloc();
		
		void* Alloc(size_t size, int flags);
		static void Free(void *ptr);
		void Finalize();
		size_t GetItemSize() { return m_itemSize; }
		void ClearMarks();
#ifdef _DEBUG
		void CheckMarks();
#endif

		static int SetMark(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			int index = GetIndex(block, item);
			int mask = kMark << ((index&7)<<2);
			uint32 *bits = &block->GetBits()[index>>3];
			int set = *bits & mask;
			*bits |= mask;
			*bits &= ~(kQueued << ((index&7)<<2));
			return set;
		}

		static int SetQueued(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			return SetBit(block, GetIndex(block, item), kQueued);
		}
		
		static int SetFinalize(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			return SetBit(block, GetIndex(block, item), kFinalize);
		}
		
		static int IsWhite(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);

			// not a real item
			if(item < block->items)
				return false;

			if(FindBeginning(item) != item)
				return false;

			return IsWhite(block, GetIndex(block, item));
		}


		static int GetMark(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
		
			// Return the "marked" bit
			return GetBit(block, GetIndex(block, item), kMark);
		}

		static void *FindBeginning(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);

			return block->items + block->size * GetIndex(block, item);
		}

		static void ClearFinalized(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			ClearBits(block, GetIndex(block, item), kFinalize);
		}		

		static int IsFinalized(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			return GetBit(block, GetIndex(block, item), kFinalize);
		}		
		static int HasWeakRef(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			return GetBit(block, GetIndex(block, item), kHasWeakRef);
		}		
		
		static bool ContainsPointers(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			return block->alloc->ContainsPointers();
		}

		static bool IsRCObject(const void *item)
		{
			// Zero low 12 bits of address to get to the Block header
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			return item >= block->items && block->alloc->IsRCObject();
		}

		static bool IsUnmarkedPointer(const void *val);
		
		int GetNumAlloc() const { return m_numAlloc; }
		int GetMaxAlloc() const { return m_maxAlloc; }
		int GetNumBlocks() const { return m_numBlocks; }

		bool ContainsPointers() const { return containsPointers; }
		bool IsRCObject() const { return containsRCObjects; }

		void GetBitsPages(void **pages);

		static void SetHasWeakRef(const void *item, bool to)
		{
			GCBlock *block = (GCBlock*) ((intptr)item & ~0xFFF);
			if(to) {
				SetBit(block, GetIndex(block, item), kHasWeakRef);
			} else {
				ClearBits(block, GetIndex(block, item), kHasWeakRef);
			}
		}

	private:
		const static int kBlockSize = 4096;

		struct GCBlock;

		friend struct GCAlloc::GCBlock;

		struct GCBlock
		{
			GC *gc;
			GCBlock* next;
			uint32 size;
			GCAlloc *alloc;			
			GCBlock* prev;
			char*  nextItem;
			void*  firstFree;        // first item on free list
			GCBlock *prevFree;
			GCBlock *nextFree;
			uint32* bits;
			short numItems;
			bool needsSweeping:1; 
			bool finalizeState:1;  // whether we've been visited during the Finalize stage
			char   *items;

			int GetCount() const
			{
				if (nextItem) {
					return GCAlloc::GetIndex(this, nextItem);
				} else {
					return alloc->m_itemsPerBlock;
				}
			}

			uint32 *GetBits() const
			{
				return bits;
			}
			
			void FreeItem(void *item, int index);

			bool IsFull() const
			{
				bool full = (nextItem == firstFree);
				// the only time nextItem and firstFree should be equal is when they
				// are both zero which is also when we are full, assert to be sure
				GCAssert(!full || nextItem==0);
				GCAssert(!full || numItems == alloc->m_itemsPerBlock);
				return full;
			}
		};

		// The list of chunk blocks
		GCBlock* m_firstBlock; 
		GCBlock* m_lastBlock;

		// The lowest priority block that has free items		
		GCBlock* m_firstFree;

		// List of blocks that need sweeping
		GCBlock* m_needsSweeping;

		int    m_itemsPerBlock;
		size_t    m_itemSize;
		int m_numBitmapBytes;
		int m_sizeClassIndex;

		bool m_bitsInPage; 

		int    m_maxAlloc;
		int    m_numAlloc;
		int    m_numBlocks;

		// fast divide numbers
		uint16 multiple;
		uint16 shift;

		bool containsPointers;
		bool containsRCObjects;
		bool m_finalized;
		
		bool IsOnEitherList(GCBlock *b)
		{
			return b->nextFree != NULL || b->prevFree != NULL || b == m_firstFree || b == m_needsSweeping;
		}

		GCBlock* CreateChunk();
		void UnlinkChunk(GCBlock *b);
		void FreeChunk(GCBlock* b);
		void AddToFreeList(GCBlock *b)
		{
			GCAssert(!IsOnEitherList(b));
			b->prevFree = NULL;
			b->nextFree = m_firstFree;
			if (m_firstFree) {
				GCAssert(m_firstFree->prevFree == 0 && m_firstFree != b);
				m_firstFree->prevFree = b;
			}
			m_firstFree = b;			
		}

		void RemoveFromFreeList(GCBlock *b)
		{
			GCAssert(m_firstFree == b || b->prevFree != NULL);
			if ( m_firstFree == b )
				m_firstFree = b->nextFree;
			else
				b->prevFree->nextFree = b->nextFree;
			
			if (b->nextFree)
				b->nextFree->prevFree = b->prevFree;
			b->nextFree = b->prevFree = NULL;
		}

		void AddToSweepList(GCBlock *b)
		{
			GCAssert(!IsOnEitherList(b) && !b->needsSweeping);
			b->prevFree = NULL;
			b->nextFree = m_needsSweeping;
			if (m_needsSweeping) {
				GCAssert(m_needsSweeping->prevFree == 0);
				m_needsSweeping->prevFree = b;
			}
			m_needsSweeping = b;
			b->needsSweeping = true;
		}

		void RemoveFromSweepList(GCBlock *b)
		{
			GCAssert(m_needsSweeping == b || b->prevFree != NULL);
			if ( m_needsSweeping == b )
				m_needsSweeping = b->nextFree;
			else
				b->prevFree->nextFree = b->nextFree;
			
			if (b->nextFree)
				b->nextFree->prevFree = b->prevFree;
			b->needsSweeping = false;
			b->nextFree = b->prevFree = NULL;
		}

		bool Sweep(GCBlock *b);
		void SweepGuts(GCBlock *b);
		

		void ClearMarks(GCAlloc::GCBlock* block);
		void SweepNeedsSweeping();

		bool IsLastFreeBlock(GCBlock *b) { return m_firstFree == NULL || (m_firstFree == b && b->nextFree == NULL); }

		static int ConservativeGetMark(const void *item, bool bogusPointerReturnValue);
		
		static int GetIndex(const GCBlock *block, const void *item)
		{
			int index = (((char*) item - block->items) * block->alloc->multiple) >> block->alloc->shift;
#ifdef _DEBUG
			GCAssert(((char*) item - block->items) / block->size == (uint32) index);
#endif
			return index;
		}			

		static int IsWhite(GCBlock *block, int index)
		{
			return (block->GetBits()[index>>3] & ((kMark|kQueued)<<((index&7)<<2))) == 0;
		}

		static int SetBit(GCBlock *block, int index, int bit)
		{
			int mask = bit << ((index&7)<<2);
			int set = (block->GetBits()[index>>3] & mask);
			block->GetBits()[index>>3] |= mask;
			return set;
		}

		static int GetBit(GCBlock *block, int index, int bit)
		{
			int mask = bit << ((index&7)<<2);
			return block->GetBits()[index>>3] & mask;
		}

		static void ClearBits(GCBlock *block, int index, int bits)
		{
			int mask = bits << ((index&7)<<2);
			block->GetBits()[index>>3] &= ~mask;
		}

		void ComputeMultiplyShift(uint16 d, uint16 &muli, uint16 &shft);

	protected:
		GC *m_gc;

	};
}

#endif /* __GCAlloc__ */
