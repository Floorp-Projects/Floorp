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
#include "jswrapper.h"
#include "methodjit/PolyIC.h"
#include "methodjit/MonoIC.h"
#ifdef JS_METHODJIT
# include "assembler/jit/ExecutableAllocator.h"
#endif
#include "jscompartment.h"

#include "jsobjinlines.h"

#include "vm/RegExpObject-inl.h"

using namespace js;
using namespace js::gc;

namespace js {
int sWrapperFamily;
}

void *
Wrapper::getWrapperFamily()
{
    return &sWrapperFamily;
}

JS_FRIEND_API(JSObject *)
js::UnwrapObject(JSObject *wrapped, bool stopAtOuter, unsigned *flagsp)
{
    unsigned flags = 0;
    while (wrapped->isWrapper()) {
        flags |= static_cast<Wrapper *>(GetProxyHandler(wrapped))->flags();
        wrapped = GetProxyPrivate(wrapped).toObjectOrNull();
        if (stopAtOuter && wrapped->getClass()->ext.innerObject)
            break;
    }
    if (flagsp)
        *flagsp = flags;
    return wrapped;
}

bool
js::IsCrossCompartmentWrapper(const JSObject *wrapper)
{
    return wrapper->isWrapper() &&
           !!(Wrapper::wrapperHandler(wrapper)->flags() & Wrapper::CROSS_COMPARTMENT);
}

Wrapper::Wrapper(unsigned flags) : ProxyHandler(&sWrapperFamily), mFlags(flags)
{
}

Wrapper::~Wrapper()
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
Wrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                               PropertyDescriptor *desc)
{
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(JS_GetPropertyDescriptorById(cx, wrappedObject(wrapper), id, JSRESOLVE_QUALIFIED, desc),
            set ? SET : GET);
}

static bool
GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, unsigned flags, JSPropertyDescriptor *desc)
{
    // If obj is a proxy, we can do better than just guessing. This is
    // important for certain types of wrappers that wrap other wrappers.
    if (obj->isProxy())
        return Proxy::getOwnPropertyDescriptor(cx, obj, id, flags & JSRESOLVE_ASSIGNING, desc);

    if (!JS_GetPropertyDescriptorById(cx, obj, id, flags, desc))
        return false;
    if (desc->obj != obj)
        desc->obj = NULL;
    return true;
}

bool
Wrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                  PropertyDescriptor *desc)
{
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(GetOwnPropertyDescriptor(cx, wrappedObject(wrapper), id, JSRESOLVE_QUALIFIED, desc),
            set ? SET : GET);
}

bool
Wrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc)
{
    SET(JS_DefinePropertyById(cx, wrappedObject(wrapper), id, desc->value,
                              desc->getter, desc->setter, desc->attrs));
}

bool
Wrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
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
Wrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = true; // default result if we refuse to perform this action
    Value v;
    SET(JS_DeletePropertyById2(cx, wrappedObject(wrapper), id, &v) &&
        ValueToBoolean(&v, bp));
}

bool
Wrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    static jsid id = JSID_VOID;
    GET(GetPropertyNames(cx, wrappedObject(wrapper), 0, &props));
}

bool
Wrapper::fix(JSContext *cx, JSObject *wrapper, Value *vp)
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
Wrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    JSBool found;
    GET(JS_HasPropertyById(cx, wrappedObject(wrapper), id, &found) &&
        Cond(found, bp));
}

bool
Wrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    PropertyDescriptor desc;
    JSObject *wobj = wrappedObject(wrapper);
    GET(JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED, &desc) &&
        Cond(desc.obj == wobj, bp));
}

bool
Wrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    GET(wrappedObject(wrapper)->getGeneric(cx, receiver, id, vp));
}

bool
Wrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, bool strict,
               Value *vp)
{
    // FIXME (bug 596351): Need deal with strict mode.
    SET(wrappedObject(wrapper)->setGeneric(cx, id, vp, false));
}

bool
Wrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    const jsid id = JSID_VOID;
    GET(GetPropertyNames(cx, wrappedObject(wrapper), JSITER_OWNONLY, &props));
}

bool
Wrapper::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(GetIterator(cx, wrappedObject(wrapper), flags, vp));
}

bool
Wrapper::call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    CHECKED(ProxyHandler::call(cx, wrapper, argc, vp), CALL);
}

