/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/ZoneGroup.h"

#include "gc/PublicIterators.h"
#include "jit/IonBuilder.h"
#include "jit/JitCompartment.h"
#include "vm/JSContext.h"

using namespace js;

namespace js {

ZoneGroup::ZoneGroup(JSRuntime* runtime)
  : runtime(runtime),
    helperThreadOwnerContext_(nullptr),
    zones_(this),
    helperThreadUse(HelperThreadUse::None)
{}

ZoneGroup::~ZoneGroup()
{
    MOZ_ASSERT(helperThreadUse == HelperThreadUse::None);

    if (this == runtime->gc.systemZoneGroup)
        runtime->gc.systemZoneGroup = nullptr;
}

void
ZoneGroup::setHelperThreadOwnerContext(JSContext* cx)
{
    MOZ_ASSERT_IF(cx, TlsContext.get() == cx);
    helperThreadOwnerContext_ = cx;
}

bool
ZoneGroup::ownedByCurrentHelperThread()
{
    MOZ_ASSERT(usedByHelperThread());
    MOZ_ASSERT(TlsContext.get());
    return helperThreadOwnerContext_ == TlsContext.get();
}

void
ZoneGroup::deleteEmptyZone(Zone* zone)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime));
    MOZ_ASSERT(zone->group() == this);
    MOZ_ASSERT(zone->compartments().empty());
    for (auto& i : zones()) {
        if (i == zone) {
            zones().erase(&i);
            zone->destroy(runtime->defaultFreeOp());
            return;
        }
    }
    MOZ_CRASH("Zone not found");
}

} // namespace js
