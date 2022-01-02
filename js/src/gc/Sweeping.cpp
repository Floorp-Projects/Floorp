/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of GC sweeping.
 *
 * In the SpiderMonkey GC, 'sweeping' is used to mean two things:
 *  - updating data structures to remove pointers to dead GC things and updating
 *    pointers to moved GC things
 *  - finalizing dead GC things
 *
 * Furthermore, the GC carries out gray and weak marking after the start of the
 * sweep phase. This is also implemented in this file.
 */

#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"

#include "builtin/FinalizationRegistryObject.h"
#include "builtin/WeakRefObject.h"
#include "debugger/DebugAPI.h"
#include "gc/AllocKind.h"
#include "gc/GCInternals.h"
#include "gc/GCLock.h"
#include "gc/GCProbes.h"
#include "gc/GCRuntime.h"
#include "gc/ParallelWork.h"
#include "gc/Statistics.h"
#include "gc/WeakMap.h"
#include "gc/Zone.h"
#include "jit/JitRuntime.h"
#include "jit/JitZone.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/HelperThreads.h"
#include "vm/JSContext.h"
#include "vm/TraceLogging.h"
#include "vm/WrapperObject.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/PropMap-inl.h"
#include "vm/Stack-inl.h"
#include "vm/StringType-inl.h"

using namespace js;
using namespace js::gc;

struct js::gc::FinalizePhase {
  gcstats::PhaseKind statsPhase;
  AllocKinds kinds;
};

/*
 * Finalization order for objects swept incrementally on the main thread.
 */
static constexpr FinalizePhase ForegroundObjectFinalizePhase = {
    gcstats::PhaseKind::FINALIZE_OBJECT,
    {AllocKind::OBJECT0, AllocKind::OBJECT2, AllocKind::OBJECT4,
     AllocKind::OBJECT8, AllocKind::OBJECT12, AllocKind::OBJECT16}};

/*
 * Finalization order for GC things swept incrementally on the main thread.
 */
static constexpr FinalizePhase ForegroundNonObjectFinalizePhase = {
    gcstats::PhaseKind::FINALIZE_NON_OBJECT,
    {AllocKind::SCRIPT, AllocKind::JITCODE}};

/*
 * Finalization order for GC things swept on the background thread.
 */
