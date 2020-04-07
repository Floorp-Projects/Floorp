/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * [SMDOC] Garbage Collector
 *
 * This code implements an incremental mark-and-sweep garbage collector, with
 * most sweeping carried out in the background on a parallel thread.
 *
 * Full vs. zone GC
 * ----------------
 *
 * The collector can collect all zones at once, or a subset. These types of
 * collection are referred to as a full GC and a zone GC respectively.
 *
 * It is possible for an incremental collection that started out as a full GC to
 * become a zone GC if new zones are created during the course of the
 * collection.
 *
 * Incremental collection
 * ----------------------
 *
 * For a collection to be carried out incrementally the following conditions
 * must be met:
 *  - the collection must be run by calling js::GCSlice() rather than js::GC()
 *  - the GC mode must have been set to JSGC_MODE_INCREMENTAL or
 *    JSGC_MODE_ZONE_INCREMENTAL with JS_SetGCParameter()
 *  - no thread may have an AutoKeepAtoms instance on the stack
 *
 * The last condition is an engine-internal mechanism to ensure that incremental
 * collection is not carried out without the correct barriers being implemented.
 * For more information see 'Incremental marking' below.
 *
 * If the collection is not incremental, all foreground activity happens inside
 * a single call to GC() or GCSlice(). However the collection is not complete
 * until the background sweeping activity has finished.
 *
 * An incremental collection proceeds as a series of slices, interleaved with
 * mutator activity, i.e. running JavaScript code. Slices are limited by a time
 * budget. The slice finishes as soon as possible after the requested time has
 * passed.
 *
 * Collector states
 * ----------------
 *
 * The collector proceeds through the following states, the current state being
 * held in JSRuntime::gcIncrementalState:
 *
 *  - MarkRoots  - marks the stack and other roots
 *  - Mark       - incrementally marks reachable things
 *  - Sweep      - sweeps zones in groups and continues marking unswept zones
 *  - Finalize   - performs background finalization, concurrent with mutator
 *  - Compact    - incrementally compacts by zone
 *  - Decommit   - performs background decommit and chunk removal
 *
 * The MarkRoots activity always takes place in the first slice. The next two
 * states can take place over one or more slices.
 *
 * In other words an incremental collection proceeds like this:
 *
 * Slice 1:   MarkRoots:  Roots pushed onto the mark stack.
 *            Mark:       The mark stack is processed by popping an element,
 *                        marking it, and pushing its children.
 *
 *          ... JS code runs ...
 *
 * Slice 2:   Mark:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n-1: Mark:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n:   Mark:       Mark stack is completely drained.
 *            Sweep:      Select first group of zones to sweep and sweep them.
 *
 *          ... JS code runs ...
 *
 * Slice n+1: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive (see below). Then sweep more zone
 *                        sweep groups.
 *
 *          ... JS code runs ...
 *
 * Slice n+2: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive. Then sweep more zones.
 *
 *          ... JS code runs ...
 *
 * Slice m:   Sweep:      Sweeping is finished, and background sweeping
 *                        started on the helper thread.
 *
 *          ... JS code runs, remaining sweeping done on background thread ...
 *
 * When background sweeping finishes the GC is complete.
 *
 * Incremental marking
 * -------------------
 *
 * Incremental collection requires close collaboration with the mutator (i.e.,
 * JS code) to guarantee correctness.
 *
 *  - During an incremental GC, if a memory location (except a root) is written
 *    to, then the value it previously held must be marked. Write barriers
 *    ensure this.
 *
 *  - Any object that is allocated during incremental GC must start out marked.
 *
 *  - Roots are marked in the first slice and hence don't need write barriers.
 *    Roots are things like the C stack and the VM stack.
 *
 * The problem that write barriers solve is that between slices the mutator can
 * change the object graph. We must ensure that it cannot do this in such a way
 * that makes us fail to mark a reachable object (marking an unreachable object
 * is tolerable).
 *
 * We use a snapshot-at-the-beginning algorithm to do this. This means that we
 * promise to mark at least everything that is reachable at the beginning of
 * collection. To implement it we mark the old contents of every non-root memory
 * location written to by the mutator while the collection is in progress, using
 * write barriers. This is described in gc/Barrier.h.
 *
 * Incremental sweeping
 * --------------------
 *
 * Sweeping is difficult to do incrementally because object finalizers must be
 * run at the start of sweeping, before any mutator code runs. The reason is
 * that some objects use their finalizers to remove themselves from caches. If
 * mutator code was allowed to run after the start of sweeping, it could observe
 * the state of the cache and create a new reference to an object that was just
 * about to be destroyed.
 *
 * Sweeping all finalizable objects in one go would introduce long pauses, so
 * instead sweeping broken up into groups of zones. Zones which are not yet
 * being swept are still marked, so the issue above does not apply.
 *
 * The order of sweeping is restricted by cross compartment pointers - for
 * example say that object |a| from zone A points to object |b| in zone B and
 * neither object was marked when we transitioned to the Sweep phase. Imagine we
 * sweep B first and then return to the mutator. It's possible that the mutator
 * could cause |a| to become alive through a read barrier (perhaps it was a
 * shape that was accessed via a shape table). Then we would need to mark |b|,
 * which |a| points to, but |b| has already been swept.
 *
 * So if there is such a pointer then marking of zone B must not finish before
 * marking of zone A.  Pointers which form a cycle between zones therefore
 * restrict those zones to being swept at the same time, and these are found
 * using Tarjan's algorithm for finding the strongly connected components of a
 * graph.
 *
 * GC things without finalizers, and things with finalizers that are able to run
 * in the background, are swept on the background thread. This accounts for most
 * of the sweeping work.
 *
 * Reset
 * -----
 *
 * During incremental collection it is possible, although unlikely, for
 * conditions to change such that incremental collection is no longer safe. In
 * this case, the collection is 'reset' by resetIncrementalGC(). If we are in
 * the mark state, this just stops marking, but if we have started sweeping
 * already, we continue non-incrementally until we have swept the current sweep
 * group. Following a reset, a new collection is started.
 *
 * Compacting GC
 * -------------
 *
 * Compacting GC happens at the end of a major GC as part of the last slice.
 * There are three parts:
 *
 *  - Arenas are selected for compaction.
 *  - The contents of those arenas are moved to new arenas.
 *  - All references to moved things are updated.
 *
 * Collecting Atoms
 * ----------------
 *
 * Atoms are collected differently from other GC things. They are contained in
 * a special zone and things in other zones may have pointers to them that are
 * not recorded in the cross compartment pointer map. Each zone holds a bitmap
 * with the atoms it might be keeping alive, and atoms are only collected if
 * they are not included in any zone's atom bitmap. See AtomMarking.cpp for how
 * this bitmap is managed.
 */

#include "gc/GC-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Range.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TextUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <initializer_list>
#include <string.h>
#include <utility>
#ifndef XP_WIN
#  include <sys/mman.h>
#  include <unistd.h>
#endif

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jstypes.h"

#include "builtin/FinalizationRegistryObject.h"
#include "debugger/DebugAPI.h"
#include "gc/FindSCCs.h"
#include "gc/FreeOp.h"
#include "gc/GCInternals.h"
#include "gc/GCLock.h"
#include "gc/GCTrace.h"
#include "gc/Memory.h"
#include "gc/ParallelWork.h"
#include "gc/Policy.h"
#include "gc/WeakMap.h"
#include "jit/BaselineJIT.h"
#include "jit/IonCode.h"
#include "jit/JitcodeMap.h"
#include "jit/JitRealm.h"
#include "jit/MacroAssembler.h"
#include "js/SliceBudget.h"
#include "proxy/DeadObjectProxy.h"
#include "util/Poison.h"
#include "util/Windows.h"
#include "vm/BigIntType.h"
#include "vm/GeckoProfiler.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Printer.h"
#include "vm/ProxyObject.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/StringType.h"
#include "vm/SymbolType.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/WrapperObject.h"

#include "gc/Heap-inl.h"
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "gc/PrivateIterators-inl.h"
#include "gc/Zone-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Stack-inl.h"
#include "vm/StringType-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayLength;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

using JS::AutoGCRooter;

/* Increase the IGC marking slice time if we are in highFrequencyGC mode. */
static constexpr int IGC_MARK_SLICE_MULTIPLIER = 2;

const AllocKind gc::slotsToThingKind[] = {
    // clang-format off
    /*  0 */ AllocKind::OBJECT0,  AllocKind::OBJECT2,  AllocKind::OBJECT2,  AllocKind::OBJECT4,
    /*  4 */ AllocKind::OBJECT4,  AllocKind::OBJECT8,  AllocKind::OBJECT8,  AllocKind::OBJECT8,
    /*  8 */ AllocKind::OBJECT8,  AllocKind::OBJECT12, AllocKind::OBJECT12, AllocKind::OBJECT12,
    /* 12 */ AllocKind::OBJECT12, AllocKind::OBJECT16, AllocKind::OBJECT16, AllocKind::OBJECT16,
    /* 16 */ AllocKind::OBJECT16
    // clang-format on
};

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

static_assert(mozilla::ArrayLength(slotsToThingKind) ==
                  SLOTS_TO_THING_KIND_LIMIT,
              "We have defined a slot count for each kind.");

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

// GC things must be standard-layout classes so we can access the cell header by
// casting the thing pointer to a CellHeader*. This checks the property for the
// least derived thing type.
#define CHECK_THING_LAYOUT(_1, traceKind, _2, _3, _4, _5, _6)         \
  static_assert(                                                      \
      std::is_standard_layout<                                        \
          MapTraceKindToType<JS::TraceKind::traceKind>::Type>::value, \
      "The class for " #traceKind " must by a standard layout type.");
FOR_EACH_ALLOCKIND(CHECK_THING_LAYOUT)
#undef CHECK_THING_LAYOUT

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

FreeSpan FreeLists::emptySentinel;

struct js::gc::FinalizePhase {
  gcstats::PhaseKind statsPhase;
  AllocKinds kinds;
};

/*
 * Finalization order for objects swept incrementally on the main thread.
 */
static constexpr FinalizePhase ForegroundObjectFinalizePhase = {
    gcstats::PhaseKind::SWEEP_OBJECT,
    {AllocKind::OBJECT0, AllocKind::OBJECT2, AllocKind::OBJECT4,
     AllocKind::OBJECT8, AllocKind::OBJECT12, AllocKind::OBJECT16}};

/*
 * Finalization order for GC things swept incrementally on the main thread.
 */
static constexpr FinalizePhase ForegroundNonObjectFinalizePhase = {
    gcstats::PhaseKind::SWEEP_SCRIPT, {AllocKind::SCRIPT, AllocKind::JITCODE}};

/*
 * Finalization order for GC things swept on the background thread.
 */
static constexpr FinalizePhase BackgroundFinalizePhases[] = {
    {gcstats::PhaseKind::SWEEP_OBJECT,
     {AllocKind::FUNCTION, AllocKind::FUNCTION_EXTENDED,
      AllocKind::OBJECT0_BACKGROUND, AllocKind::OBJECT2_BACKGROUND,
      AllocKind::ARRAYBUFFER4, AllocKind::OBJECT4_BACKGROUND,
      AllocKind::ARRAYBUFFER8, AllocKind::OBJECT8_BACKGROUND,
      AllocKind::ARRAYBUFFER12, AllocKind::OBJECT12_BACKGROUND,
      AllocKind::ARRAYBUFFER16, AllocKind::OBJECT16_BACKGROUND}},
    {gcstats::PhaseKind::SWEEP_SCOPE,
     {
         AllocKind::SCOPE,
     }},
    {gcstats::PhaseKind::SWEEP_REGEXP_SHARED,
     {
         AllocKind::REGEXP_SHARED,
     }},
    {gcstats::PhaseKind::SWEEP_STRING,
     {AllocKind::FAT_INLINE_STRING, AllocKind::STRING,
      AllocKind::EXTERNAL_STRING, AllocKind::FAT_INLINE_ATOM, AllocKind::ATOM,
      AllocKind::SYMBOL, AllocKind::BIGINT}},
    {gcstats::PhaseKind::SWEEP_SHAPE,
     {AllocKind::SHAPE, AllocKind::ACCESSOR_SHAPE, AllocKind::BASE_SHAPE,
      AllocKind::OBJECT_GROUP}}};

void Arena::unmarkAll() {
  uintptr_t* word = chunk()->bitmap.arenaBits(this);
  memset(word, 0, ArenaBitmapWords * sizeof(uintptr_t));
}

void Arena::unmarkPreMarkedFreeCells() {
  for (ArenaFreeCellIter iter(this); !iter.done(); iter.next()) {
    TenuredCell* cell = iter.getCell();
    MOZ_ASSERT(cell->isMarkedBlack());
    cell->unmark();
  }
}

#ifdef DEBUG
void Arena::checkNoMarkedFreeCells() {
  for (ArenaFreeCellIter iter(this); !iter.done(); iter.next()) {
    MOZ_ASSERT(!iter.getCell()->isMarkedAny());
  }
}
#endif

/* static */
void Arena::staticAsserts() {
  static_assert(size_t(AllocKind::LIMIT) <= 255,
                "All AllocKinds and AllocKind::LIMIT must fit in a uint8_t.");
  static_assert(mozilla::ArrayLength(ThingSizes) == AllocKindCount,
                "We haven't defined all thing sizes.");
  static_assert(mozilla::ArrayLength(FirstThingOffsets) == AllocKindCount,
                "We haven't defined all offsets.");
  static_assert(mozilla::ArrayLength(ThingsPerArena) == AllocKindCount,
                "We haven't defined all counts.");
}

/* static */
inline void Arena::checkLookupTables() {
#ifdef DEBUG
  for (size_t i = 0; i < AllocKindCount; i++) {
    MOZ_ASSERT(
        FirstThingOffsets[i] + ThingsPerArena[i] * ThingSizes[i] == ArenaSize,
        "Inconsistent arena lookup table data");
  }
#endif
}

template <typename T>
inline size_t Arena::finalize(JSFreeOp* fop, AllocKind thingKind,
                              size_t thingSize) {
  /* Enforce requirements on size of T. */
  MOZ_ASSERT(thingSize % CellAlignBytes == 0);
  MOZ_ASSERT(thingSize >= MinCellSize);
  MOZ_ASSERT(thingSize <= 255);

  MOZ_ASSERT(allocated());
  MOZ_ASSERT(thingKind == getAllocKind());
  MOZ_ASSERT(thingSize == getThingSize());
  MOZ_ASSERT(!onDelayedMarkingList_);

  uint_fast16_t firstThing = firstThingOffset(thingKind);
  uint_fast16_t firstThingOrSuccessorOfLastMarkedThing = firstThing;
  uint_fast16_t lastThing = ArenaSize - thingSize;

  FreeSpan newListHead;
  FreeSpan* newListTail = &newListHead;
  size_t nmarked = 0;

  for (ArenaCellIterUnderFinalize i(this); !i.done(); i.next()) {
    T* t = i.get<T>();
    if (t->asTenured().isMarkedAny()) {
      uint_fast16_t thing = uintptr_t(t) & ArenaMask;
      if (thing != firstThingOrSuccessorOfLastMarkedThing) {
        // We just finished passing over one or more free things,
        // so record a new FreeSpan.
        newListTail->initBounds(firstThingOrSuccessorOfLastMarkedThing,
                                thing - thingSize, this);
        newListTail = newListTail->nextSpanUnchecked(this);
      }
      firstThingOrSuccessorOfLastMarkedThing = thing + thingSize;
      nmarked++;
    } else {
      t->finalize(fop);
      AlwaysPoison(t, JS_SWEPT_TENURED_PATTERN, thingSize,
                   MemCheckKind::MakeUndefined);
      gcTracer.traceTenuredFinalize(t);
    }
  }

  if (nmarked == 0) {
    // Do nothing. The caller will update the arena appropriately.
    MOZ_ASSERT(newListTail == &newListHead);
    DebugOnlyPoison(data, JS_SWEPT_TENURED_PATTERN, sizeof(data),
                    MemCheckKind::MakeUndefined);
    return nmarked;
  }

  MOZ_ASSERT(firstThingOrSuccessorOfLastMarkedThing != firstThing);
  uint_fast16_t lastMarkedThing =
      firstThingOrSuccessorOfLastMarkedThing - thingSize;
  if (lastThing == lastMarkedThing) {
    // If the last thing was marked, we will have already set the bounds of
    // the final span, and we just need to terminate the list.
    newListTail->initAsEmpty();
  } else {
    // Otherwise, end the list with a span that covers the final stretch of free
    // things.
    newListTail->initFinal(firstThingOrSuccessorOfLastMarkedThing, lastThing,
                           this);
  }

  firstFreeSpan = newListHead;
#ifdef DEBUG
  size_t nfree = numFreeThings(thingSize);
  MOZ_ASSERT(nfree + nmarked == thingsPerArena(thingKind));
#endif
  return nmarked;
}

// Finalize arenas from src list, releasing empty arenas if keepArenas wasn't
// specified and inserting the others into the appropriate destination size
// bins.
template <typename T>
static inline bool FinalizeTypedArenas(JSFreeOp* fop, Arena** src,
                                       SortedArenaList& dest,
                                       AllocKind thingKind,
                                       SliceBudget& budget) {
  AutoSetThreadIsFinalizing setThreadUse;

  size_t thingSize = Arena::thingSize(thingKind);
  size_t thingsPerArena = Arena::thingsPerArena(thingKind);

  while (Arena* arena = *src) {
    *src = arena->next;
    size_t nmarked = arena->finalize<T>(fop, thingKind, thingSize);
    size_t nfree = thingsPerArena - nmarked;

    if (nmarked) {
      dest.insertAt(arena, nfree);
    } else {
      arena->chunk()->recycleArena(arena, dest, thingsPerArena);
    }

    budget.step(thingsPerArena);
    if (budget.isOverBudget()) {
      return false;
    }
  }

  return true;
}

/*
 * Finalize the list of areans.
 */
static bool FinalizeArenas(JSFreeOp* fop, Arena** src, SortedArenaList& dest,
                           AllocKind thingKind, SliceBudget& budget) {
  switch (thingKind) {
#define EXPAND_CASE(allocKind, traceKind, type, sizedType, bgFinal, nursery, \
                    compact)                                                 \
  case AllocKind::allocKind:                                                 \
    return FinalizeTypedArenas<type>(fop, src, dest, thingKind, budget);
    FOR_EACH_ALLOCKIND(EXPAND_CASE)
#undef EXPAND_CASE

    default:
      MOZ_CRASH("Invalid alloc kind");
  }
}

Chunk* ChunkPool::pop() {
  MOZ_ASSERT(bool(head_) == bool(count_));
  if (!count_) {
    return nullptr;
  }
  return remove(head_);
}

void ChunkPool::push(Chunk* chunk) {
  MOZ_ASSERT(!chunk->info.next);
  MOZ_ASSERT(!chunk->info.prev);

  chunk->info.next = head_;
  if (head_) {
    head_->info.prev = chunk;
  }
  head_ = chunk;
  ++count_;
}

Chunk* ChunkPool::remove(Chunk* chunk) {
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
    Chunk* prev = nullptr;
    for (Chunk* cur = head_; cur; cur = cur->info.next) {
      cur->info.prev = prev;
      prev = cur;
    }
  }

  MOZ_ASSERT(verify());
  MOZ_ASSERT(isSorted());
}

Chunk* ChunkPool::mergeSort(Chunk* list, size_t count) {
  MOZ_ASSERT(bool(list) == bool(count));

  if (count < 2) {
    return list;
  }

  size_t half = count / 2;

  // Split;
  Chunk* front = list;
  Chunk* back;
  {
    Chunk* cur = list;
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
  Chunk** cur = &list;
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
  for (Chunk* cursor = head_; cursor; cursor = cursor->info.next) {
    if (cursor->info.numArenasFree < last) {
      return false;
    }
    last = cursor->info.numArenasFree;
  }
  return true;
}

#ifdef DEBUG
bool ChunkPool::contains(Chunk* chunk) const {
  verify();
  for (Chunk* cursor = head_; cursor; cursor = cursor->info.next) {
    if (cursor == chunk) {
      return true;
    }
  }
  return false;
}

bool ChunkPool::verify() const {
  MOZ_ASSERT(bool(head_) == bool(count_));
  uint32_t count = 0;
  for (Chunk* cursor = head_; cursor; cursor = cursor->info.next, ++count) {
    MOZ_ASSERT_IF(cursor->info.prev, cursor->info.prev->info.next == cursor);
    MOZ_ASSERT_IF(cursor->info.next, cursor->info.next->info.prev == cursor);
  }
  MOZ_ASSERT(count_ == count);
  return true;
}
#endif

void ChunkPool::Iter::next() {
  MOZ_ASSERT(!done());
  current_ = current_->info.next;
}

ChunkPool GCRuntime::expireEmptyChunkPool(const AutoLockGC& lock) {
  MOZ_ASSERT(emptyChunks(lock).verify());
  MOZ_ASSERT(tunables.minEmptyChunkCount(lock) <=
             tunables.maxEmptyChunkCount());

  ChunkPool expired;
  while (emptyChunks(lock).count() > tunables.minEmptyChunkCount(lock)) {
    Chunk* chunk = emptyChunks(lock).pop();
    prepareToFreeChunk(chunk->info);
    expired.push(chunk);
  }

  MOZ_ASSERT(expired.verify());
  MOZ_ASSERT(emptyChunks(lock).verify());
  MOZ_ASSERT(emptyChunks(lock).count() <= tunables.maxEmptyChunkCount());
  MOZ_ASSERT(emptyChunks(lock).count() <= tunables.minEmptyChunkCount(lock));
  return expired;
}

static void FreeChunkPool(ChunkPool& pool) {
  for (ChunkPool::Iter iter(pool); !iter.done();) {
    Chunk* chunk = iter.get();
    iter.next();
    pool.remove(chunk);
    MOZ_ASSERT(!chunk->info.numArenasFreeCommitted);
    UnmapPages(static_cast<void*>(chunk), ChunkSize);
  }
  MOZ_ASSERT(pool.count() == 0);
}

void GCRuntime::freeEmptyChunks(const AutoLockGC& lock) {
  FreeChunkPool(emptyChunks(lock));
}

inline void GCRuntime::prepareToFreeChunk(ChunkInfo& info) {
  MOZ_ASSERT(numArenasFreeCommitted >= info.numArenasFreeCommitted);
  numArenasFreeCommitted -= info.numArenasFreeCommitted;
  stats().count(gcstats::COUNT_DESTROY_CHUNK);
#ifdef DEBUG
  /*
   * Let FreeChunkPool detect a missing prepareToFreeChunk call before it
   * frees chunk.
   */
  info.numArenasFreeCommitted = 0;
#endif
}

inline void GCRuntime::updateOnArenaFree() { ++numArenasFreeCommitted; }

void Chunk::addArenaToFreeList(GCRuntime* gc, Arena* arena) {
  MOZ_ASSERT(!arena->allocated());
  arena->next = info.freeArenasHead;
  info.freeArenasHead = arena;
  ++info.numArenasFreeCommitted;
  ++info.numArenasFree;
  gc->updateOnArenaFree();
}

void Chunk::addArenaToDecommittedList(const Arena* arena) {
  ++info.numArenasFree;
  decommittedArenas.set(Chunk::arenaIndex(arena->address()));
}

void Chunk::recycleArena(Arena* arena, SortedArenaList& dest,
                         size_t thingsPerArena) {
  arena->setAsFullyUnused();
  dest.insertAt(arena, thingsPerArena);
}

void Chunk::releaseArena(GCRuntime* gc, Arena* arena, const AutoLockGC& lock) {
  addArenaToFreeList(gc, arena);
  updateChunkListAfterFree(gc, lock);
}

bool Chunk::decommitOneFreeArena(GCRuntime* gc, AutoLockGC& lock) {
  MOZ_ASSERT(info.numArenasFreeCommitted > 0);
  Arena* arena = fetchNextFreeArena(gc);
  updateChunkListAfterAlloc(gc, lock);

  bool ok;
  {
    AutoUnlockGC unlock(lock);
    ok = MarkPagesUnusedSoft(arena, ArenaSize);
  }

  if (ok) {
    addArenaToDecommittedList(arena);
  } else {
    addArenaToFreeList(gc, arena);
  }
  updateChunkListAfterFree(gc, lock);

  return ok;
}

void Chunk::decommitFreeArenasWithoutUnlocking(const AutoLockGC& lock) {
  for (size_t i = 0; i < ArenasPerChunk; ++i) {
    if (decommittedArenas.get(i) || arenas[i].allocated()) {
      continue;
    }

    if (MarkPagesUnusedSoft(&arenas[i], ArenaSize)) {
      info.numArenasFreeCommitted--;
      decommittedArenas.set(i);
    }
  }
}

void Chunk::updateChunkListAfterAlloc(GCRuntime* gc, const AutoLockGC& lock) {
  if (MOZ_UNLIKELY(!hasAvailableArenas())) {
    gc->availableChunks(lock).remove(this);
    gc->fullChunks(lock).push(this);
  }
}

void Chunk::updateChunkListAfterFree(GCRuntime* gc, const AutoLockGC& lock) {
  if (info.numArenasFree == 1) {
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

void GCRuntime::releaseArena(Arena* arena, const AutoLockGC& lock) {
  MOZ_ASSERT(arena->allocated());
  MOZ_ASSERT(!arena->onDelayedMarkingList());

  arena->zone->gcHeapSize.removeGCArena();
  arena->release(lock);
  arena->chunk()->releaseArena(this, arena, lock);
}

GCRuntime::GCRuntime(JSRuntime* rt)
    : rt(rt),
      systemZone(nullptr),
      atomsZone(nullptr),
      heapState_(JS::HeapState::Idle),
      stats_(this),
      marker(rt),
      heapSize(nullptr),
      rootsHash(256),
      nextCellUniqueId_(LargestTaggedNullCellPointer +
                        1),  // Ensure disjoint from null tagged pointers.
      numArenasFreeCommitted(0),
      verifyPreData(nullptr),
      lastGCStartTime_(ReallyNow()),
      lastGCEndTime_(ReallyNow()),
      mode(TuningDefaults::Mode),
      numActiveZoneIters(0),
      cleanUpEverything(false),
      grayBufferState(GCRuntime::GrayBufferState::Unused),
      grayBitsValid(false),
      majorGCTriggerReason(JS::GCReason::NO_REASON),
      fullGCForAtomsRequested_(false),
      minorGCNumber(0),
      majorGCNumber(0),
      number(0),
      sliceNumber(0),
      isFull(false),
      incrementalState(gc::State::NotActive),
      initialState(gc::State::NotActive),
#ifdef JS_GC_ZEAL
      useZeal(false),
#endif
      lastMarkSlice(false),
      safeToYield(true),
      sweepOnBackgroundThread(false),
      lifoBlocksToFree((size_t)JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      lifoBlocksToFreeAfterMinorGC(
          (size_t)JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      sweepGroupIndex(0),
      sweepGroups(nullptr),
      currentSweepGroup(nullptr),
      sweepZone(nullptr),
      hasMarkedGrayRoots(false),
      abortSweepAfterCurrentGroup(false),
      sweepMarkTaskStarted(false),
      sweepMarkResult(IncrementalProgress::NotFinished),
      startedCompacting(false),
      relocatedArenasToRelease(nullptr),
#ifdef JS_GC_ZEAL
      markingValidator(nullptr),
#endif
      defaultTimeBudgetMS_(TuningDefaults::DefaultTimeBudgetMS),
      incrementalAllowed(true),
      compactingEnabled(TuningDefaults::CompactingEnabled),
      rootsRemoved(false),
#ifdef JS_GC_ZEAL
      zealModeBits(0),
      zealFrequency(0),
      nextScheduled(0),
      deterministicOnly(false),
      incrementalLimit(0),
      selectedForMarking(rt),
#endif
      fullCompartmentChecks(false),
      gcCallbackDepth(0),
      alwaysPreserveCode(false),
      lowMemoryState(false),
      lock(mutexid::GCLock),
      allocTask(this, emptyChunks_.ref()),
      sweepTask(this),
      freeTask(this),
      decommitTask(this),
      sweepMarkTask(this),
      nursery_(this),
      storeBuffer_(rt, nursery()) {
  setGCMode(JSGC_MODE_GLOBAL);
}

#ifdef JS_GC_ZEAL

void GCRuntime::getZealBits(uint32_t* zealBits, uint32_t* frequency,
                            uint32_t* scheduled) {
  *zealBits = zealModeBits;
  *frequency = zealFrequency;
  *scheduled = nextScheduled;
}

const char gc::ZealModeHelpText[] =
    "  Specifies how zealous the garbage collector should be. Some of these "
    "modes can\n"
    "  be set simultaneously, by passing multiple level options, e.g. \"2;4\" "
    "will activate\n"
    "  both modes 2 and 4. Modes can be specified by name or number.\n"
    "  \n"
    "  Values:\n"
    "    0:  (None) Normal amount of collection (resets all modes)\n"
    "    1:  (RootsChange) Collect when roots are added or removed\n"
    "    2:  (Alloc) Collect when every N allocations (default: 100)\n"
    "    4:  (VerifierPre) Verify pre write barriers between instructions\n"
    "    7:  (GenerationalGC) Collect the nursery every N nursery allocations\n"
    "    8:  (YieldBeforeMarking) Incremental GC in two slices that yields "
    "between\n"
    "        the root marking and marking phases\n"
    "    9:  (YieldBeforeSweeping) Incremental GC in two slices that yields "
    "between\n"
    "        the marking and sweeping phases\n"
    "    10: (IncrementalMultipleSlices) Incremental GC in many slices\n"
    "    11: (IncrementalMarkingValidator) Verify incremental marking\n"
    "    12: (ElementsBarrier) Use the individual element post-write barrier\n"
    "        regardless of elements size\n"
    "    13: (CheckHashTablesOnMinorGC) Check internal hashtables on minor GC\n"
    "    14: (Compact) Perform a shrinking collection every N allocations\n"
    "    15: (CheckHeapAfterGC) Walk the heap to check its integrity after "
    "every GC\n"
    "    16: (CheckNursery) Check nursery integrity on minor GC\n"
    "    17: (YieldBeforeSweepingAtoms) Incremental GC in two slices that "
    "yields\n"
    "        before sweeping the atoms table\n"
    "    18: (CheckGrayMarking) Check gray marking invariants after every GC\n"
    "    19: (YieldBeforeSweepingCaches) Incremental GC in two slices that "
    "yields\n"
    "        before sweeping weak caches\n"
    "    20: (YieldBeforeSweepingTypes) Incremental GC in two slices that "
    "yields\n"
    "        before sweeping type information\n"
    "    21: (YieldBeforeSweepingObjects) Incremental GC in two slices that "
    "yields\n"
    "        before sweeping foreground finalized objects\n"
    "    22: (YieldBeforeSweepingNonObjects) Incremental GC in two slices that "
    "yields\n"
    "        before sweeping non-object GC things\n"
    "    23: (YieldBeforeSweepingShapeTrees) Incremental GC in two slices that "
    "yields\n"
    "        before sweeping shape trees\n"
    "    24: (CheckWeakMapMarking) Check weak map marking invariants after "
    "every GC\n"
    "    25: (YieldWhileGrayMarking) Incremental GC in two slices that yields\n"
    "        during gray marking\n";

// The set of zeal modes that control incremental slices. These modes are
// mutually exclusive.
static const mozilla::EnumSet<ZealMode> IncrementalSliceZealModes = {
    ZealMode::YieldBeforeMarking,
    ZealMode::YieldBeforeSweeping,
    ZealMode::IncrementalMultipleSlices,
    ZealMode::YieldBeforeSweepingAtoms,
    ZealMode::YieldBeforeSweepingCaches,
    ZealMode::YieldBeforeSweepingTypes,
    ZealMode::YieldBeforeSweepingObjects,
    ZealMode::YieldBeforeSweepingNonObjects,
    ZealMode::YieldBeforeSweepingShapeTrees};

void GCRuntime::setZeal(uint8_t zeal, uint32_t frequency) {
  MOZ_ASSERT(zeal <= unsigned(ZealMode::Limit));

  if (verifyPreData) {
    VerifyBarriers(rt, PreBarrierVerifier);
  }

  if (zeal == 0) {
    if (hasZealMode(ZealMode::GenerationalGC)) {
      evictNursery(JS::GCReason::DEBUG_GC);
      nursery().leaveZealMode();
    }

    if (isIncrementalGCInProgress()) {
      finishGC(JS::GCReason::DEBUG_GC);
    }
  }

  ZealMode zealMode = ZealMode(zeal);
  if (zealMode == ZealMode::GenerationalGC) {
    evictNursery(JS::GCReason::DEBUG_GC);
    nursery().enterZealMode();
  }

  // Some modes are mutually exclusive. If we're setting one of those, we
  // first reset all of them.
  if (IncrementalSliceZealModes.contains(zealMode)) {
    for (auto mode : IncrementalSliceZealModes) {
      clearZealMode(mode);
    }
  }

  bool schedule = zealMode >= ZealMode::Alloc;
  if (zeal != 0) {
    zealModeBits |= 1 << unsigned(zeal);
  } else {
    zealModeBits = 0;
  }
  zealFrequency = frequency;
  nextScheduled = schedule ? frequency : 0;
}

void GCRuntime::unsetZeal(uint8_t zeal) {
  MOZ_ASSERT(zeal <= unsigned(ZealMode::Limit));
  ZealMode zealMode = ZealMode(zeal);

  if (!hasZealMode(zealMode)) {
    return;
  }

  if (verifyPreData) {
    VerifyBarriers(rt, PreBarrierVerifier);
  }

  if (zealMode == ZealMode::GenerationalGC) {
    evictNursery(JS::GCReason::DEBUG_GC);
    nursery().leaveZealMode();
  }

  clearZealMode(zealMode);

  if (zealModeBits == 0) {
    if (isIncrementalGCInProgress()) {
      finishGC(JS::GCReason::DEBUG_GC);
    }

    zealFrequency = 0;
    nextScheduled = 0;
  }
}

void GCRuntime::setNextScheduled(uint32_t count) { nextScheduled = count; }

using CharRange = mozilla::Range<const char>;
using CharRangeVector = Vector<CharRange, 0, SystemAllocPolicy>;

static bool ParseZealModeName(CharRange text, uint32_t* modeOut) {
  struct ModeInfo {
    const char* name;
    size_t length;
    uint32_t value;
  };

  static const ModeInfo zealModes[] = {{"None", 0},
#  define ZEAL_MODE(name, value) {#  name, strlen(#  name), value},
                                       JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#  undef ZEAL_MODE
  };

  for (auto mode : zealModes) {
    if (text.length() == mode.length &&
        memcmp(text.begin().get(), mode.name, mode.length) == 0) {
      *modeOut = mode.value;
      return true;
    }
  }

  return false;
}

static bool ParseZealModeNumericParam(CharRange text, uint32_t* paramOut) {
  if (text.length() == 0) {
    return false;
  }

  for (auto c : text) {
    if (!mozilla::IsAsciiDigit(c)) {
      return false;
    }
  }

  *paramOut = atoi(text.begin().get());
  return true;
}

static bool SplitStringBy(CharRange text, char delimiter,
                          CharRangeVector* result) {
  auto start = text.begin();
  for (auto ptr = start; ptr != text.end(); ptr++) {
    if (*ptr == delimiter) {
      if (!result->emplaceBack(start, ptr)) {
        return false;
      }
      start = ptr + 1;
    }
  }

  return result->emplaceBack(start, text.end());
}

static bool PrintZealHelpAndFail() {
  fprintf(stderr, "Format: JS_GC_ZEAL=level(;level)*[,N]\n");
  fputs(ZealModeHelpText, stderr);
  return false;
}

bool GCRuntime::parseAndSetZeal(const char* str) {
  // Set the zeal mode from a string consisting of one or more mode specifiers
  // separated by ';', optionally followed by a ',' and the trigger frequency.
  // The mode specifiers can by a mode name or its number.

  auto text = CharRange(str, strlen(str));

  CharRangeVector parts;
  if (!SplitStringBy(text, ',', &parts)) {
    return false;
  }

  if (parts.length() == 0 || parts.length() > 2) {
    return PrintZealHelpAndFail();
  }

  uint32_t frequency = JS_DEFAULT_ZEAL_FREQ;
  if (parts.length() == 2 && !ParseZealModeNumericParam(parts[1], &frequency)) {
    return PrintZealHelpAndFail();
  }

  CharRangeVector modes;
  if (!SplitStringBy(parts[0], ';', &modes)) {
    return false;
  }

  for (const auto& descr : modes) {
    uint32_t mode;
    if (!ParseZealModeName(descr, &mode) &&
        !(ParseZealModeNumericParam(descr, &mode) &&
          mode <= unsigned(ZealMode::Limit))) {
      return PrintZealHelpAndFail();
    }

    setZeal(mode, frequency);
  }

  return true;
}

const char* js::gc::AllocKindName(AllocKind kind) {
  static const char* const names[] = {
#  define EXPAND_THING_NAME(allocKind, _1, _2, _3, _4, _5, _6) #  allocKind,
      FOR_EACH_ALLOCKIND(EXPAND_THING_NAME)
#  undef EXPAND_THING_NAME
  };
  static_assert(ArrayLength(names) == AllocKindCount,
                "names array should have an entry for every AllocKind");

  size_t i = size_t(kind);
  MOZ_ASSERT(i < ArrayLength(names));
  return names[i];
}

void js::gc::DumpArenaInfo() {
  fprintf(stderr, "Arena header size: %zu\n\n", ArenaHeaderSize);

  fprintf(stderr, "GC thing kinds:\n");
  fprintf(stderr, "%25s %8s %8s %8s\n",
          "AllocKind:", "Size:", "Count:", "Padding:");
  for (auto kind : AllAllocKinds()) {
    fprintf(stderr, "%25s %8zu %8zu %8zu\n", AllocKindName(kind),
            Arena::thingSize(kind), Arena::thingsPerArena(kind),
            Arena::firstThingOffset(kind) - ArenaHeaderSize);
  }
}

#endif  // JS_GC_ZEAL

bool GCRuntime::init(uint32_t maxbytes) {
  MOZ_ASSERT(SystemPageSize());
  Arena::checkLookupTables();

  {
    AutoLockGCBgAlloc lock(this);

    MOZ_ALWAYS_TRUE(tunables.setParameter(JSGC_MAX_BYTES, maxbytes, lock));

    const char* size = getenv("JSGC_MARK_STACK_LIMIT");
    if (size) {
      setMarkStackLimit(atoi(size), lock);
    }

    if (!nursery().init(lock)) {
      return false;
    }

    const char* pretenureThresholdStr = getenv("JSGC_PRETENURE_THRESHOLD");
    if (pretenureThresholdStr && pretenureThresholdStr[0]) {
      char* last;
      long pretenureThreshold = strtol(pretenureThresholdStr, &last, 10);
      if (last[0] || !tunables.setParameter(JSGC_PRETENURE_THRESHOLD,
                                            pretenureThreshold, lock)) {
        fprintf(stderr, "Invalid value for JSGC_PRETENURE_THRESHOLD: %s\n",
                pretenureThresholdStr);
      }
    }
  }

#ifdef JS_GC_ZEAL
  const char* zealSpec = getenv("JS_GC_ZEAL");
  if (zealSpec && zealSpec[0] && !parseAndSetZeal(zealSpec)) {
    return false;
  }
#endif

  if (!gcTracer.initTrace(*this)) {
    return false;
  }

  if (!marker.init(mode)) {
    return false;
  }

  if (!initSweepActions()) {
    return false;
  }

  return true;
}

void GCRuntime::freezeSelfHostingZone() {
  MOZ_ASSERT(!selfHostingZoneFrozen);
  MOZ_ASSERT(!isIncrementalGCInProgress());

  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    MOZ_ASSERT(!zone->isGCScheduled());
    if (zone->isSelfHostingZone()) {
      zone->scheduleGC();
    }
  }

  gc(GC_SHRINK, JS::GCReason::INIT_SELF_HOSTING);
  selfHostingZoneFrozen = true;
}

void GCRuntime::finish() {
  MOZ_ASSERT(inPageLoadCount == 0);

  // Wait for nursery background free to end and disable it to release memory.
  if (nursery().isEnabled()) {
    nursery().disable();
  }

  // Wait until the background finalization and allocation stops and the
  // helper thread shuts down before we forcefully release any remaining GC
  // memory.
  sweepTask.join();
  freeTask.join();
  allocTask.cancelAndWait();
  decommitTask.cancelAndWait();

#ifdef JS_GC_ZEAL
  // Free memory associated with GC verification.
  finishVerifier();
#endif

  // Delete all remaining zones.
  if (rt->gcInitialized) {
    for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
      AutoSetThreadIsSweeping threadIsSweeping(zone);
      for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        for (RealmsInCompartmentIter realm(comp); !realm.done(); realm.next()) {
          js_delete(realm.get());
        }
        comp->realms().clear();
        js_delete(comp.get());
      }
      zone->compartments().clear();
      js_delete(zone.get());
    }
  }

  zones().clear();

  FreeChunkPool(fullChunks_.ref());
  FreeChunkPool(availableChunks_.ref());
  FreeChunkPool(emptyChunks_.ref());

  gcTracer.finishTrace();

  nursery().printTotalProfileTimes();
  stats().printTotalProfileTimes();
}

bool GCRuntime::setParameter(JSGCParamKey key, uint32_t value) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  waitBackgroundSweepEnd();
  AutoLockGC lock(this);
  return setParameter(key, value, lock);
}

bool GCRuntime::setParameter(JSGCParamKey key, uint32_t value,
                             AutoLockGC& lock) {
  switch (key) {
    case JSGC_SLICE_TIME_BUDGET_MS:
      defaultTimeBudgetMS_ = value ? value : SliceBudget::UnlimitedTimeBudget;
      break;
    case JSGC_MARK_STACK_LIMIT:
      if (value == 0) {
        return false;
      }
      setMarkStackLimit(value, lock);
      break;
    case JSGC_MODE:
      if (mode != JSGC_MODE_GLOBAL && mode != JSGC_MODE_ZONE &&
          mode != JSGC_MODE_INCREMENTAL && mode != JSGC_MODE_ZONE_INCREMENTAL) {
        return false;
      }
      mode = JSGCMode(value);
      break;
    case JSGC_COMPACTING_ENABLED:
      compactingEnabled = value != 0;
      break;
    default:
      if (!tunables.setParameter(key, value, lock)) {
        return false;
      }
      for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
        zone->updateGCThresholds(*this, GC_NORMAL, lock);
      }
  }

  return true;
}

