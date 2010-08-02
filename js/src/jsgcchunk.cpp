/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2006-2008 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsgcchunk.h"

#ifdef XP_WIN
# include "jswin.h"

# ifdef _MSC_VER
#  pragma warning( disable: 4267 4996 4146 )
# endif

#elif defined(XP_MACOSX) || defined(DARWIN)

# include <libkern/OSAtomic.h>
# include <mach/mach_error.h>
# include <mach/mach_init.h>
# include <mach/vm_map.h>
# include <malloc/malloc.h>

#elif defined(XP_UNIX) || defined(XP_BEOS)

# include <unistd.h>
# include <sys/mman.h>

# ifndef MAP_NOSYNC
#  define MAP_NOSYNC    0
# endif

#endif

#ifdef XP_WIN

/*
 * On Windows CE < 6 we must use separated MEM_RESERVE and MEM_COMMIT
 * VirtualAlloc calls and we cannot use MEM_RESERVE to allocate at the given
 * address. So we use a workaround based on oversized allocation.
 */
# if defined(WINCE) && !defined(MOZ_MEMORY_WINCE6)

#  define JS_GC_HAS_MAP_ALIGN

static void
UnmapPagesAtBase(void *p)
{
    JS_ALWAYS_TRUE(VirtualFree(p, 0, MEM_RELEASE));
}

static void *
MapAlignedPages(size_t size, size_t alignment)
{
    JS_ASSERT(size % alignment == 0);
    JS_ASSERT(size >= alignment);

    void *reserve = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    if (!reserve)
        return NULL;

    void *p = VirtualAlloc(reserve, size, MEM_COMMIT, PAGE_READWRITE);
    JS_ASSERT(p == reserve);

    size_t mask = alignment - 1;
    size_t offset = (uintptr_t) p & mask;
    if (!offset)
        return p;

    /* Try to extend the initial allocation. */
    UnmapPagesAtBase(reserve);
    reserve = VirtualAlloc(NULL, size + alignment - offset, MEM_RESERVE,
                           PAGE_NOACCESS);
    if (!reserve)
        return NULL;
    if (offset == ((uintptr_t) reserve & mask)) {
        void *aligned = (void *) ((uintptr_t) reserve + alignment - offset);
        p = VirtualAlloc(aligned, size, MEM_COMMIT, PAGE_READWRITE);
        JS_ASSERT(p == aligned);
        return p;
    }

    /* over allocate to ensure we have an aligned region */
    UnmapPagesAtBase(reserve);
    reserve = VirtualAlloc(NULL, size + alignment, MEM_RESERVE, PAGE_NOACCESS);
    if (!reserve)
        return NULL;

    offset = (uintptr_t) reserve & mask;
    void *aligned = (void *) ((uintptr_t) reserve + alignment - offset);
    p = VirtualAlloc(aligned, size, MEM_COMMIT, PAGE_READWRITE);
    JS_ASSERT(p == aligned);

    return p;
}

static void
UnmapPages(void *p, size_t size)
{
    if (VirtualFree(p, 0, MEM_RELEASE))
        return;

    /* We could have used the over allocation. */
    JS_ASSERT(GetLastError() == ERROR_INVALID_PARAMETER);
    MEMORY_BASIC_INFORMATION info;
    VirtualQuery(p, &info, sizeof(info));

    UnmapPagesAtBase(info.AllocationBase);
}

# else /* WINCE */

