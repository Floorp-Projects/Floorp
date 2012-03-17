/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
JSFunction::setJoinable()
{
    JS_ASSERT(isInterpreted());
    flags |= JSFUN_JOINABLE;
}

inline bool
JSFunction::isClonedMethod() const
{
    return joinable() && isExtended() && getExtendedSlot(METHOD_OBJECT_SLOT).isObject();
}

inline JSAtom *
JSFunction::methodAtom() const
{
    return (joinable() && isExtended() && getExtendedSlot(METHOD_PROPERTY_SLOT).isString())
           ? (JSAtom *) getExtendedSlot(METHOD_PROPERTY_SLOT).toString()
           : NULL;
}

inline void
JSFunction::setMethodAtom(JSAtom *atom)
{
    JS_ASSERT(joinable());
    setExtendedSlot(METHOD_PROPERTY_SLOT, js::StringValue(atom));
}

inline JSObject *
JSFunction::methodObj() const
{
    JS_ASSERT(joinable());
    return isClonedMethod() ? &getExtendedSlot(METHOD_OBJECT_SLOT).toObject() : NULL;
}

inline void
JSFunction::setMethodObj(JSObject& obj)
{
    JS_ASSERT(joinable());
    setExtendedSlot(METHOD_OBJECT_SLOT, js::ObjectValue(obj));
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
ClassMethodIsNative(JSContext *cx, JSObject *obj, Class *clasp, jsid methodid, JSNative native)
{
    JS_ASSERT(obj->getClass() == clasp);

    Value v;
    if (!HasDataProperty(cx, obj, methodid, &v)) {
        JSObject *proto = obj->getProto();
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

/*
 * Preconditions: funobj->isInterpreted() && !funobj->isFunctionPrototype() &&
 * !funobj->isBoundFunction(). This is sufficient to establish that funobj has
 * a non-configurable non-method .prototype data property, thought it might not
 * have been resolved yet, and its value could be anything.
 *
 * Return the shape of the .prototype property of funobj, resolving it if
 * needed. On error, return NULL.
 *
 * This is not safe to call on trace because it defines properties, which can
 * trigger lookups that could reenter.
 */
const Shape *
LookupInterpretedFunctionPrototype(JSContext *cx, JSObject *funobj);

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
CloneFunctionObject(JSContext *cx, JSFunction *fun, JSObject *parent,
                    gc::AllocKind kind = JSFunction::FinalizeKind)
{
    JS_ASSERT(parent);
    JSObject *proto = parent->global().getOrCreateFunctionPrototype(cx);
    if (!proto)
        return NULL;

    return js_CloneFunctionObject(cx, fun, parent, proto, kind);
}

inline JSFunction *
CloneFunctionObjectIfNotSingleton(JSContext *cx, JSFunction *fun, JSObject *parent)
{
    /*
     * For attempts to clone functions at a function definition opcode or from
     * a method barrier, don't perform the clone if the function has singleton
     * type. This was called pessimistically, and we need to preserve the
     * type's property that if it is singleton there is only a single object
     * with its type in existence.
     */
    if (fun->hasSingletonType()) {
        if (!fun->setParent(cx, SkipScopeParent(parent)))
            return NULL;
        fun->setEnvironment(parent);
        return fun;
    }

    return CloneFunctionObject(cx, fun, parent);
}

inline JSFunction *
CloneFunctionObject(JSContext *cx, JSFunction *fun)
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

    return js_CloneFunctionObject(cx, fun, fun->environment(), fun->getProto(),
                                  JSFunction::ExtendedFinalizeKind);
}

} /* namespace js */

inline void
JSFunction::setScript(JSScript *script_)
{
    JS_ASSERT(isInterpreted());
    script() = script_;
}

inline void
JSFunction::initScript(JSScript *script_)
{
    JS_ASSERT(isInterpreted());
    script().init(script_);
}

#endif /* jsfuninlines_h___ */
