/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/ZoneGroup.h"

#include "jscntxt.h"

#include "jit/JitCompartment.h"

namespace js {

ZoneGroup::ZoneGroup(JSRuntime* runtime)
  : runtime(runtime),
    ownerContext_(TlsContext.get()),
    enterCount(this, 1),
    zones_(),
    nursery_(this, this),
    storeBuffer_(this, runtime, nursery()),
    blocksToFreeAfterMinorGC((size_t) JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    caches_(this),
#ifdef DEBUG
    ionBailAfter_(this, 0),
#endif
    jitZoneGroup(this, nullptr),
    debuggerList_(this)
{}

bool
ZoneGroup::init(size_t maxNurseryBytes)
{
    if (!caches().init())
        return false;

    AutoLockGC lock(runtime);
    if (!nursery().init(maxNurseryBytes, lock))
        return false;

    jitZoneGroup = js_new<jit::JitZoneGroup>(this);
    if (!jitZoneGroup)
        return false;

    return true;
}

ZoneGroup::~ZoneGroup()
{
    js_delete(jitZoneGroup.ref());

    if (this == runtime->gc.systemZoneGroup)
        runtime->gc.systemZoneGroup = nullptr;
}

void
ZoneGroup::enter()
{
    JSContext* cx = TlsContext.get();
    if (ownerContext().context() == cx) {
        MOZ_ASSERT(enterCount);
    } else {
        MOZ_ASSERT(ownerContext().context() == nullptr);
        MOZ_ASSERT(enterCount == 0);
        ownerContext_ = CooperatingContext(cx);
        if (cx->generationalDisabled)
            nursery().disable();
    }
    enterCount++;
}

void
ZoneGroup::leave()
{
    MOZ_ASSERT(ownedByCurrentThread());
    MOZ_ASSERT(enterCount);
    if (--enterCount == 0)
        ownerContext_ = CooperatingContext(nullptr);
}

bool
ZoneGroup::ownedByCurrentThread()
{
    MOZ_ASSERT(TlsContext.get());
    return ownerContext().context() == TlsContext.get();
}

} // namespace js
