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

#ifndef __GC__
#define __GC__



#if defined MMGC_IA32

#ifdef WIN32
// save all registers: they might have pointers in them.  In theory, only
// need to save callee-saved regs.  In practice, saving three extra pointers
// is cheap insurance.
#define MMGC_GET_STACK_EXENTS(_gc, _stack, _size) \
		int save1,save2,save3,save4,save5,save6,save7;\
		__asm mov save1, eax \
		__asm mov save2, ebx \
		__asm mov save3, ecx \
		__asm mov save4, edx \
		__asm mov save5, ebp \
		__asm mov save6, esi \
		__asm mov save7, edi \
		__asm { mov _stack,esp } ;\
		MEMORY_BASIC_INFORMATION __mib;\
		VirtualQuery(_stack, &__mib, sizeof(MEMORY_BASIC_INFORMATION)); \
	    _size = __mib.RegionSize - ((uintptr) _stack - (uintptr)__mib.BaseAddress);

#else 
#define MMGC_GET_STACK_EXENTS(_gc, _stack, _size) \
		do { \
		volatile auto int save1,save2,save3,save4,save5,save6,save7;\
		asm("movl %%eax,%0" : "=r" (save1));\
		asm("movl %%ebx,%0" : "=r" (save2));\
		asm("movl %%ecx,%0" : "=r" (save3));\
		asm("movl %%edx,%0" : "=r" (save4));\
		asm("movl %%ebp,%0" : "=r" (save5));\
		asm("movl %%esi,%0" : "=r" (save6));\
		asm("movl %%edi,%0" : "=r" (save7));\
		asm("movl %%esp,%0" : "=r" (_stack));\
		_size = (uintptr)_gc->GetStackTop() - (uintptr)_stack;	} while (0)
#endif 

#elif defined MMGC_AMD64
// 64bit - r8-r15?
#define MMGC_GET_STACK_EXENTS(_gc, _stack, _size) \
		do { \
		volatile auto int64 save1,save2,save3,save4,save5,save6,save7;\
		asm("mov %%rax,%0" : "=r" (save1));\
		asm("mov %%rbx,%0" : "=r" (save2));\
		asm("mov %%rcx,%0" : "=r" (save3));\
		asm("mov %%rdx,%0" : "=r" (save4));\
		asm("mov %%rbp,%0" : "=r" (save5));\
		asm("mov %%rsi,%0" : "=r" (save6));\
		asm("mov %%rdi,%0" : "=r" (save7));\
		asm("mov %%rsp,%0" : "=r" (_stack));\
		_size = (uintptr)_gc->GetStackTop() - (uintptr)_stack;	} while (0)	

#elif defined MMGC_PPC

/* On PowerPC, we need to mark the PPC non-volatile registers,
 * since they may contain register variables that aren't
 * anywhere on the machine stack. */
// TPR - increasing to 20 causes this code to not get optimized out

#ifdef __GNUC__

#define MMGC_GET_STACK_EXENTS(_gc, _stack, _size) \
		int __ppcregs[20]; \
		asm("stmw r13,0(%0)" : : "b" (__ppcregs));\
		_stack = __ppcregs;\
		void *__stackBase;\
		asm ("mr r3,r1\n"\
		     "StackBaseLoop%1%2: mr %0,r3\n"	\
			"lwz r3,0(%0)\n"\
		        "rlwinm r3,r3,0,0,30\n"\
			"cmpi cr0,r3,0\n"\
		     "bne StackBaseLoop%1%2" : "=b" (__stackBase) : "i" (__FILE__), "i" (__LINE__) : "r3"); \
		_size = (uintptr) __stackBase - (uintptr) _stack;

#else

