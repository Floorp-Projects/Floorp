/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>

#include "gc/GCInternals.h"
#include "gc/Memory.h"
#include "jsapi-tests/tests.h"

#if defined(XP_WIN)
#include "jswin.h"
#include <psapi.h>
#elif defined(SOLARIS)
// This test doesn't apply to Solaris.
#elif defined(XP_UNIX)
#include <algorithm>
#include <errno.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#error "Memory mapping functions are not defined for your OS."
#endif

BEGIN_TEST(testGCAllocator)
{
    size_t PageSize = 0;
#if defined(XP_WIN)
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    PageSize = sysinfo.dwPageSize;
#  else // Various APIs are unavailable. This test is disabled.
    return true;
#  endif
#elif defined(SOLARIS)
    return true;
#elif defined(XP_UNIX)
    PageSize = size_t(sysconf(_SC_PAGESIZE));
#else
    return true;
#endif

    /* Finish any ongoing background free activity. */
    js::gc::FinishGC(cx);

    bool growUp;
    CHECK(addressesGrowUp(&growUp));

    if (growUp)
        return testGCAllocatorUp(PageSize);
    return testGCAllocatorDown(PageSize);
}

static const size_t Chunk = 512 * 1024;
static const size_t Alignment = 2 * Chunk;
static const int MaxTempChunks = 4096;
static const size_t StagingSize = 16 * Chunk;

bool
addressesGrowUp(bool* resultOut)
{
    /*
     * Try to detect whether the OS allocates memory in increasing or decreasing
     * address order by making several allocations and comparing the addresses.
     */

    static const unsigned ChunksToTest = 20;
    static const int ThresholdCount = 15;

    void* chunks[ChunksToTest];
    for (unsigned i = 0; i < ChunksToTest; i++) {
        chunks[i] = mapMemory(2 * Chunk);
        CHECK(chunks[i]);
    }

    int upCount = 0;
    int downCount = 0;

    for (unsigned i = 0; i < ChunksToTest - 1; i++) {
        if (chunks[i] < chunks[i + 1])
            upCount++;
        else
            downCount++;
    }

    for (unsigned i = 0; i < ChunksToTest; i++)
        unmapPages(chunks[i], 2 * Chunk);

    /* Check results were mostly consistent. */
    CHECK(abs(upCount - downCount) >= ThresholdCount);

    *resultOut = upCount > downCount;

    return true;
}

size_t
offsetFromAligned(void* p)
{
    return uintptr_t(p) % Alignment;
}

enum AllocType {
   UseNormalAllocator,
   UseLastDitchAllocator
};

bool
testGCAllocatorUp(const size_t PageSize)
{
    const size_t UnalignedSize = StagingSize + Alignment - PageSize;
    void* chunkPool[MaxTempChunks];
    // Allocate a contiguous chunk that we can partition for testing.
    void* stagingArea = mapMemory(UnalignedSize);
    if (!stagingArea)
        return false;
    // Ensure that the staging area is aligned.
    unmapPages(stagingArea, UnalignedSize);
    if (offsetFromAligned(stagingArea)) {
        const size_t Offset = offsetFromAligned(stagingArea);
        // Place the area at the lowest aligned address.
        stagingArea = (void*)(uintptr_t(stagingArea) + (Alignment - Offset));
    }
    mapMemoryAt(stagingArea, StagingSize);
    // Make sure there are no available chunks below the staging area.
    int tempChunks;
    if (!fillSpaceBeforeStagingArea(tempChunks, stagingArea, chunkPool, false))
        return false;
    // Unmap the staging area so we can set it up for testing.
    unmapPages(stagingArea, StagingSize);
    // Check that the first chunk is used if it is aligned.
    CHECK(positionIsCorrect("xxooxxx---------", stagingArea, chunkPool, tempChunks));
    // Check that the first chunk is used if it can be aligned.
    CHECK(positionIsCorrect("x-ooxxx---------", stagingArea, chunkPool, tempChunks));
    // Check that an aligned chunk after a single unalignable chunk is used.
    CHECK(positionIsCorrect("x--xooxxx-------", stagingArea, chunkPool, tempChunks));
    // Check that we fall back to the slow path after two unalignable chunks.
    CHECK(positionIsCorrect("x--xx--xoo--xxx-", stagingArea, chunkPool, tempChunks));
    // Check that we also fall back after an unalignable and an alignable chunk.
    CHECK(positionIsCorrect("x--xx---x-oo--x-", stagingArea, chunkPool, tempChunks));
    // Check that the last ditch allocator works as expected.
    CHECK(positionIsCorrect("x--xx--xx-oox---", stagingArea, chunkPool, tempChunks,
                            UseLastDitchAllocator));

    // Clean up.
    while (--tempChunks >= 0)
        unmapPages(chunkPool[tempChunks], 2 * Chunk);
    return true;
}

