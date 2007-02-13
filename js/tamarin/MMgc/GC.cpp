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


// For memset
#include <string.h>

#include "MMgc.h"

#if defined(_DEBUG) || defined(DEBUGGER)
#include <stdio.h>
#endif

#ifdef _DEBUG
#include "GCTests.h"
#endif

#ifdef DARWIN
#include <Carbon/Carbon.h>
#endif

// get alloca for CleanStack
#ifdef WIN32
#include "malloc.h"
#else
#include "alloca.h"
#endif

#if defined(_MAC) && (defined(MMGC_IA32) || defined(MMGC_AMD64))
#include <pthread.h>
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
// we turn on exceptions in DEBUG builds
#pragma warning(disable:4291) // no matching operator delete found; memory will not be freed if initialization throws an exception
#endif

#ifdef DEBUGGER
// sampling support
#include "avmplus.h"
#else
#define SAMPLE_FRAME(_x, _s) 
#define SAMPLE_CHECK() 
#endif

namespace MMgc
{

#ifdef MMGC_DRC

	// how many objects trigger a reap, should be high
	// enough that the stack scan time is noise but low enough
	// so that objects to away in a timely manner
	const int ZCT::ZCT_REAP_THRESHOLD = 512;

	// size of table in pages
	const int ZCT::ZCT_START_SIZE = 1;
#endif

#ifdef MMGC_64BIT
	const uintptr MAX_UINTPTR = 0xFFFFFFFFFFFFFFFF;
#else
	const uintptr MAX_UINTPTR = 0xFFFFFFFF;
#endif	

	// get detailed info on each size class allocators
	const bool dumpSizeClassState = false;
	
	/**
	 * Free Space Divisor.  This value may be tuned for optimum
 	 * performance.  The FSD is based on the Boehm collector.
	 */
	const static int kFreeSpaceDivisor = 4;

	const static int kMaxIncrement = 4096;

	/**
	 * delay between GC incremental marks
	 */
	const static uint64 kIncrementalMarkDelayTicks = int(10 * GC::GetPerformanceFrequency() / 1000);

	const static uint64 kMarkSweepBurstTicks = 1515909; // 200 ms on a 2ghz machine

	// Size classes for our GC.  From 8 to 128, size classes are spaced
	// evenly 8 bytes apart.  The finest granularity we can achieve is
	// 8 bytes, as pointers must be 8-byte aligned thanks to our use
	// of the bottom 3 bits of 32-bit atoms for Special Purposes.
	// Above that, the size classes have been chosen to maximize the
	// number of items that fit in a 4096-byte block, given our block
	// header information.
	const int16 GC::kSizeClasses[kNumSizeClasses] = {
		8, 16, 24, 32, 40, 48, 56, 64, 72, 80, //0-9
		88, 96, 104, 112, 120, 128,	144, 160, 168, 176,  //10-19
		184, 192, 200, 216, 224, 240, 256, 280, 296, 328, //20-29
		352, 392, 432, 488, 560, 656, 784, 984, 1312, 1968 //30-39
	};

	/* kSizeClassIndex[] generated with this code:
	    kSizeClassIndex[0] = 0;
	    for (var i:int = 1; i < kNumSizeClasses; i++)
			for (var j:int = (kSizeClasses[i-1]>>3), n=(kSizeClasses[i]>>3); j < n; j++)
				kSizeClassIndex[j] = i;
	*/

	// index'd by size>>3 - 1
	const uint8 GC::kSizeClassIndex[246] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		10, 11, 12, 13, 14, 15, 16, 16, 17, 17,
		18, 19, 20, 21, 22, 23, 23, 24, 25, 25,
		26, 26, 27, 27, 27, 28, 28, 29, 29, 29,
		29, 30, 30, 30, 31, 31, 31, 31, 31, 32,
		32, 32, 32, 32, 33, 33, 33, 33, 33, 33,
		33, 34, 34, 34, 34, 34, 34, 34, 34, 34,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
		35, 35, 36, 36, 36, 36, 36, 36, 36, 36,
		36, 36, 36, 36, 36, 36, 36, 36, 37, 37,
		37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
		37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
		37, 37, 37, 38, 38, 38, 38, 38, 38, 38,
		38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
		38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
		38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
		38, 38, 38, 38, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 39, 39	
	};
	const size_t kLargestAlloc = 1968;

	GC::GC(GCHeap *gcheap)
		: disableThreadCheck(false),
#ifdef MMGC_DRC
		  zct(gcheap),
#endif
		  nogc(false),
		  greedy(false),
		  findUnmarkedPointers(false),
		  validateDefRef(false),
		  keepDRCHistory(false),
		  gcstats(false),
#ifdef WRITE_BARRIERS
		  incremental(true),
#else
		  incremental(false),
#endif

#ifdef _DEBUG
		  // this doens't hurt performance too much so alway leave it on in DEBUG builds
		  // before sweeping we check for missing write barriers
		  incrementalValidation(false),

		  // check for missing write barriers at every Alloc
		  incrementalValidationPedantic(false),
#endif

		  marking(false),
		  memStart(MAX_UINTPTR),
		  memEnd(0),
		  heap(gcheap),
		  allocsSinceCollect(0),
		  collecting(false),
		  m_roots(0),
		  m_callbacks(0),
		  markTicks(0),
		  bytesMarked(0),
		  markIncrements(0),
		  marks(0),
		  sweeps(0),
		  totalGCPages(0),
		  stackCleaned(true),
		  rememberedStackTop(0),
		  destroying(false),
		  lastMarkTicks(0),
		  lastSweepTicks(0),
		  lastStartMarkIncrementCount(0),
		  t0(GetPerformanceCounter()),
		  dontAddToZCTDuringCollection(false),
		  numObjects(0),
		  hitZeroObjects(false),
		  smallEmptyPageList(NULL),
		  largeEmptyPageList(NULL),
		  sweepStart(0),
		  heapSizeAtLastAlloc(gcheap->GetTotalHeapSize()),
		  finalizedValue(true),
		  // Expand, don't collect, until we hit this threshold
		  collectThreshold(256)
	{		
		// sanity check for all our types
		GCAssert (sizeof(int8) == 1);
		GCAssert (sizeof(uint8) == 1);		
		GCAssert (sizeof(int16) == 2);
		GCAssert (sizeof(uint16) == 2);
		GCAssert (sizeof(int32) == 4);
		GCAssert (sizeof(uint32) == 4);
		GCAssert (sizeof(int64) == 8);
		GCAssert (sizeof(uint64) == 8);
		GCAssert (sizeof(sintptr) == sizeof(void *));
		GCAssert (sizeof(uintptr) == sizeof(void *));
		#ifdef MMGC_64BIT
		GCAssert (sizeof(sintptr) == 8);
		GCAssert (sizeof(uintptr) == 8);	
		#else	
		GCAssert (sizeof(sintptr) == 4);
		GCAssert (sizeof(uintptr) == 4);	
		#endif		
	
#ifdef MMGC_DRC
		zct.gc = this;
#endif
		// Create all the allocators up front (not lazy)
		// so that we don't have to check the pointers for
		// NULL on every allocation.
		for (int i=0; i<kNumSizeClasses; i++) {			
			containsPointersAllocs[i] = new GCAlloc(this, kSizeClasses[i], true, false, i);
#ifdef MMGC_DRC
			containsPointersRCAllocs[i] = new GCAlloc(this, kSizeClasses[i], true, true, i);
#endif
			noPointersAllocs[i] = new GCAlloc(this, kSizeClasses[i], false, false, i);
		}
		
		largeAlloc = new GCLargeAlloc(this);

		pageMap = (unsigned char*) heap->Alloc(1);

		memset(m_bitsFreelists, 0, sizeof(uint32*) * kNumSizeClasses);
		m_bitsNext = (uint32*)heap->Alloc(1);

		// precondition for emptyPageList 
		GCAssert(offsetof(GCLargeAlloc::LargeBlock, next) == offsetof(GCAlloc::GCBlock, next));

		
		for(int i=0; i<GCV_COUNT; i++)
		{
			SetGCContextVariable(i, NULL);
		}

#ifdef _DEBUG
		msgBuf = new char[32];
#ifdef WIN32
		m_gcThread = GetCurrentThreadId();
#endif
#endif

		// keep GC::Size honest
		GCAssert(offsetof(GCLargeAlloc::LargeBlock, usableSize) == offsetof(GCAlloc::GCBlock, size));

#ifndef MMGC_ARM // TODO MMGC_ARM
#ifdef _DEBUG
		RunGCTests(this);
#endif
#endif

		// keep GC::Size honest
		GCAssert(offsetof(GCLargeAlloc::LargeBlock, usableSize) == offsetof(GCAlloc::GCBlock, size));

	}

#ifdef _DEBUG
	const char *GC::msg()
	{
		sprintf(msgBuf, "[MEM %8dms]", (int)ticksToMillis(GetPerformanceCounter() - t0));
		return msgBuf;
	}