#define MMGC_GET_STACK_EXENTS(_gc, _stack, _size) \
		int __ppcregs[20]; \
		asm("stmw r13,0(%0)" : : "b" (__ppcregs));\
		_stack = __ppcregs;\
		void *__stackBase;\
		asm ("mr r3,r1\n"\
			"StackBaseLoop: mr %0,r3\n"\
			"lwz r3,0(%0)\n"\
		        "rlwinm r3,r3,0,0,30\n"\
			"cmpi cr0,r3,0\n"\
		     "bne StackBaseLoop" : "=b" (__stackBase) : : "r3"); \
		_size = (uintptr) __stackBase - (uintptr) _stack;

#endif

#elif defined MMGC_ARM

// Store nonvolatile registers r4-r10
// Find stack pointer
#define MMGC_GET_STACK_EXENTS(_gc, _stack, _size) \
		int regs[7];\
		asm("stmia %0,{r4-r10}" : : "r" (regs));\
		asm("mov %0,sp" : "=r" (_stack));\
		_size = (uintptr)StackTop - (uintptr)_stack;

#endif

#ifdef DEBUGGER
namespace avmplus
{
	class AvmCore;
}
#endif

namespace MMgc
{
	/**
	 * GCRoot is root in the reachability graph, it contains a pointer a size 
	 * and will be searched for things.  
	 */
	class GCRoot
	{
		friend class GC;
	public:
		/** subclassing constructor */
		GCRoot(GC *gc);
		/** general constructor */
		GCRoot(GC *gc, const void *object, size_t size);
		virtual ~GCRoot();

		// override new and delete so we can know the objects extents (via FixedMalloc::Size())
        void *operator new(size_t size)
        {
			return FixedMalloc::GetInstance()->Alloc(size);
        }
        
		void operator delete (void *object)
		{
			FixedMalloc::GetInstance()->Free(object);
		}

		const void *Get() const { return object; }
		void Set(const void *object, size_t size);

		GC *GetGC() const { return gc; }
		/** if your object goes away after the GC is deleted this can be useful */
		void Destroy();
	
	private:
		FixedMalloc* fm;
		GC * gc;
		GCRoot *next;
		GCRoot *prev;
		const void *object;
		size_t size;

		GCWorkItem GetWorkItem() const { return GCWorkItem(object, size, false); }
	};

	/**
	 * GCCallback is an interface that allows the application to get
	 * callbacks at interesting GC points.
	 */
	class GCCallback
	{
		friend class GC;
		friend class ZCT;
	public:
		GCCallback(GC *gc);
		virtual ~GCCallback();
		
		GC *GetGC() const { return gc; }
		/** if your object goes away after the GC is deleted this can be useful */
		void Destroy();

		/**
		 * This method is invoked after all marking and before any
		 * sweeping, useful for bookkeeping based on whether things
		 * got marked
		 */
		virtual void presweep() {}

		/**
		 * This method is invoked after all sweeping
		 */
		virtual void postsweep() {}

		/**
		 * This method is called whenever the collector decides to expand the heap
		 */
		virtual void heapgrew() {}

#ifdef MMGC_DRC
		// called before a ZCT reap begins
		virtual void prereap() {}

		// called after a ZCT reap completes
		virtual void postreap() {}
#endif

		/**
		 * This method is called before an RC object is reaped
		 */
		virtual void prereap(void* /*rcobj*/) {}

#ifdef DEBUGGER
		virtual void log(const char* /*str*/) {}

		virtual void startGCActivity() {}	
		virtual void stopGCActivity() {}
		// negative when blocks are reclaimed
		virtual void allocActivity(int /*blocks*/) {}
#endif

	private:
		GC *gc;
		GCCallback *nextCB;
		GCCallback *prevCB;
	};

	template <class T>
	class GCHiddenPointer
	{
	public:
		GCHiddenPointer(T obj) { set(obj); }
		operator T() const { return (T) (low16|high16<<16);	 }
		T operator=(T tNew) 
		{ 
			set(tNew); 
			return (T)this; 
		}
		T operator->() const { return (T) (low16|high16<<16); }

	private:
		// private to prevent its use and someone adding it, GCC creates
		// WriteBarrier's on the stack with it
		GCHiddenPointer(const GCHiddenPointer<T>& toCopy) { GCAssert(false); }
		
