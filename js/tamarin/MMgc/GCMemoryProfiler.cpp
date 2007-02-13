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


#include "MMgc.h"

#ifdef MEMORY_INFO

#ifndef __MWERKS__
#include <typeinfo>
#endif

namespace MMgc
{
	GCThreadLocal<const char*> memtag;
	GCThreadLocal<void*> memtype;

	// Turn this to see GC stack traces.
	const bool enableTraces = false;

	// this is how many stack frames we'll attempt to lookup, we may not get this many and 
	// we may leave some out
	const int kMaxTraceDepth = 7;

	// include total and swept memory totals in memory profiling dumps
	const bool showTotal = false;	
	const bool showSwept = false;

	// controls size of table which is fixed
    const int kNumTracesPow = 20;
	const int kNumTraces = (enableTraces ? 1 << kNumTracesPow : 1);

#ifdef GCHEAP_LOCK
	GCCriticalSection m_traceTableLock;
#endif

	struct StackTrace
	{
		int ips[kMaxTraceDepth];
		int size;
		int totalSize;
		int sweepSize;
        int vtable;
		const char *memtag;
		int count;
		int totalCount;
		int sweepCount;
		bool lumped;
	};

	static StackTrace traceTable[kNumTraces];

	unsigned int hashTrace(int *trace)
	{
		unsigned int hash = *trace++;
		while(*trace++ != 0) {
			hash ^= *trace;
		}
		return hash;
	}

	bool tracesEqual(int *trace1, int *trace2)
	{
		while(*trace1) {
			if(*trace1 != *trace2)
				return false;
			trace1++;
			trace2++;
		}
		return *trace1 == *trace2;
	}

	unsigned int LookupTrace(int *trace)
	{
#ifdef GCHEAP_LOCK
		GCEnterCriticalSection lock(m_traceTableLock);
#endif
		// this is true when traces are off
		if(*trace == 0)
			return 0;
		
		static int numTraces = 0;
		int modmask = kNumTraces - 1;
		unsigned int hash = hashTrace(trace);
		unsigned int index = hash & modmask;
		unsigned int n = 17; // small number means cluster at start
		int c = 1;
		while(traceTable[index].ips[0] && !tracesEqual(traceTable[index].ips, trace)) {
			// probe
			index = (index + (n=n+c)) & modmask;
		}
		if(traceTable[index].ips[0] == 0) {
			memcpy(traceTable[index].ips, trace, kMaxTraceDepth * sizeof(int));
			numTraces++;
		}
		if(numTraces == kNumTraces) {
			GCAssertMsg(false, "Increase trace table size!");
		}
		return index;
	}

	// increase this to get more
	const int kNumTypes = 10;
	const int kNumTracesPerType=5;

	// data structure to gather allocations by type with the top 5 traces
	struct TypeGroup
	{
		const char *name;
		size_t size;
		int count;
		int traces[kNumTracesPerType ? kNumTracesPerType : 1];
	};

	const char* GetTypeName(int index, void *obj)
	{
		// cache
		if(index > kNumTraces)
			return "unknown";
		if(traceTable[index].memtag)
			return traceTable[index].memtag;
		const char*name="unknown";
#if defined (WIN32) || defined(AVMPLUS_LINUX)
		try {
			const std::type_info *ti = &typeid(*(MMgc::GCObject*)obj);
			if(ti->name())
				name = ti->name();
			// sometimes name will get set to bogus memory with no exceptions catch that
			char c = *name;
			(void)c;	// silence compiler warning
		} catch(...) {
			name = "unknown";
		}
#else
		(void)obj;
#endif // WIN32 || LINUX
		// cache
		traceTable[index].memtag = name;
		return name;
	}

#define PERCENT(all, some) ((((float)some)/(float)all)*100.0)

