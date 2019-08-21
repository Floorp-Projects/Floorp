/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone-inl.h"

#include "gc/FreeOp.h"
#include "gc/Policy.h"
#include "gc/PublicIterators.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/JitRealm.h"
#include "vm/Runtime.h"
#include "wasm/WasmInstance.h"

#include "debugger/DebugAPI-inl.h"
#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "gc/WeakMap-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Realm-inl.h"

using namespace js;
using namespace js::gc;

Zone* const Zone::NotOnList = reinterpret_cast<Zone*>(1);

ZoneAllocator::ZoneAllocator(JSRuntime* rt)
    : JS::shadow::Zone(rt, &rt->gc.marker),
      gcHeapSize(&rt->gc.heapSize),
      mallocHeapSize(nullptr),
      jitHeapSize(nullptr),
      jitHeapThreshold(jit::MaxCodeBytesPerProcess * 0.8) {
  AutoLockGC lock(rt);
  updateGCThresholds(rt->gc, GC_NORMAL, lock);
}

ZoneAllocator::~ZoneAllocator() {
#ifdef DEBUG
  mallocTracker.checkEmptyOnDestroy();
  MOZ_ASSERT(gcHeapSize.bytes() == 0);
  MOZ_ASSERT(mallocHeapSize.bytes() == 0);
  MOZ_ASSERT(jitHeapSize.bytes() == 0);
#endif
}

void ZoneAllocator::fixupAfterMovingGC() {
#ifdef DEBUG
  mallocTracker.fixupAfterMovingGC();
#endif
}

void js::ZoneAllocator::updateMemoryCountersOnGCStart() {
  gcHeapSize.updateOnGCStart();
  mallocHeapSize.updateOnGCStart();
}

void js::ZoneAllocator::updateGCThresholds(GCRuntime& gc,
                                           JSGCInvocationKind invocationKind,
                                           const js::AutoLockGC& lock) {
  // This is called repeatedly during a GC to update thresholds as memory is
  // freed.
  gcHeapThreshold.updateAfterGC(gcHeapSize.retainedBytes(), invocationKind,
                                gc.tunables, gc.schedulingState, lock);
  mallocHeapThreshold.updateAfterGC(mallocHeapSize.retainedBytes(),
                                    gc.tunables.mallocThresholdBase(),
                                    gc.tunables.mallocGrowthFactor(), lock);
}

void ZoneAllocPolicy::decMemory(size_t nbytes) {
  // Unfortunately we don't have enough context here to know whether we're being
  // called on behalf of the collector so we have to do a TLS lookup to find
  // out.
  JSContext* cx = TlsContext.get();
  zone_->decPolicyMemory(this, nbytes, cx->defaultFreeOp()->isCollecting());
}

JS::Zone::Zone(JSRuntime* rt)
    : ZoneAllocator(rt),
      // Note: don't use |this| before initializing helperThreadUse_!
      // ProtectedData checks in CheckZone::check may read this field.
      helperThreadUse_(HelperThreadUse::None),
      helperThreadOwnerContext_(nullptr),
      debuggers(this, nullptr),
      uniqueIds_(this),
      suppressAllocationMetadataBuilder(this, false),
      arenas(this),
      tenuredAllocsSinceMinorGC_(0),
      types(this),
      gcWeakMapList_(this),
      compartments_(),
      crossZoneStringWrappers_(this),
      gcGrayRoots_(this),
      weakCaches_(this),
      gcWeakKeys_(this, SystemAllocPolicy(), rt->randomHashCodeScrambler()),
      gcNurseryWeakKeys_(this, SystemAllocPolicy(),
                         rt->randomHashCodeScrambler()),
      typeDescrObjects_(this, this),
      markedAtoms_(this),
      atomCache_(this),
      externalStringCache_(this),
      functionToStringCache_(this),
      keepAtomsCount(this, 0),
      purgeAtomsDeferred(this, 0),
      tenuredStrings(this, 0),
      allocNurseryStrings(this, true),
      propertyTree_(this, this),
      baseShapes_(this, this),
      initialShapes_(this, this),
      nurseryShapes_(this),
      data(this, nullptr),
      isSystem(this, false),
#ifdef DEBUG
      gcSweepGroupIndex(0),