		void set(T obj) 
		{
			uint32 p = (uint32)obj;
			high16 = p >> 16;
			low16 = p & 0x0000ffff;
		}
		uint32 high16;
		uint32 low16;
	};

	/**
	 * The Zero Count Table used by DRC.
	 */
	class ZCT
	{
		friend class GC;
		// how many items there have to be to trigger a Reap
		static const int ZCT_REAP_THRESHOLD;

		// size of table in pages
		static const int ZCT_START_SIZE;
	public:
		ZCT(GCHeap *heap);
		~ZCT();
		void Add(RCObject *obj);
		void Remove(RCObject *obj);
		void Reap();
	private:
		// for MMGC_GET_STACK_EXENTS
		uintptr StackTop;

		GC *gc;

		// in pages
		int zctSize;

		// the zero count table
		RCObject **zct;

		// index to the end
		RCObject **zctNext;

		// freelist of open slots
		RCObject **zctFreelist;

		// during a reap where we are
		int zctIndex;

		// during a reap becomes zctNext
		int nextPinnedIndex;

		int count;
		int zctReapThreshold;

		// are we reaping the zct?
		bool reaping;

		void PinStackObjects(const void *start, size_t len);
		bool IsZCTFreelist(RCObject **obj)
		{
			return obj >= zct && obj < (RCObject**)(zct+zctSize/sizeof(RCObject*));
		}
	};

	/**
	 * This is a general-purpose garbage collector used by the Flash Player.
	 * Application code must implement the GCRoot interface to mark all
	 * reachable data.  Unreachable data is automatically destroyed.
	 * Objects may optionally be finalized by subclassing the GCObject
	 * interface.
	 *
	 * This garbage collector is intended to be modular, such that
	 * it can be used in other products or replaced with a different
	 * collector in the future.
	 *
	 * Memory allocation and garbage collection strategy:
	 * Mark and sweep garbage collector using fixed size classes
	 * implemented by the GCAlloc class.  Memory blocks are obtained
	 * from the OS via the GCHeap heap manager class.
	 *
	 * When an allocation fails because a suitable memory block is
	 * not available, the garbage collector decides either to garbage
	 * collect to free up space, or to expand the heap.  The heuristic
	 * used to make the decision to collect or expand is taken from the
	 * Boehm-Demers-Weiser (BDW) garbage collector and memory allocator.
	 * The BDW algorithm is (pseudo-code):
	 *
	 *    if (allocs since collect >= heap size / FSD)
	 *      collect
	 *    else
	 *      expand(request size + heap size / FSD)
	 *
	 * The FSD is the "Free Space Divisor."  For the first cut, I'm trying
	 * 4.  TODO: Try various FSD values against real Player workloads
	 * to find the optimum value.
	 *
	 */
	class GC : public GCAllocObject
	{
		friend class GCRoot;
		friend class GCCallback;
		friend class GCAlloc;
		friend class GCLargeAlloc;
		friend class RCObject;
		friend class GCInterval;
		friend class ZCT;
	public:

		/**
		 * If you need context vars use this!
		 */
		enum
		{
			GCV_COREPLAYER,
			GCV_AVMCORE,
			GCV_COUNT
		};
		void *GetGCContextVariable(int var) const { return m_contextVars[var]; }
		void SetGCContextVariable(int var, void *val) { m_contextVars[var] = val; }
		
#ifdef DEBUGGER
		avmplus::AvmCore *core() const { return (avmplus::AvmCore*)GetGCContextVariable(GCV_AVMCORE); }
#endif

		/**
		 * greedy is a debugging flag.  When set, every allocation
		 * will cause a garbage collection.  This makes code run
		 * abysmally slow, but can be useful for detecting mark
		 * bugs.
		 */
		bool greedy;

		/**
		 * nogc is a debugging flag.  When set, garbage collection
		 * never happens.
		 */
		bool nogc;

