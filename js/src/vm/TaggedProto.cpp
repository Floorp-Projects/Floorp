/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TaggedProto.h"

#include "jsfun.h"
#include "jsobj.h"

#include "gc/Barrier.h"
#include "gc/Zone.h"
#include "js/HashTable.h"

namespace js {

/* static */ void
InternalGCMethods<TaggedProto>::preBarrier(TaggedProto& proto)
{
    InternalGCMethods<JSObject*>::preBarrier(proto.toObjectOrNull());
}

/* static */ void
InternalGCMethods<TaggedProto>::postBarrier(TaggedProto* vp, TaggedProto prev, TaggedProto next)
{
    JSObject* prevObj = prev.isObject() ? prev.toObject() : nullptr;
    JSObject* nextObj = next.isObject() ? next.toObject() : nullptr;
    InternalGCMethods<JSObject*>::postBarrier(reinterpret_cast<JSObject**>(vp), prevObj,
                                              nextObj);
}

/* static */ void
InternalGCMethods<TaggedProto>::readBarrier(const TaggedProto& proto)
{
    InternalGCMethods<JSObject*>::readBarrier(proto.toObjectOrNull());
}

} // namespace js

js::HashNumber
js::TaggedProto::hashCode() const
{
    uint64_t uid = uniqueId();
    return js::HashNumber(uid >> 32) ^ js::HashNumber(uid & 0xFFFFFFFF);
}

uint64_t
js::TaggedProto::uniqueId() const
{
    if (isLazy())
        return uint64_t(1);
    JSObject* obj = toObjectOrNull();
    if (!obj)
        return uint64_t(0);
    AutoEnterOOMUnsafeRegion oomUnsafe;
    uint64_t uid;
    if (!obj->zone()->getUniqueId(obj, &uid))
        oomUnsafe.crash("failed to get unique id for TaggedProto");
    return uid;
}