	void DumpFatties()
	{
		GCHashtable typeTable(128);
#ifdef GCHEAP_LOCK
		//GCEnterCriticalSection lock(m_traceTableLock);
#endif
		int residentSize=0;
		int residentCount=0;

		for(int i=0; i < kNumTraces; i++)
		{
			int size;

			if(showSwept) {
				size = traceTable[i].sweepSize;
			} else if(showTotal) {
				size = traceTable[i].totalSize;
			} else {
				size = traceTable[i].size;
			}

			if(size == 0)
				continue;
			residentSize += size;

			int count = traceTable[i].lumped ? 0 : traceTable[i].count;
			residentCount += traceTable[i].count;

			const char *name = "unknown";
#ifndef _MAC
#ifndef MMGC_ARM
			name = GetTypeName(i, &traceTable[i].vtable);
#endif //MMGC_ARM
#endif
			TypeGroup *tg = (TypeGroup*) typeTable.get((void*)name);
			if(tg)  {
				GCAssert(tg->name == name);
				tg->size += size;
				tg->count += count;
				for(int j=0; j<kNumTracesPerType; j++) {
					if(traceTable[tg->traces[j]].size < size) {
						if(j != kNumTracesPerType-1) {
							memmove(&tg->traces[j+1], &tg->traces[j], (kNumTracesPerType-j-1)*sizeof(int));
						}
						tg->traces[j] = i;
						break;
					}
				}			
			} else {
				tg = new TypeGroup();
				tg->size = size;
				tg->count = count;
				tg->name = name;
				tg->traces[0] = i;
				if(kNumTracesPerType) {
					int num = kNumTracesPerType - 1;
					memset(&tg->traces[1], 0, sizeof(int)*num);
				}
				typeTable.put((void*)name, tg);
			}
		}

		int codeSize = 0;

		GCHeap* heap = GCHeap::GetGCHeap();
		
#ifdef MMGC_AVMPLUS
		codeSize = heap->GetCodeMemorySize();
#endif
		int inUse = heap->GetUsedHeapSize() * GCHeap::kBlockSize;
		int committed = heap->GetTotalHeapSize() * GCHeap::kBlockSize + codeSize;
		int free = heap->GetFreeHeapSize() * GCHeap::kBlockSize;

		int memInfo = residentCount*16;

		// executive summary
		GCDebugMsg(false, "Code Size %d kb \n", codeSize>>10);
		GCDebugMsg(false, "Total in use (used pages, ignoring code) %d kb \n", inUse>>10);
		GCDebugMsg(false, "Total resident (individual allocations - code) %d kb \n", (residentSize-codeSize)>>10);
		GCDebugMsg(false, "Allocator overhead (used pages - resident, ignoring code and mem_info): %d kb \n", ((inUse-memInfo)-(residentSize-codeSize))>>10);
		GCDebugMsg(false, "Heap overhead (unused committed pages, ignoring code): %d kb \n", free>>10);
		GCDebugMsg(false, "Total committed (including code) %d kb \n\n", committed>>10);

		TypeGroup *residentFatties[kNumTypes];
		memset(residentFatties, 0, kNumTypes * sizeof(TypeGroup *));
		GCHashtableIterator iter(&typeTable);
		const char *name;
		while((name = (const char*)iter.nextKey()) != NULL)
		{
			TypeGroup *tg = (TypeGroup*)iter.value();
			for(int j=0; j<kNumTypes; j++) {
				if(!residentFatties[j]) {
					residentFatties[j] = tg;
					break;
				}
				if(residentFatties[j]->size < tg->size) {
					if(j != kNumTypes-1) {
						memmove(&residentFatties[j+1], &residentFatties[j], (kNumTypes-j-1) * sizeof(TypeGroup *));
					}
					residentFatties[j] = tg;
					break;
				}
			}			
		}

		for(int i=0; i < kNumTypes; i++)
		{
			TypeGroup *tg = residentFatties[i];
			if(!tg) 
				break;
			GCDebugMsg(false, "%s - %3.1f%% - %d kb %d items, avg %d b\n", tg->name, PERCENT(residentSize, tg->size), tg->size>>10, tg->count, tg->count ? tg->size/tg->count : 0);
			for(int j=0; j < kNumTracesPerType; j++) {
				int traceIndex = tg->traces[j];
				if(traceIndex) {
					int size = traceTable[traceIndex].size;
					int count = traceTable[traceIndex].count;
					if(showSwept) {
						size = traceTable[traceIndex].sweepSize;
						count = traceTable[traceIndex].sweepCount;
					} else if(showTotal) {
						size = traceTable[traceIndex].totalSize;
						count = traceTable[traceIndex].totalCount;
					}
					GCDebugMsg(false, "\t %3.1f%% - %d kb - %d items - ", PERCENT(tg->size, size), size>>10, count);
					PrintStackTraceByIndex(traceIndex);
				}
			}
		}

		GCHashtableIterator iter2(&typeTable);
		while(iter2.nextKey() != NULL)
			delete (TypeGroup*)iter2.value();
	}
 
