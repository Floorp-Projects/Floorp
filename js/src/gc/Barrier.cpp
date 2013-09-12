/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Barrier.h"

#include "jscompartment.h"
#include "jsobj.h"

#include "gc/Zone.h"

namespace js {

#ifdef DEBUG

bool
HeapValue::preconditionForSet(Zone *zone)
{
    if (!value.isMarkable())
        return true;

    return ZoneOfValue(value) == zone ||
           zone->runtimeFromAnyThread()->isAtomsZone(ZoneOfValue(value));
}

bool
HeapSlot::preconditionForSet(JSObject *owner, Kind kind, uint32_t slot)
{
    return kind == Slot
         ? &owner->getSlotRef(slot) == this
         : &owner->getDenseElement(slot) == (const Value *)this;
}

bool
HeapSlot::preconditionForSet(Zone *zone, JSObject *owner, Kind kind, uint32_t slot)
{
    bool ok = kind == Slot
            ? &owner->getSlotRef(slot) == this
            : &owner->getDenseElement(slot) == (const Value *)this;
    return ok && owner->zone() == zone;
}

void
HeapSlot::preconditionForWriteBarrierPost(JSObject *obj, Kind kind, uint32_t slot, Value target)
{
    JS_ASSERT_IF(kind == Slot, obj->getSlotAddressUnchecked(slot)->get() == target);
    JS_ASSERT_IF(kind == Element,
                 static_cast<HeapSlot *>(obj->getDenseElements() + slot)->get() == target);
}

bool
RuntimeFromMainThreadIsHeapMajorCollecting(JS::shadow::Zone *shadowZone)
{
    return shadowZone->runtimeFromMainThread()->isHeapMajorCollecting();
}
#endif // DEBUG

} // namespace js
