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

#ifndef __GCLargeAlloc__
#define __GCLargeAlloc__

namespace MMgc
{
	/**
	 * This is a garbage collecting allocator for large memory blocks.
	 */
	class GCLargeAlloc : public GCAllocObject
	{
		friend class GC;
	private:
		enum {
			kMarkFlag         = 0x1,
			kQueuedFlag       = 0x2,
			kFinalizeFlag     = 0x4,
			kHasWeakRef       = 0x8,
			kContainsPointers = 0x10,
			kRCObject         = 0x20
		};

	public:
		GCLargeAlloc(GC* gc);
		~GCLargeAlloc();

		void* Alloc(size_t size, int flags);
		void Free(void *ptr);
		void Finalize();
		void ClearMarks();

		static void SetHasWeakRef(const void *item, bool to)
		{
			if(to) {
				GetBlockHeader(item)->flags |= kHasWeakRef;
			} else {
				GetBlockHeader(item)->flags &= ~kHasWeakRef;
			}
		}

		static bool HasWeakRef(const void *item)
		{
			return (GetBlockHeader(item)->flags & kHasWeakRef) != 0;
		}

		static bool IsLargeBlock(const void *item)
		{
			// The pointer should be 4K aligned plus 16 bytes
		        // Mac inserts 16 bytes for new[] so make it more general
			return (((intptr)item & 0xFFF) == sizeof(LargeBlock));
		}

		static bool SetMark(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			bool oldMark = (block->flags & kMarkFlag) != 0;
			block->flags |= kMarkFlag;
			block->flags &= ~kQueuedFlag;
			return oldMark;
		}

		static void SetQueued(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			block->flags |= kQueuedFlag;
		}

		static void SetFinalize(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			block->flags |= kFinalizeFlag;
		}
		
		static bool GetMark(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			return (block->flags & kMarkFlag) != 0;
		}

		static bool IsWhite(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			if(!IsLargeBlock(item))
				return false;
			return (block->flags & (kMarkFlag|kQueuedFlag)) == 0;
		}

		static void* FindBeginning(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			return (void*) (block+1);
		}

		static void ClearFinalized(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			block->flags &= ~kFinalizeFlag;
		}

		static bool ContainsPointers(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			return (block->flags & kContainsPointers) != 0;
		}
		
		static bool IsFinalized(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			return (block->flags & kFinalizeFlag) != 0;
		}

		static bool IsRCObject(const void *item)
		{
			LargeBlock *block = GetBlockHeader(item);
			return (block->flags & kRCObject) != 0;
		}

	private:
		struct LargeBlock
		{
			GC *gc;
			uint32 usableSize;
			uint32 flags;
			LargeBlock *next;

			int GetNumBlocks() const { return (usableSize + sizeof(LargeBlock)) / GCHeap::kBlockSize; }
		};

		static LargeBlock* GetBlockHeader(const void *addr)
		{
			return (LargeBlock*) ((intptr)addr & ~0xFFF);
		}
		
		static bool NeedsFinalize(LargeBlock *block)
		{
			return (block->flags & kFinalizeFlag) != 0;
		}			
		
		// The list of chunk blocks
		LargeBlock* m_blocks;
		bool m_startedFinalize;
		static bool ConservativeGetMark(const void *item, bool bogusPointerReturnValue);

	protected:
		GC *m_gc;
	
	};
}

#endif /* __GCLargeAlloc__ */
