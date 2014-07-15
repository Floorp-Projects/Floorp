/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Memory.h"

#include "mozilla/TaggedAnonymousMemory.h"

#include "js/HeapAPI.h"
#include "vm/Runtime.h"

#if defined(XP_WIN)

#include "jswin.h"
#include <psapi.h>

#elif defined(SOLARIS)

#include <sys/mman.h>
#include <unistd.h>

#elif defined(XP_UNIX)

#include <algorithm>
#include <errno.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

namespace js {
namespace gc {

// The GC can only safely decommit memory when the page size of the
// running process matches the compiled arena size.
static size_t pageSize = 0;

// The OS allocation granularity may not match the page size.
static size_t allocGranularity = 0;

#if defined(XP_UNIX)
// The addresses handed out by mmap may grow up or down.
static int growthDirection = 0;
#endif

// The maximum number of unalignable chunks to temporarily keep alive in
// the last ditch allocation pass. OOM crash reports generally show <= 7
// unaligned chunks available (bug 1005844 comment #16).
static const int MaxLastDitchAttempts = 8;

static void GetNewChunk(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize, size_t size,
                        size_t alignment);
static bool GetNewChunkInner(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize,
                             size_t size, size_t alignment, bool addrsGrowDown);
static void *MapAlignedPagesSlow(size_t size, size_t alignment);
static void *MapAlignedPagesLastDitch(size_t size, size_t alignment);

size_t
SystemPageSize()
{
    return pageSize;
}

static bool
DecommitEnabled()
{
    return pageSize == ArenaSize;
}

/*
 * This returns the offset of address p from the nearest aligned address at
 * or below p - or alternatively, the number of unaligned bytes at the end of
 * the region starting at p (as we assert that allocation size is an integer
 * multiple of the alignment).
 */
static inline size_t
OffsetFromAligned(void *p, size_t alignment)
{
    return uintptr_t(p) % alignment;
}

void *
TestMapAlignedPagesLastDitch(size_t size, size_t alignment)
{
    return MapAlignedPagesLastDitch(size, alignment);
}


#if defined(XP_WIN)

void
InitMemorySubsystem()
{
    if (pageSize == 0) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        pageSize = sysinfo.dwPageSize;
        allocGranularity = sysinfo.dwAllocationGranularity;
    }
}

static inline void *
MapMemoryAt(void *desired, size_t length, int flags, int prot = PAGE_READWRITE)
{
    return VirtualAlloc(desired, length, flags, prot);
}

static inline void *
MapMemory(size_t length, int flags, int prot = PAGE_READWRITE)
{
    return VirtualAlloc(nullptr, length, flags, prot);
}

void *
MapAlignedPages(size_t size, size_t alignment)
{
    MOZ_ASSERT(size >= alignment);
    MOZ_ASSERT(size % alignment == 0);
    MOZ_ASSERT(size % pageSize == 0);
    MOZ_ASSERT(alignment % allocGranularity == 0);

    void *p = MapMemory(size, MEM_COMMIT | MEM_RESERVE);

    /* Special case: If we want allocation alignment, no further work is needed. */
    if (alignment == allocGranularity)
        return p;

    if (OffsetFromAligned(p, alignment) == 0)
        return p;

    void *retainedAddr;
    size_t retainedSize;
    GetNewChunk(&p, &retainedAddr, &retainedSize, size, alignment);
    if (retainedAddr)
        UnmapPages(retainedAddr, retainedSize);
    if (p) {
        if (OffsetFromAligned(p, alignment) == 0)
            return p;
        UnmapPages(p, size);
    }

    p = MapAlignedPagesSlow(size, alignment);
    if (!p)
        return MapAlignedPagesLastDitch(size, alignment);

    MOZ_ASSERT(OffsetFromAligned(p, alignment) == 0);
    return p;
}

static void *
MapAlignedPagesSlow(size_t size, size_t alignment)
{
    /*
     * Windows requires that there be a 1:1 mapping between VM allocation
     * and deallocation operations.  Therefore, take care here to acquire the
     * final result via one mapping operation.  This means unmapping any
     * preliminary result that is not correctly aligned.
     */
    void *p;
    do {
        /*
         * Over-allocate in order to map a memory region that is definitely
         * large enough, then deallocate and allocate again the correct size,
         * within the over-sized mapping.
         *
         * Since we're going to unmap the whole thing anyway, the first
         * mapping doesn't have to commit pages.
         */
        size_t reserveSize = size + alignment - pageSize;
        p = MapMemory(reserveSize, MEM_RESERVE);
        if (!p)
            return nullptr;
        void *chunkStart = (void *)AlignBytes(uintptr_t(p), alignment);
        UnmapPages(p, reserveSize);
        p = MapMemoryAt(chunkStart, size, MEM_COMMIT | MEM_RESERVE);

        /* Failure here indicates a race with another thread, so try again. */
    } while (!p);

    return p;
}

/*
 * Even though there aren't any |size + alignment - pageSize| byte chunks left,
 * the allocator may still be able to give us |size| byte chunks that are
 * either already aligned, or *can* be aligned by allocating in the nearest
 * aligned location. Since we can't tell the allocator to give us a different
 * address each time, we temporarily hold onto the unaligned part of each chunk
 * until the allocator gives us a chunk that either is, or can be aligned.
 */
static void *
MapAlignedPagesLastDitch(size_t size, size_t alignment)
{
    void *p = nullptr;
    void *tempMaps[MaxLastDitchAttempts];
    int attempt = 0;
    for (; attempt < MaxLastDitchAttempts; ++attempt) {
        size_t retainedSize;
        GetNewChunk(&p, tempMaps + attempt, &retainedSize, size, alignment);
        if (OffsetFromAligned(p, alignment) == 0) {
            if (tempMaps[attempt])
                UnmapPages(tempMaps[attempt], retainedSize);
            break;
        }
        if (!tempMaps[attempt]) {
            /* GetNewChunk failed, but we can still try the simpler method. */
            tempMaps[attempt] = p;
            p = nullptr;
        }
    }
    if (OffsetFromAligned(p, alignment)) {
        UnmapPages(p, size);
        p = nullptr;
    }
    while (--attempt >= 0)
        UnmapPages(tempMaps[attempt], 0);
    return p;
}

/*
 * On Windows, map and unmap calls must be matched, so we deallocate the
 * unaligned chunk, then reallocate the unaligned part to block off the
 * old address and force the allocator to give us a new one.
 */
static void
GetNewChunk(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize, size_t size,
            size_t alignment)
{
    void *address = *aAddress;
    void *retainedAddr = nullptr;
    size_t retainedSize = 0;
    do {
        if (!address)
            address = MapMemory(size, MEM_COMMIT | MEM_RESERVE);
        size_t offset = OffsetFromAligned(address, alignment);
        if (!offset)
            break;
        UnmapPages(address, size);
        retainedSize = alignment - offset;
        retainedAddr = MapMemoryAt(address, retainedSize, MEM_RESERVE);
        address = MapMemory(size, MEM_COMMIT | MEM_RESERVE);
        /* If retainedAddr is null here, we raced with another thread. */
    } while (!retainedAddr);
    *aAddress = address;
    *aRetainedAddr = retainedAddr;
    *aRetainedSize = retainedSize;
}

void
UnmapPages(void *p, size_t size)
{
    MOZ_ALWAYS_TRUE(VirtualFree(p, 0, MEM_RELEASE));
}

bool
MarkPagesUnused(void *p, size_t size)
{
    if (!DecommitEnabled())
        return true;

    MOZ_ASSERT(OffsetFromAligned(p, pageSize) == 0);
    LPVOID p2 = MapMemoryAt(p, size, MEM_RESET);
    return p2 == p;
}

bool
MarkPagesInUse(void *p, size_t size)
{
    MOZ_ASSERT(OffsetFromAligned(p, pageSize) == 0);
    return true;
}

size_t
GetPageFaultCount()
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return 0;
    return pmc.PageFaultCount;
}

