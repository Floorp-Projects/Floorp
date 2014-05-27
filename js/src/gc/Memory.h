/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Memory_h
#define gc_Memory_h

#include <stddef.h>

struct JSRuntime;

namespace js {
namespace gc {

class SystemPageAllocator
{
  public:
    // Sanity check that our compiled configuration matches the currently
    // running instance and initialize any runtime data needed for allocation.
    SystemPageAllocator();

    size_t systemPageSize() { return pageSize; }
    size_t systemAllocGranularity() { return allocGranularity; }

    // Allocate or deallocate pages from the system with the given alignment.
    void *mapAlignedPages(size_t size, size_t alignment);
    void unmapPages(void *p, size_t size);

    // Tell the OS that the given pages are not in use, so they should not be
    // written to a paging file. This may be a no-op on some platforms.
    bool markPagesUnused(void *p, size_t size);

    // Undo |MarkPagesUnused|: tell the OS that the given pages are of interest
    // and should be paged in and out normally. This may be a no-op on some
    // platforms.
    bool markPagesInUse(void *p, size_t size);

    // Returns #(hard faults) + #(soft faults)
    static size_t GetPageFaultCount();

    // Allocate memory mapped content.
    // The offset must be aligned according to alignment requirement.
    static void *AllocateMappedContent(int fd, size_t offset, size_t length, size_t alignment);

    // Deallocate memory mapped content.
    static void DeallocateMappedContent(void *p, size_t length);

  private:
    bool decommitEnabled();
    void *mapAlignedPagesSlow(size_t size, size_t alignment);
    void *mapAlignedPagesLastDitch(size_t size, size_t alignment);
    void getNewChunk(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize,
                     size_t size, size_t alignment);
    bool getNewChunkInner(void **aAddress, void **aRetainedAddr, size_t *aRetainedSize,
                          size_t size, size_t alignment, bool addrsGrowDown);

    // The GC can only safely decommit memory when the page size of the
    // running process matches the compiled arena size.
    size_t              pageSize;

    // The OS allocation granularity may not match the page size.
    size_t              allocGranularity;

#if defined(XP_UNIX)
    // The addresses handed out by mmap may grow up or down.
    int                 growthDirection;
#endif

    // The maximum number of unalignable chunks to temporarily keep alive in
    // the last ditch allocation pass. OOM crash reports generally show <= 7
    // unaligned chunks available (bug 1005844 comment #16).
    static const int    MaxLastDitchAttempts = 8;

public:
    void *testMapAlignedPagesLastDitch(size_t size, size_t alignment) {
        return mapAlignedPagesLastDitch(size, alignment);
    }
};

} // namespace gc
} // namespace js

#endif /* gc_Memory_h */