		/**
	      * findUnmarkedPointers is a debugging flag, only 
		  */
		bool findUnmarkedPointers;

		/**
		* turns on code that does a trace before reaping zero count object and asserting on
		* any objects that get marked, debug builds only
		*/
		bool validateDefRef;		
		bool keepDRCHistory;

		/**
		 * incremental space divisor
		 */
		int ISD;

		const static size_t collectThreshold;

		bool gcstats;

		bool dontAddToZCTDuringCollection;

#ifdef _DEBUG
		bool incrementalValidation;
		bool incrementalValidationPedantic;
#endif

		bool incremental;

#ifdef _DEBUG
		/**
		 * turn on memory profiling
		 */
		const static bool enableMemoryProfiling;
#endif

		// -- Interface
		GC(GCHeap *heap);
		~GC();
		
		/**
		 * Causes an immediate garbage collection
		 */
		void Collect();

		/**
		* flags to be passed as second argument to alloc
		*/
		enum AllocFlags
		{
			kZero=1,
			kContainsPointers=2,
			kFinalize=4,
			kRCObject=8
		};

		enum PageType
		{
			kNonGC = 0,
			kGCAllocPage = 1,
			kGCLargeAllocPageRest = 2,
			kGCLargeAllocPageFirst = 3
		};

		/**
		 * Main interface for allocating memory.  Default flags is no
		 * finalization, contains pointers is set and zero is set
		 */
		void *Alloc(size_t size, int flags=0, int skip=3);

		
		/**
		 * overflow checking way to call Alloc for a # of n size'd items,
		 * all instance of Alloc(num*sizeof(thing)) should be replaced with:
		 * Calloc(num, sizeof(thing))
		 */
		void *Calloc(size_t num, size_t elsize, int flags=0, int skip=3);

		/**
		 * One can free a GC allocated pointer, this will throw an assertion
		 * if called during the Sweep phase (ie via a finalizer) it can only be
		 * used outside the scope of a collection
		 */
		void Free(void *ptr);

		/**
		 * return the size of a piece of memory, may be bigger than what was asked for
		 */
		static size_t Size(const void *ptr)
		{
			GCAssert(GetGC(ptr)->IsGCMemory(ptr));			
			size_t size = GCLargeAlloc::GetBlockHeader(ptr)->usableSize;
			size -= DebugSize();
			return size;

		}

		/**
		 * Tracers should employ GetMark and SetMark to
		 * set the mark bits during the mark pass.
		 */
		static int GetMark(const void *item)
		{
			item = GetRealPointer(item);
			if (GCLargeAlloc::IsLargeBlock(item)) {
				return GCLargeAlloc::GetMark(item);
			} else {
				return GCAlloc::GetMark(item);
			}
		}

		static int SetMark(const void *item)
		{
			GCAssert(item != NULL);
#ifdef MEMORY_INFO
			GC *gc = GetGC(item);	
			item = GetRealPointer(item);
			GCAssert(gc->IsPointerToGCPage(item));
#endif 			
			if (GCLargeAlloc::IsLargeBlock(item)) {
				return GCLargeAlloc::SetMark(item);
			} else {
				return GCAlloc::SetMark(item);
			}
		}
		
		void SetQueued(const void *item)
		{
#ifdef MEMORY_INFO
			item = GetRealPointer(item);
			GCAssert(IsPointerToGCPage(item));
#endif 			
			if (GCLargeAlloc::IsLargeBlock(item)) {
				GCLargeAlloc::SetQueued(item);
			} else {
				GCAlloc::SetQueued(item);
			}
		}

		static void ClearFinalized(const void *item)
		{
#ifdef MEMORY_INFO
			GC *gc = GetGC(item);	
			item = GetRealPointer(item);
			GCAssert(gc->IsPointerToGCPage(item));
#endif 			
			if (GCLargeAlloc::IsLargeBlock(item)) {
				GCLargeAlloc::ClearFinalized(item);
			} else {
				GCAlloc::ClearFinalized(item);
			}
		}