#endif
	
	GC::~GC()
	{
		// Force all objects to be destroyed
		destroying = true;
		ClearMarks();
		ForceSweep();

		// Go through m_bitsFreelist and collect list of all pointers
		// that are on page boundaries into new list, pageList
		void **pageList = NULL;
		for(int i=0, n=kNumSizeClasses; i<n; i++) {
			uint32* bitsFreelist = m_bitsFreelists[i];
			while(bitsFreelist) {
				uint32 *next = *(uint32**)bitsFreelist;
				if(((uintptr)bitsFreelist & 0xfff) == 0) {
					*((void**)bitsFreelist) = pageList;
					pageList = (void**)bitsFreelist;
				}
				bitsFreelist = next;
			} 
		}
		
		// Go through page list and free all pages on it
		while (pageList) {
			void **next = (void**) *pageList;
			heap->Free((void*)pageList);
			pageList = next;
		}

		for (int i=0; i < kNumSizeClasses; i++) {
			delete containsPointersAllocs[i];
#ifdef MMGC_DRC
			delete containsPointersRCAllocs[i];
#endif
			delete noPointersAllocs[i];
		}

		if (largeAlloc) {
			delete largeAlloc;
		}

		// dtors for each GCAlloc will use this
		pageList = NULL;

		heap->Free(pageMap);

#ifdef _DEBUG
		delete [] msgBuf;
#endif

		CheckThread();

		GCAssert(!m_roots);
		GCAssert(!m_callbacks);
	}

	void GC::Collect()
	{
		if (nogc || collecting) {

			return;
		}

		// Don't start a collection if we are in the midst of a ZCT reap.
		if (zct.reaping)
		{

			return;
		}

		ReapZCT();

		// if we're in the middle of an incremental mark kill it
		// FIXME: we should just push it to completion 
		if(marking) 
		{
			marking = false;
			m_incrementalWork.Keep(0);
		}

#ifdef DEBUGGER
		StartGCActivity();
#endif

		// Dumping the stack trace at GC time can be very helpful with stack walk bugs
		// where something was created, stored away in untraced, unmanaged memory and not 
		// reachable by the conservative stack walk
		//DumpStackTrace();

		Trace();
		
		Sweep();

#ifdef _DEBUG
		FindUnmarkedPointers();
#endif

		CheckThread();

#ifdef DEBUGGER
		StopGCActivity();
#endif
	}

	void GC::Trace(const void *stackStart/*=NULL*/, size_t stackSize/*=0*/)
	{
		SAMPLE_FRAME("[mark]", core());

		// Clear all mark bits.
		ClearMarks();

		SAMPLE_CHECK();

		GCStack<GCWorkItem> work;
		{
#ifdef GCHEAP_LOCK
			GCAcquireSpinlock lock(m_rootListLock);
#endif
			GCRoot *r = m_roots;
			while(r) {
				GCWorkItem item = r->GetWorkItem();
				MarkItem(item, work);
				r = r->next;
			}
		}

		SAMPLE_CHECK();

		if(stackStart == NULL) {
			MarkQueueAndStack(work);
		} else {
			GCWorkItem item(stackStart, stackSize, false);
			PushWorkItem(work, item);
			Mark(work);
		}
		
		SAMPLE_CHECK();
	}

	void *GC::Alloc(size_t size, int flags/*0*/, int skip/*3*/)
	{
#ifdef DEBUGGER
		avmplus::AvmCore *core = (avmplus::AvmCore*)GetGCContextVariable(GCV_AVMCORE);
		if(core)
			core->sampleCheck();
#endif
		(void)skip;
		(void)skip;
		GCAssertMsg(size > 0, "cannot allocate a 0 sized block\n");

#ifdef _DEBUG
		GCAssert(size + 7 > size);
		CheckThread();
		if (GC::greedy) {
			Collect();
		}
		// always be marking in pedantic mode
		if(incrementalValidationPedantic) {
			if(!marking)
				StartIncrementalMark();
		}
#endif

		// overflow detection
		if(size+7 < size)
			return NULL;

		size = (size+7)&~7; // round up to multiple of 8

		size += DebugSize();

		GCAssertMsg(size > 0, "debug overflow, adding space for Debug stuff overflowed size_t\n");
	
		GCAlloc **allocs = noPointersAllocs;
#ifdef MMGC_DRC
		if(flags & kRCObject) {
			allocs = containsPointersRCAllocs;
		} else 
#endif
		if(flags & kContainsPointers) {
			allocs = containsPointersAllocs;
		}

		void *item;
		// Buckets up to 128 are spaced evenly at 8 bytes.
		if (size <= 128) {
			GCAlloc *b = (GCAlloc*)allocs[(size >> 3) - 1];
			GCAssert(size <= b->GetItemSize());
			item = b->Alloc(size, flags);
		} else if (size <= kLargestAlloc) {				
			// This is the fast lookup table implementation to
			// find the right allocator.
			unsigned index = kSizeClassIndex[(size>>3)-1];

			// assert that I fit 
			GCAssert(size <= allocs[index]->GetItemSize());

			// assert that I don't fit (makes sure we don't waste space)
			GCAssert(size > allocs[index-1]->GetItemSize());

			item = allocs[index]->Alloc(size, flags);
		} else {
			item = largeAlloc->Alloc(size, flags);
		}

#ifdef MEMORY_INFO
		item = DebugDecorate(item,  GC::Size(GetUserPointer(item)) + DebugSize(), skip);
#endif

#ifdef _DEBUG
		// in debug mode memory is poisoned so we have to clear it here
		// in release builds memory is zero'd to start and on free/sweep
		// in pedantic mode uninitialized data can trip the write barrier 
		// detector, only do it for pedantic because otherwise we want the
		// mutator to get the poisoned data so it crashes if it relies on 
		// uninitialized values
		if(item && (flags&kZero || incrementalValidationPedantic)) {
			memset(item, 0, Size(item));
		}
#endif

		return item;
	}

	void *GC::Calloc(size_t num, size_t elsize, int flags, int skip)
	{
		uint64 size = (uint64)num * (uint64)elsize;
		if(size > 0xfffffff0) 
		{
			GCAssertMsg(false, "Attempted allocation overflows size_t\n");
			return NULL;
		}
		return Alloc(num * elsize, flags, skip);
	}

	void GC::Free(void *item)
	{
		CheckThread();
		// we can't allow free'ing something during Sweeping, otherwise alloc counters
		// get decremented twice and destructors will be called twice.
		if(item == NULL) {
			return;
		}

		bool isLarge;

		if(collecting) {
			goto bail;
		}

		isLarge = GCLargeAlloc::IsLargeBlock(GetRealPointer(item));

		if (marking) {
			// if its on the work queue don't delete it, if this item is
			// really garbage we're just leaving it for the next sweep
			if(IsQueued(item)) 
				goto bail;
		}

#ifdef _DEBUG
#ifdef MMGC_DRC
		// RCObject have constract that they must clean themselves, since they 
		// have to scan themselves to decrement other RCObjects they might as well
		// clean themselves too, better than suffering a memset later
		if(isLarge ? GCLargeAlloc::IsRCObject(item) : GCAlloc::IsRCObject(item))
		{
			 RCObjectZeroCheck((RCObject*)item);
		}
#endif // MMGC_DRC
#endif

#ifdef MEMORY_INFO
		DebugFree(item, 0xca, 4);
#endif
	
		if (isLarge) {
			largeAlloc->Free(GetRealPointer(item));
		} else {
			GCAlloc::Free(GetRealPointer(item));
		}
		return;

bail:

		// this presumes we got here via delete, maybe we should have
		// delete call something other than the public Free to distinguish
		if(IsFinalized(item))
			ClearFinalized(item);
		if(HasWeakRef(item))
			ClearWeakRef(item);
	}

	void GC::ClearMarks()
	{
		for (int i=0; i < kNumSizeClasses; i++) {
#ifdef MMGC_DRC
			containsPointersRCAllocs[i]->ClearMarks();
#endif
			containsPointersAllocs[i]->ClearMarks();
			noPointersAllocs[i]->ClearMarks();
		}
		largeAlloc->ClearMarks();
	}

	void GC::Finalize()
	{
		for(int i=0; i < kNumSizeClasses; i++) {
#ifdef MMGC_DRC
			containsPointersRCAllocs[i]->Finalize();
#endif
			containsPointersAllocs[i]->Finalize();
			noPointersAllocs[i]->Finalize();
		}
		largeAlloc->Finalize();
		finalizedValue = !finalizedValue;

		
		for(int i=0; i < kNumSizeClasses; i++) {
#ifdef MMGC_DRC
			containsPointersRCAllocs[i]->m_finalized = false;
#endif
			containsPointersAllocs[i]->m_finalized = false;
			noPointersAllocs[i]->m_finalized = false;
		}
	}

	void GC::Sweep(bool force)
	{	
		SAMPLE_FRAME("[sweep]", core());
		sweeps++;

		int heapSize = heap->GetUsedHeapSize();

#ifdef MEMORY_INFO
		if(heap->enableMemoryProfiling) {
			GCDebugMsg(false, "Pre sweep memory info:\n");
			DumpMemoryInfo();
		}
#endif
		
		// collecting must be true because it indicates allocations should
		// start out marked, we can't rely on write barriers below since 
		// presweep could write a new GC object to a root
		collecting = true;

		// invoke presweep on all callbacks
		GCCallback *cb = m_callbacks;
		while(cb) {
			cb->presweep();
			cb = cb->nextCB;
		}

		SAMPLE_CHECK();

		Finalize();
		
		SAMPLE_CHECK();
		
		// if force is true we're being called from ~GC and this isn't necessary
		if(!force) {
			// we just executed mutator code which could have fired some WB's
			Mark(m_incrementalWork);
		}

		// ISSUE: this could be done lazily at the expense other GC's potentially expanding
		// unnecessarily, not sure its worth it as this should be pretty fast
		GCAlloc::GCBlock *b = smallEmptyPageList;
		while(b) {
			GCAlloc::GCBlock *next = b->next;
#ifdef _DEBUG
			b->alloc->SweepGuts(b);
#endif
			b->alloc->FreeChunk(b);
			b = next;
		}
		smallEmptyPageList = NULL;

		SAMPLE_CHECK();

		GCLargeAlloc::LargeBlock *lb = largeEmptyPageList;		
		while(lb) {
			GCLargeAlloc::LargeBlock *next = lb->next;
			// FIXME: this makes for some chatty locking, maybe not a problem?
			FreeBlock(lb, lb->GetNumBlocks());
			lb = next;
		}
		largeEmptyPageList = NULL;

		SAMPLE_CHECK();

#ifdef MEMORY_INFO
		if(heap->enableMemoryProfiling) {			
			GCDebugMsg(false, "Post sweep memory info:\n");
			DumpMemoryInfo();
		}
#endif

		// don't want postsweep to fire WB's
		marking = false;
		collecting = false;

		// invoke postsweep callback
		cb = m_callbacks;
		while(cb) {
			cb->postsweep();
			cb = cb->nextCB;
		}
		
		SAMPLE_CHECK();

		allocsSinceCollect = 0;
		lastSweepTicks = GetPerformanceCounter();

		if(GC::gcstats) {
			int sweepResults = 0;
			GCAlloc::GCBlock *bb = smallEmptyPageList;
			while(bb) {
				sweepResults++;
				bb = bb->next;
			}
			
			GCLargeAlloc::LargeBlock *lbb = largeEmptyPageList;
			while(lbb) {
				sweepResults += lbb->GetNumBlocks();
				lbb = lbb->next;
			}
			// include large pages given back
			sweepResults += (heapSize - heap->GetUsedHeapSize());
			double millis = duration(sweepStart);
			gclog("[GC] sweep(%d) reclaimed %d whole pages (%d kb) in %4Lf millis (%4Lf s)\n", 
				sweeps, sweepResults, sweepResults*GCHeap::kBlockSize>>10, millis,
				duration(t0)/1000);\
		}
#ifdef DEBUGGER
		StopGCActivity();
#endif

#ifdef MEMORY_INFO
		m_gcLastStackTrace = GetStackTraceIndex(5);
#endif
	}

	void* GC::AllocBlock(int size, int pageType, bool zero)
	{
#ifdef DEBUGGER
		AllocActivity(size);
#endif
		GCAssert(size > 0);
	
		// perform gc if heap expanded due to fixed memory allocations
		// utilize burst logic to prevent this from happening back to back
		// this logic is here to apply to incremental and non-incremental
		uint64 now = GetPerformanceCounter();
		if(!marking && !collecting &&
			heapSizeAtLastAlloc > collectThreshold &&
			now - lastSweepTicks > kMarkSweepBurstTicks && 
			heapSizeAtLastAlloc < heap->GetTotalHeapSize()) 
		{
			if(incremental)
				StartIncrementalMark();
			else
				Collect();
		}

		void *item;

		if(incremental)
			item = AllocBlockIncremental(size, zero);
		else
			item = AllocBlockNonIncremental(size, zero);

		if(!item) {				
			int incr = size + totalGCPages / kFreeSpaceDivisor;
			if(incr > kMaxIncrement) {
				incr = size < kMaxIncrement ? kMaxIncrement : size;
			}
			heap->ExpandHeap(incr);
			item = heap->Alloc(size, false);
		}

		GCAssert(item != NULL);
		if (item != NULL)
		{
			allocsSinceCollect += size;

			// mark GC pages in page map, small pages get marked one,
			// the first page of large pages is 3 and the rest are 2
			MarkGCPages(item, 1, pageType);
			if(pageType == kGCLargeAllocPageFirst) {
				MarkGCPages((char*)item+GCHeap::kBlockSize, size - 1, kGCLargeAllocPageRest);
			}
			totalGCPages += size;
		}

		// do this after any heap expansions from GC
		heapSizeAtLastAlloc = heap->GetTotalHeapSize();

		return item;
	}

	void* GC::AllocBlockIncremental(int size, bool zero)
	{
		if(!nogc && !collecting) {
			uint64 now = GetPerformanceCounter();
			if (marking) {		
				if(now - lastMarkTicks > kIncrementalMarkDelayTicks) {
					IncrementalMark();
				}
			} else if (totalGCPages > collectThreshold &&
				allocsSinceCollect * kFreeSpaceDivisor >= totalGCPages) {
				// burst detection
				if(now - lastSweepTicks > kMarkSweepBurstTicks)
					StartIncrementalMark();
			}
		}

		void *item = heap->Alloc(size, false, zero);
		if(!item && !collecting) {
			if(marking) {
				GCAssert(!nogc);
				FinishIncrementalMark();
				item = heap->Alloc(size, false, zero);
			}
		}
		return item;
	}

	void* GC::AllocBlockNonIncremental(int size, bool zero)
	{
		void *item = heap->Alloc(size, false, zero);
		if (!item) {
			if (heap->GetTotalHeapSize() >= collectThreshold &&
				allocsSinceCollect >= totalGCPages / kFreeSpaceDivisor) 
			{
				Collect();
				item = heap->Alloc(size, false, zero);
			}
		}
		return item;
	}

	void GC::FreeBlock(void *ptr, uint32 size)
	{
#ifdef DEBUGGER
		AllocActivity(- (int)size);
#endif
		if(!collecting) {
			allocsSinceCollect -= size;
		}
		totalGCPages -= size;
		heap->Free(ptr);
		UnmarkGCPages(ptr, size);
	}

	void GC::SetPageMapValue(uintptr addr, int val)
	{
		uintptr index = (addr-memStart) >> 12;
		GCAssert(index >> 2 < 64 * GCHeap::kBlockSize);
		pageMap[index >> 2] |= (val<<((index&0x3)*2));
	}	

	void GC::ClearPageMapValue(uintptr addr)
	{
		uintptr index = (addr-memStart) >> 12;
		GCAssert(index >> 2 < 64 * GCHeap::kBlockSize);
		pageMap[index >> 2] &= ~(3<<((index&0x3)*2));
	}	

	void GC::Mark(GCStack<GCWorkItem> &work)
	{
		while(work.Count()) {
			MarkItem(work);
		}
	}

	void GC::MarkGCPages(void *item, uint32 numPages, int to)
	{
		uintptr addr = (uintptr)item;
		uint32 shiftAmount=0;
		unsigned char *dst = pageMap;

		// save the current live range in case we need to move/copy
		uint32 numBytesToCopy = (memEnd-memStart)>>14;

		if(addr < memStart) {
			// round down to nearest 16k boundary, makes shift logic work cause it works
			// in bytes, ie 16k chunks
			addr &= ~0x3fff;
			// marking earlier pages
			if(memStart != MAX_UINTPTR) {
				shiftAmount = (memStart - addr) >> 14;
			}
			memStart = addr;
		} 
		
		if(addr + (numPages+1)*GCHeap::kBlockSize > memEnd) {
			// marking later pages
			memEnd = addr + (numPages+1)*GCHeap::kBlockSize;
			// round up to 16k 
			memEnd = (memEnd+0x3fff)&~0x3fff;
		}

        size_t numPagesNeeded = ((memEnd-memStart)>>14)/GCHeap::kBlockSize + 1;
		if(numPagesNeeded > heap->Size(pageMap)) {
			dst = (unsigned char*)heap->Alloc(numPagesNeeded);
		}

		if(shiftAmount || dst != pageMap) {
			memmove(dst + shiftAmount, pageMap, numBytesToCopy);
			memset(dst, 0, shiftAmount);
			if(dst != pageMap) {
				heap->Free(pageMap);
				pageMap = dst;
			}
		}

		addr = (uintptr)item;
		while(numPages--)
		{
			GCAssert(GetPageMapValue(addr) == 0);
			SetPageMapValue(addr, to);
			addr += GCHeap::kBlockSize;
		}
	}

	void GC::UnmarkGCPages(void *item, uint32 numpages)
	{
		uintptr addr = (uintptr) item;
		while(numpages--)
		{
			ClearPageMapValue(addr);
			addr += GCHeap::kBlockSize;
		}
	}

	void GC::CleanStack(bool force)
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		// debug builds poison the stack already
		(void)force;
		return;