void GCRuntime::resetParameter(JSGCParamKey key) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  waitBackgroundSweepEnd();
  AutoLockGC lock(this);
  resetParameter(key, lock);
}

void GCRuntime::resetParameter(JSGCParamKey key, AutoLockGC& lock) {
  switch (key) {
    case JSGC_SLICE_TIME_BUDGET_MS:
      defaultTimeBudgetMS_ = TuningDefaults::DefaultTimeBudgetMS;
      break;
    case JSGC_MARK_STACK_LIMIT:
      setMarkStackLimit(MarkStack::DefaultCapacity, lock);
      break;
    case JSGC_MODE:
      mode = TuningDefaults::Mode;
      break;
    case JSGC_COMPACTING_ENABLED:
      compactingEnabled = TuningDefaults::CompactingEnabled;
      break;
    default:
      tunables.resetParameter(key, lock);
      for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
        zone->updateGCThresholds(*this, GC_NORMAL, lock);
      }
  }
}

uint32_t GCRuntime::getParameter(JSGCParamKey key) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  AutoLockGC lock(this);
  return getParameter(key, lock);
}

uint32_t GCRuntime::getParameter(JSGCParamKey key, const AutoLockGC& lock) {
  switch (key) {
    case JSGC_MAX_BYTES:
      return uint32_t(tunables.gcMaxBytes());
    case JSGC_MIN_NURSERY_BYTES:
      MOZ_ASSERT(tunables.gcMinNurseryBytes() < UINT32_MAX);
      return uint32_t(tunables.gcMinNurseryBytes());
    case JSGC_MAX_NURSERY_BYTES:
      MOZ_ASSERT(tunables.gcMaxNurseryBytes() < UINT32_MAX);
      return uint32_t(tunables.gcMaxNurseryBytes());
    case JSGC_BYTES:
      return uint32_t(heapSize.bytes());
    case JSGC_NURSERY_BYTES:
      return nursery().capacity();
    case JSGC_NUMBER:
      return uint32_t(number);
    case JSGC_MODE:
      return uint32_t(mode);
    case JSGC_UNUSED_CHUNKS:
      return uint32_t(emptyChunks(lock).count());
    case JSGC_TOTAL_CHUNKS:
      return uint32_t(fullChunks(lock).count() + availableChunks(lock).count() +
                      emptyChunks(lock).count());
    case JSGC_SLICE_TIME_BUDGET_MS:
      if (defaultTimeBudgetMS_.ref() == SliceBudget::UnlimitedTimeBudget) {
        return 0;
      } else {
        MOZ_RELEASE_ASSERT(defaultTimeBudgetMS_ >= 0);
        MOZ_RELEASE_ASSERT(defaultTimeBudgetMS_ <= UINT32_MAX);
        return uint32_t(defaultTimeBudgetMS_);
      }
    case JSGC_MARK_STACK_LIMIT:
      return marker.maxCapacity();
    case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
      return tunables.highFrequencyThreshold().ToMilliseconds();
    case JSGC_HIGH_FREQUENCY_LOW_LIMIT:
      return tunables.highFrequencyLowLimitBytes() / 1024 / 1024;
    case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:
      return tunables.highFrequencyHighLimitBytes() / 1024 / 1024;
    case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:
      return uint32_t(tunables.highFrequencyHeapGrowthMax() * 100);
    case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:
      return uint32_t(tunables.highFrequencyHeapGrowthMin() * 100);
    case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
      return uint32_t(tunables.lowFrequencyHeapGrowth() * 100);
    case JSGC_DYNAMIC_HEAP_GROWTH:
      return tunables.isDynamicHeapGrowthEnabled();
    case JSGC_DYNAMIC_MARK_SLICE:
      return tunables.isDynamicMarkSliceEnabled();
    case JSGC_ALLOCATION_THRESHOLD:
      return tunables.gcZoneAllocThresholdBase() / 1024 / 1024;
    case JSGC_NON_INCREMENTAL_FACTOR:
      return uint32_t(tunables.nonIncrementalFactor() * 100);
    case JSGC_AVOID_INTERRUPT_FACTOR:
      return uint32_t(tunables.avoidInterruptFactor() * 100);
    case JSGC_MIN_EMPTY_CHUNK_COUNT:
      return tunables.minEmptyChunkCount(lock);
    case JSGC_MAX_EMPTY_CHUNK_COUNT:
      return tunables.maxEmptyChunkCount();
    case JSGC_COMPACTING_ENABLED:
      return compactingEnabled;
    case JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION:
      return tunables.nurseryFreeThresholdForIdleCollection();
    case JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION_PERCENT:
      return uint32_t(tunables.nurseryFreeThresholdForIdleCollectionFraction() *
                      100.0f);
    case JSGC_PRETENURE_THRESHOLD:
      return uint32_t(tunables.pretenureThreshold() * 100);
    case JSGC_PRETENURE_GROUP_THRESHOLD:
      return tunables.pretenureGroupThreshold();
    case JSGC_MIN_LAST_DITCH_GC_PERIOD:
      return tunables.minLastDitchGCPeriod().ToSeconds();
    case JSGC_ZONE_ALLOC_DELAY_KB:
      return tunables.zoneAllocDelayBytes() / 1024;
    case JSGC_MALLOC_THRESHOLD_BASE:
      return tunables.mallocThresholdBase() / 1024 / 1024;
    case JSGC_MALLOC_GROWTH_FACTOR:
      return uint32_t(tunables.mallocGrowthFactor() * 100);
    default:
      MOZ_CRASH("Unknown parameter key");
  }
}

void GCRuntime::setMarkStackLimit(size_t limit, AutoLockGC& lock) {
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
  AutoUnlockGC unlock(lock);
  AutoStopVerifyingBarriers pauseVerification(rt, false);
  marker.setMaxCapacity(limit);
}

bool GCRuntime::addBlackRootsTracer(JSTraceDataOp traceOp, void* data) {
  AssertHeapIsIdle();
  return !!blackRootTracers.ref().append(
      Callback<JSTraceDataOp>(traceOp, data));
}

void GCRuntime::removeBlackRootsTracer(JSTraceDataOp traceOp, void* data) {
  // Can be called from finalizers
  for (size_t i = 0; i < blackRootTracers.ref().length(); i++) {
    Callback<JSTraceDataOp>* e = &blackRootTracers.ref()[i];
    if (e->op == traceOp && e->data == data) {
      blackRootTracers.ref().erase(e);
      break;
    }
  }
}

void GCRuntime::setGrayRootsTracer(JSTraceDataOp traceOp, void* data) {
  AssertHeapIsIdle();
  grayRootTracer.ref() = {traceOp, data};
}

void GCRuntime::clearBlackAndGrayRootTracers() {
  MOZ_ASSERT(rt->isBeingDestroyed());
  blackRootTracers.ref().clear();
  setGrayRootsTracer(nullptr, nullptr);
}

void GCRuntime::setGCCallback(JSGCCallback callback, void* data) {
  gcCallback.ref() = {callback, data};
}

void GCRuntime::callGCCallback(JSGCStatus status, JS::GCReason reason) const {
  const auto& callback = gcCallback.ref();
  MOZ_ASSERT(callback.op);
  callback.op(rt->mainContextFromOwnThread(), status, reason, callback.data);
}

void GCRuntime::setObjectsTenuredCallback(JSObjectsTenuredCallback callback,
                                          void* data) {
  tenuredCallback.ref() = {callback, data};
}

void GCRuntime::callObjectsTenuredCallback() {
  JS::AutoSuppressGCAnalysis nogc;
  const auto& callback = tenuredCallback.ref();
  if (callback.op) {
    callback.op(rt->mainContextFromOwnThread(), callback.data);
  }
}

bool GCRuntime::addFinalizeCallback(JSFinalizeCallback callback, void* data) {
  return finalizeCallbacks.ref().append(
      Callback<JSFinalizeCallback>(callback, data));
}

template <typename F>
static void EraseCallback(CallbackVector<F>& vector, F callback) {
  for (Callback<F>* p = vector.begin(); p != vector.end(); p++) {
    if (p->op == callback) {
      vector.erase(p);
      return;
    }
  }
}

void GCRuntime::removeFinalizeCallback(JSFinalizeCallback callback) {
  EraseCallback(finalizeCallbacks.ref(), callback);
}

void GCRuntime::callFinalizeCallbacks(JSFreeOp* fop,
                                      JSFinalizeStatus status) const {
  for (auto& p : finalizeCallbacks.ref()) {
    p.op(fop, status, p.data);
  }
}

void GCRuntime::setHostCleanupFinalizationRegistryCallback(
    JSHostCleanupFinalizationRegistryCallback callback, void* data) {
  hostCleanupFinalizationRegistryCallback.ref() = {callback, data};
}

void GCRuntime::callHostCleanupFinalizationRegistryCallback(
    FinalizationRegistryObject* registry) {
  JS::AutoSuppressGCAnalysis nogc;
  const auto& callback = hostCleanupFinalizationRegistryCallback.ref();
  if (callback.op) {
    callback.op(registry, callback.data);
  }
}

bool GCRuntime::addWeakPointerZonesCallback(JSWeakPointerZonesCallback callback,
                                            void* data) {
  return updateWeakPointerZonesCallbacks.ref().append(
      Callback<JSWeakPointerZonesCallback>(callback, data));
}

void GCRuntime::removeWeakPointerZonesCallback(
    JSWeakPointerZonesCallback callback) {
  EraseCallback(updateWeakPointerZonesCallbacks.ref(), callback);
}

void GCRuntime::callWeakPointerZonesCallbacks() const {
  JSContext* cx = rt->mainContextFromOwnThread();
  for (auto const& p : updateWeakPointerZonesCallbacks.ref()) {
    p.op(cx, p.data);
  }
}

bool GCRuntime::addWeakPointerCompartmentCallback(
    JSWeakPointerCompartmentCallback callback, void* data) {
  return updateWeakPointerCompartmentCallbacks.ref().append(
      Callback<JSWeakPointerCompartmentCallback>(callback, data));
}

void GCRuntime::removeWeakPointerCompartmentCallback(
    JSWeakPointerCompartmentCallback callback) {
  EraseCallback(updateWeakPointerCompartmentCallbacks.ref(), callback);
}

void GCRuntime::callWeakPointerCompartmentCallbacks(
    JS::Compartment* comp) const {
  JSContext* cx = rt->mainContextFromOwnThread();
  for (auto const& p : updateWeakPointerCompartmentCallbacks.ref()) {
    p.op(cx, comp, p.data);
  }
}

JS::GCSliceCallback GCRuntime::setSliceCallback(JS::GCSliceCallback callback) {
  return stats().setSliceCallback(callback);
}

JS::GCNurseryCollectionCallback GCRuntime::setNurseryCollectionCallback(
    JS::GCNurseryCollectionCallback callback) {
  return stats().setNurseryCollectionCallback(callback);
}

JS::DoCycleCollectionCallback GCRuntime::setDoCycleCollectionCallback(
    JS::DoCycleCollectionCallback callback) {
  const auto prior = gcDoCycleCollectionCallback.ref();
  gcDoCycleCollectionCallback.ref() = {callback, nullptr};
  return prior.op;
}

void GCRuntime::callDoCycleCollectionCallback(JSContext* cx) {
  const auto& callback = gcDoCycleCollectionCallback.ref();
  if (callback.op) {
    callback.op(cx);
  }
}

bool GCRuntime::addRoot(Value* vp, const char* name) {
  /*
   * Sometimes Firefox will hold weak references to objects and then convert
   * them to strong references by calling AddRoot (e.g., via PreserveWrapper,
   * or ModifyBusyCount in workers). We need a read barrier to cover these
   * cases.
   */
  if (isIncrementalGCInProgress()) {
    GCPtrValue::writeBarrierPre(*vp);
  }

  return rootsHash.ref().put(vp, name);
}

void GCRuntime::removeRoot(Value* vp) {
  rootsHash.ref().remove(vp);
  notifyRootsRemoved();
}

extern JS_FRIEND_API bool js::AddRawValueRoot(JSContext* cx, Value* vp,
                                              const char* name) {
  MOZ_ASSERT(vp);
  MOZ_ASSERT(name);
  bool ok = cx->runtime()->gc.addRoot(vp, name);
  if (!ok) {
    JS_ReportOutOfMemory(cx);
  }
  return ok;
}

extern JS_FRIEND_API void js::RemoveRawValueRoot(JSContext* cx, Value* vp) {
  cx->runtime()->gc.removeRoot(vp);
}

/* Compacting GC */

bool js::gc::IsCurrentlyAnimating(const TimeStamp& lastAnimationTime,
                                  const TimeStamp& currentTime) {
  // Assume that we're currently animating if js::NotifyAnimationActivity has
  // been called in the last second.
  static const auto oneSecond = TimeDuration::FromSeconds(1);
  return !lastAnimationTime.IsNull() &&
         currentTime < (lastAnimationTime + oneSecond);
}

bool GCRuntime::shouldCompact() {
  // Compact on shrinking GC if enabled.  Skip compacting in incremental GCs
  // if we are currently animating, unless the user is inactive or we're
  // responding to memory pressure.

  if (invocationKind != GC_SHRINK || !isCompactingGCEnabled()) {
    return false;
  }

  if (initialReason == JS::GCReason::USER_INACTIVE ||
      initialReason == JS::GCReason::MEM_PRESSURE) {
    return true;
  }

  return !isIncremental ||
         !IsCurrentlyAnimating(rt->lastAnimationTime, TimeStamp::Now());
}

bool GCRuntime::isCompactingGCEnabled() const {
  return compactingEnabled &&
         rt->mainContextFromOwnThread()->compactingDisabledCount == 0;
}

AutoDisableCompactingGC::AutoDisableCompactingGC(JSContext* cx) : cx(cx) {
  ++cx->compactingDisabledCount;
  if (cx->runtime()->gc.isIncrementalGCInProgress() &&
      cx->runtime()->gc.isCompactingGc()) {
    FinishGC(cx);
  }
}

AutoDisableCompactingGC::~AutoDisableCompactingGC() {
  MOZ_ASSERT(cx->compactingDisabledCount > 0);
  --cx->compactingDisabledCount;
}

bool GCRuntime::canRelocateZone(Zone* zone) const {
  if (zone->isAtomsZone()) {
    return false;
  }

  if (zone->isSelfHostingZone() && selfHostingZoneFrozen) {
    return false;
  }

  return true;
}

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

static bool ShouldRelocateAllArenas(JS::GCReason reason) {
  return reason == JS::GCReason::DEBUG_GC;
}

/*
 * Choose which arenas to relocate all cells from. Return an arena cursor that
 * can be passed to removeRemainingArenas().
 */
Arena** ArenaList::pickArenasToRelocate(size_t& arenaTotalOut,
                                        size_t& relocTotalOut) {
  // Relocate the greatest number of arenas such that the number of used cells
  // in relocated arenas is less than or equal to the number of free cells in
  // unrelocated arenas. In other words we only relocate cells we can move
  // into existing arenas, and we choose the least full areans to relocate.
  //
  // This is made easier by the fact that the arena list has been sorted in
  // descending order of number of used cells, so we will always relocate a
  // tail of the arena list. All we need to do is find the point at which to
  // start relocating.

  check();

  if (isCursorAtEnd()) {
    return nullptr;
  }

  Arena** arenap = cursorp_;      // Next arena to consider for relocation.
  size_t previousFreeCells = 0;   // Count of free cells before arenap.
  size_t followingUsedCells = 0;  // Count of used cells after arenap.
  size_t fullArenaCount = 0;      // Number of full arenas (not relocated).
  size_t nonFullArenaCount =
      0;  // Number of non-full arenas (considered for relocation).
  size_t arenaIndex = 0;  // Index of the next arena to consider.

  for (Arena* arena = head_; arena != *cursorp_; arena = arena->next) {
    fullArenaCount++;
  }

  for (Arena* arena = *cursorp_; arena; arena = arena->next) {
    followingUsedCells += arena->countUsedCells();
    nonFullArenaCount++;
  }

  mozilla::DebugOnly<size_t> lastFreeCells(0);
  size_t cellsPerArena = Arena::thingsPerArena((*arenap)->getAllocKind());

  while (*arenap) {
    Arena* arena = *arenap;
    if (followingUsedCells <= previousFreeCells) {
      break;
    }

    size_t freeCells = arena->countFreeCells();
    size_t usedCells = cellsPerArena - freeCells;
    followingUsedCells -= usedCells;
#ifdef DEBUG
    MOZ_ASSERT(freeCells >= lastFreeCells);
    lastFreeCells = freeCells;
#endif
    previousFreeCells += freeCells;
    arenap = &arena->next;
    arenaIndex++;
  }

  size_t relocCount = nonFullArenaCount - arenaIndex;
  MOZ_ASSERT(relocCount < nonFullArenaCount);
  MOZ_ASSERT((relocCount == 0) == (!*arenap));
  arenaTotalOut += fullArenaCount + nonFullArenaCount;
  relocTotalOut += relocCount;

  return arenap;
}

#ifdef DEBUG
inline bool PtrIsInRange(const void* ptr, const void* start, size_t length) {
  return uintptr_t(ptr) - uintptr_t(start) < length;
}
#endif

static void RelocateCell(Zone* zone, TenuredCell* src, AllocKind thingKind,
                         size_t thingSize) {
  JS::AutoSuppressGCAnalysis nogc(TlsContext.get());

  // Allocate a new cell.
  MOZ_ASSERT(zone == src->zone());
  TenuredCell* dst = AllocateCellInGC(zone, thingKind);

  // Copy source cell contents to destination.
  memcpy(dst, src, thingSize);

  // Move any uid attached to the object.
  src->zone()->transferUniqueId(dst, src);

  if (IsObjectAllocKind(thingKind)) {
    JSObject* srcObj = static_cast<JSObject*>(static_cast<Cell*>(src));
    JSObject* dstObj = static_cast<JSObject*>(static_cast<Cell*>(dst));

    if (srcObj->isNative()) {
      NativeObject* srcNative = &srcObj->as<NativeObject>();
      NativeObject* dstNative = &dstObj->as<NativeObject>();

      // Fixup the pointer to inline object elements if necessary.
      if (srcNative->hasFixedElements()) {
        uint32_t numShifted =
            srcNative->getElementsHeader()->numShiftedElements();
        dstNative->setFixedElements(numShifted);
      }

      // For copy-on-write objects that own their elements, fix up the
      // owner pointer to point to the relocated object.
      // Copy-on-write objects that don't own their elements have their elements
      // pointer fixed up by JSObject::fixupAfterMovingGC.
      if (srcNative->denseElementsAreCopyOnWrite()) {
        GCPtrNativeObject& owner =
            dstNative->getElementsHeader()->ownerObject();
        if (owner == srcNative) {
          owner = dstNative;
        }
      }
    } else if (srcObj->is<ProxyObject>()) {
      if (srcObj->as<ProxyObject>().usingInlineValueArray()) {
        dstObj->as<ProxyObject>().setInlineValueArray();
      }
    }

    // Call object moved hook if present.
    if (JSObjectMovedOp op = srcObj->getClass()->extObjectMovedOp()) {
      op(dstObj, srcObj);
    }

    MOZ_ASSERT_IF(
        dstObj->isNative(),
        !PtrIsInRange(
            (const Value*)dstObj->as<NativeObject>().getDenseElements(), src,
            thingSize));
  }

  // Copy the mark bits.
  dst->copyMarkBitsFrom(src);

  // Poison the source cell contents except for the forwarding flag and pointer
  // which will be stored in the first word. We can't do this for native object
  // with fixed elements because this would overwrite the element flags and
  // these are needed when updating COW elements referred to by other objects.
#ifdef DEBUG
  JSObject* srcObj = IsObjectAllocKind(thingKind)
                         ? static_cast<JSObject*>(static_cast<Cell*>(src))
                         : nullptr;
  if (!srcObj || !srcObj->isNative() ||
      !srcObj->as<NativeObject>().hasFixedElements()) {
    AlwaysPoison(reinterpret_cast<uint8_t*>(src) + sizeof(uintptr_t),
                 JS_MOVED_TENURED_PATTERN, thingSize - sizeof(uintptr_t),
                 MemCheckKind::MakeNoAccess);
  }
#endif

  // Mark source cell as forwarded and leave a pointer to the destination.
  RelocationOverlay::forwardCell(src, dst);
}

static void RelocateArena(Arena* arena, SliceBudget& sliceBudget) {
  MOZ_ASSERT(arena->allocated());
  MOZ_ASSERT(!arena->onDelayedMarkingList());
  MOZ_ASSERT(arena->bufferedCells()->isEmpty());

  Zone* zone = arena->zone;

  AllocKind thingKind = arena->getAllocKind();
  size_t thingSize = arena->getThingSize();

  for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
    RelocateCell(zone, i.getCell(), thingKind, thingSize);
    sliceBudget.step();
  }

#ifdef DEBUG
  for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
    TenuredCell* src = i.getCell();
    MOZ_ASSERT(src->isForwarded());
    TenuredCell* dest = Forwarded(src);
    MOZ_ASSERT(src->isMarkedBlack() == dest->isMarkedBlack());
    MOZ_ASSERT(src->isMarkedGray() == dest->isMarkedGray());
  }
#endif
}

#ifdef DEBUG
static inline bool CanProtectArenas() {
  // On some systems the page size is larger than the size of an arena so we
  // can't change the mapping permissions per arena.
  return SystemPageSize() <= ArenaSize;
}
#endif

static inline bool ShouldProtectRelocatedArenas(JS::GCReason reason) {
  // For zeal mode collections we don't release the relocated arenas
  // immediately. Instead we protect them and keep them around until the next
  // collection so we can catch any stray accesses to them.
#ifdef DEBUG
  return reason == JS::GCReason::DEBUG_GC && CanProtectArenas();
#else
  return false;
#endif
}

/*
 * Relocate all arenas identified by pickArenasToRelocate: for each arena,
 * relocate each cell within it, then add it to a list of relocated arenas.
 */
Arena* ArenaList::relocateArenas(Arena* toRelocate, Arena* relocated,
                                 SliceBudget& sliceBudget,
                                 gcstats::Statistics& stats) {
  check();

  while (Arena* arena = toRelocate) {
    toRelocate = arena->next;
    RelocateArena(arena, sliceBudget);
    // Prepend to list of relocated arenas
    arena->next = relocated;
    relocated = arena;
    stats.count(gcstats::COUNT_ARENA_RELOCATED);
  }

  check();

  return relocated;
}

// Skip compacting zones unless we can free a certain proportion of their GC
// heap memory.
static const float MIN_ZONE_RECLAIM_PERCENT = 2.0;

static bool ShouldRelocateZone(size_t arenaCount, size_t relocCount,
                               JS::GCReason reason) {
  if (relocCount == 0) {
    return false;
  }

  if (IsOOMReason(reason)) {
    return true;
  }

  return (relocCount * 100.0f) / arenaCount >= MIN_ZONE_RECLAIM_PERCENT;
}

static AllocKinds CompactingAllocKinds() {
  AllocKinds result;
  for (AllocKind kind : AllAllocKinds()) {
    if (IsCompactingKind(kind)) {
      result += kind;
    }
  }
  return result;
}

bool ArenaLists::relocateArenas(Arena*& relocatedListOut, JS::GCReason reason,
                                SliceBudget& sliceBudget,
                                gcstats::Statistics& stats) {
  // This is only called from the main thread while we are doing a GC, so
  // there is no need to lock.
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
  MOZ_ASSERT(runtime()->gc.isHeapCompacting());
  MOZ_ASSERT(!runtime()->gc.isBackgroundSweeping());

  // Relocate all compatible kinds
  AllocKinds allocKindsToRelocate = CompactingAllocKinds();

  // Clear all the free lists.
  clearFreeLists();

  if (ShouldRelocateAllArenas(reason)) {
    zone_->prepareForCompacting();
    for (auto kind : allocKindsToRelocate) {
      ArenaList& al = arenaLists(kind);
      Arena* allArenas = al.head();
      al.clear();
      relocatedListOut =
          al.relocateArenas(allArenas, relocatedListOut, sliceBudget, stats);
    }
  } else {
    size_t arenaCount = 0;
    size_t relocCount = 0;
    AllAllocKindArray<Arena**> toRelocate;

    for (auto kind : allocKindsToRelocate) {
      toRelocate[kind] =
          arenaLists(kind).pickArenasToRelocate(arenaCount, relocCount);
    }

    if (!ShouldRelocateZone(arenaCount, relocCount, reason)) {
      return false;
    }

    zone_->prepareForCompacting();
    for (auto kind : allocKindsToRelocate) {
      if (toRelocate[kind]) {
        ArenaList& al = arenaLists(kind);
        Arena* arenas = al.removeRemainingArenas(toRelocate[kind]);
        relocatedListOut =
            al.relocateArenas(arenas, relocatedListOut, sliceBudget, stats);
      }
    }
  }

  return true;
}

bool GCRuntime::relocateArenas(Zone* zone, JS::GCReason reason,
                               Arena*& relocatedListOut,
                               SliceBudget& sliceBudget) {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT_MOVE);

  MOZ_ASSERT(!zone->isPreservingCode());
  MOZ_ASSERT(canRelocateZone(zone));

  js::CancelOffThreadIonCompile(rt, JS::Zone::Compact);

  if (!zone->arenas.relocateArenas(relocatedListOut, reason, sliceBudget,
                                   stats())) {
    return false;
  }

#ifdef DEBUG
  // Check that we did as much compaction as we should have. There
  // should always be less than one arena's worth of free cells.
  for (auto kind : CompactingAllocKinds()) {
    ArenaList& al = zone->arenas.arenaLists(kind);
    size_t freeCells = 0;
    for (Arena* arena = al.arenaAfterCursor(); arena; arena = arena->next) {
      freeCells += arena->countFreeCells();
    }
    MOZ_ASSERT(freeCells < Arena::thingsPerArena(kind));
  }
#endif

  return true;
}

template <typename T>
inline bool MovingTracer::updateEdge(T** thingp) {
  auto thing = *thingp;
  if (thing->runtimeFromAnyThread() == runtime() && IsForwarded(thing)) {
    *thingp = Forwarded(thing);
  }

  return true;
}

bool MovingTracer::onObjectEdge(JSObject** objp) { return updateEdge(objp); }
bool MovingTracer::onShapeEdge(Shape** shapep) { return updateEdge(shapep); }
bool MovingTracer::onStringEdge(JSString** stringp) {
  return updateEdge(stringp);
}
bool MovingTracer::onScriptEdge(js::BaseScript** scriptp) {
  return updateEdge(scriptp);
}
bool MovingTracer::onBaseShapeEdge(BaseShape** basep) {
  return updateEdge(basep);
}
bool MovingTracer::onScopeEdge(Scope** scopep) { return updateEdge(scopep); }
bool MovingTracer::onRegExpSharedEdge(RegExpShared** sharedp) {
  return updateEdge(sharedp);
}
bool MovingTracer::onBigIntEdge(BigInt** bip) { return updateEdge(bip); }

void Zone::prepareForCompacting() {
  JSFreeOp* fop = runtimeFromMainThread()->defaultFreeOp();
  discardJitCode(fop);
}

void GCRuntime::sweepTypesAfterCompacting(Zone* zone) {
  zone->beginSweepTypes();

  AutoClearTypeInferenceStateOnOOM oom(zone);

  for (auto base = zone->cellIterUnsafe<BaseScript>(); !base.done();
       base.next()) {
    if (!base->hasJitScript()) {
      continue;
    }
    AutoSweepJitScript sweep(base->asJSScript());
  }
  for (auto group = zone->cellIterUnsafe<ObjectGroup>(); !group.done();
       group.next()) {
    AutoSweepObjectGroup sweep(group);
  }

  zone->types.endSweep(rt);
}

void GCRuntime::sweepZoneAfterCompacting(MovingTracer* trc, Zone* zone) {
  MOZ_ASSERT(zone->isCollecting());
  sweepTypesAfterCompacting(zone);
  sweepFinalizationRegistries(zone);
  zone->weakRefMap().sweep();
  zone->sweepWeakMaps();
  for (auto* cache : zone->weakCaches()) {
    cache->sweep();
  }

  if (jit::JitZone* jitZone = zone->jitZone()) {
    jitZone->traceWeak(trc);
  }

  for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
    r->traceWeakObjectGroups(trc);
    r->traceWeakRegExps(trc);
    r->traceWeakSavedStacks(trc);
    r->tracekWeakVarNames(trc);
    r->traceWeakObjects(trc);
    r->traceWeakSelfHostingScriptSource(trc);
    r->sweepDebugEnvironments();
    r->traceWeakEdgesInJitRealm(trc);
    r->traceWeakObjectRealm(trc);
    r->traceWeakTemplateObjects(trc);
  }
}

