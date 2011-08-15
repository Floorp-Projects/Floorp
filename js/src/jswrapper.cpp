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
#include "jsexn.h"
#include "jsgc.h"
#include "jsgcmark.h"
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

bool
JSObject::isCrossCompartmentWrapper() const
{
    return isWrapper() && !!(getWrapperHandler()->flags() & JSWrapper::CROSS_COMPARTMENT);
}

JSWrapper *
JSObject::getWrapperHandler() const
{
    JS_ASSERT(isWrapper());
    return static_cast<JSWrapper *>(getProxyHandler());
}

JSObject *
JSObject::unwrap(uintN *flagsp)
{
    JSObject *wrapped = this;
    uintN flags = 0;
    while (wrapped->isWrapper()) {
        flags |= static_cast<JSWrapper *>(wrapped->getProxyHandler())->flags();
        wrapped = wrapped->getProxyPrivate().toObjectOrNull();
        if (wrapped->getClass()->ext.innerObject)
            break;
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
        bool status;                                                         \
        if (!enter(cx, wrapper, id, act, &status))                           \
            return status;                                                   \
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
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(JS_GetPropertyDescriptorById(cx, wrappedObject(wrapper), id, JSRESOLVE_QUALIFIED,
                                         Jsvalify(desc)), set ? SET : GET);
}

static bool
GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, uintN flags, JSPropertyDescriptor *desc)
{
    // If obj is a proxy, we can do better than just guessing. This is
    // important for certain types of wrappers that wrap other wrappers.
    if (obj->isProxy()) {
        return JSProxy::getOwnPropertyDescriptor(cx, obj, id,
                                                 flags & JSRESOLVE_ASSIGNING,
                                                 Valueify(desc));
    }

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
    desc->obj = NULL; // default result if we refuse to perform this action
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
    // if we refuse to perform this action, props remains empty
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
    *bp = true; // default result if we refuse to perform this action
    Value v;
    SET(JS_DeletePropertyById2(cx, wrappedObject(wrapper), id, Jsvalify(&v)) &&
        ValueToBoolean(&v, bp));
}

bool
JSWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
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
    *bp = false; // default result if we refuse to perform this action
    JSBool found;
    GET(JS_HasPropertyById(cx, wrappedObject(wrapper), id, &found) &&
        Cond(found, bp));
}

bool
JSWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    PropertyDescriptor desc;
    JSObject *wobj = wrappedObject(wrapper);
    GET(JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED, Jsvalify(&desc)) &&
        Cond(desc.obj == wobj, bp));
}

bool
JSWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    GET(wrappedObject(wrapper)->getProperty(cx, receiver, id, vp));
}

bool
JSWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, bool strict,
               Value *vp)
{
    // FIXME (bug 596351): Need deal with strict mode.
    SET(wrappedObject(wrapper)->setProperty(cx, id, vp, false));
}

bool
JSWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    const jsid id = JSID_VOID;
    GET(GetPropertyNames(cx, wrappedObject(wrapper), JSITER_OWNONLY, &props));
}

bool
JSWrapper::iterate(JSContext *cx, JSObject *wrapper, uintN flags, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(GetIterator(cx, wrappedObject(wrapper), flags, vp));
}

bool
JSWrapper::call(JSContext *cx, JSObject *wrapper, uintN argc, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    CHECKED(JSProxyHandler::call(cx, wrapper, argc, vp), CALL);
}

bool
JSWrapper::construct(JSContext *cx, JSObject *wrapper, uintN argc, Value *argv, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(JSProxyHandler::construct(cx, wrapper, argc, argv, vp));
}

bool
JSWrapper::hasInstance(JSContext *cx, JSObject *wrapper, const Value *vp, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    JSBool b = JS_FALSE;
    GET(JS_HasInstance(cx, wrappedObject(wrapper), Jsvalify(*vp), &b) && Cond(b, bp));
}

