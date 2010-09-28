/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jsapi.h"
#include "jscntxt.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsregexp.h"
#include "jswrapper.h"
#include "methodjit/PolyIC.h"
#include "methodjit/MonoIC.h"
#ifdef JS_METHODJIT
# include "assembler/jit/ExecutableAllocator.h"
#endif
#include "jscompartment.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

static int sWrapperFamily;

void *
JSWrapper::getWrapperFamily()
{
    return &sWrapperFamily;
}

bool
JSObject::isWrapper() const
{
    return isProxy() && getProxyHandler()->family() == &sWrapperFamily;
}

JSObject *
JSObject::unwrap(uintN *flagsp)
{
    JSObject *wrapped = this;
    uintN flags = 0;
    if (wrapped->isWrapper()) {
        flags |= static_cast<JSWrapper *>(wrapped->getProxyHandler())->flags();
        wrapped = wrapped->getProxyPrivate().toObjectOrNull();
    }
    if (flagsp)
        *flagsp = flags;
    return wrapped;
}

JSWrapper::JSWrapper(uintN flags) : JSProxyHandler(&sWrapperFamily), mFlags(flags)
{
}

JSWrapper::~JSWrapper()
{
}

#define CHECKED(op, act)                                                     \
    JS_BEGIN_MACRO                                                           \
        if (!enter(cx, wrapper, id, act))                                    \
            return false;                                                    \
        bool ok = (op);                                                      \
        leave(cx, wrapper);                                                  \
        return ok;                                                           \
    JS_END_MACRO

#define SET(action) CHECKED(action, SET)
#define GET(action) CHECKED(action, GET)

bool
JSWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                 bool set, PropertyDescriptor *desc)
{
    CHECKED(JS_GetPropertyDescriptorById(cx, wrappedObject(wrapper), id, JSRESOLVE_QUALIFIED,
                                         Jsvalify(desc)), set ? SET : GET);
}

static bool
GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, uintN flags, JSPropertyDescriptor *desc)
{
    if (!JS_GetPropertyDescriptorById(cx, obj, id, flags, desc))
        return false;
    if (desc->obj != obj)
        desc->obj = NULL;
    return true;
}

bool
JSWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                    PropertyDescriptor *desc)
{
    CHECKED(GetOwnPropertyDescriptor(cx, wrappedObject(wrapper), id, JSRESOLVE_QUALIFIED,
                                     Jsvalify(desc)), set ? SET : GET);
}

bool
JSWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                          PropertyDescriptor *desc)
{
    SET(JS_DefinePropertyById(cx, wrappedObject(wrapper), id, Jsvalify(desc->value),
                              Jsvalify(desc->getter), Jsvalify(desc->setter), desc->attrs));
}

bool
JSWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    jsid id = JSID_VOID;
    GET(GetPropertyNames(cx, wrappedObject(wrapper), JSITER_OWNONLY | JSITER_HIDDEN, &props));
}

static bool
ValueToBoolean(Value *vp, bool *bp)
{
    *bp = js_ValueToBoolean(*vp);
    return true;
}

bool
JSWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    Value v;
    SET(JS_DeletePropertyById2(cx, wrappedObject(wrapper), id, Jsvalify(&v)) &&
        ValueToBoolean(&v, bp));
}

bool
JSWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    static jsid id = JSID_VOID;
    GET(GetPropertyNames(cx, wrappedObject(wrapper), 0, &props));
}

bool
JSWrapper::fix(JSContext *cx, JSObject *wrapper, Value *vp)
{
    vp->setUndefined();
    return true;
}

static bool
Cond(JSBool b, bool *bp)
{
    *bp = !!b;
    return true;
}

bool
JSWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JSBool found;
    GET(JS_HasPropertyById(cx, wrappedObject(wrapper), id, &found) &&
        Cond(found, bp));
}

bool
JSWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PropertyDescriptor desc;
    JSObject *wobj = wrappedObject(wrapper);
    GET(JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED, Jsvalify(&desc)) &&
        Cond(desc.obj == wobj, bp));
}

bool
JSWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    GET(JS_GetPropertyById(cx, wrappedObject(wrapper), id, Jsvalify(vp)));
}

bool
JSWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    SET(JS_SetPropertyById(cx, wrappedObject(wrapper), id, Jsvalify(vp)));
}

bool
JSWrapper::enumerateOwn(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    const jsid id = JSID_VOID;
    GET(GetPropertyNames(cx, wrappedObject(wrapper), JSITER_OWNONLY, &props));
}

bool
JSWrapper::iterate(JSContext *cx, JSObject *wrapper, uintN flags, Value *vp)
{
    const jsid id = JSID_VOID;
    GET(GetIterator(cx, wrappedObject(wrapper), flags, vp));
}

bool
JSWrapper::call(JSContext *cx, JSObject *wrapper, uintN argc, Value *vp)
{
    const jsid id = JSID_VOID;
    CHECKED(JSProxyHandler::call(cx, wrapper, argc, vp), CALL);
}

