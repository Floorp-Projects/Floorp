/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone-inl.h"
#include "js/shadow/Zone.h"  // JS::shadow::Zone

#include <type_traits>

#include "gc/FreeOp.h"
#include "gc/GCLock.h"
#include "gc/Policy.h"
#include "gc/PublicIterators.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/Invalidation.h"
#include "jit/Ion.h"
#include "jit/JitZone.h"
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

ZoneAllocator::ZoneAllocator(JSRuntime* rt, Kind kind)
    : JS::shadow::Zone(rt, &rt->gc.barrierTracer, kind),
      gcHeapSize(&rt->gc.heapSize),
      mallocHeapSize(nullptr),
      jitHeapSize(nullptr),
      jitHeapThreshold(jit::MaxCodeBytesPerProcess * 0.8) {}

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

void js::ZoneAllocator::updateGCStartThresholds(
    GCRuntime& gc, JSGCInvocationKind invocationKind,
    const js::AutoLockGC& lock) {
  bool isAtomsZone = JS::Zone::from(this)->isAtomsZone();
  gcHeapThreshold.updateStartThreshold(gcHeapSize.retainedBytes(),
                                       invocationKind, gc.tunables,
                                       gc.schedulingState, isAtomsZone, lock);
  mallocHeapThreshold.updateStartThreshold(mallocHeapSize.retainedBytes(),
                                           gc.tunables, lock);
}

void js::ZoneAllocator::setGCSliceThresholds(GCRuntime& gc) {
  gcHeapThreshold.setSliceThreshold(this, gcHeapSize, gc.tunables);
  mallocHeapThreshold.setSliceThreshold(this, mallocHeapSize, gc.tunables);
  jitHeapThreshold.setSliceThreshold(this, jitHeapSize, gc.tunables);
}

void js::ZoneAllocator::clearGCSliceThresholds() {
  gcHeapThreshold.clearSliceThreshold();
  mallocHeapThreshold.clearSliceThreshold();
  jitHeapThreshold.clearSliceThreshold();
}

bool ZoneAllocator::addSharedMemory(void* mem, size_t nbytes, MemoryUse use) {
  // nbytes can be zero here for SharedArrayBuffers.

  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));

  auto ptr = sharedMemoryUseCounts.lookupForAdd(mem);
  MOZ_ASSERT_IF(ptr, ptr->value().use == use);

  if (!ptr && !sharedMemoryUseCounts.add(ptr, mem, gc::SharedMemoryUse(use))) {
    return false;
  }

  ptr->value().count++;

  // Allocations can grow, so add any increase over the previous size and record
  // the new size.
  if (nbytes > ptr->value().nbytes) {
    mallocHeapSize.addBytes(nbytes - ptr->value().nbytes);
    ptr->value().nbytes = nbytes;
  }

  maybeTriggerGCOnMalloc();

  return true;
}

void ZoneAllocator::removeSharedMemory(void* mem, size_t nbytes,
                                       MemoryUse use) {
  // nbytes can be zero here for SharedArrayBuffers.

  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
  MOZ_ASSERT(CurrentThreadIsGCFinalizing());

  auto ptr = sharedMemoryUseCounts.lookup(mem);

  MOZ_ASSERT(ptr);
  MOZ_ASSERT(ptr->value().count != 0);
  MOZ_ASSERT(ptr->value().use == use);
  MOZ_ASSERT(ptr->value().nbytes >= nbytes);

  ptr->value().count--;
  if (ptr->value().count == 0) {
    mallocHeapSize.removeBytes(ptr->value().nbytes, true);
    sharedMemoryUseCounts.remove(ptr);
  }
}

void ZoneAllocPolicy::decMemory(size_t nbytes) {
  // Unfortunately we don't have enough context here to know whether we're being
  // called on behalf of the collector so we have to do a TLS lookup to find
  // out.
  JSContext* cx = TlsContext.get();
  zone_->decNonGCMemory(this, nbytes, MemoryUse::ZoneAllocPolicy,
                        cx->defaultFreeOp()->isCollecting());
}

