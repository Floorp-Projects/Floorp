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

#include "jsobjinlines.h"

using namespace js;

static int sWrapperFamily = 0;

bool
JSObject::isWrapper() const
{
    return isProxy() && getProxyHandler()->family() == &sWrapperFamily;
}

JSObject *
JSObject::unwrap()
{
    JSObject *wrapped = this;
    while (wrapped->isWrapper())
        wrapped = wrapped->getProxyPrivate().asObjectOrNull();
    return wrapped;
}

JSWrapper::JSWrapper(void *kind) : JSProxyHandler(&sWrapperFamily), mKind(kind)
{
}

JSWrapper::~JSWrapper()
{
}

bool
JSWrapper::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                 PropertyDescriptor *desc)
{
    JSObject *wobj = wrappedObject(proxy);
    return JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED,
                                        Jsvalify(desc));
}

bool
JSWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                    PropertyDescriptor *desc)
{
    return JS_GetPropertyDescriptorById(cx, wrappedObject(proxy), id, JSRESOLVE_QUALIFIED,
                                        Jsvalify(desc));
}

bool
JSWrapper::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                          PropertyDescriptor *desc)
{
    return JS_DefinePropertyById(cx, wrappedObject(proxy), id, Jsvalify(desc->value),
                                 Jsvalify(desc->getter), Jsvalify(desc->setter),
                                 desc->attrs);
}

bool
JSWrapper::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return GetPropertyNames(cx, wrappedObject(proxy), JSITER_OWNONLY | JSITER_HIDDEN, props);
}

bool
JSWrapper::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoValueRooter tvr(cx);
    if (!JS_DeletePropertyById2(cx, wrappedObject(proxy), id, tvr.jsval_addr()))
        return false;
    *bp = js_ValueToBoolean(tvr.value());
    return true;
}

bool
JSWrapper::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return GetPropertyNames(cx, wrappedObject(proxy), 0, props);
}

bool
JSWrapper::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    vp->setUndefined();
    return true;
}

bool
JSWrapper::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSBool found;
    if (!JS_HasPropertyById(cx, wrappedObject(proxy), id, &found))
        return false;
    *bp = !!found;
    return true;
}

bool
JSWrapper::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    PropertyDescriptor desc;
    JSObject *wobj = wrappedObject(proxy);
    if (!JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED, Jsvalify(&desc)))
        return false;
    *bp = (desc.obj == wobj);
    return true;
}

bool
JSWrapper::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    return JS_GetPropertyById(cx, wrappedObject(proxy), id, Jsvalify(vp));
}

bool
JSWrapper::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    return JS_SetPropertyById(cx, wrappedObject(proxy), id, Jsvalify(vp));
}

bool
JSWrapper::enumerateOwn(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return GetPropertyNames(cx, wrappedObject(proxy), JSITER_OWNONLY, props);
}

bool
JSWrapper::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    return GetIterator(cx, wrappedObject(proxy), flags, vp);
}

void
JSWrapper::trace(JSTracer *trc, JSObject *proxy)
{
    JS_CALL_OBJECT_TRACER(trc, wrappedObject(proxy), "wrappedObject");
}

static int sTransparentWrapperKind;

JSWrapper JSWrapper::singleton(&sTransparentWrapperKind);

JSObject *
JSWrapper::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
               JSProxyHandler *handler)
{
    return NewProxyObject(cx, handler, ObjectTag(*obj), proto, parent,
                          obj->isCallable() ? obj : NULL, NULL);
}

/* Compartments. */

namespace js {

extern JSObject *
TransparentObjectWrapper(JSContext *cx, JSObject *obj, JSObject *wrappedProto)
{
    return JSWrapper::New(cx, obj, wrappedProto, NULL, &JSCrossCompartmentWrapper::singleton);
}

}

JSCompartment::JSCompartment(JSRuntime *rt)
  : rt(rt), principals(NULL), marked(false)
{
}

JSCompartment::~JSCompartment()
{
}

bool
JSCompartment::init()
{
    return crossCompartmentWrappers.init();
}

