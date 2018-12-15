/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Memory.h"

#include "mozilla/Atomics.h"
#include "mozilla/TaggedAnonymousMemory.h"

#include "js/HeapAPI.h"
#include "vm/Runtime.h"

#if defined(XP_WIN)

#include "util/Windows.h"
#include <psapi.h>

#else

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

/*
 * System allocation functions generally require the allocation size
 * to be an integer multiple of the page size of the running process.
 */
static size_t pageSize = 0;

/* The OS allocation granularity may not match the page size. */
static size_t allocGranularity = 0;

/*
 * System allocation functions may hand out regions of memory in increasing or
 * decreasing order. This ordering is used as a hint during chunk alignment to
 * reduce the number of system calls. On systems with 48-bit addresses, our
 * workarounds to obtain 47-bit pointers cause addresses to be handed out in
 * increasing order.
 *
 * We do not use the growth direction on Windows, as constraints on VirtualAlloc
 * would make its application failure prone and complex. Tests indicate that
 * VirtualAlloc always hands out regions of memory in increasing order.
 */
#if defined(XP_DARWIN) ||                                               \
    (!defined(XP_WIN) && (defined(__ia64__) || defined(__aarch64__) ||  \
                          (defined(__sparc__) && defined(__arch64__) && \
                           (defined(__NetBSD__) || defined(__linux__)))))
static mozilla::Atomic<int, mozilla::Relaxed,
                       mozilla::recordreplay::Behavior::DontPreserve>
    growthDirection(1);
#elif defined(XP_UNIX)
static mozilla::Atomic<int, mozilla::Relaxed,
                       mozilla::recordreplay::Behavior::DontPreserve>
    growthDirection(0);
#endif

enum class Commit : bool {
  No = false,
  Yes = true,
};

#if defined(XP_WIN)
enum class PageAccess : DWORD {
  None = PAGE_NOACCESS,
  Read = PAGE_READONLY,
  ReadWrite = PAGE_READWRITE,
  Execute = PAGE_EXECUTE,
  ReadExecute = PAGE_EXECUTE_READ,
  ReadWriteExecute = PAGE_EXECUTE_READWRITE,
};
#else
enum class PageAccess : int {
  None = PROT_NONE,
  Read = PROT_READ,
  ReadWrite = PROT_READ | PROT_WRITE,
  Execute = PROT_EXEC,
  ReadExecute = PROT_READ | PROT_EXEC,
  ReadWriteExecute = PROT_READ | PROT_WRITE | PROT_EXEC,
};
#endif

/*
 * Data from OOM crashes shows there may be up to 24 chunk-sized but unusable
 * chunks available in low memory situations. These chunks may all need to be
 * used up before we gain access to remaining *alignable* chunk-sized regions,
 * so we use a generous limit of 32 unusable chunks to ensure we reach them.
 */
static const int MaxLastDitchAttempts = 32;

static void TryToAlignChunk(void** aRegion, void** aRetainedRegion,
                            size_t length, size_t alignment);
static void* MapAlignedPagesSlow(size_t length, size_t alignment);
static void* MapAlignedPagesLastDitch(size_t length, size_t alignment);

size_t SystemPageSize() { return pageSize; }

/*
 * We can only decommit unused pages if the hardcoded Arena
 * size matches the page size for the running process.
 */
static inline bool DecommitEnabled() { return pageSize == ArenaSize; }

/* Returns the offset from the nearest aligned address at or below |region|. */
static inline size_t OffsetFromAligned(void* region, size_t alignment) {
  return uintptr_t(region) % alignment;
}

void* TestMapAlignedPagesLastDitch(size_t length, size_t alignment) {
  return MapAlignedPagesLastDitch(length, alignment);
}

void InitMemorySubsystem() {
  if (pageSize == 0) {
#if defined(XP_WIN)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    pageSize = sysinfo.dwPageSize;
    allocGranularity = sysinfo.dwAllocationGranularity;
#else
    pageSize = size_t(sysconf(_SC_PAGESIZE));
    allocGranularity = pageSize;
#endif
  }
}

template <Commit commit, PageAccess prot>
static inline void* MapInternal(void* desired, size_t length) {
#if defined(XP_WIN)
  DWORD flags =
      (commit == Commit::Yes ? MEM_RESERVE | MEM_COMMIT : MEM_RESERVE);
  return VirtualAlloc(desired, length, flags, DWORD(prot));
#else
  int flags = MAP_PRIVATE | MAP_ANON;
  void* region = MozTaggedAnonymousMmap(desired, length, int(prot), flags, -1,
                                        0, "js-gc-heap");
  if (region == MAP_FAILED) {
    return nullptr;
  }
  return region;
#endif
}

