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

#define kBlockHeadSize offsetof(MMgc::FixedAlloc::FixedBlock, items)

namespace MMgc
{
	FixedAlloc::FixedAlloc(int itemSize, GCHeap* heap)
	{
		m_heap = heap;

		m_firstBlock    = NULL;
		m_lastBlock     = NULL;
		m_firstFree     = NULL;
		m_maxAlloc      = 0;

#ifdef MEMORY_INFO
		itemSize += DebugSize();
#endif

		m_itemSize      = itemSize;

		// The number of items per block is kBlockSize minus
		// the # of pointers at the base of each page.
		size_t usableSpace = GCHeap::kBlockSize - kBlockHeadSize;
		m_itemsPerBlock = usableSpace / m_itemSize;
	}

	FixedAlloc::~FixedAlloc()
	{   
		// Free all of the blocks
 		while (m_firstBlock) {
#ifdef MEMORY_INFO
			if(m_firstBlock->numAlloc > 0) {
				// go through every memory location, if the fourth 4 bytes cast as
				// an integer isn't 0xedededed then its allocated space and the integer is
				// an index into the stack trace table, the first 4 bytes will contain
				// the freelist pointer for free'd items (which is why the trace index is
				// stored in the second 4)
				// first 4 bytes - free list pointer
				// 2nd 4 bytes - alloc stack trace
				// 3rd 4 bytes - free stack trace
				// 4th 4 bytes - 0xedededed if freed correctly
				unsigned int *mem = (unsigned int*) m_firstBlock->items;
				unsigned int itemNum = 0;
				while(itemNum++ < m_itemsPerBlock) {
					unsigned int fourthInt = *(mem+3);
					if(fourthInt != 0xedededed) {
						GCDebugMsg(false, "Leaked %d byte item.  Addr: 0x%x\n", GetItemSize(), mem+2);
						PrintStackTraceByIndex(*(mem+1));
					}
					mem += (m_itemSize / sizeof(int));
				}
				GCAssert(false);
			}

			// go through every item on the free list and make sure it wasn't written to
			// after being poisoned.
			void *item = m_firstBlock->firstFree;
			while(item) {
			#ifdef MMGC_64BIT
				for(int i=3, n=(m_firstBlock->size>>2)-3; i<n; i++)
			#else
				for(int i=3, n=(m_firstBlock->size>>2)-1; i<n; i++)
			#endif
				{
					unsigned int data = ((int*)item)[i];
					if(data != 0xedededed)
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
			FreeChunk(m_firstBlock);
		}
	}

	
	size_t FixedAlloc::Allocated()
	{
		size_t bytes = 0;
		FixedBlock *b = m_firstBlock;
		while(b)
		{
			bytes += b->numAlloc * b->size;
			b = b->next;
		}
		return bytes;
	}

	FixedAlloc::FixedBlock* FixedAlloc::CreateChunk()
	{
		// Allocate a new block
		m_maxAlloc += m_itemsPerBlock;

		FixedBlock* b = (FixedBlock*) m_heap->Alloc(1, true, false);
		
		GCAssert(m_itemSize <= 0xffff);
		b->numAlloc = 0;
		b->size = (uint16)m_itemSize;
		b->firstFree = 0;
		b->nextItem = b->items;
		b->alloc = this;

#ifdef _DEBUG
		// deleted and unused memory is 0xed'd, this is important for leak diagnostics
		memset(b->items, 0xed, m_itemSize * m_itemsPerBlock);
#endif

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

		return b;
	}
	
	void FixedAlloc::FreeChunk(FixedBlock* b)
	{
	  m_maxAlloc -= m_itemsPerBlock;

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

		// If this is the first free block, pick a new one...
		if ( m_firstFree == b )
			m_firstFree = b->nextFree;
		else if (b->prevFree)
			b->prevFree->nextFree = b->nextFree;

		if (b->nextFree)
			b->nextFree->prevFree = b->prevFree;

		// Free the memory
		m_heap->Free(b);
	}

	size_t FixedAlloc::GetItemSize() const
	{
		return m_itemSize - DebugSize();
	}

	void *FastAllocator::operator new[](size_t size)
	{
		return FixedMalloc::GetInstance()->Alloc(size);
	}
	
	void FastAllocator::operator delete [](void *item)
	{
		FixedMalloc::GetInstance()->Free(item);
	}
}