		static void SetFinalize(const void *item)
		{
#ifdef MEMORY_INFO
			GC *gc = GetGC(item);	
			item = GetRealPointer(item);
			GCAssert(gc->IsPointerToGCPage(item));
#endif 			
			if (GCLargeAlloc::IsLargeBlock(item)) {
				GCLargeAlloc::SetFinalize(item);
			} else {
				GCAlloc::SetFinalize(item);
			}
		}

		static int IsFinalized(const void *item)
		{
#ifdef MEMORY_INFO
			GC *gc = GetGC(item);	
			item = GetRealPointer(item);
			GCAssert(gc->IsPointerToGCPage(item));
#endif 			
			if (GCLargeAlloc::IsLargeBlock(item)) {
				return GCLargeAlloc::IsFinalized(item);
			} else {
				return GCAlloc::IsFinalized(item);
			}
		}

		static int HasWeakRef(const void *item)
		{
#ifdef MEMORY_INFO
			GC *gc = GetGC(item);	
			item = GetRealPointer(item);
			GCAssert(gc->IsPointerToGCPage(item));
#endif 			
			if (GCLargeAlloc::IsLargeBlock(item)) {
				return GCLargeAlloc::HasWeakRef(item);
			} else {
				return GCAlloc::HasWeakRef(item);
			}
		}

		/**
		 * Utility function: Returns the GC object
		 * associated with an allocated object
		 */
		static GC* GetGC(const void *item)
		{
			GC **gc = (GC**) ((uintptr)item&~0xfff);
			return *gc;
		}

		/**
		 * Used by sub-allocators to obtain memory
		 */
		void* AllocBlock(int size, int pageType, bool zero=true);
		void FreeBlock(void *ptr, uint32 size);

		GCHeap *GetGCHeap() const { return heap; }

#ifdef MMGC_DRC
		void ReapZCT() { zct.Reap(); }
		bool Reaping() { return zct.reaping; }
#ifdef _DEBUG
		void RCObjectZeroCheck(RCObject *);
#endif
#endif

		/**
		 * debugging tool
	     */
		bool IsPointerToGCPage(const void *item);

		/**
		 * Do as much marking as possible in time milliseconds
		 */
		void IncrementalMark(uint32 time=10);

		/**
		 * Are we currently marking
		 */
		bool IncrementalMarking() { return marking; }

		// a magical write barriers that finds the container's address and the GC, just
		// make sure address is a pointer to a GC page, only used by WB smart pointers
		static void WriteBarrier(const void *address, const void *value);
		static void WriteBarrierNoSub(const void *address, const void *value);

		void writeBarrier(const void *container, const void *address, const void *value)
		{
			GCAssert(IsPointerToGCPage(container));
			GCAssert(((uintptr)address & 3) == 0);
			GCAssert(address >= container);
			GCAssert(address < (char*)container + Size(container));

			WriteBarrierNoSubstitute(container, value);
			WriteBarrierWrite(address, value);
		}

		// optimized version with no RC checks or pointer masking
		void writeBarrierRC(const void *container, const void *address, const void *value);

		/**
		Write barrier when the value could be a pointer with anything in the lower 3 bits
		FIXME: maybe assert that the lower 3 bits are either zero or a pointer type signature,
		this would require the application to tell us what bit patterns are pointers.
		 */
		__forceinline void WriteBarrierNoSubstitute(const void *container, const void *value)
		{
			WriteBarrierTrap(container, (const void*)((uintptr)value&~7));
		}
			
		/**
		 AVM+ write barrier, valuePtr is known to be pointer and the caller
		does the write
		*/
		__forceinline void WriteBarrierTrap(const void *container, const void *valuePtr)
		{
			GCAssert(IsPointerToGCPage(container));
			GCAssert(((uintptr)valuePtr&7) == 0);
			GCAssert(IsPointerToGCPage(container));
			if(marking && valuePtr && GetMark(container) && IsWhite(valuePtr))
			{
				TrapWrite(container, valuePtr);
			}
		}