bool
JSCompartment::wrap(JSContext *cx, Value *vp)
{
    JS_ASSERT(cx->compartment == this);

    JS_CHECK_RECURSION(cx, return false);

    /* Only GC things have to be wrapped or copied. */
    if (!vp->isMarkable())
        return true;

    /* Static strings do not have to be wrapped. */
    if (vp->isString() && JSString::isStatic(vp->asString()))
        return true;

    /* Unwrap incoming objects. */
    if (vp->isObject()) {
        JSObject *obj =  vp->asObject().unwrap();
        vp->setObject(*obj);
        /* If the wrapped object is already in this compartment, we are done. */
        if (obj->getCompartment(cx) == this)
            return true;
    }

    /* If we already have a wrapper for this value, use it. */
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(*vp)) {
        *vp = p->value;
        return true;
    }

    if (vp->isString()) {
        Value orig = *vp;
        JSString *str = vp->asString();
        JSString *wrapped = js_NewStringCopyN(cx, str->chars(), str->length());
        if (!wrapped)
            return false;
        vp->setString(wrapped);
        return crossCompartmentWrappers.put(orig, *vp);
    }

    JSObject *obj = &vp->asObject();

    /*
     * Recurse to wrap the prototype. Long prototype chains will run out of
     * stack, causing an error in CHECK_RECURSE.
     *
     * Wrapping the proto before creating the new wrapper and adding it to the
     * cache helps avoid leaving a bad entry in the cache on OOM. But note that
     * if we wrapped both proto and parent, we would get infinite recursion
     * here (since Object.prototype->parent->proto leads to Object.prototype
     * itself).
     */
    AutoObjectRooter proto(cx, obj->getProto());
    if (!wrap(cx, proto.addr()))
        return false;

    JSObject *wrapper = cx->runtime->wrapObjectCallback(cx, obj, proto.object());
    if (!wrapper)
        return false;
    wrapper->setProto(proto.object());
    vp->setObject(*wrapper);
    if (!crossCompartmentWrappers.put(wrapper->getProxyPrivate(), *vp))
        return false;

    /*
     * Wrappers should really be parented to the wrapped parent of the wrapped
     * object, but in that case a wrapped global object would have a NULL
     * parent without being a proper global object (JSCLASS_IS_GLOBAL). Instead,
     * we parent all wrappers to the global object in their home compartment.
     * This loses us some transparency, and is generally very cheesy.
     */
    JSObject *global = cx->fp ? cx->fp->scopeChain->getGlobal() : cx->globalObject;
    wrapper->setParent(global);
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSString **strp)
{
    AutoValueRooter tvr(cx, StringTag(*strp));
    if (!wrap(cx, tvr.addr()))
        return false;
    *strp = tvr.value().asString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSObject **objp)
{
    if (!*objp)
        return true;
    AutoValueRooter tvr(cx, ObjectTag(**objp));
    if (!wrap(cx, tvr.addr()))
        return false;
    *objp = &tvr.value().asObject();
    return true;
}

bool
JSCompartment::wrapId(JSContext *cx, jsid *idp) {
    if (JSID_IS_INT(*idp))
        return true;
    AutoValueRooter tvr(cx, IdToValue(*idp));
    if (!wrap(cx, tvr.addr()))
        return false;
    return ValueToId(cx, tvr.value(), idp);
}

bool
JSCompartment::wrap(JSContext *cx, PropertyOp *propp)
{
    union {
        PropertyOp op;
        jsval v;
    } u;
    u.op = *propp;
    if (!wrap(cx, &Valueify(u.v)))
        return false;
    *propp = u.op;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyDescriptor *desc) {
    return wrap(cx, &desc->obj) &&
           (!(desc->attrs & JSPROP_GETTER) || wrap(cx, &desc->getter)) &&
           (!(desc->attrs & JSPROP_SETTER) || wrap(cx, &desc->setter)) &&
           wrap(cx, &desc->value);
}

bool
JSCompartment::wrap(JSContext *cx, AutoIdVector &props) {
    jsid *vector = props.begin();
    jsint length = props.length();
    for (size_t n = 0; n < size_t(length); ++n) {
        if (!wrapId(cx, &vector[n]))
            return false;
    }
    return true;
}

bool
JSCompartment::wrapException(JSContext *cx) {
    JS_ASSERT(cx->compartment == this);

    if (cx->throwing) {
        AutoValueRooter tvr(cx, cx->exception);
        cx->throwing = false;
        cx->exception = NullTag();
        if (wrap(cx, tvr.addr())) {
            cx->throwing = true;
            cx->exception = tvr.value();
        }
        return false;
    }
    return true;
}

void
JSCompartment::sweep(JSContext *cx)
{
    /* Remove dead wrappers from the table. */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        if (js_IsAboutToBeFinalized(e.front().value.asGCThing()))
            e.removeFront();
    }
}

static bool
SetupFakeFrame(JSContext *cx, ExecuteFrameGuard &frame, JSFrameRegs &regs, JSObject *obj)
{
    const uintN vplen = 2;
    const uintN nfixed = 0;
    if (!cx->stack().getExecuteFrame(cx, js_GetTopStackFrame(cx), vplen, nfixed, frame))
        return false;

    Value *vp = frame.getvp();
    vp[0] = UndefinedTag();
    vp[1] = UndefinedTag();

    JSStackFrame *fp = frame.getFrame();
    PodZero(fp);  // fp->fun and fp->script are both NULL
    fp->argv = vp + 2;
    fp->scopeChain = obj->getGlobal();

    regs.pc = NULL;
    regs.sp = fp->slots();

    cx->stack().pushExecuteFrame(cx, frame, regs, NULL);
    return true;
}