#else
		if(!force && (stackCleaned || rememberedStackTop == 0))
			return;

		stackCleaned = true;
		
		register void *stackP;
#if defined MMGC_IA32
		#ifdef WIN32
		__asm {
			mov stackP,esp
		}
		#else
		asm("movl %%esp,%0" : "=r" (stackP));
		#endif
#endif

#if defined MMGC_AMD64
	asm("mov %%rsp,%0" : "=r" (stackP));
#endif

#if defined MMGC_PPC
		// save off sp
		asm("mr %0,r1" : "=r" (stackP));
#endif	
		if((char*) stackP > (char*)rememberedStackTop) {
			size_t amount = (char*) stackP - (char*)rememberedStackTop;
			void *stack = alloca(amount);
			if(stack) {
				memset(stack, 0, amount);
			}
		}
#endif
	}
	
	#if defined(MMGC_PPC) && defined(__GNUC__)
	__attribute__((noinline)) 
	#endif
	void GC::MarkQueueAndStack(GCStack<GCWorkItem>& work)
	{
		GCWorkItem item;

		MMGC_GET_STACK_EXENTS(this, item.ptr, item._size);

		// this is where we will clear to when CleanStack is called
		if(rememberedStackTop == 0 || rememberedStackTop > item.ptr) {
			rememberedStackTop = item.ptr;
		}

		PushWorkItem(work, item);
		Mark(work);
	}

	GCRoot::GCRoot(GC * _gc) : gc(_gc)
	{
		// I think this makes some non-guaranteed assumptions about object layout,
		// like with multiple inheritance this might not be the beginning
		object = this;
		size = FixedMalloc::GetInstance()->Size(this);
		gc->AddRoot(this);
	}

	GCRoot::GCRoot(GC * _gc, const void * _object, size_t _size)
		: gc(_gc), object(_object), size(_size)
	{
		gc->AddRoot(this);
	}

	GCRoot::~GCRoot()
	{
		if(gc) {
			gc->RemoveRoot(this);
		}
	}

	void GCRoot::Set(const void * _object, size_t _size)
	{
		this->object = _object;
		this->size = _size;
	}

	void GCRoot::Destroy()
	{
		Set(NULL, 0);
		if(gc) {
			gc->RemoveRoot(this);
		}
		gc = NULL;
	}

	GCCallback::GCCallback(GC * _gc) : gc(_gc)
	{
		gc->AddCallback(this);
	}

	GCCallback::~GCCallback()
	{
		if(gc) {
			gc->RemoveCallback(this);
		}
	}

	void GCCallback::Destroy()
	{
		if(gc) {
			gc->RemoveCallback(this);
		}
		gc = NULL;
	}

	void GC::CheckThread()
	{
#ifdef _DEBUG
#ifdef WIN32
		GCAssertMsg(disableThreadCheck || m_gcThread == GetCurrentThreadId(), "Unsafe access to GC from wrong thread!");
#endif
#endif
	}


	bool GC::IsPointerToGCPage(const void *item)
	{
		if((uintptr)item >= memStart && (uintptr)item < memEnd)
			return GetPageMapValue((uintptr) item) != 0;
		return false;
	}

