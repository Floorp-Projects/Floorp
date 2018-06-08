/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Compartment_inl_h
#define vm_Compartment_inl_h

#include "vm/Compartment.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "vm/Iteration.h"

#include "vm/JSContext-inl.h"

inline bool
JS::Compartment::wrap(JSContext* cx, JS::MutableHandleValue vp)
{
    /* Only GC things have to be wrapped or copied. */
    if (!vp.isGCThing())
        return true;

    /*
     * Symbols are GC things, but never need to be wrapped or copied because
     * they are always allocated in the atoms zone. They still need to be
     * marked in the new compartment's zone, however.
     */
    if (vp.isSymbol()) {
        cx->markAtomValue(vp);
        return true;
    }

    /* Handle strings. */
    if (vp.isString()) {
        JS::RootedString str(cx, vp.toString());
        if (!wrap(cx, &str))
            return false;
        vp.setString(str);
        return true;
    }

#ifdef ENABLE_BIGINT
    if (vp.isBigInt()) {
        JS::RootedBigInt bi(cx, vp.toBigInt());
        if (!wrap(cx, &bi))
            return false;
        vp.setBigInt(bi);
        return true;
    }
#endif

    MOZ_ASSERT(vp.isObject());

    /*
     * All that's left are objects.
     *
     * Object wrapping isn't the fastest thing in the world, in part because
     * we have to unwrap and invoke the prewrap hook to find the identity
     * object before we even start checking the cache. Neither of these
     * operations are needed in the common case, where we're just wrapping
     * a plain JS object from the wrappee's side of the membrane to the
     * wrapper's side.
     *
     * To optimize this, we note that the cache should only ever contain
     * identity objects - that is to say, objects that serve as the
     * canonical representation for a unique object identity observable by
     * script. Unwrap and prewrap are both steps that we take to get to the
     * identity of an incoming objects, and as such, they shuld never map
     * one identity object to another object. This means that we can safely
     * check the cache immediately, and only risk false negatives. Do this
     * in opt builds, and do both in debug builds so that we can assert
     * that we get the same answer.
     */
#ifdef DEBUG
    MOZ_ASSERT(JS::ValueIsNotGray(vp));
    JS::RootedObject cacheResult(cx);
#endif
    JS::RootedValue v(cx, vp);
    if (js::WrapperMap::Ptr p = crossCompartmentWrappers.lookup(js::CrossCompartmentKey(v))) {
#ifdef DEBUG
        cacheResult = &p->value().get().toObject();
#else
        vp.set(p->value().get());
        return true;
#endif
    }

    JS::RootedObject obj(cx, &vp.toObject());
    if (!wrap(cx, &obj))
        return false;
    vp.setObject(*obj);
    MOZ_ASSERT_IF(cacheResult, obj == cacheResult);
    return true;
}

#endif /* vm_Compartment_inl_h */