static constexpr FinalizePhase BackgroundFinalizePhases[] = {
    {gcstats::PhaseKind::FINALIZE_OBJECT,
     {AllocKind::FUNCTION, AllocKind::FUNCTION_EXTENDED,
      AllocKind::OBJECT0_BACKGROUND, AllocKind::OBJECT2_BACKGROUND,
      AllocKind::ARRAYBUFFER4, AllocKind::OBJECT4_BACKGROUND,
      AllocKind::ARRAYBUFFER8, AllocKind::OBJECT8_BACKGROUND,
      AllocKind::ARRAYBUFFER12, AllocKind::OBJECT12_BACKGROUND,
      AllocKind::ARRAYBUFFER16, AllocKind::OBJECT16_BACKGROUND}},
    {gcstats::PhaseKind::FINALIZE_NON_OBJECT,
     {AllocKind::SCOPE, AllocKind::REGEXP_SHARED, AllocKind::FAT_INLINE_STRING,
      AllocKind::STRING, AllocKind::EXTERNAL_STRING, AllocKind::FAT_INLINE_ATOM,
      AllocKind::ATOM, AllocKind::SYMBOL, AllocKind::BIGINT, AllocKind::SHAPE,
      AllocKind::BASE_SHAPE, AllocKind::GETTER_SETTER,
      AllocKind::COMPACT_PROP_MAP, AllocKind::NORMAL_PROP_MAP,
      AllocKind::DICT_PROP_MAP}}};

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
  size_t nmarked = 0, nfinalized = 0;

  for (ArenaCellIterUnderFinalize cell(this); !cell.done(); cell.next()) {
    T* t = cell.as<T>();
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
      gcprobes::TenuredFinalize(t);
      nfinalized++;
    }
  }

  if constexpr (std::is_same_v<T, JSObject>) {
    if (isNewlyCreated) {
      zone->pretenuring.updateCellCountsInNewlyCreatedArenas(
          nmarked + nfinalized, nmarked);
    }
  }
  isNewlyCreated = 0;

  if (thingKind == AllocKind::STRING ||
      thingKind == AllocKind::FAT_INLINE_STRING) {
    zone->markedStrings += nmarked;
    zone->finalizedStrings += nfinalized;
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
static inline bool FinalizeTypedArenas(JSFreeOp* fop, ArenaList& src,
                                       SortedArenaList& dest,
                                       AllocKind thingKind,
                                       SliceBudget& budget) {
  AutoSetThreadIsFinalizing setThreadUse;

  size_t thingSize = Arena::thingSize(thingKind);
  size_t thingsPerArena = Arena::thingsPerArena(thingKind);

  while (Arena* arena = src.takeFirstArena()) {
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
static bool FinalizeArenas(JSFreeOp* fop, ArenaList& src, SortedArenaList& dest,
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

void GCRuntime::initBackgroundSweep(Zone* zone, JSFreeOp* fop,
                                    const FinalizePhase& phase) {
  gcstats::AutoPhase ap(stats(), phase.statsPhase);
  for (auto kind : phase.kinds) {
    zone->arenas.initBackgroundSweep(kind);
  }
}

void ArenaLists::initBackgroundSweep(AllocKind thingKind) {
  MOZ_ASSERT(IsBackgroundFinalized(thingKind));
  MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);

  if (!collectingArenaList(thingKind).isEmpty()) {
    concurrentUse(thingKind) = ConcurrentUse::BackgroundFinalize;
  }
}

void GCRuntime::backgroundFinalize(JSFreeOp* fop, Zone* zone, AllocKind kind,
                                   Arena** empty) {
  MOZ_ASSERT(empty);

  ArenaLists* lists = &zone->arenas;
  ArenaList& arenas = lists->collectingArenaList(kind);
  if (arenas.isEmpty()) {
    MOZ_ASSERT(lists->concurrentUse(kind) == ArenaLists::ConcurrentUse::None);
    return;
  }

  SortedArenaList finalizedSorted(Arena::thingsPerArena(kind));

  auto unlimited = SliceBudget::unlimited();
  FinalizeArenas(fop, arenas, finalizedSorted, kind, unlimited);
  MOZ_ASSERT(arenas.isEmpty());

  finalizedSorted.extractEmpty(empty);

  // When marking begins, all arenas are moved from arenaLists to
  // collectingArenaLists. When the mutator runs, new arenas are allocated in
  // arenaLists. Now that finalization is complete, we want to merge these lists
  // back together.

  // We must take the GC lock to be able to safely modify the ArenaList;
  // however, this does not by itself make the changes visible to all threads,
  // as not all threads take the GC lock to read the ArenaLists.
  // That safety is provided by the ReleaseAcquire memory ordering of the
  // background finalize state, which we explicitly set as the final step.
  {
    AutoLockGC lock(rt);
    MOZ_ASSERT(lists->concurrentUse(kind) ==
               ArenaLists::ConcurrentUse::BackgroundFinalize);
    lists->mergeFinalizedArenas(kind, finalizedSorted);
  }

  lists->concurrentUse(kind) = ArenaLists::ConcurrentUse::None;
}

// After finalizing arenas, merge the following to get the final state of an
// arena list:
//  - arenas allocated during marking
//  - arenas allocated during sweeping
//  - finalized arenas
void ArenaLists::mergeFinalizedArenas(AllocKind kind,
                                      SortedArenaList& finalizedArenas) {
#ifdef DEBUG
  // Updating arena lists off-thread requires taking the GC lock because the
  // main thread uses these when allocating.
  if (IsBackgroundFinalized(kind)) {
    runtimeFromAnyThread()->gc.assertCurrentThreadHasLockedGC();
  }
#endif

  ArenaList& arenas = arenaList(kind);

  ArenaList allocatedDuringCollection = std::move(arenas);
  arenas = finalizedArenas.toArenaList();
  arenas.insertListWithCursorAtEnd(allocatedDuringCollection);

  collectingArenaList(kind).clear();
}

void ArenaLists::queueForegroundThingsForSweep() {
  gcCompactPropMapArenasToUpdate =
      collectingArenaList(AllocKind::COMPACT_PROP_MAP).head();
  gcNormalPropMapArenasToUpdate =
      collectingArenaList(AllocKind::NORMAL_PROP_MAP).head();
}

void GCRuntime::sweepBackgroundThings(ZoneList& zones) {
  if (zones.isEmpty()) {
    return;
  }

  JSFreeOp fop(nullptr);

  // Sweep zones in order. The atoms zone must be finalized last as other
  // zones may have direct pointers into it.
  while (!zones.isEmpty()) {
    Zone* zone = zones.removeFront();
    MOZ_ASSERT(zone->isGCFinished());

    Arena* emptyArenas = zone->arenas.takeSweptEmptyArenas();

    AutoSetThreadIsSweeping threadIsSweeping(zone);

    // We must finalize thing kinds in the order specified by
    // BackgroundFinalizePhases.
    for (auto phase : BackgroundFinalizePhases) {
      for (auto kind : phase.kinds) {
        backgroundFinalize(&fop, zone, kind, &emptyArenas);
      }
    }

    // Release any arenas that are now empty.
    //
    // Empty arenas are only released after everything has been finalized so
    // that it's still possible to get a thing's zone after the thing has been
    // finalized. The HeapPtr destructor depends on this, and this allows
    // HeapPtrs between things of different alloc kind regardless of
    // finalization order.
    //
    // Periodically drop and reaquire the GC lock every so often to avoid
    // blocking the main thread from allocating chunks.
    static const size_t LockReleasePeriod = 32;

    while (emptyArenas) {
      AutoLockGC lock(this);
      for (size_t i = 0; i < LockReleasePeriod && emptyArenas; i++) {
        Arena* arena = emptyArenas;
        emptyArenas = emptyArenas->next;
        releaseArena(arena, lock);
      }
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
    for (auto kind : AllAllocKinds()) {
      MOZ_ASSERT_IF(state() != State::Prepare && state() != State::Mark &&
                        state() != State::Sweep,
                    zone->arenas.collectingArenaList(kind).isEmpty());
      MOZ_ASSERT(zone->arenas.doneBackgroundFinalize(kind));
    }
  }
#endif
}

void GCRuntime::queueZonesAndStartBackgroundSweep(ZoneList& zones) {
  {
    AutoLockHelperThreadState lock;
    MOZ_ASSERT(!requestSliceAfterBackgroundTask);
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

BackgroundSweepTask::BackgroundSweepTask(GCRuntime* gc)
    : GCParallelTask(gc, gcstats::PhaseKind::SWEEP) {}

void BackgroundSweepTask::run(AutoLockHelperThreadState& lock) {
  AutoTraceLog logSweeping(TraceLoggerForCurrentThread(),
                           TraceLogger_GCSweeping);

  gc->sweepFromBackgroundThread(lock);
}

void GCRuntime::sweepFromBackgroundThread(AutoLockHelperThreadState& lock) {
  do {
    ZoneList zones;
    zones.transferFrom(backgroundSweepZones.ref());

    AutoUnlockHelperThreadState unlock(lock);
    sweepBackgroundThings(zones);

    // The main thread may call queueZonesAndStartBackgroundSweep() while this
    // is running so we must check there is no more work after releasing the
    // lock.
  } while (!backgroundSweepZones.ref().isEmpty());

  maybeRequestGCAfterBackgroundTask(lock);
}

void GCRuntime::waitBackgroundSweepEnd() {
  sweepTask.join();
  if (state() != State::Sweep) {
    assertBackgroundSweepingFinished();
  }
}

void GCRuntime::startBackgroundFree() {
  AutoLockHelperThreadState lock;
  freeTask.startOrRunIfIdle(lock);
}

BackgroundFreeTask::BackgroundFreeTask(GCRuntime* gc)
    : GCParallelTask(gc, gcstats::PhaseKind::NONE) {
  // This can occur outside GCs so doesn't have a stats phase.
}

void BackgroundFreeTask::run(AutoLockHelperThreadState& lock) {
  AutoTraceLog logFreeing(TraceLoggerForCurrentThread(), TraceLogger_GCFree);

  gc->freeFromBackgroundThread(lock);
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

template <class ZoneIterT>
IncrementalProgress GCRuntime::markWeakReferences(
    SliceBudget& incrementalBudget) {
  gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_MARK_WEAK);

  auto unlimited = SliceBudget::unlimited();
  SliceBudget& budget =
      marker.incrementalWeakMapMarkingEnabled ? incrementalBudget : unlimited;

  // We may have already entered weak marking mode.
  if (!marker.isWeakMarking() && marker.enterWeakMarkingMode()) {
    // Do not rely on the information about not-yet-marked weak keys that have
    // been collected by barriers. Clear out the gcEphemeronEdges entries and
    // rebuild the full table. Note that this a cross-zone operation; delegate
    // zone entries will be populated by map zone traversals, so everything
    // needs to be cleared first, then populated.
    if (!marker.incrementalWeakMapMarkingEnabled) {
      for (ZoneIterT zone(this); !zone.done(); zone.next()) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!zone->gcEphemeronEdges().clear()) {
          oomUnsafe.crash("clearing weak keys when entering weak marking mode");
        }
      }
    }

    for (ZoneIterT zone(this); !zone.done(); zone.next()) {
      if (zone->enterWeakMarkingMode(&marker, budget) == NotFinished) {
        MOZ_ASSERT(marker.incrementalWeakMapMarkingEnabled);
        marker.leaveWeakMarkingMode();
        return NotFinished;
      }
    }
  }

  // This is not strictly necessary; if we yield here, we could run the mutator
  // in weak marking mode and unmark gray would end up doing the key lookups.
  // But it seems better to not slow down barriers. Re-entering weak marking
  // mode will be fast since already-processed markables have been removed.
  auto leaveOnExit =
      mozilla::MakeScopeExit([&] { marker.leaveWeakMarkingMode(); });

  bool markedAny = true;
  while (markedAny) {
    if (!marker.markUntilBudgetExhausted(budget)) {
      MOZ_ASSERT(marker.incrementalWeakMapMarkingEnabled);
      return NotFinished;
    }

    markedAny = false;

    if (!marker.isWeakMarking()) {
      for (ZoneIterT zone(this); !zone.done(); zone.next()) {
        markedAny |= WeakMapBase::markZoneIteratively(zone, &marker);
      }
    }

    markedAny |= jit::JitRuntime::MarkJitcodeGlobalTableIteratively(&marker);
  }
  MOZ_ASSERT(marker.isDrained());

  return Finished;
}

IncrementalProgress GCRuntime::markWeakReferencesInCurrentGroup(
    SliceBudget& budget) {
  return markWeakReferences<SweepGroupZonesIter>(budget);
}

template <class ZoneIterT>
IncrementalProgress GCRuntime::markGrayRoots(SliceBudget& budget,
                                             gcstats::PhaseKind phase) {
  MOZ_ASSERT(marker.markColor() == MarkColor::Gray);

  gcstats::AutoPhase ap(stats(), phase);

  AutoUpdateLiveCompartments updateLive(this);

  if (traceEmbeddingGrayRoots(&marker, budget) == NotFinished) {
    return NotFinished;
  }

  Compartment::traceIncomingCrossCompartmentEdgesForZoneGC(
      &marker, Compartment::GrayEdges);

  return Finished;
}

IncrementalProgress GCRuntime::markAllWeakReferences() {
  SliceBudget budget = SliceBudget::unlimited();
  return markWeakReferences<GCZonesIter>(budget);
}

void GCRuntime::markAllGrayReferences(gcstats::PhaseKind phase) {
  SliceBudget budget = SliceBudget::unlimited();
  markGrayRoots<GCZonesIter>(budget, phase);
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
      if (key->isMarkedBlack()) {
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
    Value val = marker.markQueue[i].get();
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
  ZoneComponentFinder finder(cx);
  if (!isIncremental || !findSweepGroupEdges()) {
    finder.useOneComponent();
  }

  // Use one component for two-slice zeal modes.
  if (useZeal && hasIncrementalTwoSliceZealMode()) {
    finder.useOneComponent();
  }

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
    markTask.join();

    // Abort collection of subsequent sweep groups.
    for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
      MOZ_ASSERT(!zone->gcNextGraphComponent);
      zone->changeGCState(Zone::MarkBlackOnly, Zone::NoGC);
      zone->arenas.unmarkPreMarkedFreeCells();
      zone->arenas.mergeArenasFromCollectingLists();
      zone->clearGCSliceThresholds();
    }

    for (SweepGroupCompartmentsIter comp(rt); !comp.done(); comp.next()) {
      resetGrayList(comp);
    }

    abortSweepAfterCurrentGroup = false;
    currentSweepGroup = nullptr;
  }
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
 * GCRuntime::markIncomingGrayCrossCompartmentPointers.
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

void GCRuntime::markIncomingGrayCrossCompartmentPointers() {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK_INCOMING_GRAY);

  for (SweepGroupCompartmentsIter c(rt); !c.done(); c.next()) {
    MOZ_ASSERT(c->zone()->isGCMarkingBlackAndGray());
    MOZ_ASSERT_IF(c->gcIncomingGrayPointers,
                  IsGrayListObject(c->gcIncomingGrayPointers));

    for (JSObject* src = c->gcIncomingGrayPointers; src;
         src = NextIncomingCrossCompartmentPointer(src, true)) {
      JSObject* dst = CrossCompartmentPointerReferent(src);
      MOZ_ASSERT(dst->compartment() == c);
      MOZ_ASSERT_IF(src->asTenured().isMarkedBlack(),
                    dst->asTenured().isMarkedBlack());

      if (src->asTenured().isMarkedGray()) {
        TraceManuallyBarrieredEdge(&marker, &dst,
                                   "cross-compartment gray pointer");
      }
    }

    c->gcIncomingGrayPointers = nullptr;
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

void GCRuntime::resetGrayList(Compartment* comp) {
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

void js::NotifyGCNukeWrapper(JSObject* wrapper) {
  MOZ_ASSERT(IsCrossCompartmentWrapper(wrapper));

  /*
   * References to target of wrapper are being removed, we no longer have to
   * remember to mark it.
   */
  RemoveFromGrayList(wrapper);

  /*
   * Clean up WeakRef maps which might include this wrapper.
   */
  JSObject* target = UncheckedUnwrapWithoutExpose(wrapper);
  if (target->is<WeakRefObject>()) {
    WeakRefObject* weakRef = &target->as<WeakRefObject>();
    GCRuntime* gc = &weakRef->runtimeFromMainThread()->gc;
    if (weakRef->target() && gc->unregisterWeakRefWrapper(wrapper)) {
      weakRef->clearTarget();
    }
  }

  /*
   * Clean up FinalizationRecord record objects which might be the target of
   * this wrapper.
   */
  if (target->is<FinalizationRecordObject>()) {
    auto* record = &target->as<FinalizationRecordObject>();
    FinalizationRegistryObject::unregisterRecord(record);
  }
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

IncrementalProgress GCRuntime::beginMarkingSweepGroup(JSFreeOp* fop,
                                                      SliceBudget& budget) {
  MOZ_ASSERT(!markOnBackgroundThreadDuringSweeping);
  MOZ_ASSERT(marker.isDrained());
  MOZ_ASSERT(marker.markColor() == MarkColor::Black);
  MOZ_ASSERT(cellsToAssertNotGray.ref().empty());

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

  // Change state of current group to MarkBlackAndGray to restrict gray marking
  // to this group. Note that there may be pointers to the atoms zone, and these
  // will be marked through, as they are not marked with
  // TraceCrossCompartmentEdge.
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    zone->changeGCState(Zone::MarkBlackOnly, Zone::MarkBlackAndGray);
  }

  AutoSetMarkColor setColorGray(marker, MarkColor::Gray);
  marker.setMainStackColor(MarkColor::Gray);

  // Mark incoming gray pointers from previously swept compartments.
  markIncomingGrayCrossCompartmentPointers();

  return Finished;
}

IncrementalProgress GCRuntime::markGrayRootsInCurrentGroup(
    JSFreeOp* fop, SliceBudget& budget) {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

  AutoSetMarkColor setColorGray(marker, MarkColor::Gray);

  return markGrayRoots<SweepGroupZonesIter>(
      budget, gcstats::PhaseKind::SWEEP_MARK_GRAY);
}

IncrementalProgress GCRuntime::markGray(JSFreeOp* fop, SliceBudget& budget) {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

  if (markUntilBudgetExhausted(budget) == NotFinished) {
    return NotFinished;
  }

  marker.setMainStackColor(MarkColor::Black);
  return Finished;
}

IncrementalProgress GCRuntime::endMarkingSweepGroup(JSFreeOp* fop,
                                                    SliceBudget& budget) {
  MOZ_ASSERT(!markOnBackgroundThreadDuringSweeping);
  MOZ_ASSERT(marker.isDrained());

  MOZ_ASSERT(marker.markColor() == MarkColor::Black);
  MOZ_ASSERT(!HasIncomingCrossCompartmentPointers(rt));

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

  if (markWeakReferencesInCurrentGroup(budget) == NotFinished) {
    return NotFinished;
  }

  AutoSetMarkColor setColorGray(marker, MarkColor::Gray);
  marker.setMainStackColor(MarkColor::Gray);

  // Mark transitively inside the current compartment group.
  if (markWeakReferencesInCurrentGroup(budget) == NotFinished) {
    return NotFinished;
  }

  MOZ_ASSERT(marker.isDrained());
  marker.setMainStackColor(MarkColor::Black);

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
      : GCParallelTask(gc, gcstats::PhaseKind::SWEEP_WEAK_CACHES),
        zone(zone),
        cache(wc) {}

  ImmediateSweepWeakCacheTask(ImmediateSweepWeakCacheTask&& other)
      : GCParallelTask(std::move(other)),
        zone(other.zone),
        cache(other.cache) {}

  void run(AutoLockHelperThreadState& lock) override {
    AutoUnlockHelperThreadState unlock(lock);
    AutoSetThreadIsSweeping threadIsSweeping(zone);
    SweepingTracer trc(gc->rt);
    cache.traceWeak(&trc, &gc->storeBuffer());
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
  SweepingTracer trc(rt);
  rt->symbolRegistry().traceWeak(&trc);
}

void GCRuntime::sweepCCWrappers() {
  SweepingTracer trc(rt);
  AutoSetThreadIsSweeping threadIsSweeping;  // This can touch all zones.
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    zone->traceWeakCCWEdges(&trc);
  }
}

void GCRuntime::sweepRealmGlobals() {
  SweepingTracer trc(rt);
  for (SweepGroupRealmsIter r(this); !r.done(); r.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(r->zone());
    r->traceWeakGlobalEdge(&trc);
  }
}

void GCRuntime::sweepMisc() {
  SweepingTracer trc(rt);
  for (SweepGroupRealmsIter r(this); !r.done(); r.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(r->zone());
    r->traceWeakSavedStacks(&trc);
    r->traceWeakObjectRealm(&trc);
    r->traceWeakRegExps(&trc);
  }
}

void GCRuntime::sweepCompressionTasks() {
  JSRuntime* runtime = rt;

  // Attach finished compression tasks.
  AutoLockHelperThreadState lock;
  AttachFinishedCompressions(runtime, lock);
  SweepPendingCompressions(lock);
}

void GCRuntime::sweepWeakMaps() {
  SweepingTracer trc(rt);
  AutoSetThreadIsSweeping threadIsSweeping;  // This may touch any zone.
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    /* No need to look up any more weakmap keys from this sweep group. */
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!zone->gcEphemeronEdges().clear()) {
      oomUnsafe.crash("clearing weak keys in beginSweepingSweepGroup()");
    }

    // Lock the storebuffer since this may access it when rehashing or resizing
    // the tables.
    AutoLockStoreBuffer lock(&storeBuffer());
    zone->sweepWeakMaps(&trc);
  }
}

void GCRuntime::sweepUniqueIds() {
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(zone);
    zone->sweepUniqueIds();
  }
}

void JS::Zone::sweepUniqueIds() {
  SweepingTracer trc(runtimeFromAnyThread());
  uniqueIds().traceWeak(&trc);
}

/* static */
bool UniqueIdGCPolicy::traceWeak(JSTracer* trc, Cell** keyp, uint64_t* valuep) {
  // Since this is only ever used for sweeping, we can optimize it for that
  // case. (Compacting GC updates this table manually when it moves a cell.)
  MOZ_ASSERT(trc->kind() == JS::TracerKind::Sweeping);
  return (*keyp)->isMarkedAny();
}

void GCRuntime::sweepWeakRefs() {
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    AutoSetThreadIsSweeping threadIsSweeping(zone);
    zone->weakRefMap().traceWeak(&sweepingTracer, &storeBuffer());
  }
}

void GCRuntime::sweepFinalizationRegistriesOnMainThread() {
  // This calls back into the browser which expects to be called from the main
  // thread.
  gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);
  gcstats::AutoPhase ap2(stats(),
                         gcstats::PhaseKind::SWEEP_FINALIZATION_REGISTRIES);
  SweepingTracer trc(rt);
  AutoLockStoreBuffer lock(&storeBuffer());
  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    traceWeakFinalizationRegistryEdges(&trc, zone);
  }
}

