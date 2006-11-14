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


#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "MMgc.h"

namespace MMgc
{
	GCAlloc::GCAlloc(GC* gc, int itemSize, bool containsPointers, bool isRC, int sizeClassIndex) : 
		m_gc(gc),
		containsPointers(containsPointers), 
		containsRCObjects(isRC),
		m_sizeClassIndex(sizeClassIndex)
	{
		// Round itemSize to the nearest boundary of 8
		itemSize = (itemSize+7)&~7;

		m_firstBlock    = NULL;
		m_lastBlock     = NULL;
		m_firstFree     = NULL;
		m_needsSweeping = NULL;
		m_numAlloc      = 0;
		m_maxAlloc      = 0;
		m_itemSize      = itemSize;
		m_numBlocks = 0;
		m_finalized = false;

		// The number of items per block is kBlockSize minus
		// the # of pointers at the base of each page.

		m_itemsPerBlock = (kBlockSize - sizeof(GCBlock)) / m_itemSize;

		m_numBitmapBytes = (m_itemsPerBlock>>1) + (m_itemsPerBlock & 1);
		// round up to 4 bytes so we can go through the bits 8 items at a time
		m_numBitmapBytes = (m_numBitmapBytes+3)&~3;

		GCAssert(m_numBitmapBytes<<1 >= m_itemsPerBlock);

		int usedSpace = m_itemsPerBlock * m_itemSize + sizeof(GCBlock);
		GCAssert(usedSpace <= kBlockSize);
		GCAssert(kBlockSize - usedSpace < (int)m_itemSize);
		
		// never store the bits in the page for !containsPointers b/c we don't want
		// to force pages into memory for bit marking purposes when we don't need
		// to bring them in for scanning purposes
		// ISSUE: is this bitsInPage stuff really worth it?  Maybe simplicity and 
		// locality suggest otherwise?
		m_bitsInPage = containsPointers && kBlockSize - usedSpace >= m_numBitmapBytes;

		// compute values that let us avoid division
		GCAssert(m_itemSize <= 0xffff);
		ComputeMultiplyShift((uint16)m_itemSize, multiple, shift);
	}

	GCAlloc::~GCAlloc()
	{   
		// Free all of the blocks
		GCAssertMsg(GetNumAlloc() == 0, "You have leaks");

		while (m_firstBlock) {
			if(((uintptr)m_firstBlock->bits & 0xfff) == 0)
				m_gc->GetGCHeap()->Free(m_firstBlock->bits);
#ifdef _DEBUG
			// go through every item on the free list and make sure it wasn't written to
			// after being poisoned.
			void *item = m_firstBlock->firstFree;
			while(item) {
				for(int i=3, n=(m_firstBlock->size>>2)-1; i<n; i++)
				{
					int data = ((int*)item)[i];
					if(data != 0xbabababa && data != 0xcacacaca)
					{
						GCDebugMsg(false, "Object 0x%x was written to after it was deleted, allocation trace:");
						PrintStackTrace((int*)item+2);
						GCDebugMsg(false, "Deletion trace:");
						PrintStackTrace((int*)item+3);
						GCDebugMsg(true, "Deleted item write violation!");
					}
				}
				// next free item
				item = *((void**)item);
			}
#endif
			GCBlock *b = m_firstBlock;
			UnlinkChunk(b);
			FreeChunk(b);
		}
	}

