/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscompartment_inlines_h___
#define jscompartment_inlines_h___

js::GlobalObject *
JSCompartment::maybeGlobal() const
{
    JS_ASSERT_IF(global_, global_->compartment() == this);
    return global_;
}

js::AutoCompartment::AutoCompartment(JSContext *cx, JSObject *target)
  : cx_(cx),
    origin_(cx->compartment)
{
    cx_->enterCompartment(target->compartment());
}

js::AutoCompartment::~AutoCompartment()
{
    cx_->leaveCompartment(origin_);
}

inline void *
js::Allocator::malloc_(size_t bytes)
{
    return compartment->rt->malloc_(bytes, compartment);
}

inline void *
js::Allocator::calloc_(size_t bytes)
{
    return compartment->rt->calloc_(bytes, compartment);
}

inline void *
js::Allocator::realloc_(void *p, size_t bytes)
{
    return compartment->rt->realloc_(p, bytes, compartment);
}

inline void *
js::Allocator::realloc_(void* p, size_t oldBytes, size_t newBytes)
{
    return compartment->rt->realloc_(p, oldBytes, newBytes, compartment);
}

template <class T>
inline T *
js::Allocator::pod_malloc()
{
    return compartment->rt->pod_malloc<T>(compartment);
}

template <class T>
inline T *
js::Allocator::pod_calloc()
{
    return compartment->rt->pod_calloc<T>(compartment);
}

template <class T>
inline T *
js::Allocator::pod_malloc(size_t numElems)
{
    return compartment->rt->pod_malloc<T>(numElems, compartment);
}

template <class T>
inline T *
js::Allocator::pod_calloc(size_t numElems)
{
    return compartment->rt->pod_calloc<T>(numElems, compartment);
}

inline void *
js::Allocator::parallelNewGCThing(gc::AllocKind thingKind, size_t thingSize)
{
    return arenas.parallelAllocate(compartment, thingKind, thingSize);
}

#endif /* jscompartment_inlines_h___ */