void GCRuntime::startTask(GCParallelTask& task,
                          AutoLockHelperThreadState& lock) {
  if (!CanUseExtraThreads()) {
    AutoUnlockHelperThreadState unlock(lock);
    task.runFromMainThread();
    stats().recordParallelPhase(task.phaseKind, task.duration());
    return;
  }

  task.startWithLockHeld(lock);
}

void GCRuntime::joinTask(GCParallelTask& task,
                         AutoLockHelperThreadState& lock) {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::JOIN_PARALLEL_TASKS);
  task.joinWithLockHeld(lock);
}

void GCRuntime::sweepDebuggerOnMainThread(JSFreeOp* fop) {
  SweepingTracer trc(rt);
  AutoLockStoreBuffer lock(&storeBuffer());

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
      r->traceWeakDebugEnvironmentEdges(&trc);
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

  GCRuntime* gc = &rt->gc;
  bool ok =
      IterateWeakCaches(rt, [&](JS::detail::WeakCacheBase* cache, Zone* zone) {
        if (cache->empty()) {
          return true;
        }

        // Caches that support incremental sweeping will be swept later.
        if (zone && cache->setIncrementalBarrierTracer(&gc->sweepingTracer)) {
          return true;
        }

        return immediateTasks->emplaceBack(gc, zone, *cache);
      });

  if (!ok) {
    immediateTasks->clearAndFree();
  }

  return ok;
}