static void *
MapPages(void *addr, size_t size)
{
    void *p = VirtualAlloc(addr, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    JS_ASSERT_IF(p && addr, p == addr);
    return p;
}

static void
UnmapPages(void *addr, size_t size)
{
    JS_ALWAYS_TRUE(VirtualFree(addr, 0, MEM_RELEASE));
}

# endif /* !WINCE */

#elif defined(XP_MACOSX) || defined(DARWIN)

static void *
MapPages(void *addr, size_t size)
{
    vm_address_t p;
    int flags;
    if (addr) {
        p = (vm_address_t) addr;
        flags = 0;
    } else {
        flags = VM_FLAGS_ANYWHERE;
    }

    kern_return_t err = vm_allocate((vm_map_t) mach_task_self(),
                                    &p, (vm_size_t) size, flags);
    if (err != KERN_SUCCESS)
        return NULL;

    JS_ASSERT(p);
    JS_ASSERT_IF(addr, p == (vm_address_t) addr);
    return (void *) p;
}

static void
UnmapPages(void *addr, size_t size)
{
    JS_ALWAYS_TRUE(vm_deallocate((vm_map_t) mach_task_self(),
                                 (vm_address_t) addr,
                                 (vm_size_t) size)
                   == KERN_SUCCESS);
}

#elif defined(XP_UNIX) || defined(XP_BEOS)

/* Required on Solaris 10. Might improve performance elsewhere. */
# if defined(SOLARIS) && defined(MAP_ALIGN)
#  define JS_GC_HAS_MAP_ALIGN

static void *
MapAlignedPages(size_t size, size_t alignment)
{
    /*
     * We don't use MAP_FIXED here, because it can cause the *replacement*
     * of existing mappings, and we only want to create new mappings.
     */
#ifdef SOLARIS
    void *p = mmap((caddr_t) alignment, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_NOSYNC | MAP_ALIGN | MAP_ANON, -1, 0);
#else
    void *p = mmap((void *) alignment, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_NOSYNC | MAP_ALIGN | MAP_ANON, -1, 0);
#endif
    if (p == MAP_FAILED)
        return NULL;
    return p;
}

# else /* JS_GC_HAS_MAP_ALIGN */

static void *
MapPages(void *addr, size_t size)
{
    /*
     * We don't use MAP_FIXED here, because it can cause the *replacement*
     * of existing mappings, and we only want to create new mappings.
     */
    void *p = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                   -1, 0);
    if (p == MAP_FAILED)
        return NULL;
    if (addr && p != addr) {
        /* We succeeded in mapping memory, but not in the right place. */
        JS_ALWAYS_TRUE(munmap(p, size) == 0);
        return NULL;
    }
    return p;
}

# endif /* !JS_GC_HAS_MAP_ALIGN */

static void
UnmapPages(void *addr, size_t size)
{
#ifdef SOLARIS
    JS_ALWAYS_TRUE(munmap((caddr_t) addr, size) == 0);
#else
    JS_ALWAYS_TRUE(munmap(addr, size) == 0);
#endif
}

#endif

namespace js {

GCChunkAllocator defaultGCChunkAllocator;

inline void *
FindChunkStart(void *p)
{
    jsuword addr = reinterpret_cast<jsuword>(p);
    addr = (addr + GC_CHUNK_MASK) & ~GC_CHUNK_MASK;
    return reinterpret_cast<void *>(addr);
}

JS_FRIEND_API(void *)
AllocGCChunk()
{
    void *p;

#ifdef JS_GC_HAS_MAP_ALIGN
    p = MapAlignedPages(GC_CHUNK_SIZE, GC_CHUNK_SIZE);
    if (!p)
        return NULL;
#else
    /*
     * Windows requires that there be a 1:1 mapping between VM allocation
     * and deallocation operations.  Therefore, take care here to acquire the
     * final result via one mapping operation.  This means unmapping any
     * preliminary result that is not correctly aligned.
     */
    p = MapPages(NULL, GC_CHUNK_SIZE);
    if (!p)
        return NULL;

    if (reinterpret_cast<jsuword>(p) & GC_CHUNK_MASK) {
        UnmapPages(p, GC_CHUNK_SIZE);
        p = MapPages(FindChunkStart(p), GC_CHUNK_SIZE);
        while (!p) {
            /*
             * Over-allocate in order to map a memory region that is
             * definitely large enough then deallocate and allocate again the
             * correct size, within the over-sized mapping.
             */
            p = MapPages(NULL, GC_CHUNK_SIZE * 2);
            if (!p)
                return 0;
            UnmapPages(p, GC_CHUNK_SIZE * 2);
            p = MapPages(FindChunkStart(p), GC_CHUNK_SIZE);

            /*
             * Failure here indicates a race with another thread, so
             * try again.
             */
        }
    }
#endif /* !JS_GC_HAS_MAP_ALIGN */

    JS_ASSERT(!(reinterpret_cast<jsuword>(p) & GC_CHUNK_MASK));
    return p;
}

JS_FRIEND_API(void)
FreeGCChunk(void *p)
{
    JS_ASSERT(p);
    JS_ASSERT(!(reinterpret_cast<jsuword>(p) & GC_CHUNK_MASK));
    UnmapPages(p, GC_CHUNK_SIZE);
}

} /* namespace js */