JS::Zone::Zone(JSRuntime* rt, Kind kind)
    : ZoneAllocator(rt, kind),
      // Note: don't use |this| before initializing helperThreadUse_!
      // ProtectedData checks in CheckZone::check may read this field.
      helperThreadUse_(HelperThreadUse::None),
      helperThreadOwnerContext_(nullptr),
      arenas(this),
      data(this, nullptr),
      tenuredBigInts(this, 0),
      nurseryAllocatedStrings(this, 0),
      markedStrings(this, 0),
      finalizedStrings(this, 0),
      allocNurseryStrings(this, true),
      allocNurseryBigInts(this, true),
      suppressAllocationMetadataBuilder(this, false),
      previousGCStringStats(this),
      stringStats(this),
      uniqueIds_(this),
      tenuredAllocsSinceMinorGC_(0),
      gcWeakMapList_(this),
      compartments_(),
      crossZoneStringWrappers_(this),
      gcGrayRoots_(this),
      weakCaches_(this),
      gcWeakKeys_(this, SystemAllocPolicy(), rt->randomHashCodeScrambler()),
      gcNurseryWeakKeys_(this, SystemAllocPolicy(),
                         rt->randomHashCodeScrambler()),
      rttValueObjects_(this, this),
      markedAtoms_(this),
      atomCache_(this),
      externalStringCache_(this),
      functionToStringCache_(this),
      propertyTree_(this, this),
      baseShapes_(this, this),
      initialShapes_(this, this),
      finalizationRegistries_(this, this),
      finalizationRecordMap_(this, this),
      jitZone_(this, nullptr),
      gcScheduled_(false),
      gcScheduledSaved_(false),
      gcPreserveCode_(false),
      keepShapeCaches_(this, false),
      wasCollected_(false),
      listNext_(NotOnList),
      weakRefMap_(this, this),
      keptObjects(this, this) {
  /* Ensure that there are no vtables to mess us up here. */
  MOZ_ASSERT(reinterpret_cast<JS::shadow::Zone*>(this) ==
             static_cast<JS::shadow::Zone*>(this));
  MOZ_ASSERT_IF(isAtomsZone(), !rt->unsafeAtomsZone());
  MOZ_ASSERT_IF(isSelfHostingZone(), !rt->hasInitializedSelfHosting());

  // We can't call updateGCStartThresholds until the Zone has been constructed.
  AutoLockGC lock(rt);
  updateGCStartThresholds(rt->gc, GC_NORMAL, lock);
}

Zone::~Zone() {
  MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::None);
  MOZ_ASSERT_IF(regExps_.ref(), regExps().empty());

  DebugAPI::deleteDebugScriptMap(debugScriptMap);

  MOZ_ASSERT(gcWeakMapList().isEmpty());

  JSRuntime* rt = runtimeFromAnyThread();
  if (this == rt->gc.systemZone) {
    MOZ_ASSERT(isSystemZone());
    rt->gc.systemZone = nullptr;
  }

  js_delete(jitZone_.ref());
}

bool Zone::init() {
  regExps_.ref() = make_unique<RegExpZone>(this);
  return regExps_.ref() && gcWeakKeys().init() && gcNurseryWeakKeys().init();
}

void Zone::setNeedsIncrementalBarrier(bool needs) {
  needsIncrementalBarrier_ = needs;
}

void Zone::changeGCState(GCState prev, GCState next) {
  MOZ_ASSERT(RuntimeHeapIsBusy());
  MOZ_ASSERT(canCollect());
  MOZ_ASSERT(gcState() == prev);

  // This can be called when barriers have been temporarily disabled by
  // AutoDisableBarriers. In that case, don't update needsIncrementalBarrier_
  // and barriers will be re-enabled by ~AutoDisableBarriers() if necessary.
  bool barriersDisabled = isGCMarking() && !needsIncrementalBarrier();

  gcState_ = next;

  // Update the barriers state when we transition between marking and
  // non-marking states, unless barriers have been disabled.
  if (!barriersDisabled) {
    needsIncrementalBarrier_ = isGCMarking();
  }
}

