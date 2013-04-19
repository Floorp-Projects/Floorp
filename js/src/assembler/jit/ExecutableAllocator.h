/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ExecutableAllocator_h
#define ExecutableAllocator_h

#include <stddef.h> // for ptrdiff_t
#include <limits>

#include "jsalloc.h"
#include "jsapi.h"
#include "jsprvtd.h"

#include "assembler/wtf/Assertions.h"
#include "js/HashTable.h"
#include "js/Vector.h"

#if WTF_CPU_SPARC
#ifdef linux  // bugzilla 502369
static void sync_instruction_memory(caddr_t v, u_int len)
{
    caddr_t end = v + len;
    caddr_t p = v;
    while (p < end) {
        asm("flush %0" : : "r" (p));
        p += 32;
    }
}
#else
extern  "C" void sync_instruction_memory(caddr_t v, u_int len);
#endif
#endif

#if WTF_OS_IOS
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#endif

#if WTF_OS_SYMBIAN
#include <e32std.h>
#endif

#if WTF_CPU_MIPS && WTF_OS_LINUX
#include <sys/cachectl.h>
#endif

#if ENABLE_ASSEMBLER_WX_EXCLUSIVE
#define PROTECTION_FLAGS_RW (PROT_READ | PROT_WRITE)
#define PROTECTION_FLAGS_RX (PROT_READ | PROT_EXEC)
#define INITIAL_PROTECTION_FLAGS PROTECTION_FLAGS_RX
#else
#define INITIAL_PROTECTION_FLAGS (PROT_READ | PROT_WRITE | PROT_EXEC)
#endif

#if ENABLE_ASSEMBLER

//#define DEBUG_STRESS_JSC_ALLOCATOR

namespace JS {
    struct CodeSizes;
}

namespace JSC {

  class ExecutableAllocator;

  enum CodeKind { ION_CODE, BASELINE_CODE, REGEXP_CODE, ASMJS_CODE, OTHER_CODE };

  // These are reference-counted. A new one starts with a count of 1.
  class ExecutablePool {

    friend class ExecutableAllocator;
private:
    struct Allocation {
        char* pages;
        size_t size;
#if WTF_OS_SYMBIAN
        RChunk* chunk;
#endif
    };

    ExecutableAllocator* m_allocator;
    char* m_freePtr;
    char* m_end;
    Allocation m_allocation;

    // Reference count for automatic reclamation.
    unsigned m_refCount;

    // Number of bytes currently used for Method and Regexp JIT code.
    size_t m_ionCodeBytes;
    size_t m_baselineCodeBytes;
    size_t m_asmJSCodeBytes;
    size_t m_regexpCodeBytes;
    size_t m_otherCodeBytes;

public:
    // Flag for downstream use, whether to try to release references to this pool.
    bool m_destroy;

    // GC number in which the m_destroy flag was most recently set. Used downstream to
    // remember whether m_destroy was computed for the currently active GC.
    size_t m_gcNumber;

    void release(bool willDestroy = false)
    {
        JS_ASSERT(m_refCount != 0);
        // XXX: disabled, see bug 654820.
        //JS_ASSERT_IF(willDestroy, m_refCount == 1);
        if (--m_refCount == 0)
            js_delete(this);
    }

    ExecutablePool(ExecutableAllocator* allocator, Allocation a)
      : m_allocator(allocator), m_freePtr(a.pages), m_end(m_freePtr + a.size), m_allocation(a),
        m_refCount(1), m_ionCodeBytes(0), m_baselineCodeBytes(0),
        m_asmJSCodeBytes(0), m_regexpCodeBytes(0), m_otherCodeBytes(0),
        m_destroy(false), m_gcNumber(0)
    { }

    ~ExecutablePool();

private:
    // It should be impossible for us to roll over, because only small
    // pools have multiple holders, and they have one holder per chunk
    // of generated code, and they only hold 16KB or so of code.
    void addRef()
    {
        JS_ASSERT(m_refCount);
        ++m_refCount;
    }