#endif
      jitZone_(this, nullptr),
      gcScheduled_(false),
      gcScheduledSaved_(false),
      gcPreserveCode_(false),
      keepShapeCaches_(this, false),
      wasCollected_(false),
      listNext_(NotOnList) {
  /* Ensure that there are no vtables to mess us up here. */
  MOZ_ASSERT(reinterpret_cast<JS::shadow::Zone*>(this) ==
             static_cast<JS::shadow::Zone*>(this));
}

Zone::~Zone() {
  MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::None);
  MOZ_ASSERT(gcWeakMapList().isEmpty());
  MOZ_ASSERT_IF(regExps_.ref(), regExps().empty());

  JSRuntime* rt = runtimeFromAnyThread();
  if (this == rt->gc.systemZone) {
    rt->gc.systemZone = nullptr;
  }

  js_delete(debuggers.ref());
  js_delete(jitZone_.ref());
}

bool Zone::init(bool isSystemArg) {
  isSystem = isSystemArg;
  regExps_.ref() = make_unique<RegExpZone>(this);
  return regExps_.ref() && gcWeakKeys().init() && gcNurseryWeakKeys().init();
}

void Zone::setNeedsIncrementalBarrier(bool needs) {
  MOZ_ASSERT_IF(needs, canCollect());
  needsIncrementalBarrier_ = needs;
}

void Zone::beginSweepTypes() { types.beginSweep(); }

Zone::DebuggerVector* Zone::getOrCreateDebuggers(JSContext* cx) {
  if (debuggers) {
    return debuggers;
  }

  debuggers = js_new<DebuggerVector>();
  if (!debuggers) {
    ReportOutOfMemory(cx);
  }
  return debuggers;
}

void Zone::sweepBreakpoints(JSFreeOp* fop) {
  if (fop->runtime()->debuggerList().isEmpty()) {
    return;
  }

  /*
   * Sweep all compartments in a zone at the same time, since there is no way
   * to iterate over the scripts belonging to a single compartment in a zone.
   */

  MOZ_ASSERT(isGCSweepingOrCompacting());
  for (auto iter = cellIterUnsafe<JSScript>(); !iter.done(); iter.next()) {
    JSScript* script = iter;
    DebugAPI::sweepBreakpoints(fop, script);
  }

  for (RealmsInZoneIter realms(this); !realms.done(); realms.next()) {
    for (wasm::Instance* instance : realms->wasm.instances()) {
      if (!instance->debugEnabled()) {
        continue;
      }
      if (!IsAboutToBeFinalized(&instance->object_)) {
        continue;
      }
      instance->debug().clearAllBreakpoints(fop, instance->objectUnbarriered());
    }
  }
}

static void SweepWeakEntryVectorWhileMinorSweeping(
    js::gc::WeakEntryVector& entries) {
  EraseIf(entries, [](js::gc::WeakMarkable& markable) -> bool {
    return IsAboutToBeFinalizedDuringMinorSweep(&markable.key);
  });
}

void Zone::sweepAfterMinorGC(JSTracer* trc) {
  sweepWeakKeysAfterMinorGC();
  crossZoneStringWrappers().sweepAfterMinorGC(trc);
}