static inline void UnmapInternal(void* region, size_t length) {
  MOZ_ASSERT(region && OffsetFromAligned(region, allocGranularity) == 0);
  MOZ_ASSERT(length > 0 && length % pageSize == 0);

#if defined(XP_WIN)
  MOZ_RELEASE_ASSERT(VirtualFree(region, 0, MEM_RELEASE) != 0);
#else
  if (munmap(region, length)) {
    MOZ_RELEASE_ASSERT(errno == ENOMEM);
  }
#endif
}

/* The JS engine uses 47-bit pointers; all higher bits must be clear. */
static inline bool IsInvalidRegion(void* region, size_t length) {
  const uint64_t invalidPointerMask = UINT64_C(0xffff800000000000);
  return (uintptr_t(region) + length - 1) & invalidPointerMask;
}

template <Commit commit = Commit::Yes, PageAccess prot = PageAccess::ReadWrite>
static inline void* MapMemory(size_t length) {
  MOZ_ASSERT(length > 0);

#if defined(XP_WIN)
  return MapInternal<commit, prot>(nullptr, length);
#elif defined(__ia64__) || \
    (defined(__sparc__) && defined(__arch64__) && defined(__NetBSD__))
  // These platforms allow addresses with more than 47 bits. However, their
  // mmap treats the first parameter as a hint of what address to allocate.
  // If the region starting at the exact address passed is not available, the
  // closest available region above it will be returned. Thus we supply a base
  // address of 0x0000070000000000, 121 TiB below our 47-bit limit.
  const uintptr_t hint = UINT64_C(0x0000070000000000);
  void* region =
      MapInternal<commit, prot>(reinterpret_cast<void*>(hint), length);

  // If we're given a region that ends above the 47-bit limit,
  // treat it as running out of memory.
  if (IsInvalidRegion(region, length)) {
    UnmapInternal(region, length);
    return nullptr;
  }
  return region;
#elif defined(__aarch64__) || \
    (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  // These platforms allow addresses with more than 47 bits. Unlike above,
  // their mmap does not treat its first parameter as a hint, so we're forced
  // to loop through valid regions manually. The following logic is far from
  // ideal and may cause allocation to take a long time on these platforms.
  const uintptr_t start = UINT64_C(0x0000070000000000);
  const uintptr_t end = UINT64_C(0x0000800000000000);
  const uintptr_t step = ChunkSize;
  uintptr_t desired;
  void* region = nullptr;
  for (desired = start; !region && desired + length <= end; desired += step) {
    region =
        MapInternal<commit, prot>(reinterpret_cast<void*>(desired), length);
    // If mmap on this platform *does* treat the supplied address as a hint,
    // this platform should be using the logic above!
    if (region) {
      MOZ_RELEASE_ASSERT(uintptr_t(region) == desired);
    }
  }
  return region;
#else
  return MapInternal<commit, prot>(nullptr, length);
#endif
}

template <Commit commit = Commit::Yes, PageAccess prot = PageAccess::ReadWrite>
static inline void* MapMemoryAt(void* desired, size_t length) {
  MOZ_ASSERT(desired && OffsetFromAligned(desired, allocGranularity) == 0);
  MOZ_ASSERT(length > 0);

#if defined(XP_WIN)
  return MapInternal<commit, prot>(desired, length);
#else
#if defined(__ia64__) || defined(__aarch64__) ||  \
    (defined(__sparc__) && defined(__arch64__) && \
     (defined(__NetBSD__) || defined(__linux__)))
  MOZ_RELEASE_ASSERT(!IsInvalidRegion(desired, length));
#endif

  void* region = MapInternal<commit, prot>(desired, length);
  if (!region) {
    return nullptr;
  }

  // On some platforms mmap treats the desired address as a hint, so
  // check that the address we got is the address we requested.
  if (region != desired) {
    UnmapInternal(region, length);
    return nullptr;
  }
  return region;
#endif
}

void* MapAlignedPages(size_t length, size_t alignment) {
  MOZ_RELEASE_ASSERT(length > 0 && alignment > 0);
  MOZ_RELEASE_ASSERT(length % pageSize == 0);
  MOZ_RELEASE_ASSERT(std::max(alignment, allocGranularity) %
                         std::min(alignment, allocGranularity) ==
                     0);

  void* region = MapMemory(length);
  if (OffsetFromAligned(region, alignment) == 0) {
    return region;
  }

  void* retainedRegion;
  TryToAlignChunk(&region, &retainedRegion, length, alignment);
  if (retainedRegion) {
    UnmapInternal(retainedRegion, length);
  }
  if (region) {
    if (OffsetFromAligned(region, alignment) == 0) {
      return region;
    }
    UnmapInternal(region, length);
  }

  region = MapAlignedPagesSlow(length, alignment);
  if (!region) {
    return MapAlignedPagesLastDitch(length, alignment);
  }

  MOZ_ASSERT(OffsetFromAligned(region, alignment) == 0);
  return region;
}

static void* MapAlignedPagesSlow(size_t length, size_t alignment) {
  void* alignedRegion = nullptr;
  do {
    size_t reserveLength = length + alignment - pageSize;
#if defined(XP_WIN)
    // Don't commit the requested pages as we won't use the region directly.
    void* region = MapMemory<Commit::No>(reserveLength);
#else
    void* region = MapMemory(reserveLength);
#endif
    if (!region) {
      return nullptr;
    }
    alignedRegion =
        reinterpret_cast<void*>(AlignBytes(uintptr_t(region), alignment));
#if defined(XP_WIN)
    // Windows requires that map and unmap calls be matched, so deallocate
    // and immediately reallocate at the desired (aligned) address.
    UnmapInternal(region, reserveLength);
    alignedRegion = MapMemoryAt(alignedRegion, length);
#else
    // munmap allows us to simply unmap the pages that don't interest us.
    if (alignedRegion != region) {
      UnmapInternal(region, uintptr_t(alignedRegion) - uintptr_t(region));
    }
    void* regionEnd =
        reinterpret_cast<void*>(uintptr_t(region) + reserveLength);
    void* alignedEnd =
        reinterpret_cast<void*>(uintptr_t(alignedRegion) + length);
    if (alignedEnd != regionEnd) {
      UnmapInternal(alignedEnd, uintptr_t(regionEnd) - uintptr_t(alignedEnd));
    }
#endif
    // On Windows we may have raced with another thread; if so, try again.
  } while (!alignedRegion);

  return alignedRegion;
}

/*
 * In a low memory or high fragmentation situation, alignable chunks of the
 * desired length may still be available, even if there are no more contiguous
 * free chunks that meet the |length + alignment - pageSize| requirement of
 * MapAlignedPagesSlow. In this case, try harder to find an alignable chunk
 * by temporarily holding onto the unaligned parts of each chunk until the
 * allocator gives us a chunk that either is, or can be aligned.
 */
static void* MapAlignedPagesLastDitch(size_t length, size_t alignment) {
  void* tempMaps[MaxLastDitchAttempts];
  int attempt = 0;
  void* region = MapMemory(length);
  if (OffsetFromAligned(region, alignment) == 0) {
    return region;
  }
  for (; attempt < MaxLastDitchAttempts; ++attempt) {
    TryToAlignChunk(&region, tempMaps + attempt, length, alignment);
    if (OffsetFromAligned(region, alignment) == 0) {
      if (tempMaps[attempt]) {
        UnmapInternal(tempMaps[attempt], length);
      }
      break;
    }
    if (!tempMaps[attempt]) {
      break;  // Bail if TryToAlignChunk failed.
    }
  }
  if (OffsetFromAligned(region, alignment)) {
    UnmapInternal(region, length);
    region = nullptr;
  }
  while (--attempt >= 0) {
    UnmapInternal(tempMaps[attempt], length);
  }
  return region;
}

#if defined(XP_WIN)

/*
 * On Windows, map and unmap calls must be matched, so we deallocate the
 * unaligned chunk, then reallocate the unaligned part to block off the
 * old address and force the allocator to give us a new one.
 */
static void TryToAlignChunk(void** aRegion, void** aRetainedRegion,
                            size_t length, size_t alignment) {
  void* region = *aRegion;
  MOZ_ASSERT(region && OffsetFromAligned(region, alignment) != 0);

  void* retainedRegion = nullptr;
  do {
    size_t offset = OffsetFromAligned(region, alignment);
    if (offset == 0) {
      // If the address is aligned, either we hit OOM or we're done.
      break;
    }
    UnmapInternal(region, length);
    size_t retainedLength = alignment - offset;
    retainedRegion = MapMemoryAt<Commit::No>(region, retainedLength);
    region = MapMemory(length);

    // If retainedRegion is null here, we raced with another thread.
  } while (!retainedRegion);
  *aRegion = region;
  *aRetainedRegion = retainedRegion;
}

#else  // !defined(XP_WIN)

/*
 * mmap calls don't have to be matched with calls to munmap, so we can unmap
 * just the pages we don't need. However, as we don't know a priori if addresses
 * are handed out in increasing or decreasing order, we have to try both
 * directions (depending on the environment, one will always fail).
 */
static void TryToAlignChunk(void** aRegion, void** aRetainedRegion,
                            size_t length, size_t alignment) {
  void* regionStart = *aRegion;
  MOZ_ASSERT(regionStart && OffsetFromAligned(regionStart, alignment) != 0);

  bool addressesGrowUpward = growthDirection > 0;
  bool directionUncertain = -8 < growthDirection && growthDirection <= 8;
  size_t offsetLower = OffsetFromAligned(regionStart, alignment);
  size_t offsetUpper = alignment - offsetLower;
  for (size_t i = 0; i < 2; ++i) {
    if (addressesGrowUpward) {
      void* upperStart =
          reinterpret_cast<void*>(uintptr_t(regionStart) + offsetUpper);
      void* regionEnd =
          reinterpret_cast<void*>(uintptr_t(regionStart) + length);
      if (MapMemoryAt(regionEnd, offsetUpper)) {
        UnmapInternal(regionStart, offsetUpper);
        if (directionUncertain) {
          ++growthDirection;
        }
        regionStart = upperStart;
        break;
      }
    } else {
      void* lowerStart =
          reinterpret_cast<void*>(uintptr_t(regionStart) - offsetLower);
      void* lowerEnd = reinterpret_cast<void*>(uintptr_t(lowerStart) + length);
      if (MapMemoryAt(lowerStart, offsetLower)) {
        UnmapInternal(lowerEnd, offsetLower);
        if (directionUncertain) {
          --growthDirection;
        }
        regionStart = lowerStart;
        break;
      }
    }
    // If we're confident in the growth direction, don't try the other.
    if (!directionUncertain) {
      break;
    }
    addressesGrowUpward = !addressesGrowUpward;
  }
  // If our current chunk cannot be aligned, just get a new one.
  void* retainedRegion = nullptr;
  if (OffsetFromAligned(regionStart, alignment) != 0) {
    retainedRegion = regionStart;
    regionStart = MapMemory(length);
  }
  *aRegion = regionStart;
  *aRetainedRegion = retainedRegion;
}

#endif

void UnmapPages(void* region, size_t length) {
  MOZ_RELEASE_ASSERT(region &&
                     OffsetFromAligned(region, allocGranularity) == 0);
  MOZ_RELEASE_ASSERT(length > 0 && length % pageSize == 0);

  // ASan does not automatically unpoison memory, so we have to do this here.
  MOZ_MAKE_MEM_UNDEFINED(region, length);

  UnmapInternal(region, length);
}

bool MarkPagesUnused(void* region, size_t length) {
  MOZ_RELEASE_ASSERT(region && OffsetFromAligned(region, pageSize) == 0);
  MOZ_RELEASE_ASSERT(length > 0 && length % pageSize == 0);

  MOZ_MAKE_MEM_NOACCESS(region, length);

  if (!DecommitEnabled()) {
    return true;
  }

#if defined(XP_WIN)
  return VirtualAlloc(region, length, MEM_RESET,
                      DWORD(PageAccess::ReadWrite)) == region;
#elif defined(XP_DARWIN)
  return madvise(region, length, MADV_FREE) == 0;
#else
  return madvise(region, length, MADV_DONTNEED) == 0;
#endif
}

void MarkPagesInUse(void* region, size_t length) {
  MOZ_RELEASE_ASSERT(region && OffsetFromAligned(region, pageSize) == 0);
  MOZ_RELEASE_ASSERT(length > 0 && length % pageSize == 0);

  MOZ_MAKE_MEM_UNDEFINED(region, length);
}

size_t GetPageFaultCount() {
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    return 0;
  }
#if defined(XP_WIN)
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) == 0) {
    return 0;
  }
  return pmc.PageFaultCount;