bool
testGCAllocatorDown(const size_t PageSize)
{
    const size_t UnalignedSize = StagingSize + Alignment - PageSize;
    void* chunkPool[MaxTempChunks];
    // Allocate a contiguous chunk that we can partition for testing.
    void* stagingArea = mapMemory(UnalignedSize);
    if (!stagingArea)
        return false;
    // Ensure that the staging area is aligned.
    unmapPages(stagingArea, UnalignedSize);
    if (offsetFromAligned(stagingArea)) {
        void* stagingEnd = (void*)(uintptr_t(stagingArea) + UnalignedSize);
        const size_t Offset = offsetFromAligned(stagingEnd);
        // Place the area at the highest aligned address.
        stagingArea = (void*)(uintptr_t(stagingEnd) - Offset - StagingSize);
    }
    mapMemoryAt(stagingArea, StagingSize);
    // Make sure there are no available chunks above the staging area.
    int tempChunks;
    if (!fillSpaceBeforeStagingArea(tempChunks, stagingArea, chunkPool, true))
        return false;
    // Unmap the staging area so we can set it up for testing.
    unmapPages(stagingArea, StagingSize);
    // Check that the first chunk is used if it is aligned.
    CHECK(positionIsCorrect("---------xxxooxx", stagingArea, chunkPool, tempChunks));
    // Check that the first chunk is used if it can be aligned.
    CHECK(positionIsCorrect("---------xxxoo-x", stagingArea, chunkPool, tempChunks));
    // Check that an aligned chunk after a single unalignable chunk is used.
    CHECK(positionIsCorrect("-------xxxoox--x", stagingArea, chunkPool, tempChunks));
    // Check that we fall back to the slow path after two unalignable chunks.
    CHECK(positionIsCorrect("-xxx--oox--xx--x", stagingArea, chunkPool, tempChunks));
    // Check that we also fall back after an unalignable and an alignable chunk.
    CHECK(positionIsCorrect("-x--oo-x---xx--x", stagingArea, chunkPool, tempChunks));
    // Check that the last ditch allocator works as expected.
    CHECK(positionIsCorrect("---xoo-xx--xx--x", stagingArea, chunkPool, tempChunks,
                            UseLastDitchAllocator));

    // Clean up.
    while (--tempChunks >= 0)
        unmapPages(chunkPool[tempChunks], 2 * Chunk);
    return true;
}

bool
fillSpaceBeforeStagingArea(int& tempChunks, void* stagingArea,
                           void** chunkPool, bool addressesGrowDown)
{
    // Make sure there are no available chunks before the staging area.
    tempChunks = 0;
    chunkPool[tempChunks++] = mapMemory(2 * Chunk);
    while (tempChunks < MaxTempChunks && chunkPool[tempChunks - 1] &&
           (chunkPool[tempChunks - 1] < stagingArea) ^ addressesGrowDown) {
        chunkPool[tempChunks++] = mapMemory(2 * Chunk);
        if (!chunkPool[tempChunks - 1])
            break; // We already have our staging area, so OOM here is okay.
        if ((chunkPool[tempChunks - 1] < chunkPool[tempChunks - 2]) ^ addressesGrowDown)
            break; // The address growth direction is inconsistent!
    }
    // OOM also means success in this case.
    if (!chunkPool[tempChunks - 1]) {
        --tempChunks;
        return true;
    }
    // Bail if we can't guarantee the right address space layout.
    if ((chunkPool[tempChunks - 1] < stagingArea) ^ addressesGrowDown || (tempChunks > 1 &&
            (chunkPool[tempChunks - 1] < chunkPool[tempChunks - 2]) ^ addressesGrowDown))
    {
        while (--tempChunks >= 0)
            unmapPages(chunkPool[tempChunks], 2 * Chunk);
        unmapPages(stagingArea, StagingSize);
        return false;
    }
    return true;
}