void Zone::sweepWeakKeysAfterMinorGC() {
  for (WeakKeyTable::Range r = gcNurseryWeakKeys().all(); !r.empty();
       r.popFront()) {
    // Sweep gcNurseryWeakKeys to move live (forwarded) keys to gcWeakKeys,
    // scanning through all the entries for such keys to update them.
    //
    // Forwarded and dead keys may also appear in their delegates' entries,
    // so sweep those too (see below.)

    // The tricky case is when the key has a delegate that was already
    // tenured. Then it will be in its compartment's gcWeakKeys, but we
    // still need to update the key (which will be in the entries
    // associated with it.)
    gc::Cell* key = r.front().key;
    MOZ_ASSERT(!key->isTenured());
    if (!Nursery::getForwardedPointer(&key)) {
      // Dead nursery cell => discard.
      continue;
    }

    // Key been moved. The value is an array of <map,key> pairs; update all
    // keys in that array.
    WeakEntryVector& entries = r.front().value;
    SweepWeakEntryVectorWhileMinorSweeping(entries);

    // Live (moved) nursery cell. Append entries to gcWeakKeys.
    auto entry = gcWeakKeys().get(key);
    if (!entry) {
      if (!gcWeakKeys().put(key, gc::WeakEntryVector())) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        oomUnsafe.crash("Failed to tenure weak keys entry");
      }
      entry = gcWeakKeys().get(key);
    }

    for (auto& markable : entries) {
      if (!entry->value.append(markable)) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        oomUnsafe.crash("Failed to tenure weak keys entry");
      }
    }

    // If the key has a delegate, then it will map to a WeakKeyEntryVector
    // containing the key that needs to be updated.

    JSObject* delegate = WeakMapBase::getDelegate(key->as<JSObject>());
    if (!delegate) {
      continue;
    }
    MOZ_ASSERT(delegate->isTenured());

    // If delegate was formerly nursery-allocated, we will sweep its
    // entries when we visit its gcNurseryWeakKeys (if we haven't already).
    // Note that we don't know the nursery address of the delegate, since
    // the location it was stored in has already been updated.
    //
    // Otherwise, it will be in gcWeakKeys and we sweep it here.
    auto p = delegate->zone()->gcWeakKeys().get(delegate);
    if (p) {
      SweepWeakEntryVectorWhileMinorSweeping(p->value);
    }
  }

  if (!gcNurseryWeakKeys().clear()) {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    oomUnsafe.crash("OOM while clearing gcNurseryWeakKeys.");
  }
}

void Zone::sweepAllCrossCompartmentWrappers() {
  crossZoneStringWrappers().sweep();
  for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next()) {
    comp->sweepCrossCompartmentObjectWrappers();
  }
}

/* static */
void Zone::fixupAllCrossCompartmentWrappersAfterMovingGC(JSTracer* trc) {
  MOZ_ASSERT(trc->runtime()->gc.isHeapCompacting());

  for (ZonesIter zone(trc->runtime(), WithAtoms); !zone.done(); zone.next()) {
    // Sweep the wrapper map to update keys (wrapped values) in other
    // compartments that may have been moved.
    zone->crossZoneStringWrappers().sweep();

    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
      comp->fixupCrossCompartmentObjectWrappersAfterMovingGC(trc);
    }
  }
}

void Zone::dropStringWrappersOnGC() {
  MOZ_ASSERT(JS::RuntimeHeapIsCollecting());
  crossZoneStringWrappers().clear();
}

#ifdef JSGC_HASH_TABLE_CHECKS

void Zone::checkAllCrossCompartmentWrappersAfterMovingGC() {
  checkStringWrappersAfterMovingGC();
  for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next()) {
    comp->checkObjectWrappersAfterMovingGC();
  }
}

void Zone::checkStringWrappersAfterMovingGC() {
  for (StringWrapperMap::Enum e(crossZoneStringWrappers()); !e.empty();
       e.popFront()) {
    // Assert that the postbarriers have worked and that nothing is left in the
    // wrapper map that points into the nursery, and that the hash table entries
    // are discoverable.
    auto key = e.front().key();
    CheckGCThingAfterMovingGC(key);

    auto ptr = crossZoneStringWrappers().lookup(key);
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &e.front());
  }
}
#endif

void Zone::sweepWeakMaps() {
  /* Finalize unreachable (key,value) pairs in all weak maps. */
  WeakMapBase::sweepZone(this);
}

