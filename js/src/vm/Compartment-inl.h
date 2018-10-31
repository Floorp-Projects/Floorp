/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Compartment_inl_h
#define vm_Compartment_inl_h

#include "vm/Compartment.h"

#include <type_traits>

#include "jsapi.h"
#include "jsfriendapi.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/Wrapper.h"
#include "vm/Iteration.h"
#include "vm/JSObject.h"

#include "vm/JSContext-inl.h"

inline bool
JS::Compartment::wrap(JSContext* cx, JS::MutableHandleValue vp)
{
    /* Only GC things have to be wrapped or copied. */
    if (!vp.isGCThing()) {
        return true;
    }

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
        if (!wrap(cx, &str)) {
            return false;
        }
        vp.setString(str);
        return true;
    }

#ifdef ENABLE_BIGINT
    if (vp.isBigInt()) {
        JS::RootedBigInt bi(cx, vp.toBigInt());
        if (!wrap(cx, &bi)) {
            return false;
        }
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
    if (!wrap(cx, &obj)) {
        return false;
    }
    vp.setObject(*obj);
    MOZ_ASSERT_IF(cacheResult, obj == cacheResult);
    return true;
}

namespace js {
namespace detail {

template<class T>
bool
UnwrapThisSlowPath(JSContext* cx,
                   HandleValue val,
                   const char* className,
                   const char* methodName,
                   MutableHandle<T*> unwrappedResult)
{
    if (!val.isObject()) {
        JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                                   className, methodName, InformalValueTypeName(val));
        return false;
    }

    JSObject* obj = &val.toObject();
    if (IsWrapper(obj)) {
        obj = CheckedUnwrap(obj);
        if (!obj) {
            ReportAccessDenied(cx);
            return false;
        }
    }

    if (!obj->is<T>()) {
        JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                                   className, methodName, InformalValueTypeName(val));
        return false;
    }

    unwrappedResult.set(&obj->as<T>());
    return true;
}

} // namespace detail

/**
 * Remove all wrappers from `val` and try to downcast the result to `T`.
 *
 * DANGER: The value stored in `unwrappedResult` may not be same-compartment
 * with `cx`.
 *
 * This throws a TypeError if the value isn't an object, cannot be unwrapped,
 * or isn't an instance of the expected type.
 *
 * Terminology note: The term "non-generic method" comes from ECMA-262. A
 * non-generic method is one that checks the type of `this`, typically because
 * it needs access to internal slots. Generic methods do not type-check. For
 * example, `Array.prototype.join` is generic; it can be applied to anything
 * with a `.length` property and elements.
 */
template<class T>
inline bool
UnwrapThisForNonGenericMethod(JSContext* cx,
                              HandleValue val,
                              const char* className,
                              const char* methodName,
                              MutableHandle<T*> unwrappedResult)
{
    static_assert(!std::is_convertible<T*, Wrapper*>::value,
                  "T can't be a Wrapper type; this function discards wrappers");

    cx->check(val);
    if (val.isObject() && val.toObject().is<T>()) {
        unwrappedResult.set(&val.toObject().as<T>());
        return true;
    }
    return detail::UnwrapThisSlowPath(cx, val, className, methodName, unwrappedResult);
}

/**
 * Extra signature so callers don't have to specify T explicitly.
 */
template<class T>
inline bool
UnwrapThisForNonGenericMethod(JSContext* cx,
                              HandleValue val,
                              const char* className,
                              const char* methodName,
                              Rooted<T*>* out)
{
    return UnwrapThisForNonGenericMethod(cx, val, className, methodName, MutableHandle<T*>(out));
}

/**
 * Read a private slot that is known to point to a particular type of object.
 *
 * Some internal slots specified in various standards effectively have static
 * types. For example, the [[ownerReadableStream]] slot of a stream reader is
 * guaranteed to be a ReadableStream. However, because of compartments, we
 * sometimes store a cross-compartment wrapper in that slot. And since wrappers
 * can be nuked, that wrapper may become a dead object proxy.
 *
 * UnwrapInternalSlot() copes with the cross-compartment and dead object cases,
 * but not plain bugs where the slot hasn't been initialized or doesn't contain
 * the expected type of object. Call this only if the slot is certain to
 * contain either an instance of T, a wrapper for a T, or a dead object.
 *
 * cx and unwrappedObj are not required to be same-compartment.
 *
 * DANGER: The result stored in `unwrappedResult` will not necessarily be
 * same-compartment with either cx or obj.
 */
template <class T>
MOZ_MUST_USE bool
UnwrapInternalSlot(JSContext* cx,
                   Handle<NativeObject*> unwrappedObj,
                   uint32_t slot,
                   MutableHandle<T*> unwrappedResult)
{
    static_assert(!std::is_convertible<T*, Wrapper*>::value,
                  "T can't be a Wrapper type; this function discards wrappers");

    JSObject* result = &unwrappedObj->getFixedSlot(slot).toObject();
    if (IsProxy(result)) {
        if (JS_IsDeadWrapper(result)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }

        // It would probably be OK to do an unchecked unwrap here, but we allow
        // arbitrary security policies, so check anyway.
        result = CheckedUnwrap(result);
        if (!result) {
            ReportAccessDenied(cx);
            return false;
        }
    }

    unwrappedResult.set(&result->as<T>());
    return true;
}

/**
 * Extra signature so callers don't have to specify T explicitly.
 */
template <class T>
inline MOZ_MUST_USE bool
UnwrapInternalSlot(JSContext* cx,
                   Handle<NativeObject*> obj,
                   uint32_t slot,
                   Rooted<T*>* unwrappedResult)
{
    return UnwrapInternalSlot(cx, obj, slot, MutableHandle<T*>(unwrappedResult));
}

} // namespace js

#endif /* vm_Compartment_inl_h */
