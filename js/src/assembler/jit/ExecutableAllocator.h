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
#include "assembler/wtf/Assertions.h"

#include "jsapi.h"
#include "jsprvtd.h"
#include "jsvector.h"
#include "jslock.h"

#if WTF_PLATFORM_IPHONE
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#endif

#if WTF_PLATFORM_SYMBIAN
#include <e32std.h>
#endif

#if WTF_CPU_MIPS && WTF_PLATFORM_LINUX
#include <sys/cachectl.h>
#endif

#if WTF_PLATFORM_WINCE
// From pkfuncs.h (private header file from the Platform Builder)
#define CACHE_SYNC_ALL 0x07F
extern "C" __declspec(dllimport) void CacheRangeFlush(LPVOID pAddr, DWORD dwLength, DWORD dwFlags);
#endif

#define JIT_ALLOCATOR_PAGE_SIZE (ExecutableAllocator::pageSize)
#if WTF_PLATFORM_WIN_OS || WTF_PLATFORM_WINCE
/*
 * In practice, VirtualAlloc allocates in 64K chunks. (Technically, it
 * allocates in page chunks, but the starting address is always a multiple
 * of 64K, so each allocation uses up 64K of address space.
 */
# define JIT_ALLOCATOR_LARGE_ALLOC_SIZE (ExecutableAllocator::pageSize * 16)
#else
# define JIT_ALLOCATOR_LARGE_ALLOC_SIZE (ExecutableAllocator::pageSize * 4)
#endif

#if ENABLE_ASSEMBLER_WX_EXCLUSIVE
#define PROTECTION_FLAGS_RW (PROT_READ | PROT_WRITE)
#define PROTECTION_FLAGS_RX (PROT_READ | PROT_EXEC)
#define INITIAL_PROTECTION_FLAGS PROTECTION_FLAGS_RX
#else
#define INITIAL_PROTECTION_FLAGS (PROT_READ | PROT_WRITE | PROT_EXEC)
#endif

namespace JSC {

// Something included via windows.h defines a macro with this name,
// which causes the function below to fail to compile.
#ifdef _MSC_VER
# undef max
#endif

const size_t OVERSIZE_ALLOCATION = size_t(-1);

inline size_t roundUpAllocationSize(size_t request, size_t granularity)
{
    if ((std::numeric_limits<size_t>::max() - granularity) <= request)
        return OVERSIZE_ALLOCATION;
    
    // Round up to next page boundary
    size_t size = request + (granularity - 1);
    size = size & ~(granularity - 1);
    JS_ASSERT(size >= request);
    return size;
}

}

#if ENABLE_ASSEMBLER

namespace JSC {

  // These are reference-counted. A new one (from the constructor or create)
  // starts with a count of 1. 
  class ExecutablePool {
private:
    struct Allocation {
        char* pages;
        size_t size;
#if WTF_PLATFORM_SYMBIAN
        RChunk* chunk;
#endif
    };
    typedef js::Vector<Allocation, 2 ,js::SystemAllocPolicy > AllocationList;

    // Reference count for automatic reclamation.
    jsrefcount m_refCount;

public:
      // It should be impossible for us to roll over, because only small
      // pools have multiple holders, and they have one holder per chunk
      // of generated code, and they only hold 16KB or so of code.
      void addRef() { JS_ATOMIC_INCREMENT(&m_refCount); }
      void release() { 
	  JS_ASSERT(m_refCount != 0);
	  if (JS_ATOMIC_DECREMENT(&m_refCount) == 0) 
	      delete this; 
      }

    //static PassRefPtr<ExecutablePool> create(size_t n)
    static ExecutablePool* create(size_t n)
    {
        ExecutablePool *pool = new ExecutablePool(n);
        if (!pool->m_freePtr) {
            delete pool;
            return NULL;
        }
        return pool;
    }

    void* alloc(size_t n)
    {
        JS_ASSERT(m_freePtr <= m_end);

        // Round 'n' up to a multiple of word size; if all allocations are of
        // word sized quantities, then all subsequent allocations will be aligned.
        n = roundUpAllocationSize(n, sizeof(void*));
        if (n == OVERSIZE_ALLOCATION)
            return NULL;

        if (static_cast<ptrdiff_t>(n) < (m_end - m_freePtr)) {
            void* result = m_freePtr;
            m_freePtr += n;
            return result;
        }

        // Insufficient space to allocate in the existing pool
        // so we need allocate into a new pool
        return poolAllocate(n);
    }
    
    ~ExecutablePool()
    {
        Allocation* end = m_pools.end();
        for (Allocation* ptr = m_pools.begin(); ptr != end; ++ptr)
            ExecutablePool::systemRelease(*ptr);
    }

    size_t available() const { return (m_pools.length() > 1) ? 0 : m_end - m_freePtr; }

private:
    // On OOM, this will return an Allocation where pages is NULL.
    static Allocation systemAlloc(size_t n);
    static void systemRelease(const Allocation& alloc);

    ExecutablePool(size_t n);