void *
AllocateMappedContent(int fd, size_t offset, size_t length, size_t alignment)
{
    // TODO: Bug 988813 - Support memory mapped array buffer for Windows platform.
    return nullptr;
}

// Deallocate mapped memory for object.
void
DeallocateMappedContent(void *p, size_t length)
{
    // TODO: Bug 988813 - Support memory mapped array buffer for Windows platform.
}

#elif defined(SOLARIS)

#ifndef MAP_NOSYNC
# define MAP_NOSYNC 0
#endif

void
InitMemorySubsystem()
{
    if (pageSize == 0)
        pageSize = allocGranularity = size_t(sysconf(_SC_PAGESIZE));
}

void *
MapAlignedPages(size_t size, size_t alignment)
{
    MOZ_ASSERT(size >= alignment);
    MOZ_ASSERT(size % alignment == 0);
    MOZ_ASSERT(size % pageSize == 0);
    MOZ_ASSERT(alignment % allocGranularity == 0);

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANON | MAP_ALIGN | MAP_NOSYNC;

    void *p = mmap((caddr_t)alignment, size, prot, flags, -1, 0);
    if (p == MAP_FAILED)
        return nullptr;
    return p;
}

void
UnmapPages(void *p, size_t size)
{
    MOZ_ALWAYS_TRUE(0 == munmap((caddr_t)p, size));
}

