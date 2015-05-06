/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Barrier.h"

#include "jscompartment.h"
#include "jsobj.h"

#include "gc/Zone.h"
#include "js/Value.h"
#include "vm/Symbol.h"

#ifdef DEBUG

bool
js::HeapSlot::preconditionForSet(NativeObject* owner, Kind kind, uint32_t slot)
{
    return kind == Slot
         ? &owner->getSlotRef(slot) == this
         : &owner->getDenseElement(slot) == (const Value*)this;
}

bool
js::HeapSlot::preconditionForWriteBarrierPost(NativeObject* obj, Kind kind, uint32_t slot,
                                              Value target) const
{
    return kind == Slot
         ? obj->getSlotAddressUnchecked(slot)->get() == target
         : static_cast<HeapSlot*>(obj->getDenseElements() + slot)->get() == target;
}

bool
js::RuntimeFromMainThreadIsHeapMajorCollecting(JS::shadow::Zone* shadowZone)
{
    return shadowZone->runtimeFromMainThread()->isHeapMajorCollecting();
}

bool
js::CurrentThreadIsIonCompiling()
{
    return TlsPerThreadData.get()->ionCompiling;
}

bool
js::CurrentThreadIsGCSweeping()
{
    return js::TlsPerThreadData.get()->gcSweeping;
}

#endif // DEBUG

template <typename S>
template <typename T>
void
js::ReadBarrierFunctor<S>::operator()(T* t)
{
    InternalGCMethods<T*>::readBarrier(t);
}
template void js::ReadBarrierFunctor<JS::Value>::operator()<JS::Symbol>(JS::Symbol*);
template void js::ReadBarrierFunctor<JS::Value>::operator()<JSObject>(JSObject*);
template void js::ReadBarrierFunctor<JS::Value>::operator()<JSString>(JSString*);

template <typename S>
template <typename T>
void
js::PreBarrierFunctor<S>::operator()(T* t)
{
    InternalGCMethods<T*>::preBarrier(t);
}
template void js::PreBarrierFunctor<JS::Value>::operator()<JS::Symbol>(JS::Symbol*);
template void js::PreBarrierFunctor<JS::Value>::operator()<JSObject>(JSObject*);
template void js::PreBarrierFunctor<JS::Value>::operator()<JSString>(JSString*);
template void js::PreBarrierFunctor<jsid>::operator()<JS::Symbol>(JS::Symbol*);
template void js::PreBarrierFunctor<jsid>::operator()<JSString>(JSString*);

JS_PUBLIC_API(void)
JS::HeapObjectPostBarrier(JSObject** objp)
{
    MOZ_ASSERT(objp);
    MOZ_ASSERT(*objp);
    js::InternalGCMethods<JSObject*>::postBarrierRelocate(objp);
}

JS_PUBLIC_API(void)
JS::HeapObjectRelocate(JSObject** objp)
{
    MOZ_ASSERT(objp);
    MOZ_ASSERT(*objp);
    js::InternalGCMethods<JSObject*>::postBarrierRemove(objp);
}

JS_PUBLIC_API(void)
JS::HeapValuePostBarrier(JS::Value* valuep)
{
    MOZ_ASSERT(valuep);
    js::InternalGCMethods<JS::Value>::postBarrierRelocate(valuep);
}

JS_PUBLIC_API(void)
JS::HeapValueRelocate(JS::Value* valuep)
{
    MOZ_ASSERT(valuep);
    js::InternalGCMethods<JS::Value>::postBarrierRemove(valuep);
}