void Zone::discardJitCode(JSFreeOp* fop,
                          ShouldDiscardBaselineCode discardBaselineCode,
                          ShouldDiscardJitScripts discardJitScripts) {
  if (!jitZone()) {
    return;
  }

  if (isPreservingCode()) {
    return;
  }

  if (discardBaselineCode || discardJitScripts) {
#ifdef DEBUG
    // Assert no JitScripts are marked as active.
    for (auto iter = cellIter<JSScript>(); !iter.done(); iter.next()) {
      JSScript* script = iter.unbarrieredGet();
      if (jit::JitScript* jitScript = script->maybeJitScript()) {
        MOZ_ASSERT(!jitScript->active());
      }
    }
#endif

    // Mark JitScripts on the stack as active.
    jit::MarkActiveJitScripts(this);
  }

  // Invalidate all Ion code in this zone.
  jit::InvalidateAll(fop, this);

  for (auto script = cellIterUnsafe<JSScript>(); !script.done();
       script.next()) {
    jit::JitScript* jitScript = script->maybeJitScript();
    if (!jitScript) {
      continue;
    }

    jit::FinishInvalidation(fop, script);

    // Discard baseline script if it's not marked as active.
    if (discardBaselineCode) {
      if (jitScript->hasBaselineScript() && !jitScript->active()) {
        jit::FinishDiscardBaselineScript(fop, script);
      }
    }

    // Warm-up counter for scripts are reset on GC. After discarding code we
    // need to let it warm back up to get information such as which
    // opcodes are setting array holes or accessing getter properties.
    script->resetWarmUpCounterForGC();

    // Try to release the script's JitScript. This should happen after
    // releasing JIT code because we can't do this when the script still has
    // JIT code.
    if (discardJitScripts) {
      script->maybeReleaseJitScript(fop);
      jitScript = script->maybeJitScript();
      if (!jitScript) {
        continue;
      }
    }

    // If we did not release the JitScript, we need to purge optimized IC
    // stubs because the optimizedStubSpace will be purged below.
    if (discardBaselineCode) {
      jitScript->purgeOptimizedStubs(script);

      // ICs were purged so the script will need to warm back up before it can
      // be inlined during Ion compilation.
      jitScript->clearIonCompiledOrInlined();
    }

    // Clear the JitScript's control flow graph. The LifoAlloc is purged
    // below.
    jitScript->clearControlFlowGraph();

    // Finally, reset the active flag.
    jitScript->resetActive();
  }

  /*
   * When scripts contains pointers to nursery things, the store buffer
   * can contain entries that point into the optimized stub space. Since
   * this method can be called outside the context of a GC, this situation
   * could result in us trying to mark invalid store buffer entries.
   *
   * Defer freeing any allocated blocks until after the next minor GC.
   */
  if (discardBaselineCode) {
    jitZone()->optimizedStubSpace()->freeAllAfterMinorGC(this);
    jitZone()->purgeIonCacheIRStubInfo();
  }

  /*
   * Free all control flow graphs that are cached on BaselineScripts.
   * Assuming this happens on the main thread and all control flow
   * graph reads happen on the main thread, this is safe.
   */
  jitZone()->cfgSpace()->lifoAlloc().freeAll();
}

#ifdef JSGC_HASH_TABLE_CHECKS
void JS::Zone::checkUniqueIdTableAfterMovingGC() {
  for (auto r = uniqueIds().all(); !r.empty(); r.popFront()) {
    js::gc::CheckGCThingAfterMovingGC(r.front().key());
  }
}
#endif

uint64_t Zone::gcNumber() {
  // Zones in use by exclusive threads are not collected, and threads using
  // them cannot access the main runtime's gcNumber without racing.
  return usedByHelperThread() ? 0 : runtimeFromMainThread()->gc.gcNumber();
}

js::jit::JitZone* Zone::createJitZone(JSContext* cx) {
  MOZ_ASSERT(!jitZone_);
  MOZ_ASSERT(cx->runtime()->hasJitRuntime());

  UniquePtr<jit::JitZone> jitZone(cx->new_<js::jit::JitZone>());
  if (!jitZone) {
    return nullptr;
  }

  jitZone_ = jitZone.release();
  return jitZone_;
}

bool Zone::hasMarkedRealms() {
  for (RealmsInZoneIter realm(this); !realm.done(); realm.next()) {
    if (realm->marked()) {
      return true;
    }
  }
  return false;
}

bool Zone::canCollect() {
  // The atoms zone cannot be collected while off-thread parsing is taking
  // place.
  if (isAtomsZone()) {
    return !runtimeFromAnyThread()->hasHelperThreadZones();
  }

  // Zones that will be or are currently used by other threads cannot be
  // collected.
  return !createdForHelperThread();
}

void Zone::notifyObservingDebuggers() {
  MOZ_ASSERT(JS::RuntimeHeapIsCollecting(),
             "This method should be called during GC.");

  JSRuntime* rt = runtimeFromMainThread();
  JSContext* cx = rt->mainContextFromOwnThread();

  for (RealmsInZoneIter realms(this); !realms.done(); realms.next()) {
    RootedGlobalObject global(cx, realms->unsafeUnbarrieredMaybeGlobal());
    if (!global) {
      continue;
    }

    DebugAPI::notifyParticipatesInGC(global, rt->gc.majorGCCount());
  }
}