template <typename T>
static inline void UpdateCellPointers(MovingTracer* trc, T* cell) {
  // We only update unmoved GC things or the new copy of moved GC things, never
  // the old copy. If this happened it could clear the forwarded flag which
  // could lead to pointers to the old copy not being updated.
  MOZ_ASSERT(!cell->isForwarded());

  cell->fixupAfterMovingGC();
  cell->traceChildren(trc);
}

template <typename T>
static void UpdateArenaPointersTyped(MovingTracer* trc, Arena* arena) {
  for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
    UpdateCellPointers(trc, reinterpret_cast<T*>(i.getCell()));
  }
}

static bool CanUpdateKindInBackground(AllocKind kind) {
  // We try to update as many GC things in parallel as we can, but there are
  // kinds for which this might not be safe:
  //  - we assume JSObjects that are foreground finalized are not safe to
  //    update in parallel
  //  - updating a shape touches child shapes in fixupShapeTreeAfterMovingGC()
  return js::gc::IsBackgroundFinalized(kind) && !IsShapeAllocKind(kind) &&
         kind != AllocKind::BASE_SHAPE;
}

/*
 * Update the internal pointers for all cells in an arena.
 */
static void UpdateArenaPointers(MovingTracer* trc, Arena* arena) {
  AllocKind kind = arena->getAllocKind();

  MOZ_ASSERT_IF(!CanUpdateKindInBackground(kind),
                CurrentThreadCanAccessRuntime(trc->runtime()));

  switch (kind) {
#define EXPAND_CASE(allocKind, traceKind, type, sizedType, bgFinal, nursery, \
                    compact)                                                 \
  case AllocKind::allocKind:                                                 \
    UpdateArenaPointersTyped<type>(trc, arena);                              \
    return;
    FOR_EACH_ALLOCKIND(EXPAND_CASE)
#undef EXPAND_CASE

    default:
      MOZ_CRASH("Invalid alloc kind for UpdateArenaPointers");
  }
}

struct ArenaListSegment {
  Arena* begin;
  Arena* end;
};

/*
 * Update the internal pointers for all arenas in a segment of an arena list.
 *
 * Returns the number of steps to count against the slice budget.
 */
static size_t UpdateArenaListSegmentPointers(GCRuntime* gc,
                                             const ArenaListSegment& arenas) {
  MOZ_ASSERT(arenas.begin);
  MovingTracer trc(gc->rt);
  size_t count = 0;
  for (Arena* arena = arenas.begin; arena != arenas.end; arena = arena->next) {
    UpdateArenaPointers(&trc, arena);
    count++;
  }
  return count * 256;
}

class ArenasToUpdate {
  // Maximum number of arenas to update in one block.
#ifdef DEBUG
  static const unsigned MaxArenasToProcess = 16;
#else
  static const unsigned MaxArenasToProcess = 256;
#endif

 public:
  explicit ArenasToUpdate(Zone* zone);
  ArenasToUpdate(Zone* zone, const AllocKinds& kinds);

  bool done() const { return !segmentBegin; }

  ArenaListSegment get() const {
    MOZ_ASSERT(!done());
    return {segmentBegin, segmentEnd};
  }

  void next();

 private:
  Maybe<AllocKinds> kinds;            // Selects which thing kinds to update.
  Zone* zone;                         // Zone to process.
  AllocKind kind = AllocKind::FIRST;  // Current alloc kind to process.
  Arena* segmentBegin = nullptr;
  Arena* segmentEnd = nullptr;

  static AllocKind nextAllocKind(AllocKind i) {
    return AllocKind(uint8_t(i) + 1);
  }

  void settle();
  void findSegmentEnd();
};

ArenasToUpdate::ArenasToUpdate(Zone* zone) : zone(zone) { settle(); }

ArenasToUpdate::ArenasToUpdate(Zone* zone, const AllocKinds& kinds)
    : kinds(Some(kinds)), zone(zone) {
  settle();
}

void ArenasToUpdate::settle() {
  // Called when we have set |kind| to a new kind. Sets |arena| to the next
  // arena or null if there are no more arenas to update.

  MOZ_ASSERT(!segmentBegin);

  for (; kind < AllocKind::LIMIT; kind = nextAllocKind(kind)) {
    if (kinds && !kinds.ref().contains(kind)) {
      continue;
    }

    Arena* arena = zone->arenas.getFirstArena(kind);
    if (arena) {
      segmentBegin = arena;
      findSegmentEnd();
      break;
    }
  }
}

void ArenasToUpdate::findSegmentEnd() {
  // Take up to MaxArenasToProcess arenas from the list starting at
  // |segmentBegin| and set |segmentEnd|.
  Arena* arena = segmentBegin;
  for (size_t i = 0; arena && i < MaxArenasToProcess; i++) {
    arena = arena->next;
  }
  segmentEnd = arena;
}

void ArenasToUpdate::next() {
  MOZ_ASSERT(!done());

  segmentBegin = segmentEnd;
  if (segmentBegin) {
    findSegmentEnd();
    return;
  }

  kind = nextAllocKind(kind);
  settle();
}

static AllocKinds ForegroundUpdateKinds(AllocKinds kinds) {
  AllocKinds result;
  for (AllocKind kind : kinds) {
    if (!CanUpdateKindInBackground(kind)) {
      result += kind;
    }
  }
  return result;
}

void GCRuntime::updateTypeDescrObjects(MovingTracer* trc, Zone* zone) {
  // We need to update each type descriptor object and any objects stored in
  // its reserved slots, since some of these contain array objects that also
  // need to be updated. Do not update any non-reserved slots, since they might
  // point back to unprocessed descriptor objects.

  zone->typeDescrObjects().sweep();

  for (auto r = zone->typeDescrObjects().all(); !r.empty(); r.popFront()) {
    NativeObject* obj = &r.front()->as<NativeObject>();
    UpdateCellPointers(trc, obj);
    MOZ_ASSERT(JSCLASS_RESERVED_SLOTS(obj->getClass()) == JS_DESCR_SLOTS);
    for (size_t i = 0; i < JS_DESCR_SLOTS; i++) {
      Value value = obj->getSlot(i);
      if (value.isObject()) {
        UpdateCellPointers(trc, &value.toObject());
      }
    }
  }
}

void GCRuntime::updateCellPointers(Zone* zone, AllocKinds kinds) {
  AllocKinds fgKinds = ForegroundUpdateKinds(kinds);
  AllocKinds bgKinds = kinds - fgKinds;

  ArenasToUpdate fgArenas(zone, fgKinds);
  ArenasToUpdate bgArenas(zone, bgKinds);

  AutoLockHelperThreadState lock;

  AutoRunParallelWork bgTasks(this, UpdateArenaListSegmentPointers,
                              gcstats::PhaseKind::COMPACT_UPDATE_CELLS,
                              bgArenas, SliceBudget::unlimited(), lock);

  ParallelWorker fgTask(this, UpdateArenaListSegmentPointers, fgArenas,
                        SliceBudget::unlimited(), lock);

  {
    AutoUnlockHelperThreadState unlock(lock);
    fgTask.runFromMainThread();
  }
}

// After cells have been relocated any pointers to a cell's old locations must
// be updated to point to the new location.  This happens by iterating through
// all cells in heap and tracing their children (non-recursively) to update
// them.
//
// This is complicated by the fact that updating a GC thing sometimes depends on
// making use of other GC things.  After a moving GC these things may not be in
// a valid state since they may contain pointers which have not been updated
// yet.
//
// The main dependencies are:
//
//   - Updating a JSObject makes use of its shape
//   - Updating a typed object makes use of its type descriptor object
//
// This means we require at least three phases for update:
//
//  1) shapes
//  2) typed object type descriptor objects
//  3) all other objects
//
// Also, there can be data races calling IsForwarded() on the new location of a
// cell that is being updated in parallel on another thread. This can be avoided
// by updating some kinds of cells in different phases. This is done for
// BaseScripts and Scopes.
//
// Since we want to minimize the number of phases, arrange kinds into three
// arbitrary phases.

static constexpr AllocKinds UpdatePhaseOne{
    AllocKind::SCRIPT,         AllocKind::BASE_SHAPE,   AllocKind::SHAPE,
    AllocKind::ACCESSOR_SHAPE, AllocKind::OBJECT_GROUP, AllocKind::STRING,
    AllocKind::JITCODE};

// UpdatePhaseTwo is typed object descriptor objects.

static constexpr AllocKinds UpdatePhaseThree{AllocKind::SCOPE,
                                             AllocKind::FUNCTION,
                                             AllocKind::FUNCTION_EXTENDED,
                                             AllocKind::OBJECT0,
                                             AllocKind::OBJECT0_BACKGROUND,
                                             AllocKind::OBJECT2,
                                             AllocKind::OBJECT2_BACKGROUND,
                                             AllocKind::ARRAYBUFFER4,
                                             AllocKind::OBJECT4,
                                             AllocKind::OBJECT4_BACKGROUND,
                                             AllocKind::ARRAYBUFFER8,
                                             AllocKind::OBJECT8,
                                             AllocKind::OBJECT8_BACKGROUND,
                                             AllocKind::ARRAYBUFFER12,
                                             AllocKind::OBJECT12,
                                             AllocKind::OBJECT12_BACKGROUND,
                                             AllocKind::ARRAYBUFFER16,
                                             AllocKind::OBJECT16,
                                             AllocKind::OBJECT16_BACKGROUND};

void GCRuntime::updateAllCellPointers(MovingTracer* trc, Zone* zone) {
  updateCellPointers(zone, UpdatePhaseOne);

  // UpdatePhaseTwo: Update TypeDescrs before all other objects as typed
  // objects access these objects when we trace them.
  updateTypeDescrObjects(trc, zone);

  updateCellPointers(zone, UpdatePhaseThree);
}

/*
 * Update pointers to relocated cells in a single zone by doing a traversal of
 * that zone's arenas and calling per-zone sweep hooks.
 *
 * The latter is necessary to update weak references which are not marked as
 * part of the traversal.
 */
void GCRuntime::updateZonePointersToRelocatedCells(Zone* zone) {
  MOZ_ASSERT(!rt->isBeingDestroyed());
  MOZ_ASSERT(zone->isGCCompacting());

  AutoTouchingGrayThings tgt;

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT_UPDATE);
  MovingTracer trc(rt);

  zone->fixupAfterMovingGC();
  zone->fixupScriptMapsAfterMovingGC(&trc);

  // Fixup compartment global pointers as these get accessed during marking.
  for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
    comp->fixupAfterMovingGC(&trc);
  }

  zone->externalStringCache().purge();
  zone->functionToStringCache().purge();

  // Iterate through all cells that can contain relocatable pointers to update
  // them. Since updating each cell is independent we try to parallelize this
  // as much as possible.
  updateAllCellPointers(&trc, zone);

  // Mark roots to update them.
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);

    WeakMapBase::traceZone(zone, &trc);
  }

  // Sweep everything to fix up weak pointers.
  sweepZoneAfterCompacting(&trc, zone);

  // Call callbacks to get the rest of the system to fixup other untraced
  // pointers.
  for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
    callWeakPointerCompartmentCallbacks(comp);
  }
}

/*
 * Update runtime-wide pointers to relocated cells.
 */
void GCRuntime::updateRuntimePointersToRelocatedCells(AutoGCSession& session) {
  MOZ_ASSERT(!rt->isBeingDestroyed());

  gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::COMPACT_UPDATE);
  MovingTracer trc(rt);

  Zone::fixupAllCrossCompartmentWrappersAfterMovingGC(&trc);

  rt->geckoProfiler().fixupStringsMapAfterMovingGC();

  // Mark roots to update them.

  traceRuntimeForMajorGC(&trc, session);

  {
    gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::MARK_ROOTS);
    DebugAPI::traceAllForMovingGC(&trc);
    DebugAPI::traceCrossCompartmentEdges(&trc);

    // Mark all gray roots. We call the trace callback to get the current set.
    traceEmbeddingGrayRoots(&trc);
    Compartment::traceIncomingCrossCompartmentEdgesForZoneGC(
        &trc, Compartment::GrayEdges);
  }

  // Sweep everything to fix up weak pointers.
  DebugAPI::sweepAll(rt->defaultFreeOp());
  jit::JitRuntime::TraceWeakJitcodeGlobalTable(rt, &trc);
  for (JS::detail::WeakCacheBase* cache : rt->weakCaches()) {
    cache->sweep();
  }

  // Type inference may put more blocks here to free.
  {
    AutoLockHelperThreadState lock;
    lifoBlocksToFree.ref().freeAll();
  }

  // Call callbacks to get the rest of the system to fixup other untraced
  // pointers.
  callWeakPointerZonesCallbacks();
}

void GCRuntime::clearRelocatedArenas(Arena* arenaList, JS::GCReason reason) {
  AutoLockGC lock(this);
  clearRelocatedArenasWithoutUnlocking(arenaList, reason, lock);
}

void GCRuntime::clearRelocatedArenasWithoutUnlocking(Arena* arenaList,
                                                     JS::GCReason reason,
                                                     const AutoLockGC& lock) {
  // Clear the relocated arenas, now containing only forwarding pointers
  while (arenaList) {
    Arena* arena = arenaList;
    arenaList = arenaList->next;

    // Clear the mark bits
    arena->unmarkAll();

    // Mark arena as empty
    arena->setAsFullyUnused();

#ifdef DEBUG
    // The cell contents have been partially marked no access in RelocateCell,
    // so we need to mark the region as undefined again so we can poison it.
    SetMemCheckKind(reinterpret_cast<void*>(arena->thingsStart()),
                    arena->getThingsSpan(), MemCheckKind::MakeUndefined);
#endif

    AlwaysPoison(reinterpret_cast<void*>(arena->thingsStart()),
                 JS_MOVED_TENURED_PATTERN, arena->getThingsSpan(),
                 MemCheckKind::MakeNoAccess);

    // Don't count arenas as being freed by the GC if we purposely moved
    // everything to new arenas, as that will already have allocated a similar
    // number of arenas. This only happens for collections triggered by GC zeal.
    bool allArenasRelocated = ShouldRelocateAllArenas(reason);
    arena->zone->gcHeapSize.removeBytes(ArenaSize, !allArenasRelocated);

    // Release the arena but don't return it to the chunk yet.
    arena->release(lock);
  }
}

void GCRuntime::protectAndHoldArenas(Arena* arenaList) {
  for (Arena* arena = arenaList; arena;) {
    MOZ_ASSERT(!arena->allocated());
    Arena* next = arena->next;
    if (!next) {
      // Prepend to hold list before we protect the memory.
      arena->next = relocatedArenasToRelease;
      relocatedArenasToRelease = arenaList;
    }
    ProtectPages(arena, ArenaSize);
    arena = next;
  }
}

void GCRuntime::unprotectHeldRelocatedArenas() {
  for (Arena* arena = relocatedArenasToRelease; arena; arena = arena->next) {
    UnprotectPages(arena, ArenaSize);
    MOZ_ASSERT(!arena->allocated());
  }
}

void GCRuntime::releaseRelocatedArenas(Arena* arenaList) {
  AutoLockGC lock(this);
  releaseRelocatedArenasWithoutUnlocking(arenaList, lock);
}

void GCRuntime::releaseRelocatedArenasWithoutUnlocking(Arena* arenaList,
                                                       const AutoLockGC& lock) {
  // Release relocated arenas previously cleared with clearRelocatedArenas().
  while (arenaList) {
    Arena* arena = arenaList;
    arenaList = arenaList->next;

    // We already updated the memory accounting so just call
    // Chunk::releaseArena.
    arena->chunk()->releaseArena(this, arena, lock);
  }
}

// In debug mode we don't always release relocated arenas straight away.
// Sometimes protect them instead and hold onto them until the next GC sweep
// phase to catch any pointers to them that didn't get forwarded.

void GCRuntime::releaseHeldRelocatedArenas() {
#ifdef DEBUG
  unprotectHeldRelocatedArenas();
  Arena* arenas = relocatedArenasToRelease;
  relocatedArenasToRelease = nullptr;
  releaseRelocatedArenas(arenas);
#endif
}

void GCRuntime::releaseHeldRelocatedArenasWithoutUnlocking(
    const AutoLockGC& lock) {
#ifdef DEBUG
  unprotectHeldRelocatedArenas();
  releaseRelocatedArenasWithoutUnlocking(relocatedArenasToRelease, lock);
  relocatedArenasToRelease = nullptr;
#endif
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
      arenaListsToSweep_(),
      incrementalSweptArenaKind(zone, AllocKind::LIMIT),
      incrementalSweptArenas(zone),
      gcShapeArenasToUpdate(zone, nullptr),
      gcAccessorShapeArenasToUpdate(zone, nullptr),
      gcScriptArenasToUpdate(zone, nullptr),
      gcObjectGroupArenasToUpdate(zone, nullptr),
      savedEmptyArenas(zone, nullptr) {
  for (auto i : AllAllocKinds()) {
    concurrentUse(i) = ConcurrentUse::None;
    arenaListsToSweep(i) = nullptr;
  }
}

void ReleaseArenaList(JSRuntime* rt, Arena* arena, const AutoLockGC& lock) {
  Arena* next;
  for (; arena; arena = next) {
    next = arena->next;
    rt->gc.releaseArena(arena, lock);
  }
}

ArenaLists::~ArenaLists() {
  AutoLockGC lock(runtime());

  for (auto i : AllAllocKinds()) {
    /*
     * We can only call this during the shutdown after the last GC when
     * the background finalization is disabled.
     */
    MOZ_ASSERT(concurrentUse(i) == ConcurrentUse::None);
    ReleaseArenaList(runtime(), arenaLists(i).head(), lock);
  }
  ReleaseArenaList(runtime(), incrementalSweptArenas.ref().head(), lock);

  ReleaseArenaList(runtime(), savedEmptyArenas, lock);
}

void ArenaLists::queueForForegroundSweep(JSFreeOp* fop,
                                         const FinalizePhase& phase) {
  gcstats::AutoPhase ap(fop->runtime()->gc.stats(), phase.statsPhase);
  for (auto kind : phase.kinds) {
    queueForForegroundSweep(kind);
  }
}

void ArenaLists::queueForForegroundSweep(AllocKind thingKind) {
  MOZ_ASSERT(!IsBackgroundFinalized(thingKind));
  MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);
  MOZ_ASSERT(!arenaListsToSweep(thingKind));

  arenaListsToSweep(thingKind) = arenaLists(thingKind).head();
  arenaLists(thingKind).clear();
}

void ArenaLists::queueForBackgroundSweep(JSFreeOp* fop,
                                         const FinalizePhase& phase) {
  gcstats::AutoPhase ap(fop->runtime()->gc.stats(), phase.statsPhase);
  for (auto kind : phase.kinds) {
    queueForBackgroundSweep(kind);
  }
}

inline void ArenaLists::queueForBackgroundSweep(AllocKind thingKind) {
  MOZ_ASSERT(IsBackgroundFinalized(thingKind));

  ArenaList* al = &arenaLists(thingKind);
  if (al->isEmpty()) {
    MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);
    return;
  }

  MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);

  arenaListsToSweep(thingKind) = al->head();
  al->clear();
  concurrentUse(thingKind) = ConcurrentUse::BackgroundFinalize;
}

/*static*/
void ArenaLists::backgroundFinalize(JSFreeOp* fop, Arena* listHead,
                                    Arena** empty) {
  MOZ_ASSERT(listHead);
  MOZ_ASSERT(empty);

  AllocKind thingKind = listHead->getAllocKind();
  Zone* zone = listHead->zone;

  size_t thingsPerArena = Arena::thingsPerArena(thingKind);
  SortedArenaList finalizedSorted(thingsPerArena);

  auto unlimited = SliceBudget::unlimited();
  FinalizeArenas(fop, &listHead, finalizedSorted, thingKind, unlimited);
  MOZ_ASSERT(!listHead);

  finalizedSorted.extractEmpty(empty);

  // When arenas are queued for background finalization, all arenas are moved
  // to arenaListsToSweep[], leaving the arenaLists[] empty. However, new
  // arenas may be allocated before background finalization finishes; now that
  // finalization is complete, we want to merge these lists back together.
  ArenaLists* lists = &zone->arenas;
  ArenaList* al = &lists->arenaLists(thingKind);

  // Flatten |finalizedSorted| into a regular ArenaList.
  ArenaList finalized = finalizedSorted.toArenaList();

  // We must take the GC lock to be able to safely modify the ArenaList;
  // however, this does not by itself make the changes visible to all threads,
  // as not all threads take the GC lock to read the ArenaLists.
  // That safety is provided by the ReleaseAcquire memory ordering of the
  // background finalize state, which we explicitly set as the final step.
  {
    AutoLockGC lock(lists->runtimeFromAnyThread());
    MOZ_ASSERT(lists->concurrentUse(thingKind) ==
               ConcurrentUse::BackgroundFinalize);

    // Join |al| and |finalized| into a single list.
    *al = finalized.insertListWithCursorAtEnd(*al);

    lists->arenaListsToSweep(thingKind) = nullptr;
  }

  lists->concurrentUse(thingKind) = ConcurrentUse::None;
}

void ArenaLists::releaseForegroundSweptEmptyArenas() {
  AutoLockGC lock(runtime());
  ReleaseArenaList(runtime(), savedEmptyArenas, lock);
  savedEmptyArenas = nullptr;
}

void ArenaLists::queueForegroundThingsForSweep() {
  gcShapeArenasToUpdate = arenaListsToSweep(AllocKind::SHAPE);
  gcAccessorShapeArenasToUpdate = arenaListsToSweep(AllocKind::ACCESSOR_SHAPE);
  gcObjectGroupArenasToUpdate = arenaListsToSweep(AllocKind::OBJECT_GROUP);
  gcScriptArenasToUpdate = arenaListsToSweep(AllocKind::SCRIPT);
}

TimeStamp SliceBudget::unlimitedDeadline;

void SliceBudget::Init() {
  MOZ_ASSERT(!unlimitedDeadline);
  uint64_t oneYearsInSeconds = 365 * 24 * 60 * 60;
  unlimitedDeadline =
      ReallyNow() + TimeDuration::FromSeconds(100 * oneYearsInSeconds);
}

SliceBudget::SliceBudget()
    : timeBudget(UnlimitedTimeBudget), workBudget(UnlimitedWorkBudget) {
  makeUnlimited();
}

SliceBudget::SliceBudget(TimeBudget time)
    : timeBudget(time), workBudget(UnlimitedWorkBudget) {
  if (time.budget < 0) {
    makeUnlimited();
  } else {
    // Note: TimeBudget(0) is equivalent to WorkBudget(CounterReset).
    deadline = ReallyNow() + TimeDuration::FromMilliseconds(time.budget);
    counter = CounterReset;
  }
}

SliceBudget::SliceBudget(WorkBudget work)
    : timeBudget(UnlimitedTimeBudget), workBudget(work) {
  if (work.budget < 0) {
    makeUnlimited();
  } else {
    deadline = TimeStamp();
    counter = work.budget;
  }
}

int SliceBudget::describe(char* buffer, size_t maxlen) const {
  if (isUnlimited()) {
    return snprintf(buffer, maxlen, "unlimited");
  } else if (isWorkBudget()) {
    return snprintf(buffer, maxlen, "work(%" PRId64 ")", workBudget.budget);
  } else {
    return snprintf(buffer, maxlen, "%" PRId64 "ms", timeBudget.budget);
  }
}

bool SliceBudget::checkOverBudget() {
  if (deadline.IsNull()) {
    return true;
  }

  bool over = ReallyNow() >= deadline;
  if (!over) {
    counter = CounterReset;
  }
  return over;
}

void GCRuntime::requestMajorGC(JS::GCReason reason) {
  MOZ_ASSERT(!CurrentThreadIsPerformingGC());

  if (majorGCRequested()) {
    return;
  }

  majorGCTriggerReason = reason;
  rt->mainContextFromOwnThread()->requestInterrupt(InterruptReason::GC);
}

void Nursery::requestMinorGC(JS::GCReason reason) const {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
  MOZ_ASSERT(!CurrentThreadIsPerformingGC());

  if (minorGCRequested()) {
    return;
  }

  minorGCTriggerReason_ = reason;
  runtime()->mainContextFromOwnThread()->requestInterrupt(InterruptReason::GC);
}

bool GCRuntime::triggerGC(JS::GCReason reason) {
  /*
   * Don't trigger GCs if this is being called off the main thread from
   * onTooMuchMalloc().
   */
  if (!CurrentThreadCanAccessRuntime(rt)) {
    return false;
  }

  /* GC is already running. */
  if (JS::RuntimeHeapIsCollecting()) {
    return false;
  }

  JS::PrepareForFullGC(rt->mainContextFromOwnThread());
  requestMajorGC(reason);
  return true;
}

void GCRuntime::maybeAllocTriggerZoneGC(Zone* zone, size_t nbytes) {
  if (!CurrentThreadCanAccessRuntime(rt)) {
    // Zones in use by a helper thread can't be collected.
    MOZ_ASSERT(zone->usedByHelperThread() || zone->isAtomsZone());
    return;
  }

  MOZ_ASSERT(!JS::RuntimeHeapIsCollecting());

  TriggerResult trigger = checkHeapThreshold(
      zone->gcHeapSize, zone->gcHeapThreshold, zone->isCollecting());

  if (trigger.kind == TriggerKind::None) {
    return;
  }

  if (trigger.kind == TriggerKind::NonIncremental) {
    triggerZoneGC(zone, JS::GCReason::ALLOC_TRIGGER, trigger.usedBytes,
                  trigger.thresholdBytes);
    return;
  }

  MOZ_ASSERT(trigger.kind == TriggerKind::Incremental);

  // During an incremental GC, reduce the delay to the start of the next
  // incremental slice.
  if (zone->gcDelayBytes < nbytes) {
    zone->gcDelayBytes = 0;
  } else {
    zone->gcDelayBytes -= nbytes;
  }

  if (!zone->gcDelayBytes) {
    // Start or continue an in progress incremental GC. We do this
    // to try to avoid performing non-incremental GCs on zones
    // which allocate a lot of data, even when incremental slices
    // can't be triggered via scheduling in the event loop.
    triggerZoneGC(zone, JS::GCReason::INCREMENTAL_ALLOC_TRIGGER,
                  trigger.usedBytes, trigger.thresholdBytes);

    // Delay the next slice until a certain amount of allocation
    // has been performed.
    zone->gcDelayBytes = tunables.zoneAllocDelayBytes();
  }
}

void js::gc::MaybeMallocTriggerZoneGC(JSRuntime* rt, ZoneAllocator* zoneAlloc,
                                      const HeapSize& heap,
                                      const HeapThreshold& threshold,
                                      JS::GCReason reason) {
  rt->gc.maybeMallocTriggerZoneGC(Zone::from(zoneAlloc), heap, threshold,
                                  reason);
}

void GCRuntime::maybeMallocTriggerZoneGC(Zone* zone) {
  if (maybeMallocTriggerZoneGC(zone, zone->mallocHeapSize,
                               zone->mallocHeapThreshold,
                               JS::GCReason::TOO_MUCH_MALLOC)) {
    return;
  }

  maybeMallocTriggerZoneGC(zone, zone->jitHeapSize, zone->jitHeapThreshold,
                           JS::GCReason::TOO_MUCH_JIT_CODE);
}

bool GCRuntime::maybeMallocTriggerZoneGC(Zone* zone, const HeapSize& heap,
                                         const HeapThreshold& threshold,
                                         JS::GCReason reason) {
  if (!CurrentThreadCanAccessRuntime(rt)) {
    // Zones in use by a helper thread can't be collected.
    MOZ_ASSERT(zone->usedByHelperThread() || zone->isAtomsZone() ||
               JS::RuntimeHeapIsBusy());
    return false;
  }

  if (rt->heapState() != JS::HeapState::Idle) {
    return false;
  }

  TriggerResult trigger =
      checkHeapThreshold(heap, threshold, zone->isCollecting());
  if (trigger.kind == TriggerKind::None) {
    return false;
  }

  if (trigger.kind == TriggerKind::Incremental && zone->wasGCStarted()) {
    // Don't start subsequent incremental slices if we're already collecting
    // this zone. This is different to our behaviour for GC allocation in
    // maybeAllocTriggerZoneGC.
    MOZ_ASSERT(isIncrementalGCInProgress());
    return false;
  }

  // Trigger a zone GC. budgetIncrementalGC() will work out whether to do an
  // incremental or non-incremental collection.
  triggerZoneGC(zone, reason, trigger.usedBytes, trigger.thresholdBytes);
  return true;
}

TriggerResult GCRuntime::checkHeapThreshold(const HeapSize& heapSize,
                                            const HeapThreshold& heapThreshold,
                                            bool isCollecting) {
  size_t usedBytes = heapSize.bytes();
  size_t thresholdBytes = heapThreshold.bytes();
  if (usedBytes < thresholdBytes) {
    return TriggerResult{TriggerKind::None, 0, 0};
  }

  size_t niThreshold = thresholdBytes * tunables.nonIncrementalFactor();
  if (usedBytes >= niThreshold) {
    // We have passed the non-incremental threshold: immediately trigger a
    // non-incremental GC.
    return TriggerResult{TriggerKind::NonIncremental, usedBytes, niThreshold};
  }

  // Use a higher threshold if starting a GC would reset an in-progress
  // collection.
  if (isIncrementalGCInProgress() && !isCollecting &&
      usedBytes < thresholdBytes * tunables.avoidInterruptFactor()) {
    return TriggerResult{TriggerKind::None, 0, 0};
  }

  // Start or continue an in progress incremental GC.
  return TriggerResult{TriggerKind::Incremental, usedBytes, thresholdBytes};
}

bool GCRuntime::triggerZoneGC(Zone* zone, JS::GCReason reason, size_t used,
                              size_t threshold) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

  /* GC is already running. */
  if (JS::RuntimeHeapIsBusy()) {
    return false;
  }

#ifdef JS_GC_ZEAL
  if (hasZealMode(ZealMode::Alloc)) {
    MOZ_RELEASE_ASSERT(triggerGC(reason));
    return true;
  }
#endif

  if (zone->isAtomsZone()) {
    /* We can't do a zone GC of just the atoms zone. */
    if (rt->hasHelperThreadZones()) {
      /* We can't collect atoms while off-thread parsing is allocating. */
      fullGCForAtomsRequested_ = true;
      return false;
    }
    stats().recordTrigger(used, threshold);
    MOZ_RELEASE_ASSERT(triggerGC(reason));
    return true;
  }

  stats().recordTrigger(used, threshold);
  PrepareZoneForGC(zone);
  requestMajorGC(reason);
  return true;
}

void GCRuntime::maybeGC() {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

#ifdef JS_GC_ZEAL
  if (hasZealMode(ZealMode::Alloc) || hasZealMode(ZealMode::RootsChange)) {
    JS::PrepareForFullGC(rt->mainContextFromOwnThread());
    gc(GC_NORMAL, JS::GCReason::DEBUG_GC);
    return;
  }
#endif

  if (gcIfRequested()) {
    return;
  }

  if (isIncrementalGCInProgress()) {
    return;
  }

  bool scheduledZones = false;
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    if (checkEagerAllocTrigger(zone->gcHeapSize, zone->gcHeapThreshold) ||
        checkEagerAllocTrigger(zone->mallocHeapSize,
                               zone->mallocHeapThreshold)) {
      zone->scheduleGC();
      scheduledZones = true;
    }
  }

  if (scheduledZones) {
    startGC(GC_NORMAL, JS::GCReason::EAGER_ALLOC_TRIGGER);
  }
}

bool GCRuntime::checkEagerAllocTrigger(const HeapSize& size,
                                       const HeapThreshold& threshold) {
  float thresholdBytes =
      threshold.eagerAllocTrigger(schedulingState.inHighFrequencyGCMode());
  float usedBytes = size.bytes();
  if (usedBytes <= 1024 * 1024 || usedBytes < thresholdBytes) {
    return false;
  }

  stats().recordTrigger(usedBytes, thresholdBytes);
  return true;
}

void GCRuntime::triggerFullGCForAtoms(JSContext* cx) {
  MOZ_ASSERT(fullGCForAtomsRequested_);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  MOZ_ASSERT(!JS::RuntimeHeapIsCollecting());
  MOZ_ASSERT(cx->canCollectAtoms());
  fullGCForAtomsRequested_ = false;
  MOZ_RELEASE_ASSERT(triggerGC(JS::GCReason::DELAYED_ATOMS_GC));
}

// Do all possible decommit immediately from the current thread without
// releasing the GC lock or allocating any memory.
void GCRuntime::decommitFreeArenasWithoutUnlocking(const AutoLockGC& lock) {
  MOZ_ASSERT(emptyChunks(lock).count() == 0);
  for (ChunkPool::Iter chunk(availableChunks(lock)); !chunk.done();
       chunk.next()) {
    chunk->decommitFreeArenasWithoutUnlocking(lock);
  }
  MOZ_ASSERT(availableChunks(lock).verify());
}

void GCRuntime::startDecommit() {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::DECOMMIT);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  MOZ_ASSERT(decommitTask.isIdle());

  // If we are allocating heavily enough to trigger "high frequency" GC, then
  // skip decommit so that we do not compete with the mutator. However if we're
  // doing a shrinking GC we always decommit to release as much memory as
  // possible.
  if (schedulingState.inHighFrequencyGCMode() && !cleanUpEverything) {
    return;
  }

  BackgroundDecommitTask::ChunkVector toDecommit;
  {
    AutoLockGC lock(this);

    // Verify that all entries in the empty chunks pool are already decommitted.
    for (ChunkPool::Iter chunk(emptyChunks(lock)); !chunk.done();
         chunk.next()) {
      MOZ_ASSERT(!chunk->info.numArenasFreeCommitted);
    }

    // Since we release the GC lock while doing the decommit syscall below,
    // it is dangerous to iterate the available list directly, as the active
    // thread could modify it concurrently. Instead, we build and pass an
    // explicit Vector containing the Chunks we want to visit.
    MOZ_ASSERT(availableChunks(lock).verify());
    availableChunks(lock).sort();
    for (ChunkPool::Iter iter(availableChunks(lock)); !iter.done();
         iter.next()) {
      if (!toDecommit.append(iter.get())) {
        // The OOM handler does a full, immediate decommit.
        return onOutOfMallocMemory(lock);
      }
    }
  }
  decommitTask.setChunksToScan(toDecommit);

  if (sweepOnBackgroundThread) {
    decommitTask.start();
    return;
  }

  decommitTask.runFromMainThread();
}

void js::gc::BackgroundDecommitTask::setChunksToScan(ChunkVector& chunks) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(gc->rt));
  MOZ_ASSERT(isIdle());
  MOZ_ASSERT(toDecommit.ref().empty());
  std::swap(toDecommit.ref(), chunks);
}

void js::gc::BackgroundDecommitTask::run() {
  AutoLockGC lock(gc);

  for (Chunk* chunk : toDecommit.ref()) {
    // The arena list is not doubly-linked, so we have to work in the free
    // list order and not in the natural order.
    while (chunk->info.numArenasFreeCommitted) {
      bool ok = chunk->decommitOneFreeArena(gc, lock);

      // If we are low enough on memory that we can't update the page
      // tables, or if we need to return for any other reason, break out
      // of the loop.
      if (cancel_ || !ok) {
        break;
      }
    }
  }
  toDecommit.ref().clearAndFree();

  ChunkPool toFree = gc->expireEmptyChunkPool(lock);
  if (toFree.count()) {
    AutoUnlockGC unlock(lock);
    FreeChunkPool(toFree);
  }
}

