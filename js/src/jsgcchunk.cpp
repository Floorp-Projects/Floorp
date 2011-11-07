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

#elif defined(XP_OS2)

# define INCL_DOSMEMMGR
# include <os2.h>

#elif defined(XP_MACOSX) || defined(DARWIN)

# include <libkern/OSAtomic.h>
# include <mach/mach_error.h>
# include <mach/mach_init.h>
# include <mach/vm_map.h>
# include <malloc/malloc.h>
# include <sys/mman.h>

#elif defined(XP_UNIX)

# include <unistd.h>
# include <sys/mman.h>

# ifndef MAP_NOSYNC
#  define MAP_NOSYNC    0
# endif

#endif

#ifdef XP_WIN

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

#elif defined(XP_OS2)

#define JS_GC_HAS_MAP_ALIGN 1
#define OS2_MAX_RECURSIONS  16

static void
UnmapPages(void *addr, size_t size)
{
    if (!DosFreeMem(addr))
        return;

    /* if DosFreeMem() failed, 'addr' is probably part of an "expensive"
     * allocation, so calculate the base address and try again
     */
    unsigned long cb = 2 * size;
    unsigned long flags;
    if (DosQueryMem(addr, &cb, &flags) || cb < size)
        return;

    jsuword base = reinterpret_cast<jsuword>(addr) - ((2 * size) - cb);
    DosFreeMem(reinterpret_cast<void*>(base));

    return;
}

static void *
MapAlignedPagesRecursively(size_t size, size_t alignment, int& recursions)
{
    if (++recursions >= OS2_MAX_RECURSIONS)
        return NULL;

    void *tmp;
    if (DosAllocMem(&tmp, size,
                    OBJ_ANY | PAG_COMMIT | PAG_READ | PAG_WRITE)) {
        JS_ALWAYS_TRUE(DosAllocMem(&tmp, size,
                                   PAG_COMMIT | PAG_READ | PAG_WRITE) == 0);
    }
    size_t offset = reinterpret_cast<jsuword>(tmp) & (alignment - 1);
    if (!offset)
        return tmp;

    /* if there are 'filler' bytes of free space above 'tmp', free 'tmp',
     * then reallocate it as a 'filler'-sized block;  assuming we're not
     * in a race with another thread, the next recursion should succeed
     */
    size_t filler = size + alignment - offset;
    unsigned long cb = filler;
    unsigned long flags = 0;
    unsigned long rc = DosQueryMem(&(static_cast<char*>(tmp))[size],
                                   &cb, &flags);
    if (!rc && (flags & PAG_FREE) && cb >= filler) {
        UnmapPages(tmp, 0);
        if (DosAllocMem(&tmp, filler,
                        OBJ_ANY | PAG_COMMIT | PAG_READ | PAG_WRITE)) {
            JS_ALWAYS_TRUE(DosAllocMem(&tmp, filler,
                                       PAG_COMMIT | PAG_READ | PAG_WRITE) == 0);
        }
    }

    void *p = MapAlignedPagesRecursively(size, alignment, recursions);
    UnmapPages(tmp, 0);

    return p;
}

static void *
MapAlignedPages(size_t size, size_t alignment)
{
    int recursions = -1;

    /* make up to OS2_MAX_RECURSIONS attempts to get an aligned block
     * of the right size by recursively allocating blocks of unaligned
     * free memory until only an aligned allocation is possible
     */
    void *p = MapAlignedPagesRecursively(size, alignment, recursions);
    if (p)
        return p;

    /* if memory is heavily fragmented, the recursive strategy may fail;
     * instead, use the "expensive" strategy:  allocate twice as much
     * as requested and return an aligned address within this block
     */
    if (DosAllocMem(&p, 2 * size,
                    OBJ_ANY | PAG_COMMIT | PAG_READ | PAG_WRITE)) {
        JS_ALWAYS_TRUE(DosAllocMem(&p, 2 * size,
                                   PAG_COMMIT | PAG_READ | PAG_WRITE) == 0);
    }

    jsuword addr = reinterpret_cast<jsuword>(p);
    addr = (addr + (alignment - 1)) & ~(alignment - 1);

    return reinterpret_cast<void *>(addr);
}

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

#elif defined(XP_UNIX)

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
namespace gc {

static inline void *
FindChunkStart(void *p)
{
    jsuword addr = reinterpret_cast<jsuword>(p);
    addr = (addr + ChunkMask) & ~ChunkMask;
    return reinterpret_cast<void *>(addr);
}

void *
AllocChunk()
{
    void *p;

#ifdef JS_GC_HAS_MAP_ALIGN
    p = MapAlignedPages(ChunkSize, ChunkSize);
    if (!p)
        return NULL;
#else
    /*
     * Windows requires that there be a 1:1 mapping between VM allocation
     * and deallocation operations.  Therefore, take care here to acquire the
     * final result via one mapping operation.  This means unmapping any
     * preliminary result that is not correctly aligned.
     */
    p = MapPages(NULL, ChunkSize);
    if (!p)
        return NULL;

    if (reinterpret_cast<jsuword>(p) & ChunkMask) {
        UnmapPages(p, ChunkSize);
        p = MapPages(FindChunkStart(p), ChunkSize);
        while (!p) {
            /*
             * Over-allocate in order to map a memory region that is
             * definitely large enough then deallocate and allocate again the
             * correct size, within the over-sized mapping.
             */
            p = MapPages(NULL, ChunkSize * 2);
            if (!p)
                return 0;
            UnmapPages(p, ChunkSize * 2);
            p = MapPages(FindChunkStart(p), ChunkSize);

            /*
             * Failure here indicates a race with another thread, so
             * try again.
             */
        }
    }
#endif /* !JS_GC_HAS_MAP_ALIGN */

    JS_ASSERT(!(reinterpret_cast<jsuword>(p) & ChunkMask));
    return p;
}

void
FreeChunk(void *p)
{
    JS_ASSERT(p);
    JS_ASSERT(!(reinterpret_cast<jsuword>(p) & ChunkMask));
    UnmapPages(p, ChunkSize);
}

#ifdef XP_WIN
bool
CommitMemory(void *addr, size_t size)
{
    JS_ASSERT(uintptr_t(addr) % 4096UL == 0);
    return true;
}

bool
DecommitMemory(void *addr, size_t size)
{
    JS_ASSERT(uintptr_t(addr) % 4096UL == 0);
    LPVOID p = VirtualAlloc(addr, size, MEM_RESET, PAGE_READWRITE);
    return p == addr;
}
#elif defined XP_OSX || defined XP_UNIX
#  ifndef MADV_DONTNEED
#    define MADV_DONTNEED MADV_FREE
#  endif
bool
CommitMemory(void *addr, size_t size)
{
    JS_ASSERT(uintptr_t(addr) % 4096UL == 0);
    return true;
}

bool
DecommitMemory(void *addr, size_t size)
{
    JS_ASSERT(uintptr_t(addr) % 4096UL == 0);
    int result = madvise(addr, size, MADV_DONTNEED);
    return result != -1;
}
#else
# error "No CommitMemory defined on this platform."
#endif

} /* namespace gc */
} /* namespace js */