	GCAlloc::GCBlock* GCAlloc::CreateChunk()
	{
		// Get space in the bitmap.  Do this before allocating the actual block,
		// since we might call GC::AllocBlock for more bitmap space and thus
		// cause some incremental marking.
		uint32* bits = NULL;

		if(!m_bitsInPage)
			bits = m_gc->GetBits(m_numBitmapBytes, m_sizeClassIndex);

		// Allocate a new block
		m_maxAlloc += m_itemsPerBlock;
		m_numBlocks++;

		int numBlocks = kBlockSize/GCHeap::kBlockSize;
		GCBlock* b = (GCBlock*) m_gc->AllocBlock(numBlocks, GC::kGCAllocPage);

		b->gc = m_gc;
		b->alloc = this;
		b->size = m_itemSize;
		b->needsSweeping = false;
		if(m_gc->collecting && m_finalized)
			b->finalizeState = m_gc->finalizedValue;
		else 
			b->finalizeState = !m_gc->finalizedValue;

		b->bits = m_bitsInPage ? (uint32*)((char*)b + sizeof(GCBlock)) : bits;

		// Link the block at the end of the list
		b->prev = m_lastBlock;
		b->next = 0;
		
		if (m_lastBlock) {
			m_lastBlock->next = b;
		}
		if (!m_firstBlock) {
			m_firstBlock = b;
		}
		m_lastBlock = b;

		// Add our new ChunkBlock to the firstFree list (which should be empty)
		if (m_firstFree)
		{
			GCAssert(m_firstFree->prevFree == 0);
			m_firstFree->prevFree = b;
		}
		b->nextFree = m_firstFree;
		b->prevFree = 0;
		m_firstFree = b;

		// calculate back from end (better alignment, no dead space at end)
		b->items = (char*)b+GCHeap::kBlockSize - m_itemsPerBlock * m_itemSize;
		b->nextItem = b->items;
		b->numItems = 0;

		return b;
	}

	void GCAlloc::UnlinkChunk(GCBlock *b)
	{
		m_maxAlloc -= m_itemsPerBlock;
		m_numBlocks--;

		// Unlink the block from the list
		if (b == m_firstBlock) {
			m_firstBlock = b->next;
		} else {
			b->prev->next = b->next;
		}
		
		if (b == m_lastBlock) {
			m_lastBlock = b->prev;
		} else {
			b->next->prev = b->prev;
		}

		if(b->nextFree || b->prevFree || b == m_firstFree) {
			RemoveFromFreeList(b);
		}
#ifdef _DEBUG
		b->next = b->prev = NULL;
		b->nextFree = b->prevFree = NULL;
#endif
	}

	void GCAlloc::FreeChunk(GCBlock* b)
	{
		GCAssert(b->numItems == 0);
		if(!m_bitsInPage) {
			memset(b->GetBits(), 0, m_numBitmapBytes);
			m_gc->FreeBits(b->GetBits(), m_sizeClassIndex);
			b->bits = NULL;
		}

		// Free the memory
		m_gc->FreeBlock(b, 1);
	}

	void* GCAlloc::Alloc(size_t size, int flags)
	{
		(void)size;
		GCAssertMsg(((size_t)m_itemSize >= size), "allocator itemsize too small");
start:
		if (m_firstFree == NULL && m_needsSweeping == NULL) {
			if (CreateChunk() == NULL) {
				return NULL;
			}
		}
	
		GCBlock* b = m_firstFree ? m_firstFree : m_needsSweeping;

		// lazy sweeping
		if(b->needsSweeping) {
			if(m_gc->collecting) {
				CreateChunk();
				b = m_firstFree;
			}
			else if(Sweep(b)) {
				goto start;
			}
		}

		GCAssert(b == m_firstFree);

		GCAssert(b && !b->IsFull());

		void *item;
		if(b->firstFree) {
			item = b->firstFree;
			b->firstFree = *((void**)item);
			// clear free list pointer, the rest was zero'd in free
			*(int*) item = 0;
#ifdef MEMORY_INFO
			// ensure previously used item wasn't written to
			// -1 because write back pointer space isn't poisoned.
#ifdef MMGC_64BIT			
			for(int i=3, n=(b->size>>2)-3; i<n; i++)
#else
			for(int i=3, n=(b->size>>2)-1; i<n; i++)
#endif			
			{
				int data = ((int*)item)[i];
				if(data != 0xcacacaca && data != 0xbabababa)
				{
					GCDebugMsg(false, "Object 0x%x was written to after it was deleted, allocation trace:", item);
					PrintStackTrace((int*)item+2);
					GCDebugMsg(false, "Deletion trace:");
					PrintStackTrace((int*)item+3);
					GCDebugMsg(true, "Deleted item write violation!");
				}
			}
#endif
		} else {
			item = b->nextItem;
			if(((uintptr)((char*)item + b->size) & 0xfff) != 0) {
				b->nextItem = (char*)item +  b->size;
			} else {
				b->nextItem = NULL;
			}
		}

		// set up bits, items start out white and whether they need finalization
		// is determined by the caller

		// make sure we ended up in the right place
		GCAssert(((flags&GC::kContainsPointers) != 0) == ContainsPointers());

		// this assumes what we assert
		GCAssert((unsigned long)GC::kFinalize == (unsigned long)GCAlloc::kFinalize);
		int index = GetIndex(b, item);
		
		GCAssert(index >= 0);

		ClearBits(b, index, 0xf);
		SetBit(b, index, flags & kFinalize);

		b->numItems++;
#ifdef MEMORY_INFO
		m_numAlloc++;
#endif

		// If we're out of free items, be sure to remove ourselves from the
		// list of blocks with free items.  
		if (b->IsFull()) {
			m_firstFree = b->nextFree;
			b->nextFree = NULL;
			GCAssert(b->prevFree == NULL);

			if (m_firstFree)
				m_firstFree->prevFree = 0;
		}

		// prevent mid-collection (ie destructor) allocations on un-swept pages from
		// getting swept.  If the page is finalized and doesn't need sweeping we don't want
		// to set the mark otherwise it will be marked when we start the next marking phase
		// and write barrier's won't fire (since its black)
		if(m_gc->collecting)
		{ 
			if((b->finalizeState != m_gc->finalizedValue) || b->needsSweeping)
				SetBit(b, index, kMark);
		}

		return item;
	}