void GCRuntime::sweepBackgroundThings(ZoneList& zones, LifoAlloc& freeBlocks) {
  freeBlocks.freeAll();

  if (zones.isEmpty()) {
    return;
  }

  JSFreeOp fop(nullptr);

  // Sweep zones in order. The atoms zone must be finalized last as other
  // zones may have direct pointers into it.
  while (!zones.isEmpty()) {
    Zone* zone = zones.removeFront();
    Arena* emptyArenas = nullptr;

    AutoSetThreadIsSweeping threadIsSweeping(zone);

    // We must finalize thing kinds in the order specified by
    // BackgroundFinalizePhases.
    for (auto phase : BackgroundFinalizePhases) {
      for (auto kind : phase.kinds) {
        Arena* arenas = zone->arenas.arenaListsToSweep(kind);
        MOZ_RELEASE_ASSERT(uintptr_t(arenas) != uintptr_t(-1));
        if (arenas) {
          ArenaLists::backgroundFinalize(&fop, arenas, &emptyArenas);
        }
      }
    }

    // Release any arenas that are now empty.
    //
    // Periodically drop and reaquire the GC lock every so often to avoid
    // blocking the main thread from allocating chunks.
    //
    // Also use this opportunity to periodically recalculate the GC thresholds
    // as we free more memory.
    static const size_t LockReleasePeriod = 32;

    while (emptyArenas) {
      AutoLockGC lock(this);
      for (size_t i = 0; i < LockReleasePeriod && emptyArenas; i++) {
        Arena* arena = emptyArenas;
        emptyArenas = emptyArenas->next;
        releaseArena(arena, lock);
      }
      zone->updateGCThresholds(*this, invocationKind, lock);
    }
  }
}

void GCRuntime::assertBackgroundSweepingFinished() {
#ifdef DEBUG
  {
    AutoLockHelperThreadState lock;
    MOZ_ASSERT(backgroundSweepZones.ref().isEmpty());
  }

  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    for (auto i : AllAllocKinds()) {
      MOZ_ASSERT(!zone->arenas.arenaListsToSweep(i));
      MOZ_ASSERT(zone->arenas.doneBackgroundFinalize(i));
    }
  }
#endif
}

void GCRuntime::queueZonesAndStartBackgroundSweep(ZoneList& zones) {
  {
    AutoLockHelperThreadState lock;
    backgroundSweepZones.ref().transferFrom(zones);
    if (sweepOnBackgroundThread) {
      sweepTask.startOrRunIfIdle(lock);
    }
  }
  if (!sweepOnBackgroundThread) {
    sweepTask.join();
    sweepTask.runFromMainThread();
  }
}

void BackgroundSweepTask::run() {
  AutoTraceLog logSweeping(TraceLoggerForCurrentThread(),
                           TraceLogger_GCSweeping);

  AutoLockHelperThreadState lock;

  gc->sweepFromBackgroundThread(lock);

  // Signal to the main thread that we're about to finish, because we release
  // the lock again before GCParallelTask's state is changed to finished.
  setFinishing(lock);
}

void GCRuntime::sweepFromBackgroundThread(AutoLockHelperThreadState& lock) {
  do {
    ZoneList zones;
    zones.transferFrom(backgroundSweepZones.ref());
    LifoAlloc freeLifoAlloc(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    freeLifoAlloc.transferFrom(&lifoBlocksToFree.ref());

    AutoUnlockHelperThreadState unlock(lock);
    sweepBackgroundThings(zones, freeLifoAlloc);

    // The main thread may call queueZonesAndStartBackgroundSweep() while this
    // is running so we must check there is no more work after releasing the
    // lock.
  } while (!backgroundSweepZones.ref().isEmpty());
}

void GCRuntime::waitBackgroundSweepEnd() {
  sweepTask.join();

  // TODO: Improve assertion to work in incremental GC?
  if (!isIncrementalGCInProgress()) {
    assertBackgroundSweepingFinished();
  }
}

void GCRuntime::queueUnusedLifoBlocksForFree(LifoAlloc* lifo) {
  MOZ_ASSERT(JS::RuntimeHeapIsBusy());
  AutoLockHelperThreadState lock;
  lifoBlocksToFree.ref().transferUnusedFrom(lifo);
}

void GCRuntime::queueAllLifoBlocksForFree(LifoAlloc* lifo) {
  MOZ_ASSERT(JS::RuntimeHeapIsBusy());
  AutoLockHelperThreadState lock;
  lifoBlocksToFree.ref().transferFrom(lifo);
}

void GCRuntime::queueAllLifoBlocksForFreeAfterMinorGC(LifoAlloc* lifo) {
  lifoBlocksToFreeAfterMinorGC.ref().transferFrom(lifo);
}

void GCRuntime::queueBuffersForFreeAfterMinorGC(Nursery::BufferSet& buffers) {
  AutoLockHelperThreadState lock;

  if (!buffersToFreeAfterMinorGC.ref().empty()) {
    // In the rare case that this hasn't processed the buffers from a previous
    // minor GC we have to wait here.
    MOZ_ASSERT(!freeTask.isIdle(lock));
    freeTask.joinWithLockHeld(lock);
  }

  MOZ_ASSERT(buffersToFreeAfterMinorGC.ref().empty());
  std::swap(buffersToFreeAfterMinorGC.ref(), buffers);
}

void GCRuntime::startBackgroundFree() {
  AutoLockHelperThreadState lock;
  freeTask.startOrRunIfIdle(lock);
}

void BackgroundFreeTask::run() {
  AutoTraceLog logFreeing(TraceLoggerForCurrentThread(), TraceLogger_GCFree);

  AutoLockHelperThreadState lock;

  gc->freeFromBackgroundThread(lock);

  // Signal to the main thread that we're about to finish, because we release
  // the lock again before GCParallelTask's state is changed to finished.
  setFinishing(lock);
}

void GCRuntime::freeFromBackgroundThread(AutoLockHelperThreadState& lock) {
  do {
    LifoAlloc lifoBlocks(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    lifoBlocks.transferFrom(&lifoBlocksToFree.ref());

    Nursery::BufferSet buffers;
    std::swap(buffers, buffersToFreeAfterMinorGC.ref());

    AutoUnlockHelperThreadState unlock(lock);

    lifoBlocks.freeAll();

    JSFreeOp* fop = TlsContext.get()->defaultFreeOp();
    for (Nursery::BufferSet::Range r = buffers.all(); !r.empty();
         r.popFront()) {
      // Malloc memory associated with nursery objects is not tracked as these
      // are assumed to be short lived.
      fop->freeUntracked(r.front());
    }
  } while (!lifoBlocksToFree.ref().isEmpty() ||
           !buffersToFreeAfterMinorGC.ref().empty());
}

void GCRuntime::waitBackgroundFreeEnd() { freeTask.join(); }

/* static */
bool UniqueIdGCPolicy::needsSweep(Cell** cellp, uint64_t*) {
  Cell* cell = *cellp;
  return MapGCThingTyped(cell, cell->getTraceKind(), [](auto t) {
    mozilla::DebugOnly<const Cell*> prior = t;
    bool result = IsAboutToBeFinalizedUnbarriered(&t);
    // Sweep should not have to deal with moved pointers, since moving GC
    // handles updating the UID table manually.
    MOZ_ASSERT(t == prior);
    return result;
  });
}

void JS::Zone::sweepUniqueIds() { uniqueIds().sweep(); }

void Realm::destroy(JSFreeOp* fop) {
  JSRuntime* rt = fop->runtime();
  if (auto callback = rt->destroyRealmCallback) {
    callback(fop, this);
  }
  if (principals()) {
    JS_DropPrincipals(rt->mainContextFromOwnThread(), principals());
  }
  // Bug 1560019: Malloc memory associated with a zone but not with a specific
  // GC thing is not currently tracked.
  fop->deleteUntracked(this);
}

void Compartment::destroy(JSFreeOp* fop) {
  JSRuntime* rt = fop->runtime();
  if (auto callback = rt->destroyCompartmentCallback) {
    callback(fop, this);
  }
  // Bug 1560019: Malloc memory associated with a zone but not with a specific
  // GC thing is not currently tracked.
  fop->deleteUntracked(this);
  rt->gc.stats().sweptCompartment();
}

void Zone::destroy(JSFreeOp* fop) {
  MOZ_ASSERT(compartments().empty());
  // Bug 1560019: Malloc memory associated with a zone but not with a specific
  // GC thing is not currently tracked.
  fop->deleteUntracked(this);
  fop->runtime()->gc.stats().sweptZone();
}

/*
 * It's simpler if we preserve the invariant that every zone (except the atoms
 * zone) has at least one compartment, and every compartment has at least one
 * realm. If we know we're deleting the entire zone, then sweepCompartments is
 * allowed to delete all compartments. In this case, |keepAtleastOne| is false.
 * If any cells remain alive in the zone, set |keepAtleastOne| true to prohibit
 * sweepCompartments from deleting every compartment. Instead, it preserves an
 * arbitrary compartment in the zone.
 */
void Zone::sweepCompartments(JSFreeOp* fop, bool keepAtleastOne,
                             bool destroyingRuntime) {
  MOZ_ASSERT(!compartments().empty());
  MOZ_ASSERT_IF(destroyingRuntime, !keepAtleastOne);

  Compartment** read = compartments().begin();
  Compartment** end = compartments().end();
  Compartment** write = read;
  while (read < end) {
    Compartment* comp = *read++;

    /*
     * Don't delete the last compartment and realm if keepAtleastOne is
     * still true, meaning all the other compartments were deleted.
     */
    bool keepAtleastOneRealm = read == end && keepAtleastOne;
    comp->sweepRealms(fop, keepAtleastOneRealm, destroyingRuntime);

    if (!comp->realms().empty()) {
      *write++ = comp;
      keepAtleastOne = false;
    } else {
      comp->destroy(fop);
    }
  }
  compartments().shrinkTo(write - compartments().begin());
  MOZ_ASSERT_IF(keepAtleastOne, !compartments().empty());
  MOZ_ASSERT_IF(destroyingRuntime, compartments().empty());
}

void Compartment::sweepRealms(JSFreeOp* fop, bool keepAtleastOne,
                              bool destroyingRuntime) {
  MOZ_ASSERT(!realms().empty());
  MOZ_ASSERT_IF(destroyingRuntime, !keepAtleastOne);

  Realm** read = realms().begin();
  Realm** end = realms().end();
  Realm** write = read;
  while (read < end) {
    Realm* realm = *read++;

    /*
     * Don't delete the last realm if keepAtleastOne is still true, meaning
     * all the other realms were deleted.
     */
    bool dontDelete = read == end && keepAtleastOne;
    if ((realm->marked() || dontDelete) && !destroyingRuntime) {
      *write++ = realm;
      keepAtleastOne = false;
    } else {
      realm->destroy(fop);
    }
  }
  realms().shrinkTo(write - realms().begin());
  MOZ_ASSERT_IF(keepAtleastOne, !realms().empty());
  MOZ_ASSERT_IF(destroyingRuntime, realms().empty());
}

void GCRuntime::deleteEmptyZone(Zone* zone) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  MOZ_ASSERT(zone->compartments().empty());
  for (auto& i : zones()) {
    if (i == zone) {
      zones().erase(&i);
      zone->destroy(rt->defaultFreeOp());
      return;
    }
  }
  MOZ_CRASH("Zone not found");
}

void GCRuntime::sweepZones(JSFreeOp* fop, bool destroyingRuntime) {
  MOZ_ASSERT_IF(destroyingRuntime, numActiveZoneIters == 0);

  if (numActiveZoneIters) {
    return;
  }

  assertBackgroundSweepingFinished();

  Zone** read = zones().begin();
  Zone** end = zones().end();
  Zone** write = read;

  while (read < end) {
    Zone* zone = *read++;

    if (zone->wasGCStarted()) {
      MOZ_ASSERT(!zone->isQueuedForBackgroundSweep());
      const bool zoneIsDead =
          zone->arenas.arenaListsAreEmpty() && !zone->hasMarkedRealms();
      MOZ_ASSERT_IF(destroyingRuntime, zoneIsDead);
      if (zoneIsDead) {
        AutoSetThreadIsSweeping threadIsSweeping(zone);
        zone->arenas.checkEmptyFreeLists();
        zone->sweepCompartments(fop, false, destroyingRuntime);
        MOZ_ASSERT(zone->compartments().empty());
        MOZ_ASSERT(zone->typeDescrObjects().empty());
        zone->destroy(fop);
        continue;
      }
      zone->sweepCompartments(fop, true, destroyingRuntime);
    }
    *write++ = zone;
  }
  zones().shrinkTo(write - zones().begin());
}

void ArenaLists::checkEmptyArenaList(AllocKind kind) {
  MOZ_ASSERT(arenaLists(kind).isEmpty());
}

class MOZ_RAII AutoRunParallelTask : public GCParallelTask {
  // This class takes a pointer to a member function of GCRuntime.
  using TaskFunc = JS_MEMBER_FN_PTR_TYPE(GCRuntime, void);

  TaskFunc func_;
  gcstats::PhaseKind phase_;
  AutoLockHelperThreadState& lock_;

 public:
  AutoRunParallelTask(GCRuntime* gc, TaskFunc func, gcstats::PhaseKind phase,
                      AutoLockHelperThreadState& lock)
      : GCParallelTask(gc), func_(func), phase_(phase), lock_(lock) {
    gc->startTask(*this, phase_, lock_);
  }

  ~AutoRunParallelTask() { gc->joinTask(*this, phase_, lock_); }

  void run() override {
    // The hazard analysis can't tell what the call to func_ will do but it's
    // not allowed to GC.
    JS::AutoSuppressGCAnalysis nogc;

    // Call pointer to member function on |gc|.
    JS_CALL_MEMBER_FN_PTR(gc, func_);
  }
};

void GCRuntime::purgeRuntimeForMinorGC() {
  // If external strings become nursery allocable, remember to call
  // zone->externalStringCache().purge() (and delete this assert.)
  MOZ_ASSERT(!IsNurseryAllocable(AllocKind::EXTERNAL_STRING));

  for (ZonesIter zone(this, SkipAtoms); !zone.done(); zone.next()) {
    zone->functionToStringCache().purge();
  }

  rt->caches().purgeForMinorGC(rt);
}

void GCRuntime::purgeRuntime() {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::PURGE);

  for (GCRealmsIter realm(rt); !realm.done(); realm.next()) {
    realm->purge();
  }

  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    zone->purgeAtomCacheOrDefer();
    zone->externalStringCache().purge();
    zone->functionToStringCache().purge();
  }

  JSContext* cx = rt->mainContextFromOwnThread();
  queueUnusedLifoBlocksForFree(&cx->tempLifoAlloc());
  cx->interpreterStack().purge(rt);
  cx->frontendCollectionPool().purge();

  rt->caches().purge();

  if (auto cache = rt->maybeThisRuntimeSharedImmutableStrings()) {
    cache->purge();
  }

  MOZ_ASSERT(unmarkGrayStack.empty());
  unmarkGrayStack.clearAndFree();

  // If we're the main runtime, tell helper threads to free their unused
  // memory when they are next idle.
  if (!rt->parentRuntime) {
    HelperThreadState().triggerFreeUnusedMemory();
  }
}

bool GCRuntime::shouldPreserveJITCode(Realm* realm,
                                      const TimeStamp& currentTime,
                                      JS::GCReason reason,
                                      bool canAllocateMoreCode) {
  if (cleanUpEverything) {
    return false;
  }
  if (!canAllocateMoreCode) {
    return false;
  }

  if (alwaysPreserveCode) {
    return true;
  }
  if (realm->preserveJitCode()) {
    return true;
  }

  if (IsCurrentlyAnimating(realm->lastAnimationTime, currentTime)) {
    return true;
  }

  if (reason == JS::GCReason::DEBUG_GC) {
    return true;
  }

  return false;
}

#ifdef DEBUG
class CompartmentCheckTracer final : public JS::CallbackTracer {
  bool onChild(const JS::GCCellPtr& thing) override;

 public:
  explicit CompartmentCheckTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt),
        src(nullptr),
        zone(nullptr),
        compartment(nullptr) {
    setTraceWeakEdges(false);
  }

  Cell* src;
  JS::TraceKind srcKind;
  Zone* zone;
  Compartment* compartment;
};

static bool InCrossCompartmentMap(JSRuntime* rt, JSObject* src,
                                  JS::GCCellPtr dst) {
  // Cross compartment edges are either in the cross compartment map or in a
  // debugger weakmap.

  Compartment* srccomp = src->compartment();

  if (dst.is<JSObject>()) {
    if (ObjectWrapperMap::Ptr p = srccomp->lookupWrapper(&dst.as<JSObject>())) {
      if (*p->value().unsafeGet() == src) {
        return true;
      }
    }
  }

  if (DebugAPI::edgeIsInDebuggerWeakmap(rt, src, dst)) {
    return true;
  }

  return false;
}

bool CompartmentCheckTracer::onChild(const JS::GCCellPtr& thing) {
  Compartment* comp =
      MapGCThingTyped(thing, [](auto t) { return t->maybeCompartment(); });
  if (comp && compartment) {
    MOZ_ASSERT(
        comp == compartment ||
        (srcKind == JS::TraceKind::Object &&
         InCrossCompartmentMap(runtime(), static_cast<JSObject*>(src), thing)));
  } else {
    TenuredCell* tenured = &thing.asCell()->asTenured();
    Zone* thingZone = tenured->zoneFromAnyThread();
    MOZ_ASSERT(thingZone == zone || thingZone->isAtomsZone());
  }
  return true;
}

void GCRuntime::checkForCompartmentMismatches() {
  JSContext* cx = rt->mainContextFromOwnThread();
  if (cx->disableStrictProxyCheckingCount) {
    return;
  }

  CompartmentCheckTracer trc(rt);
  AutoAssertEmptyNursery empty(cx);
  for (ZonesIter zone(this, SkipAtoms); !zone.done(); zone.next()) {
    trc.zone = zone;
    for (auto thingKind : AllAllocKinds()) {
      for (auto i = zone->cellIterUnsafe<TenuredCell>(thingKind, empty);
           !i.done(); i.next()) {
        trc.src = i.getCell();
        trc.srcKind = MapAllocToTraceKind(thingKind);
        trc.compartment = MapGCThingTyped(
            trc.src, trc.srcKind, [](auto t) { return t->maybeCompartment(); });
        js::TraceChildren(&trc, trc.src, trc.srcKind);
      }
    }
  }
}
#endif

static void RelazifyFunctions(Zone* zone, AllocKind kind) {
  MOZ_ASSERT(kind == AllocKind::FUNCTION ||
             kind == AllocKind::FUNCTION_EXTENDED);

  JSRuntime* rt = zone->runtimeFromMainThread();
  AutoAssertEmptyNursery empty(rt->mainContextFromOwnThread());

  for (auto i = zone->cellIterUnsafe<JSObject>(kind, empty); !i.done();
       i.next()) {
    JSFunction* fun = &i->as<JSFunction>();
    // When iterating over the GC-heap, we may encounter function objects that
    // are incomplete (missing a BaseScript when we expect one). We must check
    // for this case before we can call JSFunction::hasBytecode().
    if (fun->isIncomplete()) {
      continue;
    }
    if (fun->hasBytecode()) {
      fun->maybeRelazify(rt);
    }
  }
}

static bool ShouldCollectZone(Zone* zone, JS::GCReason reason) {
  // If we are repeating a GC because we noticed dead compartments haven't
  // been collected, then only collect zones containing those compartments.
  if (reason == JS::GCReason::COMPARTMENT_REVIVED) {
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
      if (comp->gcState.scheduledForDestruction) {
        return true;
      }
    }

    return false;
  }

  // Otherwise we only collect scheduled zones.
  if (!zone->isGCScheduled()) {
    return false;
  }

  // If canCollectAtoms() is false then either an instance of AutoKeepAtoms is
  // currently on the stack or parsing is currently happening on another
  // thread. In either case we don't have information about which atoms are
  // roots, so we must skip collecting atoms.
  //
  // Note that only affects the first slice of an incremental GC since root
  // marking is completed before we return to the mutator.
  //
  // Off-thread parsing is inhibited after the start of GC which prevents
  // races between creating atoms during parsing and sweeping atoms on the
  // main thread.
  //
  // Otherwise, we always schedule a GC in the atoms zone so that atoms which
  // the other collected zones are using are marked, and we can update the
  // set of atoms in use by the other collected zones at the end of the GC.
  if (zone->isAtomsZone()) {
    return TlsContext.get()->canCollectAtoms();
  }

  return zone->canCollect();
}

bool GCRuntime::prepareZonesForCollection(JS::GCReason reason,
                                          bool* isFullOut) {
#ifdef DEBUG
  /* Assert that zone state is as we expect */
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    MOZ_ASSERT(!zone->isCollecting());
    MOZ_ASSERT_IF(!zone->isAtomsZone(), !zone->compartments().empty());
    for (auto i : AllAllocKinds()) {
      MOZ_ASSERT(!zone->arenas.arenaListsToSweep(i));
    }
  }
#endif

  *isFullOut = true;
  bool any = false;

  auto currentTime = ReallyNow();

  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    /* Set up which zones will be collected. */
    bool shouldCollect = ShouldCollectZone(zone, reason);
    if (shouldCollect) {
      MOZ_ASSERT(zone->canCollect());
      any = true;
      zone->changeGCState(Zone::NoGC, Zone::MarkBlackOnly);
    } else if (zone->canCollect()) {
      *isFullOut = false;
    }

    zone->setWasCollected(shouldCollect);
    zone->setPreservingCode(false);
  }

  // Discard JIT code more aggressively if the process is approaching its
  // executable code limit.
  bool canAllocateMoreCode = jit::CanLikelyAllocateMoreExecutableMemory();

  for (CompartmentsIter c(rt); !c.done(); c.next()) {
    c->gcState.scheduledForDestruction = false;
    c->gcState.maybeAlive = false;
    c->gcState.hasEnteredRealm = false;
    for (RealmsInCompartmentIter r(c); !r.done(); r.next()) {
      if (r->shouldTraceGlobal() || !r->zone()->isGCScheduled()) {
        c->gcState.maybeAlive = true;
      }
      if (shouldPreserveJITCode(r, currentTime, reason, canAllocateMoreCode)) {
        r->zone()->setPreservingCode(true);
      }
      if (r->hasBeenEnteredIgnoringJit()) {
        c->gcState.hasEnteredRealm = true;
      }
    }
  }

  if (!cleanUpEverything && canAllocateMoreCode) {
    jit::JitActivationIterator activation(rt->mainContextFromOwnThread());
    if (!activation.done()) {
      activation->compartment()->zone()->setPreservingCode(true);
    }
  }

  /*
   * Check that we do collect the atoms zone if we triggered a GC for that
   * purpose.
   */
  MOZ_ASSERT_IF(reason == JS::GCReason::DELAYED_ATOMS_GC,
                atomsZone->isGCMarking());

  /* Check that at least one zone is scheduled for collection. */
  return any;
}

void GCRuntime::discardJITCodeForGC() {
  js::CancelOffThreadIonCompile(rt, JS::Zone::MarkBlackOnly);
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_DISCARD_CODE);
    zone->discardJitCode(rt->defaultFreeOp(), Zone::DiscardBaselineCode,
                         Zone::DiscardJitScripts);
  }
}

void GCRuntime::relazifyFunctionsForShrinkingGC() {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::RELAZIFY_FUNCTIONS);
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    if (zone->isSelfHostingZone()) {
      continue;
    }
    RelazifyFunctions(zone, AllocKind::FUNCTION);
    RelazifyFunctions(zone, AllocKind::FUNCTION_EXTENDED);
  }
}

void GCRuntime::purgeShapeCachesForShrinkingGC() {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::PURGE_SHAPE_CACHES);
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    if (!canRelocateZone(zone) || zone->keepShapeCaches()) {
      continue;
    }
    for (auto baseShape = zone->cellIterUnsafe<BaseShape>(); !baseShape.done();
         baseShape.next()) {
      baseShape->maybePurgeCache(rt->defaultFreeOp());
    }
  }
}

// The debugger keeps track of the URLs for the sources of each realm's scripts.
// These URLs are purged on shrinking GCs.
void GCRuntime::purgeSourceURLsForShrinkingGC() {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::PURGE_SOURCE_URLS);
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    // URLs are not tracked for realms in the system zone.
    if (!canRelocateZone(zone) || zone->isSystemZone()) {
      continue;
    }
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
      for (RealmsInCompartmentIter realm(comp); !realm.done(); realm.next()) {
        GlobalObject* global = realm.get()->unsafeUnbarrieredMaybeGlobal();
        if (global) {
          global->clearSourceURLSHolder();
        }
      }
    }
  }
}

class ArenasToUnmark {
 public:
  explicit ArenasToUnmark(GCRuntime* gc);

  bool done() const { return arenas.isNothing(); }

  ArenaListSegment get() const {
    MOZ_ASSERT(!done());
    return arenas.ref().get();
  }

  void next();

 private:
  void settle();

  GCZonesIter zones;
  Maybe<ArenasToUpdate> arenas;
};

ArenasToUnmark::ArenasToUnmark(GCRuntime* gc) : zones(gc) { settle(); }

void ArenasToUnmark::settle() {
  MOZ_ASSERT(arenas.isNothing());

  while (!zones.done()) {
    arenas.emplace(zones.get());
    if (!arenas.ref().done()) {
      break;
    }

    arenas.reset();
    zones.next();
  }

  MOZ_ASSERT(done() || !arenas.ref().done());
}

void ArenasToUnmark::next() {
  MOZ_ASSERT(!done());

  arenas.ref().next();

  if (arenas.ref().done()) {
    arenas.reset();
    zones.next();
    settle();
  }
}

static size_t UnmarkArenaListSegment(GCRuntime* gc,
                                     const ArenaListSegment& arenas) {
  MOZ_ASSERT(arenas.begin);
  MovingTracer trc(gc->rt);
  size_t count = 0;
  for (Arena* arena = arenas.begin; arena != arenas.end; arena = arena->next) {
    arena->unmarkAll();
    count++;
  }
  return count * 256;
}

void GCRuntime::unmarkWeakMaps() {
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    /* Unmark all weak maps in the zones being collected. */
    WeakMapBase::unmarkZone(zone);
  }
}

static bool IsShutdownGC(JS::GCReason reason) {
  return reason == JS::GCReason::WORKER_SHUTDOWN ||
         reason == JS::GCReason::SHUTDOWN_CC ||
         reason == JS::GCReason::DESTROY_RUNTIME;
}

bool GCRuntime::beginMarkPhase(JS::GCReason reason, AutoGCSession& session) {
#ifdef DEBUG
  if (fullCompartmentChecks) {
    checkForCompartmentMismatches();
  }
#endif

  if (!prepareZonesForCollection(reason, &isFull.ref())) {
    return false;
  }

  /* * Check it's safe to access the atoms zone if we are collecting it. */
  if (atomsZone->isCollecting()) {
    session.maybeCheckAtomsAccess.emplace(rt);
  }

  /*
   * In an incremental GC, clear the area free lists to ensure that subsequent
   * allocations refill them and end up marking new cells back. See
   * arenaAllocatedDuringGC().
   */
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    zone->arenas.clearFreeLists();
  }

  marker.start();
  GCMarker* gcmarker = &marker;
  gcmarker->clearMarkCount();

  {
    gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::PREPARE);

    AutoLockHelperThreadState helperLock;

    /*
     * Clear all mark state for the zones we are collecting. This is linear in
     * the size of the heap we are collecting and so can be slow. Do this in
     * parallel across multiple helper threads.
     */
    ArenasToUnmark unmarkingWork(this);
    AutoRunParallelWork unmarkCollectedZones(
        this, UnmarkArenaListSegment, gcstats::PhaseKind::UNMARK, unmarkingWork,
        SliceBudget::unlimited(), helperLock);

    /* Clear mark state for WeakMaps in parallel with other work. */
    AutoRunParallelTask unmarkWeakMaps(this, &GCRuntime::unmarkWeakMaps,
                                       gcstats::PhaseKind::UNMARK_WEAKMAPS,
                                       helperLock);

    /*
     * Buffer gray roots for incremental collections. This is linear in the
     * number of roots which can be in the tens of thousands. Do this in
     * parallel with the rest of this block.
     */
    Maybe<AutoRunParallelTask> bufferGrayRootsTask;
    if (isIncremental) {
      bufferGrayRootsTask.emplace(this, &GCRuntime::bufferGrayRoots,
                                  gcstats::PhaseKind::BUFFER_GRAY_ROOTS,
                                  helperLock);
    }
    AutoUnlockHelperThreadState unlock(helperLock);

    // Discard JIT code. For incremental collections, the sweep phase will
    // also discard JIT code.
    discardJITCodeForGC();
    startBackgroundFreeAfterMinorGC();

    /*
     * Relazify functions after discarding JIT code (we can't relazify
     * functions with JIT code) and before the actual mark phase, so that
     * the current GC can collect the JSScripts we're unlinking here.  We do
     * this only when we're performing a shrinking GC, as too much
     * relazification can cause performance issues when we have to reparse
     * the same functions over and over.
     */
    if (invocationKind == GC_SHRINK) {
      relazifyFunctionsForShrinkingGC();
      purgeShapeCachesForShrinkingGC();
      purgeSourceURLsForShrinkingGC();
    }

    /*
     * We must purge the runtime at the beginning of an incremental GC. The
     * danger if we purge later is that the snapshot invariant of
     * incremental GC will be broken, as follows. If some object is
     * reachable only through some cache (say the dtoaCache) then it will
     * not be part of the snapshot.  If we purge after root marking, then
     * the mutator could obtain a pointer to the object and start using
     * it. This object might never be marked, so a GC hazard would exist.
     */
    purgeRuntime();

    if (IsShutdownGC(reason)) {
      /* Clear any engine roots that may hold external data live. */
      for (GCZonesIter zone(this, SkipAtoms); !zone.done(); zone.next()) {
        zone->clearRootsForShutdownGC();
      }
    }
  }

  /*
   * Mark phase.
   */
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK);
  if (rt->isBeingDestroyed()) {
    checkNoRuntimeRoots(session);
  } else {
    traceRuntimeForMajorGC(gcmarker, session);
  }

  if (isIncremental) {
    findDeadCompartments();
  }

  updateMemoryCountersOnGCStart();
  stats().measureInitialHeapSize();

  /*
   * Process any queued source compressions during the start of a major
   * GC.
   */
  {
    AutoLockHelperThreadState helperLock;
    HelperThreadState().startHandlingCompressionTasks(helperLock);
  }

  return true;
}

void GCRuntime::findDeadCompartments() {
  gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::MARK_ROOTS);
  gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::MARK_COMPARTMENTS);

  /*
   * This code ensures that if a compartment is "dead", then it will be
   * collected in this GC. A compartment is considered dead if its maybeAlive
   * flag is false. The maybeAlive flag is set if:
   *
   *   (1) the compartment has been entered (set in beginMarkPhase() above)
   *   (2) the compartment's zone is not being collected (set in
   *       beginMarkPhase() above)
   *   (3) an object in the compartment was marked during root marking, either
   *       as a black root or a gray root (set in RootMarking.cpp), or
   *   (4) the compartment has incoming cross-compartment edges from another
   *       compartment that has maybeAlive set (set by this method).
   *
   * If the maybeAlive is false, then we set the scheduledForDestruction flag.
   * At the end of the GC, we look for compartments where
   * scheduledForDestruction is true. These are compartments that were somehow
   * "revived" during the incremental GC. If any are found, we do a special,
   * non-incremental GC of those compartments to try to collect them.
   *
   * Compartments can be revived for a variety of reasons. On reason is bug
   * 811587, where a reflector that was dead can be revived by DOM code that
   * still refers to the underlying DOM node.
   *
   * Read barriers and allocations can also cause revival. This might happen
   * during a function like JS_TransplantObject, which iterates over all
   * compartments, live or dead, and operates on their objects. See bug 803376
   * for details on this problem. To avoid the problem, we try to avoid
   * allocation and read barriers during JS_TransplantObject and the like.
   */

  // Propagate the maybeAlive flag via cross-compartment edges.

  Vector<Compartment*, 0, js::SystemAllocPolicy> workList;

  for (CompartmentsIter comp(rt); !comp.done(); comp.next()) {
    if (comp->gcState.maybeAlive) {
      if (!workList.append(comp)) {
        return;
      }
    }
  }

  while (!workList.empty()) {
    Compartment* comp = workList.popCopy();
    for (Compartment::WrappedObjectCompartmentEnum e(comp); !e.empty();
         e.popFront()) {
      Compartment* dest = e.front();
      if (!dest->gcState.maybeAlive) {
        dest->gcState.maybeAlive = true;
        if (!workList.append(dest)) {
          return;
        }
      }
    }
  }

  // Set scheduledForDestruction based on maybeAlive.

  for (GCCompartmentsIter comp(rt); !comp.done(); comp.next()) {
    MOZ_ASSERT(!comp->gcState.scheduledForDestruction);
    if (!comp->gcState.maybeAlive) {
      comp->gcState.scheduledForDestruction = true;
    }
  }
}

void GCRuntime::updateMemoryCountersOnGCStart() {
  heapSize.updateOnGCStart();

  // Update memory counters for the zones we are collecting.
  for (GCZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    zone->updateMemoryCountersOnGCStart();
  }
}

template <class ZoneIterT>
void GCRuntime::markWeakReferences(gcstats::PhaseKind phase) {
  MOZ_ASSERT(marker.isDrained());

  gcstats::AutoPhase ap1(stats(), phase);

  if (marker.enterWeakMarkingMode()) {
    for (ZoneIterT zone(this); !zone.done(); zone.next()) {
      zone->enterWeakMarkingMode(&marker);
    }
#ifdef DEBUG
    for (ZoneIterT zone(this); !zone.done(); zone.next()) {
      zone->checkWeakMarkingMode();
    }
#endif
  }

  // TODO bug 1167452: Make weak marking incremental
  drainMarkStack();

  for (;;) {
    bool markedAny = false;
    if (!marker.isWeakMarking()) {
      for (ZoneIterT zone(this); !zone.done(); zone.next()) {
        markedAny |= WeakMapBase::markZoneIteratively(zone, &marker);
      }
    }
    markedAny |= jit::JitRuntime::MarkJitcodeGlobalTableIteratively(&marker);

    if (!markedAny) {
      break;
    }

    drainMarkStack();
  }
  MOZ_ASSERT(marker.isDrained());

  marker.leaveWeakMarkingMode();
}

void GCRuntime::markWeakReferencesInCurrentGroup(gcstats::PhaseKind phase) {
  markWeakReferences<SweepGroupZonesIter>(phase);
}

template <class ZoneIterT>
void GCRuntime::markGrayRoots(gcstats::PhaseKind phase) {
  MOZ_ASSERT(marker.markColor() == MarkColor::Gray);

  gcstats::AutoPhase ap(stats(), phase);
  if (hasValidGrayRootsBuffer()) {
    for (ZoneIterT zone(this); !zone.done(); zone.next()) {
      markBufferedGrayRoots(zone);
    }
  } else {
    MOZ_ASSERT(!isIncremental);
    traceEmbeddingGrayRoots(&marker);
    Compartment::traceIncomingCrossCompartmentEdgesForZoneGC(
        &marker, Compartment::GrayEdges);
  }
}

void GCRuntime::markAllWeakReferences(gcstats::PhaseKind phase) {
  markWeakReferences<GCZonesIter>(phase);
}

void GCRuntime::markAllGrayReferences(gcstats::PhaseKind phase) {
  markGrayRoots<GCZonesIter>(phase);
  drainMarkStack();
}

void GCRuntime::dropStringWrappers() {
  /*
   * String "wrappers" are dropped on GC because their presence would require
   * us to sweep the wrappers in all compartments every time we sweep a
   * compartment group.
   */
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    zone->dropStringWrappersOnGC();
  }
}