#ifdef MMGC_DRC

	ZCT::ZCT(GCHeap *heap)
	{
		zctSize = ZCT_START_SIZE;
		zct = (RCObject**) heap->Alloc(zctSize);
		zctNext = zct;
		zctFreelist = NULL;
		reaping = false;
		gc = NULL;
		count = 0;
		zctReapThreshold = ZCT_REAP_THRESHOLD;
	}

	ZCT::~ZCT()
	{
		gc->heap->Free(zct);
	}

	void ZCT::Add(RCObject *obj)
	{
		if(gc->collecting)
		{
			// this is a vestige from FP8 to fix bug 165100, it has the affect of delaying 
			// the deletion of some objects which causes the site to work
			if(gc->dontAddToZCTDuringCollection)
				return;

			// unmarked objects are gonna get swept anyways
			if(!GC::GetMark(obj))
				return;
		}

#if 0
		// note if we add things while reaping we could delete the object
		// here if we had a way to monitor our stack usage
		if(reaping && PLENTY_OF_STACK()) {
			GCCallback *cb = gc->m_callbacks;
			while(cb) {
				cb->prereap(obj);
				cb = cb->nextCB;
			}
			if(gc->IsFinalized(obj))
				((GCFinalizedObject*)obj)->~GCFinalizedObject();
			gc->Free(obj);
			return;
		}
#endif

		if(zctFreelist) {
			RCObject **nextFree = (RCObject**)*zctFreelist;
			*zctFreelist = obj;
			obj->setZCTIndex(zctFreelist - zct);
			zctFreelist = nextFree;			
		} else if(reaping && zctIndex > nextPinnedIndex) {
			// we're going over the list, insert this guy at the front if possible
			zctIndex--;
			GCAssert(zct[zctIndex] == NULL);
			obj->setZCTIndex(zctIndex);
			zct[zctIndex] = obj;
		} else {
			*zctNext = obj;
			obj->setZCTIndex(zctNext - zct);
			zctNext++;
		}
		count++;

		if(!reaping) {
			// object that were pinned last reap should be unpinned
			if(obj->IsPinned())
				obj->Unpin();
			if(!gc->collecting && zctNext >= zct+zctReapThreshold)
				Reap();
		}

		if(zctNext >= zct + zctSize*4096/sizeof(void *)) {
			// grow 
			RCObject **newZCT = (RCObject**) gc->heap->Alloc(zctSize*2);
			memcpy(newZCT, zct, zctSize*GCHeap::kBlockSize);
			gc->heap->Free(zct);
			zctNext = newZCT + (zctNext-zct);
			zct = newZCT;	
			zctSize *= 2;
			GCAssert(!zctFreelist);
		}
	}

	void ZCT::Remove(RCObject *obj)
	{
		int index = obj->getZCTIndex();
		GCAssert(zct[index] == obj);

		if(reaping)
		{
			// freelist doesn't exist during reaping, simplifies things, holes will
			// be compacted next go around if index < nextPinnedIndex.  the freelist
			// allows incoming object to be added behind zctIndex which would mean
			// the reap process wouldn't cascade like its supposed to. 
			zct[index] = NULL;
		}
		else
		{
			zct[index] = (RCObject*) zctFreelist;
			zctFreelist = &zct[index];
		}
		obj->ClearZCTFlag();
		count--;
	}

	void ZCT::PinStackObjects(const void *start, size_t len)
	{
		RCObject **p = (RCObject**)start;
		RCObject **end = p + len/sizeof(RCObject*);

		const void *_memStart = (const void*)gc->memStart;
		const void *_memEnd = (const void*)gc->memEnd;

		while(p < end)
		{
			const void *val = GC::Pointer(*p++);	
			
			if(val < _memStart || val >= _memEnd)
				continue;

			int bits = gc->GetPageMapValue((uintptr)val); 
			bool doit = false;
			if (bits == GC::kGCAllocPage) {
				doit = GCAlloc::IsRCObject(val) && GCAlloc::FindBeginning(val) == GetRealPointer(val);
			} else if(bits == GC::kGCLargeAllocPageFirst) {
				doit = GCLargeAlloc::IsRCObject(val) && GCLargeAlloc::FindBeginning(val) == GetRealPointer(val);
			}

			if(doit) {
				RCObject *obj = (RCObject*)val;
				obj->Pin();
			}
		}
	}
			
#ifdef _DEBUG

	void GC::RCObjectZeroCheck(RCObject *item)
	{
		int size = Size(item)/sizeof(int);
		int *p = (int*)item;
		// skip vtable, first 4 bytes are cleared in Alloc
		p++;
#ifdef MMGC_64BIT
		p++; // vtable is 8-bytes
		size--; 
#endif		
		// in incrementalValidation mode manually deleted items
		// aren't put on the freelist so skip them
		if(incrementalValidation) {
			if(*p == (int32)0xcacacaca)
				return;
		}
		for(int i=1; i<size; i++,p++)
		{
			if(*p)
			{
				PrintStackTrace(item);
				GCAssertMsg(false, "RCObject didn't clean up itself.");
			}
		}	
	}

	// used in DRC validation
	void CleanEntireStack()
	{
#if defined(MMGC_IA32) && defined(WIN32)
		// wipe clean the entire stack below esp.
		register void *stackP;
		__asm {
			mov stackP,esp
		} 
		MEMORY_BASIC_INFORMATION mib;
		// find the top of the stack
		VirtualQuery(stackP, &mib, sizeof(MEMORY_BASIC_INFORMATION));
		// go down whilst pages are committed
		char *stackPeak = (char*) mib.BaseAddress;
		while(true)
		{
			VirtualQuery(stackPeak - 4096, &mib, sizeof(MEMORY_BASIC_INFORMATION));
			// break when we hit the stack growth guard page or an uncommitted page
			// guard pages are committed so we need both checks (really the commit check
			// is just an extra precaution)
			if((mib.Protect & PAGE_GUARD) == PAGE_GUARD || mib.State != MEM_COMMIT)
				break;
			stackPeak -= 4096;
		}
		size_t amount = (char*) stackP - stackPeak - 128;
		{
			void *stack = alloca(amount);
			if(stack) {
				memset(stack, 0, amount);
			}
		}
#endif // _MMGC_IA32
	}
#endif // _DEBUG

	void ZCT::Reap()
	{
		if(gc->collecting || reaping || count == 0)
			return;

#ifdef DEBUGGER
		uint64 start = GC::GetPerformanceCounter();
		uint32 pagesStart = gc->totalGCPages;
		uint32 numObjects=0;
		uint32 objSize=0;
#endif

		reaping = true;
		
		SAMPLE_FRAME("[reap]", gc->core());

		// start by pinning live stack objects
		GCWorkItem item;
		MMGC_GET_STACK_EXENTS(gc, item.ptr, item._size);
		PinStackObjects(item.ptr, item._size);

		// important to do this before calling prereap
		// use index to iterate in case it grows, as we go through the list we
		// unpin pinned objects and pack them at the front of the list, that
		// way the ZCT is in a good state at the end
		zctIndex = 0;
		nextPinnedIndex = 0;

		// invoke prereap on all callbacks
		GCCallback *cb = gc->m_callbacks;
		while(cb) {
			cb->prereap();
			cb = cb->nextCB;
		}

#ifdef _DEBUG
		if(gc->validateDefRef) {
			// kill incremental mark since we're gonna wipe the marks
			gc->marking = false;
			gc->m_incrementalWork.Keep(0);

			CleanEntireStack();
			gc->Trace(item.ptr, item._size);
		}
#endif

		// first nuke the freelist, we won't have one when we're done reaping
		// and we don't want secondary objects put on the freelist (otherwise
		// reaping couldn't be a linear scan)
		while(zctFreelist) {
			RCObject **next = (RCObject**)*zctFreelist;
			*zctFreelist = 0;
            zctFreelist = next;
		}
		
		while(zct+zctIndex < zctNext) {
			SAMPLE_CHECK();
			RCObject *rcobj = zct[zctIndex++];
			if(rcobj && !rcobj->IsPinned()) {
				rcobj->ClearZCTFlag();
				zct[zctIndex-1] = NULL;
				count--;
#ifdef _DEBUG
				if(gc->validateDefRef) {
					if(gc->GetMark(rcobj)) {	
						rcobj->DumpHistory();
						GCDebugMsg(false, "Back pointer chain:");
						gc->DumpBackPointerChain(rcobj);
						GCAssertMsg(false, "Zero count object reachable, ref counts not correct!");
					}
				}
#endif
				// invoke prereap on all callbacks
				GCCallback *cbb = gc->m_callbacks;
				while(cbb) {
					cbb->prereap(rcobj);
					cbb = cbb->nextCB;
				}

				GCAssert(*(int*)rcobj != 0);
				GCAssert(gc->IsFinalized(rcobj));
				((GCFinalizedObject*)rcobj)->~GCFinalizedObject();
#ifdef DEBUGGER
				numObjects++;
				objSize += GC::Size(rcobj);
#endif
				gc->Free(rcobj);

				GCAssert(gc->weakRefs.get(rcobj) == NULL);
			} else if(rcobj) {
				// move it to front
				rcobj->Unpin();
				if(nextPinnedIndex != zctIndex-1) {
					rcobj->setZCTIndex(nextPinnedIndex);
					GCAssert(zct[nextPinnedIndex] == NULL);
					zct[nextPinnedIndex] = rcobj;
					zct[zctIndex-1] = NULL;
				}
				nextPinnedIndex++;				
			}
		}

		zctNext = zct + nextPinnedIndex;
		zctReapThreshold = count + ZCT_REAP_THRESHOLD;
		if(zctReapThreshold > int(zctSize*GCHeap::kBlockSize/sizeof(RCObject*)))
			zctReapThreshold = zctSize*GCHeap::kBlockSize/sizeof(RCObject*);
		GCAssert(nextPinnedIndex == count);
		zctIndex = nextPinnedIndex = 0;

		// invoke postreap on all callbacks
		cb = gc->m_callbacks;
		while(cb) {
			cb->postreap();
			cb = cb->nextCB;
		}
#ifdef DEBUGGER
		if(gc->gcstats && numObjects) {
			gc->gclog("[GC] DRC reaped %d objects (%d kb) freeing %d pages (%d kb) in %f millis (%f s)", 
				numObjects, objSize>>10, pagesStart - gc->totalGCPages, gc->totalGCPages*GCHeap::kBlockSize >> 10, 
				GC::duration(start), gc->duration(gc->t0)/1000);
		}
#endif
		reaping = false;
	}