bool
positionIsCorrect(const char* str, void* base, void** chunkPool, int tempChunks,
                  AllocType allocator = UseNormalAllocator)
{
    // str represents a region of memory, with each character representing a
    // region of Chunk bytes. str should contain only x, o and -, where
    // x = mapped by the test to set up the initial conditions,
    // o = mapped by the GC allocator, and
    // - = unmapped.
    // base should point to a region of contiguous free memory
    // large enough to hold strlen(str) chunks of Chunk bytes.
    int len = strlen(str);
    int i;
    // Find the index of the desired address.
    for (i = 0; i < len && str[i] != 'o'; ++i);
    void* desired = (void*)(uintptr_t(base) + i * Chunk);
    // Map the regions indicated by str.
    for (i = 0; i < len; ++i) {
        if (str[i] == 'x')
            mapMemoryAt((void*)(uintptr_t(base) +  i * Chunk), Chunk);
    }
    // Allocate using the GC's allocator.
    void* result;
    if (allocator == UseNormalAllocator)
        result = js::gc::MapAlignedPages(2 * Chunk, Alignment);
    else
        result = js::gc::TestMapAlignedPagesLastDitch(2 * Chunk, Alignment);
    // Clean up the mapped regions.
    if (result)
        js::gc::UnmapPages(result, 2 * Chunk);
    for (--i; i >= 0; --i) {
        if (str[i] == 'x')
            js::gc::UnmapPages((void*)(uintptr_t(base) +  i * Chunk), Chunk);
    }
    // CHECK returns, so clean up on failure.
    if (result != desired) {
        while (--tempChunks >= 0)
            js::gc::UnmapPages(chunkPool[tempChunks], 2 * Chunk);
    }
    return result == desired;
}

#if defined(XP_WIN)
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

void*
mapMemoryAt(void* desired, size_t length)
{
    return VirtualAlloc(desired, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void*
mapMemory(size_t length)
{
    return VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void
unmapPages(void* p, size_t size)
{
    MOZ_ALWAYS_TRUE(VirtualFree(p, 0, MEM_RELEASE));
}

#  else // Various APIs are unavailable. This test is disabled.

void* mapMemoryAt(void* desired, size_t length) { return nullptr; }
void* mapMemory(size_t length) { return nullptr; }
void unmapPages(void* p, size_t size) { }

#  endif
#elif defined(SOLARIS) // This test doesn't apply to Solaris.

void* mapMemoryAt(void* desired, size_t length) { return nullptr; }
void* mapMemory(size_t length) { return nullptr; }
void unmapPages(void* p, size_t size) { }

#elif defined(XP_UNIX)

void*
mapMemoryAt(void* desired, size_t length)
{

#if defined(__ia64__) || defined(__aarch64__) || \
    (defined(__sparc__) && defined(__arch64__) && (defined(__NetBSD__) || defined(__linux__)))
    MOZ_RELEASE_ASSERT(0xffff800000000000ULL & (uintptr_t(desired) + length - 1) == 0);
#endif
    void* region = mmap(desired, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED)
        return nullptr;
    if (region != desired) {
        if (munmap(region, length))
            MOZ_RELEASE_ASSERT(errno == ENOMEM);
        return nullptr;
    }
    return region;
}

void*
mapMemory(size_t length)
{
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANON;
    int fd = -1;
    off_t offset = 0;
    // The test code must be aligned with the implementation in gc/Memory.cpp.
#if defined(__ia64__) || (defined(__sparc__) && defined(__arch64__) && defined(__NetBSD__))
    void* region = mmap((void*)0x0000070000000000, length, prot, flags, fd, offset);
    if (region == MAP_FAILED)
        return nullptr;
    if ((uintptr_t(region) + (length - 1)) & 0xffff800000000000) {
        if (munmap(region, length))
            MOZ_RELEASE_ASSERT(errno == ENOMEM);
        return nullptr;
    }
    return region;
#elif defined(__aarch64__) || (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
    const uintptr_t start = UINT64_C(0x0000070000000000);
    const uintptr_t end   = UINT64_C(0x0000800000000000);
    const uintptr_t step  = js::gc::ChunkSize;
    uintptr_t hint;
    void* region = MAP_FAILED;
    for (hint = start; region == MAP_FAILED && hint + length <= end; hint += step) {
        region = mmap((void*)hint, length, prot, flags, fd, offset);
        if (region != MAP_FAILED) {
            if ((uintptr_t(region) + (length - 1)) & 0xffff800000000000) {
                if (munmap(region, length)) {
                    MOZ_RELEASE_ASSERT(errno == ENOMEM);
                }
                region = MAP_FAILED;
            }
        }
    }
    return region == MAP_FAILED ? nullptr : region;
#else
    void* region = mmap(nullptr, length, prot, flags, fd, offset);
    if (region == MAP_FAILED)
        return nullptr;
    return region;
#endif
}

void
unmapPages(void* p, size_t size)
{
    if (munmap(p, size))
        MOZ_RELEASE_ASSERT(errno == ENOMEM);
}

#else // !defined(XP_WIN) && !defined(SOLARIS) && !defined(XP_UNIX)
#error "Memory mapping functions are not defined for your OS."
#endif
END_TEST(testGCAllocator)