template <class Pred>
static void EraseIf(js::gc::WeakEntryVector& entries, Pred pred) {
  auto* begin = entries.begin();
  auto* const end = entries.end();

  auto* newEnd = begin;
  for (auto* p = begin; p != end; p++) {
    if (!pred(*p)) {
      *newEnd++ = *p;
    }
  }

  size_t removed = end - newEnd;
  entries.shrinkBy(removed);
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

    JSObject* delegate = gc::detail::GetDelegate(key->as<JSObject>());
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
    for (auto iter = cellIter<BaseScript>(); !iter.done(); iter.next()) {
      BaseScript* base = iter.unbarrieredGet();
      if (jit::JitScript* jitScript = base->maybeJitScript()) {
        MOZ_ASSERT(!jitScript->active());
      }
    }
#endif

    // Mark JitScripts on the stack as active.
    jit::MarkActiveJitScripts(this);
  }

  // Invalidate all Ion code in this zone.
  jit::InvalidateAll(fop, this);

  for (auto base = cellIterUnsafe<BaseScript>(); !base.done(); base.next()) {
    jit::JitScript* jitScript = base->maybeJitScript();
    if (!jitScript) {
      continue;
    }

    JSScript* script = base->asJSScript();
    jit::FinishInvalidation(fop, script);

    // Discard baseline script if it's not marked as active.
    if (discardBaselineCode) {
      if (jitScript->hasBaselineScript() && !jitScript->active()) {
        jit::FinishDiscardBaselineScript(fop, script);
      }
    }

#ifdef JS_CACHEIR_SPEW
    maybeUpdateWarmUpCount(script);
#endif

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
        // Try to discard the ScriptCounts too.
        if (!script->realm()->collectCoverageForDebug() &&
            !fop->runtime()->profilingScripts) {
          script->destroyScriptCounts();
        }
        continue;
      }
    }

    // If we did not release the JitScript, we need to purge optimized IC
    // stubs because the optimizedStubSpace will be purged below.
    if (discardBaselineCode) {
      jitScript->purgeOptimizedStubs(script);
    }

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
}

void JS::Zone::beforeClearDelegateInternal(JSObject* wrapper,
                                           JSObject* delegate) {
  MOZ_ASSERT(js::gc::detail::GetDelegate(wrapper) == delegate);
  MOZ_ASSERT(needsIncrementalBarrier());
  MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(this));
  runtimeFromMainThread()->gc.marker.severWeakDelegate(wrapper, delegate);
}

void JS::Zone::afterAddDelegateInternal(JSObject* wrapper) {
  MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(this));
  JSObject* delegate = js::gc::detail::GetDelegate(wrapper);
  if (delegate) {
    runtimeFromMainThread()->gc.marker.restoreWeakDelegate(wrapper, delegate);
  }
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

  // We don't collect the self hosting zone after it has been initialized.
  if (isSelfHostingZone()) {
    return !runtimeFromAnyThread()->gc.isSelfHostingZoneFrozen();
  }

  // Zones that will be or are currently used by other threads cannot be
  // collected.
  return !createdForHelperThread();
}

