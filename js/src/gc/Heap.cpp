/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tenured heap management.
 *
 * This file contains method definitions for the following classes for code that
 * is not specific to a particular phase of GC:
 *
 *  - Arena
 *  - ArenaList
 *  - FreeLists
 *  - ArenaLists
 *  - TenuredChunk
 *  - ChunkPool
 */

#include "gc/Heap-inl.h"

#include "gc/GCLock.h"
#include "jit/Assembler.h"
#include "vm/RegExpShared.h"

#include "gc/ArenaList-inl.h"
#include "gc/PrivateIterators-inl.h"

using namespace js;
using namespace js::gc;

// Check that reserved bits of a Cell are compatible with our typical allocators
// since most derived classes will store a pointer in the first word.
static const size_t MinFirstWordAlignment = 1u << CellFlagBitsReservedForGC;
static_assert(js::detail::LIFO_ALLOC_ALIGN >= MinFirstWordAlignment,
              "CellFlagBitsReservedForGC should support LifoAlloc");
static_assert(CellAlignBytes >= MinFirstWordAlignment,
              "CellFlagBitsReservedForGC should support gc::Cell");
static_assert(js::jit::CodeAlignment >= MinFirstWordAlignment,
              "CellFlagBitsReservedForGC should support JIT code");
static_assert(js::gc::JSClassAlignBytes >= MinFirstWordAlignment,
              "CellFlagBitsReservedForGC should support JSClass pointers");
static_assert(js::ScopeDataAlignBytes >= MinFirstWordAlignment,
              "CellFlagBitsReservedForGC should support scope data pointers");