static void SweepAllWeakCachesOnMainThread(JSRuntime* rt) {
  // If we ran out of memory, do all the work on the main thread.
  gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::SWEEP_WEAK_CACHES);
  SweepingTracer trc(rt);
  IterateWeakCaches(rt, [&](JS::detail::WeakCacheBase* cache, Zone* zone) {
    if (cache->needsIncrementalBarrier()) {
      cache->setIncrementalBarrierTracer(nullptr);
    }
    cache->traceWeak(&trc, &rt->gc.storeBuffer());
    return true;
  });
}

void GCRuntime::sweepEmbeddingWeakPointers(JSFreeOp* fop) {
  using namespace gcstats;

  AutoLockStoreBuffer lock(&storeBuffer());

  AutoPhase ap(stats(), PhaseKind::FINALIZE_START);
  callFinalizeCallbacks(fop, JSFINALIZE_GROUP_PREPARE);
  {
    AutoPhase ap2(stats(), PhaseKind::WEAK_ZONES_CALLBACK);
    callWeakPointerZonesCallbacks(&sweepingTracer);
  }
  {
    AutoPhase ap2(stats(), PhaseKind::WEAK_COMPARTMENT_CALLBACK);
    for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
      for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        callWeakPointerCompartmentCallbacks(&sweepingTracer, comp);
      }
    }
  }
  callFinalizeCallbacks(fop, JSFINALIZE_GROUP_START);
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
    zone->arenas.checkSweepStateNotInUse();
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

  // Updating the atom marking bitmaps. This marks atoms referenced by
  // uncollected zones so cannot be done in parallel with the other sweeping
  // work below.
  if (sweepingAtoms) {
    AutoPhase ap(stats(), PhaseKind::UPDATE_ATOMS_BITMAP);
    updateAtomsBitmap();
  }

  AutoSetThreadIsSweeping threadIsSweeping;

  // This must happen before sweeping realm globals.
  sweepDebuggerOnMainThread(fop);

  // This must happen before updating embedding weak pointers.
  sweepRealmGlobals();

  sweepEmbeddingWeakPointers(fop);

  {
    AutoLockHelperThreadState lock;

    AutoPhase ap(stats(), PhaseKind::SWEEP_COMPARTMENTS);

    AutoRunParallelTask sweepCCWrappers(this, &GCRuntime::sweepCCWrappers,
                                        PhaseKind::SWEEP_CC_WRAPPER, lock);
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
    bool canSweepWeakCachesOffThread =
        PrepareWeakCacheTasks(rt, &sweepCacheTasks);
    if (canSweepWeakCachesOffThread) {
      weakCachesToSweep.ref().emplace(currentSweepGroup);
      for (auto& task : sweepCacheTasks) {
        startTask(task, lock);
      }
    }

    {
      AutoUnlockHelperThreadState unlock(lock);
      sweepJitDataOnMainThread(fop);

      if (!canSweepWeakCachesOffThread) {
        MOZ_ASSERT(sweepCacheTasks.empty());
        SweepAllWeakCachesOnMainThread(rt);
      }
    }

    for (auto& task : sweepCacheTasks) {
      joinTask(task, lock);
    }
  }

  if (sweepingAtoms) {
    startSweepingAtomsTable();
  }

  // FinalizationRegistry sweeping touches weak maps and so must not run in
  // parallel with that. This triggers a read barrier and can add marking work
  // for zones that are still marking.
  sweepFinalizationRegistriesOnMainThread();

  // Queue all GC things in all zones for sweeping, either on the foreground
  // or on the background thread.

  for (SweepGroupZonesIter zone(this); !zone.done(); zone.next()) {
    for (const auto& phase : BackgroundFinalizePhases) {
      initBackgroundSweep(zone, fop, phase);
    }

    zone->arenas.queueForegroundThingsForSweep();
  }

  MOZ_ASSERT(!sweepZone);

  safeToYield = true;
  markOnBackgroundThreadDuringSweeping = CanUseExtraThreads();

  return Finished;
}