bool
MarkPagesUnused(void *p, size_t size)
{
    MOZ_ASSERT(OffsetFromAligned(p, pageSize) == 0);
    return true;
}

bool
MarkPagesInUse(void *p, size_t size)
{
    MOZ_ASSERT(OffsetFromAligned(p, pageSize) == 0);
    return true;
}

size_t
GetPageFaultCount()
{
    return 0;
}

void *
AllocateMappedContent(int fd, size_t offset, size_t length, size_t alignment)
{
    // Not implemented.
    return nullptr;
}

// Deallocate mapped memory for object.
void
DeallocateMappedContent(void *p, size_t length)
{
    // Not implemented.
}

#elif defined(XP_UNIX)

void
InitMemorySubsystem()
{
    if (pageSize == 0) {
        pageSize = allocGranularity = size_t(sysconf(_SC_PAGESIZE));
        growthDirection = 0;
    }
}

static inline void *
MapMemoryAt(void *desired, size_t length, int prot = PROT_READ | PROT_WRITE,
            int flags = MAP_PRIVATE | MAP_ANON, int fd = -1, off_t offset = 0)
{
#if defined(__ia64__)
    MOZ_ASSERT(0xffff800000000000ULL & (uintptr_t(desired) + length - 1) == 0);
#endif
    void *region = mmap(desired, length, prot, flags, fd, offset);
    if (region == MAP_FAILED)
        return nullptr;
    /*
     * mmap treats the given address as a hint unless the MAP_FIXED flag is
     * used (which isn't usually what you want, as this overrides existing
     * mappings), so check that the address we got is the address we wanted.
     */
    if (region != desired) {
        if (munmap(region, length))
            MOZ_ASSERT(errno == ENOMEM);
        return nullptr;
    }
    return region;
}