bool
JSWrapper::construct(JSContext *cx, JSObject *wrapper, uintN argc, Value *argv, Value *rval)
{
    const jsid id = JSID_VOID;
    GET(JSProxyHandler::construct(cx, wrapper, argc, argv, rval));
}

JSString *
JSWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    JSString *str;
    if (!enter(cx, wrapper, JSID_VOID, GET))
        return NULL;
    str = JSProxyHandler::obj_toString(cx, wrapper);
    leave(cx, wrapper);
    return str;
}

JSString *
JSWrapper::fun_toString(JSContext *cx, JSObject *wrapper, uintN indent)
{
    JSString *str;
    if (!enter(cx, wrapper, JSID_VOID, GET))
        return NULL;
    str = JSProxyHandler::fun_toString(cx, wrapper, indent);
    leave(cx, wrapper);
    return str;
}

void
JSWrapper::trace(JSTracer *trc, JSObject *wrapper)
{
    MarkObject(trc, *wrappedObject(wrapper), "wrappedObject");
}

bool
JSWrapper::enter(JSContext *cx, JSObject *wrapper, jsid id, Action act)
{
    return true;
}

void
JSWrapper::leave(JSContext *cx, JSObject *wrapper)
{
}

JSWrapper JSWrapper::singleton(0);

JSObject *
JSWrapper::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
               JSWrapper *handler)
{
    return NewProxyObject(cx, handler, ObjectValue(*obj), proto, parent,
                          obj->isCallable() ? obj : NULL, NULL);
}

/* Compartments. */

namespace js {

extern JSObject *
TransparentObjectWrapper(JSContext *cx, JSObject *obj, JSObject *wrappedProto, JSObject *parent,
                         uintN flags)
{
    // Allow wrapping outer window proxies.
    JS_ASSERT(!obj->isWrapper() || obj->getClass()->ext.innerObject);
    return JSWrapper::New(cx, obj, wrappedProto, NULL, &JSCrossCompartmentWrapper::singleton);
}

}

AutoCompartment::AutoCompartment(JSContext *cx, JSObject *target)
    : context(cx),
      origin(cx->compartment),
      target(target),
      destination(target->getCompartment()),
      input(cx),
      entered(false)
{
}

AutoCompartment::~AutoCompartment()
{
    if (entered)
        leave();
}

bool
AutoCompartment::enter()
{
    JS_ASSERT(!entered);
    if (origin != destination) {
        LeaveTrace(context);

#ifdef DEBUG
        JSCompartment *oldCompartment = context->compartment;
        context->resetCompartment();
        wasSane = (context->compartment == oldCompartment);
#endif

        context->compartment = destination;
        JSObject *scopeChain = target->getGlobal();
        frame.construct();
        if (!context->stack().pushDummyFrame(context, *scopeChain, &frame.ref())) {
            frame.destroy();
            context->compartment = origin;
            return false;
        }
    }
    entered = true;
    return true;
}

void
AutoCompartment::leave()
{
    JS_ASSERT(entered);
    if (origin != destination) {
        frame.destroy();
        context->resetCompartment();
        JS_ASSERT_IF(wasSane && context->hasfp(), context->compartment == origin);
        context->compartment->wrapException(context);
    }
    entered = false;
}

/* Cross compartment wrappers. */

JSCrossCompartmentWrapper::JSCrossCompartmentWrapper(void *family) : JSWrapper(0)
{
}

JSCrossCompartmentWrapper::JSCrossCompartmentWrapper(uintN flags) : JSWrapper(flags)
{
}

JSCrossCompartmentWrapper::~JSCrossCompartmentWrapper()
{
}

bool
JSCrossCompartmentWrapper::isCrossCompartmentWrapper(JSObject *obj)
{
    return obj->isProxy() && obj->getProxyHandler() == &JSCrossCompartmentWrapper::singleton;
}

#define PIERCE(cx, wrapper, mode, pre, op, post)            \
    JS_BEGIN_MACRO                                          \
        AutoCompartment call(cx, wrappedObject(wrapper));   \
        if (!call.enter())                                  \
            return false;                                   \
        bool ok = (pre) && (op);                            \
        call.leave();                                       \
        return ok && (post);                                \
    JS_END_MACRO

#define NOTHING (true)

bool
JSCrossCompartmentWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                 bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::getPropertyDescriptor(cx, wrapper, id, set, desc),
           call.origin->wrap(cx, desc));
}

bool
JSCrossCompartmentWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                    bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::getOwnPropertyDescriptor(cx, wrapper, id, set, desc),
           call.origin->wrap(cx, desc));
}

bool
JSCrossCompartmentWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc)
{
    AutoPropertyDescriptorRooter desc2(cx, desc);
    PIERCE(cx, wrapper, SET,
           call.destination->wrapId(cx, &id) && call.destination->wrap(cx, &desc2),
           JSWrapper::defineProperty(cx, wrapper, id, &desc2),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           JSWrapper::getOwnPropertyNames(cx, wrapper, props),
           call.origin->wrap(cx, props));
}