#ifdef AVMPLUS_LINUX
	pthread_key_t stackTopKey = 0;

	uint32 GC::GetStackTop() const
	{
		if(stackTopKey == 0)
		{
			int res = pthread_key_create(&stackTopKey, NULL);
			GCAssert(res == 0);
		}

		void *stackTop = pthread_getspecific(stackTopKey);
		if(stackTop)
			return (uint32)stackTop;

		size_t sz;
		void *s_base;
		pthread_attr_t attr;
		
		// WARNING: stupid expensive function, hence the TLS
		int res = pthread_getattr_np(pthread_self(),&attr);
		GCAssert(res == 0);
		
		if(res)
		{
			// not good
			return 0;
		}

		res = pthread_attr_getstack(&attr,&s_base,&sz);
		GCAssert(res == 0);
		pthread_attr_destroy(&attr);

		stackTop = (void*) ((size_t)s_base + sz);
		// stackTop has to be greater than current stack pointer
		GCAssert(stackTop > &sz);
		pthread_setspecific(stackTopKey, stackTop);
		return (uint32)stackTop;
		
	}
#endif // AVMPLUS_LINUX
//#define ENABLE_GC_LOG

	#if defined(WIN32) && defined(ENABLE_GC_LOG)
	static bool called_cfltcvt_init = false;
	extern "C" void _cfltcvt_init();
	#endif

	void GC::gclog(const char *format, ...)
	{
		(void)format;
#ifdef ENABLE_GC_LOG
		char buf[4096];
		va_list argptr;

#ifdef WIN32
		if(!called_cfltcvt_init) {
			_cfltcvt_init();
			called_cfltcvt_init = true;
		}
#endif

		va_start(argptr, format);
		vsprintf(buf, format, argptr);
		va_end(argptr);

		GCAssert(strlen(buf) < 4096);
			
		GCCallback *cb = m_callbacks;
		while(cb) {
			cb->log(buf);
			cb = cb->nextCB;
		}
#endif
	}

#ifdef MEMORY_INFO
	void GC::DumpBackPointerChain(void *o)
	{
		int *p = (int*)GetRealPointer ( o ) ;
		int size = *p++;
		int traceIndex = *p++;
		//if(*(p+1) == 0xcacacaca || *(p+1) == 0xbabababa) {
			// bail, object was deleted
		//	return;
		//}
		GCDebugMsg(false, "Object: %s:0x%x\n", GetTypeName(traceIndex, o), p);
		PrintStackTraceByIndex(traceIndex);
		GCDebugMsg(false, "---\n");
		// skip data + endMarker
		p += 1 + (size>>2);
		void *container = (void*) *p;
		if(container && IsPointerToGCPage(container))
			DumpBackPointerChain(container);
		else 
		{
			GCDebugMsg(false, "GCRoot object: 0x%x\n", container);
			if((uintptr)container >= memStart && (uintptr)container < memEnd)
				PrintStackTrace(container);
		}
	}

	void GC::WriteBackPointer(const void *item, const void *container, size_t itemSize)
	{
		GCAssert(container != NULL);
		int *p = (int*) item;
		size_t size = *p++;
		if(size && size <= itemSize) {
			// skip traceIndex + data + endMarker
			p += (2 + (size>>2));
			GCAssert(sizeof(uintptr) == sizeof(void*));
			*p = (uintptr) container;
		}
	}

#endif

	bool GC::IsRCObject(const void *item)
	{
		if((uintptr)item >= memStart && (uintptr)item < memEnd && ((uintptr)item&0xfff) != 0)
		{
			int bits = GetPageMapValue((uintptr)item);		
			item = GetRealPointer(item);
			switch(bits)
			{
			case kGCAllocPage:
				if((char*)item < ((GCAlloc::GCBlock*)((uintptr)item&~0xfff))->items)
					return false;
				return GCAlloc::IsRCObject(item);
			case kGCLargeAllocPageFirst:
				return GCLargeAlloc::IsRCObject(item);
			default:
				return false;
			}
		}
		return false;
	}

#endif // MMGC_DRC
	
#ifdef MEMORY_INFO
	
	int DumpAlloc(GCAlloc *a)
	{
		int inUse =  a->GetNumAlloc() * a->GetItemSize();
		int maxAlloc =  a->GetMaxAlloc()* a->GetItemSize();
		int efficiency = maxAlloc > 0 ? inUse * 100 / maxAlloc : 100;
		if(inUse) {
			GCDebugMsg(false, "Allocator(%d): %d%% efficiency %d kb out of %d kb\n", a->GetItemSize(), efficiency, inUse>>10, maxAlloc>>10);
		}
		return maxAlloc-inUse;
	}

	void GC::DumpMemoryInfo()
	{
		if(heap->enableMemoryProfiling)
		{
			DumpFatties();
			if (dumpSizeClassState)
			{
				int waste=0;
				for(int i=0; i < kNumSizeClasses; i++)
				{
					waste += DumpAlloc(containsPointersAllocs[i]);
#ifdef MMGC_DRC
					waste += DumpAlloc(containsPointersRCAllocs[i]);
#endif
					waste += DumpAlloc(noPointersAllocs[i]);
				}
				GCDebugMsg(false, "Wasted %d kb\n", waste>>10);

			}
		}
	}

#endif

#ifdef _DEBUG

	void GC::CheckFreelist(GCAlloc *gca)
	{	
		GCAlloc::GCBlock *b = gca->m_firstFree;
		while(b)
		{
			void *freelist = b->firstFree;
			while(freelist)
			{			
				// b->firstFree should be either 0 end of free list or a pointer into b, otherwise, someone
				// wrote to freed memory and hosed our freelist
				GCAssert(freelist == 0 || ((uintptr) freelist >= (uintptr) b->items && (uintptr) freelist < (uintptr) b + GCHeap::kBlockSize));
				freelist = *((void**)freelist);
			}
			b = b->nextFree;
		}
	}

	void GC::CheckFreelists()
	{
		for(int i=0; i < kNumSizeClasses; i++)
		{
			CheckFreelist(containsPointersAllocs[i]);
			CheckFreelist(noPointersAllocs[i]);
		}
	}

	void GC::UnmarkedScan(const void *mem, size_t size)
	{
		uintptr lowerBound = memStart;
		uintptr upperBound = memEnd;
		
		uintptr *p = (uintptr *) mem;
		uintptr *end = p + (size / sizeof(void*));

		while(p < end)
		{
			uintptr val = *p++;
			
			if(val < lowerBound || val >= upperBound)
				continue;
			
			// normalize and divide by 4K to get index
			int bits = GetPageMapValue(val);
			switch(bits)
			{
			case 0:
				continue;
				break;
			case kGCAllocPage:
				GCAssert(GCAlloc::ConservativeGetMark((const void*) (val&~7), true));
				break;
			case kGCLargeAllocPageFirst:
				GCAssert(GCLargeAlloc::ConservativeGetMark((const void*) (val&~7), true));
				break;
			default:
				GCAssertMsg(false, "Invalid pageMap value");
				break;
			}
		}
	}

	void GC::FindUnmarkedPointers()
	{
		if(findUnmarkedPointers)
		{
			uintptr m = memStart;

			while(m < memEnd)
			{
#ifdef WIN32
				// first skip uncommitted memory
				MEMORY_BASIC_INFORMATION mib;
				VirtualQuery((void*) m, &mib, sizeof(MEMORY_BASIC_INFORMATION));
				if((mib.Protect & PAGE_READWRITE) == 0) {
					m += mib.RegionSize;
					continue;
				}
#endif
				// divide by 4K to get index
				int bits = GetPageMapValue(m);
				if(bits == kNonGC) {
					UnmarkedScan((const void*)m, GCHeap::kBlockSize);
					m += GCHeap::kBlockSize;
				} else if(bits == kGCLargeAllocPageFirst) {
					GCLargeAlloc::LargeBlock *lb = (GCLargeAlloc::LargeBlock*)m;
					const void *item = GetUserPointer((const void*)(lb+1));
					if(GCLargeAlloc::GetMark(item) && GCLargeAlloc::ContainsPointers(GetRealPointer(item))) {
						UnmarkedScan(item, GC::Size(item));
					}
					m += lb->GetNumBlocks() * GCHeap::kBlockSize;
				} else if(bits == kGCAllocPage) {
					// go through all marked objects
					GCAlloc::GCBlock *b = (GCAlloc::GCBlock *) m;
                    for (int i=0; i < b->alloc->m_itemsPerBlock; i++) {
                        // If the mark is 0, delete it.
                        int marked = GCAlloc::GetBit(b, i, GCAlloc::kMark);
                        if (!marked) {
                            void* item = (char*)b->items + b->alloc->m_itemSize*i;
                            if(GCAlloc::ContainsPointers(item)) {
								UnmarkedScan(GetUserPointer(item), b->alloc->m_itemSize - DebugSize());
							}
						}
					}
					
					m += GCHeap::kBlockSize;
				}				 
			}
		}
	}