static inline void *
MapMemory(size_t length, int prot = PROT_READ | PROT_WRITE,
          int flags = MAP_PRIVATE | MAP_ANON, int fd = -1, off_t offset = 0)
{
#if defined(__ia64__)
    /*
     * The JS engine assumes that all allocated pointers have their high 17 bits clear,
     * which ia64's mmap doesn't support directly. However, we can emulate it by passing
     * mmap an "addr" parameter with those bits clear. The mmap will return that address,
     * or the nearest available memory above that address, providing a near-guarantee
     * that those bits are clear. If they are not, we return nullptr below to indicate
     * out-of-memory.
     *
     * The addr is chosen as 0x0000070000000000, which still allows about 120TB of virtual
     * address space.
     *
     * See Bug 589735 for more information.
     */
    void *region = mmap((void*)0x0000070000000000, length, prot, flags, fd, offset);
    if (region == MAP_FAILED)
        return nullptr;
    /*
     * If the allocated memory doesn't have its upper 17 bits clear, consider it
     * as out of memory.
     */
    if ((uintptr_t(region) + (length - 1)) & 0xffff800000000000) {
        if (munmap(region, length))
            MOZ_ASSERT(errno == ENOMEM);
        return nullptr;
    }
    return region;
#else
    void *region = MozTaggedAnonymousMmap(nullptr, length, prot, flags, fd, offset, "js-gc-heap");
    if (region == MAP_FAILED)
        return nullptr;
    return region;
#endif
}

void *
MapAlignedPages(size_t size, size_t alignment)
{
    MOZ_ASSERT(size >= alignment);
    MOZ_ASSERT(size % alignment == 0);
    MOZ_ASSERT(size % pageSize == 0);
    MOZ_ASSERT(alignment % allocGranularity == 0);

    void *p = MapMemory(size);

    /* Special case: If we want page alignment, no further work is needed. */
    if (alignment == allocGranularity)
        return p;

    if (OffsetFromAligned(p, alignment) == 0)
        return p;

    void *retainedAddr;
    size_t retainedSize;
    GetNewChunk(&p, &retainedAddr, &retainedSize, size, alignment);
    if (retainedAddr)
        UnmapPages(retainedAddr, retainedSize);
    if (p) {
        if (OffsetFromAligned(p, alignment) == 0)
            return p;
        UnmapPages(p, size);
    }

    p = MapAlignedPagesSlow(size, alignment);
    if (!p)
        return MapAlignedPagesLastDitch(size, alignment);

    MOZ_ASSERT(OffsetFromAligned(p, alignment) == 0);
    return p;
}

static void *
MapAlignedPagesSlow(size_t size, size_t alignment)
{
    /* Overallocate and unmap the region's edges. */
    size_t reqSize = size + alignment - pageSize;
    void *region = MapMemory(reqSize);
    if (!region)
        return nullptr;

    void *regionEnd = (void *)(uintptr_t(region) + reqSize);
    void *front;
    void *end;
    if (growthDirection <= 0) {
        size_t offset = OffsetFromAligned(regionEnd, alignment);
        end = (void *)(uintptr_t(regionEnd) - offset);
        front = (void *)(uintptr_t(end) - size);
    } else {
        size_t offset = OffsetFromAligned(region, alignment);
        front = (void *)(uintptr_t(region) + (offset ? alignment - offset : 0));
        end = (void *)(uintptr_t(front) + size);
    }

    if (front != region)
        UnmapPages(region, uintptr_t(front) - uintptr_t(region));
    if (end != regionEnd)
        UnmapPages(end, uintptr_t(regionEnd) - uintptr_t(end));

    return front;
}

/*
 * Even though there aren't any |size + alignment - pageSize| byte chunks left,
 * the allocator may still be able to give us |size| byte chunks that are
 * either already aligned, or *can* be aligned by allocating in the nearest
 * aligned location. Since we can't tell the allocator to give us a different
 * address each time, we temporarily hold onto the unaligned part of each chunk
 * until the allocator gives us a chunk that either is, or can be aligned.
 */