		void ConservativeWriteBarrierNoSubstitute(const void *address, const void *value)
		{
			if(IsPointerToGCPage(address))
				WriteBarrierNoSubstitute(FindBeginning(address), value);
		}

	private:
		__forceinline void WriteBarrierWrite(const void *address, const void *value);
		__forceinline void WriteBarrierWriteRC(const void *address, const void *value);

	public:

		bool ContainsPointers(const void *item);

		void *FindBeginning(const void *gcItem)
		{
			GCAssert(gcItem != NULL);
			GCAssert(GetPageMapValue((uintptr)gcItem) != 0);
			void *realItem = NULL;
			int bits = GetPageMapValue((uintptr)gcItem);
			switch(bits)
			{
			case kGCAllocPage:
				realItem = GCAlloc::FindBeginning(gcItem);
				break;
			case kGCLargeAllocPageFirst:
				realItem = GCLargeAlloc::FindBeginning(gcItem);
				break;
			case kGCLargeAllocPageRest:
				while(bits == kGCLargeAllocPageRest)
				{
					gcItem = (void*) ((uintptr)gcItem - GCHeap::kBlockSize);
					bits = GetPageMapValue((uintptr)gcItem);
				}
				realItem = GCLargeAlloc::FindBeginning(gcItem);
				break;
			}		
#ifdef MEMORY_INFO
			realItem = GetUserPointer(realItem);
#endif
			return realItem;
		}

#ifdef MMGC_DRC
		bool IsRCObject(const void *);
#else 
		bool IsRCObject(const void *) { return false; }
#endif


		bool Collecting() const { return collecting; }

		bool IsGCMemory (const void *);

		bool IsQueued(const void *item);

		static uint64 GetPerformanceCounter();
		static uint64 GetPerformanceFrequency();
		
		static double duration(uint64 start) 
		{
			return (double(GC::GetPerformanceCounter() - start) * 1000) / GC::GetPerformanceFrequency();
		}

		void DisableThreadCheck() { disableThreadCheck = true; }
		
		uint64 t0;

#ifdef _DEBUG
		char *msgBuf;
		const char *msg();
#endif

		static uint64 ticksToMicros(uint64 ticks) { return (ticks*1000000)/GetPerformanceFrequency(); }

		static uint64 ticksToMillis(uint64 ticks) { return (ticks*1000)/GetPerformanceFrequency(); }

		// marking rate counter
		uint64 bytesMarked;
		uint64 markTicks;

		// calls to mark item
		uint32 lastStartMarkIncrementCount;
		uint32 markIncrements;
		uint32 marks;
        uint32 sweeps;

		uint32 numObjects;
		uint32 objSize;

		uint64 sweepStart;

		// if we hit zero in a marking incremental force completion in the next one
		bool hitZeroObjects;

		// called at some apropos moment from the mututor, ideally at a point
		// where all possible GC references will be below the current stack pointer
		// (ie in memory we can safely zero).  This will return right away if
		// called more than once between collections so it can safely be called
		// a lot without impacting performance
		void CleanStack(bool force=false);

		bool Destroying() { return destroying; }

		void ClearWeakRef(const void *obj);

		uintptr	GetStackTop() const;

	private:

		void gclog(const char *format, ...);

		const static int kNumSizeClasses = 40;

		// FIXME: only used for FixedAlloc, GCAlloc sized dynamically
		const static int kPageUsableSpace = 3936;

		uint32 *GetBits(int numBytes, int sizeClass);
		void FreeBits(uint32 *bits, int sizeClass)
		{
#ifdef _DEBUG
			for(int i=0, n=noPointersAllocs[sizeClass]->m_numBitmapBytes; i<n;i++) GCAssert(((uint8*)bits)[i] == 0);
#endif
			*(uint32**)bits = m_bitsFreelists[sizeClass];
			m_bitsFreelists[sizeClass] = bits;
		}
		uint32 *m_bitsFreelists[kNumSizeClasses];
		uint32 *m_bitsNext;