/*
 * Group zones that must be swept at the same time.
 *
 * From the point of view of the mutator, groups of zones transition atomically
 * from marking to sweeping. If compartment A has an edge to an unmarked object
 * in compartment B, then we must not start sweeping A in a later slice than we
 * start sweeping B. That's because a write barrier in A could lead to the
 * unmarked object in B becoming marked. However, if we had already swept that
 * object, we would be in trouble.
 *
 * If we consider these dependencies as a graph, then all the compartments in
 * any strongly-connected component of this graph must start sweeping in the
 * same slice.
 *
 * Tarjan's algorithm is used to calculate the components.
 */

bool Compartment::findSweepGroupEdges() {
  Zone* source = zone();
  for (WrappedObjectCompartmentEnum e(this); !e.empty(); e.popFront()) {
    Compartment* targetComp = e.front();
    Zone* target = targetComp->zone();

    if (!target->isGCMarking() || source->hasSweepGroupEdgeTo(target)) {
      continue;
    }

    for (ObjectWrapperEnum e(this, targetComp); !e.empty(); e.popFront()) {
      JSObject* key = e.front().mutableKey();
      MOZ_ASSERT(key->zone() == target);

      // Add an edge to the wrapped object's zone to ensure that the wrapper
      // zone is not still being marked when we start sweeping the wrapped zone.
      // As an optimization, if the wrapped object is already marked black there
      // is no danger of later marking and we can skip this.
      if (key->asTenured().isMarkedBlack()) {
        continue;
      }

      if (!source->addSweepGroupEdgeTo(target)) {
        return false;
      }

      // We don't need to consider any more wrappers for this target
      // compartment since we already added an edge.
      break;
    }
  }

  return true;
}

bool Zone::findSweepGroupEdges(Zone* atomsZone) {
  // Any zone may have a pointer to an atom in the atoms zone, and these aren't
  // in the cross compartment map.
  if (atomsZone->wasGCStarted() && !addSweepGroupEdgeTo(atomsZone)) {
    return false;
  }

  for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next()) {
    if (!comp->findSweepGroupEdges()) {
      return false;
    }
  }

  return WeakMapBase::findSweepGroupEdgesForZone(this);
}

static bool AddEdgesForMarkQueue(GCMarker& marker) {
#ifdef DEBUG
  // For testing only.
  //
  // Add edges between all objects mentioned in the test mark queue, since
  // otherwise they will get marked in a different order than their sweep
  // groups. Note that this is only done at the beginning of an incremental
  // collection, so it is possible for objects to be added later that do not
  // follow the sweep group ordering. These objects will wait until their sweep
  // group comes up, or will be skipped if their sweep group is already past.
  JS::Zone* prevZone = nullptr;
  for (size_t i = 0; i < marker.markQueue.length(); i++) {
    Value val = marker.markQueue[i].get().unbarrieredGet();
    if (!val.isObject()) {
      continue;
    }
    JSObject* obj = &val.toObject();
    JS::Zone* zone = obj->zone();
    if (!zone->isGCMarking()) {
      continue;
    }
    if (prevZone && prevZone != zone) {
      if (!prevZone->addSweepGroupEdgeTo(zone)) {
        return false;
      }
    }
    prevZone = zone;
  }
#endif
  return true;
}

bool GCRuntime::findSweepGroupEdges() {
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    if (!zone->findSweepGroupEdges(atomsZone)) {
      return false;
    }
  }

  if (!AddEdgesForMarkQueue(marker)) {
    return false;
  }

  return DebugAPI::findSweepGroupEdges(rt);
}

void GCRuntime::groupZonesForSweeping(JS::GCReason reason) {
#ifdef DEBUG
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    MOZ_ASSERT(zone->gcSweepGroupEdges().empty());
  }
#endif

  JSContext* cx = rt->mainContextFromOwnThread();
  ZoneComponentFinder finder(cx->nativeStackLimit[JS::StackForSystemCode]);
  if (!isIncremental || !findSweepGroupEdges()) {
    finder.useOneComponent();
  }

#ifdef JS_GC_ZEAL
  // Use one component for two-slice zeal modes.
  if (useZeal && hasIncrementalTwoSliceZealMode()) {
    finder.useOneComponent();
  }
#endif

  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    MOZ_ASSERT(zone->isGCMarking());
    finder.addNode(zone);
  }
  sweepGroups = finder.getResultsList();
  currentSweepGroup = sweepGroups;
  sweepGroupIndex = 1;

  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    zone->clearSweepGroupEdges();
  }

#ifdef DEBUG
  unsigned idx = sweepGroupIndex;
  for (Zone* head = currentSweepGroup; head; head = head->nextGroup()) {
    for (Zone* zone = head; zone; zone = zone->nextNodeInGroup()) {
      MOZ_ASSERT(zone->isGCMarking());
      zone->gcSweepGroupIndex = idx;
    }
    idx++;
  }

  MOZ_ASSERT_IF(!isIncremental, !currentSweepGroup->nextGroup());
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    MOZ_ASSERT(zone->gcSweepGroupEdges().empty());
  }
#endif
}

static void ResetGrayList(Compartment* comp);

void GCRuntime::getNextSweepGroup() {
  currentSweepGroup = currentSweepGroup->nextGroup();
  ++sweepGroupIndex;
  if (!currentSweepGroup) {
    abortSweepAfterCurrentGroup = false;
    return;
  }

  MOZ_ASSERT_IF(abortSweepAfterCurrentGroup, !isIncremental);
  if (!isIncremental) {
    ZoneComponentFinder::mergeGroups(currentSweepGroup);
  }

  for (Zone* zone = currentSweepGroup; zone; zone = zone->nextNodeInGroup()) {
    MOZ_ASSERT(zone->isGCMarkingBlackOnly());
    MOZ_ASSERT(!zone->isQueuedForBackgroundSweep());
  }

  if (abortSweepAfterCurrentGroup) {
    joinTask(sweepMarkTask, gcstats::PhaseKind::SWEEP_MARK);

    for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
      MOZ_ASSERT(!zone->gcNextGraphComponent);
      zone->setNeedsIncrementalBarrier(false);
      zone->changeGCState(Zone::MarkBlackOnly, Zone::NoGC);
      zone->arenas.unmarkPreMarkedFreeCells();
      zone->gcGrayRoots().Clear();
    }

    for (SweepGroupCompartmentsIter comp(rt); !comp.done(); comp.next()) {
      ResetGrayList(comp);
    }

    abortSweepAfterCurrentGroup = false;
    currentSweepGroup = nullptr;
  }

  hasMarkedGrayRoots = false;
}

/*
 * Gray marking:
 *
 * At the end of collection, anything reachable from a gray root that has not
 * otherwise been marked black must be marked gray.
 *
 * This means that when marking things gray we must not allow marking to leave
 * the current compartment group, as that could result in things being marked
 * gray when they might subsequently be marked black.  To achieve this, when we
 * find a cross compartment pointer we don't mark the referent but add it to a
 * singly-linked list of incoming gray pointers that is stored with each
 * compartment.
 *
 * The list head is stored in Compartment::gcIncomingGrayPointers and contains
 * cross compartment wrapper objects. The next pointer is stored in the second
 * extra slot of the cross compartment wrapper.
 *
 * The list is created during gray marking when one of the
 * MarkCrossCompartmentXXX functions is called for a pointer that leaves the
 * current compartent group.  This calls DelayCrossCompartmentGrayMarking to
 * push the referring object onto the list.
 *
 * The list is traversed and then unlinked in
 * GCRuntime::markIncomingCrossCompartmentPointers.
 */

static bool IsGrayListObject(JSObject* obj) {
  MOZ_ASSERT(obj);
  return obj->is<CrossCompartmentWrapperObject>() && !IsDeadProxyObject(obj);
}

/* static */
unsigned ProxyObject::grayLinkReservedSlot(JSObject* obj) {
  MOZ_ASSERT(IsGrayListObject(obj));
  return CrossCompartmentWrapperObject::GrayLinkReservedSlot;
}

#ifdef DEBUG
static void AssertNotOnGrayList(JSObject* obj) {
  MOZ_ASSERT_IF(
      IsGrayListObject(obj),
      GetProxyReservedSlot(obj, ProxyObject::grayLinkReservedSlot(obj))
          .isUndefined());
}
#endif

static void AssertNoWrappersInGrayList(JSRuntime* rt) {
#ifdef DEBUG
  for (CompartmentsIter c(rt); !c.done(); c.next()) {
    MOZ_ASSERT(!c->gcIncomingGrayPointers);
    for (Compartment::ObjectWrapperEnum e(c); !e.empty(); e.popFront()) {
      AssertNotOnGrayList(e.front().value().unbarrieredGet());
    }
  }
#endif
}

static JSObject* CrossCompartmentPointerReferent(JSObject* obj) {
  MOZ_ASSERT(IsGrayListObject(obj));
  return &obj->as<ProxyObject>().private_().toObject();
}

static JSObject* NextIncomingCrossCompartmentPointer(JSObject* prev,
                                                     bool unlink) {
  unsigned slot = ProxyObject::grayLinkReservedSlot(prev);
  JSObject* next = GetProxyReservedSlot(prev, slot).toObjectOrNull();
  MOZ_ASSERT_IF(next, IsGrayListObject(next));

  if (unlink) {
    SetProxyReservedSlot(prev, slot, UndefinedValue());
  }

  return next;
}

void js::gc::DelayCrossCompartmentGrayMarking(JSObject* src) {
  MOZ_ASSERT(IsGrayListObject(src));
  MOZ_ASSERT(src->isMarkedGray());

  AutoTouchingGrayThings tgt;

  /* Called from MarkCrossCompartmentXXX functions. */
  unsigned slot = ProxyObject::grayLinkReservedSlot(src);
  JSObject* dest = CrossCompartmentPointerReferent(src);
  Compartment* comp = dest->compartment();

  if (GetProxyReservedSlot(src, slot).isUndefined()) {
    SetProxyReservedSlot(src, slot,
                         ObjectOrNullValue(comp->gcIncomingGrayPointers));
    comp->gcIncomingGrayPointers = src;
  } else {
    MOZ_ASSERT(GetProxyReservedSlot(src, slot).isObjectOrNull());
  }

#ifdef DEBUG
  /*
   * Assert that the object is in our list, also walking the list to check its
   * integrity.
   */
  JSObject* obj = comp->gcIncomingGrayPointers;
  bool found = false;
  while (obj) {
    if (obj == src) {
      found = true;
    }
    obj = NextIncomingCrossCompartmentPointer(obj, false);
  }
  MOZ_ASSERT(found);
#endif
}

void GCRuntime::markIncomingCrossCompartmentPointers(MarkColor color) {
  gcstats::AutoPhase ap(stats(),
                        color == MarkColor::Black
                            ? gcstats::PhaseKind::SWEEP_MARK_INCOMING_BLACK
                            : gcstats::PhaseKind::SWEEP_MARK_INCOMING_GRAY);

  bool unlinkList = color == MarkColor::Gray;

  for (SweepGroupCompartmentsIter c(rt); !c.done(); c.next()) {
    MOZ_ASSERT(c->zone()->isGCMarking());
    MOZ_ASSERT_IF(color == MarkColor::Gray,
                  c->zone()->isGCMarkingBlackAndGray());
    MOZ_ASSERT_IF(c->gcIncomingGrayPointers,
                  IsGrayListObject(c->gcIncomingGrayPointers));

    for (JSObject* src = c->gcIncomingGrayPointers; src;
         src = NextIncomingCrossCompartmentPointer(src, unlinkList)) {
      JSObject* dst = CrossCompartmentPointerReferent(src);
      MOZ_ASSERT(dst->compartment() == c);

      if (color == MarkColor::Gray) {
        if (src->asTenured().isMarkedGray()) {
          TraceManuallyBarrieredEdge(&marker, &dst,
                                     "cross-compartment gray pointer");
        }
      } else {
        if (src->asTenured().isMarkedBlack()) {
          TraceManuallyBarrieredEdge(&marker, &dst,
                                     "cross-compartment black pointer");
        }
      }
    }

    if (unlinkList) {
      c->gcIncomingGrayPointers = nullptr;
    }
  }
}

static bool RemoveFromGrayList(JSObject* wrapper) {
  AutoTouchingGrayThings tgt;

  if (!IsGrayListObject(wrapper)) {
    return false;
  }

  unsigned slot = ProxyObject::grayLinkReservedSlot(wrapper);
  if (GetProxyReservedSlot(wrapper, slot).isUndefined()) {
    return false; /* Not on our list. */
  }

  JSObject* tail = GetProxyReservedSlot(wrapper, slot).toObjectOrNull();
  SetProxyReservedSlot(wrapper, slot, UndefinedValue());

  Compartment* comp = CrossCompartmentPointerReferent(wrapper)->compartment();
  JSObject* obj = comp->gcIncomingGrayPointers;
  if (obj == wrapper) {
    comp->gcIncomingGrayPointers = tail;
    return true;
  }

  while (obj) {
    unsigned slot = ProxyObject::grayLinkReservedSlot(obj);
    JSObject* next = GetProxyReservedSlot(obj, slot).toObjectOrNull();
    if (next == wrapper) {
      js::detail::SetProxyReservedSlotUnchecked(obj, slot,
                                                ObjectOrNullValue(tail));
      return true;
    }
    obj = next;
  }

  MOZ_CRASH("object not found in gray link list");
}

static void ResetGrayList(Compartment* comp) {
  JSObject* src = comp->gcIncomingGrayPointers;
  while (src) {
    src = NextIncomingCrossCompartmentPointer(src, true);
  }
  comp->gcIncomingGrayPointers = nullptr;
}

#ifdef DEBUG
static bool HasIncomingCrossCompartmentPointers(JSRuntime* rt) {
  for (SweepGroupCompartmentsIter c(rt); !c.done(); c.next()) {
    if (c->gcIncomingGrayPointers) {
      return true;
    }
  }

  return false;
}
#endif

void js::NotifyGCNukeWrapper(JSObject* obj) {
  /*
   * References to target of wrapper are being removed, we no longer have to
   * remember to mark it.
   */
  RemoveFromGrayList(obj);
}

enum {
  JS_GC_SWAP_OBJECT_A_REMOVED = 1 << 0,
  JS_GC_SWAP_OBJECT_B_REMOVED = 1 << 1
};

unsigned js::NotifyGCPreSwap(JSObject* a, JSObject* b) {
  /*
   * Two objects in the same compartment are about to have had their contents
   * swapped.  If either of them are in our gray pointer list, then we remove
   * them from the lists, returning a bitset indicating what happened.
   */
  return (RemoveFromGrayList(a) ? JS_GC_SWAP_OBJECT_A_REMOVED : 0) |
         (RemoveFromGrayList(b) ? JS_GC_SWAP_OBJECT_B_REMOVED : 0);
}

void js::NotifyGCPostSwap(JSObject* a, JSObject* b, unsigned removedFlags) {
  /*
   * Two objects in the same compartment have had their contents swapped.  If
   * either of them were in our gray pointer list, we re-add them again.
   */
  if (removedFlags & JS_GC_SWAP_OBJECT_A_REMOVED) {
    DelayCrossCompartmentGrayMarking(b);
  }
  if (removedFlags & JS_GC_SWAP_OBJECT_B_REMOVED) {
    DelayCrossCompartmentGrayMarking(a);
  }
}

static inline void MaybeCheckWeakMapMarking(GCRuntime* gc) {
#if defined(JS_GC_ZEAL) || defined(DEBUG)

  bool shouldCheck;
#  if defined(DEBUG)
  shouldCheck = true;
#  else
  shouldCheck = gc->hasZealMode(ZealMode::CheckWeakMapMarking);
#  endif

  if (shouldCheck) {
    for (SweepGroupZonesIter zone(gc); !zone.done(); zone.next()) {
      MOZ_RELEASE_ASSERT(WeakMapBase::checkMarkingForZone(zone));
    }
  }

#endif
}

IncrementalProgress GCRuntime::markGrayReferencesInCurrentGroup(
    JSFreeOp* fop, SliceBudget& budget) {
  // Wait for sweepMarkTask finishes sweep-marking.
  if (joinSweepMarkTask() == NotFinished) {
    return NotFinished;
  }

  MOZ_ASSERT(marker.markColor() == MarkColor::Black);

  if (hasMarkedGrayRoots) {
    return Finished;
  }

  MOZ_ASSERT(cellsToAssertNotGray.ref().empty());

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

  // Mark any incoming gray pointers from previously swept compartments that
  // have subsequently been marked black. This can occur when gray cells
  // become black by the action of UnmarkGray.
  markIncomingCrossCompartmentPointers(MarkColor::Black);
  drainMarkStack();

  // Change state of current group to MarkGray to restrict marking to this
  // group.  Note that there may be pointers to the atoms zone, and
  // these will be marked through, as they are not marked with
  // TraceCrossCompartmentEdge.
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    zone->changeGCState(Zone::MarkBlackOnly, Zone::MarkBlackAndGray);
  }

  AutoSetMarkColor setColorGray(marker, MarkColor::Gray);
  marker.setMainStackColor(MarkColor::Gray);

  // Mark incoming gray pointers from previously swept compartments.
  markIncomingCrossCompartmentPointers(MarkColor::Gray);

  markGrayRoots<SweepGroupZonesIter>(gcstats::PhaseKind::SWEEP_MARK_GRAY);

  hasMarkedGrayRoots = true;

#ifdef JS_GC_ZEAL
  if (shouldYieldForZeal(ZealMode::YieldWhileGrayMarking)) {
    return NotFinished;
  }
#endif

  if (markUntilBudgetExhausted(budget, gcstats::PhaseKind::SWEEP_MARK_GRAY) ==
      NotFinished) {
    return NotFinished;
  }
  marker.setMainStackColor(MarkColor::Black);
  return Finished;
}

IncrementalProgress GCRuntime::endMarkingSweepGroup(JSFreeOp* fop,
                                                    SliceBudget& budget) {
  // Wait for sweepMarkTask finishes sweep-marking.
  if (joinSweepMarkTask() == NotFinished) {
    return NotFinished;
  }

  MOZ_ASSERT(marker.markColor() == MarkColor::Black);
  MOZ_ASSERT(!HasIncomingCrossCompartmentPointers(rt));

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

  markWeakReferencesInCurrentGroup(gcstats::PhaseKind::SWEEP_MARK_WEAK);

  AutoSetMarkColor setColorGray(marker, MarkColor::Gray);
  marker.setMainStackColor(MarkColor::Gray);

  // Mark transitively inside the current compartment group.
  markWeakReferencesInCurrentGroup(gcstats::PhaseKind::SWEEP_MARK_GRAY_WEAK);

  MOZ_ASSERT(marker.isDrained());

  // We must not yield after this point before we start sweeping the group.
  safeToYield = false;

  MaybeCheckWeakMapMarking(this);

  return Finished;
}

// Causes the given WeakCache to be swept when run.
class ImmediateSweepWeakCacheTask : public GCParallelTask {
  Zone* zone;
  JS::detail::WeakCacheBase& cache;

  ImmediateSweepWeakCacheTask(const ImmediateSweepWeakCacheTask&) = delete;

 public:
  ImmediateSweepWeakCacheTask(GCRuntime* gc, Zone* zone,
                              JS::detail::WeakCacheBase& wc)
      : GCParallelTask(gc), zone(zone), cache(wc) {}

  ImmediateSweepWeakCacheTask(ImmediateSweepWeakCacheTask&& other)
      : GCParallelTask(std::move(other)),
        zone(other.zone),
        cache(other.cache) {}

  void run() override {
    AutoSetThreadIsSweeping threadIsSweeping(zone);
    cache.sweep();
  }
};

void GCRuntime::updateAtomsBitmap() {
  DenseBitmap marked;
  if (atomMarking.computeBitmapFromChunkMarkBits(rt, marked)) {
    for (GCZonesIter zone(this); !zone.done(); zone.next()) {
      atomMarking.refineZoneBitmapForCollectedZone(zone, marked);
    }
  } else {
    // Ignore OOM in computeBitmapFromChunkMarkBits. The
    // refineZoneBitmapForCollectedZone call can only remove atoms from the
    // zone bitmap, so it is conservative to just not call it.
  }

  atomMarking.markAtomsUsedByUncollectedZones(rt);

  // For convenience sweep these tables non-incrementally as part of bitmap
  // sweeping; they are likely to be much smaller than the main atoms table.
  rt->symbolRegistry().sweep();
  SweepingTracer trc(rt);
  for (RealmsIter realm(this); !realm.done(); realm.next()) {
    realm->tracekWeakVarNames(&trc);
  }
}

void GCRuntime::sweepCCWrappers() {
  AutoSetThreadIsSweeping threadIsSweeping;  // This can touch all zones.
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    zone->sweepAllCrossCompartmentWrappers();
  }
}

void GCRuntime::sweepObjectGroups() {
  SweepingTracer trc(rt);
  for (SweepGroupRealmsIter r(this); !r.done(); r.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(r->zone());
    r->traceWeakObjectGroups(&trc);
  }
}

void GCRuntime::sweepMisc() {
  SweepingTracer trc(rt);
  for (SweepGroupRealmsIter r(this); !r.done(); r.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(r->zone());
    r->traceWeakObjects(&trc);
    r->traceWeakTemplateObjects(&trc);
    r->traceWeakSavedStacks(&trc);
    r->traceWeakSelfHostingScriptSource(&trc);
    r->traceWeakObjectRealm(&trc);
    r->traceWeakRegExps(&trc);
  }
}

void GCRuntime::sweepCompressionTasks() {
  JSRuntime* runtime = rt;

  // Attach finished compression tasks.
  AutoLockHelperThreadState lock;
  AttachFinishedCompressions(runtime, lock);

  // Sweep pending tasks that are holding onto should-be-dead ScriptSources.
  auto& pending = HelperThreadState().compressionPendingList(lock);
  for (size_t i = 0; i < pending.length(); i++) {
    if (pending[i]->shouldCancel()) {
      HelperThreadState().remove(pending, &i);
    }
  }
}

void GCRuntime::sweepWeakMaps() {
  AutoSetThreadIsSweeping threadIsSweeping;  // This may touch any zone.
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    /* No need to look up any more weakmap keys from this sweep group. */
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!zone->gcWeakKeys().clear()) {
      oomUnsafe.crash("clearing weak keys in beginSweepingSweepGroup()");
    }

    zone->sweepWeakMaps();
  }
}

void GCRuntime::sweepUniqueIds() {
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(zone);
    zone->sweepUniqueIds();
  }
}

void GCRuntime::sweepWeakRefs() {
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(zone);
    zone->weakRefMap().sweep();
  }
}

void GCRuntime::sweepFinalizationRegistriesOnMainThread() {
  // This calls back into the browser which expects to be called from the main
  // thread.
  gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);
  gcstats::AutoPhase ap2(stats(),
                         gcstats::PhaseKind::SWEEP_FINALIZATION_REGISTRIES);
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    sweepFinalizationRegistries(zone);
  }
}

void GCRuntime::startTask(GCParallelTask& task, gcstats::PhaseKind phase,
                          AutoLockHelperThreadState& lock) {
  if (!CanUseExtraThreads()) {
    AutoUnlockHelperThreadState unlock(lock);
    gcstats::AutoPhase ap(stats(), phase);
    task.runFromMainThread();
    return;
  }

  task.startWithLockHeld(lock);
}

void GCRuntime::joinTask(GCParallelTask& task, gcstats::PhaseKind phase,
                         AutoLockHelperThreadState& lock) {
  // This is similar to GCParallelTask::joinWithLockHeld but handles recording
  // execution and wait time.

  if (task.isIdle(lock)) {
    return;
  }

  // If the task was dispatched but has not yet started then cancel the task and
  // run it from the main thread. This stops us from blocking here when the
  // helper threads are busy with other tasks.
  if (task.isDispatched(lock)) {
    task.cancelDispatchedTask(lock);
    AutoUnlockHelperThreadState unlock(lock);
    gcstats::AutoPhase ap(stats(), phase);
    task.runFromMainThread();
    return;
  }

  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::JOIN_PARALLEL_TASKS);
    task.joinRunningOrFinishedTask(lock);
  }

  stats().recordParallelPhase(phase, task.duration());
}

void GCRuntime::joinTask(GCParallelTask& task, gcstats::PhaseKind phase) {
  AutoLockHelperThreadState lock;
  joinTask(task, phase, lock);
}

void GCRuntime::sweepDebuggerOnMainThread(JSFreeOp* fop) {
  // Detach unreachable debuggers and global objects from each other.
  // This can modify weakmaps and so must happen before weakmap sweeping.
  DebugAPI::sweepAll(fop);

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);

  // Sweep debug environment information. This performs lookups in the Zone's
  // unique IDs table and so must not happen in parallel with sweeping that
  // table.
  {
    gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::SWEEP_MISC);
    for (SweepGroupRealmsIter r(rt); !r.done(); r.next()) {
      r->sweepDebugEnvironments();
    }
  }
}

void GCRuntime::sweepJitDataOnMainThread(JSFreeOp* fop) {
  SweepingTracer trc(rt);
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_JIT_DATA);

    if (initialState != State::NotActive) {
      // Cancel any active or pending off thread compilations. We also did
      // this before marking (in DiscardJITCodeForGC) so this is a no-op
      // for non-incremental GCs.
      js::CancelOffThreadIonCompile(rt, JS::Zone::Sweep);
    }

    // Bug 1071218: the following method has not yet been refactored to
    // work on a single zone-group at once.

    // Sweep entries containing about-to-be-finalized JitCode and
    // update relocated TypeSet::Types inside the JitcodeGlobalTable.
    jit::JitRuntime::TraceWeakJitcodeGlobalTable(rt, &trc);
  }

  if (initialState != State::NotActive) {
    gcstats::AutoPhase apdc(stats(), gcstats::PhaseKind::SWEEP_DISCARD_CODE);
    for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
      zone->discardJitCode(fop);
    }
  }

  // JitZone/JitRealm must be swept *after* discarding JIT code, because
  // Zone::discardJitCode might access CacheIRStubInfos deleted here.
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_JIT_DATA);

    for (SweepGroupRealmsIter r(rt); !r.done(); r.next()) {
      r->traceWeakEdgesInJitRealm(&trc);
    }

    for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
      if (jit::JitZone* jitZone = zone->jitZone()) {
        jitZone->traceWeak(&trc);
      }
    }
  }

  {
    gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_TYPES);
    gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::SWEEP_TYPES_BEGIN);
    for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
      zone->beginSweepTypes();
    }
  }
}

using WeakCacheTaskVector =
    mozilla::Vector<ImmediateSweepWeakCacheTask, 0, SystemAllocPolicy>;

// Call a functor for all weak caches that need to be swept in the current
// sweep group.
template <typename Functor>
static inline bool IterateWeakCaches(JSRuntime* rt, Functor f) {
  for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
    for (JS::detail::WeakCacheBase* cache : zone->weakCaches()) {
      if (!f(cache, zone.get())) {
        return false;
      }
    }
  }

  for (JS::detail::WeakCacheBase* cache : rt->weakCaches()) {
    if (!f(cache, nullptr)) {
      return false;
    }
  }

  return true;
}

static bool PrepareWeakCacheTasks(JSRuntime* rt,
                                  WeakCacheTaskVector* immediateTasks) {
  // Start incremental sweeping for caches that support it or add to a vector
  // of sweep tasks to run on a helper thread.

  MOZ_ASSERT(immediateTasks->empty());

  bool ok =
      IterateWeakCaches(rt, [&](JS::detail::WeakCacheBase* cache, Zone* zone) {
        if (!cache->needsSweep()) {
          return true;
        }

        // Caches that support incremental sweeping will be swept later.
        if (zone && cache->setNeedsIncrementalBarrier(true)) {
          return true;
        }

        return immediateTasks->emplaceBack(&rt->gc, zone, *cache);
      });

  if (!ok) {
    immediateTasks->clearAndFree();
  }

  return ok;
}

static void SweepAllWeakCachesOnMainThread(JSRuntime* rt) {
  // If we ran out of memory, do all the work on the main thread.
  gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::SWEEP_WEAK_CACHES);
  IterateWeakCaches(rt, [&](JS::detail::WeakCacheBase* cache, Zone* zone) {
    if (cache->needsIncrementalBarrier()) {
      cache->setNeedsIncrementalBarrier(false);
    }
    cache->sweep();
    return true;
  });
}

IncrementalProgress GCRuntime::beginSweepingSweepGroup(JSFreeOp* fop,
                                                       SliceBudget& budget) {
  /*
   * Begin sweeping the group of zones in currentSweepGroup, performing
   * actions that must be done before yielding to caller.
   */

  using namespace gcstats;

  AutoSCC scc(stats(), sweepGroupIndex);

  bool sweepingAtoms = false;
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    /* Set the GC state to sweeping. */
    zone->changeGCState(Zone::MarkBlackAndGray, Zone::Sweep);

    /* Purge the ArenaLists before sweeping. */
    zone->arenas.unmarkPreMarkedFreeCells();
    zone->arenas.clearFreeLists();

    if (zone->isAtomsZone()) {
      sweepingAtoms = true;
    }
  }

#ifdef JS_GC_ZEAL
  validateIncrementalMarking();
#endif

#ifdef DEBUG
  for (auto cell : cellsToAssertNotGray.ref()) {
    JS::AssertCellIsNotGray(cell);
  }
  cellsToAssertNotGray.ref().clearAndFree();
#endif

  {
    AutoPhase ap(stats(), PhaseKind::FINALIZE_START);
    callFinalizeCallbacks(fop, JSFINALIZE_GROUP_PREPARE);
    {
      AutoPhase ap2(stats(), PhaseKind::WEAK_ZONES_CALLBACK);
      callWeakPointerZonesCallbacks();
    }
    {
      AutoPhase ap2(stats(), PhaseKind::WEAK_COMPARTMENT_CALLBACK);
      for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
        for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
          callWeakPointerCompartmentCallbacks(comp);
        }
      }
    }
    callFinalizeCallbacks(fop, JSFINALIZE_GROUP_START);
  }

  // Updating the atom marking bitmaps. This marks atoms referenced by
  // uncollected zones so cannot be done in parallel with the other sweeping
  // work below.
  if (sweepingAtoms) {
    AutoPhase ap(stats(), PhaseKind::UPDATE_ATOMS_BITMAP);
    updateAtomsBitmap();
  }

  AutoSetThreadIsSweeping threadIsSweeping;

  sweepDebuggerOnMainThread(fop);

  {
    AutoLockHelperThreadState lock;

    AutoPhase ap(stats(), PhaseKind::SWEEP_COMPARTMENTS);

    AutoRunParallelTask sweepCCWrappers(this, &GCRuntime::sweepCCWrappers,
                                        PhaseKind::SWEEP_CC_WRAPPER, lock);
    AutoRunParallelTask sweepObjectGroups(this, &GCRuntime::sweepObjectGroups,
                                          PhaseKind::SWEEP_TYPE_OBJECT, lock);
    AutoRunParallelTask sweepMisc(this, &GCRuntime::sweepMisc,
                                  PhaseKind::SWEEP_MISC, lock);
    AutoRunParallelTask sweepCompTasks(this, &GCRuntime::sweepCompressionTasks,
                                       PhaseKind::SWEEP_COMPRESSION, lock);
    AutoRunParallelTask sweepWeakMaps(this, &GCRuntime::sweepWeakMaps,
                                      PhaseKind::SWEEP_WEAKMAPS, lock);
    AutoRunParallelTask sweepUniqueIds(this, &GCRuntime::sweepUniqueIds,
                                       PhaseKind::SWEEP_UNIQUEIDS, lock);
    AutoRunParallelTask sweepWeakRefs(this, &GCRuntime::sweepWeakRefs,
                                      PhaseKind::SWEEP_WEAKREFS, lock);

    WeakCacheTaskVector sweepCacheTasks;
    if (PrepareWeakCacheTasks(rt, &sweepCacheTasks)) {
      weakCachesToSweep.ref().emplace(currentSweepGroup);
    } else {
      SweepAllWeakCachesOnMainThread(rt);
    }

    for (auto& task : sweepCacheTasks) {
      startTask(task, PhaseKind::SWEEP_WEAK_CACHES, lock);
    }

    {
      AutoUnlockHelperThreadState unlock(lock);
      sweepJitDataOnMainThread(fop);
    }

    for (auto& task : sweepCacheTasks) {
      joinTask(task, PhaseKind::SWEEP_WEAK_CACHES, lock);
    }
  }

  if (sweepingAtoms) {
    startSweepingAtomsTable();
  }

  // FinalizationRegistry sweeping touches weak maps and so must not run in
  // parallel with that.
  sweepFinalizationRegistriesOnMainThread();

  // Queue all GC things in all zones for sweeping, either on the foreground
  // or on the background thread.

  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    zone->arenas.queueForForegroundSweep(fop, ForegroundObjectFinalizePhase);
    zone->arenas.queueForForegroundSweep(fop, ForegroundNonObjectFinalizePhase);
    for (unsigned i = 0; i < ArrayLength(BackgroundFinalizePhases); ++i) {
      zone->arenas.queueForBackgroundSweep(fop, BackgroundFinalizePhases[i]);
    }

    zone->arenas.queueForegroundThingsForSweep();
  }

  MOZ_ASSERT(!sweepZone);

  safeToYield = true;

  return Finished;
}

#ifdef JS_GC_ZEAL
bool GCRuntime::shouldYieldForZeal(ZealMode mode) {
  bool yield = useZeal && isIncremental && hasZealMode(mode);

  // Only yield on the first sweep slice for this mode.
  bool firstSweepSlice = initialState != State::Sweep;
  if (mode == ZealMode::IncrementalMultipleSlices && !firstSweepSlice) {
    yield = false;
  }

  return yield;
}
#endif

IncrementalProgress GCRuntime::endSweepingSweepGroup(JSFreeOp* fop,
                                                     SliceBudget& budget) {
  // This is to prevent TSan data race, sweepMarkTask will check if the GC state
  // is Marking, but later below we will change GC state to Finished.
  if (joinSweepMarkTask() == NotFinished) {
    return NotFinished;
  }

  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::FINALIZE_END);
    JSFreeOp fop(rt);
    callFinalizeCallbacks(&fop, JSFINALIZE_GROUP_END);
  }

  /* Free LIFO blocks on a background thread if possible. */
  startBackgroundFree();

  /* Update the GC state for zones we have swept. */
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    if (jit::JitZone* jitZone = zone->jitZone()) {
      // Clear out any small pools that we're hanging on to.
      jitZone->execAlloc().purge();
    }
    AutoLockGC lock(this);
    zone->changeGCState(Zone::Sweep, Zone::Finished);
    zone->updateGCThresholds(*this, invocationKind, lock);
    zone->arenas.unmarkPreMarkedFreeCells();
  }

  /*
   * Start background thread to sweep zones if required, sweeping the atoms
   * zone last if present.
   */
  bool sweepAtomsZone = false;
  ZoneList zones;
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    if (zone->isAtomsZone()) {
      sweepAtomsZone = true;
    } else {
      zones.append(zone);
    }
  }
  if (sweepAtomsZone) {
    zones.append(atomsZone);
  }

  queueZonesAndStartBackgroundSweep(zones);

  return Finished;
}

void GCRuntime::beginSweepPhase(JS::GCReason reason, AutoGCSession& session) {
  /*
   * Sweep phase.
   *
   * Finalize as we sweep, outside of lock but with RuntimeHeapIsBusy()
   * true so that any attempt to allocate a GC-thing from a finalizer will
   * fail, rather than nest badly and leave the unmarked newborn to be swept.
   */

  MOZ_ASSERT(!abortSweepAfterCurrentGroup);

  releaseHeldRelocatedArenas();

#ifdef JS_GC_ZEAL
  computeNonIncrementalMarkingForValidation(session);
#endif

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);

  hasMarkedGrayRoots = false;

  AssertNoWrappersInGrayList(rt);
  dropStringWrappers();

  groupZonesForSweeping(reason);

  sweepActions->assertFinished();
}