/* macro to stack allocate a string containing 3*i (indent) spaces */
#define ALLOCA_AND_FILL_WITH_SPACES(b, i) \
	{ b = (char*)alloca((3*(i))+1); \
	int n = 0; \
	for(; n<3*(i); n++) b[n] = ' '; \
	b[n] = '\0'; }

	void GC::ProbeForMatch(const void *mem, size_t size, uintptr value, int recurseDepth, int currentDepth)
	{
		uintptr lowerBound = memStart;
		uintptr upperBound = memEnd;
		
		uintptr *p = (uintptr *) mem;
		uintptr *end = p + (size / sizeof(void*));

		int bits = GetPageMapValue((uintptr)mem);

		while(p < end)
		{
			uintptr val = *p++;
			
			if(val < lowerBound || val >= upperBound)
				continue;

			// did we hit ?
			if (val == value)
			{
				// ok so let's see where we are 
				uintptr* where = p-1;
				GCHeap::HeapBlock* block = heap->AddrToBlock(where);
				//GCAssertMsg(block->inUse(), "Not sure how we got here if the block is not in use?");
				GCAssertMsg(block->committed, "Means we are probing uncommitted memory. not good");
				int* ptr;			  

				switch(bits)
				{
				case kNonGC:
					{
						if (block->size == 1)
						{
							// fixed sized entries find out the size of the block
							FixedAlloc::FixedBlock* fixed = (FixedAlloc::FixedBlock*) block->baseAddr;
							int fixedsize = fixed->size;

							// now compute which element we are 
							uintptr startAt = (uintptr) &(fixed->items[0]);
							uintptr item = ((uintptr)where-startAt) / fixedsize;

							ptr = (int*) ( startAt + (item*fixedsize) );
						}
						else
						{
							// fixed large allocs ; start is after the block 
							ptr = (int*) block->baseAddr;
						}
					}
					break;

				default:
					ptr = ((int*)FindBeginning(where)) - 2;
					break;
				}

				int  taggedSize = *ptr;
				int  traceIndex = *(ptr+1);
				int* real = (ptr+2);

				char* buffer = 0;
				ALLOCA_AND_FILL_WITH_SPACES(buffer, currentDepth);

				if (buffer) GCDebugMsg(false, buffer);
				GCDebugMsg(false, "Location: 0x%08x  Object: 0x%08x (size %d)\n", where, real, taggedSize);
				if (buffer) GCDebugMsg(false, buffer);
				PrintStackTraceByIndex(traceIndex);

				if (recurseDepth > 0)
					WhosPointingAtMe(real, recurseDepth-1, currentDepth+1);
			}
		}
	}

	/**
	 * This routine searches through all of GC memory looking for references to 'me' 
	 * It ignores already claimed memory thus locating active references only.
	 * recurseDepth can be set to a +ve value in order to follow the chain of 
	 * pointers arbitrarily deep.  Watch out when setting it since you may see
	 * an exponential blow-up (usu. 1 or 2 is enough).	'currentDepth' is for
	 * indenting purposes and should be left alone.
	 */
	void GC::WhosPointingAtMe(void* me, int recurseDepth, int currentDepth)
	{
		uintptr val = (uintptr)me;
		uintptr m = memStart;

		char* buffer = 0;
		ALLOCA_AND_FILL_WITH_SPACES(buffer, currentDepth);

		if (buffer) GCDebugMsg(false, buffer);
		GCDebugMsg(false, "[%d] Probing for pointers to : 0x%08x\n", currentDepth, me);
		while(m < memEnd)
		{
#ifdef WIN32
			// first skip uncommitted memory
			MEMORY_BASIC_INFORMATION mib;
			VirtualQuery((void*) m, &mib, sizeof(MEMORY_BASIC_INFORMATION));
			if((mib.Protect & PAGE_READWRITE) == 0) 
			{
				m += mib.RegionSize;
				continue;
			}
#endif

			// divide by 4K to get index
			int bits = GetPageMapValue(m);
			if(bits == kNonGC) 
			{
				ProbeForMatch((const void*)m, GCHeap::kBlockSize, val, recurseDepth, currentDepth);
				m += GCHeap::kBlockSize;
			} 
			else if(bits == kGCLargeAllocPageFirst) 
			{
				GCLargeAlloc::LargeBlock *lb = (GCLargeAlloc::LargeBlock*)m;
				const void *item = GetUserPointer((const void*)(lb+1));
				bool marked = GCLargeAlloc::GetMark(item);
				if (marked)
				{
					if(GCLargeAlloc::ContainsPointers(GetRealPointer(item))) 
					{
						ProbeForMatch(item, GC::Size(item), val, recurseDepth, currentDepth);
					}
				}
				m += lb->GetNumBlocks() * GCHeap::kBlockSize;
			} 
			else if(bits == kGCAllocPage) 
			{
				// go through all marked objects
				GCAlloc::GCBlock *b = (GCAlloc::GCBlock *) m;
                for (int i=0; i < b->alloc->m_itemsPerBlock; i++) 
				{
                    int marked = GCAlloc::GetBit(b, i, GCAlloc::kMark);
                    if (marked) 
					{
                        void* item = (char*)b->items + b->alloc->m_itemSize*i;
                        if(GCAlloc::ContainsPointers(item)) 
						{
							ProbeForMatch(GetUserPointer(item), b->alloc->m_itemSize - DebugSize(), val, recurseDepth, currentDepth);
						}
					}
				}				
				m += GCHeap::kBlockSize;
			}
			else
			{
				GCAssertMsg(false, "Oh seems we missed a case...Tom any ideas here?");
			
			}
		}
	}
#undef ALLOCA_AND_FILL_WITH_SPACES
#endif


	void GC::StartIncrementalMark()
	{
		GCAssert(!marking);
		GCAssert(!collecting);

		lastStartMarkIncrementCount = markIncrements;

		// set the stack cleaning trigger
		stackCleaned = false;

		marking = true;

		GCAssert(m_incrementalWork.Count() == 0);
	
		uint64 start = GetPerformanceCounter();

		// clean up any pages that need sweeping
		for(int i=0; i < kNumSizeClasses; i++) {
#ifdef MMGC_DRC
			containsPointersRCAllocs[i]->SweepNeedsSweeping();
#endif
			containsPointersAllocs[i]->SweepNeedsSweeping();
			noPointersAllocs[i]->SweepNeedsSweeping();
		}

		// at this point every object should have no marks or be marked kFreelist
#ifdef _DEBUG		
		for(int i=0; i < kNumSizeClasses; i++) {
#ifdef MMGC_DRC
			containsPointersRCAllocs[i]->CheckMarks();
#endif
			containsPointersAllocs[i]->CheckMarks();
			noPointersAllocs[i]->CheckMarks();
		}
#endif
	
		{
#ifdef GCHEAP_LOCK
			GCAcquireSpinlock lock(m_rootListLock);
#endif
			GCRoot *r = m_roots;
			while(r) {
				GCWorkItem item = r->GetWorkItem();
				MarkItem(item, m_incrementalWork);
				r = r->next;
			}
		}
		markTicks += GetPerformanceCounter() - start;
		IncrementalMark();
	}

#if 0
	// TODO: SSE2 version
	void GC::MarkItem_MMX(const void *ptr, size_t size, GCStack<GCWorkItem> &work)
	{
		 uintptr *p = (uintptr*) ptr;
		// deleted things are removed from the queue by setting them to null
		if(!p)
			return;

		bytesMarked += size;
		marks++;

		uintptr *end = p + (size / sizeof(void*));
		uintptr thisPage = (uintptr)p & ~0xfff;

		// since MarkItem recurses we have to do this before entering the loop
		if(IsPointerToGCPage(ptr)) 
		{
			int b = SetMark(ptr);
#ifdef _DEBUG
			// def ref validation does a Trace which can 
			// cause things on the work queue to be already marked
			// in incremental GC
			if(!validateDefRef) {
				GCAssert(!b);
			}
#endif
		}


		_asm {
			// load memStart and End into mm0
			movq mm0,memStart			
		}

		while(p < end) 
		{		
			_asm {
				mov       ebx, [p]
				mov       ecx, [count]
				sar       ecx, 1
				mov       eax, dword ptr [lowerBound]
				dec       eax
				movd      mm1, eax
				movd      mm2, dword ptr [upperBound]
				punpckldq mm1, mm1
				punpckldq mm2, mm2
				mov		  eax, 3
				movd	  mm5, eax
				punpckldq mm5, mm5				
			  MarkLoop:
				movq      mm0, qword ptr [ebx]
				movq      mm3, mm0
				pcmpgtd   mm3, mm1
				movq      mm4, mm2
				pcmpgtd   mm4, mm0
				pand      mm3, mm4
				packssdw  mm3, mm3
				movd      eax, mm3
				or        eax, eax
				jz        Advance

				// normalize and divide by 4K to get index
				psubd	  mm0, mm1
				psrld     mm0, 12

				// shift amount to determine position in the byte (times 2 b/c 2 bits per page)
				movq      mm6, mm0
				pand      mm6, mm5
				pslld     mm6, 1
				packssdw  mm6, mm6

				// index = index >> 2 for pageMap index
				psrld     mm0, 2
				packssdw  mm0, mm0

				// check 
				push      ecx

				

				push	  [workAddr]
				movd	  edx, mm6
				push      edx // packShiftAmount
				movd	  edx, mm0
				push	  edx // packIndex4
				push	  eax // packTest
				push	  dword ptr [ebx+4] // val2
				push	  dword ptr [ebx] // val1
				mov		  ecx, [this]
				call	  ConservativeMarkMMXHelper
					
				pop		  ecx

			Advance:
				add       ebx, 8
				loop      MarkLoop
				mov       dword ptr [p], ebx				
			}
		}
	}