#ifdef JS_GC_ZEAL
bool GCRuntime::shouldYieldForZeal(ZealMode mode) {
  bool yield = useZeal && hasZealMode(mode);

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
  // This is to prevent a race between markTask checking the zone state and
  // us changing it below.
  if (joinBackgroundMarkTask() == NotFinished) {
    return NotFinished;
  }

  MOZ_ASSERT(marker.isDrained());

  // Disable background marking during sweeping until we start sweeping the next
  // zone group.
  markOnBackgroundThreadDuringSweeping = false;

  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::FINALIZE_END);
    AutoLockStoreBuffer lock(&storeBuffer());
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
    zone->arenas.unmarkPreMarkedFreeCells();
    zone->arenas.checkNoArenasToUpdate();
    zone->pretenuring.clearCellCountsInNewlyCreatedArenas();
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

IncrementalProgress GCRuntime::markDuringSweeping(JSFreeOp* fop,
                                                  SliceBudget& budget) {
  MOZ_ASSERT(markTask.isIdle());

  if (marker.isDrained()) {
    return Finished;
  }

  if (markOnBackgroundThreadDuringSweeping) {
    AutoLockHelperThreadState lock;
    MOZ_ASSERT(markTask.isIdle(lock));
    markTask.setBudget(budget);
    markTask.startOrRunIfIdle(lock);
    return Finished;  // This means don't yield to the mutator here.
  }

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);
  return markUntilBudgetExhausted(budget);
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
  MOZ_ASSERT(!markOnBackgroundThreadDuringSweeping);

