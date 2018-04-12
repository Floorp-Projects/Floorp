/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ZoneGroup_h
#define gc_ZoneGroup_h

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
    // The helper thread context with exclusive access to this zone group, if
    // usedByHelperThread(), or nullptr when on the main thread.
    UnprotectedData<JSContext*> helperThreadOwnerContext_;

  public:
    bool ownedByCurrentHelperThread();
    void setHelperThreadOwnerContext(JSContext* cx);

    // All zones in the group.
  private:
    ZoneGroupOrGCTaskData<ZoneVector> zones_;
  public:
    ZoneVector& zones() { return zones_.ref(); }

  private:
    enum class HelperThreadUse : uint32_t
    {
        None,
        Pending,
        Active
    };

    mozilla::Atomic<HelperThreadUse> helperThreadUse;

  public:
    // Whether a zone in this group was created for use by a helper thread.
    bool createdForHelperThread() const {
        return helperThreadUse != HelperThreadUse::None;
    }
    // Whether a zone in this group is currently in use by a helper thread.
    bool usedByHelperThread() const {
        return helperThreadUse == HelperThreadUse::Active;
    }
    void setCreatedForHelperThread() {
        MOZ_ASSERT(helperThreadUse == HelperThreadUse::None);
        helperThreadUse = HelperThreadUse::Pending;
    }
    void setUsedByHelperThread() {
        MOZ_ASSERT(helperThreadUse == HelperThreadUse::Pending);
        helperThreadUse = HelperThreadUse::Active;
    }
    void clearUsedByHelperThread() {
        MOZ_ASSERT(helperThreadUse != HelperThreadUse::None);
        helperThreadUse = HelperThreadUse::None;
    }

    explicit ZoneGroup(JSRuntime* runtime);
    ~ZoneGroup();

    // Delete an empty zone after its contents have been merged.
    void deleteEmptyZone(Zone* zone);
};

} // namespace js

#endif // gc_Zone_h