bool ArenaLists::foregroundFinalize(JSFreeOp* fop, AllocKind thingKind,
                                    SliceBudget& sliceBudget,
                                    SortedArenaList& sweepList) {
  if (!arenaListsToSweep(thingKind) && incrementalSweptArenas.ref().isEmpty()) {
    return true;
  }

  // Arenas are released for use for new allocations as soon as the finalizers
  // for that allocation kind have run. This means that a cell's finalizer can
  // safely use IsAboutToBeFinalized to check other cells of the same alloc
  // kind, but not of different alloc kinds: the other arena may have already
  // had new objects allocated in it, and since we allocate black,
  // IsAboutToBeFinalized will return false even though the referent we intended
  // to check is long gone.
  if (!FinalizeArenas(fop, &arenaListsToSweep(thingKind), sweepList, thingKind,
                      sliceBudget)) {
    incrementalSweptArenaKind = thingKind;
    incrementalSweptArenas = sweepList.toArenaList();
    return false;
  }

  // Clear any previous incremental sweep state we may have saved.
  incrementalSweptArenas.ref().clear();

  sweepList.extractEmpty(&savedEmptyArenas.ref());

  ArenaList finalized = sweepList.toArenaList();
  arenaLists(thingKind) =
      finalized.insertListWithCursorAtEnd(arenaLists(thingKind));

  return true;
}

void js::gc::SweepMarkTask::run() {
  gc->sweepMarkResult = gc->markUntilBudgetExhausted(this->budget);
}

IncrementalProgress GCRuntime::joinSweepMarkTask() {
  joinTask(sweepMarkTask, gcstats::PhaseKind::SWEEP_MARK);

  return sweepMarkTaskStarted ? sweepMarkResult : Finished;
}

IncrementalProgress GCRuntime::markUntilBudgetExhausted(
    SliceBudget& sliceBudget, gcstats::PhaseKind phase) {
  gcstats::AutoPhase ap(stats(), phase);
  return markUntilBudgetExhausted(sliceBudget);
}

IncrementalProgress GCRuntime::markUntilBudgetExhausted(
    SliceBudget& sliceBudget) {
  // Run a marking slice and return whether the stack is now empty.

#ifdef DEBUG
  AutoSetThreadIsMarking threadIsMarking;
#endif  // DEBUG

  if (marker.processMarkQueue() == GCMarker::QueueYielded) {
    return NotFinished;
  }

  return marker.markUntilBudgetExhausted(sliceBudget) ? Finished : NotFinished;
}

void GCRuntime::drainMarkStack() {
  auto unlimited = SliceBudget::unlimited();
  MOZ_RELEASE_ASSERT(marker.markUntilBudgetExhausted(unlimited));
}

static void SweepThing(JSFreeOp* fop, Shape* shape) {
  if (!shape->isMarkedAny()) {
    shape->sweep(fop);
  }
}

static void SweepThing(JSFreeOp* fop, BaseScript* script) {
  AutoSweepJitScript sweep(script);
}

static void SweepThing(JSFreeOp* fop, ObjectGroup* group) {
  AutoSweepObjectGroup sweep(group);
}

template <typename T>
static bool SweepArenaList(JSFreeOp* fop, Arena** arenasToSweep,
                           SliceBudget& sliceBudget) {
  while (Arena* arena = *arenasToSweep) {
    for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
      SweepThing(fop, i.get<T>());
    }

    *arenasToSweep = (*arenasToSweep)->next;
    AllocKind kind = MapTypeToFinalizeKind<T>::kind;
    sliceBudget.step(Arena::thingsPerArena(kind));
    if (sliceBudget.isOverBudget()) {
      return false;
    }
  }

  return true;
}

IncrementalProgress GCRuntime::sweepTypeInformation(JSFreeOp* fop,
                                                    SliceBudget& budget) {
  // Sweep dead type information stored in scripts and object groups, but
  // don't finalize them yet. We have to sweep dead information from both live
  // and dead scripts and object groups, so that no dead references remain in
  // them. Type inference can end up crawling these zones again, such as for
  // TypeCompartment::markSetsUnknown, and if this happens after sweeping for
  // the sweep group finishes we won't be able to determine which things in
  // the zone are live.

  gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);
  gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::SWEEP_TYPES);

  ArenaLists& al = sweepZone->arenas;

  AutoClearTypeInferenceStateOnOOM oom(sweepZone);

  if (!SweepArenaList<BaseScript>(fop, &al.gcScriptArenasToUpdate.ref(),
                                  budget)) {
    return NotFinished;
  }

  if (!SweepArenaList<ObjectGroup>(fop, &al.gcObjectGroupArenasToUpdate.ref(),
                                   budget)) {
    return NotFinished;
  }

  // Finish sweeping type information in the zone.
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_TYPES_END);
    sweepZone->types.endSweep(rt);
  }

  return Finished;
}

IncrementalProgress GCRuntime::releaseSweptEmptyArenas(JSFreeOp* fop,
                                                       SliceBudget& budget) {
  // Foreground finalized GC things have already been finalized, and now their
  // arenas can be reclaimed by freeing empty ones and making non-empty ones
  // available for allocation.

  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    zone->arenas.releaseForegroundSweptEmptyArenas();
  }
  return Finished;
}

void GCRuntime::startSweepingAtomsTable() {
  auto& maybeAtoms = maybeAtomsToSweep.ref();
  MOZ_ASSERT(maybeAtoms.isNothing());

  AtomsTable* atomsTable = rt->atomsForSweeping();
  if (!atomsTable) {
    return;
  }

  // Create secondary tables to hold new atoms added while we're sweeping the
  // main tables incrementally.
  if (!atomsTable->startIncrementalSweep()) {
    SweepingTracer trc(rt);
    atomsTable->traceWeak(&trc);
    return;
  }

  // Initialize remaining atoms to sweep.
  maybeAtoms.emplace(*atomsTable);
}

IncrementalProgress GCRuntime::sweepAtomsTable(JSFreeOp* fop,
                                               SliceBudget& budget) {
  if (!atomsZone->isGCSweeping()) {
    return Finished;
  }

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_ATOMS_TABLE);

  auto& maybeAtoms = maybeAtomsToSweep.ref();
  if (!maybeAtoms) {
    return Finished;
  }

  if (!rt->atomsForSweeping()->sweepIncrementally(maybeAtoms.ref(), budget)) {
    return NotFinished;
  }

  maybeAtoms.reset();

  return Finished;
}

static size_t IncrementalSweepWeakCache(GCRuntime* gc,
                                        const WeakCacheToSweep& item) {
  AutoSetThreadIsSweeping threadIsSweeping(item.zone);

  JS::detail::WeakCacheBase* cache = item.cache;
  MOZ_ASSERT(cache->needsIncrementalBarrier());
  size_t steps = cache->sweep();
  cache->setNeedsIncrementalBarrier(false);

  return steps;
}

WeakCacheSweepIterator::WeakCacheSweepIterator(JS::Zone* sweepGroup)
    : sweepZone(sweepGroup), sweepCache(sweepZone->weakCaches().getFirst()) {
  settle();
}

bool WeakCacheSweepIterator::done() const { return !sweepZone; }

WeakCacheToSweep WeakCacheSweepIterator::get() const {
  MOZ_ASSERT(!done());

  return {sweepCache, sweepZone};
}

void WeakCacheSweepIterator::next() {
  MOZ_ASSERT(!done());

  sweepCache = sweepCache->getNext();
  settle();
}

void WeakCacheSweepIterator::settle() {
  while (sweepZone) {
    while (sweepCache && !sweepCache->needsIncrementalBarrier()) {
      sweepCache = sweepCache->getNext();
    }

    if (sweepCache) {
      break;
    }

    sweepZone = sweepZone->nextNodeInGroup();
    if (sweepZone) {
      sweepCache = sweepZone->weakCaches().getFirst();
    }
  }

  MOZ_ASSERT((!sweepZone && !sweepCache) ||
             (sweepCache && sweepCache->needsIncrementalBarrier()));
}

IncrementalProgress GCRuntime::sweepWeakCaches(JSFreeOp* fop,
                                               SliceBudget& budget) {
  if (weakCachesToSweep.ref().isNothing()) {
    return Finished;
  }

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);

  WeakCacheSweepIterator& work = weakCachesToSweep.ref().ref();

  AutoLockHelperThreadState lock;

  {
    AutoRunParallelWork runWork(this, IncrementalSweepWeakCache,
                                gcstats::PhaseKind::SWEEP_WEAK_CACHES, work,
                                budget, lock);
    AutoUnlockHelperThreadState unlock(lock);
  }

  if (work.done()) {
    weakCachesToSweep.ref().reset();
    return Finished;
  }

  return NotFinished;
}

IncrementalProgress GCRuntime::finalizeAllocKind(JSFreeOp* fop,
                                                 SliceBudget& budget) {
  // Set the number of things per arena for this AllocKind.
  size_t thingsPerArena = Arena::thingsPerArena(sweepAllocKind);
  auto& sweepList = incrementalSweepList.ref();
  sweepList.setThingsPerArena(thingsPerArena);

  AutoSetThreadIsSweeping threadIsSweeping(sweepZone);

  if (!sweepZone->arenas.foregroundFinalize(fop, sweepAllocKind, budget,
                                            sweepList)) {
    return NotFinished;
  }

  // Reset the slots of the sweep list that we used.
  sweepList.reset(thingsPerArena);

  return Finished;
}

IncrementalProgress GCRuntime::sweepShapeTree(JSFreeOp* fop,
                                              SliceBudget& budget) {
  // Remove dead shapes from the shape tree, but don't finalize them yet.

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_SHAPE);

  ArenaLists& al = sweepZone->arenas;

  if (!SweepArenaList<Shape>(fop, &al.gcShapeArenasToUpdate.ref(), budget)) {
    return NotFinished;
  }

  if (!SweepArenaList<AccessorShape>(
          fop, &al.gcAccessorShapeArenasToUpdate.ref(), budget)) {
    return NotFinished;
  }

  return Finished;
}

// An iterator for a standard container that provides an STL-like begin()/end()
// interface. This iterator provides a done()/get()/next() style interface.
template <typename Container>
class ContainerIter {
  using Iter = decltype(std::declval<const Container>().begin());
  using Elem = decltype(*std::declval<Iter>());

  Iter iter;
  const Iter end;

 public:
  explicit ContainerIter(const Container& container)
      : iter(container.begin()), end(container.end()) {}

  bool done() const { return iter == end; }

  Elem get() const { return *iter; }

  void next() {
    MOZ_ASSERT(!done());
    ++iter;
  }
};

// IncrementalIter is a template class that makes a normal iterator into one
// that can be used to perform incremental work by using external state that
// persists between instantiations. The state is only initialised on the first
// use and subsequent uses carry on from the previous state.
template <typename Iter>
struct IncrementalIter {
  using State = Maybe<Iter>;
  using Elem = decltype(std::declval<Iter>().get());

 private:
  State& maybeIter;

 public:
  template <typename... Args>
  explicit IncrementalIter(State& maybeIter, Args&&... args)
      : maybeIter(maybeIter) {
    if (maybeIter.isNothing()) {
      maybeIter.emplace(std::forward<Args>(args)...);
    }
  }

  ~IncrementalIter() {
    if (done()) {
      maybeIter.reset();
    }
  }

  bool done() const { return maybeIter.ref().done(); }

  Elem get() const { return maybeIter.ref().get(); }

  void next() { maybeIter.ref().next(); }
};

// Iterate through the sweep groups created by
// GCRuntime::groupZonesForSweeping().
class js::gc::SweepGroupsIter {
  GCRuntime* gc;

 public:
  explicit SweepGroupsIter(JSRuntime* rt) : gc(&rt->gc) {
    MOZ_ASSERT(gc->currentSweepGroup);
  }

  bool done() const { return !gc->currentSweepGroup; }

  Zone* get() const { return gc->currentSweepGroup; }

  void next() {
    MOZ_ASSERT(!done());
    gc->getNextSweepGroup();
  }
};

namespace sweepaction {

// Implementation of the SweepAction interface that calls a method on GCRuntime.
class SweepActionCall final : public SweepAction {
  using Method = IncrementalProgress (GCRuntime::*)(JSFreeOp* fop,
                                                    SliceBudget& budget);

  Method method;

 public:
  explicit SweepActionCall(Method m) : method(m) {}
  IncrementalProgress run(Args& args) override {
    return (args.gc->*method)(args.fop, args.budget);
  }
  void assertFinished() const override {}
};

// Implementation of the SweepAction interface that yields in a specified zeal
// mode.
class SweepActionMaybeYield final : public SweepAction {
#ifdef JS_GC_ZEAL
  ZealMode mode;
  bool isYielding;
#endif

 public:
  explicit SweepActionMaybeYield(ZealMode mode)
#ifdef JS_GC_ZEAL
      : mode(mode),
        isYielding(false)
#endif
  {
  }

  IncrementalProgress run(Args& args) override {
#ifdef JS_GC_ZEAL
    if (!isYielding && args.gc->shouldYieldForZeal(mode)) {
      isYielding = true;
      return NotFinished;
    }

    isYielding = false;
#endif
    return Finished;
  }

  void assertFinished() const override { MOZ_ASSERT(!isYielding); }

  // These actions should be skipped if GC zeal is not configured.
#ifndef JS_GC_ZEAL
  bool shouldSkip() override { return true; }
#endif
};

// Implementation of the SweepAction interface that calls a list of actions in
// sequence.
class SweepActionSequence final : public SweepAction {
  using ActionVector = Vector<UniquePtr<SweepAction>, 0, SystemAllocPolicy>;
  using Iter = IncrementalIter<ContainerIter<ActionVector>>;

  ActionVector actions;
  typename Iter::State iterState;

 public:
  bool init(UniquePtr<SweepAction>* acts, size_t count) {
    for (size_t i = 0; i < count; i++) {
      auto& action = acts[i];
      if (!action) {
        return false;
      }
      if (action->shouldSkip()) {
        continue;
      }
      if (!actions.emplaceBack(std::move(action))) {
        return false;
      }
    }
    return true;
  }

  IncrementalProgress run(Args& args) override {
    for (Iter iter(iterState, actions); !iter.done(); iter.next()) {
      if (iter.get()->run(args) == NotFinished) {
        return NotFinished;
      }
    }
    return Finished;
  }

  void assertFinished() const override {
    MOZ_ASSERT(iterState.isNothing());
    for (const auto& action : actions) {
      action->assertFinished();
    }
  }
};

template <typename Iter, typename Init>
class SweepActionForEach final : public SweepAction {
  using Elem = decltype(std::declval<Iter>().get());
  using IncrIter = IncrementalIter<Iter>;

  Init iterInit;
  Elem* elemOut;
  UniquePtr<SweepAction> action;
  typename IncrIter::State iterState;

 public:
  SweepActionForEach(const Init& init, Elem* maybeElemOut,
                     UniquePtr<SweepAction> action)
      : iterInit(init), elemOut(maybeElemOut), action(std::move(action)) {}

  IncrementalProgress run(Args& args) override {
    MOZ_ASSERT_IF(elemOut, *elemOut == Elem());
    auto clearElem = mozilla::MakeScopeExit([&] { setElem(Elem()); });
    for (IncrIter iter(iterState, iterInit); !iter.done(); iter.next()) {
      setElem(iter.get());
      if (action->run(args) == NotFinished) {
        return NotFinished;
      }
    }
    return Finished;
  }

  void assertFinished() const override {
    MOZ_ASSERT(iterState.isNothing());
    MOZ_ASSERT_IF(elemOut, *elemOut == Elem());
    action->assertFinished();
  }

 private:
  void setElem(const Elem& value) {
    if (elemOut) {
      *elemOut = value;
    }
  }
};

static UniquePtr<SweepAction> Call(IncrementalProgress (GCRuntime::*method)(
    JSFreeOp* fop, SliceBudget& budget)) {
  return MakeUnique<SweepActionCall>(method);
}

static UniquePtr<SweepAction> MaybeYield(ZealMode zealMode) {
  return MakeUnique<SweepActionMaybeYield>(zealMode);
}

template <typename... Rest>
static UniquePtr<SweepAction> Sequence(UniquePtr<SweepAction> first,
                                       Rest... rest) {
  UniquePtr<SweepAction> actions[] = {std::move(first), std::move(rest)...};
  auto seq = MakeUnique<SweepActionSequence>();
  if (!seq || !seq->init(actions, ArrayLength(actions))) {
    return nullptr;
  }

  return UniquePtr<SweepAction>(std::move(seq));
}

static UniquePtr<SweepAction> RepeatForSweepGroup(
    JSRuntime* rt, UniquePtr<SweepAction> action) {
  if (!action) {
    return nullptr;
  }

  using Action = SweepActionForEach<SweepGroupsIter, JSRuntime*>;
  return js::MakeUnique<Action>(rt, nullptr, std::move(action));
}

static UniquePtr<SweepAction> ForEachZoneInSweepGroup(
    JSRuntime* rt, Zone** zoneOut, UniquePtr<SweepAction> action) {
  if (!action) {
    return nullptr;
  }

  using Action = SweepActionForEach<SweepGroupZonesIter, JSRuntime*>;
  return js::MakeUnique<Action>(rt, zoneOut, std::move(action));
}

static UniquePtr<SweepAction> ForEachAllocKind(AllocKinds kinds,
                                               AllocKind* kindOut,
                                               UniquePtr<SweepAction> action) {
  if (!action) {
    return nullptr;
  }

  using Action = SweepActionForEach<ContainerIter<AllocKinds>, AllocKinds>;
  return js::MakeUnique<Action>(kinds, kindOut, std::move(action));
}

}  // namespace sweepaction

bool GCRuntime::initSweepActions() {
  using namespace sweepaction;
  using sweepaction::Call;

  sweepActions.ref() = RepeatForSweepGroup(
      rt,
      Sequence(
          Call(&GCRuntime::markGrayReferencesInCurrentGroup),
          Call(&GCRuntime::endMarkingSweepGroup),
          Call(&GCRuntime::beginSweepingSweepGroup),
          MaybeYield(ZealMode::IncrementalMultipleSlices),
          MaybeYield(ZealMode::YieldBeforeSweepingAtoms),
          Call(&GCRuntime::sweepAtomsTable),
          MaybeYield(ZealMode::YieldBeforeSweepingCaches),
          Call(&GCRuntime::sweepWeakCaches),
          ForEachZoneInSweepGroup(
              rt, &sweepZone.ref(),
              Sequence(MaybeYield(ZealMode::YieldBeforeSweepingTypes),
                       Call(&GCRuntime::sweepTypeInformation),
                       MaybeYield(ZealMode::YieldBeforeSweepingObjects),
                       ForEachAllocKind(ForegroundObjectFinalizePhase.kinds,
                                        &sweepAllocKind.ref(),
                                        Call(&GCRuntime::finalizeAllocKind)),
                       MaybeYield(ZealMode::YieldBeforeSweepingNonObjects),
                       ForEachAllocKind(ForegroundNonObjectFinalizePhase.kinds,
                                        &sweepAllocKind.ref(),
                                        Call(&GCRuntime::finalizeAllocKind)),
                       MaybeYield(ZealMode::YieldBeforeSweepingShapeTrees),
                       Call(&GCRuntime::sweepShapeTree))),
          Call(&GCRuntime::releaseSweptEmptyArenas),
          Call(&GCRuntime::endSweepingSweepGroup)));

  return sweepActions != nullptr;
}

IncrementalProgress GCRuntime::performSweepActions(SliceBudget& budget) {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);
  JSFreeOp fop(rt);

  // Kick off a parallel task to drain the mark stack, except in the first sweep
  // slice where we must not yield to the mutator until we've starting sweeping
  // a sweep group. The stack must be empty in that case.
  MOZ_ASSERT(initialState <= State::Sweep);
  MOZ_ASSERT_IF(initialState != State::Sweep, marker.isDrained());
  MOZ_ASSERT(!sweepMarkTaskStarted);
  if (initialState == State::Sweep && !marker.isDrained()) {
    AutoLockHelperThreadState lock;
    MOZ_ASSERT(sweepMarkTask.isIdle(lock));
    sweepMarkTask.setBudget(budget);
    sweepMarkTask.startOrRunIfIdle(lock);
    sweepMarkTaskStarted = true;
  }

  SweepAction::Args args{this, &fop, budget};
  IncrementalProgress progress = sweepActions->run(args);

  if (sweepMarkTaskStarted) {
    joinTask(sweepMarkTask, gcstats::PhaseKind::SWEEP_MARK);
    sweepMarkTaskStarted = false;
    if (sweepMarkResult == NotFinished) {
      progress = NotFinished;
    }
  }

  MOZ_ASSERT_IF(progress == NotFinished, isIncremental);
  return progress;
}

bool GCRuntime::allCCVisibleZonesWereCollected() {
  // Calculate whether the gray marking state is now valid.
  //
  // The gray bits change from invalid to valid if we finished a full GC from
  // the point of view of the cycle collector. We ignore the following:
  //
  //  - Helper thread zones, as these are not reachable from the main heap.
  //  - The atoms zone, since strings and symbols are never marked gray.
  //  - Empty zones.
  //
  // These exceptions ensure that when the CC requests a full GC the gray mark
  // state ends up valid even it we don't collect all of the zones.

  for (ZonesIter zone(this, SkipAtoms); !zone.done(); zone.next()) {
    if (!zone->isCollecting() && !zone->usedByHelperThread() &&
        !zone->arenas.arenaListsAreEmpty()) {
      return false;
    }
  }

  return true;
}

void GCRuntime::endSweepPhase(bool destroyingRuntime) {
  sweepActions->assertFinished();

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);
  JSFreeOp fop(rt);

  MOZ_ASSERT_IF(destroyingRuntime, !sweepOnBackgroundThread);

  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::DESTROY);

    /*
     * Sweep script filenames after sweeping functions in the generic loop
     * above. In this way when a scripted function's finalizer destroys the
     * script and calls rt->destroyScriptHook, the hook can still access the
     * script's filename. See bug 323267.
     */
    SweepScriptData(rt);
  }

  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::FINALIZE_END);
    callFinalizeCallbacks(&fop, JSFINALIZE_COLLECTION_END);

    if (allCCVisibleZonesWereCollected()) {
      grayBitsValid = true;
    }
  }

#ifdef JS_GC_ZEAL
  finishMarkingValidation();
#endif

#ifdef DEBUG
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    for (auto i : AllAllocKinds()) {
      MOZ_ASSERT_IF(!IsBackgroundFinalized(i) || !sweepOnBackgroundThread,
                    !zone->arenas.arenaListsToSweep(i));
    }
  }
#endif

  AssertNoWrappersInGrayList(rt);
}

void GCRuntime::beginCompactPhase() {
  MOZ_ASSERT(!isBackgroundSweeping());
  assertBackgroundSweepingFinished();

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT);

  MOZ_ASSERT(zonesToMaybeCompact.ref().isEmpty());
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    if (canRelocateZone(zone)) {
      zonesToMaybeCompact.ref().append(zone);
    }
  }

  MOZ_ASSERT(!relocatedArenasToRelease);
  startedCompacting = true;
}

IncrementalProgress GCRuntime::compactPhase(JS::GCReason reason,
                                            SliceBudget& sliceBudget,
                                            AutoGCSession& session) {
  assertBackgroundSweepingFinished();
  MOZ_ASSERT(startedCompacting);

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT);

  // TODO: JSScripts can move. If the sampler interrupts the GC in the
  // middle of relocating an arena, invalid JSScript pointers may be
  // accessed. Suppress all sampling until a finer-grained solution can be
  // found. See bug 1295775.
  AutoSuppressProfilerSampling suppressSampling(rt->mainContextFromOwnThread());

  ZoneList relocatedZones;
  Arena* relocatedArenas = nullptr;
  while (!zonesToMaybeCompact.ref().isEmpty()) {
    Zone* zone = zonesToMaybeCompact.ref().front();
    zonesToMaybeCompact.ref().removeFront();

    MOZ_ASSERT(nursery().isEmpty());
    zone->changeGCState(Zone::Finished, Zone::Compact);

    if (relocateArenas(zone, reason, relocatedArenas, sliceBudget)) {
      updateZonePointersToRelocatedCells(zone);
      relocatedZones.append(zone);
    } else {
      zone->changeGCState(Zone::Compact, Zone::Finished);
    }

    if (sliceBudget.isOverBudget()) {
      break;
    }
  }

  if (!relocatedZones.isEmpty()) {
    updateRuntimePointersToRelocatedCells(session);

    do {
      Zone* zone = relocatedZones.front();
      relocatedZones.removeFront();
      zone->changeGCState(Zone::Compact, Zone::Finished);
    } while (!relocatedZones.isEmpty());
  }

  clearRelocatedArenas(relocatedArenas, reason);

  if (ShouldProtectRelocatedArenas(reason)) {
    protectAndHoldArenas(relocatedArenas);
  } else {
    releaseRelocatedArenas(relocatedArenas);
  }

  // Clear caches that can contain cell pointers.
  rt->caches().purgeForCompaction();

#ifdef DEBUG
  checkHashTablesAfterMovingGC();
#endif

  return zonesToMaybeCompact.ref().isEmpty() ? Finished : NotFinished;
}

void GCRuntime::endCompactPhase() { startedCompacting = false; }

void GCRuntime::finishCollection() {
  assertBackgroundSweepingFinished();
  MOZ_ASSERT(marker.isDrained());
  marker.stop();
  clearBufferedGrayRoots();

  auto currentTime = ReallyNow();
  schedulingState.updateHighFrequencyMode(lastGCEndTime_, currentTime,
                                          tunables);

  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    if (zone->isCollecting()) {
      zone->changeGCState(Zone::Finished, Zone::NoGC);
      zone->gcDelayBytes = 0;
      zone->notifyObservingDebuggers();
    }

    MOZ_ASSERT(!zone->wasGCStarted());
    MOZ_ASSERT(!zone->needsIncrementalBarrier());
    MOZ_ASSERT(!zone->isOnList());
  }

  MOZ_ASSERT(zonesToMaybeCompact.ref().isEmpty());
  MOZ_ASSERT(cellsToAssertNotGray.ref().empty());

  lastGCEndTime_ = currentTime;
}

static const char* HeapStateToLabel(JS::HeapState heapState) {
  switch (heapState) {
    case JS::HeapState::MinorCollecting:
      return "js::Nursery::collect";
    case JS::HeapState::MajorCollecting:
      return "js::GCRuntime::collect";
    case JS::HeapState::Tracing:
      return "JS_IterateCompartments";
    case JS::HeapState::Idle:
    case JS::HeapState::CycleCollecting:
      MOZ_CRASH(
          "Should never have an Idle or CC heap state when pushing GC "
          "profiling stack frames!");
  }
  MOZ_ASSERT_UNREACHABLE("Should have exhausted every JS::HeapState variant!");
  return nullptr;
}

/* Start a new heap session. */
AutoHeapSession::AutoHeapSession(GCRuntime* gc, JS::HeapState heapState)
    : gc(gc),
      prevState(gc->heapState_),
      profilingStackFrame(gc->rt->mainContextFromOwnThread(),
                          HeapStateToLabel(heapState),
                          JS::ProfilingCategoryPair::GCCC) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(gc->rt));
  MOZ_ASSERT(prevState == JS::HeapState::Idle);
  MOZ_ASSERT(heapState != JS::HeapState::Idle);

  gc->heapState_ = heapState;
}

AutoHeapSession::~AutoHeapSession() {
  MOZ_ASSERT(JS::RuntimeHeapIsBusy());
  gc->heapState_ = prevState;
}

JS_PUBLIC_API JS::HeapState JS::RuntimeHeapState() {
  return TlsContext.get()->runtime()->gc.heapState();
}

GCRuntime::IncrementalResult GCRuntime::resetIncrementalGC(
    gc::AbortReason reason) {
  // Drop as much work as possible from an ongoing incremental GC so
  // we can start a new GC after it has finished.
  if (incrementalState == State::NotActive) {
    return IncrementalResult::Ok;
  }

  AutoGCSession session(this, JS::HeapState::MajorCollecting);

  switch (incrementalState) {
    case State::NotActive:
    case State::MarkRoots:
    case State::Finish:
      MOZ_CRASH("Unexpected GC state in resetIncrementalGC");
      break;

    case State::Mark: {
      // Cancel any ongoing marking.
      marker.reset();
      clearBufferedGrayRoots();

      for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        ResetGrayList(c);
      }

      for (GCZonesIter zone(this); !zone.done(); zone.next()) {
        zone->setNeedsIncrementalBarrier(false);
        zone->changeGCState(Zone::MarkBlackOnly, Zone::NoGC);
        zone->gcDelayBytes = 0;
        zone->arenas.unmarkPreMarkedFreeCells();
      }

      {
        AutoLockHelperThreadState lock;
        lifoBlocksToFree.ref().freeAll();
      }

      lastMarkSlice = false;
      incrementalState = State::Finish;

      MOZ_ASSERT(!marker.shouldCheckCompartments());

      break;
    }

    case State::Sweep: {
      // Finish sweeping the current sweep group, then abort.
      for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->gcState.scheduledForDestruction = false;
      }

      abortSweepAfterCurrentGroup = true;
      isCompacting = false;

      break;
    }

    case State::Finalize: {
      isCompacting = false;
      break;
    }

    case State::Compact: {
      // Skip any remaining zones that would have been compacted.
      MOZ_ASSERT(isCompacting);
      startedCompacting = true;
      zonesToMaybeCompact.ref().clear();
      break;
    }

    case State::Decommit: {
      break;
    }
  }

  stats().reset(reason);

  return IncrementalResult::ResetIncremental;
}

namespace {

/*
 * Temporarily disable barriers during GC slices.
 */
class AutoDisableBarriers {
 public:
  explicit AutoDisableBarriers(GCRuntime* gc);
  ~AutoDisableBarriers();

 private:
  GCRuntime* gc;
};

} /* anonymous namespace */

AutoDisableBarriers::AutoDisableBarriers(GCRuntime* gc) : gc(gc) {
  for (GCZonesIter zone(gc); !zone.done(); zone.next()) {
    /*
     * Clear needsIncrementalBarrier early so we don't do any write
     * barriers during GC. We don't need to update the Ion barriers (which
     * is expensive) because Ion code doesn't run during GC. If need be,
     * we'll update the Ion barriers in ~AutoDisableBarriers.
     */
    if (zone->isGCMarking()) {
      MOZ_ASSERT(zone->needsIncrementalBarrier());
      zone->setNeedsIncrementalBarrier(false);
    }
    MOZ_ASSERT(!zone->needsIncrementalBarrier());
  }
}

AutoDisableBarriers::~AutoDisableBarriers() {
  /* We can't use GCZonesIter if this is the end of the last slice. */
  for (ZonesIter zone(gc, WithAtoms); !zone.done(); zone.next()) {
    MOZ_ASSERT(!zone->needsIncrementalBarrier());
    if (zone->isGCMarking()) {
      zone->setNeedsIncrementalBarrier(true);
    }
  }
}

static bool ShouldCleanUpEverything(JS::GCReason reason,
                                    JSGCInvocationKind gckind) {
  // During shutdown, we must clean everything up, for the sake of leak
  // detection. When a runtime has no contexts, or we're doing a GC before a
  // shutdown CC, those are strong indications that we're shutting down.
  return IsShutdownGC(reason) || gckind == GC_SHRINK;
}

static bool ShouldSweepOnBackgroundThread(JS::GCReason reason) {
  return reason != JS::GCReason::DESTROY_RUNTIME && !gcTracer.traceEnabled() &&
         CanUseExtraThreads();
}

void GCRuntime::incrementalSlice(SliceBudget& budget,
                                 const MaybeInvocationKind& gckind,
                                 JS::GCReason reason, AutoGCSession& session) {
  AutoDisableBarriers disableBarriers(this);
  AutoSetThreadIsPerformingGC performingGC;

  bool destroyingRuntime = (reason == JS::GCReason::DESTROY_RUNTIME);

  initialState = incrementalState;

#ifdef JS_GC_ZEAL
  /*
   * Do the incremental collection type specified by zeal mode if the
   * collection was triggered by runDebugGC() and incremental GC has not been
   * cancelled by resetIncrementalGC().
   */
  useZeal = reason == JS::GCReason::DEBUG_GC && !budget.isUnlimited();
#else
  bool useZeal = false;
#endif

#ifdef DEBUG
  {
    char budgetBuffer[32];
    budget.describe(budgetBuffer, 32);
    stats().writeLogMessage(
        "Incremental: %d, lastMarkSlice: %d, useZeal: %d, budget: %s",
        bool(isIncremental), bool(lastMarkSlice), bool(useZeal), budgetBuffer);
  }
#endif

  MOZ_ASSERT_IF(isIncrementalGCInProgress(), isIncremental);

  /*
   * Non-incremental collection expects that the nursery is empty.
   */
  if (!isIncremental) {
    MOZ_ASSERT(nursery().isEmpty());
    storeBuffer().checkEmpty();
  }

  isIncremental = !budget.isUnlimited();

  if (useZeal && hasIncrementalTwoSliceZealMode()) {
    /*
     * Yields between slices occurs at predetermined points in these modes;
     * the budget is not used.
     */
    stats().writeLogMessage("Using unlimited budget for two-slice zeal mode");
    budget.makeUnlimited();
  }

  incGcSliceNumber();

  switch (incrementalState) {
    case State::NotActive:
      incMajorGcNumber();
      invocationKind = gckind.valueOr(GC_NORMAL);
      initialReason = reason;
      cleanUpEverything = ShouldCleanUpEverything(reason, invocationKind);
      sweepOnBackgroundThread = ShouldSweepOnBackgroundThread(reason);
      isCompacting = shouldCompact();
      MOZ_ASSERT(!lastMarkSlice);
      rootsRemoved = false;
      lastGCStartTime_ = ReallyNow();

#ifdef DEBUG
      for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
        zone->gcSweepGroupIndex = 0;
      }
#endif

      incrementalState = State::MarkRoots;

      [[fallthrough]];

    case State::MarkRoots:
      if (!beginMarkPhase(reason, session)) {
        incrementalState = State::NotActive;
        return;
      }

      /* If we needed delayed marking for gray roots, then collect until done.
       */
      if (isIncremental && !hasValidGrayRootsBuffer()) {
        budget.makeUnlimited();
        isIncremental = false;
        stats().nonincremental(AbortReason::GrayRootBufferingFailed);
      }

      incrementalState = State::Mark;

      if (isIncremental && useZeal &&
          hasZealMode(ZealMode::YieldBeforeMarking)) {
        break;
      }

      [[fallthrough]];

    case State::Mark:
      AutoGCRooter::traceAllWrappers(rt->mainContextFromOwnThread(), &marker);

      if (markUntilBudgetExhausted(budget, gcstats::PhaseKind::MARK) ==
          NotFinished) {
        break;
      }

      MOZ_ASSERT(marker.isDrained());

      /*
       * There are a number of reasons why we break out of collection here,
       * either ending the slice or to run a new interation of the loop in
       * GCRuntime::collect()
       */

      /*
       * In incremental GCs where we have already performed more than one
       * slice we yield after marking with the aim of starting the sweep in
       * the next slice, since the first slice of sweeping can be expensive.
       *
       * This is modified by the various zeal modes.  We don't yield in
       * YieldBeforeMarking mode and we always yield in YieldBeforeSweeping
       * mode.
       *
       * We will need to mark anything new on the stack when we resume, so
       * we stay in Mark state.
       */
      if (isIncremental && !lastMarkSlice) {
        if ((initialState == State::Mark &&
             !(useZeal && hasZealMode(ZealMode::YieldBeforeMarking))) ||
            (useZeal && hasZealMode(ZealMode::YieldBeforeSweeping))) {
          lastMarkSlice = true;
          stats().writeLogMessage("Yielding before starting sweeping");
          break;
        }
      }

      incrementalState = State::Sweep;
      lastMarkSlice = false;
      beginSweepPhase(reason, session);

      [[fallthrough]];

    case State::Sweep:
      MOZ_ASSERT(nursery().isEmpty());
      storeBuffer().checkEmpty();

      AutoGCRooter::traceAllWrappers(rt->mainContextFromOwnThread(), &marker);

      if (performSweepActions(budget) == NotFinished) {
        break;
      }

      endSweepPhase(destroyingRuntime);

      incrementalState = State::Finalize;

      [[fallthrough]];

    case State::Finalize: {
      gcstats::AutoPhase ap(stats(),
                            gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);

      // Yield until background finalization is done.
      if (!budget.isUnlimited()) {
        // Poll for end of background sweeping
        if (isBackgroundSweeping()) {
          break;
        }
      } else {
        waitBackgroundSweepEnd();
      }
    }

      {
        // Re-sweep the zones list, now that background finalization is
        // finished to actually remove and free dead zones.
        gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP);
        gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::DESTROY);
        JSFreeOp fop(rt);
        sweepZones(&fop, destroyingRuntime);
      }

      MOZ_ASSERT(!startedCompacting);
      incrementalState = State::Compact;

      // Always yield before compacting since it is not incremental.
      if (isCompacting && !budget.isUnlimited()) {
        break;
      }

      [[fallthrough]];

    case State::Compact:
      if (isCompacting) {
        MOZ_ASSERT(nursery().isEmpty());
        storeBuffer().checkEmpty();
        if (!startedCompacting) {
          beginCompactPhase();
        }

        if (compactPhase(reason, budget, session) == NotFinished) {
          break;
        }

        endCompactPhase();
      }

      startDecommit();
      incrementalState = State::Decommit;

      [[fallthrough]];

    case State::Decommit: {
      gcstats::AutoPhase ap(stats(),
                            gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);

      // Yield until background decommit is done.
      if (!budget.isUnlimited() && decommitTask.wasStarted()) {
        break;
      }

      decommitTask.join();

      incrementalState = State::Finish;

      [[fallthrough]];
    }

    case State::Finish:
      finishCollection();
      incrementalState = State::NotActive;
      break;
  }

  MOZ_ASSERT(safeToYield);
  MOZ_ASSERT(marker.markColor() == MarkColor::Black);
}