#ifdef DEBUG
  releaseHeldRelocatedArenas();
  verifyAllChunks();
#endif

#ifdef JS_GC_ZEAL
  computeNonIncrementalMarkingForValidation(session);
#endif

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);

  AssertNoWrappersInGrayList(rt);
  dropStringWrappers();

  groupZonesForSweeping(reason);

  sweepActions->assertFinished();
}

bool GCRuntime::foregroundFinalize(JSFreeOp* fop, Zone* zone,
                                   AllocKind thingKind,
                                   SliceBudget& sliceBudget,
                                   SortedArenaList& sweepList) {
  ArenaLists& lists = zone->arenas;
  lists.checkNoArenasToUpdateForKind(thingKind);

  // Non-empty arenas are reused for use for new allocations as soon as the
  // finalizers for that allocation kind have run. Empty arenas are only
  // released when everything in the zone has been swept (see
  // GCRuntime::sweepBackgroundThings for more details).
  if (!FinalizeArenas(fop, lists.collectingArenaList(thingKind), sweepList,
                      thingKind, sliceBudget)) {
    // Copy the current contents of sweepList so that ArenaIter can find them.
    lists.setIncrementalSweptArenas(thingKind, sweepList);
    return false;
  }

  sweepList.extractEmpty(&lists.savedEmptyArenas.ref());
  lists.mergeFinalizedArenas(thingKind, sweepList);
  lists.clearIncrementalSweptArenas();

  return true;
}