	/* static */
	void GCAlloc::Free(void *item)
	{
		GCBlock *b = (GCBlock*) ((uintptr) item & ~0xFFF);
		GCAlloc *a = b->alloc;
	
#ifdef _DEBUG		
		// check that its not already been freed
		void *free = b->firstFree;
		while(free) {
			GCAssert(free != item);
			free = *((void**) free);
		}
#endif

		int index = GetIndex(b, item);
		if(GetBit(b, index, kHasWeakRef)) {
			b->gc->ClearWeakRef(GetUserPointer(item));
		}
		
		bool wasFull = b->IsFull();

		if(b->needsSweeping) {
			bool gone = a->Sweep(b);
			if(gone) {
				GCAssertMsg(false, "How can a page I'm about to free an item on be empty?");
			}
			wasFull = false;
		}

		if(wasFull) {
			a->AddToFreeList(b);
		}

		b->FreeItem(item, index);

		if(b->numItems == 0) {
			a->UnlinkChunk(b);
			a->FreeChunk(b);
		}
	}

	void GCAlloc::Finalize()
	{
		m_finalized = true;
		// Go through every item of every block.  Look for items
		// that are in use but not marked as reachable, and delete
		// them.
		
		GCBlock *next = NULL;
		for (GCBlock* b = m_firstBlock; b != NULL; b = next)
		{
			// we can unlink block below
			next = b->next;

			// remove from freelist to avoid mutator destructor allocations
			// from using this block
			bool putOnFreeList = false;
			if(m_firstFree == b || b->prevFree != NULL || b->nextFree != NULL) {
				putOnFreeList = true;
				RemoveFromFreeList(b);
			}

			GCAssert(kMark == 0x1 && kFinalize == 0x4 && kHasWeakRef == 0x8);

			int numMarkedItems = 0;

			// TODO: MMX version for IA32
			uint32 *bits = (uint32*) b->GetBits();
			uint32 count = b->nextItem ? GetIndex(b, b->nextItem) : m_itemsPerBlock;
			// round up to eight
			uint32 numInts = ((count+7)&~7) >> 3;
			for(uint32 i=0; i < numInts; i++) 
			{
				uint32 marks = bits[i];					
				// hmm, is it better to screw around with exact counts or just examine
				// 8 items on each pass, with the later we open the door to unrolling
				uint32 subCount = i==(numInts-1) ? ((count-1)&7)+1 : 8;
				for(uint32 j=0; j<subCount;j++,marks>>=4)
				{
					int mq = marks & kFreelist;
					if(mq == kFreelist)
						continue;

					if(mq == kMark) {
						numMarkedItems++;
						continue;
					}

					GCAssertMsg(mq != kQueued, "No queued objects should exist when finalizing");

					if(!(marks & (kFinalize|kHasWeakRef)))
						continue;
        
					void* item = (char*)b->items + m_itemSize*((i*8)+j);

					if (marks & kFinalize)
					{     
						GCFinalizedObject *obj = (GCFinalizedObject*)GetUserPointer(item);
						GCAssert(*(int*)obj != 0);
						obj->~GCFinalizedObject();

						bits[i] &= ~(kFinalize<<(j*4));

#if defined(_DEBUG) && defined(MMGC_DRC)
						if(b->alloc->IsRCObject()) {
							m_gc->RCObjectZeroCheck((RCObject*)obj);
						}
#endif
					}

					if (marks & kHasWeakRef) {							
						b->gc->ClearWeakRef(GetUserPointer(item));
					}
				}
			}

			if(numMarkedItems == 0) {
				// add to list of block to be returned to the Heap after finalization
				// we don't do this during finalization b/c we want finalizers to be able
				// to reference the memory of other objects being finalized
				UnlinkChunk(b);
				b->gc->AddToSmallEmptyBlockList(b);
				putOnFreeList = false;
			} else if(numMarkedItems == b->numItems) {
				// nothing changed on this page, clear marks
				ClearMarks(b);
			} else if(!b->needsSweeping) {
				// free'ing some items but not all
				if(b->nextFree || b->prevFree || b == m_firstFree) {
					RemoveFromFreeList(b);
					b->nextFree = b->prevFree = NULL;
				}
				AddToSweepList(b);
				putOnFreeList = false;
			}
			b->finalizeState = m_gc->finalizedValue;
			if(putOnFreeList)
				AddToFreeList(b);
		}
	}