static void *
MapAlignedPagesLastDitch(size_t size, size_t alignment)
{
    void *p = nullptr;
    void *tempMaps[MaxLastDitchAttempts];
    size_t tempSizes[MaxLastDitchAttempts];
    int attempt = 0;
    for (; attempt < MaxLastDitchAttempts; ++attempt) {
        GetNewChunk(&p, tempMaps + attempt, tempSizes + attempt, size, alignment);
        if (OffsetFromAligned(p, alignment) == 0) {
            if (tempMaps[attempt])
                UnmapPages(tempMaps[attempt], tempSizes[attempt]);
            break;
        }
        if (!tempMaps[attempt]) {
            /* GetNewChunk failed, but we can still try the simpler method. */
            tempMaps[attempt] = p;
            tempSizes[attempt] = size;
            p = nullptr;
        }
    }
    if (OffsetFromAligned(p, alignment)) {
        UnmapPages(p, size);
        p = nullptr;
    }
    while (--attempt >= 0)
        UnmapPages(tempMaps[attempt], tempSizes[attempt]);
    return p;
}

/*
 * mmap calls don't have to be matched with calls to munmap, so we can unmap
 * just the pages we don't need. However, as we don't know a priori if addresses
 * are handed out in increasing or decreasing order, we have to try both
 * directions (depending on the environment, one will always fail).
 */
static void
GetNewChunk(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize, size_t size,
            size_t alignment)
{
    void *address = *aAddress;
    void *retainedAddr = nullptr;
    size_t retainedSize = 0;
    do {
        bool addrsGrowDown = growthDirection <= 0;
        /* Try the direction indicated by growthDirection. */
        if (GetNewChunkInner(&address, &retainedAddr, &retainedSize, size,
                             alignment, addrsGrowDown)) {
            break;
        }
        /* If that failed, try the opposite direction. */
        if (GetNewChunkInner(&address, &retainedAddr, &retainedSize, size,
                             alignment, !addrsGrowDown)) {
            break;
        }
        /* If retainedAddr is non-null here, we raced with another thread. */
    } while (retainedAddr);
    *aAddress = address;
    *aRetainedAddr = retainedAddr;
    *aRetainedSize = retainedSize;
}

#define SET_OUT_PARAMS_AND_RETURN(address_, retainedAddr_, retainedSize_, toReturn_)\
    do {                                                                            \
        *aAddress = address_; *aRetainedAddr = retainedAddr_;                       \
        *aRetainedSize = retainedSize_; return toReturn_;                           \
    } while(false)

static bool
GetNewChunkInner(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize, size_t size,
                 size_t alignment, bool addrsGrowDown)
{
    void *initial = *aAddress;
    if (!initial)
        initial = MapMemory(size);
    if (OffsetFromAligned(initial, alignment) == 0)
        SET_OUT_PARAMS_AND_RETURN(initial, nullptr, 0, true);
    /* Set the parameters based on whether addresses grow up or down. */
    size_t offset;
    void *discardedAddr;
    void *retainedAddr;
    int delta;
    if (addrsGrowDown) {
        offset = OffsetFromAligned(initial, alignment);
        discardedAddr = initial;
        retainedAddr = (void *)(uintptr_t(initial) + size - offset);
        delta = -1;
    } else {
        offset = alignment - OffsetFromAligned(initial, alignment);
        discardedAddr = (void*)(uintptr_t(initial) + offset);
        retainedAddr = initial;
        delta = 1;
    }
    /* Keep only the |offset| unaligned bytes. */
    UnmapPages(discardedAddr, size - offset);
    void *address = MapMemory(size);
    if (!address) {
        /* Map the rest of the original chunk again in case we can recover. */
        address = MapMemoryAt(initial, size - offset);
        if (!address)
            UnmapPages(retainedAddr, offset);
        SET_OUT_PARAMS_AND_RETURN(address, nullptr, 0, false);
    }
    if ((addrsGrowDown && address < retainedAddr) || (!addrsGrowDown && address > retainedAddr)) {
        growthDirection += delta;
        SET_OUT_PARAMS_AND_RETURN(address, retainedAddr, offset, true);
    }
    /* If we didn't choose the right direction, reduce its score. */
    growthDirection -= delta;
    /* Accept an aligned address if growthDirection didn't just flip. */
    if (OffsetFromAligned(address, alignment) == 0 && growthDirection + delta != 0)
        SET_OUT_PARAMS_AND_RETURN(address, retainedAddr, offset, true);
    UnmapPages(address, size);
    /* Map the original chunk again since we chose the wrong direction. */
    address = MapMemoryAt(initial, size - offset);
    if (!address) {
        /* Return non-null retainedAddr to indicate thread-related failure. */
        UnmapPages(retainedAddr, offset);
        SET_OUT_PARAMS_AND_RETURN(nullptr, retainedAddr, 0, false);
    }
    SET_OUT_PARAMS_AND_RETURN(address, nullptr, 0, false);
}