bool Zone::isOnList() const { return listNext_ != NotOnList; }

Zone* Zone::nextZone() const {
  MOZ_ASSERT(isOnList());
  return listNext_;
}

void Zone::clearTables() {
  MOZ_ASSERT(regExps().empty());

  baseShapes().clear();
  initialShapes().clear();
}

void Zone::fixupAfterMovingGC() {
  ZoneAllocator::fixupAfterMovingGC();
  fixupInitialShapeTable();
}

bool Zone::addTypeDescrObject(JSContext* cx, HandleObject obj) {
  // Type descriptor objects are always tenured so we don't need post barriers
  // on the set.
  MOZ_ASSERT(!IsInsideNursery(obj));

  if (!typeDescrObjects().put(obj)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

void Zone::deleteEmptyCompartment(JS::Compartment* comp) {
  MOZ_ASSERT(comp->zone() == this);
  arenas.checkEmptyArenaLists();

  MOZ_ASSERT(compartments().length() == 1);
  MOZ_ASSERT(compartments()[0] == comp);
  MOZ_ASSERT(comp->realms().length() == 1);

  Realm* realm = comp->realms()[0];
  JSFreeOp* fop = runtimeFromMainThread()->defaultFreeOp();
  realm->destroy(fop);
  comp->destroy(fop);

  compartments().clear();
}

void Zone::setHelperThreadOwnerContext(JSContext* cx) {
  MOZ_ASSERT_IF(cx, TlsContext.get() == cx);
  helperThreadOwnerContext_ = cx;
}

bool Zone::ownedByCurrentHelperThread() {
  MOZ_ASSERT(usedByHelperThread());
  MOZ_ASSERT(TlsContext.get());
  return helperThreadOwnerContext_ == TlsContext.get();
}

void Zone::releaseAtoms() {
  MOZ_ASSERT(hasKeptAtoms());

  keepAtomsCount--;

  if (!hasKeptAtoms() && purgeAtomsDeferred) {
    purgeAtomsDeferred = false;
    purgeAtomCache();
  }
}

void Zone::purgeAtomCacheOrDefer() {
  if (hasKeptAtoms()) {
    purgeAtomsDeferred = true;
    return;
  }

  purgeAtomCache();
}

void Zone::purgeAtomCache() {
  MOZ_ASSERT(!hasKeptAtoms());
  MOZ_ASSERT(!purgeAtomsDeferred);

  atomCache().clearAndCompact();

  // Also purge the dtoa caches so that subsequent lookups populate atom
  // cache too.
  for (RealmsInZoneIter r(this); !r.done(); r.next()) {
    r->dtoaCache.purge();
  }
}

void Zone::traceAtomCache(JSTracer* trc) {
  MOZ_ASSERT(hasKeptAtoms());
  for (auto r = atomCache().all(); !r.empty(); r.popFront()) {
    JSAtom* atom = r.front().asPtrUnbarriered();
    TraceRoot(trc, &atom, "kept atom");
    MOZ_ASSERT(r.front().asPtrUnbarriered() == atom);
  }
}

void Zone::addSizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf, size_t* typePool, size_t* regexpZone,
    size_t* jitZone, size_t* baselineStubsOptimized, size_t* cachedCFG,
    size_t* uniqueIdMap, size_t* shapeCaches, size_t* atomsMarkBitmaps,
    size_t* compartmentObjects, size_t* crossCompartmentWrappersTables,
    size_t* compartmentsPrivateData) {
  *typePool += types.typeLifoAlloc().sizeOfExcludingThis(mallocSizeOf);
  *regexpZone += regExps().sizeOfExcludingThis(mallocSizeOf);
  if (jitZone_) {
    jitZone_->addSizeOfIncludingThis(mallocSizeOf, jitZone,
                                     baselineStubsOptimized, cachedCFG);
  }
  *uniqueIdMap += uniqueIds().shallowSizeOfExcludingThis(mallocSizeOf);
  *shapeCaches += baseShapes().sizeOfExcludingThis(mallocSizeOf) +
                  initialShapes().sizeOfExcludingThis(mallocSizeOf);
  *atomsMarkBitmaps += markedAtoms().sizeOfExcludingThis(mallocSizeOf);
  *crossCompartmentWrappersTables +=
      crossZoneStringWrappers().sizeOfExcludingThis(mallocSizeOf);

  for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next()) {
    comp->addSizeOfIncludingThis(mallocSizeOf, compartmentObjects,
                                 crossCompartmentWrappersTables,
                                 compartmentsPrivateData);
  }
}