BackgroundMarkTask::BackgroundMarkTask(GCRuntime* gc)
    : GCParallelTask(gc, gcstats::PhaseKind::SWEEP_MARK),
      budget(SliceBudget::unlimited()) {}

void js::gc::BackgroundMarkTask::run(AutoLockHelperThreadState& lock) {
  AutoUnlockHelperThreadState unlock(lock);

  // Time reporting is handled separately for parallel tasks.
  gc->sweepMarkResult =
      gc->markUntilBudgetExhausted(this->budget, GCMarker::DontReportMarkTime);
}

IncrementalProgress GCRuntime::joinBackgroundMarkTask() {
  AutoLockHelperThreadState lock;
  if (markTask.isIdle(lock)) {
    return Finished;
  }

  joinTask(markTask, lock);

  IncrementalProgress result = sweepMarkResult;
  sweepMarkResult = Finished;
  return result;
}

template <typename T>
static void SweepThing(JSFreeOp* fop, T* thing) {
  if (!thing->isMarkedAny()) {
    thing->sweep(fop);
  }
}

template <typename T>
static bool SweepArenaList(JSFreeOp* fop, Arena** arenasToSweep,
                           SliceBudget& sliceBudget) {
  while (Arena* arena = *arenasToSweep) {
    MOZ_ASSERT(arena->zone->isGCSweeping());

    for (ArenaCellIterUnderGC cell(arena); !cell.done(); cell.next()) {
      SweepThing(fop, cell.as<T>());
    }

    Arena* next = arena->next;
    MOZ_ASSERT_IF(next, next->zone == arena->zone);
    *arenasToSweep = next;

    AllocKind kind = MapTypeToAllocKind<T>::kind;
    sliceBudget.step(Arena::thingsPerArena(kind));
    if (sliceBudget.isOverBudget()) {
      return false;
    }
  }

  return true;
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
  if (!atomsTable->startIncrementalSweep(maybeAtoms)) {
    SweepingTracer trc(rt);
    atomsTable->traceWeak(&trc);
  }
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

  SweepingTracer trc(gc->rt);
  size_t steps = cache->traceWeak(&trc, &gc->storeBuffer());
  cache->setIncrementalBarrierTracer(nullptr);

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
  MOZ_ASSERT(sweepZone->isGCSweeping());

  // Set the number of things per arena for this AllocKind.
  size_t thingsPerArena = Arena::thingsPerArena(sweepAllocKind);
  auto& sweepList = incrementalSweepList.ref();
  sweepList.setThingsPerArena(thingsPerArena);

  AutoSetThreadIsSweeping threadIsSweeping(sweepZone);

  if (!foregroundFinalize(fop, sweepZone, sweepAllocKind, budget, sweepList)) {
    return NotFinished;
  }

  // Reset the slots of the sweep list that we used.
  sweepList.reset(thingsPerArena);

  return Finished;
}