AutoCompartment::AutoCompartment(JSContext *cx, JSObject *target)
    : context(cx),
      origin(cx->compartment),
      target(target),
      destination(target->getCompartment(cx)),
      statics(cx),
      input(cx)
{
    JS_ASSERT(origin != destination);  // necessary for entered() implementation
}

AutoCompartment::~AutoCompartment()
{
    if (entered())
        leave();
}

bool
AutoCompartment::enter()
{
    JS_ASSERT(!entered());
    context->compartment = destination;
    frame.construct();
    bool ok = SetupFakeFrame(context, frame.ref(), regs, target);
    if (!ok) {
        frame.destroy();
        context->compartment = origin;
    }
    js_SaveAndClearRegExpStatics(context, &statics, &input);
    return ok;
}

void
AutoCompartment::leave()
{
    JS_ASSERT(entered());
    js_RestoreRegExpStatics(context, &statics, &input);
    frame.destroy();
    context->compartment = origin;
    origin->wrapException(context);
}

/* Cross compartment wrappers. */

static int sCrossCompartmentWrapperKind = 0;

bool
JSObject::isCrossCompartmentWrapper() const
{
    return isWrapper() &&
           static_cast<JSWrapper *>(getProxyHandler())->kind() == &sCrossCompartmentWrapperKind;
}

JSCrossCompartmentWrapper::JSCrossCompartmentWrapper() : JSWrapper(&sCrossCompartmentWrapperKind)
{
}

JSCrossCompartmentWrapper::~JSCrossCompartmentWrapper()
{
}

#define PIERCE(cx, proxy, mode, pre, op, post)                               \
    JS_BEGIN_MACRO                                                           \
        AutoCompartment call(cx, wrappedObject(proxy));                      \
        if (!call.enter() || !(pre) || !enter(cx, proxy, id, mode) || !(op)) \
            return false;                                                    \
        leave(cx, proxy);                                                    \
        call.leave();                                                        \
        return (post);                                                       \
    JS_END_MACRO

#define NOTHING (true)

bool
JSCrossCompartmentWrapper::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    PIERCE(cx, proxy, GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::getPropertyDescriptor(cx, proxy, id, desc),
           call.origin->wrap(cx, desc));
}

bool
JSCrossCompartmentWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    PIERCE(cx, proxy, GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::getOwnPropertyDescriptor(cx, proxy, id, desc),
           call.origin->wrap(cx, desc));
}

bool
JSCrossCompartmentWrapper::defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    AutoPropertyDescriptorRooter desc2(cx, desc);
    PIERCE(cx, proxy, SET,
           call.destination->wrapId(cx, &id) && call.destination->wrap(cx, &desc2),
           JSWrapper::getOwnPropertyDescriptor(cx, proxy, id, &desc2),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    jsid id = JSID_VOID;
    PIERCE(cx, proxy, GET,
           NOTHING,
           JSWrapper::getOwnPropertyNames(cx, proxy, props),
           call.origin->wrap(cx, props));
}

bool
JSCrossCompartmentWrapper::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    PIERCE(cx, proxy, SET,
           call.destination->wrapId(cx, &id),
           JSWrapper::delete_(cx, proxy, id, bp),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    jsid id = JSID_VOID;
    PIERCE(cx, proxy, GET,
           NOTHING,
           JSWrapper::enumerate(cx, proxy, props),
           call.origin->wrap(cx, props));
}

bool
JSCrossCompartmentWrapper::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    PIERCE(cx, proxy, GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::has(cx, proxy, id, bp),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    PIERCE(cx, proxy, GET,
           call.destination->wrapId(cx, &id),
           JSWrapper::hasOwn(cx, proxy, id, bp),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    PIERCE(cx, proxy, GET,
           call.destination->wrap(cx, &receiver) && call.destination->wrapId(cx, &id),
           JSWrapper::get(cx, proxy, receiver, id, vp),
           call.origin->wrap(cx, vp));
}

bool
JSCrossCompartmentWrapper::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    AutoValueRooter tvr(cx, *vp);
    PIERCE(cx, proxy, SET,
           call.destination->wrap(cx, &receiver) && call.destination->wrapId(cx, &id) && call.destination->wrap(cx, tvr.addr()),
           JSWrapper::set(cx, proxy, receiver, id, tvr.addr()),
           NOTHING);
}

