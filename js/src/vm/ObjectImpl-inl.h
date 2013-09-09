/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectImpl_inl_h
#define vm_ObjectImpl_inl_h

#include "vm/ObjectImpl.h"

#include "mozilla/Assertions.h"

#include "jsgc.h"
#include "jsproxy.h"

#include "gc/Marking.h"
#include "vm/ProxyObject.h"

#include "gc/Barrier-inl.h"

inline JSCompartment *
js::ObjectImpl::compartment() const
{
    return lastProperty()->base()->compartment();
}

inline bool
js::ObjectImpl::nativeContains(ExclusiveContext *cx, Shape *shape)
{
    return nativeLookup(cx, shape->propid()) == shape;
}

inline bool
js::ObjectImpl::nativeContainsPure(Shape *shape)
{
    return nativeLookupPure(shape->propid()) == shape;
}

inline bool
js::ObjectImpl::nonProxyIsExtensible() const
{
    MOZ_ASSERT(!asObjectPtr()->is<ProxyObject>());

    // [[Extensible]] for ordinary non-proxy objects is an object flag.
    return !lastProperty()->hasObjectFlag(BaseShape::NOT_EXTENSIBLE);
}

/* static */ inline bool
js::ObjectImpl::isExtensible(ExclusiveContext *cx, js::Handle<ObjectImpl*> obj, bool *extensible)
{
    if (obj->asObjectPtr()->is<ProxyObject>()) {
        if (!cx->shouldBeJSContext())
            return false;
        HandleObject h =
            HandleObject::fromMarkedLocation(reinterpret_cast<JSObject* const*>(obj.address()));
        return Proxy::isExtensible(cx->asJSContext(), h, extensible);
    }

    *extensible = obj->nonProxyIsExtensible();
    return true;
}

inline bool
js::ObjectImpl::isNative() const
{
    return lastProperty()->isNative();
}

inline uint32_t
js::ObjectImpl::slotSpan() const
{
    if (inDictionaryMode())
        return lastProperty()->base()->slotSpan();
    return lastProperty()->slotSpan();
}

inline uint32_t
js::ObjectImpl::numDynamicSlots() const
{
    return dynamicSlotsCount(numFixedSlots(), slotSpan());
}

inline bool
js::ObjectImpl::isDelegate() const
{
    return lastProperty()->hasObjectFlag(BaseShape::DELEGATE);
}

inline bool
js::ObjectImpl::inDictionaryMode() const
{
    return lastProperty()->inDictionary();
}

JS_ALWAYS_INLINE JS::Zone *
js::ObjectImpl::zone() const
{
    JS_ASSERT(CurrentThreadCanAccessZone(shape_->zone()));
    return shape_->zone();
}

/* static */ inline void
js::ObjectImpl::readBarrier(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    Zone *zone = obj->zone();
    if (zone->needsBarrier()) {
        MOZ_ASSERT(!zone->runtimeFromMainThread()->isHeapMajorCollecting());
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
        MOZ_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPre(void **old)
{
#ifdef JSGC_INCREMENTAL
    Zone *zone = this->zone();
    if (zone->needsBarrier()) {
        if (*old && getClass()->trace)
            getClass()->trace(zone->barrierTracer(), this->asObjectPtr());
    }
#endif
}

/* static */ inline void
js::ObjectImpl::writeBarrierPre(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    /*
     * This would normally be a null test, but TypeScript::global uses 0x1 as a
     * special value.
     */
    if (IsNullTaggedPointer(obj) || !obj->runtimeFromMainThread()->needsBarrier())
        return;

    Zone *zone = obj->zone();
    if (zone->needsBarrier()) {
        MOZ_ASSERT(!zone->runtimeFromMainThread()->isHeapMajorCollecting());
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        MOZ_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::setPrivate(void *data)
{
    void **pprivate = &privateRef(numFixedSlots());
    privateWriteBarrierPre(pprivate);
    *pprivate = data;
}

inline void
js::ObjectImpl::setPrivateGCThing(js::gc::Cell *cell)
{
    void **pprivate = &privateRef(numFixedSlots());
    privateWriteBarrierPre(pprivate);
    *pprivate = reinterpret_cast<void *>(cell);
    privateWriteBarrierPost(pprivate);
}

#endif /* vm_ObjectImpl_inl_h */