#else
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err) {
    return 0;
  }
  return usage.ru_majflt;
#endif
}

void* AllocateMappedContent(int fd, size_t offset, size_t length,
                            size_t alignment) {
  if (length == 0 || alignment == 0 || offset % alignment != 0 ||
      std::max(alignment, allocGranularity) %
              std::min(alignment, allocGranularity) !=
          0) {
    return nullptr;
  }

  size_t alignedOffset = offset - (offset % allocGranularity);
  size_t alignedLength = length + (offset % allocGranularity);

  // We preallocate the mapping using MapAlignedPages, which expects
  // the length parameter to be an integer multiple of the page size.
  size_t mappedLength = alignedLength;
  if (alignedLength % pageSize != 0) {
    mappedLength += pageSize - alignedLength % pageSize;
  }

#if defined(XP_WIN)
  HANDLE hFile = reinterpret_cast<HANDLE>(intptr_t(fd));

  // This call will fail if the file does not exist.
  HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (!hMap) {
    return nullptr;
  }

  DWORD offsetH = uint32_t(uint64_t(alignedOffset) >> 32);
  DWORD offsetL = uint32_t(alignedOffset);

  uint8_t* map = nullptr;
  for (;;) {
    // The value of a pointer is technically only defined while the region
    // it points to is allocated, so explicitly treat this one as a number.
    uintptr_t region = uintptr_t(MapAlignedPages(mappedLength, alignment));
    if (region == 0) {
      break;
    }
    UnmapInternal(reinterpret_cast<void*>(region), mappedLength);
    // If the offset or length are out of bounds, this call will fail.
    map = static_cast<uint8_t*>(
        MapViewOfFileEx(hMap, FILE_MAP_COPY, offsetH, offsetL, alignedLength,
                        reinterpret_cast<void*>(region)));

    // Retry if another thread mapped the address we were trying to use.
    if (map || GetLastError() != ERROR_INVALID_ADDRESS) {
      break;
    }
  }

  // This just decreases the file mapping object's internal reference count;
  // it won't actually be destroyed until we unmap the associated view.
  CloseHandle(hMap);

  if (!map) {
    return nullptr;
  }
#else
  // Sanity check the offset and length, as mmap does not do this for us.
  struct stat st;
  if (fstat(fd, &st) || offset >= uint64_t(st.st_size) ||
      length > uint64_t(st.st_size) - offset) {
    return nullptr;
  }

  void* region = MapAlignedPages(mappedLength, alignment);
  if (!region) {
    return nullptr;
  }

  // Calling mmap with MAP_FIXED will replace the previous mapping, allowing
  // us to reuse the region we obtained without racing with other threads.
  uint8_t* map =
      static_cast<uint8_t*>(mmap(region, alignedLength, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_FIXED, fd, alignedOffset));
  MOZ_RELEASE_ASSERT(map != MAP_FAILED);
#endif

#ifdef DEBUG
  // Zero out data before and after the desired mapping to catch errors early.
  if (offset != alignedOffset) {
    memset(map, 0, offset - alignedOffset);
  }
  if (alignedLength % pageSize) {
    memset(map + alignedLength, 0, pageSize - (alignedLength % pageSize));
  }
#endif

  return map + (offset - alignedOffset);
}