bool
Wrapper::construct(JSContext *cx, JSObject *wrapper, unsigned argc, Value *argv, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(ProxyHandler::construct(cx, wrapper, argc, argv, vp));
}

bool
Wrapper::nativeCall(JSContext *cx, JSObject *wrapper, Class *clasp, Native native, CallArgs args)
{
    const jsid id = JSID_VOID;
    CHECKED(CallJSNative(cx, native, args), CALL);
}

bool
Wrapper::hasInstance(JSContext *cx, JSObject *wrapper, const Value *vp, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    JSBool b = JS_FALSE;
    GET(JS_HasInstance(cx, wrappedObject(wrapper), *vp, &b) && Cond(b, bp));
}

JSType
Wrapper::typeOf(JSContext *cx, JSObject *wrapper)
{
    return TypeOfValue(cx, ObjectValue(*wrappedObject(wrapper)));
}

bool
Wrapper::objectClassIs(JSObject *wrapper, ESClassValue classValue, JSContext *cx)
{
    return ObjectClassIs(*wrappedObject(wrapper), classValue, cx);
}

JSString *
Wrapper::obj_toString(JSContext *cx, JSObject *wrapper)
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
Wrapper::fun_toString(JSContext *cx, JSObject *wrapper, unsigned indent)
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
    JSString *str = ProxyHandler::fun_toString(cx, wrapper, indent);
    leave(cx, wrapper);
    return str;
}

bool
Wrapper::regexp_toShared(JSContext *cx, JSObject *wrapper, RegExpGuard *g)
{
    return wrappedObject(wrapper)->asRegExp().getShared(cx, g);
}

bool
Wrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp)
{
    *vp = ObjectValue(*wrappedObject(wrapper));
    if (hint == JSTYPE_VOID)
        return ToPrimitive(cx, vp);
    return ToPrimitive(cx, hint, vp);
}

bool
Wrapper::iteratorNext(JSContext *cx, JSObject *wrapper, Value *vp)
{
    if (!js_IteratorMore(cx, wrappedObject(wrapper), vp))
        return false;

    if (vp->toBoolean()) {
        *vp = cx->iterValue;
        cx->iterValue.setUndefined();
    } else {
        vp->setMagic(JS_NO_ITER_VALUE);
    }
    return true;
}

void
Wrapper::trace(JSTracer *trc, JSObject *wrapper)
{
    MarkSlot(trc, &wrapper->getReservedSlotRef(JSSLOT_PROXY_PRIVATE), "wrappedObject");
}

JSObject *
Wrapper::wrappedObject(const JSObject *wrapper)
{
    return GetProxyPrivate(wrapper).toObjectOrNull();
}

Wrapper *
Wrapper::wrapperHandler(const JSObject *wrapper)
{
    return static_cast<Wrapper *>(GetProxyHandler(wrapper));
}

bool
Wrapper::enter(JSContext *cx, JSObject *wrapper, jsid id, Action act, bool *bp)
{
    *bp = true;
    return true;
}

void
Wrapper::leave(JSContext *cx, JSObject *wrapper)
{
}

Wrapper Wrapper::singleton((unsigned)0);

JSObject *
Wrapper::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent, Wrapper *handler)
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
                         unsigned flags)
{
    // Allow wrapping outer window proxies.
    JS_ASSERT(!obj->isWrapper() || obj->getClass()->ext.innerObject);
    return Wrapper::New(cx, obj, wrappedProto, parent, &CrossCompartmentWrapper::singleton);
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

    JS_ASSERT(context->compartment == target->compartment());
    JSCompartment *destination = context->compartment;

    JSObject &scopeChain = target->global();
    JS_ASSERT(scopeChain.isNative());

    return context->stack.pushDummyFrame(context, destination, scopeChain, frame);
}

AutoCompartment::AutoCompartment(JSContext *cx, JSObject *target)
    : context(cx),
      origin(cx->compartment),
      target(target),
      destination(target->compartment()),
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
        JSObject &scopeChain = target->global();
        JS_ASSERT(scopeChain.isNative());

        frame.construct();
        if (!context->stack.pushDummyFrame(context, destination, scopeChain, &frame.ref()))
            return false;

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
        if (exc.isObject() && exc.toObject().isError() && exc.toObject().getPrivate()) {
            cx->clearPendingException();
            ac.leave();
            JSObject *copyobj = js_CopyErrorObject(cx, &exc.toObject(), scope);
            if (copyobj)
                cx->setPendingException(ObjectValue(*copyobj));
        }
    }
}

