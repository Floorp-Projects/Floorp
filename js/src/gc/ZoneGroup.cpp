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
    helperThreadUse(HelperThreadUse::None),
#ifdef DEBUG
    ionBailAfter_(this, 0),
#endif
    jitZoneGroup(this, nullptr),
    debuggerList_(this),
    numFinishedBuilders(0),
    ionLazyLinkListSize_(0)
{}

bool
ZoneGroup::init()
{
    AutoLockGC lock(runtime);

    jitZoneGroup = js_new<jit::JitZoneGroup>(this);
    if (!jitZoneGroup)
        return false;

    return true;
}

ZoneGroup::~ZoneGroup()
{
#ifdef DEBUG
    MOZ_ASSERT(helperThreadUse == HelperThreadUse::None);
    {
        AutoLockHelperThreadState lock;
        MOZ_ASSERT(ionLazyLinkListSize_ == 0);
        MOZ_ASSERT(ionLazyLinkList().isEmpty());
    }
#endif

    js_delete(jitZoneGroup.ref());

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

ZoneGroup::IonBuilderList&
ZoneGroup::ionLazyLinkList()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime),
               "Should only be mutated by the active thread.");
    return ionLazyLinkList_.ref();
}

void
ZoneGroup::ionLazyLinkListRemove(jit::IonBuilder* builder)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime),
               "Should only be mutated by the active thread.");
    MOZ_ASSERT(this == builder->script()->zone()->group());
    MOZ_ASSERT(ionLazyLinkListSize_ > 0);

    builder->removeFrom(ionLazyLinkList());
    ionLazyLinkListSize_--;

    MOZ_ASSERT(ionLazyLinkList().isEmpty() == (ionLazyLinkListSize_ == 0));
}

void
ZoneGroup::ionLazyLinkListAdd(jit::IonBuilder* builder)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime),
               "Should only be mutated by the active thread.");
    MOZ_ASSERT(this == builder->script()->zone()->group());
    ionLazyLinkList().insertFront(builder);
    ionLazyLinkListSize_++;
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