bool
JSCrossCompartmentWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, SET,
           call.destination->wrapId(cx, &id),
           JSWrapper::delete_(cx, wrapper, id, bp),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           JSWrapper::enumerate(cx, wrapper, props),
           call.origin->wrap(cx, props));
}

bool
JSCrossCompartmentWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::has(cx, wrapper, id, bp),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::hasOwn(cx, wrapper, id, bp),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           call.destination->wrap(cx, &receiver) && call.destination->wrapId(cx, &id),
           JSWrapper::get(cx, wrapper, receiver, id, vp),
           call.origin->wrap(cx, vp));
}

bool
JSCrossCompartmentWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    AutoValueRooter tvr(cx, *vp);
    PIERCE(cx, wrapper, SET,
           call.destination->wrap(cx, &receiver) && call.destination->wrapId(cx, &id) && call.destination->wrap(cx, tvr.addr()),
           JSWrapper::set(cx, wrapper, receiver, id, tvr.addr()),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::enumerateOwn(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           JSWrapper::enumerateOwn(cx, wrapper, props),
           call.origin->wrap(cx, props));
}

/*
 * We can reify non-escaping iterator objects instead of having to wrap them. This
 * allows fast iteration over objects across a compartment boundary.
 */
static bool
CanReify(Value *vp)
{
    JSObject *obj;
    return vp->isObject() &&
           (obj = &vp->toObject())->getClass() == &js_IteratorClass &&
           (obj->getNativeIterator()->flags & JSITER_ENUMERATE);
}

static bool
Reify(JSContext *cx, JSCompartment *origin, Value *vp)
{
    JSObject *iterObj = &vp->toObject();
    NativeIterator *ni = iterObj->getNativeIterator();

    /* Wrap the iteratee. */
    JSObject *obj = ni->obj;
    if (!origin->wrap(cx, &obj))
        return false;

    /*
     * Wrap the elements in the iterator's snapshot.
     * N.B. the order of closing/creating iterators is important due to the
     * implicit cx->enumerators state.
     */

    if (ni->isKeyIter()) {
        size_t length = ni->numKeys();
        AutoIdVector keys(cx);
        if (length > 0) {
            if (!keys.resize(length))
                return false;
            for (size_t i = 0; i < length; ++i) {
                keys[i] = ni->beginKey()[i];
                if (!origin->wrapId(cx, &keys[i]))
                    return false;
            }
        }

        return js_CloseIterator(cx, iterObj) &&
               VectorToKeyIterator(cx, obj, ni->flags, keys, vp);
    }

    size_t length = ni->numValues();
    AutoValueVector vals(cx);
    if (length > 0) {
        if (!vals.resize(length))
            return false;
        for (size_t i = 0; i < length; ++i) {
            vals[i] = ni->beginValue()[i];
            if (!origin->wrap(cx, &vals[i]))
                return false;
        }

    }

    return js_CloseIterator(cx, iterObj) &&
           VectorToValueIterator(cx, obj, ni->flags, vals, vp);
}

bool
JSCrossCompartmentWrapper::iterate(JSContext *cx, JSObject *wrapper, uintN flags, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           JSWrapper::iterate(cx, wrapper, flags, vp),
           CanReify(vp) ? Reify(cx, call.origin, vp) : call.origin->wrap(cx, vp));
}

bool
JSCrossCompartmentWrapper::call(JSContext *cx, JSObject *wrapper, uintN argc, Value *vp)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    vp[0] = ObjectValue(*call.target);
    if (!call.destination->wrap(cx, &vp[1]))
        return false;
    Value *argv = JS_ARGV(cx, vp);
    for (size_t n = 0; n < argc; ++n) {
        if (!call.destination->wrap(cx, &argv[n]))
            return false;
    }
    if (!JSWrapper::call(cx, wrapper, argc, vp))
        return false;

    call.leave();
    return call.origin->wrap(cx, vp);
}

bool
JSCrossCompartmentWrapper::construct(JSContext *cx, JSObject *wrapper, uintN argc, Value *argv,
                                     Value *rval)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    for (size_t n = 0; n < argc; ++n) {
        if (!call.destination->wrap(cx, &argv[n]))
            return false;
    }
    if (!JSWrapper::construct(cx, wrapper, argc, argv, rval))
        return false;

    call.leave();
    return call.origin->wrap(cx, rval) &&
           call.origin->wrapException(cx);
}

JSString *
JSCrossCompartmentWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return NULL;

    JSString *str = JSWrapper::obj_toString(cx, wrapper);
    if (!str)
        return NULL;

    call.leave();
    if (!call.origin->wrap(cx, &str))
        return NULL;
    return str;
}

JSString *
JSCrossCompartmentWrapper::fun_toString(JSContext *cx, JSObject *wrapper, uintN indent)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return NULL;

    JSString *str = JSWrapper::fun_toString(cx, wrapper, indent);
    if (!str)
        return NULL;

    call.leave();
    if (!call.origin->wrap(cx, &str))
        return NULL;
    return str;
}

JSCrossCompartmentWrapper JSCrossCompartmentWrapper::singleton(0u);