JSType
JSWrapper::typeOf(JSContext *cx, JSObject *wrapper)
{
    return TypeOfValue(cx, ObjectValue(*wrappedObject(wrapper)));
}

JSString *
JSWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    bool status;
    if (!enter(cx, wrapper, JSID_VOID, GET, &status)) {
        if (status) {
            // Perform some default behavior that doesn't leak any information.
            return JS_NewStringCopyZ(cx, "[object Object]");
        }
        return NULL;
    }
    JSString *str = obj_toStringHelper(cx, wrappedObject(wrapper));
    leave(cx, wrapper);
    return str;
}

JSString *
JSWrapper::fun_toString(JSContext *cx, JSObject *wrapper, uintN indent)
{
    bool status;
    if (!enter(cx, wrapper, JSID_VOID, GET, &status)) {
        if (status) {
            // Perform some default behavior that doesn't leak any information.
            if (wrapper->isCallable())
                return JS_NewStringCopyZ(cx, "function () {\n    [native code]\n}");
            js::Value v = ObjectValue(*wrapper);
            js_ReportIsNotFunction(cx, &v, 0);
            return NULL;
        }
        return NULL;
    }
    JSString *str = JSProxyHandler::fun_toString(cx, wrapper, indent);
    leave(cx, wrapper);
    return str;
}

bool
JSWrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp)
{
    *vp = ObjectValue(*wrappedObject(wrapper));
    if (hint == JSTYPE_VOID)
        return ToPrimitive(cx, vp);
    return ToPrimitive(cx, hint, vp);
}

void
JSWrapper::trace(JSTracer *trc, JSObject *wrapper)
{
    MarkObject(trc, *wrappedObject(wrapper), "wrappedObject");
}

bool
JSWrapper::enter(JSContext *cx, JSObject *wrapper, jsid id, Action act, bool *bp)
{
    *bp = true;
    return true;
}

void
JSWrapper::leave(JSContext *cx, JSObject *wrapper)
{
}

JSWrapper JSWrapper::singleton((uintN)0);

JSObject *
JSWrapper::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
               JSWrapper *handler)
{
    JS_ASSERT(parent);
    if (obj->isXML()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_WRAP_XML_OBJECT);
        return NULL;
    }
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
    return JSWrapper::New(cx, obj, wrappedProto, parent, &JSCrossCompartmentWrapper::singleton);
}

}

ForceFrame::ForceFrame(JSContext *cx, JSObject *target)
    : context(cx),
      target(target),
      frame(NULL)
{
}

ForceFrame::~ForceFrame()
{
    context->delete_(frame);
}

bool
ForceFrame::enter()
{
    frame = context->new_<DummyFrameGuard>();
    if (!frame)
       return false;
    LeaveTrace(context);

    JS_ASSERT(context->compartment == target->compartment());

    JSObject *scopeChain = target->getGlobal();
    JS_ASSERT(scopeChain->isNative());

    return context->stack.pushDummyFrame(context, REPORT_ERROR, *scopeChain, frame);
}

AutoCompartment::AutoCompartment(JSContext *cx, JSObject *target)
    : context(cx),
      origin(cx->compartment),
      target(target),
      destination(target->getCompartment()),
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

        JSObject *scopeChain = target->getGlobal();
        JS_ASSERT(scopeChain->isNative());

        frame.construct();

        /*
         * Set the compartment eagerly so that pushDummyFrame associates the
         * resource allocation request with 'destination' instead of 'origin'.
         * (This is important when content has overflowed the stack and chrome
         * is preparing to run JS to throw up a slow script dialog.) However,
         * if an exception is thrown, we need it to be in origin's compartment
         * so be careful to only report after restoring.
         */
        context->compartment = destination;
        if (!context->stack.pushDummyFrame(context, DONT_REPORT_ERROR, *scopeChain, &frame.ref())) {
            context->compartment = origin;
            js_ReportOverRecursed(context);
            return false;
        }

        if (context->isExceptionPending())
            context->wrapPendingException();
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
    }
    entered = false;
}