IncrementalProgress GCRuntime::sweepPropMapTree(JSFreeOp* fop,
                                                SliceBudget& budget) {
  // Remove dead SharedPropMaps from the tree. This happens incrementally on the
  // main thread. PropMaps are finalized later on the a background thread.

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_PROP_MAP);

  ArenaLists& al = sweepZone->arenas;

  if (!SweepArenaList<CompactPropMap>(
          fop, &al.gcCompactPropMapArenasToUpdate.ref(), budget)) {
    return NotFinished;
  }
  if (!SweepArenaList<NormalPropMap>(
          fop, &al.gcNormalPropMapArenasToUpdate.ref(), budget)) {
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
  using State = mozilla::Maybe<Iter>;
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
  if (!seq || !seq->init(actions, std::size(actions))) {
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
          Call(&GCRuntime::beginMarkingSweepGroup),
          Call(&GCRuntime::markGrayRootsInCurrentGroup),
          MaybeYield(ZealMode::YieldWhileGrayMarking),
          Call(&GCRuntime::markGray), Call(&GCRuntime::endMarkingSweepGroup),
          Call(&GCRuntime::beginSweepingSweepGroup),
          MaybeYield(ZealMode::IncrementalMultipleSlices),
          MaybeYield(ZealMode::YieldBeforeSweepingAtoms),
          Call(&GCRuntime::sweepAtomsTable),
          MaybeYield(ZealMode::YieldBeforeSweepingCaches),
          Call(&GCRuntime::sweepWeakCaches),
          ForEachZoneInSweepGroup(
              rt, &sweepZone.ref(),
              Sequence(MaybeYield(ZealMode::YieldBeforeSweepingObjects),
                       ForEachAllocKind(ForegroundObjectFinalizePhase.kinds,
                                        &sweepAllocKind.ref(),
                                        Call(&GCRuntime::finalizeAllocKind)),
                       MaybeYield(ZealMode::YieldBeforeSweepingNonObjects),
                       ForEachAllocKind(ForegroundNonObjectFinalizePhase.kinds,
                                        &sweepAllocKind.ref(),
                                        Call(&GCRuntime::finalizeAllocKind)),
                       MaybeYield(ZealMode::YieldBeforeSweepingPropMapTrees),
                       Call(&GCRuntime::sweepPropMapTree))),
          Call(&GCRuntime::endSweepingSweepGroup)));

  return sweepActions != nullptr;
}

IncrementalProgress GCRuntime::performSweepActions(SliceBudget& budget) {
  AutoMajorGCProfilerEntry s(this);
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);
  JSFreeOp fop(rt);

  // Don't trigger pre-barriers when finalizing.
  AutoDisableBarriers disableBarriers(this);

  // Drain the mark stack, possibly in a parallel task if we're in a part of
  // sweeping that allows it.
  //
  // In the first sweep slice where we must not yield to the mutator until we've
  // starting sweeping a sweep group but in that case the stack must be empty
  // already.

  MOZ_ASSERT(initialState <= State::Sweep);
  MOZ_ASSERT_IF(initialState != State::Sweep, marker.isDrained());
  if (initialState == State::Sweep &&
      markDuringSweeping(&fop, budget) == NotFinished) {
    return NotFinished;
  }

  // Then continue running sweep actions.

  SweepAction::Args args{this, &fop, budget};
  IncrementalProgress sweepProgress = sweepActions->run(args);
  IncrementalProgress markProgress = joinBackgroundMarkTask();

  if (sweepProgress == Finished && markProgress == Finished) {
    return Finished;
  }

  MOZ_ASSERT(isIncremental);
  return NotFinished;
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
    if (!zone->isCollecting() && !zone->arenas.arenaListsAreEmpty()) {
      return false;
    }
  }

  return true;
}

void GCRuntime::endSweepPhase(bool destroyingRuntime) {
  MOZ_ASSERT(!markOnBackgroundThreadDuringSweeping);

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
    AutoLockStoreBuffer lock(&storeBuffer());
    callFinalizeCallbacks(&fop, JSFINALIZE_COLLECTION_END);

    if (allCCVisibleZonesWereCollected()) {
      grayBitsValid = true;
    }
  }

  if (isIncremental) {
    findDeadCompartments();
  }

#ifdef JS_GC_ZEAL
  finishMarkingValidation();
#endif

#ifdef DEBUG
  for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next()) {
    for (auto i : AllAllocKinds()) {
      MOZ_ASSERT_IF(!IsBackgroundFinalized(i) || !sweepOnBackgroundThread,
                    zone->arenas.collectingArenaList(i).isEmpty());
    }
  }
#endif

  AssertNoWrappersInGrayList(rt);
}