void Zone::notifyObservingDebuggers() {
  AutoAssertNoGC nogc;
  MOZ_ASSERT(JS::RuntimeHeapIsCollecting(),
             "This method should be called during GC.");

  JSRuntime* rt = runtimeFromMainThread();

  for (RealmsInZoneIter realms(this); !realms.done(); realms.next()) {
    GlobalObject* global = realms->unsafeUnbarrieredMaybeGlobal();
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

bool Zone::addRttValueObject(JSContext* cx, HandleObject obj) {
  // Type descriptor objects are always tenured so we don't need post barriers
  // on the set.
  MOZ_ASSERT(!IsInsideNursery(obj));

  if (!rttValueObjects().put(obj)) {
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

void Zone::purgeAtomCache() {
  atomCache().clearAndCompact();

  // Also purge the dtoa caches so that subsequent lookups populate atom
  // cache too.
  for (RealmsInZoneIter r(this); !r.done(); r.next()) {
    r->dtoaCache.purge();
  }
}

void Zone::addSizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf, JS::CodeSizes* code, size_t* regexpZone,
    size_t* jitZone, size_t* baselineStubsOptimized, size_t* uniqueIdMap,
    size_t* shapeCaches, size_t* atomsMarkBitmaps, size_t* compartmentObjects,
    size_t* crossCompartmentWrappersTables, size_t* compartmentsPrivateData,
    size_t* scriptCountsMapArg) {
  *regexpZone += regExps().sizeOfExcludingThis(mallocSizeOf);
  if (jitZone_) {
    jitZone_->addSizeOfIncludingThis(mallocSizeOf, code, jitZone,
                                     baselineStubsOptimized);
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

  if (scriptCountsMap) {
    *scriptCountsMapArg +=
        scriptCountsMap->shallowSizeOfIncludingThis(mallocSizeOf);
    for (auto r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
      *scriptCountsMapArg +=
          r.front().value()->sizeOfIncludingThis(mallocSizeOf);
    }
  }
}

void* ZoneAllocator::onOutOfMemory(js::AllocFunction allocFunc,
                                   arena_id_t arena, size_t nbytes,
                                   void* reallocPtr) {
  if (!js::CurrentThreadCanAccessRuntime(runtime_)) {
    return nullptr;
  }
  // The analysis sees that JSRuntime::onOutOfMemory could report an error,
  // which with a JSErrorInterceptor could GC. But we're passing a null cx (to
  // a default parameter) so the error will not be reported.
  JS::AutoSuppressGCAnalysis suppress;
  return runtimeFromMainThread()->onOutOfMemory(allocFunc, arena, nbytes,
                                                reallocPtr);
}

void ZoneAllocator::reportAllocationOverflow() const {
  js::ReportAllocationOverflow(nullptr);
}

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

void Zone::traceScriptTableRoots(JSTracer* trc) {
  static_assert(std::is_convertible_v<BaseScript*, gc::TenuredCell*>,
                "BaseScript must not be nursery-allocated for script-table "
                "tracing to work");

  // Performance optimization: the script-table keys are JSScripts, which
  // cannot be in the nursery, so we can skip this tracing if we are only in a
  // minor collection. We static-assert this fact above.
  if (JS::RuntimeHeapIsMinorCollecting()) {
    return;
  }

  // N.B.: the script-table keys are weak *except* in an exceptional case: when
  // then --dump-bytecode command line option or the PCCount JSFriend API is
  // used, then the scripts for all counts must remain alive. We only trace
  // when the `trc->runtime()->profilingScripts` flag is set. This flag is
  // cleared in JSRuntime::destroyRuntime() during shutdown to ensure that
  // scripts are collected before the runtime goes away completely.
  if (scriptCountsMap && trc->runtime()->profilingScripts) {
    for (ScriptCountsMap::Range r = scriptCountsMap->all(); !r.empty();
         r.popFront()) {
      BaseScript* script = const_cast<BaseScript*>(r.front().key());
      MOZ_ASSERT(script->hasScriptCounts());
      TraceRoot(trc, &script, "profilingScripts");
      MOZ_ASSERT(script == r.front().key(), "const_cast is only a work-around");
    }
  }

  // Trace the debugger's DebugScript weak map.
  if (debugScriptMap) {
    DebugAPI::traceDebugScriptMap(trc, debugScriptMap);
  }
}

void Zone::fixupScriptMapsAfterMovingGC(JSTracer* trc) {
  // Map entries are removed by BaseScript::finalize, but we need to update the
  // script pointers here in case they are moved by the GC.

  if (scriptCountsMap) {
    for (ScriptCountsMap::Enum e(*scriptCountsMap); !e.empty(); e.popFront()) {
      BaseScript* script = e.front().key();
      TraceManuallyBarrieredEdge(trc, &script, "Realm::scriptCountsMap::key");
      if (script != e.front().key()) {
        e.rekeyFront(script);
      }
    }
  }

  if (scriptLCovMap) {
    for (ScriptLCovMap::Enum e(*scriptLCovMap); !e.empty(); e.popFront()) {
      BaseScript* script = e.front().key();
      if (!IsAboutToBeFinalizedUnbarriered(&script) &&
          script != e.front().key()) {
        e.rekeyFront(script);
      }
    }
  }

#ifdef MOZ_VTUNE
  if (scriptVTuneIdMap) {
    for (ScriptVTuneIdMap::Enum e(*scriptVTuneIdMap); !e.empty();
         e.popFront()) {
      BaseScript* script = e.front().key();
      if (!IsAboutToBeFinalizedUnbarriered(&script) &&
          script != e.front().key()) {
        e.rekeyFront(script);
      }
    }
  }
#endif

#ifdef JS_CACHEIR_SPEW
  if (scriptFinalWarmUpCountMap) {
    for (ScriptFinalWarmUpCountMap::Enum e(*scriptFinalWarmUpCountMap);
         !e.empty(); e.popFront()) {
      BaseScript* script = e.front().key();
      if (!IsAboutToBeFinalizedUnbarriered(&script) &&
          script != e.front().key()) {
        e.rekeyFront(script);
      }
    }
  }
#endif
}

#ifdef JSGC_HASH_TABLE_CHECKS
void Zone::checkScriptMapsAfterMovingGC() {
  if (scriptCountsMap) {
    for (auto r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
      BaseScript* script = r.front().key();
      MOZ_ASSERT(script->zone() == this);
      CheckGCThingAfterMovingGC(script);
      auto ptr = scriptCountsMap->lookup(script);
      MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
    }
  }

  if (scriptLCovMap) {
    for (auto r = scriptLCovMap->all(); !r.empty(); r.popFront()) {
      BaseScript* script = r.front().key();
      MOZ_ASSERT(script->zone() == this);
      CheckGCThingAfterMovingGC(script);
      auto ptr = scriptLCovMap->lookup(script);
      MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
    }
  }

#  ifdef MOZ_VTUNE
  if (scriptVTuneIdMap) {
    for (auto r = scriptVTuneIdMap->all(); !r.empty(); r.popFront()) {
      BaseScript* script = r.front().key();
      MOZ_ASSERT(script->zone() == this);
      CheckGCThingAfterMovingGC(script);
      auto ptr = scriptVTuneIdMap->lookup(script);
      MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
    }
  }
#  endif  // MOZ_VTUNE

#  ifdef JS_CACHEIR_SPEW
  if (scriptFinalWarmUpCountMap) {
    for (auto r = scriptFinalWarmUpCountMap->all(); !r.empty(); r.popFront()) {
      BaseScript* script = r.front().key();
      MOZ_ASSERT(script->zone() == this);
      CheckGCThingAfterMovingGC(script);
      auto ptr = scriptFinalWarmUpCountMap->lookup(script);
      MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
    }
  }
#  endif  // JS_CACHEIR_SPEW
}
#endif

void Zone::clearScriptCounts(Realm* realm) {
  if (!scriptCountsMap) {
    return;
  }

  // Clear all hasScriptCounts_ flags of BaseScript, in order to release all
  // ScriptCounts entries of the given realm.
  for (auto i = scriptCountsMap->modIter(); !i.done(); i.next()) {
    BaseScript* script = i.get().key();
    if (script->realm() != realm) {
      continue;
    }
    // We can't destroy the ScriptCounts yet if the script has Baseline code,
    // because Baseline code bakes in pointers to the counters. The ScriptCounts
    // will be destroyed in Zone::discardJitCode when discarding the JitScript.
    if (script->hasBaselineScript()) {
      continue;
    }
    script->clearHasScriptCounts();
    i.remove();
  }
}

void Zone::clearScriptLCov(Realm* realm) {
  if (!scriptLCovMap) {
    return;
  }

  for (auto i = scriptLCovMap->modIter(); !i.done(); i.next()) {
    BaseScript* script = i.get().key();
    if (script->realm() == realm) {
      i.remove();
    }
  }
}

void Zone::clearRootsForShutdownGC() {
  // Finalization callbacks are not called if we're shutting down.
  finalizationRecordMap().clear();

  clearKeptObjects();
}

void Zone::finishRoots() {
  for (RealmsInZoneIter r(this); !r.done(); r.next()) {
    r->finishRoots();
  }
}

void Zone::traceKeptObjects(JSTracer* trc) { keptObjects.ref().trace(trc); }

bool Zone::keepDuringJob(HandleObject target) {
  return keptObjects.ref().put(target);
}

void Zone::clearKeptObjects() { keptObjects.ref().clear(); }