#define CHECK_THING_SIZE(allocKind, traceKind, type, sizedType, bgFinal,       \
                         nursery, compact)                                     \
  static_assert(sizeof(sizedType) >= SortedArenaList::MinThingSize,            \
                #sizedType " is smaller than SortedArenaList::MinThingSize!"); \
  static_assert(sizeof(sizedType) >= sizeof(FreeSpan),                         \
                #sizedType " is smaller than FreeSpan");                       \
  static_assert(sizeof(sizedType) % CellAlignBytes == 0,                       \
                "Size of " #sizedType " is not a multiple of CellAlignBytes"); \
  static_assert(sizeof(sizedType) >= MinCellSize,                              \
                "Size of " #sizedType " is smaller than the minimum size");
FOR_EACH_ALLOCKIND(CHECK_THING_SIZE);
#undef CHECK_THING_SIZE

FreeSpan FreeLists::emptySentinel;

template <typename T>
struct ArenaLayout {
  static constexpr size_t thingSize() { return sizeof(T); }
  static constexpr size_t thingsPerArena() {
    return (ArenaSize - ArenaHeaderSize) / thingSize();
  }
  static constexpr size_t firstThingOffset() {
    return ArenaSize - thingSize() * thingsPerArena();
  }
};

const uint8_t Arena::ThingSizes[] = {
#define EXPAND_THING_SIZE(_1, _2, _3, sizedType, _4, _5, _6) \
  ArenaLayout<sizedType>::thingSize(),
    FOR_EACH_ALLOCKIND(EXPAND_THING_SIZE)
#undef EXPAND_THING_SIZE
};

const uint8_t Arena::FirstThingOffsets[] = {
#define EXPAND_FIRST_THING_OFFSET(_1, _2, _3, sizedType, _4, _5, _6) \
  ArenaLayout<sizedType>::firstThingOffset(),
    FOR_EACH_ALLOCKIND(EXPAND_FIRST_THING_OFFSET)
#undef EXPAND_FIRST_THING_OFFSET
};

const uint8_t Arena::ThingsPerArena[] = {
#define EXPAND_THINGS_PER_ARENA(_1, _2, _3, sizedType, _4, _5, _6) \
  ArenaLayout<sizedType>::thingsPerArena(),
    FOR_EACH_ALLOCKIND(EXPAND_THINGS_PER_ARENA)
#undef EXPAND_THINGS_PER_ARENA
};

void Arena::unmarkAll() {
  MarkBitmapWord* arenaBits = chunk()->markBits.arenaBits(this);
  for (size_t i = 0; i < ArenaBitmapWords; i++) {
    arenaBits[i] = 0;
  }
}

void Arena::unmarkPreMarkedFreeCells() {
  for (ArenaFreeCellIter cell(this); !cell.done(); cell.next()) {
    MOZ_ASSERT(cell->isMarkedBlack());
    cell->unmark();
  }
}

#ifdef DEBUG

void Arena::checkNoMarkedFreeCells() {
  for (ArenaFreeCellIter cell(this); !cell.done(); cell.next()) {
    MOZ_ASSERT(!cell->isMarkedAny());
  }
}

void Arena::checkAllCellsMarkedBlack() {
  for (ArenaCellIter cell(this); !cell.done(); cell.next()) {
    MOZ_ASSERT(cell->isMarkedBlack());
  }
}

#endif

#if defined(DEBUG) || defined(JS_GC_ZEAL)
void Arena::checkNoMarkedCells() {
  for (ArenaCellIter cell(this); !cell.done(); cell.next()) {
    MOZ_ASSERT(!cell->isMarkedAny());
  }
}
#endif

/* static */
void Arena::staticAsserts() {
  static_assert(size_t(AllocKind::LIMIT) <= 255,
                "All AllocKinds and AllocKind::LIMIT must fit in a uint8_t.");
  static_assert(std::size(ThingSizes) == AllocKindCount,
                "We haven't defined all thing sizes.");
  static_assert(std::size(FirstThingOffsets) == AllocKindCount,
                "We haven't defined all offsets.");
  static_assert(std::size(ThingsPerArena) == AllocKindCount,
                "We haven't defined all counts.");
}

/* static */
void Arena::checkLookupTables() {
#ifdef DEBUG
  for (size_t i = 0; i < AllocKindCount; i++) {
    MOZ_ASSERT(
        FirstThingOffsets[i] + ThingsPerArena[i] * ThingSizes[i] == ArenaSize,
        "Inconsistent arena lookup table data");
  }
#endif
}

#ifdef DEBUG
void js::gc::ArenaList::dump() {
  fprintf(stderr, "ArenaList %p:", this);
  if (cursorp_ == &head_) {
    fprintf(stderr, " *");
  }
  for (Arena* arena = head(); arena; arena = arena->next) {
    fprintf(stderr, " %p", arena);
    if (cursorp_ == &arena->next) {
      fprintf(stderr, " *");
    }
  }
  fprintf(stderr, "\n");
}
#endif

Arena* ArenaList::removeRemainingArenas(Arena** arenap) {
  // This is only ever called to remove arenas that are after the cursor, so
  // we don't need to update it.
#ifdef DEBUG
  for (Arena* arena = *arenap; arena; arena = arena->next) {
    MOZ_ASSERT(cursorp_ != &arena->next);
  }
#endif
  Arena* remainingArenas = *arenap;
  *arenap = nullptr;
  check();
  return remainingArenas;
}

FreeLists::FreeLists() {
  for (auto i : AllAllocKinds()) {
    freeLists_[i] = &emptySentinel;
  }
}

ArenaLists::ArenaLists(Zone* zone)
    : zone_(zone),
      freeLists_(zone),
      arenaLists_(zone),
      collectingArenaLists_(zone),
      incrementalSweptArenaKind(zone, AllocKind::LIMIT),
      incrementalSweptArenas(zone),
      gcCompactPropMapArenasToUpdate(zone, nullptr),
      gcNormalPropMapArenasToUpdate(zone, nullptr),
      savedEmptyArenas(zone, nullptr) {
  for (auto i : AllAllocKinds()) {
    concurrentUse(i) = ConcurrentUse::None;
  }
}

void ReleaseArenas(JSRuntime* rt, Arena* arena, const AutoLockGC& lock) {
  Arena* next;
  for (; arena; arena = next) {
    next = arena->next;
    rt->gc.releaseArena(arena, lock);
  }
}

void ReleaseArenaList(JSRuntime* rt, ArenaList& arenaList,
                      const AutoLockGC& lock) {
  ReleaseArenas(rt, arenaList.head(), lock);
  arenaList.clear();
}

ArenaLists::~ArenaLists() {
  AutoLockGC lock(runtime());

  for (auto i : AllAllocKinds()) {
    /*
     * We can only call this during the shutdown after the last GC when
     * the background finalization is disabled.
     */
    MOZ_ASSERT(concurrentUse(i) == ConcurrentUse::None);
    ReleaseArenaList(runtime(), arenaList(i), lock);
  }
  ReleaseArenaList(runtime(), incrementalSweptArenas.ref(), lock);

  ReleaseArenas(runtime(), savedEmptyArenas, lock);
}

void ArenaLists::moveArenasToCollectingLists() {
  checkEmptyFreeLists();
  for (AllocKind kind : AllAllocKinds()) {
    MOZ_ASSERT(collectingArenaList(kind).isEmpty());
    collectingArenaList(kind) = std::move(arenaList(kind));
    MOZ_ASSERT(arenaList(kind).isEmpty());
  }
}

void ArenaLists::mergeArenasFromCollectingLists() {
  for (AllocKind kind : AllAllocKinds()) {
    collectingArenaList(kind).insertListWithCursorAtEnd(arenaList(kind));
    arenaList(kind) = std::move(collectingArenaList(kind));
    MOZ_ASSERT(collectingArenaList(kind).isEmpty());
  }
}

Arena* ArenaLists::takeSweptEmptyArenas() {
  Arena* arenas = savedEmptyArenas;
  savedEmptyArenas = nullptr;
  return arenas;
}

void ArenaLists::setIncrementalSweptArenas(AllocKind kind,
                                           SortedArenaList& arenas) {
  incrementalSweptArenaKind = kind;
  incrementalSweptArenas.ref().clear();
  incrementalSweptArenas = arenas.toArenaList();
}

void ArenaLists::clearIncrementalSweptArenas() {
  incrementalSweptArenaKind = AllocKind::LIMIT;
  incrementalSweptArenas.ref().clear();
}

void ArenaLists::checkGCStateNotInUse() {
  // Called before and after collection to check the state is as expected.
#ifdef DEBUG
  checkSweepStateNotInUse();
  for (auto i : AllAllocKinds()) {
    MOZ_ASSERT(collectingArenaList(i).isEmpty());
  }
#endif
}

void ArenaLists::checkSweepStateNotInUse() {
#ifdef DEBUG
  checkNoArenasToUpdate();
  MOZ_ASSERT(incrementalSweptArenaKind == AllocKind::LIMIT);
  MOZ_ASSERT(incrementalSweptArenas.ref().isEmpty());
  MOZ_ASSERT(!savedEmptyArenas);
  for (auto i : AllAllocKinds()) {
    MOZ_ASSERT(concurrentUse(i) == ConcurrentUse::None);
  }
#endif
}

void ArenaLists::checkNoArenasToUpdate() {
  MOZ_ASSERT(!gcCompactPropMapArenasToUpdate);
  MOZ_ASSERT(!gcNormalPropMapArenasToUpdate);
}

void ArenaLists::checkNoArenasToUpdateForKind(AllocKind kind) {
#ifdef DEBUG
  switch (kind) {
    case AllocKind::COMPACT_PROP_MAP:
      MOZ_ASSERT(!gcCompactPropMapArenasToUpdate);
      break;
    case AllocKind::NORMAL_PROP_MAP:
      MOZ_ASSERT(!gcNormalPropMapArenasToUpdate);
      break;
    default:
      break;
  }
#endif
}

bool TenuredChunk::isPageFree(size_t pageIndex) const {
  if (decommittedPages[pageIndex]) {
    return true;
  }

  size_t arenaIndex = pageIndex * ArenasPerPage;
  for (size_t i = 0; i < ArenasPerPage; i++) {
    if (arenas[arenaIndex + i].allocated()) {
      return false;
    }
  }

  return true;
}

bool TenuredChunk::isPageFree(const Arena* arena) const {
  MOZ_ASSERT(arena);
  // arena must come from the freeArenasHead list.
  MOZ_ASSERT(!arena->allocated());
  size_t count = 1;
  size_t expectedPage = pageIndex(arena);

  Arena* nextArena = arena->next;
  while (nextArena && (pageIndex(nextArena) == expectedPage)) {
    count++;
    if (count == ArenasPerPage) {
      break;
    }
    nextArena = nextArena->next;
  }

  return count == ArenasPerPage;
}

void TenuredChunk::addArenaToFreeList(GCRuntime* gc, Arena* arena) {
  MOZ_ASSERT(!arena->allocated());
  arena->next = info.freeArenasHead;
  info.freeArenasHead = arena;
  ++info.numArenasFreeCommitted;
  ++info.numArenasFree;
  gc->updateOnArenaFree();
}

void TenuredChunk::addArenasInPageToFreeList(GCRuntime* gc, size_t pageIndex) {
  MOZ_ASSERT(isPageFree(pageIndex));

  size_t arenaIndex = pageIndex * ArenasPerPage;
  for (size_t i = 0; i < ArenasPerPage; i++) {
    Arena* a = &arenas[arenaIndex + i];
    MOZ_ASSERT(!a->allocated());
    a->next = info.freeArenasHead;
    info.freeArenasHead = a;
    // These arenas are already free, don't need to update numArenasFree.
    ++info.numArenasFreeCommitted;
    gc->updateOnArenaFree();
  }
}

void TenuredChunk::rebuildFreeArenasList() {
  if (info.numArenasFreeCommitted == 0) {
    MOZ_ASSERT(!info.freeArenasHead);
    return;
  }

  mozilla::BitSet<ArenasPerChunk, uint32_t> freeArenas;
  freeArenas.ResetAll();

  Arena* arena = info.freeArenasHead;
  while (arena) {
    freeArenas[arenaIndex(arena->address())] = true;
    arena = arena->next;
  }

  info.freeArenasHead = nullptr;
  Arena** freeCursor = &info.freeArenasHead;

  for (size_t i = 0; i < PagesPerChunk; i++) {
    for (size_t j = 0; j < ArenasPerPage; j++) {
      size_t arenaIndex = i * ArenasPerPage + j;
      if (freeArenas[arenaIndex]) {
        *freeCursor = &arenas[arenaIndex];
        freeCursor = &arenas[arenaIndex].next;
      }
    }
  }

  *freeCursor = nullptr;
}

void TenuredChunk::decommitFreeArenas(GCRuntime* gc, const bool& cancel,
                                      AutoLockGC& lock) {
  MOZ_ASSERT(DecommitEnabled());

  // We didn't traverse all arenas in the chunk to prevent accessing the arena
  // mprotect'ed during compaction in debug build. Instead, we traverse through
  // freeArenasHead list.
  Arena** freeCursor = &info.freeArenasHead;
  while (*freeCursor && !cancel) {
    if ((ArenasPerPage > 1) && !isPageFree(*freeCursor)) {
      freeCursor = &(*freeCursor)->next;
      continue;
    }

    // Find the next free arena after this page.
    Arena* nextArena = *freeCursor;
    for (size_t i = 0; i < ArenasPerPage; i++) {
      nextArena = nextArena->next;
      MOZ_ASSERT_IF(i != ArenasPerPage - 1, isPageFree(pageIndex(nextArena)));
    }

    size_t pIndex = pageIndex(*freeCursor);

    // Remove the free arenas from the list.
    *freeCursor = nextArena;

    info.numArenasFreeCommitted -= ArenasPerPage;
    // When we unlock below, other thread might acquire the lock and do
    // allocations. But it may find out the numArenasFree > 0 but there isn't
    // any free arena (Because we mark the decommit bit after the
    // MarkPagesUnusedSoft call). So we reduce the numArenasFree before unlock
    // and add it back once we acquire the lock again.
    info.numArenasFree -= ArenasPerPage;
    updateChunkListAfterAlloc(gc, lock);

    bool ok = decommitOneFreePage(gc, pIndex, lock);

    info.numArenasFree += ArenasPerPage;
    updateChunkListAfterFree(gc, ArenasPerPage, lock);

    if (!ok) {
      break;
    }

    // When we unlock in decommit, freeArenasHead might be updated by other
    // threads doing allocations.
    // Because the free list is sorted, we check if the freeArenasHead has
    // bypassed the page we did in decommit, if so, we forward to the updated
    // freeArenasHead, otherwise we just continue to check the next free arena
    // in the list.
    //
    // If the freeArenasHead is nullptr, we set it to the largest index so
    // freeArena will be set to freeArenasHead as well.
    size_t latestIndex =
        info.freeArenasHead ? pageIndex(info.freeArenasHead) : PagesPerChunk;
    if (latestIndex > pIndex) {
      freeCursor = &info.freeArenasHead;
    }
  }
}

void TenuredChunk::markArenasInPageDecommitted(size_t pageIndex) {
  // The arenas within this page are already free, and numArenasFreeCommitted is
  // subtracted in decommitFreeArenas.
  decommittedPages[pageIndex] = true;
}

void TenuredChunk::recycleArena(Arena* arena, SortedArenaList& dest,
                                size_t thingsPerArena) {
  arena->setAsFullyUnused();
  dest.insertAt(arena, thingsPerArena);
}

void TenuredChunk::releaseArena(GCRuntime* gc, Arena* arena,
                                const AutoLockGC& lock) {
  addArenaToFreeList(gc, arena);
  updateChunkListAfterFree(gc, 1, lock);
}

bool TenuredChunk::decommitOneFreePage(GCRuntime* gc, size_t pageIndex,
                                       AutoLockGC& lock) {
  MOZ_ASSERT(DecommitEnabled());

#ifdef DEBUG
  size_t index = pageIndex * ArenasPerPage;
  for (size_t i = 0; i < ArenasPerPage; i++) {
    MOZ_ASSERT(!arenas[index + i].allocated());
  }
#endif

  bool ok;
  {
    AutoUnlockGC unlock(lock);
    ok = MarkPagesUnusedSoft(pageAddress(pageIndex), PageSize);
  }

  if (ok) {
    markArenasInPageDecommitted(pageIndex);
  } else {
    addArenasInPageToFreeList(gc, pageIndex);
  }

  return ok;
}

void TenuredChunk::decommitFreeArenasWithoutUnlocking(const AutoLockGC& lock) {
  MOZ_ASSERT(DecommitEnabled());

  info.freeArenasHead = nullptr;
  Arena** freeCursor = &info.freeArenasHead;

  for (size_t i = 0; i < PagesPerChunk; i++) {
    if (decommittedPages[i]) {
      continue;
    }

    if (!isPageFree(i) || js::oom::ShouldFailWithOOM() ||
        !MarkPagesUnusedSoft(pageAddress(i), SystemPageSize())) {
      // Find out the free arenas and add it to freeArenasHead.
      for (size_t j = 0; j < ArenasPerPage; j++) {
        size_t arenaIndex = i * ArenasPerPage + j;
        if (!arenas[arenaIndex].allocated()) {
          *freeCursor = &arenas[arenaIndex];
          freeCursor = &arenas[arenaIndex].next;
        }
      }
      continue;
    }

    decommittedPages[i] = true;
    MOZ_ASSERT(info.numArenasFreeCommitted >= ArenasPerPage);
    info.numArenasFreeCommitted -= ArenasPerPage;
  }

  *freeCursor = nullptr;

#ifdef DEBUG
  verify();
#endif
}

void TenuredChunk::updateChunkListAfterAlloc(GCRuntime* gc,
                                             const AutoLockGC& lock) {
  if (MOZ_UNLIKELY(!hasAvailableArenas())) {
    gc->availableChunks(lock).remove(this);
    gc->fullChunks(lock).push(this);
  }
}

void TenuredChunk::updateChunkListAfterFree(GCRuntime* gc, size_t numArenasFree,
                                            const AutoLockGC& lock) {
  if (info.numArenasFree == numArenasFree) {
    gc->fullChunks(lock).remove(this);
    gc->availableChunks(lock).push(this);
  } else if (!unused()) {
    MOZ_ASSERT(gc->availableChunks(lock).contains(this));
  } else {
    MOZ_ASSERT(unused());
    gc->availableChunks(lock).remove(this);
    decommitAllArenas();
    MOZ_ASSERT(info.numArenasFreeCommitted == 0);
    gc->recycleChunk(this, lock);
  }
}

TenuredChunk* ChunkPool::pop() {
  MOZ_ASSERT(bool(head_) == bool(count_));
  if (!count_) {
    return nullptr;
  }
  return remove(head_);
}

void ChunkPool::push(TenuredChunk* chunk) {
  MOZ_ASSERT(!chunk->info.next);
  MOZ_ASSERT(!chunk->info.prev);

  chunk->info.next = head_;
  if (head_) {
    head_->info.prev = chunk;
  }
  head_ = chunk;
  ++count_;
}

TenuredChunk* ChunkPool::remove(TenuredChunk* chunk) {
  MOZ_ASSERT(count_ > 0);
  MOZ_ASSERT(contains(chunk));

  if (head_ == chunk) {
    head_ = chunk->info.next;
  }
  if (chunk->info.prev) {
    chunk->info.prev->info.next = chunk->info.next;
  }
  if (chunk->info.next) {
    chunk->info.next->info.prev = chunk->info.prev;
  }
  chunk->info.next = chunk->info.prev = nullptr;
  --count_;

  return chunk;
}

// We could keep the chunk pool sorted, but that's likely to be more expensive.
// This sort is nlogn, but keeping it sorted is likely to be m*n, with m being
// the number of operations (likely higher than n).
void ChunkPool::sort() {
  // Only sort if the list isn't already sorted.
  if (!isSorted()) {
    head_ = mergeSort(head(), count());

    // Fixup prev pointers.
    TenuredChunk* prev = nullptr;
    for (TenuredChunk* cur = head_; cur; cur = cur->info.next) {
      cur->info.prev = prev;
      prev = cur;
    }
  }

  MOZ_ASSERT(verify());
  MOZ_ASSERT(isSorted());
}

TenuredChunk* ChunkPool::mergeSort(TenuredChunk* list, size_t count) {
  MOZ_ASSERT(bool(list) == bool(count));

  if (count < 2) {
    return list;
  }

  size_t half = count / 2;

  // Split;
  TenuredChunk* front = list;
  TenuredChunk* back;
  {
    TenuredChunk* cur = list;
    for (size_t i = 0; i < half - 1; i++) {
      MOZ_ASSERT(cur);
      cur = cur->info.next;
    }
    back = cur->info.next;
    cur->info.next = nullptr;
  }

  front = mergeSort(front, half);
  back = mergeSort(back, count - half);

  // Merge
  list = nullptr;
  TenuredChunk** cur = &list;
  while (front || back) {
    if (!front) {
      *cur = back;
      break;
    }
    if (!back) {
      *cur = front;
      break;
    }

    // Note that the sort is stable due to the <= here. Nothing depends on
    // this but it could.
    if (front->info.numArenasFree <= back->info.numArenasFree) {
      *cur = front;
      front = front->info.next;
      cur = &(*cur)->info.next;
    } else {
      *cur = back;
      back = back->info.next;
      cur = &(*cur)->info.next;
    }
  }

  return list;
}

bool ChunkPool::isSorted() const {
  uint32_t last = 1;
  for (TenuredChunk* cursor = head_; cursor; cursor = cursor->info.next) {
    if (cursor->info.numArenasFree < last) {
      return false;
    }
    last = cursor->info.numArenasFree;
  }
  return true;
}

#ifdef DEBUG

bool ChunkPool::contains(TenuredChunk* chunk) const {
  verify();
  for (TenuredChunk* cursor = head_; cursor; cursor = cursor->info.next) {
    if (cursor == chunk) {
      return true;
    }
  }
  return false;
}

bool ChunkPool::verify() const {
  MOZ_ASSERT(bool(head_) == bool(count_));
  uint32_t count = 0;
  for (TenuredChunk* cursor = head_; cursor;
       cursor = cursor->info.next, ++count) {
    MOZ_ASSERT_IF(cursor->info.prev, cursor->info.prev->info.next == cursor);
    MOZ_ASSERT_IF(cursor->info.next, cursor->info.next->info.prev == cursor);
  }
  MOZ_ASSERT(count_ == count);
  return true;
}

void ChunkPool::verifyChunks() const {
  for (TenuredChunk* chunk = head_; chunk; chunk = chunk->info.next) {
    chunk->verify();
  }
}

void TenuredChunk::verify() const {
  size_t freeCount = 0;
  size_t freeCommittedCount = 0;
  for (size_t i = 0; i < ArenasPerChunk; ++i) {
    if (decommittedPages[pageIndex(i)]) {
      // Free but not committed.
      freeCount++;
      continue;
    }

    if (!arenas[i].allocated()) {
      // Free and committed.
      freeCount++;
      freeCommittedCount++;
    }
  }

  MOZ_ASSERT(freeCount == info.numArenasFree);
  MOZ_ASSERT(freeCommittedCount == info.numArenasFreeCommitted);

  size_t freeListCount = 0;
  for (Arena* arena = info.freeArenasHead; arena; arena = arena->next) {
    freeListCount++;
  }

  MOZ_ASSERT(freeListCount == info.numArenasFreeCommitted);
}

#endif

void ChunkPool::Iter::next() {
  MOZ_ASSERT(!done());
  current_ = current_->info.next;
}