/* Cross compartment wrappers. */

CrossCompartmentWrapper::CrossCompartmentWrapper(unsigned flags)
  : Wrapper(CROSS_COMPARTMENT | flags)
{
}

CrossCompartmentWrapper::~CrossCompartmentWrapper()
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
CrossCompartmentWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                               bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           call.destination->wrapId(cx, &id),
           Wrapper::getPropertyDescriptor(cx, wrapper, id, set, desc),
           call.origin->wrap(cx, desc));
}

bool
CrossCompartmentWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                  bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           call.destination->wrapId(cx, &id),
           Wrapper::getOwnPropertyDescriptor(cx, wrapper, id, set, desc),
           call.origin->wrap(cx, desc));
}

bool
CrossCompartmentWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc)
{
    AutoPropertyDescriptorRooter desc2(cx, desc);
    PIERCE(cx, wrapper, SET,
           call.destination->wrapId(cx, &id) && call.destination->wrap(cx, &desc2),
           Wrapper::defineProperty(cx, wrapper, id, &desc2),
           NOTHING);
}

bool
CrossCompartmentWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::getOwnPropertyNames(cx, wrapper, props),
           call.origin->wrap(cx, props));
}

bool
CrossCompartmentWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, SET,
           call.destination->wrapId(cx, &id),
           Wrapper::delete_(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::enumerate(cx, wrapper, props),
           call.origin->wrap(cx, props));
}

bool
CrossCompartmentWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           call.destination->wrapId(cx, &id),
           Wrapper::has(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           call.destination->wrapId(cx, &id),
           Wrapper::hasOwn(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           call.destination->wrap(cx, &receiver) && call.destination->wrapId(cx, &id),
           Wrapper::get(cx, wrapper, receiver, id, vp),
           call.origin->wrap(cx, vp));
}

bool
CrossCompartmentWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                             bool strict, Value *vp)
{
    AutoValueRooter tvr(cx, *vp);
    PIERCE(cx, wrapper, SET,
           call.destination->wrap(cx, &receiver) &&
           call.destination->wrapId(cx, &id) &&
           call.destination->wrap(cx, tvr.addr()),
           Wrapper::set(cx, wrapper, receiver, id, strict, tvr.addr()),
           NOTHING);
}

bool
CrossCompartmentWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::keys(cx, wrapper, props),
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
           (obj = &vp->toObject())->getClass() == &IteratorClass &&
           (obj->getNativeIterator()->flags & JSITER_ENUMERATE);
}

struct AutoCloseIterator
{
    AutoCloseIterator(JSContext *cx, JSObject *obj) : cx(cx), obj(obj) {}

    ~AutoCloseIterator() { if (obj) CloseIterator(cx, obj); }

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
            jsid id;
            if (!ValueToId(cx, StringValue(ni->begin()[i]), &id))
                return false;
            id = js_CheckForStringIndex(id);
            keys[i] = id;
            if (!origin->wrapId(cx, &keys[i]))
                return false;
        }
    }

    close.clear();
    if (!CloseIterator(cx, iterObj))
        return false;

    if (isKeyIter)
        return VectorToKeyIterator(cx, obj, ni->flags, keys, vp);
    return VectorToValueIterator(cx, obj, ni->flags, keys, vp); 
}

bool
CrossCompartmentWrapper::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::iterate(cx, wrapper, flags, vp),
           CanReify(vp) ? Reify(cx, call.origin, vp) : call.origin->wrap(cx, vp));
}

bool
CrossCompartmentWrapper::call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp)
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
    if (!Wrapper::call(cx, wrapper, argc, vp))
        return false;

    call.leave();
    return call.origin->wrap(cx, vp);
}

bool
CrossCompartmentWrapper::construct(JSContext *cx, JSObject *wrapper, unsigned argc, Value *argv,
                                   Value *rval)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    for (size_t n = 0; n < argc; ++n) {
        if (!call.destination->wrap(cx, &argv[n]))
            return false;
    }
    if (!Wrapper::construct(cx, wrapper, argc, argv, rval))
        return false;

    call.leave();
    return call.origin->wrap(cx, rval);
}

extern JSBool
js_generic_native_method_dispatcher(JSContext *cx, unsigned argc, Value *vp);