ErrorCopier::~ErrorCopier()
{
    JSContext *cx = ac.context;
    if (cx->compartment == ac.destination &&
        ac.origin != ac.destination &&
        cx->isExceptionPending())
    {
        Value exc = cx->getPendingException();
        if (exc.isObject() && exc.toObject().isError()) {
            cx->clearPendingException();
            ac.leave();
            JSObject *copyobj = js_CopyErrorObject(cx, &exc.toObject(), scope);
            if (copyobj)
                cx->setPendingException(ObjectValue(*copyobj));
        }
    }
}

/* Cross compartment wrappers. */

JSCrossCompartmentWrapper::JSCrossCompartmentWrapper(uintN flags)
  : JSWrapper(CROSS_COMPARTMENT | flags)
{
}

JSCrossCompartmentWrapper::~JSCrossCompartmentWrapper()
{
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
JSCrossCompartmentWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                               bool strict, Value *vp)
{
    AutoValueRooter tvr(cx, *vp);
    PIERCE(cx, wrapper, SET,
           call.destination->wrap(cx, &receiver) &&
           call.destination->wrapId(cx, &id) &&
           call.destination->wrap(cx, tvr.addr()),
           JSWrapper::set(cx, wrapper, receiver, id, strict, tvr.addr()),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           JSWrapper::keys(cx, wrapper, props),
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

struct AutoCloseIterator
{
    AutoCloseIterator(JSContext *cx, JSObject *obj) : cx(cx), obj(obj) {}

    ~AutoCloseIterator() { if (obj) js_CloseIterator(cx, obj); }

    void clear() { obj = NULL; }

  private:
    JSContext *cx;
    JSObject *obj;
};

static bool
Reify(JSContext *cx, JSCompartment *origin, Value *vp)
{
    JSObject *iterObj = &vp->toObject();
    NativeIterator *ni = iterObj->getNativeIterator();

    AutoCloseIterator close(cx, iterObj);

    /* Wrap the iteratee. */
    JSObject *obj = ni->obj;
    if (!origin->wrap(cx, &obj))
        return false;

    /*
     * Wrap the elements in the iterator's snapshot.
     * N.B. the order of closing/creating iterators is important due to the
     * implicit cx->enumerators state.
     */
    size_t length = ni->numKeys();
    bool isKeyIter = ni->isKeyIter();
    AutoIdVector keys(cx);
    if (length > 0) {
        if (!keys.resize(length))
            return false;
        for (size_t i = 0; i < length; ++i) {
            keys[i] = ni->begin()[i];
            if (!origin->wrapId(cx, &keys[i]))
                return false;
        }
    }

    close.clear();
    if (!js_CloseIterator(cx, iterObj))
        return false;

    if (isKeyIter)
        return VectorToKeyIterator(cx, obj, ni->flags, keys, vp);
    return VectorToValueIterator(cx, obj, ni->flags, keys, vp); 
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
    return call.origin->wrap(cx, rval);
}

bool
JSCrossCompartmentWrapper::hasInstance(JSContext *cx, JSObject *wrapper, const Value *vp, bool *bp)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    Value v = *vp;
    if (!call.destination->wrap(cx, &v))
        return false;
    return JSWrapper::hasInstance(cx, wrapper, &v, bp);
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

bool
JSCrossCompartmentWrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    if (!JSWrapper::defaultValue(cx, wrapper, hint, vp))
        return false;

    call.leave();
    return call.origin->wrap(cx, vp);
}

void
JSCrossCompartmentWrapper::trace(JSTracer *trc, JSObject *wrapper)
{
    MarkCrossCompartmentObject(trc, *wrappedObject(wrapper), "wrappedObject");
}

JSCrossCompartmentWrapper JSCrossCompartmentWrapper::singleton(0u);