bool GCRuntime::hasForegroundWork() const {
  switch (incrementalState) {
    case State::NotActive:
      // Incremental GC is not running and no work is pending.
      return false;
    case State::Finalize:
      // We yield in the Finalize state to wait for background sweeping.
      return !isBackgroundSweeping();
    case State::Decommit:
      // We yield in the Decommit state to wait for background decommit.
      return !decommitTask.wasStarted();
    default:
      // In all other states there is still work to do.
      return true;
  }
}

gc::AbortReason gc::IsIncrementalGCUnsafe(JSRuntime* rt) {
  MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

  if (!rt->gc.isIncrementalGCAllowed()) {
    return gc::AbortReason::IncrementalDisabled;
  }

  return gc::AbortReason::None;
}

inline void GCRuntime::checkZoneIsScheduled(Zone* zone, JS::GCReason reason,
                                            const char* trigger) {
#ifdef DEBUG
  if (zone->isGCScheduled()) {
    return;
  }

  fprintf(stderr,
          "checkZoneIsScheduled: Zone %p not scheduled as expected in %s GC "
          "for %s trigger\n",
          zone, JS::ExplainGCReason(reason), trigger);
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    fprintf(stderr, "  Zone %p:%s%s\n", zone.get(),
            zone->isAtomsZone() ? " atoms" : "",
            zone->isGCScheduled() ? " scheduled" : "");
  }
  fflush(stderr);
  MOZ_CRASH("Zone not scheduled");
#endif
}

GCRuntime::IncrementalResult GCRuntime::budgetIncrementalGC(
    bool nonincrementalByAPI, JS::GCReason reason, SliceBudget& budget) {
  if (nonincrementalByAPI) {
    stats().nonincremental(gc::AbortReason::NonIncrementalRequested);
    budget.makeUnlimited();

    // Reset any in progress incremental GC if this was triggered via the
    // API. This isn't required for correctness, but sometimes during tests
    // the caller expects this GC to collect certain objects, and we need
    // to make sure to collect everything possible.
    if (reason != JS::GCReason::ALLOC_TRIGGER) {
      return resetIncrementalGC(gc::AbortReason::NonIncrementalRequested);
    }

    return IncrementalResult::Ok;
  }

  if (reason == JS::GCReason::ABORT_GC) {
    budget.makeUnlimited();
    stats().nonincremental(gc::AbortReason::AbortRequested);
    return resetIncrementalGC(gc::AbortReason::AbortRequested);
  }

  AbortReason unsafeReason = IsIncrementalGCUnsafe(rt);
  if (unsafeReason == AbortReason::None) {
    if (reason == JS::GCReason::COMPARTMENT_REVIVED) {
      unsafeReason = gc::AbortReason::CompartmentRevived;
    } else if (mode != JSGC_MODE_INCREMENTAL &&
               mode != JSGC_MODE_ZONE_INCREMENTAL) {
      unsafeReason = gc::AbortReason::ModeChange;
    }
  }

  if (unsafeReason != AbortReason::None) {
    budget.makeUnlimited();
    stats().nonincremental(unsafeReason);
    return resetIncrementalGC(unsafeReason);
  }

  AbortReason resetReason = AbortReason::None;
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    if (!zone->canCollect()) {
      continue;
    }

    if (zone->gcHeapSize.bytes() >=
        zone->gcHeapThreshold.nonIncrementalTriggerBytes(tunables)) {
      checkZoneIsScheduled(zone, reason, "GC bytes");
      budget.makeUnlimited();
      stats().nonincremental(AbortReason::GCBytesTrigger);
      if (zone->wasGCStarted() && zone->gcState() > Zone::Sweep) {
        resetReason = AbortReason::GCBytesTrigger;
      }
    }

    if (zone->mallocHeapSize.bytes() >=
        zone->mallocHeapThreshold.nonIncrementalTriggerBytes(tunables)) {
      checkZoneIsScheduled(zone, reason, "malloc bytes");
      budget.makeUnlimited();
      stats().nonincremental(AbortReason::MallocBytesTrigger);
      if (zone->wasGCStarted() && zone->gcState() > Zone::Sweep) {
        resetReason = AbortReason::MallocBytesTrigger;
      }
    }

    if (zone->jitHeapSize.bytes() >=
        zone->jitHeapThreshold.nonIncrementalTriggerBytes(tunables)) {
      checkZoneIsScheduled(zone, reason, "JIT code bytes");
      budget.makeUnlimited();
      stats().nonincremental(AbortReason::JitCodeBytesTrigger);
      if (zone->wasGCStarted() && zone->gcState() > Zone::Sweep) {
        resetReason = AbortReason::JitCodeBytesTrigger;
      }
    }

    if (isIncrementalGCInProgress() &&
        zone->isGCScheduled() != zone->wasGCStarted()) {
      budget.makeUnlimited();
      resetReason = AbortReason::ZoneChange;
    }
  }

  if (resetReason != AbortReason::None) {
    return resetIncrementalGC(resetReason);
  }

  return IncrementalResult::Ok;
}

static double LinearInterpolate(double x, double x0, double y0, double x1,
                                double y1) {
  MOZ_ASSERT(x0 < x1);

  if (x < x0) {
    return y0;
  }

  if (x < x1) {
    return y0 + (y1 - y0) * ((x - x0) / (x1 - x0));
  }

  return y1;
}

void GCRuntime::maybeIncreaseSliceBudget(SliceBudget& budget) {
#ifndef JS_MORE_DETERMINISTIC
  // Increase time budget for long-running incremental collections. Enforce a
  // minimum time budget that increases linearly with time/slice count up to a
  // maximum.

  if (budget.isTimeBudget() && !budget.isUnlimited() &&
      isIncrementalGCInProgress()) {
    // All times are in milliseconds.
    struct BudgetAtTime {
      double time;
      double budget;
    };
    const BudgetAtTime MinBudgetStart{1500, 0.0};
    const BudgetAtTime MinBudgetEnd{2500, 100.0};

    double totalTime = (ReallyNow() - lastGCStartTime()).ToMilliseconds();

    double minBudget =
        LinearInterpolate(totalTime, MinBudgetStart.time, MinBudgetStart.budget,
                          MinBudgetEnd.time, MinBudgetEnd.budget);

    if (budget.timeBudget.budget < minBudget) {
      budget = SliceBudget(TimeBudget(minBudget));
    }
  }
#endif  // JS_MORE_DETERMINISTIC
}

static void ScheduleZones(GCRuntime* gc) {
  for (ZonesIter zone(gc, WithAtoms); !zone.done(); zone.next()) {
    if (!zone->canCollect()) {
      continue;
    }

    if (gc->gcMode() == JSGC_MODE_GLOBAL ||
        gc->gcMode() == JSGC_MODE_INCREMENTAL) {
      zone->scheduleGC();
    }

    // To avoid resets, continue to collect any zones that were being
    // collected in a previous slice.
    if (gc->isIncrementalGCInProgress() && zone->wasGCStarted()) {
      zone->scheduleGC();
    }

    // This is a heuristic to reduce the total number of collections.
    bool inHighFrequencyMode = gc->schedulingState.inHighFrequencyGCMode();
    if (zone->gcHeapSize.bytes() >=
            zone->gcHeapThreshold.eagerAllocTrigger(inHighFrequencyMode) ||
        zone->mallocHeapSize.bytes() >=
            zone->mallocHeapThreshold.eagerAllocTrigger(inHighFrequencyMode) ||
        zone->jitHeapSize.bytes() >= zone->jitHeapThreshold.bytes()) {
      zone->scheduleGC();
    }
  }
}

static void UnscheduleZones(GCRuntime* gc) {
  for (ZonesIter zone(gc->rt, WithAtoms); !zone.done(); zone.next()) {
    zone->unscheduleGC();
  }
}

class js::gc::AutoCallGCCallbacks {
  GCRuntime& gc_;
  JS::GCReason reason_;

 public:
  explicit AutoCallGCCallbacks(GCRuntime& gc, JS::GCReason reason)
      : gc_(gc), reason_(reason) {
    gc_.maybeCallGCCallback(JSGC_BEGIN, reason);
  }
  ~AutoCallGCCallbacks() { gc_.maybeCallGCCallback(JSGC_END, reason_); }
};

void GCRuntime::maybeCallGCCallback(JSGCStatus status, JS::GCReason reason) {
  if (!gcCallback.ref().op) {
    return;
  }

  if (isIncrementalGCInProgress()) {
    return;
  }

  if (gcCallbackDepth == 0) {
    // Save scheduled zone information in case the callback clears it.
    for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
      zone->gcScheduledSaved_ = zone->gcScheduled_;
    }
  }

  gcCallbackDepth++;

  callGCCallback(status, reason);

  MOZ_ASSERT(gcCallbackDepth != 0);
  gcCallbackDepth--;

  if (gcCallbackDepth == 0) {
    // Ensure any zone that was originally scheduled stays scheduled.
    for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
      zone->gcScheduled_ = zone->gcScheduled_ || zone->gcScheduledSaved_;
    }
  }
}

/*
 * We disable inlining to ensure that the bottom of the stack with possible GC
 * roots recorded in MarkRuntime excludes any pointers we use during the marking
 * implementation.
 */
MOZ_NEVER_INLINE GCRuntime::IncrementalResult GCRuntime::gcCycle(
    bool nonincrementalByAPI, SliceBudget budget,
    const MaybeInvocationKind& gckind, JS::GCReason reason) {
  // Assert if this is a GC unsafe region.
  rt->mainContextFromOwnThread()->verifyIsSafeToGC();

  // It's ok if threads other than the main thread have suppressGC set, as
  // they are operating on zones which will not be collected from here.
  MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

  // Note that GC callbacks are allowed to re-enter GC.
  AutoCallGCCallbacks callCallbacks(*this, reason);

  // Increase slice budget for long running collections before it is recorded by
  // AutoGCSlice.
  maybeIncreaseSliceBudget(budget);

  ScheduleZones(this);
  gcstats::AutoGCSlice agc(stats(), scanZonesBeforeGC(),
                           gckind.valueOr(invocationKind), budget, reason);

  auto result = budgetIncrementalGC(nonincrementalByAPI, reason, budget);
  if (result == IncrementalResult::ResetIncremental) {
    reason = JS::GCReason::RESET;
  }

  if (shouldCollectNurseryForSlice(nonincrementalByAPI, budget)) {
    minorGC(reason, gcstats::PhaseKind::EVICT_NURSERY_FOR_MAJOR_GC);
  } else {
    ++number;  // This otherwise happens in minorGC().
  }

  AutoGCSession session(this, JS::HeapState::MajorCollecting);

  majorGCTriggerReason = JS::GCReason::NO_REASON;

  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);

    // Background finalization and decommit are finished by defininition
    // before we can start a new GC session.
    if (!isIncrementalGCInProgress()) {
      assertBackgroundSweepingFinished();
      MOZ_ASSERT(decommitTask.isIdle());
    }

    // We must also wait for background allocation to finish so we can
    // avoid taking the GC lock when manipulating the chunks during the GC.
    // The background alloc task can run between slices, so we must wait
    // for it at the start of every slice.
    allocTask.cancelAndWait();
  }

  // We don't allow off-thread parsing to start while we're doing an
  // incremental GC of the atoms zone.
  if (rt->activeGCInAtomsZone()) {
    session.maybeCheckAtomsAccess.emplace(rt);
  }

  gcTracer.traceMajorGCStart();

  incrementalSlice(budget, gckind, reason, session);

#ifdef JS_GC_ZEAL
  clearSelectedForMarking();
#endif

  gcTracer.traceMajorGCEnd();

  MOZ_ASSERT_IF(result == IncrementalResult::ResetIncremental,
                !isIncrementalGCInProgress());

  return result;
}

bool GCRuntime::shouldCollectNurseryForSlice(bool nonincrementalByAPI,
                                             SliceBudget& budget) {
  if (!nursery().isEnabled()) {
    return false;
  }

  switch (incrementalState) {
    case State::NotActive:
    case State::Sweep:
    case State::Compact:
      return true;
    case State::Mark:
    case State::Finalize:
    case State::Decommit:
      return (nonincrementalByAPI || budget.isUnlimited() || lastMarkSlice ||
              nursery().shouldCollect() || hasIncrementalTwoSliceZealMode());
    case State::Finish:
      return false;
    case State::MarkRoots:
      MOZ_CRASH("Unexpected GC state");
  }

  return false;
}

#ifdef JS_GC_ZEAL
static bool IsDeterministicGCReason(JS::GCReason reason) {
  switch (reason) {
    case JS::GCReason::API:
    case JS::GCReason::DESTROY_RUNTIME:
    case JS::GCReason::LAST_DITCH:
    case JS::GCReason::TOO_MUCH_MALLOC:
    case JS::GCReason::TOO_MUCH_WASM_MEMORY:
    case JS::GCReason::TOO_MUCH_JIT_CODE:
    case JS::GCReason::ALLOC_TRIGGER:
    case JS::GCReason::DEBUG_GC:
    case JS::GCReason::CC_FORCED:
    case JS::GCReason::SHUTDOWN_CC:
    case JS::GCReason::ABORT_GC:
    case JS::GCReason::DISABLE_GENERATIONAL_GC:
    case JS::GCReason::FINISH_GC:
    case JS::GCReason::PREPARE_FOR_TRACING:
      return true;

    default:
      return false;
  }
}
#endif

gcstats::ZoneGCStats GCRuntime::scanZonesBeforeGC() {
  gcstats::ZoneGCStats zoneStats;
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    zoneStats.zoneCount++;
    zoneStats.compartmentCount += zone->compartments().length();
    if (zone->canCollect()) {
      zoneStats.collectableZoneCount++;
      if (zone->isGCScheduled()) {
        zoneStats.collectedZoneCount++;
        zoneStats.collectedCompartmentCount += zone->compartments().length();
      }
    }
  }

  return zoneStats;
}

// The GC can only clean up scheduledForDestruction realms that were marked live
// by a barrier (e.g. by RemapWrappers from a navigation event). It is also
// common to have realms held live because they are part of a cycle in gecko,
// e.g. involving the HTMLDocument wrapper. In this case, we need to run the
// CycleCollector in order to remove these edges before the realm can be freed.
void GCRuntime::maybeDoCycleCollection() {
  const static float ExcessiveGrayRealms = 0.8f;
  const static size_t LimitGrayRealms = 200;

  size_t realmsTotal = 0;
  size_t realmsGray = 0;
  for (RealmsIter realm(rt); !realm.done(); realm.next()) {
    ++realmsTotal;
    GlobalObject* global = realm->unsafeUnbarrieredMaybeGlobal();
    if (global && global->isMarkedGray()) {
      ++realmsGray;
    }
  }
  float grayFraction = float(realmsGray) / float(realmsTotal);
  if (grayFraction > ExcessiveGrayRealms || realmsGray > LimitGrayRealms) {
    callDoCycleCollectionCallback(rt->mainContextFromOwnThread());
  }
}

void GCRuntime::checkCanCallAPI() {
  MOZ_RELEASE_ASSERT(CurrentThreadCanAccessRuntime(rt));

  /* If we attempt to invoke the GC while we are running in the GC, assert. */
  MOZ_RELEASE_ASSERT(!JS::RuntimeHeapIsBusy());
}

bool GCRuntime::checkIfGCAllowedInCurrentState(JS::GCReason reason) {
  if (rt->mainContextFromOwnThread()->suppressGC) {
    return false;
  }

  // Only allow shutdown GCs when we're destroying the runtime. This keeps
  // the GC callback from triggering a nested GC and resetting global state.
  if (rt->isBeingDestroyed() && !IsShutdownGC(reason)) {
    return false;
  }

#ifdef JS_GC_ZEAL
  if (deterministicOnly && !IsDeterministicGCReason(reason)) {
    return false;
  }
#endif

  return true;
}

bool GCRuntime::shouldRepeatForDeadZone(JS::GCReason reason) {
  MOZ_ASSERT_IF(reason == JS::GCReason::COMPARTMENT_REVIVED, !isIncremental);
  MOZ_ASSERT(!isIncrementalGCInProgress());

  if (!isIncremental) {
    return false;
  }

  for (CompartmentsIter c(rt); !c.done(); c.next()) {
    if (c->gcState.scheduledForDestruction) {
      return true;
    }
  }

  return false;
}

void GCRuntime::collect(bool nonincrementalByAPI, SliceBudget budget,
                        const MaybeInvocationKind& gckindArg,
                        JS::GCReason reason) {
  MOZ_ASSERT(reason != JS::GCReason::NO_REASON);

  MaybeInvocationKind gckind = gckindArg;
  MOZ_ASSERT_IF(!isIncrementalGCInProgress(), gckind.isSome());

  // Checks run for each request, even if we do not actually GC.
  checkCanCallAPI();

  // Check if we are allowed to GC at this time before proceeding.
  if (!checkIfGCAllowedInCurrentState(reason)) {
    return;
  }

  stats().writeLogMessage("GC starting in state %s",
                          StateName(incrementalState));

  AutoTraceLog logGC(TraceLoggerForCurrentThread(), TraceLogger_GC);
  AutoStopVerifyingBarriers av(rt, IsShutdownGC(reason));
  AutoEnqueuePendingParseTasksAfterGC aept(*this);
  AutoMaybeLeaveAtomsZone leaveAtomsZone(rt->mainContextFromOwnThread());

#ifdef DEBUG
  if (IsShutdownGC(reason)) {
    marker.markQueue.clear();
    marker.queuePos = 0;
  }
#endif

  bool repeat;
  do {
    IncrementalResult cycleResult =
        gcCycle(nonincrementalByAPI, budget, gckind, reason);

    if (reason == JS::GCReason::ABORT_GC) {
      MOZ_ASSERT(!isIncrementalGCInProgress());
      stats().writeLogMessage("GC aborted by request");
      break;
    }

    /*
     * Sometimes when we finish a GC we need to immediately start a new one.
     * This happens in the following cases:
     *  - when we reset the current GC
     *  - when finalizers drop roots during shutdown
     *  - when zones that we thought were dead at the start of GC are
     *    not collected (see the large comment in beginMarkPhase)
     */
    repeat = false;
    if (!isIncrementalGCInProgress()) {
      if (cycleResult == ResetIncremental) {
        repeat = true;
      } else if (rootsRemoved && IsShutdownGC(reason)) {
        /* Need to re-schedule all zones for GC. */
        JS::PrepareForFullGC(rt->mainContextFromOwnThread());
        repeat = true;
        reason = JS::GCReason::ROOTS_REMOVED;
      } else if (shouldRepeatForDeadZone(reason)) {
        repeat = true;
        reason = JS::GCReason::COMPARTMENT_REVIVED;
      }
    }

    if (repeat) {
      gckind = Some(invocationKind);
    }
  } while (repeat);

  if (reason == JS::GCReason::COMPARTMENT_REVIVED) {
    maybeDoCycleCollection();
  }

#ifdef JS_GC_ZEAL
  if (hasZealMode(ZealMode::CheckHeapAfterGC)) {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::TRACE_HEAP);
    CheckHeapAfterGC(rt);
  }
  if (hasZealMode(ZealMode::CheckGrayMarking) && !isIncrementalGCInProgress()) {
    MOZ_RELEASE_ASSERT(CheckGrayMarkingState(rt));
  }
#endif
  stats().writeLogMessage("GC ending in state %s", StateName(incrementalState));

  UnscheduleZones(this);
}

js::AutoEnqueuePendingParseTasksAfterGC::
    ~AutoEnqueuePendingParseTasksAfterGC() {
  if (!OffThreadParsingMustWaitForGC(gc_.rt)) {
    EnqueuePendingParseTasksAfterGC(gc_.rt);
  }
}

SliceBudget GCRuntime::defaultBudget(JS::GCReason reason, int64_t millis) {
  if (millis == 0) {
    if (reason == JS::GCReason::ALLOC_TRIGGER) {
      millis = defaultSliceBudgetMS();
    } else if (schedulingState.inHighFrequencyGCMode() &&
               tunables.isDynamicMarkSliceEnabled()) {
      millis = defaultSliceBudgetMS() * IGC_MARK_SLICE_MULTIPLIER;
    } else {
      millis = defaultSliceBudgetMS();
    }
  }

  return SliceBudget(TimeBudget(millis));
}

void GCRuntime::gc(JSGCInvocationKind gckind, JS::GCReason reason) {
  collect(true, SliceBudget::unlimited(), mozilla::Some(gckind), reason);
}

void GCRuntime::startGC(JSGCInvocationKind gckind, JS::GCReason reason,
                        int64_t millis) {
  MOZ_ASSERT(!isIncrementalGCInProgress());
  if (!JS::IsIncrementalGCEnabled(rt->mainContextFromOwnThread())) {
    gc(gckind, reason);
    return;
  }
  collect(false, defaultBudget(reason, millis), Some(gckind), reason);
}

void GCRuntime::gcSlice(JS::GCReason reason, int64_t millis) {
  MOZ_ASSERT(isIncrementalGCInProgress());
  collect(false, defaultBudget(reason, millis), Nothing(), reason);
}

void GCRuntime::finishGC(JS::GCReason reason) {
  MOZ_ASSERT(isIncrementalGCInProgress());

  // If we're not collecting because we're out of memory then skip the
  // compacting phase if we need to finish an ongoing incremental GC
  // non-incrementally to avoid janking the browser.
  if (!IsOOMReason(initialReason)) {
    if (incrementalState == State::Compact) {
      abortGC();
      return;
    }

    isCompacting = false;
  }

  collect(false, SliceBudget::unlimited(), Nothing(), reason);
}

void GCRuntime::abortGC() {
  MOZ_ASSERT(isIncrementalGCInProgress());
  checkCanCallAPI();
  MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

  collect(false, SliceBudget::unlimited(), Nothing(), JS::GCReason::ABORT_GC);
}

static bool ZonesSelected(GCRuntime* gc) {
  for (ZonesIter zone(gc, WithAtoms); !zone.done(); zone.next()) {
    if (zone->isGCScheduled()) {
      return true;
    }
  }
  return false;
}

void GCRuntime::startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget) {
  MOZ_ASSERT(!isIncrementalGCInProgress());
  if (!ZonesSelected(this)) {
    JS::PrepareForFullGC(rt->mainContextFromOwnThread());
  }
  collect(false, budget, Some(gckind), JS::GCReason::DEBUG_GC);
}

void GCRuntime::debugGCSlice(SliceBudget& budget) {
  MOZ_ASSERT(isIncrementalGCInProgress());
  if (!ZonesSelected(this)) {
    JS::PrepareForIncrementalGC(rt->mainContextFromOwnThread());
  }
  collect(false, budget, Nothing(), JS::GCReason::DEBUG_GC);
}

/* Schedule a full GC unless a zone will already be collected. */
void js::PrepareForDebugGC(JSRuntime* rt) {
  if (!ZonesSelected(&rt->gc)) {
    JS::PrepareForFullGC(rt->mainContextFromOwnThread());
  }
}

void GCRuntime::onOutOfMallocMemory() {
  // Stop allocating new chunks.
  allocTask.cancelAndWait();

  // Make sure we release anything queued for release.
  decommitTask.join();
  nursery().joinDecommitTask();

  // Wait for background free of nursery huge slots to finish.
  sweepTask.join();

  AutoLockGC lock(this);
  onOutOfMallocMemory(lock);
}

void GCRuntime::onOutOfMallocMemory(const AutoLockGC& lock) {
  // Release any relocated arenas we may be holding on to, without releasing
  // the GC lock.
  releaseHeldRelocatedArenasWithoutUnlocking(lock);

  // Throw away any excess chunks we have lying around.
  freeEmptyChunks(lock);

  // Immediately decommit as many arenas as possible in the hopes that this
  // might let the OS scrape together enough pages to satisfy the failing
  // malloc request.
  decommitFreeArenasWithoutUnlocking(lock);
}

void GCRuntime::minorGC(JS::GCReason reason, gcstats::PhaseKind phase) {
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());

  MOZ_ASSERT_IF(reason == JS::GCReason::EVICT_NURSERY,
                !rt->mainContextFromOwnThread()->suppressGC);
  if (rt->mainContextFromOwnThread()->suppressGC) {
    return;
  }

  AutoMaybeLeaveAtomsZone leaveAtomsZone(rt->mainContextFromOwnThread());

  // Note that we aren't collecting the updated alloc counts from any helper
  // threads.  We should be but I'm not sure where to add that
  // synchronisation.
  uint32_t numAllocs =
      rt->mainContextFromOwnThread()->getAndResetAllocsThisZoneSinceMinorGC();
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    numAllocs += zone->getAndResetTenuredAllocsSinceMinorGC();
  }
  stats().setAllocsSinceMinorGCTenured(numAllocs);

  gcstats::AutoPhase ap(stats(), phase);

  nursery().clearMinorGCRequest();
  TraceLoggerThread* logger = TraceLoggerForCurrentThread();
  AutoTraceLog logMinorGC(logger, TraceLogger_MinorGC);
  nursery().collect(reason);
  MOZ_ASSERT(nursery().isEmpty());

  startBackgroundFreeAfterMinorGC();

#ifdef JS_GC_ZEAL
  if (hasZealMode(ZealMode::CheckHeapAfterGC)) {
    CheckHeapAfterGC(rt);
  }
#endif

  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    maybeAllocTriggerZoneGC(zone);
    maybeMallocTriggerZoneGC(zone);
  }
}

void GCRuntime::startBackgroundFreeAfterMinorGC() {
  MOZ_ASSERT(nursery().isEmpty());

  {
    AutoLockHelperThreadState lock;

    lifoBlocksToFree.ref().transferFrom(&lifoBlocksToFreeAfterMinorGC.ref());

    if (lifoBlocksToFree.ref().isEmpty() &&
        buffersToFreeAfterMinorGC.ref().empty()) {
      return;
    }
  }

  startBackgroundFree();
}

JS::AutoDisableGenerationalGC::AutoDisableGenerationalGC(JSContext* cx)
    : cx(cx) {
  if (!cx->generationalDisabled) {
    cx->runtime()->gc.evictNursery(JS::GCReason::DISABLE_GENERATIONAL_GC);
    cx->nursery().disable();
  }
  ++cx->generationalDisabled;
}

JS::AutoDisableGenerationalGC::~AutoDisableGenerationalGC() {
  if (--cx->generationalDisabled == 0 &&
      cx->runtime()->gc.tunables.gcMaxNurseryBytes() > 0) {
    cx->nursery().enable();
  }
}

JS_PUBLIC_API bool JS::IsGenerationalGCEnabled(JSRuntime* rt) {
  return !rt->mainContextFromOwnThread()->generationalDisabled;
}

bool GCRuntime::gcIfRequested() {
  // This method returns whether a major GC was performed.

  if (nursery().minorGCRequested()) {
    minorGC(nursery().minorGCTriggerReason());
  }

  if (majorGCRequested()) {
    if (majorGCTriggerReason == JS::GCReason::DELAYED_ATOMS_GC &&
        !rt->mainContextFromOwnThread()->canCollectAtoms()) {
      // A GC was requested to collect the atoms zone, but it's no longer
      // possible. Skip this collection.
      majorGCTriggerReason = JS::GCReason::NO_REASON;
      return false;
    }

    if (!isIncrementalGCInProgress()) {
      startGC(GC_NORMAL, majorGCTriggerReason);
    } else {
      gcSlice(majorGCTriggerReason);
    }
    return true;
  }

  return false;
}

void js::gc::FinishGC(JSContext* cx, JS::GCReason reason) {
  // Calling this when GC is suppressed won't have any effect.
  MOZ_ASSERT(!cx->suppressGC);

  // GC callbacks may run arbitrary code, including JS. Check this regardless of
  // whether we GC for this invocation.
  MOZ_ASSERT(cx->isNurseryAllocAllowed());

  if (JS::IsIncrementalGCInProgress(cx)) {
    JS::PrepareForIncrementalGC(cx);
    JS::FinishIncrementalGC(cx, reason);
  }

  cx->runtime()->gc.waitBackgroundFreeEnd();
}