		GCHashtable weakRefs;
		friend class GCObject;
		friend class GCFinalizedObject;
		static GCWeakRef *GetWeakRef(const void *obj);

		bool destroying;

		// we track the heap size so if it expands due to fixed memory allocations
		// we can trigger a mark/sweep, other wise we can have lots of small GC 
		// objects allocating tons of fixed memory which results in huge heaps
		// and possible out of memory situations.
		size_t heapSizeAtLastAlloc;

		bool stackCleaned;
		const void *rememberedStackTop;

		// for external which does thread safe multi-thread AS execution
		bool disableThreadCheck;

		bool marking;
		GCStack<GCWorkItem> m_incrementalWork;
		void StartIncrementalMark();
		void FinishIncrementalMark();
		int IsWhite(const void *item);
		
		uint64 lastMarkTicks;
		uint64 lastSweepTicks;

		const static int16 kSizeClasses[kNumSizeClasses];		
		const static uint8 kSizeClassIndex[246];

		void *m_contextVars[GCV_COUNT];

		// bitmap for what pages are in use, 2 bits for every page
		// 0 - not in use
		// 1 - used by GCAlloc
		// 3 - used by GCLargeAlloc
		uintptr memStart;
		uintptr memEnd;

		size_t totalGCPages;

		unsigned char *pageMap;

		inline int GetPageMapValue(uintptr addr) const
		{
			uintptr index = (addr-memStart) >> 12;

			GCAssert(index >> 2 < 64 * GCHeap::kBlockSize);
			// shift amount to determine position in the byte (times 2 b/c 2 bits per page)
			uint32 shiftAmount = (index&0x3) * 2;
			// 3 ... is mask for 2 bits, shifted to the left by shiftAmount
			// finally shift back by shift amount to get the value 0, 1 or 3
			//return (pageMap[addr >> 2] & (3<<shiftAmount)) >> shiftAmount;
			return (pageMap[index >> 2] >> shiftAmount) & 3;
		}
		void SetPageMapValue(uintptr addr, int val);
		void ClearPageMapValue(uintptr addr);

		void MarkGCPages(void *item, uint32 numpages, int val);
		void UnmarkGCPages(void *item, uint32 numpages);
		
		/**
		 * Mark a region of memory, this will search all memory pointed to recursively
		 * and mark any GC Objects it finds
		 */
		void ConservativeMarkRegion(const void *base, size_t bytes);

		GCAlloc *containsPointersAllocs[kNumSizeClasses];
#ifdef MMGC_DRC
		GCAlloc *containsPointersRCAllocs[kNumSizeClasses];
#endif
		GCAlloc *noPointersAllocs[kNumSizeClasses];
		GCLargeAlloc *largeAlloc;
		GCHeap *heap;
		
		void* AllocBlockIncremental(int size, bool zero=true);
		void* AllocBlockNonIncremental(int size, bool zero=true);

		void ClearMarks();
#ifdef _DEBUG
		public:
		// sometimes useful for mutator to call this
		void Trace(const void *stackStart=NULL, size_t stackSize=0);
		private:
#else
		void Trace(const void *stackStart=NULL, size_t stackSize=0);
#endif

		void Finalize();
		void Sweep(bool force=false);
		void ForceSweep() { Sweep(true); }
		void Mark(GCStack<GCWorkItem> &work);
		void MarkQueueAndStack(GCStack<GCWorkItem> &work);
		void MarkItem(GCStack<GCWorkItem> &work)
		{
			GCWorkItem workitem = work.Pop();
			MarkItem(workitem, work);
		}
		void MarkItem(GCWorkItem &workitem, GCStack<GCWorkItem> &work);
		// Write barrier slow path

		void TrapWrite(const void *black, const void *white);