bool
JSCrossCompartmentWrapper::enumerateOwn(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    jsid id = JSID_VOID;
    PIERCE(cx, proxy, GET,
           NOTHING,
           JSWrapper::enumerateOwn(cx, proxy, props),
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
           (obj = &vp->asObject())->getClass() == &js_IteratorClass.base &&
           (obj->getNativeIterator()->flags & JSITER_ENUMERATE);
}

static bool
Reify(JSContext *cx, JSCompartment *origin, Value *vp)
{
    JSObject *iterObj = &vp->asObject();
    NativeIterator *ni = iterObj->getNativeIterator();

    /* Wrap the iteratee. */
    JSObject *obj = ni->obj;
    if (!origin->wrap(cx, &obj))
        return false;

    /* Wrap the elements in the iterator's snapshot. */
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

        if (!VectorToKeyIterator(cx, obj, ni->flags, keys, vp))
            return false;
    } else {
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

        if (!VectorToValueIterator(cx, obj, ni->flags, vals, vp))
            return false;
    }

    return js_CloseIterator(cx, *vp);
}

bool
JSCrossCompartmentWrapper::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    jsid id = JSID_VOID;
    PIERCE(cx, proxy, GET,
           NOTHING,
           JSWrapper::iterate(cx, proxy, flags, vp),
           CanReify(vp) ? Reify(cx, call.origin, vp) : call.origin->wrap(cx, vp));
}

bool
JSCrossCompartmentWrapper::call(JSContext *cx, JSObject *proxy, uintN argc, Value *vp)
{
    AutoCompartment call(cx, wrappedObject(proxy));
    if (!call.enter())
        return false;

    vp[0] = ObjectTag(*call.target);
    if (!call.destination->wrap(cx, &vp[1]))
        return false;
    Value *argv = JS_ARGV(cx, vp);
    for (size_t n = 0; n < argc; ++n) {
        if (!call.destination->wrap(cx, &argv[n]))
            return false;
    }
    Value *fakevp = call.getvp();
    fakevp[0] = vp[0];
    fakevp[1] = vp[1];
    if (!enter(cx, proxy, JSID_VOID, GET) || !JSWrapper::call(cx, proxy, argc, vp))
        return false;

    leave(cx, proxy);
    call.leave();
    return call.origin->wrap(cx, vp);
}

bool
JSCrossCompartmentWrapper::construct(JSContext *cx, JSObject *proxy, JSObject *receiver,
                                     uintN argc, Value *argv, Value *rval)
{
    AutoCompartment call(cx, wrappedObject(proxy));
    if (!call.enter())
        return false;

    for (size_t n = 0; n < argc; ++n) {
        if (!call.destination->wrap(cx, &argv[n]))
            return false;
    }
    Value *vp = call.getvp();
    vp[0] = ObjectTag(*call.target);
    if (!call.destination->wrap(cx, &receiver) ||
        !enter(cx, proxy, JSID_VOID, GET) ||
        !JSWrapper::construct(cx, proxy, receiver, argc, argv, rval)) {
        return false;
    }

    leave(cx, proxy);
    call.leave();
    return call.origin->wrap(cx, rval) &&
           call.origin->wrapException(cx);
}

JSString *
JSCrossCompartmentWrapper::obj_toString(JSContext *cx, JSObject *proxy)
{
    AutoCompartment call(cx, wrappedObject(proxy));
    if (!call.enter())
        return NULL;

    if (!enter(cx, proxy, JSID_VOID, GET))
        return NULL;
    JSString *str = JSWrapper::obj_toString(cx, proxy);
    if (!str)
        return NULL;
    AutoStringRooter tvr(cx, str);

    leave(cx, proxy);
    call.leave();
    if (!call.origin->wrap(cx, &str))
        return NULL;
    return str;
}

JSString *
JSCrossCompartmentWrapper::fun_toString(JSContext *cx, JSObject *proxy, uintN indent)
{
    AutoCompartment call(cx, wrappedObject(proxy));
    if (!call.enter())
        return NULL;

    if (!enter(cx, proxy, JSID_VOID, GET))
        return NULL;
    JSString *str = JSWrapper::fun_toString(cx, proxy, indent);
    if (!str)
        return NULL;
    AutoStringRooter tvr(cx, str);

    leave(cx, proxy);
    call.leave();
    if (!call.origin->wrap(cx, &str))
        return NULL;
    return str;
}

bool
JSCrossCompartmentWrapper::enter(JSContext *cx, JSObject *proxy, jsid id, Mode mode)
{
    return true;
}

void
JSCrossCompartmentWrapper::leave(JSContext *cx, JSObject *proxy)
{
}

JSCrossCompartmentWrapper JSCrossCompartmentWrapper::singleton;