#endif

	void GC::MarkItem(GCWorkItem &wi, GCStack<GCWorkItem> &work)
	{
		size_t size = wi.GetSize();
		uintptr *p = (uintptr*) wi.ptr;

		bytesMarked += size;
		marks++;

		uintptr *end = p + (size / sizeof(void*));
		uintptr thisPage = (uintptr)p & ~0xfff;

		// set the mark bits on this guy
		if(wi.IsGCItem())
		{
			int b = SetMark(wi.ptr);
			(void)b;
#ifdef _DEBUG
			// def ref validation does a Trace which can 
			// cause things on the work queue to be already marked
			// in incremental GC
			if(!validateDefRef) {
				GCAssert(!b);
			}
#endif			
		}
		else
		{
			GCAssert(!IsPointerToGCPage(wi.ptr));
		}

		uintptr _memStart = memStart;
		uintptr _memEnd = memEnd;
		
#ifdef DEBUGGER
		numObjects++;
		objSize+=size;
#endif

		while(p < end) 
		{
			uintptr val = *p++;  

			if(val < _memStart || val >= _memEnd)
				continue;

			// normalize and divide by 4K to get index
			int bits = GetPageMapValue(val); 
			
			if (bits == 1)
			{
				//GCAlloc::ConservativeMark(work, (void*) (val&~7), workitem.ptr);
				const void* item = (void*) (val&~7);

				GCAlloc::GCBlock *block = (GCAlloc::GCBlock*) ((uintptr) item & ~0xFFF);

				// back up to real beginning
				item = GetRealPointer((const void*) item);

				// guard against bogus pointers to the block header
				if(item < block->items)
					continue;

				// make sure this is a pointer to the beginning

				int itemNum = GCAlloc::GetIndex(block, item);

				if(block->items + itemNum * block->size != item)
					continue;

				// inline IsWhite/SetBit
				// FIXME: see if using 32 bit values is faster
				uint32 *pbits = &block->GetBits()[itemNum>>3];
				int shift = (itemNum&0x7)<<2;
				int bits2 = *pbits;
				//if(GCAlloc::IsWhite(block, itemNum)) 
				if((bits2 & ((GCAlloc::kMark|GCAlloc::kQueued)<<shift)) == 0)
				{
					if(block->alloc->ContainsPointers())
					{
						const void *realItem = item;
						size_t itemSize = block->size;
						#ifdef MEMORY_INFO
						realItem = GetUserPointer(realItem);
						itemSize -= DebugSize();
						#endif
						if(((uintptr)realItem & ~0xfff) != thisPage)
						{							
							*pbits = bits2 | (GCAlloc::kQueued << shift);
							block->gc->PushWorkItem(work, GCWorkItem(realItem, itemSize, true));
						}
						else
						{
							// clear queued bit
							*pbits = bits2 & ~(GCAlloc::kQueued << shift);
							// skip stack for same page items, this recursion is naturally limited by
							// how many item can appear on a page, worst case is 8 byte linked list or
							// 512
							GCWorkItem newItem(realItem, itemSize, true);
							MarkItem(newItem, work);
						}
					}
					else
					{
						//GCAlloc::SetBit(block, itemNum, GCAlloc::kMark);
						// clear queued bit
						*pbits = bits2 & ~(GCAlloc::kQueued << shift);
						*pbits = bits2 | (GCAlloc::kMark << shift);
					}
					#if defined(MEMORY_INFO)
					GC::WriteBackPointer(item, (end==(void*)0x130000) ? p-1 : wi.ptr, block->size);
					#endif
				}
			}
			else if (bits == 3)
			{
				//largeAlloc->ConservativeMark(work, (void*) (val&~7), workitem.ptr);
				const void* item = (void*) (val&~7);

				// back up to real beginning
				item = GetRealPointer((const void*) item);

				if(((uintptr) item & 0xfff) == sizeof(GCLargeAlloc::LargeBlock))
				{
					GCLargeAlloc::LargeBlock *b = GCLargeAlloc::GetBlockHeader(item);
					if((b->flags & (GCLargeAlloc::kQueuedFlag|GCLargeAlloc::kMarkFlag)) == 0) 
					{
						size_t usize = b->usableSize;
						if((b->flags & GCLargeAlloc::kContainsPointers) != 0) 
						{
							b->flags |= GCLargeAlloc::kQueuedFlag;
							const void *realItem = item;
							#ifdef MEMORY_INFO
							realItem = GetUserPointer(item);
							usize -= DebugSize();
							#endif
							b->gc->PushWorkItem(work, GCWorkItem(realItem, usize, true));
						} 
						else
						{
							// doesn't need marking go right to black
							b->flags |= GCLargeAlloc::kMarkFlag;
						}
						#if defined(MEMORY_INFO)
						GC::WriteBackPointer(item, end==(void*)0x130000 ? p-1 : wi.ptr, usize);
						#endif
					}
				}
			}
		}
	}

	uint64 GC::GetPerformanceCounter()
	{
		#ifdef WIN32
		LARGE_INTEGER value;
		QueryPerformanceCounter(&value);
		return value.QuadPart;
		#else
		#ifndef AVMPLUS_LINUX // TODO_LINUX
		#ifndef MMGC_ARM
		UnsignedWide microsecs;
		::Microseconds(&microsecs);

		UInt64 retval;
		memcpy(&retval, &microsecs, sizeof(retval));
		return retval;
		#endif //MMGC_ARM
		#endif //AVMPLUS_LINUX
		#endif
	}

	uint64 GC::GetPerformanceFrequency()
	{
		#ifdef WIN32
		static uint64 gPerformanceFrequency = 0;		
		if (gPerformanceFrequency == 0) {
			QueryPerformanceFrequency((LARGE_INTEGER*)&gPerformanceFrequency);
		}
		return gPerformanceFrequency;
		#else
		return 1000000;
		#endif
	}
	
	void GC::IncrementalMark(uint32 time)
	{
		SAMPLE_FRAME("[mark]", core());
		if(m_incrementalWork.Count() == 0 || hitZeroObjects) {
			FinishIncrementalMark();
			return;
		} 

#ifdef DEBUGGER
		StartGCActivity();
#endif
		
		markIncrements++;
		// FIXME: tune this so that getPerformanceCounter() overhead is noise
		static unsigned int checkTimeIncrements = 100;
		uint64 start = GetPerformanceCounter();

#ifdef DEBUGGER
		numObjects=0;
		objSize=0;
#endif

		uint64 ticks = start + time * GetPerformanceFrequency() / 1000;
		do {
			unsigned int count = m_incrementalWork.Count();
			if (count == 0) {
				hitZeroObjects = true;
				break;
			}
			if (count > checkTimeIncrements) {
				count = checkTimeIncrements;
			}
			for(unsigned int i=0; i<count; i++) 
			{
 				MarkItem(m_incrementalWork);
			}
			SAMPLE_CHECK();
		} while(GetPerformanceCounter() < ticks);

		lastMarkTicks = GetPerformanceCounter();
		markTicks += lastMarkTicks - start;

#ifdef DEBUGGER
		if(GC::gcstats) {
			double millis = duration(start);
			uint32 kb = objSize>>10;
			gclog("[GC] mark(%d) %d objects (%d kb %d mb/s) in %4Lf millis (%4Lf s)\n", 
				markIncrements-lastStartMarkIncrementCount, numObjects, kb, 
				uint32(double(kb)/millis), millis, duration(t0)/1000);
		}
		StopGCActivity();
#endif
	}

	void GC::FinishIncrementalMark()
	{
		// Don't finish an incremental mark (i.e., sweep) if we
		// are in the midst of a ZCT reap.
		if (zct.reaping)
		{
			return;
		}
		
		hitZeroObjects = false;

		// finished in Sweep
		sweepStart = GetPerformanceCounter();
		
		// mark roots again, could have changed (alternative is to put WB's on the roots
		// which we may need to do if we find FinishIncrementalMark taking too long)
		
		{
#ifdef GCHEAP_LOCK
			GCAcquireSpinlock lock(m_rootListLock);
#endif
			GCRoot *r = m_roots;
			while(r) {					
				GCWorkItem item = r->GetWorkItem();
				// need to do this while holding the root lock so we don't end 
				// up trying to scan a deleted item later, another reason to keep
				// the root set small
				MarkItem(item, m_incrementalWork);
				r = r->next;
			}
		}
		MarkQueueAndStack(m_incrementalWork);

#ifdef _DEBUG
		// need to traverse all marked objects and make sure they don't contain
		// pointers to unmarked objects
		FindMissingWriteBarriers();
#endif

		GCAssert(!collecting);
		collecting = true;
		GCAssert(m_incrementalWork.Count() == 0);
		Sweep();
		GCAssert(m_incrementalWork.Count() == 0);
		collecting = false;
		marking = false;
	}

	int GC::IsWhite(const void *item)
	{
		// back up to real beginning
		item = GetRealPointer((const void*) item);

		// normalize and divide by 4K to get index
		if(!IsPointerToGCPage(item))
			return false;
		int bits = GetPageMapValue((uintptr)item);	
		switch(bits) {
		case 1:
			return GCAlloc::IsWhite(item);
		case 3:
			// FIXME: we only want pointers to the first page for large items, fix
			// this by marking the first page and subsequent pages of large items differently
			// in the page map (ie use 2).
			return GCLargeAlloc::IsWhite(item);
		}
		return false;
	}
	
	// TODO: fix headers so this can be declared there and inlined
	void GC::WriteBarrierWrite(const void *address, const void *value)
	{
		GCAssert(!IsRCObject(value));
		*(uintptr*)address = (uintptr) value;
	}

	// optimized version with no RC checks or pointer swizzling
	void GC::writeBarrierRC(const void *container, const void *address, const void *value)
	{
		GCAssert(IsPointerToGCPage(container));
		GCAssert(((uintptr)container & 3) == 0);
		GCAssert(((uintptr)address & 2) == 0);
		GCAssert(address >= container);
		GCAssert(address < (char*)container + Size(container));

		WriteBarrierNoSubstitute(container, value);
		WriteBarrierWriteRC(address, value);
	}

	// TODO: fix headers so this can be declared there and inlined
	void GC::WriteBarrierWriteRC(const void *address, const void *value)
	{
		#ifdef MMGC_DRC
			RCObject *rc = (RCObject*)Pointer(*(RCObject**)address);
			if(rc != NULL) {
				GCAssert(rc == FindBeginning(rc));
				GCAssert(IsRCObject(rc));
				rc->DecrementRef();
			}
		#endif
		*(uintptr*)address = (uintptr) value;
		#ifdef MMGC_DRC		
			rc = (RCObject*)Pointer(value);
			if(rc != NULL) {
				GCAssert(IsRCObject(rc));
				GCAssert(rc == FindBeginning(value));
				rc->IncrementRef();
			}
		#endif
	}

	void GC::WriteBarrier(const void *address, const void *value)
	{
		GC* gc = GC::GetGC(address);
		if(Pointer(value) != NULL && gc->marking) {
			void *container = gc->FindBeginning(address);
			gc->WriteBarrierNoSubstitute(container, value);
		}
		gc->WriteBarrierWrite(address, value);
	}

	void GC::WriteBarrierNoSub(const void *address, const void *value)
	{
		GC *gc = NULL;
		if(value != NULL && (gc = GC::GetGC(address))->marking) {
			void *container = gc->FindBeginning(address);
			gc->WriteBarrierNoSubstitute(container, value);		
		}
	}

	void GC::TrapWrite(const void *black, const void *white)
	{
		// assert fast path preconditions
		(void)black;
		GCAssert(marking);
		GCAssert(GetMark(black));
		GCAssert(IsWhite(white));
		// currently using the simplest finest grained implementation,
		// which could result in huge work queues.  should try the
		// more granular approach of moving the black object to grey
		// (smaller work queue, less frequent wb slow path) but if the
		// black object is big we end up doing useless redundant
		// marking.  optimal approach from lit is card marking (mark a
		// region of the black object as needing to be re-marked)
		if(ContainsPointers(white)) {
			SetQueued(white);
			PushWorkItem(m_incrementalWork, GCWorkItem(white, Size(white), true));
		} else {
			SetMark(white);
		}
	}

	bool GC::ContainsPointers(const void *item)
	{
		item = GetRealPointer(item);
		if (GCLargeAlloc::IsLargeBlock(item)) {
			return GCLargeAlloc::ContainsPointers(item);
		} else {
			return GCAlloc::ContainsPointers(item);
		}
	}

	bool GC::IsGCMemory (const void *item)
	{
		int bits = GetPageMapValue((uintptr)item);
		return (bits != 0);
	}

	bool GC::IsQueued(const void *item)
	{
		return !GetMark(item) && !IsWhite(item);
	}

	uint32 *GC::GetBits(int numBytes, int sizeClass)
	{
		uint32 *bits;

		GCAssert(numBytes % 4 == 0);

		#ifdef MMGC_64BIT // we use first 8-byte slot for the free list
		if (numBytes == 4)
			numBytes = 8;
		#endif

		// hit freelists first
		if(m_bitsFreelists[sizeClass]) {
			bits = m_bitsFreelists[sizeClass];
			m_bitsFreelists[sizeClass] = *(uint32**)bits;
			memset(bits, 0, sizeof(uint32*));
			return bits;
		}

		if(!m_bitsNext)
			m_bitsNext = (uint32*)heap->Alloc(1);

		int leftOver = GCHeap::kBlockSize - ((uintptr)m_bitsNext & 0xfff);
		if(leftOver >= numBytes) {
			bits = m_bitsNext;
			if(leftOver == numBytes) 
				m_bitsNext = 0;
			else 
				m_bitsNext += numBytes/sizeof(uint32);
		} else {
			if(leftOver > 0) {
				// put waste in freelist
				for(int i=0, n=kNumSizeClasses; i<n; i++) {
					GCAlloc *a = noPointersAllocs[i];
					if(!a->m_bitsInPage && a->m_numBitmapBytes <= leftOver) {
						FreeBits(m_bitsNext, a->m_sizeClassIndex);
						break;
					}
				}
			}
			m_bitsNext = 0;
			// recurse rather than duplicating code
			return GetBits(numBytes, sizeClass);
		}
		return bits;
	}

	void GC::AddRoot(GCRoot *root)
	{
#ifdef GCHEAP_LOCK
		GCAcquireSpinlock lock(m_rootListLock);
#endif
		root->prev = NULL;
		root->next = m_roots;
		if(m_roots)
			m_roots->prev = root;
		m_roots = root;
	}

	void GC::RemoveRoot(GCRoot *root)
	{		
#ifdef GCHEAP_LOCK
		GCAcquireSpinlock lock(m_rootListLock);
#endif
		if( m_roots == root )
			m_roots = root->next;
		else
			root->prev->next = root->next;

		if(root->next)
			root->next->prev = root->prev;
	}
	
	void GC::AddCallback(GCCallback *cb)
	{
		CheckThread();
		cb->prevCB = NULL;
		cb->nextCB = m_callbacks;
		if(m_callbacks)
			m_callbacks->prevCB = cb;
		m_callbacks = cb;
	}

	void GC::RemoveCallback(GCCallback *cb)
	{
		CheckThread();
		if( m_callbacks == cb )
			m_callbacks = cb->nextCB;
		else
			cb->prevCB->nextCB = cb->nextCB;

		if(cb->nextCB)
			cb->nextCB->prevCB = cb->prevCB;
	}

	void GC::PushWorkItem(GCStack<GCWorkItem> &stack, GCWorkItem item)
	{
		if(item.ptr) {
			stack.Push(item);
		}
	}

	GCWeakRef* GC::GetWeakRef(const void *item) 
	{
		GC *gc = GetGC(item);
		GCWeakRef *ref = (GCWeakRef*) gc->weakRefs.get(item);
		if(ref == NULL) {
			ref = new (gc) GCWeakRef(item);
			gc->weakRefs.put(item, ref);
			item = GetRealPointer(item);
			if (GCLargeAlloc::IsLargeBlock(item)) {
				GCLargeAlloc::SetHasWeakRef(item, true);
			} else {
				GCAlloc::SetHasWeakRef(item, true);
			}
		} else {
			GCAssert(ref->get() == item);
		}
		return ref;
	}

	void GC::ClearWeakRef(const void *item)
	{
		GCWeakRef *ref = (GCWeakRef*) weakRefs.remove(item);
		GCAssert(weakRefs.get(item) == NULL);
		GCAssert(ref != NULL);
		GCAssert(ref->get() == item || ref->get() == NULL);
		if(ref) {
			ref->m_obj = NULL;
			item = GetRealPointer(item);
			if (GCLargeAlloc::IsLargeBlock(item)) {
				GCLargeAlloc::SetHasWeakRef(item, false);
			} else {
				GCAlloc::SetHasWeakRef(item, false);
			}
		}
	}