	size_t DebugSize()
	{ 
	#ifdef MMGC_64BIT
		// Our writeback pointer is 8 bytes so we need to round up to the next 8 byte
		// size.  (only 5 DWORDS are used)
		return 6 * sizeof(int); 
	#else
		return 4 * sizeof(int); 
	#endif
	}

	/* 
	 * allocate the memory such that we can detect underwrites, overwrites and remember
	 * the allocation stack in case of a leak.   Memory is laid out like so:
	 *
	 * first four bytes == size / 4 
	 * second four bytes == stack trace index
	 * size data bytes
	 * 4 bytes == 0xdeadbeef
	 * last 4/8 bytes - writeback pointer
	 *
	 * Its important that the stack trace index is not stored in the first 4 bytes,
	 * it enables the leak detection to work see ~FixedAlloc.  Underwrite detection isn't
	 * perfect, an assert will be fired if the stack table index is invalid (greater than
	 * the table size or to an unused table entry) or it the size gets mangled and the
	 * end tag isn't at mem+size.  
	*/
	void *DebugDecorate(void *item, size_t size, int skip)
	{
		if (!item) return NULL;

		static void *lastItem = 0;
		static int lastTrace = 0;

#ifndef _MAC
		if(lastItem)
		{
			// this guy might be deleted so swallow access violations
			try {
				traceTable[lastTrace].vtable = *(int*)lastItem;
			} catch(...) {}
			lastItem = 0;
		}
#endif

		int traceIndex;

		// get index into trace table
		{
#ifdef GCHEAP_LOCK
			GCEnterCriticalSection lock(m_traceTableLock);
#endif
			traceIndex = GetStackTraceIndex(skip);
		}

		if(GCHeap::GetGCHeap()->enableMemoryProfiling && memtype)
		{
			// if an allocation is tagged with MMGC_MEM_TYPE its a sub
			// allocation of a "master" type and this flag prevents it
			// from contributing to the count so averages make more sense
			traceTable[traceIndex].lumped = true;
		}

		// subtract decoration space
		size -= DebugSize();

		ChangeSize(traceIndex, size);

		int *mem = (int*)item;
		// set up the memory
		*mem++ = size;
		*mem++ = traceIndex;
		void *ret = mem;
		mem += (size>>2);
		*mem++ = 0xdeadbeef;
		*mem = 0;
	#ifdef MMGC_64BIT
		*(mem+1) = 0;
		*(mem+2) = 0;
	#endif	

		// save these off so we can save the vtable (which is assigned after memory is
		// allocated)
		if(GCHeap::GetGCHeap()->enableMemoryProfiling)
		{
			if(memtag || memtype) {
				if(memtag)
					traceTable[traceIndex].memtag = memtag;
				else
					traceTable[traceIndex].vtable =  *(int*)(void*)memtype;
				memtag = NULL;
				memtype = NULL;
			} else {
				lastTrace = traceIndex;
				lastItem = ret;
			}
		}

		return ret;
	}

