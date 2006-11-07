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


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "MMgc.h"

namespace MMgc
{
	GCLargeAlloc::GCLargeAlloc(GC* gc) : m_gc(gc)
	{
		m_blocks = NULL;
		m_startedFinalize = false;
	}

	void* GCLargeAlloc::Alloc(size_t size, int flags)
	{
		int blocks = (size+sizeof(LargeBlock)+GCHeap::kBlockSize-1) / GCHeap::kBlockSize;
		
		LargeBlock *block = (LargeBlock*) m_gc->AllocBlock(blocks, GC::kGCLargeAllocPageFirst, (flags&GC::kZero) != 0);
		void *item = NULL;

		if (block)
		{
			block->flags = ((flags&GC::kFinalize) != 0) ? kFinalizeFlag : 0;
			block->flags |= ((flags&GC::kContainsPointers) != 0) ? kContainsPointers : 0;
			block->flags |= ((flags&GC::kRCObject) != 0) ? kRCObject : 0;
			block->gc = this->m_gc;
			block->next = m_blocks;
			block->usableSize = blocks*GCHeap::kBlockSize - sizeof(LargeBlock);
			m_blocks = block;
			
			item = (void*)(block+1);

			if(m_gc->collecting && !m_startedFinalize)
				block->flags |= kMarkFlag;

#ifdef _DEBUG
			if (flags & GC::kZero)
			{
				// AllocBlock should take care of this
				for(int i=0, n=size/sizeof(int); i<n; i++) {
					if(((int*)item)[i] != 0)
						GCAssert(false);
				}
			}
#endif
		}
		return item;
	}

	
	void GCLargeAlloc::Free(void *item)
	{
		LargeBlock *b = GetBlockHeader(item);

		if(b->flags & kHasWeakRef)
			b->gc->ClearWeakRef(GetUserPointer(item));

		LargeBlock **prev = &m_blocks;
		while(*prev)
		{
			if(b == *prev)
			{
				*prev = b->next;
				m_gc->FreeBlock(b, b->GetNumBlocks());
				return;
			}
			prev = &(*prev)->next;
		}
		GCAssertMsg(false, "Bad free!");
	}

	void GCLargeAlloc::ClearMarks()
	{
		LargeBlock *block = m_blocks;
		while (block) {
			block->flags &= ~(kMarkFlag|kQueuedFlag);
			block = block->next;
		}
	}

	void GCLargeAlloc::Finalize()
	{
		m_startedFinalize = true;
		LargeBlock **prev = &m_blocks;
		while (*prev) {			
			LargeBlock *b = *prev;
			if ((b->flags & kMarkFlag) == 0) {
				void *item = b+1;
				if (NeedsFinalize(b)) {
					GCFinalizedObject *obj = (GCFinalizedObject *) item;
					obj = (GCFinalizedObject *) GetUserPointer(obj);
					obj->~GCFinalizedObject();
#if defined(_DEBUG) && defined(MMGC_DRC)
					if((b->flags & kRCObject) != 0) {
						b->gc->RCObjectZeroCheck((RCObject*)obj);
					}
#endif
				}
				if(b->flags & kHasWeakRef) {
					b->gc->ClearWeakRef(GetUserPointer(item));
				}
				// unlink from list
				*prev = b->next;
				b->gc->AddToLargeEmptyBlockList(b);
				continue;
			}
			// clear marks
			b->flags &= ~(kMarkFlag|kQueuedFlag);
			prev = &b->next;
		}
		m_startedFinalize = false;
	}

	GCLargeAlloc::~GCLargeAlloc()
	{
		GCAssert(!m_blocks);
	}

	/* static */
	bool GCLargeAlloc::ConservativeGetMark(const void *item, bool bogusPointerReturnValue)
	{
		if(((intptr) item & 0xfff) == sizeof(LargeBlock))
		{
			return GetMark(item);
		}
		return bogusPointerReturnValue;
	}
}