    void* alloc(size_t n, CodeKind kind)
    {
        JS_ASSERT(n <= available());
        void *result = m_freePtr;
        m_freePtr += n;

        switch (kind) {
          case ION_CODE:      m_ionCodeBytes      += n;        break;
          case BASELINE_CODE: m_baselineCodeBytes += n;        break;
          case ASMJS_CODE:    m_asmJSCodeBytes    += n;        break;
          case REGEXP_CODE:   m_regexpCodeBytes   += n;        break;
          case OTHER_CODE:    m_otherCodeBytes    += n;        break;
          default:            JS_NOT_REACHED("bad code kind"); break;
        }
        return result;
    }

    size_t available() const {
        JS_ASSERT(m_end >= m_freePtr);
        return m_end - m_freePtr;
    }
};

enum AllocationBehavior
{
    AllocationCanRandomize,
    AllocationDeterministic
};

class ExecutableAllocator {
    typedef void (*DestroyCallback)(void* addr, size_t size);
    enum ProtectionSetting { Writable, Executable };
    DestroyCallback destroyCallback;

public:
    explicit ExecutableAllocator(AllocationBehavior allocBehavior)
      : destroyCallback(NULL),
        allocBehavior(allocBehavior)
    {
        if (!pageSize) {
            pageSize = determinePageSize();
            /*
             * On Windows, VirtualAlloc effectively allocates in 64K chunks.
             * (Technically, it allocates in page chunks, but the starting
             * address is always a multiple of 64K, so each allocation uses up
             * 64K of address space.)  So a size less than that would be
             * pointless.  But it turns out that 64KB is a reasonable size for
             * all platforms.  (This assumes 4KB pages.)
             */
            largeAllocSize = pageSize * 16;
        }

        JS_ASSERT(m_smallPools.empty());
    }

    ~ExecutableAllocator()
    {
        for (size_t i = 0; i < m_smallPools.length(); i++)
            m_smallPools[i]->release(/* willDestroy = */true);
        // XXX: temporarily disabled because it fails;  see bug 654820.
        //JS_ASSERT(m_pools.empty());     // if this asserts we have a pool leak
    }

    void purge() {
        for (size_t i = 0; i < m_smallPools.length(); i++)
            m_smallPools[i]->release();

	m_smallPools.clear();
    }

    // alloc() returns a pointer to some memory, and also (by reference) a
    // pointer to reference-counted pool. The caller owns a reference to the
    // pool; i.e. alloc() increments the count before returning the object.
    void* alloc(size_t n, ExecutablePool** poolp, CodeKind type)
    {
        // Round 'n' up to a multiple of word size; if all allocations are of
        // word sized quantities, then all subsequent allocations will be
        // aligned.
        n = roundUpAllocationSize(n, sizeof(void*));
        if (n == OVERSIZE_ALLOCATION) {
            *poolp = NULL;
            return NULL;
        }

        *poolp = poolForSize(n);
        if (!*poolp)
            return NULL;

        // This alloc is infallible because poolForSize() just obtained
        // (found, or created if necessary) a pool that had enough space.
        void *result = (*poolp)->alloc(n, type);
        JS_ASSERT(result);
        return result;
    }

    void releasePoolPages(ExecutablePool *pool) {
        JS_ASSERT(pool->m_allocation.pages);
        if (destroyCallback)
            destroyCallback(pool->m_allocation.pages, pool->m_allocation.size);
        systemRelease(pool->m_allocation);
        JS_ASSERT(m_pools.initialized());
        m_pools.remove(m_pools.lookup(pool));   // this asserts if |pool| is not in m_pools
    }

    void sizeOfCode(JS::CodeSizes *sizes) const;

    void setDestroyCallback(DestroyCallback destroyCallback) {
        this->destroyCallback = destroyCallback;
    }

    void setRandomize(bool enabled) {
        allocBehavior = enabled ? AllocationCanRandomize : AllocationDeterministic;
    }

private:
    static size_t pageSize;
    static size_t largeAllocSize;
#if WTF_OS_WINDOWS
    static uint64_t rngSeed;
#endif

    static const size_t OVERSIZE_ALLOCATION = size_t(-1);