Realm* js::NewRealm(JSContext* cx, JSPrincipals* principals,
                    const JS::RealmOptions& options) {
  JSRuntime* rt = cx->runtime();
  JS_AbortIfWrongThread(cx);

  UniquePtr<Zone> zoneHolder;
  UniquePtr<Compartment> compHolder;

  Compartment* comp = nullptr;
  Zone* zone = nullptr;
  JS::CompartmentSpecifier compSpec =
      options.creationOptions().compartmentSpecifier();
  switch (compSpec) {
    case JS::CompartmentSpecifier::NewCompartmentInSystemZone:
      // systemZone might be null here, in which case we'll make a zone and
      // set this field below.
      zone = rt->gc.systemZone;
      break;
    case JS::CompartmentSpecifier::NewCompartmentInExistingZone:
      zone = options.creationOptions().zone();
      MOZ_ASSERT(zone);
      break;
    case JS::CompartmentSpecifier::ExistingCompartment:
      comp = options.creationOptions().compartment();
      zone = comp->zone();
      break;
    case JS::CompartmentSpecifier::NewCompartmentAndZone:
      break;
  }

  if (!zone) {
    zoneHolder = MakeUnique<Zone>(cx->runtime());
    if (!zoneHolder || !zoneHolder->init()) {
      ReportOutOfMemory(cx);
      return nullptr;
    }

    const JSPrincipals* trusted = rt->trustedPrincipals();
    bool isSystem = principals && principals == trusted;
    if (isSystem) {
      zoneHolder->setIsSystemZone();
    }

    zone = zoneHolder.get();
  }

  bool invisibleToDebugger = options.creationOptions().invisibleToDebugger();
  if (comp) {
    // Debugger visibility is per-compartment, not per-realm, so make sure the
    // new realm's visibility matches its compartment's.
    MOZ_ASSERT(comp->invisibleToDebugger() == invisibleToDebugger);
  } else {
    compHolder = cx->make_unique<JS::Compartment>(zone, invisibleToDebugger);
    if (!compHolder) {
      return nullptr;
    }

    comp = compHolder.get();
  }

  UniquePtr<Realm> realm(cx->new_<Realm>(comp, options));
  if (!realm || !realm->init(cx, principals)) {
    return nullptr;
  }

  // Make sure we don't put system and non-system realms in the same
  // compartment.
  if (!compHolder) {
    MOZ_RELEASE_ASSERT(realm->isSystem() == IsSystemCompartment(comp));
  }

  AutoLockGC lock(rt);

  // Reserve space in the Vectors before we start mutating them.
  if (!comp->realms().reserve(comp->realms().length() + 1) ||
      (compHolder &&
       !zone->compartments().reserve(zone->compartments().length() + 1)) ||
      (zoneHolder && !rt->gc.zones().reserve(rt->gc.zones().length() + 1))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // After this everything must be infallible.

  comp->realms().infallibleAppend(realm.get());

  if (compHolder) {
    zone->compartments().infallibleAppend(compHolder.release());
  }

  if (zoneHolder) {
    rt->gc.zones().infallibleAppend(zoneHolder.release());

    // Lazily set the runtime's system zone.
    if (compSpec == JS::CompartmentSpecifier::NewCompartmentInSystemZone) {
      MOZ_RELEASE_ASSERT(!rt->gc.systemZone);
      rt->gc.systemZone = zone;
      zone->setIsSystemZone();
    }
  }

  return realm.release();
}

void gc::MergeRealms(Realm* source, Realm* target) {
  JSRuntime* rt = source->runtimeFromMainThread();
  rt->gc.mergeRealms(source, target);
  rt->gc.maybeAllocTriggerZoneGC(target->zone());
  rt->gc.maybeMallocTriggerZoneGC(target->zone());
}

void GCRuntime::mergeRealms(Realm* source, Realm* target) {
  // The source realm must be specifically flagged as mergable.  This
  // also implies that the realm is not visible to the debugger.
  MOZ_ASSERT(source->creationOptions().mergeable());
  MOZ_ASSERT(source->creationOptions().invisibleToDebugger());

  MOZ_ASSERT(!source->hasBeenEnteredIgnoringJit());
  MOZ_ASSERT(source->zone()->compartments().length() == 1);

  JSContext* cx = rt->mainContextFromOwnThread();

  MOZ_ASSERT(!source->zone()->wasGCStarted());
  JS::AutoAssertNoGC nogc(cx);

  AutoTraceSession session(rt);

  // Cleanup tables and other state in the source realm/zone that will be
  // meaningless after merging into the target realm/zone.

  source->clearTables();
  source->zone()->clearTables();
  source->unsetIsDebuggee();

  // The delazification flag indicates the presence of lazy scripts in a realm
  // for the Debugger API, so if the source realm created lazy scripts, the flag
  // must be propagated to the target realm.
  if (source->needsDelazificationForDebugger()) {
    target->scheduleDelazificationForDebugger();
  }

  // Release any relocated arenas which we may be holding on to as they might
  // be in the source zone
  releaseHeldRelocatedArenas();

  // Fixup realm pointers in source to refer to target, and make sure
  // type information generations are in sync.

  GlobalObject* global = target->maybeGlobal();
  MOZ_ASSERT(global);
  AssertTargetIsNotGray(global);

  for (auto group = source->zone()->cellIterUnsafe<ObjectGroup>();
       !group.done(); group.next()) {
    // Replace placeholder object prototypes with the correct prototype in
    // the target realm.
    TaggedProto proto(group->proto());
    if (proto.isObject()) {
      JSObject* obj = proto.toObject();
      if (GlobalObject::isOffThreadPrototypePlaceholder(obj)) {
        JSObject* targetProto =
            global->getPrototypeForOffThreadPlaceholder(obj);
        MOZ_ASSERT(targetProto->isDelegate());
        MOZ_ASSERT_IF(targetProto->staticPrototypeIsImmutable(),
                      obj->staticPrototypeIsImmutable());
        MOZ_ASSERT_IF(targetProto->isNewGroupUnknown(),
                      obj->isNewGroupUnknown());
        group->setProtoUnchecked(TaggedProto(targetProto));
      }
    }

    group->setGeneration(target->zone()->types.generation);
    group->realm_ = target;
  }

  // Fixup zone pointers in source's zone to refer to target's zone.

  bool targetZoneIsCollecting =
      isIncrementalGCInProgress() && target->zone()->wasGCStarted();
  for (auto thingKind : AllAllocKinds()) {
    for (ArenaIter aiter(source->zone(), thingKind); !aiter.done();
         aiter.next()) {
      Arena* arena = aiter.get();
      arena->zone = target->zone();
      if (MOZ_UNLIKELY(targetZoneIsCollecting)) {
        // If we are currently collecting the target zone then we must
        // treat all merged things as if they were allocated during the
        // collection.
        for (ArenaCellIter iter(arena); !iter.done(); iter.next()) {
          TenuredCell* cell = iter.getCell();
          MOZ_ASSERT(!cell->isMarkedAny());
          cell->markBlack();
        }
      }
    }
  }

  // The source should be the only realm in its zone.
  for (RealmsInZoneIter r(source->zone()); !r.done(); r.next()) {
    MOZ_ASSERT(r.get() == source);
  }

  // Merge the allocator, stats and UIDs in source's zone into target's zone.
  target->zone()->arenas.adoptArenas(&source->zone()->arenas,
                                     targetZoneIsCollecting);
  target->zone()->addTenuredAllocsSinceMinorGC(
      source->zone()->getAndResetTenuredAllocsSinceMinorGC());
  target->zone()->gcHeapSize.adopt(source->zone()->gcHeapSize);
  target->zone()->adoptUniqueIds(source->zone());
  target->zone()->adoptMallocBytes(source->zone());

  // Merge other info in source's zone into target's zone.
  target->zone()->types.typeLifoAlloc().transferFrom(
      &source->zone()->types.typeLifoAlloc());
  MOZ_RELEASE_ASSERT(source->zone()->types.sweepTypeLifoAlloc.ref().isEmpty());

  // Atoms which are marked in source's zone are now marked in target's zone.
  atomMarking.adoptMarkedAtoms(target->zone(), source->zone());

  // The source Realm is a parse-only realm and should not have collected any
  // zone-tracked metadata.
  Zone* sourceZone = source->zone();
  MOZ_ASSERT(!sourceZone->scriptLCovMap);
  MOZ_ASSERT(!sourceZone->scriptCountsMap);
  MOZ_ASSERT(!sourceZone->debugScriptMap);
#ifdef MOZ_VTUNE
  MOZ_ASSERT(!sourceZone->scriptVTuneIdMap);
#endif

  // The source realm is now completely empty, and is the only realm in its
  // compartment, which is the only compartment in its zone. Delete realm,
  // compartment and zone without waiting for this to be cleaned up by a full
  // GC.

  sourceZone->deleteEmptyCompartment(source->compartment());
  deleteEmptyZone(sourceZone);
}

void GCRuntime::runDebugGC() {
#ifdef JS_GC_ZEAL
  if (rt->mainContextFromOwnThread()->suppressGC) {
    return;
  }

  if (hasZealMode(ZealMode::GenerationalGC)) {
    return minorGC(JS::GCReason::DEBUG_GC);
  }

  PrepareForDebugGC(rt);

  auto budget = SliceBudget::unlimited();
  if (hasZealMode(ZealMode::IncrementalMultipleSlices)) {
    /*
     * Start with a small slice limit and double it every slice. This
     * ensure that we get multiple slices, and collection runs to
     * completion.
     */
    if (!isIncrementalGCInProgress()) {
      incrementalLimit = zealFrequency / 2;
    } else {
      incrementalLimit *= 2;
    }
    budget = SliceBudget(WorkBudget(incrementalLimit));

    js::gc::State initialState = incrementalState;
    Maybe<JSGCInvocationKind> gckind =
        isIncrementalGCInProgress() ? Nothing() : Some(GC_SHRINK);
    collect(false, budget, gckind, JS::GCReason::DEBUG_GC);

    /* Reset the slice size when we get to the sweep or compact phases. */
    if ((initialState == State::Mark && incrementalState == State::Sweep) ||
        (initialState == State::Sweep && incrementalState == State::Compact)) {
      incrementalLimit = zealFrequency / 2;
    }
  } else if (hasIncrementalTwoSliceZealMode()) {
    // These modes trigger incremental GC that happens in two slices and the
    // supplied budget is ignored by incrementalSlice.
    budget = SliceBudget(WorkBudget(1));

    Maybe<JSGCInvocationKind> gckind =
        isIncrementalGCInProgress() ? Nothing() : Some(GC_NORMAL);
    collect(false, budget, gckind, JS::GCReason::DEBUG_GC);
  } else if (hasZealMode(ZealMode::Compact)) {
    gc(GC_SHRINK, JS::GCReason::DEBUG_GC);
  } else {
    gc(GC_NORMAL, JS::GCReason::DEBUG_GC);
  }

#endif
}

void GCRuntime::setFullCompartmentChecks(bool enabled) {
  MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
  fullCompartmentChecks = enabled;
}

void GCRuntime::notifyRootsRemoved() {
  rootsRemoved = true;

#ifdef JS_GC_ZEAL
  /* Schedule a GC to happen "soon". */
  if (hasZealMode(ZealMode::RootsChange)) {
    nextScheduled = 1;
  }
#endif
}

#ifdef JS_GC_ZEAL
bool GCRuntime::selectForMarking(JSObject* object) {
  MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
  return selectedForMarking.ref().get().append(object);
}

void GCRuntime::clearSelectedForMarking() {
  selectedForMarking.ref().get().clearAndFree();
}

void GCRuntime::setDeterministic(bool enabled) {
  MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
  deterministicOnly = enabled;
}
#endif

#ifdef DEBUG

/* Should only be called manually under gdb */
void PreventGCDuringInteractiveDebug() { TlsContext.get()->suppressGC++; }

#endif

void js::ReleaseAllJITCode(JSFreeOp* fop) {
  js::CancelOffThreadIonCompile(fop->runtime());

  for (ZonesIter zone(fop->runtime(), SkipAtoms); !zone.done(); zone.next()) {
    zone->setPreservingCode(false);
    zone->discardJitCode(fop);
  }

  for (RealmsIter realm(fop->runtime()); !realm.done(); realm.next()) {
    if (jit::JitRealm* jitRealm = realm->jitRealm()) {
      jitRealm->discardStubs();
    }
  }
}

void ArenaLists::adoptArenas(ArenaLists* fromArenaLists,
                             bool targetZoneIsCollecting) {
  // GC may be active so take the lock here so we can mutate the arena lists.
  AutoLockGC lock(runtime());

  fromArenaLists->clearFreeLists();

  for (auto thingKind : AllAllocKinds()) {
    MOZ_ASSERT(fromArenaLists->concurrentUse(thingKind) == ConcurrentUse::None);
    ArenaList* fromList = &fromArenaLists->arenaLists(thingKind);
    ArenaList* toList = &arenaLists(thingKind);
    fromList->check();
    toList->check();
    Arena* next;
    for (Arena* fromArena = fromList->head(); fromArena; fromArena = next) {
      // Copy fromArena->next before releasing/reinserting.
      next = fromArena->next;

      MOZ_ASSERT(!fromArena->isEmpty());

      // If the target zone is being collected then we need to add the
      // arenas before the cursor because the collector assumes that the
      // cursor is always at the end of the list. This has the side-effect
      // of preventing allocation into any non-full arenas until the end
      // of the next GC.
      if (targetZoneIsCollecting) {
        toList->insertBeforeCursor(fromArena);
      } else {
        toList->insertAtCursor(fromArena);
      }
    }
    fromList->clear();
    toList->check();
  }
}

AutoSuppressGC::AutoSuppressGC(JSContext* cx)
    : suppressGC_(cx->suppressGC.ref()) {
  suppressGC_++;
}

#ifdef DEBUG
AutoDisableProxyCheck::AutoDisableProxyCheck() {
  TlsContext.get()->disableStrictProxyChecking();
}

AutoDisableProxyCheck::~AutoDisableProxyCheck() {
  TlsContext.get()->enableStrictProxyChecking();
}

JS_FRIEND_API void JS::AssertGCThingMustBeTenured(JSObject* obj) {
  MOZ_ASSERT(obj->isTenured() &&
             (!IsNurseryAllocable(obj->asTenured().getAllocKind()) ||
              obj->getClass()->hasFinalize()));
}

JS_FRIEND_API void JS::AssertGCThingIsNotNurseryAllocable(Cell* cell) {
  MOZ_ASSERT(cell);
  MOZ_ASSERT(!cell->is<JSObject>() && !cell->is<JSString>() &&
             !cell->is<JS::BigInt>());
}

JS_FRIEND_API void js::gc::AssertGCThingHasType(js::gc::Cell* cell,
                                                JS::TraceKind kind) {
  if (!cell) {
    MOZ_ASSERT(kind == JS::TraceKind::Null);
    return;
  }

  MOZ_ASSERT(IsCellPointerValid(cell));

  if (IsInsideNursery(cell)) {
    MOZ_ASSERT(kind == (cell->nurseryCellIsString()
                            ? JS::TraceKind::String
                            : cell->nurseryCellIsBigInt()
                                  ? JS::TraceKind::BigInt
                                  : JS::TraceKind::Object));
    return;
  }

  MOZ_ASSERT(MapAllocToTraceKind(cell->asTenured().getAllocKind()) == kind);
}
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

JS::AutoAssertNoGC::AutoAssertNoGC(JSContext* maybecx)
    : cx_(maybecx ? maybecx : TlsContext.get()) {
  if (cx_) {
    cx_->inUnsafeRegion++;
  }
}

JS::AutoAssertNoGC::~AutoAssertNoGC() {
  if (cx_) {
    MOZ_ASSERT(cx_->inUnsafeRegion > 0);
    cx_->inUnsafeRegion--;
  }
}

#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED

#ifdef DEBUG

AutoAssertNoNurseryAlloc::AutoAssertNoNurseryAlloc() {
  TlsContext.get()->disallowNurseryAlloc();
}

AutoAssertNoNurseryAlloc::~AutoAssertNoNurseryAlloc() {
  TlsContext.get()->allowNurseryAlloc();
}

JS::AutoEnterCycleCollection::AutoEnterCycleCollection(JSRuntime* rt)
    : runtime_(rt) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
  runtime_->gc.heapState_ = HeapState::CycleCollecting;
}

JS::AutoEnterCycleCollection::~AutoEnterCycleCollection() {
  MOZ_ASSERT(JS::RuntimeHeapIsCycleCollecting());
  runtime_->gc.heapState_ = HeapState::Idle;
}

JS::AutoAssertGCCallback::AutoAssertGCCallback() : AutoSuppressGCAnalysis() {
  MOZ_ASSERT(JS::RuntimeHeapIsCollecting());
}

#endif  // DEBUG

JS_FRIEND_API const char* JS::GCTraceKindToAscii(JS::TraceKind kind) {
  switch (kind) {
#define MAP_NAME(name, _0, _1, _2) \
  case JS::TraceKind::name:        \
    return "JS " #name;
    JS_FOR_EACH_TRACEKIND(MAP_NAME);
#undef MAP_NAME
    default:
      return "Invalid";
  }
}

JS_FRIEND_API size_t JS::GCTraceKindSize(JS::TraceKind kind) {
  switch (kind) {
#define MAP_SIZE(name, type, _0, _1) \
  case JS::TraceKind::name:          \
    return sizeof(type);
    JS_FOR_EACH_TRACEKIND(MAP_SIZE);
#undef MAP_SIZE
    default:
      return 0;
  }
}

JS::GCCellPtr::GCCellPtr(const Value& v) : ptr(0) {
  switch (v.type()) {
    case ValueType::String:
      ptr = checkedCast(v.toString(), JS::TraceKind::String);
      break;
    case ValueType::Object:
      ptr = checkedCast(&v.toObject(), JS::TraceKind::Object);
      break;
    case ValueType::Symbol:
      ptr = checkedCast(v.toSymbol(), JS::TraceKind::Symbol);
      break;
    case ValueType::BigInt:
      ptr = checkedCast(v.toBigInt(), JS::TraceKind::BigInt);
      break;
    case ValueType::PrivateGCThing:
      ptr = checkedCast(v.toGCThing(), v.toGCThing()->getTraceKind());
      break;
    case ValueType::Double:
    case ValueType::Int32:
    case ValueType::Boolean:
    case ValueType::Undefined:
    case ValueType::Null:
    case ValueType::Magic: {
      MOZ_ASSERT(!v.isGCThing());
      ptr = checkedCast(nullptr, JS::TraceKind::Null);
      break;
    }
  }
}

JS::TraceKind JS::GCCellPtr::outOfLineKind() const {
  MOZ_ASSERT((ptr & OutOfLineTraceKindMask) == OutOfLineTraceKindMask);
  MOZ_ASSERT(asCell()->isTenured());
  return MapAllocToTraceKind(asCell()->asTenured().getAllocKind());
}

#ifdef JSGC_HASH_TABLE_CHECKS
void GCRuntime::checkHashTablesAfterMovingGC() {
  /*
   * Check that internal hash tables no longer have any pointers to things
   * that have been moved.
   */
  rt->geckoProfiler().checkStringsMapAfterMovingGC();
  for (ZonesIter zone(this, SkipAtoms); !zone.done(); zone.next()) {
    zone->checkUniqueIdTableAfterMovingGC();
    zone->checkInitialShapesTableAfterMovingGC();
    zone->checkBaseShapeTableAfterMovingGC();
    zone->checkAllCrossCompartmentWrappersAfterMovingGC();
    zone->checkScriptMapsAfterMovingGC();

    JS::AutoCheckCannotGC nogc;
    for (auto baseShape = zone->cellIterUnsafe<BaseShape>(); !baseShape.done();
         baseShape.next()) {
      ShapeCachePtr p = baseShape->getCache(nogc);
      p.checkAfterMovingGC();
    }
  }

  for (CompartmentsIter c(this); !c.done(); c.next()) {
    for (RealmsInCompartmentIter r(c); !r.done(); r.next()) {
      r->checkObjectGroupTablesAfterMovingGC();
      r->dtoaCache.checkCacheAfterMovingGC();
      if (r->debugEnvs()) {
        r->debugEnvs()->checkHashTablesAfterMovingGC();
      }
    }
  }
}
#endif

JS_PUBLIC_API void JS::PrepareZoneForGC(Zone* zone) { zone->scheduleGC(); }

JS_PUBLIC_API void JS::PrepareForFullGC(JSContext* cx) {
  for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
    zone->scheduleGC();
  }
}

JS_PUBLIC_API void JS::PrepareForIncrementalGC(JSContext* cx) {
  if (!JS::IsIncrementalGCInProgress(cx)) {
    return;
  }

  for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
    if (zone->wasGCStarted()) {
      PrepareZoneForGC(zone);
    }
  }
}

JS_PUBLIC_API bool JS::IsGCScheduled(JSContext* cx) {
  for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
    if (zone->isGCScheduled()) {
      return true;
    }
  }

  return false;
}

JS_PUBLIC_API void JS::SkipZoneForGC(Zone* zone) { zone->unscheduleGC(); }

JS_PUBLIC_API void JS::NonIncrementalGC(JSContext* cx,
                                        JSGCInvocationKind gckind,
                                        GCReason reason) {
  MOZ_ASSERT(gckind == GC_NORMAL || gckind == GC_SHRINK);
  cx->runtime()->gc.gc(gckind, reason);
}

JS_PUBLIC_API void JS::StartIncrementalGC(JSContext* cx,
                                          JSGCInvocationKind gckind,
                                          GCReason reason, int64_t millis) {
  MOZ_ASSERT(gckind == GC_NORMAL || gckind == GC_SHRINK);
  cx->runtime()->gc.startGC(gckind, reason, millis);
}

JS_PUBLIC_API void JS::IncrementalGCSlice(JSContext* cx, GCReason reason,
                                          int64_t millis) {
  cx->runtime()->gc.gcSlice(reason, millis);
}

JS_PUBLIC_API bool JS::IncrementalGCHasForegroundWork(JSContext* cx) {
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
  CHECK_THREAD(cx);
  return cx->runtime()->gc.hasForegroundWork();
}

JS_PUBLIC_API void JS::FinishIncrementalGC(JSContext* cx, GCReason reason) {
  cx->runtime()->gc.finishGC(reason);
}

JS_PUBLIC_API void JS::AbortIncrementalGC(JSContext* cx) {
  if (IsIncrementalGCInProgress(cx)) {
    cx->runtime()->gc.abortGC();
  }
}

char16_t* JS::GCDescription::formatSliceMessage(JSContext* cx) const {
  UniqueChars cstr = cx->runtime()->gc.stats().formatCompactSliceMessage();

  size_t nchars = strlen(cstr.get());
  UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
  if (!out) {
    return nullptr;
  }
  out.get()[nchars] = 0;

  CopyAndInflateChars(out.get(), cstr.get(), nchars);
  return out.release();
}

char16_t* JS::GCDescription::formatSummaryMessage(JSContext* cx) const {
  UniqueChars cstr = cx->runtime()->gc.stats().formatCompactSummaryMessage();

  size_t nchars = strlen(cstr.get());
  UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
  if (!out) {
    return nullptr;
  }
  out.get()[nchars] = 0;

  CopyAndInflateChars(out.get(), cstr.get(), nchars);
  return out.release();
}

JS::dbg::GarbageCollectionEvent::Ptr JS::GCDescription::toGCEvent(
    JSContext* cx) const {
  return JS::dbg::GarbageCollectionEvent::Create(
      cx->runtime(), cx->runtime()->gc.stats(),
      cx->runtime()->gc.majorGCCount());
}

char16_t* JS::GCDescription::formatJSONTelemetry(JSContext* cx,
                                                 uint64_t timestamp) const {
  UniqueChars cstr = cx->runtime()->gc.stats().renderJsonMessage(
      timestamp, gcstats::Statistics::JSONUse::TELEMETRY);

  size_t nchars = strlen(cstr.get());
  UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
  if (!out) {
    return nullptr;
  }
  out.get()[nchars] = 0;

  CopyAndInflateChars(out.get(), cstr.get(), nchars);
  return out.release();
}

TimeStamp JS::GCDescription::startTime(JSContext* cx) const {
  return cx->runtime()->gc.stats().start();
}

TimeStamp JS::GCDescription::endTime(JSContext* cx) const {
  return cx->runtime()->gc.stats().end();
}

TimeStamp JS::GCDescription::lastSliceStart(JSContext* cx) const {
  return cx->runtime()->gc.stats().slices().back().start;
}

TimeStamp JS::GCDescription::lastSliceEnd(JSContext* cx) const {
  return cx->runtime()->gc.stats().slices().back().end;
}

JS::UniqueChars JS::GCDescription::sliceToJSONProfiler(JSContext* cx) const {
  size_t slices = cx->runtime()->gc.stats().slices().length();
  MOZ_ASSERT(slices > 0);
  return cx->runtime()->gc.stats().renderJsonSlice(slices - 1);
}

JS::UniqueChars JS::GCDescription::formatJSONProfiler(JSContext* cx) const {
  return cx->runtime()->gc.stats().renderJsonMessage(
      0, js::gcstats::Statistics::JSONUse::PROFILER);
}

JS_PUBLIC_API JS::UniqueChars JS::MinorGcToJSON(JSContext* cx) {
  JSRuntime* rt = cx->runtime();
  return rt->gc.stats().renderNurseryJson();
}

JS_PUBLIC_API JS::GCSliceCallback JS::SetGCSliceCallback(
    JSContext* cx, GCSliceCallback callback) {
  return cx->runtime()->gc.setSliceCallback(callback);
}

JS_PUBLIC_API JS::DoCycleCollectionCallback JS::SetDoCycleCollectionCallback(
    JSContext* cx, JS::DoCycleCollectionCallback callback) {
  return cx->runtime()->gc.setDoCycleCollectionCallback(callback);
}

JS_PUBLIC_API JS::GCNurseryCollectionCallback
JS::SetGCNurseryCollectionCallback(JSContext* cx,
                                   GCNurseryCollectionCallback callback) {
  return cx->runtime()->gc.setNurseryCollectionCallback(callback);
}

JS_PUBLIC_API void JS::SetLowMemoryState(JSContext* cx, bool newState) {
  return cx->runtime()->gc.setLowMemoryState(newState);
}

JS_PUBLIC_API void JS::DisableIncrementalGC(JSContext* cx) {
  cx->runtime()->gc.disallowIncrementalGC();
}

JS_PUBLIC_API bool JS::IsIncrementalGCEnabled(JSContext* cx) {
  return cx->runtime()->gc.isIncrementalGCEnabled();
}

JS_PUBLIC_API bool JS::IsIncrementalGCInProgress(JSContext* cx) {
  return cx->runtime()->gc.isIncrementalGCInProgress();
}

JS_PUBLIC_API bool JS::IsIncrementalGCInProgress(JSRuntime* rt) {
  return rt->gc.isIncrementalGCInProgress() &&
         !rt->gc.isVerifyPreBarriersEnabled();
}

JS_PUBLIC_API bool JS::IsIncrementalBarrierNeeded(JSContext* cx) {
  if (JS::RuntimeHeapIsBusy()) {
    return false;
  }

  auto state = cx->runtime()->gc.state();
  return state != gc::State::NotActive && state <= gc::State::Sweep;
}

JS_PUBLIC_API void JS::IncrementalPreWriteBarrier(JSObject* obj) {
  if (!obj) {
    return;
  }

  JSObject::writeBarrierPre(obj);
}

JS_PUBLIC_API void JS::IncrementalPreWriteBarrier(GCCellPtr thing) {
  if (!thing) {
    return;
  }

  TenuredCell::writeBarrierPre(&thing.asCell()->asTenured());
}

JS_PUBLIC_API bool JS::WasIncrementalGC(JSRuntime* rt) {
  return rt->gc.isIncrementalGc();
}

uint64_t js::gc::NextCellUniqueId(JSRuntime* rt) {
  return rt->gc.nextCellUniqueId();
}

namespace js {
namespace gc {
namespace MemInfo {

static bool GCBytesGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->runtime()->gc.heapSize.bytes()));
  return true;
}

static bool GCMaxBytesGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->runtime()->gc.tunables.gcMaxBytes()));
  return true;
}

static bool GCHighFreqGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(
      cx->runtime()->gc.schedulingState.inHighFrequencyGCMode());
  return true;
}

static bool GCNumberGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->runtime()->gc.gcNumber()));
  return true;
}

static bool MajorGCCountGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->runtime()->gc.majorGCCount()));
  return true;
}

static bool MinorGCCountGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->runtime()->gc.minorGCCount()));
  return true;
}

static bool GCSliceCountGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->runtime()->gc.gcSliceCount()));
  return true;
}

static bool ZoneGCBytesGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->zone()->gcHeapSize.bytes()));
  return true;
}

static bool ZoneGCTriggerBytesGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->zone()->gcHeapThreshold.bytes()));
  return true;
}

static bool ZoneGCAllocTriggerGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  bool highFrequency =
      cx->runtime()->gc.schedulingState.inHighFrequencyGCMode();
  args.rval().setNumber(
      double(cx->zone()->gcHeapThreshold.eagerAllocTrigger(highFrequency)));
  return true;
}

static bool ZoneMallocBytesGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->zone()->mallocHeapSize.bytes()));
  return true;
}

static bool ZoneMallocTriggerBytesGetter(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->zone()->mallocHeapThreshold.bytes()));
  return true;
}

static bool ZoneGCDelayBytesGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->zone()->gcDelayBytes));
  return true;
}

static bool ZoneGCNumberGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(double(cx->zone()->gcNumber()));
  return true;
}

#ifdef JS_MORE_DETERMINISTIC
static bool DummyGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}
#endif

} /* namespace MemInfo */

JSObject* NewMemoryInfoObject(JSContext* cx) {
  RootedObject obj(cx, JS_NewObject(cx, nullptr));
  if (!obj) {
    return nullptr;
  }

  using namespace MemInfo;
  struct NamedGetter {
    const char* name;
    JSNative getter;
  } getters[] = {{"gcBytes", GCBytesGetter},
                 {"gcMaxBytes", GCMaxBytesGetter},
                 {"gcIsHighFrequencyMode", GCHighFreqGetter},
                 {"gcNumber", GCNumberGetter},
                 {"majorGCCount", MajorGCCountGetter},
                 {"minorGCCount", MinorGCCountGetter},
                 {"sliceCount", GCSliceCountGetter}};

  for (auto pair : getters) {
#ifdef JS_MORE_DETERMINISTIC
    JSNative getter = DummyGetter;
#else
    JSNative getter = pair.getter;
#endif
    if (!JS_DefineProperty(cx, obj, pair.name, getter, nullptr,
                           JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }

  RootedObject zoneObj(cx, JS_NewObject(cx, nullptr));
  if (!zoneObj) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, obj, "zone", zoneObj, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  struct NamedZoneGetter {
    const char* name;
    JSNative getter;
  } zoneGetters[] = {{"gcBytes", ZoneGCBytesGetter},
                     {"gcTriggerBytes", ZoneGCTriggerBytesGetter},
                     {"gcAllocTrigger", ZoneGCAllocTriggerGetter},
                     {"mallocBytes", ZoneMallocBytesGetter},
                     {"mallocTriggerBytes", ZoneMallocTriggerBytesGetter},
                     {"delayBytes", ZoneGCDelayBytesGetter},
                     {"gcNumber", ZoneGCNumberGetter}};

  for (auto pair : zoneGetters) {
#ifdef JS_MORE_DETERMINISTIC
    JSNative getter = DummyGetter;
#else
    JSNative getter = pair.getter;
#endif
    if (!JS_DefineProperty(cx, zoneObj, pair.name, getter, nullptr,
                           JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }

  return obj;
}

const char* StateName(State state) {
  switch (state) {
#define MAKE_CASE(name) \
  case State::name:     \
    return #name;
    GCSTATES(MAKE_CASE)
#undef MAKE_CASE
  }
  MOZ_CRASH("Invalid gc::State enum value");
}

const char* StateName(JS::Zone::GCState state) {
  switch (state) {
    case JS::Zone::NoGC:
      return "NoGC";
    case JS::Zone::MarkBlackOnly:
      return "MarkBlackOnly";
    case JS::Zone::MarkBlackAndGray:
      return "MarkBlackAndGray";
    case JS::Zone::Sweep:
      return "Sweep";
    case JS::Zone::Finished:
      return "Finished";
    case JS::Zone::Compact:
      return "Compact";
  }
  MOZ_CRASH("Invalid Zone::GCState enum value");
}

void AutoAssertEmptyNursery::checkCondition(JSContext* cx) {
  if (!noAlloc) {
    noAlloc.emplace();
  }
  this->cx = cx;
  MOZ_ASSERT(cx->nursery().isEmpty());
}

AutoEmptyNursery::AutoEmptyNursery(JSContext* cx) : AutoAssertEmptyNursery() {
  MOZ_ASSERT(!cx->suppressGC);
  cx->runtime()->gc.stats().suspendPhases();
  cx->runtime()->gc.evictNursery(JS::GCReason::EVICT_NURSERY);
  cx->runtime()->gc.stats().resumePhases();
  checkCondition(cx);
}

} /* namespace gc */
} /* namespace js */

#ifdef DEBUG

namespace js {

// We don't want jsfriendapi.h to depend on GenericPrinter,
// so these functions are declared directly in the cpp.

extern JS_FRIEND_API void DumpString(JSString* str, js::GenericPrinter& out);

}  // namespace js

void js::gc::Cell::dump(js::GenericPrinter& out) const {
  switch (getTraceKind()) {
    case JS::TraceKind::Object:
      reinterpret_cast<const JSObject*>(this)->dump(out);
      break;

    case JS::TraceKind::String:
      js::DumpString(reinterpret_cast<JSString*>(const_cast<Cell*>(this)), out);
      break;

    case JS::TraceKind::Shape:
      reinterpret_cast<const Shape*>(this)->dump(out);
      break;

    default:
      out.printf("%s(%p)\n", JS::GCTraceKindToAscii(getTraceKind()),
                 (void*)this);
  }
}

// For use in a debugger.
void js::gc::Cell::dump() const {
  js::Fprinter out(stderr);
  dump(out);
}
#endif

static inline bool CanCheckGrayBits(const Cell* cell) {
  MOZ_ASSERT(cell);
  if (!cell->isTenured()) {
    return false;
  }

  auto tc = &cell->asTenured();
  auto rt = tc->runtimeFromAnyThread();
  return CurrentThreadCanAccessRuntime(rt) && rt->gc.areGrayBitsValid();
}

JS_PUBLIC_API bool js::gc::detail::CellIsMarkedGrayIfKnown(const Cell* cell) {
  // We ignore the gray marking state of cells and return false in the
  // following cases:
  //
  // 1) When OOM has caused us to clear the gcGrayBitsValid_ flag.
  //
  // 2) When we are in an incremental GC and examine a cell that is in a zone
  // that is not being collected. Gray targets of CCWs that are marked black
  // by a barrier will eventually be marked black in the next GC slice.
  //
  // 3) When we are not on the runtime's main thread. Helper threads might
  // call this while parsing, and they are not allowed to inspect the
  // runtime's incremental state. The objects being operated on are not able
  // to be collected and will not be marked any color.

  if (!CanCheckGrayBits(cell)) {
    return false;
  }

  auto tc = &cell->asTenured();
  MOZ_ASSERT(!tc->zoneFromAnyThread()->usedByHelperThread());

  auto rt = tc->runtimeFromMainThread();
  if (rt->gc.isIncrementalGCInProgress() && !tc->zone()->wasGCStarted()) {
    return false;
  }

  return detail::CellIsMarkedGray(tc);
}

#ifdef DEBUG

JS_PUBLIC_API void js::gc::detail::AssertCellIsNotGray(const Cell* cell) {
  // Check that a cell is not marked gray.
  //
  // Since this is a debug-only check, take account of the eventual mark state
  // of cells that will be marked black by the next GC slice in an incremental
  // GC. For performance reasons we don't do this in CellIsMarkedGrayIfKnown.

  if (!CanCheckGrayBits(cell)) {
    return;
  }

  // TODO: I'd like to AssertHeapIsIdle() here, but this ends up getting
  // called during GC and while iterating the heap for memory reporting.
  MOZ_ASSERT(!JS::RuntimeHeapIsCycleCollecting());

  auto tc = &cell->asTenured();
  if (tc->zone()->isGCMarkingBlackAndGray()) {
    // We are doing gray marking in the cell's zone. Even if the cell is
    // currently marked gray it may eventually be marked black. Delay checking
    // non-black cells until we finish gray marking.

    if (!tc->isMarkedBlack()) {
      JSRuntime* rt = tc->zone()->runtimeFromMainThread();
      AutoEnterOOMUnsafeRegion oomUnsafe;
      if (!rt->gc.cellsToAssertNotGray.ref().append(cell)) {
        oomUnsafe.crash("Can't append to delayed gray checks list");
      }
    }
    return;
  }

  MOZ_ASSERT(!tc->isMarkedGray());
}

extern JS_PUBLIC_API bool js::gc::detail::ObjectIsMarkedBlack(
    const JSObject* obj) {
  return obj->isMarkedBlack();
}

#endif

js::gc::ClearEdgesTracer::ClearEdgesTracer(JSRuntime* rt)
    : CallbackTracer(rt, TraceWeakMapKeysValues) {}

js::gc::ClearEdgesTracer::ClearEdgesTracer()
    : ClearEdgesTracer(TlsContext.get()->runtime()) {}

template <typename S>
inline bool js::gc::ClearEdgesTracer::clearEdge(S** thingp) {
  InternalBarrierMethods<S*>::preBarrier(*thingp);
  InternalBarrierMethods<S*>::postBarrier(thingp, *thingp, nullptr);
  *thingp = nullptr;
  return false;
}

bool js::gc::ClearEdgesTracer::onObjectEdge(JSObject** objp) {
  return clearEdge(objp);
}
bool js::gc::ClearEdgesTracer::onStringEdge(JSString** strp) {
  return clearEdge(strp);
}
bool js::gc::ClearEdgesTracer::onSymbolEdge(JS::Symbol** symp) {
  return clearEdge(symp);
}
bool js::gc::ClearEdgesTracer::onBigIntEdge(JS::BigInt** bip) {
  return clearEdge(bip);
}
bool js::gc::ClearEdgesTracer::onScriptEdge(js::BaseScript** scriptp) {
  return clearEdge(scriptp);
}
bool js::gc::ClearEdgesTracer::onShapeEdge(js::Shape** shapep) {
  return clearEdge(shapep);
}
bool js::gc::ClearEdgesTracer::onObjectGroupEdge(js::ObjectGroup** groupp) {
  return clearEdge(groupp);
}
bool js::gc::ClearEdgesTracer::onBaseShapeEdge(js::BaseShape** basep) {
  return clearEdge(basep);
}
bool js::gc::ClearEdgesTracer::onJitCodeEdge(js::jit::JitCode** codep) {
  return clearEdge(codep);
}
bool js::gc::ClearEdgesTracer::onScopeEdge(js::Scope** scopep) {
  return clearEdge(scopep);
}
bool js::gc::ClearEdgesTracer::onRegExpSharedEdge(js::RegExpShared** sharedp) {
  return clearEdge(sharedp);
}
bool js::gc::ClearEdgesTracer::onChild(const JS::GCCellPtr& thing) {
  MOZ_CRASH();
  return true;
}

JS_PUBLIC_API void js::gc::FinalizeDeadNurseryObject(JSContext* cx,
                                                     JSObject* obj) {
  CHECK_THREAD(cx);
  MOZ_ASSERT(JS::RuntimeHeapIsMinorCollecting());

  MOZ_ASSERT(obj);
  MOZ_ASSERT(IsInsideNursery(obj));
  mozilla::DebugOnly<JSObject*> prior(obj);
  MOZ_ASSERT(IsAboutToBeFinalizedUnbarriered(&prior));
  MOZ_ASSERT(obj == prior);

  const JSClass* jsClass = js::GetObjectClass(obj);
  jsClass->doFinalize(cx->defaultFreeOp(), obj);
}

JS_FRIEND_API void js::gc::SetPerformanceHint(JSContext* cx,
                                              PerformanceHint hint) {
  CHECK_THREAD(cx);
  MOZ_ASSERT(!JS::RuntimeHeapIsCollecting());

  cx->runtime()->gc.setPerformanceHint(hint);
}

void GCRuntime::setPerformanceHint(PerformanceHint hint) {
  bool wasInPageLoad = inPageLoadCount != 0;

  if (hint == PerformanceHint::InPageLoad) {
    inPageLoadCount++;
  } else {
    MOZ_ASSERT(inPageLoadCount);
    inPageLoadCount--;
  }

  bool inPageLoad = inPageLoadCount != 0;
  if (inPageLoad == wasInPageLoad) {
    return;
  }

  AutoLockGC lock(this);
  schedulingState.inPageLoad = inPageLoad;
  atomsZone->updateGCThresholds(*this, invocationKind, lock);
  maybeAllocTriggerZoneGC(atomsZone);
}