void* ZoneAllocator::onOutOfMemory(js::AllocFunction allocFunc,
                                   arena_id_t arena, size_t nbytes,
                                   void* reallocPtr) {
  if (!js::CurrentThreadCanAccessRuntime(runtime_)) {
    return nullptr;
  }
  return runtimeFromMainThread()->onOutOfMemory(allocFunc, arena, nbytes,
                                                reallocPtr);
}

void ZoneAllocator::reportAllocationOverflow() const {
  js::ReportAllocationOverflow(nullptr);
}

#ifdef DEBUG

void MemoryTracker::adopt(MemoryTracker& other) {
  LockGuard<Mutex> lock(mutex);

  AutoEnterOOMUnsafeRegion oomUnsafe;

  for (auto r = other.map.all(); !r.empty(); r.popFront()) {
    if (!map.put(r.front().key(), r.front().value())) {
      oomUnsafe.crash("MemoryTracker::adopt");
    }
  }
  other.map.clear();

  // There may still be ZoneAllocPolicies associated with the old zone since
  // some are not destroyed until the zone itself dies. Instead check there is
  // no memory associated with them and clear their zone pointer in debug builds
  // to catch further memory association.
  for (auto r = other.policyMap.all(); !r.empty(); r.popFront()) {
    MOZ_ASSERT(r.front().value() == 0);
    r.front().key()->zone_ = nullptr;
  }
  other.policyMap.clear();
}

static const char* MemoryUseName(MemoryUse use) {
  switch (use) {
#  define DEFINE_CASE(Name) \
    case MemoryUse::Name:   \
      return #Name;
    JS_FOR_EACH_MEMORY_USE(DEFINE_CASE)
#  undef DEFINE_CASE
  }

  MOZ_CRASH("Unknown memory use");
}

MemoryTracker::MemoryTracker() : mutex(mutexid::MemoryTracker) {}

void MemoryTracker::checkEmptyOnDestroy() {
  bool ok = true;

  if (!map.empty()) {
    ok = false;
    fprintf(stderr, "Missing calls to JS::RemoveAssociatedMemory:\n");
    for (auto r = map.all(); !r.empty(); r.popFront()) {
      fprintf(stderr, "  %p 0x%zx %s\n", r.front().key().cell(),
              r.front().value(), MemoryUseName(r.front().key().use()));
    }
  }

  if (!policyMap.empty()) {
    ok = false;
    fprintf(stderr, "Missing calls to Zone::decPolicyMemory:\n");
    for (auto r = policyMap.all(); !r.empty(); r.popFront()) {
      fprintf(stderr, "  %p 0x%zx\n", r.front().key(), r.front().value());
    }
  }

  MOZ_ASSERT(ok);
}

inline bool MemoryTracker::allowMultipleAssociations(MemoryUse use) const {
  // For most uses only one association is possible for each GC thing. Allow a
  // one-to-many relationship only where necessary.
  return use == MemoryUse::RegExpSharedBytecode ||
         use == MemoryUse::BreakpointSite || use == MemoryUse::Breakpoint ||
         use == MemoryUse::ForOfPICStub;
}

void MemoryTracker::trackMemory(Cell* cell, size_t nbytes, MemoryUse use) {
  MOZ_ASSERT(cell->isTenured());

  LockGuard<Mutex> lock(mutex);

  Key key{cell, use};
  AutoEnterOOMUnsafeRegion oomUnsafe;
  auto ptr = map.lookupForAdd(key);
  if (ptr) {
    if (!allowMultipleAssociations(use)) {
      MOZ_CRASH_UNSAFE_PRINTF("Association already present: %p 0x%zx %s", cell,
                              nbytes, MemoryUseName(use));
    }
    ptr->value() += nbytes;
    return;
  }

  if (!map.add(ptr, key, nbytes)) {
    oomUnsafe.crash("MemoryTracker::noteExternalAlloc");
  }
}

