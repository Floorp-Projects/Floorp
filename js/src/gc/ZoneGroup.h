/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ZoneGroup_h
#define gc_ZoneGroup_h

#include "jsgc.h"

#include "gc/Statistics.h"
#include "vm/Caches.h"

namespace js {

namespace jit { class JitZoneGroup; }

class AutoKeepAtoms;

typedef Vector<JS::Zone*, 4, SystemAllocPolicy> ZoneVector;

using ScriptAndCountsVector = GCVector<ScriptAndCounts, 0, SystemAllocPolicy>;

// Zone groups encapsulate data about a group of zones that are logically
// related in some way. Currently, each runtime has a single zone group, and
// all zones except the atoms zone (which has no group) are in that group.
// This will change soon.
//
// When JSRuntimes become multithreaded (also happening soon; see bug 1323066),
// zone groups will be the primary means by which threads ensure exclusive
// access to the data they are using. Most data in a zone group, its zones,
// compartments, GC things and so forth may only be used by the thread that has
// entered the zone group.
//
// This restriction is not quite in place yet: zones used by an parse thread
// are accessed by that thread even though it does not have exclusive access
// to the entire zone group. This will also be changing soon.

class ZoneGroup
{
  public:
    JSRuntime* const runtime;

    // The context with exclusive access to this zone group.
    mozilla::Atomic<JSContext*, mozilla::ReleaseAcquire> context;

    // The number of times the context has entered this zone group.
    ZoneGroupData<size_t> enterCount;

    void enter();
    void leave();
    bool ownedByCurrentThread();

    // All zones in the group.
  private:
    ActiveThreadOrGCTaskData<ZoneVector> zones_;
  public:
    ZoneVector& zones() { return zones_.ref(); }

    explicit ZoneGroup(JSRuntime* runtime);
    ~ZoneGroup();

    bool init(size_t maxNurseryBytes);

  private:
    ZoneGroupData<Nursery> nursery_;
    ZoneGroupData<gc::StoreBuffer> storeBuffer_;
  public:
    Nursery& nursery() { return nursery_.ref(); }
    gc::StoreBuffer& storeBuffer() { return storeBuffer_.ref(); }

    // Free LIFO blocks are transferred to this allocator before being freed
    // after minor GC.
    ActiveThreadData<LifoAlloc> blocksToFreeAfterMinorGC;

    void minorGC(JS::gcreason::Reason reason,
                 gcstats::Phase phase = gcstats::PHASE_MINOR_GC) JS_HAZ_GC_CALL;
    void evictNursery(JS::gcreason::Reason reason = JS::gcreason::EVICT_NURSERY) {
        minorGC(reason, gcstats::PHASE_EVICT_NURSERY);
    }
    void freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo);

    const void* addressOfNurseryPosition() {
        return nursery_.refNoCheck().addressOfPosition();
    }
    const void* addressOfNurseryCurrentEnd() {
        return nursery_.refNoCheck().addressOfCurrentEnd();
    }

    // Queue a thunk to run after the next minor GC.
    void callAfterMinorGC(void (*thunk)(void* data), void* data) {
        nursery().queueSweepAction(thunk, data);
    }

    inline bool isCollecting();
    inline bool isGCScheduled();

  private:
    ZoneGroupData<ZoneGroupCaches> caches_;
  public:
    ZoneGroupCaches& caches() { return caches_.ref(); }

#ifdef DEBUG
  private:
    // The number of possible bailing places encounters before forcefully bailing
    // in that place. Zero means inactive.
    ZoneGroupData<uint32_t> ionBailAfter_;

  public:
    void* addressOfIonBailAfter() { return &ionBailAfter_; }

    // Set after how many bailing places we should forcefully bail.
    // Zero disables this feature.
    void setIonBailAfter(uint32_t after) {
        ionBailAfter_ = after;
    }
#endif

    ZoneGroupData<jit::JitZoneGroup*> jitZoneGroup;

  private:
    /* Linked list of all Debugger objects in the group. */
    ZoneGroupData<mozilla::LinkedList<js::Debugger>> debuggerList_;
  public:
    mozilla::LinkedList<js::Debugger>& debuggerList() { return debuggerList_.ref(); }

    /* If true, new scripts must be created with PC counter information. */
    ZoneGroupOrIonCompileData<bool> profilingScripts;

    /* Strong references on scripts held for PCCount profiling API. */
    ZoneGroupData<JS::PersistentRooted<ScriptAndCountsVector>*> scriptAndCountsVector;
};

class MOZ_RAII AutoAccessZoneGroup
{
    ZoneGroup* group;

  public:
    explicit AutoAccessZoneGroup(ZoneGroup* group)
      : group(group)
    {
        group->enter();
    }

    ~AutoAccessZoneGroup() {
        group->leave();
    }
};

class MOZ_RAII AutoAccessZoneGroups
{
    Vector<ZoneGroup*, 4, SystemAllocPolicy> acquiredGroups;

  public:
    AutoAccessZoneGroups() {}

    ~AutoAccessZoneGroups() {
        for (size_t i = 0; i < acquiredGroups.length(); i++)
            acquiredGroups[i]->leave();
    }

    void access(ZoneGroup* group) {
        group->enter();
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!acquiredGroups.append(group))
            oomUnsafe.crash("acquiredGroups.append failed");
    }
};

} // namespace js

#endif // gc_Zone_h