void DeallocateMappedContent(void* region, size_t length) {
  if (!region) {
    return;
  }

  // Due to bug 1502562, the following assertion does not currently hold.
  // MOZ_RELEASE_ASSERT(length > 0);

  // Calculate the address originally returned by the system call.
  // This is needed because AllocateMappedContent returns a pointer
  // that might be offset from the mapping, as the beginning of a
  // mapping must be aligned with the allocation granularity.
  uintptr_t map = uintptr_t(region) - (uintptr_t(region) % allocGranularity);
#if defined(XP_WIN)
  MOZ_RELEASE_ASSERT(UnmapViewOfFile(reinterpret_cast<void*>(map)) != 0);
#else
  size_t alignedLength = length + (uintptr_t(region) % allocGranularity);
  if (munmap(reinterpret_cast<void*>(map), alignedLength)) {
    MOZ_RELEASE_ASSERT(errno == ENOMEM);
  }
#endif
}

static inline void ProtectMemory(void* region, size_t length, PageAccess prot) {
  MOZ_RELEASE_ASSERT(region && OffsetFromAligned(region, pageSize) == 0);
  MOZ_RELEASE_ASSERT(length > 0 && length % pageSize == 0);
#if defined(XP_WIN)
  DWORD oldProtect;
  MOZ_RELEASE_ASSERT(VirtualProtect(region, length, DWORD(prot), &oldProtect) !=
                     0);
#else
  MOZ_RELEASE_ASSERT(mprotect(region, length, int(prot)) == 0);
#endif
}

void ProtectPages(void* region, size_t length) {
  ProtectMemory(region, length, PageAccess::None);
}

void MakePagesReadOnly(void* region, size_t length) {
  ProtectMemory(region, length, PageAccess::Read);
}

void UnprotectPages(void* region, size_t length) {
  ProtectMemory(region, length, PageAccess::ReadWrite);
}

}  // namespace gc
}  // namespace js