void MemoryTracker::untrackMemory(Cell* cell, size_t nbytes, MemoryUse use) {
  MOZ_ASSERT(cell->isTenured());

  LockGuard<Mutex> lock(mutex);

  Key key{cell, use};
  auto ptr = map.lookup(key);
  if (!ptr) {
    MOZ_CRASH_UNSAFE_PRINTF("Association not found: %p 0x%zx %s", cell, nbytes,
                            MemoryUseName(use));
  }

  if (!allowMultipleAssociations(use) && ptr->value() != nbytes) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Association for %p %s has different size: "
        "expected 0x%zx but got 0x%zx",
        cell, MemoryUseName(use), ptr->value(), nbytes);
  }

  if (ptr->value() < nbytes) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Association for %p %s size is too small: "
        "expected at least 0x%zx but got 0x%zx",
        cell, MemoryUseName(use), nbytes, ptr->value());
  }

  ptr->value() -= nbytes;

  if (ptr->value() == 0) {
    map.remove(ptr);
  }
}

void MemoryTracker::swapMemory(Cell* a, Cell* b, MemoryUse use) {
  MOZ_ASSERT(a->isTenured());
  MOZ_ASSERT(b->isTenured());

  Key ka{a, use};
  Key kb{b, use};

  LockGuard<Mutex> lock(mutex);

  size_t sa = getAndRemoveEntry(ka, lock);
  size_t sb = getAndRemoveEntry(kb, lock);

  AutoEnterOOMUnsafeRegion oomUnsafe;

  if ((sa && !map.put(kb, sa)) || (sb && !map.put(ka, sb))) {
    oomUnsafe.crash("MemoryTracker::swapTrackedMemory");
  }
}

size_t MemoryTracker::getAndRemoveEntry(const Key& key,
                                        LockGuard<Mutex>& lock) {
  auto ptr = map.lookup(key);
  if (!ptr) {
    return 0;
  }

  size_t size = ptr->value();
  map.remove(ptr);
  return size;
}

void MemoryTracker::registerPolicy(ZoneAllocPolicy* policy) {
  LockGuard<Mutex> lock(mutex);

  auto ptr = policyMap.lookupForAdd(policy);
  if (ptr) {
    MOZ_CRASH_UNSAFE_PRINTF("ZoneAllocPolicy %p already registered", policy);
  }

  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!policyMap.add(ptr, policy, 0)) {
    oomUnsafe.crash("MemoryTracker::registerPolicy");
  }
}

void MemoryTracker::unregisterPolicy(ZoneAllocPolicy* policy) {
  LockGuard<Mutex> lock(mutex);

  auto ptr = policyMap.lookup(policy);
  if (!ptr) {
    MOZ_CRASH_UNSAFE_PRINTF("ZoneAllocPolicy %p not found", policy);
  }
  if (ptr->value() != 0) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "ZoneAllocPolicy %p still has 0x%zx bytes associated", policy,
        ptr->value());
  }

  policyMap.remove(ptr);
}

void MemoryTracker::movePolicy(ZoneAllocPolicy* dst, ZoneAllocPolicy* src) {
  LockGuard<Mutex> lock(mutex);

  auto srcPtr = policyMap.lookup(src);
  if (!srcPtr) {
    MOZ_CRASH_UNSAFE_PRINTF("ZoneAllocPolicy %p not found", src);
  }

  size_t nbytes = srcPtr->value();
  policyMap.remove(srcPtr);

  auto dstPtr = policyMap.lookupForAdd(dst);
  if (dstPtr) {
    MOZ_CRASH_UNSAFE_PRINTF("ZoneAllocPolicy %p already registered", dst);
  }

  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!policyMap.add(dstPtr, dst, nbytes)) {
    oomUnsafe.crash("MemoryTracker::movePolicy");
  }
}

void MemoryTracker::incPolicyMemory(ZoneAllocPolicy* policy, size_t nbytes) {
  LockGuard<Mutex> lock(mutex);

  auto ptr = policyMap.lookup(policy);
  if (!ptr) {
    MOZ_CRASH_UNSAFE_PRINTF("ZoneAllocPolicy %p not found", policy);
  }

  ptr->value() += nbytes;
}

