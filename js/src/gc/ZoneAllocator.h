/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Public header for allocating memory associated with GC things.
 */

#ifndef gc_ZoneAllocator_h
#define gc_ZoneAllocator_h

#include "gc/Scheduling.h"
#include "vm/Runtime.h"  // For JSRuntime::gc.

namespace JS {
class Zone;
}  // namespace JS

namespace js {

namespace gc {
void MaybeMallocTriggerZoneGC(JSRuntime* rt, ZoneAllocator* zoneAlloc);
}

// Base class of JS::Zone that provides malloc memory allocation and accounting.
class ZoneAllocator : public JS::shadow::Zone,
                      public js::MallocProvider<JS::Zone> {
 protected:
  explicit ZoneAllocator(JSRuntime* rt);
  ~ZoneAllocator();
  void fixupAfterMovingGC();

 public:
  static ZoneAllocator* from(JS::Zone* zone) {
    // This is a safe downcast, but the compiler hasn't seen the definition yet.
    return reinterpret_cast<ZoneAllocator*>(zone);
  }

  MOZ_MUST_USE void* onOutOfMemory(js::AllocFunction allocFunc,
                                   arena_id_t arena, size_t nbytes,
                                   void* reallocPtr = nullptr);
  void reportAllocationOverflow() const;

  void setGCMaxMallocBytes(size_t value, const js::AutoLockGC& lock) {
    gcMallocCounter.setMax(value, lock);
  }
  void updateMallocCounter(size_t nbytes) {
    updateMemoryCounter(gcMallocCounter, nbytes);
  }
  void adoptMallocBytes(ZoneAllocator* other) {
    gcMallocCounter.adopt(other->gcMallocCounter);
    gcMallocBytes.adopt(other->gcMallocBytes);
#ifdef DEBUG
    gcMallocTracker.adopt(other->gcMallocTracker);
#endif
  }
  size_t GCMaxMallocBytes() const { return gcMallocCounter.maxBytes(); }
  size_t GCMallocBytes() const { return gcMallocCounter.bytes(); }

  void updateJitCodeMallocBytes(size_t nbytes) {
    updateMemoryCounter(jitCodeCounter, nbytes);
  }

  void updateAllGCMallocCountersOnGCStart();
  void updateAllGCMallocCountersOnGCEnd(const js::AutoLockGC& lock);
  void updateAllGCThresholds(gc::GCRuntime& gc,
                             JSGCInvocationKind invocationKind,
                             const js::AutoLockGC& lock);
  js::gc::TriggerKind shouldTriggerGCForTooMuchMalloc();

  // Memory accounting APIs for malloc memory owned by GC cells.

  void addCellMemory(js::gc::Cell* cell, size_t nbytes, js::MemoryUse use) {
    MOZ_ASSERT(cell);
    MOZ_ASSERT(nbytes);
    gcMallocBytes.addBytes(nbytes);

    // We don't currently check GC triggers here.

#ifdef DEBUG
    gcMallocTracker.trackMemory(cell, nbytes, use);
#endif
  }

  void removeCellMemory(js::gc::Cell* cell, size_t nbytes, js::MemoryUse use) {
    MOZ_ASSERT(cell);
    MOZ_ASSERT(nbytes);
    gcMallocBytes.removeBytes(nbytes);

#ifdef DEBUG
    gcMallocTracker.untrackMemory(cell, nbytes, use);
#endif
  }

  void swapCellMemory(js::gc::Cell* a, js::gc::Cell* b, js::MemoryUse use) {
#ifdef DEBUG
    gcMallocTracker.swapMemory(a, b, use);
#endif
  }

#ifdef DEBUG
  void registerPolicy(js::ZoneAllocPolicy* policy) {
    return gcMallocTracker.registerPolicy(policy);
  }
  void unregisterPolicy(js::ZoneAllocPolicy* policy) {
    return gcMallocTracker.unregisterPolicy(policy);
  }
#endif

  void incPolicyMemory(js::ZoneAllocPolicy* policy, size_t nbytes) {
    MOZ_ASSERT(nbytes);
    gcMallocBytes.addBytes(nbytes);

#ifdef DEBUG
    gcMallocTracker.incPolicyMemory(policy, nbytes);
#endif

    maybeMallocTriggerZoneGC();
  }
  void decPolicyMemory(js::ZoneAllocPolicy* policy, size_t nbytes) {
    MOZ_ASSERT(nbytes);
    gcMallocBytes.removeBytes(nbytes);

#ifdef DEBUG
    gcMallocTracker.decPolicyMemory(policy, nbytes);
#endif
  }

  // Check malloc allocation threshold and trigger a zone GC if necessary.
  void maybeMallocTriggerZoneGC() {
    JSRuntime* rt = runtimeFromAnyThread();
    if (gcMallocBytes.gcBytes() >= gcMallocThreshold.gcTriggerBytes() &&
        rt->heapState() == JS::HeapState::Idle) {
      gc::MaybeMallocTriggerZoneGC(rt, this);
    }
  }

 private:
  void updateMemoryCounter(js::gc::MemoryCounter& counter, size_t nbytes) {
    JSRuntime* rt = runtimeFromAnyThread();

    counter.update(nbytes);
    auto trigger = counter.shouldTriggerGC(rt->gc.tunables);
    if (MOZ_LIKELY(trigger == js::gc::NoTrigger) ||
        trigger <= counter.triggered()) {
      return;
    }

    maybeTriggerGCForTooMuchMalloc(counter, trigger);
  }

  void maybeTriggerGCForTooMuchMalloc(js::gc::MemoryCounter& counter,
                                      js::gc::TriggerKind trigger);

 public:
  // Track GC heap size under this Zone.
  js::gc::HeapSize zoneSize;

  // Thresholds used to trigger GC based on heap size.
  js::gc::ZoneHeapThreshold threshold;

  // Amount of data to allocate before triggering a new incremental slice for
  // the current GC.
  js::MainThreadData<size_t> gcDelayBytes;

 private:
  // Malloc counter to measure memory pressure for GC scheduling. This counter
  // is used for allocations where the size of the allocation is not known on
  // free. Currently this is used for all internal malloc allocations.
  js::gc::MemoryCounter gcMallocCounter;

 public:
  // Malloc counter used for allocations where size information is
  // available. Used for some internal and all tracked external allocations.
  js::gc::HeapSize gcMallocBytes;

  // Thresholds used to trigger GC based on malloc allocations.
  js::gc::ZoneMallocThreshold gcMallocThreshold;

 private:
#ifdef DEBUG
  // In debug builds, malloc allocations can be tracked to make debugging easier
  // (possible?) if allocation and free sizes don't balance.
  js::gc::MemoryTracker gcMallocTracker;
#endif

  // Counter of JIT code executable memory for GC scheduling. Also imprecise,
  // since wasm can generate code that outlives a zone.
  js::gc::MemoryCounter jitCodeCounter;

  friend class js::gc::GCRuntime;
};

/*
 * Allocation policy that performs precise memory tracking on the zone. This
 * should be used for all containers associated with a GC thing or a zone.
 *
 * Since it doesn't hold a JSContext (those may not live long enough), it can't
 * report out-of-memory conditions itself; the caller must check for OOM and
 * take the appropriate action.
 *
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ZoneAllocPolicy : public MallocProvider<ZoneAllocPolicy> {
  ZoneAllocator* zone_;

#ifdef DEBUG
  friend class js::gc::MemoryTracker;  // Can clear |zone_| on merge.
#endif

 public:
  MOZ_IMPLICIT ZoneAllocPolicy(ZoneAllocator* z) : zone_(z) {
#ifdef DEBUG
    zone()->registerPolicy(this);
#endif
  }
  ZoneAllocPolicy(ZoneAllocPolicy& other) : ZoneAllocPolicy(other.zone_) {}
  ZoneAllocPolicy(ZoneAllocPolicy&& other) : ZoneAllocPolicy(other.zone_) {}
  ~ZoneAllocPolicy() {
#ifdef DEBUG
    if (zone_) {
      zone_->unregisterPolicy(this);
    }
#endif
  }

  // Public methods required to fulfill the AllocPolicy interface.

  template <typename T>
  void free_(T* p, size_t numElems) {
    if (p) {
      decMemory(numElems * sizeof(T));
      js_free(p);
    }
  }

  MOZ_MUST_USE bool checkSimulatedOOM() const {
    return !js::oom::ShouldFailWithOOM();
  }

  void reportAllocOverflow() const { reportAllocationOverflow(); }

  // Internal methods called by the MallocProvider implementation.

  MOZ_MUST_USE void* onOutOfMemory(js::AllocFunction allocFunc,
                                   arena_id_t arena, size_t nbytes,
                                   void* reallocPtr = nullptr) {
    return zone()->onOutOfMemory(allocFunc, arena, nbytes, reallocPtr);
  }
  void reportAllocationOverflow() const { zone()->reportAllocationOverflow(); }
  void updateMallocCounter(size_t nbytes) {
    zone()->incPolicyMemory(this, nbytes);
  }

 private:
  ZoneAllocator* zone() const {
    MOZ_ASSERT(zone_);
    return zone_;
  }
  void decMemory(size_t nbytes) { zone_->decPolicyMemory(this, nbytes); }
};

// Functions for memory accounting on the zone.

// Associate malloc memory with a GC thing. This call should be matched by a
// following call to RemoveCellMemory with the same size and use. The total
// amount of malloc memory associated with a zone is used to trigger GC.
//
// You should use InitReservedSlot / InitObjectPrivate in preference to this
// where possible.

inline void AddCellMemory(gc::TenuredCell* cell, size_t nbytes, MemoryUse use) {
  if (nbytes) {
    ZoneAllocator::from(cell->zone())->addCellMemory(cell, nbytes, use);
  }
}
inline void AddCellMemory(gc::Cell* cell, size_t nbytes, MemoryUse use) {
  if (cell->isTenured()) {
    AddCellMemory(&cell->asTenured(), nbytes, use);
  }
}

// Remove association between malloc memory and a GC thing. This call should
// follow a call to AddCellMemory with the same size and use.

inline void RemoveCellMemory(gc::TenuredCell* cell, size_t nbytes,
                             MemoryUse use) {
  if (nbytes) {
    auto zoneBase = ZoneAllocator::from(cell->zoneFromAnyThread());
    zoneBase->removeCellMemory(cell, nbytes, use);
  }
}
inline void RemoveCellMemory(gc::Cell* cell, size_t nbytes, MemoryUse use) {
  if (cell->isTenured()) {
    RemoveCellMemory(&cell->asTenured(), nbytes, use);
  }
}

// Initialize an object's reserved slot with a private value pointing to
// malloc-allocated memory and associate the memory with the object.
//
// This call should be matched with a call to FreeOp::free_/delete_ in the
// object's finalizer to free the memory and update the memory accounting.

inline void InitReservedSlot(NativeObject* obj, uint32_t slot, void* ptr,
                             size_t nbytes, MemoryUse use) {
  AddCellMemory(obj, nbytes, use);
  obj->initReservedSlot(slot, PrivateValue(ptr));
}
template <typename T>
inline void InitReservedSlot(NativeObject* obj, uint32_t slot, T* ptr,
                             MemoryUse use) {
  InitReservedSlot(obj, slot, ptr, sizeof(T), use);
}

// Initialize an object's private slot with a pointer to malloc-allocated memory
// and associate the memory with the object.
//
// This call should be matched with a call to FreeOp::free_/delete_ in the
// object's finalizer to free the memory and update the memory accounting.

inline void InitObjectPrivate(NativeObject* obj, void* ptr, size_t nbytes,
                              MemoryUse use) {
  AddCellMemory(obj, nbytes, use);
  obj->initPrivate(ptr);
}
template <typename T>
inline void InitObjectPrivate(NativeObject* obj, T* ptr, MemoryUse use) {
  InitObjectPrivate(obj, ptr, sizeof(T), use);
}

}  // namespace js

#endif  // gc_ZoneAllocator_h
