/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ObjectImpl_inl_h___
#define ObjectImpl_inl_h___

#include "mozilla/Assertions.h"

#include "jscell.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsgcmark.h"

#include "js/TemplateLib.h"

#include "ObjectImpl.h"

inline bool
js::ObjectImpl::isNative() const
{
    return lastProperty()->isNative();
}

inline js::Class *
js::ObjectImpl::getClass() const
{
    return lastProperty()->getObjectClass();
}

inline JSClass *
js::ObjectImpl::getJSClass() const
{
    return Jsvalify(getClass());
}

inline bool
js::ObjectImpl::hasClass(const Class *c) const
{
    return getClass() == c;
}

inline const js::ObjectOps *
js::ObjectImpl::getOps() const
{
    return &getClass()->ops;
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

/* static */ inline size_t
js::ObjectImpl::dynamicSlotsCount(size_t nfixed, size_t span)
{
    if (span <= nfixed)
        return 0;
    span -= nfixed;
    if (span <= SLOT_CAPACITY_MIN)
        return SLOT_CAPACITY_MIN;

    size_t slots = RoundUpPow2(span);
    MOZ_ASSERT(slots >= span);
    return slots;
}

inline size_t
js::ObjectImpl::sizeOfThis() const
{
    return arenaHeader()->getThingSize();
}

/* static */ inline void
js::ObjectImpl::readBarrier(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = obj->compartment();
    if (comp->needsBarrier()) {
        MOZ_ASSERT(!comp->rt->gcRunning);
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(comp->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPre(void **old)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = compartment();
    if (comp->needsBarrier()) {
        if (*old && getClass()->trace)
            getClass()->trace(comp->barrierTracer(), this->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPost(void **old)
{
}

/* static */ inline void
js::ObjectImpl::writeBarrierPre(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    /*
     * This would normally be a null test, but TypeScript::global uses 0x1 as a
     * special value.
     */
    if (uintptr_t(obj) < 32)
        return;

    JSCompartment *comp = obj->compartment();
    if (comp->needsBarrier()) {
        MOZ_ASSERT(!comp->rt->gcRunning);
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

/* static */ inline void
js::ObjectImpl::writeBarrierPost(ObjectImpl *obj, void *addr)
{
}

inline bool
js::ObjectImpl::isExtensible() const
{
    return !lastProperty()->hasObjectFlag(BaseShape::NOT_EXTENSIBLE);
}

#endif /* ObjectImpl_inl_h__ */