void MemoryTracker::decPolicyMemory(ZoneAllocPolicy* policy, size_t nbytes) {
  LockGuard<Mutex> lock(mutex);

  auto ptr = policyMap.lookup(policy);
  if (!ptr) {
    MOZ_CRASH_UNSAFE_PRINTF("ZoneAllocPolicy %p not found", policy);
  }

  size_t& value = ptr->value();
  if (value < nbytes) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "ZoneAllocPolicy %p is too small: "
        "expected at least 0x%zx but got 0x%zx bytes",
        policy, nbytes, value);
  }

  value -= nbytes;
}

void MemoryTracker::fixupAfterMovingGC() {
  // Update the table after we move GC things. We don't use MovableCellHasher
  // because that would create a difference between debug and release builds.
  for (Map::Enum e(map); !e.empty(); e.popFront()) {
    const Key& key = e.front().key();
    Cell* cell = key.cell();
    if (cell->isForwarded()) {
      cell = gc::RelocationOverlay::fromCell(cell)->forwardingAddress();
      e.rekeyFront(Key{cell, key.use()});
    }
  }
}

inline MemoryTracker::Key::Key(Cell* cell, MemoryUse use)
    : cell_(uint64_t(cell)), use_(uint64_t(use)) {
#  ifdef JS_64BIT
  static_assert(sizeof(Key) == 8,
                "MemoryTracker::Key should be packed into 8 bytes");
#  endif
  MOZ_ASSERT(this->cell() == cell);
  MOZ_ASSERT(this->use() == use);
}

inline Cell* MemoryTracker::Key::cell() const {
  return reinterpret_cast<Cell*>(cell_);
}
inline MemoryUse MemoryTracker::Key::use() const {
  return static_cast<MemoryUse>(use_);
}

inline HashNumber MemoryTracker::Hasher::hash(const Lookup& l) {
  return mozilla::HashGeneric(DefaultHasher<Cell*>::hash(l.cell()),
                              DefaultHasher<unsigned>::hash(unsigned(l.use())));
}

inline bool MemoryTracker::Hasher::match(const Key& k, const Lookup& l) {
  return k.cell() == l.cell() && k.use() == l.use();
}

inline void MemoryTracker::Hasher::rekey(Key& k, const Key& newKey) {
  k = newKey;
}

#endif  // DEBUG

ZoneList::ZoneList() : head(nullptr), tail(nullptr) {}

ZoneList::ZoneList(Zone* zone) : head(zone), tail(zone) {
  MOZ_RELEASE_ASSERT(!zone->isOnList());
  zone->listNext_ = nullptr;
}

ZoneList::~ZoneList() { MOZ_ASSERT(isEmpty()); }

void ZoneList::check() const {
#ifdef DEBUG
  MOZ_ASSERT((head == nullptr) == (tail == nullptr));
  if (!head) {
    return;
  }

  Zone* zone = head;
  for (;;) {
    MOZ_ASSERT(zone && zone->isOnList());
    if (zone == tail) break;
    zone = zone->listNext_;
  }
  MOZ_ASSERT(!zone->listNext_);
#endif
}

bool ZoneList::isEmpty() const { return head == nullptr; }

Zone* ZoneList::front() const {
  MOZ_ASSERT(!isEmpty());
  MOZ_ASSERT(head->isOnList());
  return head;
}

void ZoneList::append(Zone* zone) {
  ZoneList singleZone(zone);
  transferFrom(singleZone);
}

void ZoneList::transferFrom(ZoneList& other) {
  check();
  other.check();
  if (!other.head) {
    return;
  }

  MOZ_ASSERT(tail != other.tail);

  if (tail) {
    tail->listNext_ = other.head;
  } else {
    head = other.head;
  }
  tail = other.tail;

  other.head = nullptr;
  other.tail = nullptr;
}

Zone* ZoneList::removeFront() {
  MOZ_ASSERT(!isEmpty());
  check();

  Zone* front = head;
  head = head->listNext_;
  if (!head) {
    tail = nullptr;
  }

  front->listNext_ = Zone::NotOnList;

  return front;
}

void ZoneList::clear() {
  while (!isEmpty()) {
    removeFront();
  }
}

JS_PUBLIC_API void JS::shadow::RegisterWeakCache(
    JS::Zone* zone, detail::WeakCacheBase* cachep) {
  zone->registerWeakCache(cachep);
}
