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
void MaybeMallocTriggerZoneGC(JSRuntime* rt, ZoneAllocator* zoneAlloc,
                              const HeapSize& heap,
                              const ZoneThreshold& threshold,
                              JS::GCReason reason);
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
    // This is a safe upcast, but the compiler hasn't seen the definition yet.
    return reinterpret_cast<ZoneAllocator*>(zone);
  }

  MOZ_MUST_USE void* onOutOfMemory(js::AllocFunction allocFunc,
                                   arena_id_t arena, size_t nbytes,
                                   void* reallocPtr = nullptr);
  void reportAllocationOverflow() const;

  void adoptMallocBytes(ZoneAllocator* other) {
    gcMallocBytes.adopt(other->gcMallocBytes);
    gcJitBytes.adopt(other->gcJitBytes);
#ifdef DEBUG
    gcMallocTracker.adopt(other->gcMallocTracker);
#endif
  }

  void updateMemoryCountersOnGCStart();
  void updateGCThresholds(gc::GCRuntime& gc, JSGCInvocationKind invocationKind,
                          const js::AutoLockGC& lock);

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

  void removeCellMemory(js::gc::Cell* cell, size_t nbytes, js::MemoryUse use,
                        bool wasSwept = false) {
    MOZ_ASSERT(cell);
    MOZ_ASSERT(nbytes);
    MOZ_ASSERT_IF(CurrentThreadIsGCSweeping(), wasSwept);

    gcMallocBytes.removeBytes(nbytes, wasSwept);

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
  void movePolicy(js::ZoneAllocPolicy* dst, js::ZoneAllocPolicy* src) {
    return gcMallocTracker.movePolicy(dst, src);
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
  void decPolicyMemory(js::ZoneAllocPolicy* policy, size_t nbytes,
                       bool wasSwept) {
    MOZ_ASSERT(nbytes);
    MOZ_ASSERT_IF(CurrentThreadIsGCSweeping(), wasSwept);

    gcMallocBytes.removeBytes(nbytes, wasSwept);

#ifdef DEBUG
    gcMallocTracker.decPolicyMemory(policy, nbytes);
#endif
  }

  void incJitMemory(size_t nbytes) {
    MOZ_ASSERT(nbytes);
    gcJitBytes.addBytes(nbytes);
    maybeTriggerZoneGC(gcJitBytes, gcJitThreshold,
                       JS::GCReason::TOO_MUCH_JIT_CODE);
  }
  void decJitMemory(size_t nbytes) {
    MOZ_ASSERT(nbytes);
    gcJitBytes.removeBytes(nbytes, true);
  }

  // Check malloc allocation threshold and trigger a zone GC if necessary.
  void maybeMallocTriggerZoneGC() {
    maybeTriggerZoneGC(gcMallocBytes, gcMallocThreshold,
                       JS::GCReason::TOO_MUCH_MALLOC);
  }

 private:
  void maybeTriggerZoneGC(const js::gc::HeapSize& heap,
                          const js::gc::ZoneThreshold& threshold,
                          JS::GCReason reason) {
    JSRuntime* rt = runtimeFromAnyThread();
    if (heap.gcBytes() >= threshold.gcTriggerBytes() &&
        rt->heapState() == JS::HeapState::Idle) {
      gc::MaybeMallocTriggerZoneGC(rt, this, heap, threshold, reason);
    }
  }

 public:
  // Track GC heap size under this Zone.
  js::gc::HeapSize zoneSize;

  // Thresholds used to trigger GC based on heap size.
  js::gc::ZoneHeapThreshold threshold;

  // Amount of data to allocate before triggering a new incremental slice for
  // the current GC.
  js::MainThreadData<size_t> gcDelayBytes;

  // Malloc counter used for allocations where size information is
  // available. Used for some internal and all tracked external allocations.
  js::gc::HeapSize gcMallocBytes;

  // Thresholds used to trigger GC based on malloc allocations.
  js::gc::ZoneMallocThreshold gcMallocThreshold;

  // Malloc counter used for JIT code allocation.
  js::gc::HeapSize gcJitBytes;

  // Thresholds used to trigger GC based on JIT allocations.
  js::gc::ZoneFixedThreshold gcJitThreshold;

 private:
#ifdef DEBUG
  // In debug builds, malloc allocations can be tracked to make debugging easier
  // (possible?) if allocation and free sizes don't balance.
  js::gc::MemoryTracker gcMallocTracker;
#endif

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
  MOZ_IMPLICIT ZoneAllocPolicy(JS::Zone* z)
      : ZoneAllocPolicy(ZoneAllocator::from(z)) {}
  ZoneAllocPolicy(ZoneAllocPolicy& other) : ZoneAllocPolicy(other.zone_) {}
  ZoneAllocPolicy(ZoneAllocPolicy&& other) : zone_(other.zone_) {
#ifdef DEBUG
    zone()->movePolicy(this, &other);
#endif
    other.zone_ = nullptr;
  }
  ~ZoneAllocPolicy() {
#ifdef DEBUG
    if (zone_) {
      zone_->unregisterPolicy(this);
    }
#endif
  }

  ZoneAllocPolicy& operator=(const ZoneAllocPolicy& other) {
#ifdef DEBUG
    zone()->unregisterPolicy(this);
#endif
    zone_ = other.zone();
#ifdef DEBUG
    zone()->registerPolicy(this);
#endif
    return *this;
  }
  ZoneAllocPolicy& operator=(ZoneAllocPolicy&& other) {
#ifdef DEBUG
    zone()->unregisterPolicy(this);
    zone()->movePolicy(this, &other);
#endif
    other.zone_ = nullptr;
    return *this;
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
  void decMemory(size_t nbytes);
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
                             MemoryUse use, bool wasSwept = false) {
  if (nbytes) {
    auto zoneBase = ZoneAllocator::from(cell->zoneFromAnyThread());
    zoneBase->removeCellMemory(cell, nbytes, use, wasSwept);
  }
}
inline void RemoveCellMemory(gc::Cell* cell, size_t nbytes, MemoryUse use,
                             bool wasSwept = false) {
  if (cell->isTenured()) {
    RemoveCellMemory(&cell->asTenured(), nbytes, use, wasSwept);
  }
}

// Initialize an object's reserved slot with a private value pointing to
// malloc-allocated memory and associate the memory with the object.
//
// This call should be matched with a call to JSFreeOp::free_/delete_ in the
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
// This call should be matched with a call to JSFreeOp::free_/delete_ in the
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