#undef SET_OUT_PARAMS_AND_RETURN

void
UnmapPages(void *p, size_t size)
{
    if (munmap(p, size))
        MOZ_ASSERT(errno == ENOMEM);
}

bool
MarkPagesUnused(void *p, size_t size)
{
    if (!DecommitEnabled())
        return false;

    MOZ_ASSERT(OffsetFromAligned(p, pageSize) == 0);
    int result = madvise(p, size, MADV_DONTNEED);
    return result != -1;
}

bool
MarkPagesInUse(void *p, size_t size)
{
    MOZ_ASSERT(OffsetFromAligned(p, pageSize) == 0);
    return true;
}

size_t
GetPageFaultCount()
{
    struct rusage usage;
    int err = getrusage(RUSAGE_SELF, &usage);
    if (err)
        return 0;
    return usage.ru_majflt;
}

void *
AllocateMappedContent(int fd, size_t offset, size_t length, size_t alignment)
{
#define NEED_PAGE_ALIGNED 0
    size_t pa_start; // Page aligned starting
    size_t pa_end; // Page aligned ending
    size_t pa_size; // Total page aligned size
    struct stat st;
    uint8_t *buf;

    // Make sure file exists and do sanity check for offset and size.
    if (fstat(fd, &st) < 0 || offset >= (size_t) st.st_size ||
        length == 0 || length > (size_t) st.st_size - offset)
        return nullptr;

    // Check for minimal alignment requirement.
#if NEED_PAGE_ALIGNED
    alignment = std::max(alignment, pageSize);
#endif
    if (offset & (alignment - 1))
        return nullptr;

    // Page aligned starting of the offset.
    pa_start = offset & ~(pageSize - 1);
    // Calculate page aligned ending by adding one page to the page aligned
    // starting of data end position(offset + length - 1).
    pa_end = ((offset + length - 1) & ~(pageSize - 1)) + pageSize;
    pa_size = pa_end - pa_start;

    // Ask for a continuous memory location.
    buf = (uint8_t *) MapMemory(pa_size);
    if (!buf)
        return nullptr;

    buf = (uint8_t *) MapMemoryAt(buf, pa_size, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_FIXED, fd, pa_start);
    if (!buf)
        return nullptr;

    // Reset the data before target file, which we don't need to see.
    memset(buf, 0, offset - pa_start);

    // Reset the data after target file, which we don't need to see.
    memset(buf + (offset - pa_start) + length, 0, pa_end - (offset + length));

    return buf + (offset - pa_start);
}

void
DeallocateMappedContent(void *p, size_t length)
{
    void *pa_start; // Page aligned starting
    size_t total_size; // Total allocated size

    pa_start = (void *)(uintptr_t(p) & ~(pageSize - 1));
    total_size = ((uintptr_t(p) + length) & ~(pageSize - 1)) + pageSize - uintptr_t(pa_start);
    if (munmap(pa_start, total_size))
        MOZ_ASSERT(errno == ENOMEM);
}

#else
#error "Memory mapping functions are not defined for your OS."
#endif

} // namespace gc
} // namespace js