    static size_t roundUpAllocationSize(size_t request, size_t granularity)
    {
        // Something included via windows.h defines a macro with this name,
        // which causes the function below to fail to compile.
        #ifdef _MSC_VER
        # undef max
        #endif

        if ((std::numeric_limits<size_t>::max() - granularity) <= request)
            return OVERSIZE_ALLOCATION;
        
        // Round up to next page boundary
        size_t size = request + (granularity - 1);
        size = size & ~(granularity - 1);
        JS_ASSERT(size >= request);
        return size;
    }

    // On OOM, this will return an Allocation where pages is NULL.
    ExecutablePool::Allocation systemAlloc(size_t n);
    static void systemRelease(const ExecutablePool::Allocation& alloc);
    void *computeRandomAllocationAddress();

    ExecutablePool* createPool(size_t n)
    {
        size_t allocSize = roundUpAllocationSize(n, pageSize);
        if (allocSize == OVERSIZE_ALLOCATION)
            return NULL;

        if (!m_pools.initialized() && !m_pools.init())
            return NULL;

#ifdef DEBUG_STRESS_JSC_ALLOCATOR
        ExecutablePool::Allocation a = systemAlloc(size_t(4294967291));
#else
        ExecutablePool::Allocation a = systemAlloc(allocSize);
#endif
        if (!a.pages)
            return NULL;

        ExecutablePool *pool = js_new<ExecutablePool>(this, a);
        if (!pool) {
            systemRelease(a);
            return NULL;
        }
        m_pools.put(pool);
        return pool;
    }

public:
    ExecutablePool* poolForSize(size_t n)
    {
#ifndef DEBUG_STRESS_JSC_ALLOCATOR
        // Try to fit in an existing small allocator.  Use the pool with the
        // least available space that is big enough (best-fit).  This is the
        // best strategy because (a) it maximizes the chance of the next
        // allocation fitting in a small pool, and (b) it minimizes the
        // potential waste when a small pool is next abandoned.
        ExecutablePool *minPool = NULL;
        for (size_t i = 0; i < m_smallPools.length(); i++) {
            ExecutablePool *pool = m_smallPools[i];
            if (n <= pool->available() && (!minPool || pool->available() < minPool->available()))
                minPool = pool;
        }
        if (minPool) {
            minPool->addRef();
            return minPool;
        }
#endif

        // If the request is large, we just provide a unshared allocator
        if (n > largeAllocSize)
            return createPool(n);

        // Create a new allocator
        ExecutablePool* pool = createPool(largeAllocSize);
        if (!pool)
            return NULL;
  	    // At this point, local |pool| is the owner.

        if (m_smallPools.length() < maxSmallPools) {
            // We haven't hit the maximum number of live pools;  add the new pool.
            m_smallPools.append(pool);
            pool->addRef();
        } else {
            // Find the pool with the least space.
            int iMin = 0;
            for (size_t i = 1; i < m_smallPools.length(); i++)
                if (m_smallPools[i]->available() <
                    m_smallPools[iMin]->available())
                {
                    iMin = i;
                }

            // If the new allocator will result in more free space than the small
            // pool with the least space, then we will use it instead
            ExecutablePool *minPool = m_smallPools[iMin];
            if ((pool->available() - n) > minPool->available()) {
                minPool->release();
                m_smallPools[iMin] = pool;
                pool->addRef();
            }
        }

   	    // Pass ownership to the caller.
        return pool;
    }

#if ENABLE_ASSEMBLER_WX_EXCLUSIVE
    static void makeWritable(void* start, size_t size)
    {
        reprotectRegion(start, size, Writable);
    }