	void GCAlloc::SweepGuts(GCBlock *b)
	{	
		// TODO: MMX version for IA32
		uint32 *bits = (uint32*) b->GetBits();
		uint32 count = b->nextItem ? GetIndex(b, b->nextItem) : m_itemsPerBlock;
		// round up to eight
		uint32 numInts = ((count+7)&~7) >> 3;
		for(uint32 i=0; i < numInts; i++) 
		{
			uint32 marks = bits[i];
			// hmm, is it better to screw around with exact counts or just examine
			// 8 items on each pass, with the later we open the door to unrolling
			uint32 subCount = i==(numInts-1) ? ((count-1)&7)+1 : 8;
			for(uint32 j=0; j<subCount;j++,marks>>=4)
			{
				int mq = marks & kFreelist;
				if(mq == kMark)
				{
					// live item, clear bits
					bits[i] &= ~(kFreelist<<(j*4));
					continue;					
				}

				 if(mq == kFreelist)
					 continue; // freelist item, ignore

				// garbage, freelist it
				void *item = (char*)b->items + m_itemSize*(i*8+j);

#ifdef MEMORY_INFO 
				DebugFreeReverse(item, 0xba, 4);
#endif
				b->FreeItem(item, (i*8+j));
			}
		}
	}

	bool GCAlloc::Sweep(GCBlock *b)
	{	
		GCAssert(b->needsSweeping);
		RemoveFromSweepList(b);

		SweepGuts(b);

		if(b->numItems == 0)
		{
			UnlinkChunk(b);
			FreeChunk(b);
			return true;
		} 

		AddToFreeList(b);

		return false;
	}
		
	void GCAlloc::SweepNeedsSweeping()
	{
		GCBlock* next;
		for (GCBlock* b = m_needsSweeping; b != NULL; b = next)
		{
			next = b->nextFree;	
			Sweep(b);
		}
		GCAssert(m_needsSweeping == NULL);
	}