#ifdef _DEBUG
	
	void GC::WhitePointerScan(const void *mem, size_t size)
	{		
		uintptr *p = (uintptr *) mem;
		// the minus 8 skips the deadbeef and back pointer 
		uintptr *end = p + ((size) / sizeof(void*));

		while(p < end)
		{
			uintptr val = *p;		
			if(val == 0xdeadbeef)
				break;
			if(IsWhite((const void*) (val&~7)) && 
			   *(((int32*)(val&~7))+1) != (int32)0xcacacaca && // Free'd
			   *(((int32*)(val&~7))+1) != (int32)0xbabababa) // Swept
			{
				GCDebugMsg(false, "Object 0x%x allocated here:\n", mem);
				PrintStackTrace(mem);
				GCDebugMsg(false, "Didn't mark pointer at 0x%x, object 0x%x allocated here:\n", p, val);
				PrintStackTrace((const void*)(val&~7));
				GCAssert(false);
			}
			p++;
		}
	}

	void GC::FindMissingWriteBarriers()
	{
		if(!incrementalValidation)
			return;

		uintptr m = memStart;
		while(m < memEnd)
		{
#ifdef WIN32
			// first skip uncommitted memory
			MEMORY_BASIC_INFORMATION mib;
			VirtualQuery((void*) m, &mib, sizeof(MEMORY_BASIC_INFORMATION));
			if((mib.Protect & PAGE_READWRITE) == 0) {
				m += mib.RegionSize;
				continue;
			}
#endif
			// divide by 4K to get index
			int bits = GetPageMapValue(m);
			switch(bits)
			{
			case 0:
				m += GCHeap::kBlockSize;
				break;
			case 3:
				{
					GCLargeAlloc::LargeBlock *lb = (GCLargeAlloc::LargeBlock*)m;
					const void *item = GetUserPointer((const void*)(lb+1));
					if(GCLargeAlloc::GetMark(item) && GCLargeAlloc::ContainsPointers(item)) {
						WhitePointerScan(item, lb->usableSize - DebugSize());
					}
					m += lb->GetNumBlocks() * GCHeap::kBlockSize;
				}
				break;
			case 1:
				{
					// go through all marked objects in this page
					GCAlloc::GCBlock *b = (GCAlloc::GCBlock *) m;
                    for (int i=0; i< b->alloc->m_itemsPerBlock; i++) {
                        // find all marked objects and search them
                        if(!GCAlloc::GetBit(b, i, GCAlloc::kMark))
                            continue;

						if(b->alloc->ContainsPointers()) {
	                        void* item = (char*)b->items + b->alloc->m_itemSize*i;
							WhitePointerScan(GetUserPointer(item), b->alloc->m_itemSize - DebugSize());
						}
					}
					m += GCHeap::kBlockSize;
				}
				break;
			default:
				GCAssert(false);
				break;
			}
		}
	}
#endif

#ifdef DEBUGGER
	void GC::StartGCActivity()
	{
		// invoke postsweep callback
		GCCallback *cb = m_callbacks;
		while(cb) {
			cb->startGCActivity();
			cb = cb->nextCB;
		} 
	}

	void GC::StopGCActivity()
	{
		// invoke postsweep callback
		GCCallback *cb = m_callbacks;
		while(cb) {
			cb->stopGCActivity();
			cb = cb->nextCB;
		}
	}

	void GC::AllocActivity(int blocks)
	{
		// invoke postsweep callback
		GCCallback *cb = m_callbacks;
		while(cb) {
			cb->allocActivity(blocks);
			cb = cb->nextCB;
		}
	}
#endif  /* DEBUGGER*/

#if defined(_MAC) && (defined(MMGC_IA32) || defined(MMGC_AMD64))
	uintptr GC::GetStackTop() const
	{
		return (uintptr)pthread_get_stackaddr_np(pthread_self());
	}
#endif


}