    static void makeExecutable(void* start, size_t size)
    {
        reprotectRegion(start, size, Executable);
    }
#else
    static void makeWritable(void*, size_t) {}
    static void makeExecutable(void*, size_t) {}
#endif


#if WTF_CPU_X86 || WTF_CPU_X86_64
    static void cacheFlush(void*, size_t)
    {
    }
#elif WTF_CPU_MIPS
    static void cacheFlush(void* code, size_t size)
    {
#if WTF_COMPILER_GCC && (GCC_VERSION >= 40300)
#if WTF_MIPS_ISA_REV(2) && (GCC_VERSION < 40403)
        int lineSize;
        asm("rdhwr %0, $1" : "=r" (lineSize));
        //
        // Modify "start" and "end" to avoid GCC 4.3.0-4.4.2 bug in
        // mips_expand_synci_loop that may execute synci one more time.
        // "start" points to the first byte of the cache line.
        // "end" points to the last byte of the line before the last cache line.
        // Because size is always a multiple of 4, this is safe to set
        // "end" to the last byte.
        //
        intptr_t start = reinterpret_cast<intptr_t>(code) & (-lineSize);
        intptr_t end = ((reinterpret_cast<intptr_t>(code) + size - 1) & (-lineSize)) - 1;
        __builtin___clear_cache(reinterpret_cast<char*>(start), reinterpret_cast<char*>(end));
#else
        intptr_t end = reinterpret_cast<intptr_t>(code) + size;
        __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(end));
#endif
#else
        _flush_cache(reinterpret_cast<char*>(code), size, BCACHE);
#endif
    }
#elif WTF_CPU_ARM && WTF_OS_IOS
    static void cacheFlush(void* code, size_t size)
    {
        sys_dcache_flush(code, size);
        sys_icache_invalidate(code, size);
    }
#elif WTF_CPU_ARM_THUMB2 && WTF_IOS
    static void cacheFlush(void* code, size_t size)
    {
        asm volatile (
            "push    {r7}\n"
            "mov     r0, %0\n"
            "mov     r1, %1\n"
            "movw    r7, #0x2\n"
            "movt    r7, #0xf\n"
            "movs    r2, #0x0\n"
            "svc     0x0\n"
            "pop     {r7}\n"
            :
            : "r" (code), "r" (reinterpret_cast<char*>(code) + size)
            : "r0", "r1", "r2");
    }
#elif WTF_OS_SYMBIAN
    static void cacheFlush(void* code, size_t size)
    {
        User::IMB_Range(code, static_cast<char*>(code) + size);
    }
#elif WTF_CPU_ARM_TRADITIONAL && WTF_OS_LINUX && WTF_COMPILER_RVCT
    static __asm void cacheFlush(void* code, size_t size);
#elif WTF_CPU_ARM_TRADITIONAL && (WTF_OS_LINUX || WTF_OS_ANDROID) && WTF_COMPILER_GCC
    static void cacheFlush(void* code, size_t size)
    {
        asm volatile (
            "push    {r7}\n"
            "mov     r0, %0\n"
            "mov     r1, %1\n"
            "mov     r7, #0xf0000\n"
            "add     r7, r7, #0x2\n"
            "mov     r2, #0x0\n"
            "svc     0x0\n"
            "pop     {r7}\n"
            :
            : "r" (code), "r" (reinterpret_cast<char*>(code) + size)
            : "r0", "r1", "r2");
    }
#elif WTF_CPU_SPARC
    static void cacheFlush(void* code, size_t size)
    {
        sync_instruction_memory((caddr_t)code, size);
    }
#endif

private:

#if ENABLE_ASSEMBLER_WX_EXCLUSIVE
    static void reprotectRegion(void*, size_t, ProtectionSetting);
#endif

    // These are strong references;  they keep pools alive.
    static const size_t maxSmallPools = 4;
    typedef js::Vector<ExecutablePool *, maxSmallPools, js::SystemAllocPolicy> SmallExecPoolVector;
    SmallExecPoolVector m_smallPools;

    // All live pools are recorded here, just for stats purposes.  These are
    // weak references;  they don't keep pools alive.  When a pool is destroyed
    // its reference is removed from m_pools.
    typedef js::HashSet<ExecutablePool *, js::DefaultHasher<ExecutablePool *>, js::SystemAllocPolicy>
            ExecPoolHashSet;
    ExecPoolHashSet m_pools;    // All pools, just for stats purposes.
    AllocationBehavior allocBehavior;

    static size_t determinePageSize();
};

}

#endif // ENABLE(ASSEMBLER)

#endif // !defined(ExecutableAllocator)

