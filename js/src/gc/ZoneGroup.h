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
#include "vm/Stack.h"

namespace js {

namespace jit { class JitZoneGroup; }

class AutoKeepAtoms;

typedef Vector<JS::Zone*, 4, SystemAllocPolicy> ZoneVector;

// Zone groups encapsulate data about a group of zones that are logically
// related in some way.
//
// Zone groups are the primary means by which threads ensure exclusive access
// to the data they are using. Most data in a zone group, its zones,
// compartments, GC things and so forth may only be used by the thread that has
// entered the zone group.

class ZoneGroup
{
  public:
    JSRuntime* const runtime;

  private:
    // The context with exclusive access to this zone group.
    UnprotectedData<CooperatingContext> ownerContext_;

    // The number of times the context has entered this zone group.
    UnprotectedData<size_t> enterCount;

    // If this flag is true, then we may need to block before entering this zone
    // group. Blocking happens using JSContext::yieldToEmbedding.
    UnprotectedData<bool> useExclusiveLocking;

  public:
    CooperatingContext& ownerContext() { return ownerContext_.ref(); }
    void* addressOfOwnerContext() { return &ownerContext_.ref().cx; }

    void enter(JSContext* cx);
    void leave();
    bool ownedByCurrentThread();

    // All zones in the group.
  private:
    ZoneGroupOrGCTaskData<ZoneVector> zones_;
  public:
    ZoneVector& zones() { return zones_.ref(); }

    // Whether a zone in this group is in use by a helper thread.
    mozilla::Atomic<bool> usedByHelperThread;

    explicit ZoneGroup(JSRuntime* runtime);
    ~ZoneGroup();

    bool init();

    inline Nursery& nursery();
    inline gc::StoreBuffer& storeBuffer();

    // Queue a thunk to run after the next minor GC.
    inline void callAfterMinorGC(void (*thunk)(void* data), void* data);

    inline bool isCollecting();
    inline bool isGCScheduled();

    // See the useExclusiveLocking field above.
    void setUseExclusiveLocking() { useExclusiveLocking = true; }

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

    // Number of Ion compilations which were finished off thread and are
    // waiting to be lazily linked. This is only set while holding the helper
    // thread state lock, but may be read from at other times.
    mozilla::Atomic<size_t> numFinishedBuilders;

  private:
    /* List of Ion compilation waiting to get linked. */
    typedef mozilla::LinkedList<js::jit::IonBuilder> IonBuilderList;

    js::HelperThreadLockData<IonBuilderList> ionLazyLinkList_;
    js::HelperThreadLockData<size_t> ionLazyLinkListSize_;

  public:
    IonBuilderList& ionLazyLinkList();

    size_t ionLazyLinkListSize() {
        return ionLazyLinkListSize_;
    }

    void ionLazyLinkListRemove(js::jit::IonBuilder* builder);
    void ionLazyLinkListAdd(js::jit::IonBuilder* builder);
};

} // namespace js

#endif // gc_Zone_h
