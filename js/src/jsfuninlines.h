/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsfuninlines_h___
#define jsfuninlines_h___

#include "jsfun.h"
#include "jsscript.h"

#include "vm/GlobalObject.h"

#include "vm/ScopeObject-inl.h"

inline bool
JSFunction::inStrictMode() const
{
    return script()->strictModeCode;
}

inline JSObject *
JSFunction::environment() const
{
    JS_ASSERT(isInterpreted());
    return u.i.env_;
}

inline void
JSFunction::setEnvironment(JSObject *obj)
{
    JS_ASSERT(isInterpreted());
    *(js::HeapPtrObject *)&u.i.env_ = obj;
}

inline void
JSFunction::initEnvironment(JSObject *obj)
{
    JS_ASSERT(isInterpreted());
    ((js::HeapPtrObject *)&u.i.env_)->init(obj);
}

inline void
JSFunction::initializeExtended()
{
    JS_ASSERT(isExtended());

    JS_ASSERT(js::ArrayLength(toExtended()->extendedSlots) == 2);
    toExtended()->extendedSlots[0].init(js::UndefinedValue());
    toExtended()->extendedSlots[1].init(js::UndefinedValue());
}

inline void
JSFunction::setExtendedSlot(size_t which, const js::Value &val)
{
    JS_ASSERT(which < js::ArrayLength(toExtended()->extendedSlots));
    toExtended()->extendedSlots[which] = val;
}

inline const js::Value &
JSFunction::getExtendedSlot(size_t which) const
{
    JS_ASSERT(which < js::ArrayLength(toExtended()->extendedSlots));
    return toExtended()->extendedSlots[which];
}

namespace js {

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v)
{
    return v.isObject() && v.toObject().isFunction();
}

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v, JSFunction **fun)
{
    if (v.isObject() && v.toObject().isFunction()) {
        *fun = v.toObject().toFunction();
        return true;
    }
    return false;
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->isNative();
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, JSFunction **fun)
{
    return IsFunctionObject(v, fun) && (*fun)->isNative();
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, JSNative native)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->maybeNative() == native;
}

/*
 * When we have an object of a builtin class, we don't quite know what its
 * valueOf/toString methods are, since these methods may have been overwritten
 * or shadowed. However, we can still do better than the general case by
 * hard-coding the necessary properties for us to find the native we expect.
 *
 * TODO: a per-thread shape-based cache would be faster and simpler.
 */
static JS_ALWAYS_INLINE bool
ClassMethodIsNative(JSContext *cx, HandleObject obj, Class *clasp, HandleId methodid, JSNative native)
{
    JS_ASSERT(obj->getClass() == clasp);

    Value v;
    if (!HasDataProperty(cx, obj, methodid, &v)) {
        RootedObject proto(cx, obj->getProto());
        if (!proto || proto->getClass() != clasp || !HasDataProperty(cx, proto, methodid, &v))
            return false;
    }

    return js::IsNativeFunction(v, native);
}

extern JS_ALWAYS_INLINE bool
SameTraceType(const Value &lhs, const Value &rhs)
{
    return SameType(lhs, rhs) &&
           (lhs.isPrimitive() ||
            lhs.toObject().isFunction() == rhs.toObject().isFunction());
}

/* Valueified JS_IsConstructing. */
static JS_ALWAYS_INLINE bool
IsConstructing(const Value *vp)
{
#ifdef DEBUG
    JSObject *callee = &JS_CALLEE(cx, vp).toObject();
    if (callee->isFunction()) {
        JSFunction *fun = callee->toFunction();
        JS_ASSERT((fun->flags & JSFUN_CONSTRUCTOR) != 0);
    } else {
        JS_ASSERT(callee->getClass()->construct != NULL);
    }
#endif
    return vp[1].isMagic();
}

inline bool
IsConstructing(CallReceiver call)
{
    return IsConstructing(call.base());
}

inline const char *
GetFunctionNameBytes(JSContext *cx, JSFunction *fun, JSAutoByteString *bytes)
{
    if (fun->atom)
        return bytes->encode(cx, fun->atom);
    return js_anonymous_str;
}

extern JSFunctionSpec function_methods[];

extern JSBool
Function(JSContext *cx, unsigned argc, Value *vp);

extern bool
IsBuiltinFunctionConstructor(JSFunction *fun);

static inline JSObject *
SkipScopeParent(JSObject *parent)
{
    if (!parent)
        return NULL;
    while (parent->isScope())
        parent = &parent->asScope().enclosingScope();
    return parent;
}

inline JSFunction *
CloneFunctionObject(JSContext *cx, HandleFunction fun, HandleObject parent,
                    gc::AllocKind kind = JSFunction::FinalizeKind)
{
    JS_ASSERT(parent);
    RootedObject proto(cx, parent->global().getOrCreateFunctionPrototype(cx));
    if (!proto)
        return NULL;

    return js_CloneFunctionObject(cx, fun, parent, proto, kind);
}

inline JSFunction *
CloneFunctionObjectIfNotSingleton(JSContext *cx, HandleFunction fun, HandleObject parent)
{
    /*
     * For attempts to clone functions at a function definition opcode,
     * don't perform the clone if the function has singleton type. This
     * was called pessimistically, and we need to preserve the type's
     * property that if it is singleton there is only a single object
     * with its type in existence.
     */
    if (fun->hasSingletonType()) {
        if (!JSObject::setParent(cx, fun, RootedObject(cx, SkipScopeParent(parent))))
            return NULL;
        fun->setEnvironment(parent);
        return fun;
    }

    return CloneFunctionObject(cx, fun, parent);
}

inline JSFunction *
CloneFunctionObject(JSContext *cx, HandleFunction fun)
{
    /*
     * Variant which makes an exact clone of fun, preserving parent and proto.
     * Calling the above version CloneFunctionObject(cx, fun, fun->getParent())
     * is not equivalent: API clients, including XPConnect, can reparent
     * objects so that fun->global() != fun->getProto()->global().
     * See ReparentWrapperIfFound.
     */
    JS_ASSERT(fun->getParent() && fun->getProto());

    if (fun->hasSingletonType())
        return fun;

    return js_CloneFunctionObject(cx, fun,
                                  RootedObject(cx, fun->environment()),
                                  RootedObject(cx, fun->getProto()),
                                  JSFunction::ExtendedFinalizeKind);
}

} /* namespace js */

inline void
JSFunction::setScript(JSScript *script_)
{
    JS_ASSERT(isInterpreted());
    mutableScript() = script_;
}

inline void
JSFunction::initScript(JSScript *script_)
{
    JS_ASSERT(isInterpreted());
    mutableScript().init(script_);
}

#endif /* jsfuninlines_h___ */