bool
CrossCompartmentWrapper::nativeCall(JSContext *cx, JSObject *wrapper, Class *clasp, Native native, CallArgs srcArgs)
{
    JS_ASSERT_IF(!srcArgs.calleev().isUndefined(),
                 srcArgs.callee().toFunction()->native() == native ||
                 srcArgs.callee().toFunction()->native() == js_generic_native_method_dispatcher);
    JS_ASSERT(&srcArgs.thisv().toObject() == wrapper);
    JS_ASSERT(!UnwrapObject(wrapper)->isCrossCompartmentWrapper());

    JSObject *wrapped = wrappedObject(wrapper);
    AutoCompartment call(cx, wrapped);
    if (!call.enter())
        return false;

    InvokeArgsGuard dstArgs;
    if (!cx->stack.pushInvokeArgs(cx, srcArgs.length(), &dstArgs))
        return false;

    Value *src = srcArgs.base(); 
    Value *srcend = srcArgs.array() + srcArgs.length();
    Value *dst = dstArgs.base();
    for (; src != srcend; ++src, ++dst) {
        *dst = *src;
        if (!call.destination->wrap(cx, dst))
            return false;
    }

    if (!Wrapper::nativeCall(cx, wrapper, clasp, native, dstArgs))
        return false;

    srcArgs.rval() = dstArgs.rval();
    dstArgs.pop();
    call.leave();
    return call.origin->wrap(cx, &srcArgs.rval());
}

bool
CrossCompartmentWrapper::hasInstance(JSContext *cx, JSObject *wrapper, const Value *vp, bool *bp)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    Value v = *vp;
    if (!call.destination->wrap(cx, &v))
        return false;
    return Wrapper::hasInstance(cx, wrapper, &v, bp);
}

JSString *
CrossCompartmentWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return NULL;

    JSString *str = Wrapper::obj_toString(cx, wrapper);
    if (!str)
        return NULL;

    call.leave();
    if (!call.origin->wrap(cx, &str))
        return NULL;
    return str;
}

JSString *
CrossCompartmentWrapper::fun_toString(JSContext *cx, JSObject *wrapper, unsigned indent)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return NULL;

    JSString *str = Wrapper::fun_toString(cx, wrapper, indent);
    if (!str)
        return NULL;

    call.leave();
    if (!call.origin->wrap(cx, &str))
        return NULL;
    return str;
}

bool
CrossCompartmentWrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!call.enter())
        return false;

    if (!Wrapper::defaultValue(cx, wrapper, hint, vp))
        return false;

    call.leave();
    return call.origin->wrap(cx, vp);
}

bool
CrossCompartmentWrapper::iteratorNext(JSContext *cx, JSObject *wrapper, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::iteratorNext(cx, wrapper, vp),
           call.origin->wrap(cx, vp));
}

void
CrossCompartmentWrapper::trace(JSTracer *trc, JSObject *wrapper)
{
    MarkCrossCompartmentSlot(trc, &wrapper->getReservedSlotRef(JSSLOT_PROXY_PRIVATE),
                             "wrappedObject");
}

CrossCompartmentWrapper CrossCompartmentWrapper::singleton(0u);

/* Security wrappers. */

template <class Base>
SecurityWrapper<Base>::SecurityWrapper(unsigned flags)
  : Base(flags)
{}

template <class Base>
bool
SecurityWrapper<Base>::nativeCall(JSContext *cx, JSObject *wrapper, Class *clasp, Native native,
                                  CallArgs args)
{
    /*
     * Let this through until compartment-per-global lets us have stronger
     * invariants wrt document.domain (bug 714547).
     */
    return Base::nativeCall(cx, wrapper, clasp, native, args);
}

template <class Base>
bool
SecurityWrapper<Base>::objectClassIs(JSObject *obj, ESClassValue classValue, JSContext *cx)
{
    /*
     * Let this through until compartment-per-global lets us have stronger
     * invariants wrt document.domain (bug 714547).
     */
    return Base::objectClassIs(obj, classValue, cx);
}

template <class Base>
bool
SecurityWrapper<Base>::regexp_toShared(JSContext *cx, JSObject *obj, RegExpGuard *g)
{
    return Base::regexp_toShared(cx, obj, g);
}


template class js::SecurityWrapper<Wrapper>;
template class js::SecurityWrapper<CrossCompartmentWrapper>;