	void DebugFreeHelper(void *item, int poison, int skip)
	{
		int *ip = (int*) item;
		int size = *ip;
		int traceIndex = *(ip+1);
		int *endMarker = ip + 2 + (size>>2);

		// clean up
		*ip = 0;
		ip += 2;

		// this can be called twice on some memory in inc gc 
		if(size == 0)
			return;

		if (*endMarker != (int32)0xdeadbeef)
		{
			// if you get here, you have a buffer overrun.  The stack trace about to
			// be printed tells you where the block was allocated from.  To find the
			// overrun, put a memory breakpoint on the location endMarker is pointing to.
			GCDebugMsg("Memory overwrite detected\n", false);
			PrintStackTraceByIndex(traceIndex);
			GCAssert(false);
		}
	
		{
#ifdef GCHEAP_LOCK
			GCEnterCriticalSection lock(m_traceTableLock);
#endif
			ChangeSize(traceIndex, -1 * size);
			if(poison == 0xba) {
				traceTable[traceIndex].sweepSize += size;	
				traceTable[traceIndex].sweepCount++;
			}
			traceIndex = GetStackTraceIndex(skip);
		}

		// whack the entire thing except the first 8 bytes,
		// the free list
		if(poison == 0xca || poison == 0xba)
			size = GC::Size(ip);
		else
			size = FixedMalloc::GetInstance()->Size(ip);

		// size is the non-Debug size, so add 4 to get last 4 bytes, don't
		// touch write back pointer space
		memset(ip, poison, size+4);
		// write stack index to ip (currently 3rd 4 bytes of item)			
		*ip = traceIndex;
	}

	void *DebugFree(void *item, int poison, int skip)
	{
		item = (int*) item - 2;
		DebugFreeHelper(item, poison, skip);
		return item;
	}

	void *DebugFreeReverse(void *item, int poison, int skip)
	{
		DebugFreeHelper(item,  poison, skip);
		item = (int*) item + 2;
		return item;
	}

	void ChangeSize(int traceIndex, int delta)
	{ 
#ifdef GCHEAP_LOCK
		GCEnterCriticalSection lock(m_traceTableLock);
#endif
		if(!enableTraces)
			return;
		traceTable[traceIndex].size += delta; 
		traceTable[traceIndex].count += (delta > 0) ? 1 : -1;
		GCAssert(traceTable[traceIndex].size >= 0);
		if(delta > 0) {
			traceTable[traceIndex].totalSize += delta; 
			traceTable[traceIndex].totalCount++; 
		}
	}

	unsigned int GetStackTraceIndex(int skip)
	{
		if(!enableTraces)
			return 0;
		int trace[kMaxTraceDepth]; // an array of pcs
		GetStackTrace(trace, kMaxTraceDepth, skip);

		// get index into trace table
		return LookupTrace(trace);
	}

	void DumpStackTraceHelper(int *trace)
	{
		if(!enableTraces)
			return;

		char out[2048];
		char *tp = out;
		for(int i=0; trace[i] != 0; i++) {
			char buff[256];
			GetInfoFromPC(trace[i], buff, 256);
			strcpy(tp, buff);
			tp += strlen(buff);
			*tp++ = ' ';		
		}
		*tp++ = '\n';
		*tp = '\0';

		GCDebugMsg(out, false);
	}

	void DumpStackTrace()
	{
		if(!enableTraces)
			return;
		int trace[kMaxTraceDepth];
		GetStackTrace(trace, kMaxTraceDepth, 1);
		DumpStackTraceHelper(trace);
	}
 
	void PrintStackTrace(const void *item)
	{ 
		if (item)
			PrintStackTraceByIndex(*((int*)item - 1)); 
	}

	void PrintStackTraceByIndex(int i)
	{
#ifdef GCHEAP_LOCK
		GCEnterCriticalSection lock(m_traceTableLock);
#endif
		if(i < kNumTraces)
			DumpStackTraceHelper(traceTable[i].ips);
	}
	
	void ChangeSizeForObject(void *object, int size)
	{
		int traceIndex = *(((int*)object)-1);
		ChangeSize(traceIndex, size);
	}

	void SetMemTag(const char *s)
	{
		if(memtag == NULL)
			memtag = s;
	}
	void SetMemType(void *s)
	{
		if(memtype == NULL)
			memtype = s;
	}
}
#endif