	void GCAlloc::ClearMarks(GCAlloc::GCBlock* block)
	{
        // Clear all the mark bits
		uint32 *pbits =  (uint32*)block->GetBits();
		const static uint32 mq32 = 0x33333333;
		GCAssert((kMark|kQueued) == 0x3);
		// TODO: MMX version for IA32
		for(int i=0, n=m_numBitmapBytes>>2; i < n; i++) {
			pbits[i] &= ~mq32;
        }
		
		const void *item = block->firstFree;
		while(item != NULL) {
			// set freelist bit pattern
			SetBit(block, GetIndex(block, item), kFreelist);
			item = *(const void**)item;
		}
	}

	void GCAlloc::ClearMarks()
	{
		GCBlock *block = m_firstBlock;
start:
		while (block) {
			GCBlock *next = block->next;

			if(block->needsSweeping) {
				if(Sweep(block)) {
					UnlinkChunk(block);
					FreeChunk(block);
					block = next;
					goto start;
				}
			}

			ClearMarks(block);

			// Advance to next block
			block = next;
		}
	}	

#ifdef _DEBUG
	void GCAlloc::CheckMarks()
	{
		GCBlock *b = m_firstBlock;

		while (b) {
			GCBlock *next = b->next;
			GCAssertMsg(!b->needsSweeping, "All needsSweeping should have been swept at this point.");

			// TODO: MMX version for IA32
			uint32 *bits = (uint32*) b->GetBits();
			uint32 count = b->nextItem ? GetIndex(b, b->nextItem) : m_itemsPerBlock;

			// round up to eight
			uint32 numInts = ((count+7)&~7) >> 3;
			for(uint32 i=0; i < numInts; i++) 
			{
				uint32 marks = bits[i];
				// hmm, is it better to screw around with exact counts or just examine
				// 8 items on each pass, with the later we open the door to unrolling
				uint32 subCount = i==(numInts-1) ? ((count-1)&7)+1 : 8;
				for(uint32 j=0; j<subCount;j++,marks>>=4)
				{
					uint32 m = marks&kFreelist;
					GCAssertMsg(m == 0 || m == kFreelist, "All items should be free or clear, nothing should be marked or queued.");
				}
			}
			
			// Advance to next block
			b = next;
		}
	}	
#endif

	/*static*/
	int GCAlloc::ConservativeGetMark(const void *item, bool bogusPointerReturnValue)
	{
		GCBlock *block = (GCBlock*) ((uintptr) item & ~0xFFF);

#ifdef MEMORY_INFO
		item = GetRealPointer(item);
#endif

		// guard against bogus pointers to the block header
		if (item < block->items)
			return bogusPointerReturnValue;

		// floor value to start of item
		// FIXME: do this w/o division if we can
		int itemNum = GetIndex(block, item);

		// skip pointers into dead space at end of block
		if (itemNum > block->alloc->m_itemsPerBlock - 1)
			return bogusPointerReturnValue;

		// skip pointers into objects
		if(block->items + itemNum * block->size != item)
			return bogusPointerReturnValue;

		return GetMark(item);
	}

	// allows us to avoid division in GetItemIndex, kudos to Tinic
	void GCAlloc::ComputeMultiplyShift(uint16 d, uint16 &muli, uint16 &shft) 
	{
		uint32 s = 0;
		uint32 n = 0;
		uint32 m = 0;
		for ( ; n < ( 1 << 13 ) ; s++) {
			m = n;
			n = ( ( 1 << ( s + 1 ) ) / d ) + 1;
		}
		shft = (uint16) s - 1;
		muli = (uint16) m;
	}

	void GCAlloc::GCBlock::FreeItem(void *item, int index)
	{
		GCAssert(alloc->m_numAlloc != 0);

		void *oldFree = firstFree;
		firstFree = item;
#ifdef MEMORY_INFO
		alloc->m_numAlloc--;
#endif
		numItems--;

		SetBit(this, index, kFreelist);

#ifndef _DEBUG
		// memset rest of item not including free list pointer, in _DEBUG
		// we poison the memory (and clear in Alloc)
		// FIXME: can we do something faster with MMX here?
		if(!alloc->IsRCObject())
			memset((char*)item, 0, size);
#endif
		// Add this item to the free list
		*((void**)item) = oldFree;	
	}
}