    void* poolAllocate(size_t n);

    char* m_freePtr;
    char* m_end;
    AllocationList m_pools;
};

class ExecutableAllocator {
    enum ProtectionSeting { Writable, Executable };

    // Initialization can fail so we use a create method instead.
    ExecutableAllocator() {}
public:
    static size_t pageSize;

    // Returns NULL on OOM.
    static ExecutableAllocator *create()
    {
        ExecutableAllocator *allocator = new ExecutableAllocator();
        if (!allocator)
            return allocator;

        if (!pageSize)
            intializePageSize();
        allocator->m_smallAllocationPool = ExecutablePool::create(JIT_ALLOCATOR_LARGE_ALLOC_SIZE);
        if (!allocator->m_smallAllocationPool) {
            delete allocator;
            return NULL;
        }
    }

    ~ExecutableAllocator() { delete m_smallAllocationPool; }

    // poolForSize returns reference-counted objects. The caller owns a reference
    // to the object; i.e., poolForSize increments the count before returning the
    // object.

    ExecutablePool* poolForSize(size_t n)
    {
        // Try to fit in the existing small allocator
        if (n < m_smallAllocationPool->available()) {
	    m_smallAllocationPool->addRef();
            return m_smallAllocationPool;
	}

        // If the request is large, we just provide a unshared allocator
        if (n > JIT_ALLOCATOR_LARGE_ALLOC_SIZE)
            return ExecutablePool::create(n);

        // Create a new allocator
        ExecutablePool* pool = ExecutablePool::create(JIT_ALLOCATOR_LARGE_ALLOC_SIZE);
        if (!pool)
            return NULL;
  	    // At this point, local |pool| is the owner.

        // If the new allocator will result in more free space than in
        // the current small allocator, then we will use it instead
        if ((pool->available() - n) > m_smallAllocationPool->available()) {
	        m_smallAllocationPool->release();
            m_smallAllocationPool = pool;
	        pool->addRef();
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
        // "start" points to the fisrt byte of the cache line.
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
#elif WTF_CPU_ARM_THUMB2 && WTF_PLATFORM_IPHONE
    static void cacheFlush(void* code, size_t size)
    {
        sys_dcache_flush(code, size);
        sys_icache_invalidate(code, size);
    }
#elif WTF_CPU_ARM_THUMB2 && WTF_PLATFORM_LINUX
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
#elif WTF_PLATFORM_SYMBIAN
    static void cacheFlush(void* code, size_t size)
    {
        User::IMB_Range(code, static_cast<char*>(code) + size);
    }
#elif WTF_CPU_ARM_TRADITIONAL && WTF_PLATFORM_LINUX && WTF_COMPILER_RVCT
    static __asm void cacheFlush(void* code, size_t size);
#elif WTF_CPU_ARM_TRADITIONAL && (WTF_PLATFORM_LINUX || WTF_PLATFORM_ANDROID) && WTF_COMPILER_GCC
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
#elif WTF_PLATFORM_WINCE
    static void cacheFlush(void* code, size_t size)
    {
        CacheRangeFlush(code, size, CACHE_SYNC_ALL);
    }
#else
    #error "The cacheFlush support is missing on this platform."
#endif

private:

#if ENABLE_ASSEMBLER_WX_EXCLUSIVE
    static void reprotectRegion(void*, size_t, ProtectionSeting);
#endif

    ExecutablePool* m_smallAllocationPool;
    static void intializePageSize();
};

// This constructor can fail due to OOM. If it does, m_freePtr will be
// set to NULL. 
inline ExecutablePool::ExecutablePool(size_t n) : m_refCount(1)
{
    size_t allocSize = roundUpAllocationSize(n, JIT_ALLOCATOR_PAGE_SIZE);
    if (allocSize == OVERSIZE_ALLOCATION) {
        m_freePtr = NULL;
        return;
    }
    Allocation mem = systemAlloc(allocSize);
    if (!mem.pages) {
        m_freePtr = NULL;
        return;
    }
    if (!m_pools.append(mem)) {
        systemRelease(mem);
        m_freePtr = NULL;
        return;
    }
    m_freePtr = mem.pages;
    m_end = m_freePtr + allocSize;
}

inline void* ExecutablePool::poolAllocate(size_t n)
{
    size_t allocSize = roundUpAllocationSize(n, JIT_ALLOCATOR_PAGE_SIZE);
    if (allocSize == OVERSIZE_ALLOCATION)
        return NULL;
    
    Allocation result = systemAlloc(allocSize);
    if (!result.pages)
        return NULL;
    
    JS_ASSERT(m_end >= m_freePtr);
    if ((allocSize - n) > static_cast<size_t>(m_end - m_freePtr)) {
        // Replace allocation pool
        m_freePtr = result.pages + n;
        m_end = result.pages + allocSize;
    }

    m_pools.append(result);
    return result.pages;
}

}

#endif // ENABLE(ASSEMBLER)

#endif // !defined(ExecutableAllocator)