		unsigned int allocsSinceCollect;
		// The collecting flag prevents an unwanted recursive collection.
		bool collecting;
 
		bool finalizedValue;

		// list of pages to be swept, built up in Finalize
		void AddToSmallEmptyBlockList(GCAlloc::GCBlock *b)
		{
			b->next = smallEmptyPageList;
			smallEmptyPageList = b;
		}
		GCAlloc::GCBlock *smallEmptyPageList;
		
		// list of pages to be swept, built up in Finalize
		void AddToLargeEmptyBlockList(GCLargeAlloc::LargeBlock *lb)
		{
			lb->next = largeEmptyPageList;
			largeEmptyPageList = lb;
		}
		GCLargeAlloc::LargeBlock *largeEmptyPageList;
		
#ifdef GCHEAP_LOCK
		GCSpinLock m_rootListLock;
#endif

		GCRoot *m_roots;
		void AddRoot(GCRoot *root);
		void RemoveRoot(GCRoot *root);
		
		GCCallback *m_callbacks;
		void AddCallback(GCCallback *cb);
		void RemoveCallback(GCCallback *cb);

#ifdef MMGC_DRC
		// Deferred ref counting implementation
		ZCT zct;
		void AddToZCT(RCObject *obj) { zct.Add(obj); }
		// public for one hack from Interval.cpp - no one else should call
		// this out of the GC
public:
		void RemoveFromZCT(RCObject *obj) { zct.Remove(obj); }
private:
#endif

		static const void *Pointer(const void *p) { return (const void*)(((uintptr)p)&~7); }

#ifdef MEMORY_INFO
public:
		void DumpMemoryInfo();
private:
#endif

		void CheckThread();

		void PushWorkItem(GCStack<GCWorkItem> &stack, GCWorkItem item);

#ifdef DEBUGGER
		// sent for Collect (fullgc)
		// and Start/IncrementalMark/Sweep for incremental
		void StartGCActivity();
		void StopGCActivity();
		void AllocActivity(int blocks);
#endif

#ifdef _DEBUG
		void CheckFreelist(GCAlloc *gca);
		void CheckFreelists();

		int m_gcLastStackTrace;
		void UnmarkedScan(const void *mem, size_t size);
		// find unmarked pointers in entire heap
		void FindUnmarkedPointers();

		// methods for incremental verification

		// scan a region of memory for white pointers, used by FindMissingWriteBarriers
		void WhitePointerScan(const void *mem, size_t size);

		// scan all GC memory (skipping roots) If a GC object is black make sure
		// it has no white pointers, skip it if its grey.
		void FindMissingWriteBarriers();
#ifdef WIN32
		// store a handle to the thread that create the GC to ensure thread safety
		DWORD m_gcThread;
#endif
#endif

public:
#ifdef MEMORY_INFO
		void DumpBackPointerChain(void *o);

		// debugging routine that records who marked who, can be used to
		// answer the question, how did I get marked?  also could be used to
		// find false positives by verifying the back pointer chain gets back
		// to a GC root
		static void WriteBackPointer(const void *item, const void *container, size_t itemSize);
#endif
#ifdef _DEBUG
		// Dump a list of objects that have pointers to the given location.
		void WhosPointingAtMe(void* me, int recurseDepth=0, int currentDepth=0);
    	void ProbeForMatch(const void *mem, size_t size, uintptr value, int recurseDepth, int currentDepth);
#endif
	};

	// helper class to wipe out vtable pointer of members for DRC
	class Cleaner
	{
	public:
		Cleaner() {}
		// don't let myself move between objects
		Cleaner& operator=(const Cleaner& /*rhs*/) { return *this; }
		void set(const void * _v, size_t _size) { this->v = (int*)_v; this->size = _size; }
		~Cleaner() { 
			if(v) 
				memset(v, 0, size);
			v = 0; 
			size = 0;
		}
		int *v;
		size_t size;
	};
}

#endif /* __GC__ */
