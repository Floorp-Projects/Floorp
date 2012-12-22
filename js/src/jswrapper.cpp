/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsexn.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jswrapper.h"

#ifdef JS_METHODJIT
# include "assembler/jit/ExecutableAllocator.h"
#endif
#include "gc/Marking.h"
#include "methodjit/PolyIC.h"
#include "methodjit/MonoIC.h"

#include "jsobjinlines.h"

#include "builtin/Iterator-inl.h"
#include "vm/RegExpObject-inl.h"

using namespace js;
using namespace js::gc;

int js::sWrapperFamily;

void *
Wrapper::getWrapperFamily()
{
    return &sWrapperFamily;
}

JSObject *
Wrapper::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
             Wrapper *handler)
{
    JS_ASSERT(parent);

    AutoMarkInDeadCompartment amd(cx->compartment);

#if JS_HAS_XML_SUPPORT
    if (obj->isXML()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_WRAP_XML_OBJECT);
        return NULL;
    }
#endif
    return NewProxyObject(cx, handler, ObjectValue(*obj), proto, parent,
                          obj->isCallable() ? obj : NULL, NULL);
}

JSObject *
Wrapper::Renew(JSContext *cx, JSObject *existing, JSObject *obj, Wrapper *handler)
{
#if JS_HAS_XML_SUPPORT
    if (obj->isXML()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_WRAP_XML_OBJECT);
        return NULL;
    }
#endif

    JS_ASSERT(!obj->isCallable());
    return RenewProxyObject(cx, existing, handler, ObjectValue(*obj));
}

Wrapper *
Wrapper::wrapperHandler(RawObject wrapper)
{
    JS_ASSERT(wrapper->isWrapper());
    return static_cast<Wrapper*>(GetProxyHandler(wrapper));
}

JSObject *
Wrapper::wrappedObject(RawObject wrapper)
{
    JS_ASSERT(wrapper->isWrapper());
    return GetProxyTargetObject(wrapper);
}

bool
Wrapper::enter(JSContext *cx, JSObject *wrapper, jsid id, Action act, bool *bp)
{
    *bp = true;
    return true;
}

JS_FRIEND_API(JSObject *)
js::UnwrapObject(JSObject *wrapped, bool stopAtOuter, unsigned *flagsp)
{
    unsigned flags = 0;
    while (wrapped->isWrapper() &&
           !JS_UNLIKELY(stopAtOuter && wrapped->getClass()->ext.innerObject)) {
        flags |= Wrapper::wrapperHandler(wrapped)->flags();
        wrapped = GetProxyPrivate(wrapped).toObjectOrNull();
    }
    if (flagsp)
        *flagsp = flags;
    return wrapped;
}

JS_FRIEND_API(JSObject *)
js::UnwrapObjectChecked(RawObject obj)
{
    while (true) {
        JSObject *wrapper = obj;
        obj = UnwrapOneChecked(obj);
        if (!obj || obj == wrapper)
            return obj;
    }
}

JS_FRIEND_API(JSObject *)
js::UnwrapOneChecked(RawObject obj)
{
    // Checked unwraps should never unwrap outer windows.
    if (!obj->isWrapper() ||
        JS_UNLIKELY(!!obj->getClass()->ext.innerObject))
    {
        return obj;
    }

    Wrapper *handler = Wrapper::wrapperHandler(obj);
    return handler->isSafeToUnwrap() ? Wrapper::wrappedObject(obj) : NULL;
}

bool
js::IsCrossCompartmentWrapper(RawObject wrapper)
{
    return wrapper->isWrapper() &&
           !!(Wrapper::wrapperHandler(wrapper)->flags() & Wrapper::CROSS_COMPARTMENT);
}

#define CHECKED(op, act)                                                     \
    JS_BEGIN_MACRO                                                           \
        bool status;                                                         \
        if (!enter(cx, wrapper, id, act, &status))                           \
            return status;                                                   \
        return (op);                                                         \
    JS_END_MACRO

#define SET(action) CHECKED(action, SET)
#define GET(action) CHECKED(action, GET)

Wrapper::Wrapper(unsigned flags, bool hasPrototype) : DirectProxyHandler(&sWrapperFamily)
                                                    , mFlags(flags)
                                                    , mSafeToUnwrap(true)
{
    setHasPrototype(hasPrototype);
}

Wrapper::~Wrapper()
{
}

bool
Wrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                               jsid id, bool set,
                               PropertyDescriptor *desc)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(DirectProxyHandler::getPropertyDescriptor(cx, wrapper, id, set, desc),
            set ? SET : GET);
}

bool
Wrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                  jsid id, bool set,
                                  PropertyDescriptor *desc)
{
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(DirectProxyHandler::getOwnPropertyDescriptor(cx, wrapper, id, set, desc), GET);
}

bool
Wrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                        PropertyDescriptor *desc)
{
    SET(DirectProxyHandler::defineProperty(cx, wrapper, id, desc));
}

bool
Wrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                             AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    jsid id = JSID_VOID;
    GET(DirectProxyHandler::getOwnPropertyNames(cx, wrapper, props));
}

bool
Wrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = true; // default result if we refuse to perform this action
    SET(DirectProxyHandler::delete_(cx, wrapper, id, bp));
}

bool
Wrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    // if we refuse to perform this action, props remains empty
    static jsid id = JSID_VOID;
    GET(DirectProxyHandler::enumerate(cx, wrapper, props));
}

/*
 * Ordinarily, the convert trap would require unwrapping. However, the default
 * implementation of convert, JS_ConvertStub, obtains a default value by calling
 * the toString/valueOf method on the wrapper, if any. Throwing if we can't unwrap
 * in this case would be overly conservative. To make matters worse, XPConnect sometimes
 * installs a custom convert trap that obtains a default value by calling the
 * toString method on the wrapper. Doing a puncture in this case would be overly
 * conservative as well. We deal with these anomalies by falling back to the DefaultValue
 * algorithm whenever unwrapping is forbidden.
 */
bool
Wrapper::defaultValue(JSContext *cx, JSObject *wrapper_, JSType hint, Value *vp)
{
    RootedObject wrapper(cx, wrapper_);

    if (!wrapperHandler(wrapper)->isSafeToUnwrap()) {
        RootedValue v(cx);
        if (!DefaultValue(cx, wrapper, hint, &v))
            return false;
        *vp = v;
        return true;
    }
    /*
     * We enter the compartment of the wrappee here, even if we're not a cross
     * compartment wrapper. Moreover, cross compartment wrappers do not enter
     * the compartment of the wrappee before calling this function. This is
     * necessary because the DefaultValue algorithm above operates on the
     * wrapper, not the wrappee, so we want to delay the decision to switch
     * compartments until this point.
     */
    AutoCompartment call(cx, wrappedObject(wrapper));
    return DirectProxyHandler::defaultValue(cx, wrapper_, hint, vp);
}

bool
Wrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    *bp = false; // default result if we refuse to perform this action
    GET(DirectProxyHandler::has(cx, wrapper, id, bp));
}

bool
Wrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    GET(DirectProxyHandler::hasOwn(cx, wrapper, id, bp));
}

bool
Wrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    GET(DirectProxyHandler::get(cx, wrapper, receiver, id, vp));
}

bool
Wrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, bool strict,
             Value *vp)
{
    SET(DirectProxyHandler::set(cx, wrapper, receiver, id, strict, vp));
}

bool
Wrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    const jsid id = JSID_VOID;
    GET(DirectProxyHandler::keys(cx, wrapper, props));
}

bool
Wrapper::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(DirectProxyHandler::iterate(cx, wrapper, flags, vp));
}

bool
Wrapper::call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    CHECKED(DirectProxyHandler::call(cx, wrapper, argc, vp), CALL);
}

bool
Wrapper::construct(JSContext *cx, JSObject *wrapper, unsigned argc, Value *argv, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    CHECKED(DirectProxyHandler::construct(cx, wrapper, argc, argv, vp), CALL);
}

bool
Wrapper::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args)
{
    const jsid id = JSID_VOID;
    Rooted<JSObject*> wrapper(cx, &args.thisv().toObject());
    CHECKED(DirectProxyHandler::nativeCall(cx, test, impl, args), CALL);
}

bool
Wrapper::hasInstance(JSContext *cx, HandleObject wrapper, MutableHandleValue v, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(DirectProxyHandler::hasInstance(cx, wrapper, v, bp));
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
    JSString *str = DirectProxyHandler::obj_toString(cx, wrapper);
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
            ReportIsNotFunction(cx, ObjectValue(*wrapper));
            return NULL;
        }
        return NULL;
    }
    JSString *str = DirectProxyHandler::fun_toString(cx, wrapper, indent);
    return str;
}

Wrapper Wrapper::singleton((unsigned)0);
Wrapper Wrapper::singletonWithPrototype((unsigned)0, true);

/* Compartments. */

extern JSObject *
js::TransparentObjectWrapper(JSContext *cx, JSObject *existing, JSObject *objArg,
                             JSObject *wrappedProtoArg, JSObject *parentArg,
                             unsigned flags)
{
    RootedObject obj(cx, objArg);
    RootedObject wrappedProto(cx, wrappedProtoArg);
    RootedObject parent(cx, parentArg);

    // Allow wrapping outer window proxies.
    JS_ASSERT(!obj->isWrapper() || obj->getClass()->ext.innerObject);
    return Wrapper::New(cx, obj, wrappedProto, parent, &CrossCompartmentWrapper::singleton);
}

ErrorCopier::~ErrorCopier()
{
    JSContext *cx = ac.ref().context();
    if (ac.ref().origin() != cx->compartment && cx->isExceptionPending()) {
        Value exc = cx->getPendingException();
        if (exc.isObject() && exc.toObject().isError() && exc.toObject().getPrivate()) {
            cx->clearPendingException();
            ac.destroy();
            Rooted<JSObject*> errObj(cx, &exc.toObject());
            JSObject *copyobj = js_CopyErrorObject(cx, errObj, scope);
            if (copyobj)
                cx->setPendingException(ObjectValue(*copyobj));
        }
    }
}

/* Cross compartment wrappers. */

CrossCompartmentWrapper::CrossCompartmentWrapper(unsigned flags, bool hasPrototype)
  : Wrapper(CROSS_COMPARTMENT | flags, hasPrototype)
{
}

CrossCompartmentWrapper::~CrossCompartmentWrapper()
{
}

#define PIERCE(cx, wrapper, mode, pre, op, post)                \
    JS_BEGIN_MACRO                                              \
        bool ok;                                                \
        {                                                       \
            AutoCompartment call(cx, wrappedObject(wrapper));   \
            ok = (pre) && (op);                                 \
        }                                                       \
        return ok && (post);                                    \
    JS_END_MACRO

#define NOTHING (true)

bool
CrossCompartmentWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                               bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           cx->compartment->wrapId(cx, &id),
           Wrapper::getPropertyDescriptor(cx, wrapper, id, set, desc),
           cx->compartment->wrap(cx, desc));
}

bool
CrossCompartmentWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                  bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           cx->compartment->wrapId(cx, &id),
           Wrapper::getOwnPropertyDescriptor(cx, wrapper, id, set, desc),
           cx->compartment->wrap(cx, desc));
}

bool
CrossCompartmentWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc)
{
    AutoPropertyDescriptorRooter desc2(cx, desc);
    PIERCE(cx, wrapper, SET,
           cx->compartment->wrapId(cx, &id) && cx->compartment->wrap(cx, &desc2),
           Wrapper::defineProperty(cx, wrapper, id, &desc2),
           NOTHING);
}

bool
CrossCompartmentWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::getOwnPropertyNames(cx, wrapper, props),
           cx->compartment->wrap(cx, props));
}

bool
CrossCompartmentWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, SET,
           cx->compartment->wrapId(cx, &id),
           Wrapper::delete_(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::enumerate(cx, wrapper, props),
           cx->compartment->wrap(cx, props));
}

bool
CrossCompartmentWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           cx->compartment->wrapId(cx, &id),
           Wrapper::has(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           cx->compartment->wrapId(cx, &id),
           Wrapper::hasOwn(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::get(JSContext *cx, JSObject *wrapperArg, JSObject *receiverArg,
                             jsid idArg, Value *vp)
{
    RootedObject wrapper(cx, wrapperArg);
    RootedObject receiver(cx, receiverArg);
    RootedId id(cx, idArg);
    PIERCE(cx, wrapper, GET,
           cx->compartment->wrap(cx, receiver.address()) && cx->compartment->wrapId(cx, id.address()),
           Wrapper::get(cx, wrapper, receiver, id, vp),
           cx->compartment->wrap(cx, vp));
}

bool
CrossCompartmentWrapper::set(JSContext *cx, JSObject *wrapper_, JSObject *receiver_, jsid id_,
                             bool strict, Value *vp)
{
    RootedObject wrapper(cx, wrapper_), receiver(cx, receiver_);
    RootedId id(cx, id_);
    RootedValue value(cx, *vp);
    PIERCE(cx, wrapper, SET,
           cx->compartment->wrap(cx, receiver.address()) &&
           cx->compartment->wrapId(cx, id.address()) &&
           cx->compartment->wrap(cx, value.address()),
           Wrapper::set(cx, wrapper, receiver, id, strict, value.address()),
           NOTHING);
}

bool
CrossCompartmentWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::keys(cx, wrapper, props),
           cx->compartment->wrap(cx, props));
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
           (obj = &vp->toObject())->isPropertyIterator() &&
           (obj->asPropertyIterator().getNativeIterator()->flags & JSITER_ENUMERATE);
}

struct AutoCloseIterator
{
    AutoCloseIterator(JSContext *cx, JSObject *obj) : cx(cx), obj(cx, obj) {}

    ~AutoCloseIterator() { if (obj) CloseIterator(cx, obj); }

    void clear() { obj = NULL; }

  private:
    JSContext *cx;
    RootedObject obj;
};

static bool
Reify(JSContext *cx, JSCompartment *origin, Value *vp)
{
    Rooted<PropertyIteratorObject*> iterObj(cx, &vp->toObject().asPropertyIterator());
    NativeIterator *ni = iterObj->getNativeIterator();

    AutoCloseIterator close(cx, iterObj);

    /* Wrap the iteratee. */
    RootedObject obj(cx, ni->obj);
    if (!origin->wrap(cx, obj.address()))
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
        if (!keys.reserve(length))
            return false;
        for (size_t i = 0; i < length; ++i) {
            jsid id;
            if (!ValueToId(cx, StringValue(ni->begin()[i]), &id))
                return false;
            keys.infallibleAppend(id);
            if (!origin->wrapId(cx, &keys[i]))
                return false;
        }
    }

    close.clear();
    if (!CloseIterator(cx, iterObj))
        return false;

    RootedValue value(cx, *vp);

    if (isKeyIter) {
        if (!VectorToKeyIterator(cx, obj, ni->flags, keys, &value))
            return false;
    } else {
        if (!VectorToValueIterator(cx, obj, ni->flags, keys, &value))
            return false;
    }

    *vp = value;
    return true;
}

bool
CrossCompartmentWrapper::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           Wrapper::iterate(cx, wrapper, flags, vp),
           CanReify(vp) ? Reify(cx, cx->compartment, vp) : cx->compartment->wrap(cx, vp));
}

bool
CrossCompartmentWrapper::call(JSContext *cx, JSObject *wrapper_, unsigned argc, Value *vp)
{
    RootedObject wrapper(cx, wrapper_);
    JSObject *wrapped = wrappedObject(wrapper);
    {
        AutoCompartment call(cx, wrapped);

        vp[0] = ObjectValue(*wrapped);
        if (!cx->compartment->wrap(cx, &vp[1]))
            return false;
        Value *argv = JS_ARGV(cx, vp);
        for (size_t n = 0; n < argc; ++n) {
            if (!cx->compartment->wrap(cx, &argv[n]))
                return false;
        }
        if (!Wrapper::call(cx, wrapper, argc, vp))
            return false;
    }
    return cx->compartment->wrap(cx, vp);
}

bool
CrossCompartmentWrapper::construct(JSContext *cx, JSObject *wrapper_, unsigned argc, Value *argv,
                                   Value *rval)
{
    RootedObject wrapper(cx, wrapper_);
    JSObject *wrapped = wrappedObject(wrapper);
    {
        AutoCompartment call(cx, wrapped);

        for (size_t n = 0; n < argc; ++n) {
            if (!cx->compartment->wrap(cx, &argv[n]))
                return false;
        }
        if (!Wrapper::construct(cx, wrapper, argc, argv, rval))
            return false;
    }
    return cx->compartment->wrap(cx, rval);
}

bool
CrossCompartmentWrapper::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                    CallArgs srcArgs)
{
    Rooted<JSObject*> wrapper(cx, &srcArgs.thisv().toObject());
    JS_ASSERT(srcArgs.thisv().isMagic(JS_IS_CONSTRUCTING) ||
              !UnwrapObject(wrapper)->isCrossCompartmentWrapper());

    RootedObject wrapped(cx, wrappedObject(wrapper));
    {
        AutoCompartment call(cx, wrapped);
        InvokeArgsGuard dstArgs;
        if (!cx->stack.pushInvokeArgs(cx, srcArgs.length(), &dstArgs))
            return false;

        Value *src = srcArgs.base();
        Value *srcend = srcArgs.array() + srcArgs.length();
        Value *dst = dstArgs.base();
        for (; src < srcend; ++src, ++dst) {
            *dst = *src;
            if (!cx->compartment->wrap(cx, dst))
                return false;
        }

        if (!CallNonGenericMethod(cx, test, impl, dstArgs))
            return false;

        srcArgs.rval().set(dstArgs.rval());
        dstArgs.pop();
    }
    return cx->compartment->wrap(cx, srcArgs.rval().address());
}

bool
CrossCompartmentWrapper::hasInstance(JSContext *cx, HandleObject wrapper, MutableHandleValue v, bool *bp)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    if (!cx->compartment->wrap(cx, v.address()))
        return false;
    return Wrapper::hasInstance(cx, wrapper, v, bp);
}

JSString *
CrossCompartmentWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    JSString *str = NULL;
    {
        AutoCompartment call(cx, wrappedObject(wrapper));
        str = Wrapper::obj_toString(cx, wrapper);
        if (!str)
            return NULL;
    }
    if (!cx->compartment->wrap(cx, &str))
        return NULL;
    return str;
}

JSString *
CrossCompartmentWrapper::fun_toString(JSContext *cx, JSObject *wrapper, unsigned indent)
{
    JSString *str = NULL;
    {
        AutoCompartment call(cx, wrappedObject(wrapper));
        str = Wrapper::fun_toString(cx, wrapper, indent);
        if (!str)
            return NULL;
    }
    if (!cx->compartment->wrap(cx, &str))
        return NULL;
    return str;
}

bool
CrossCompartmentWrapper::regexp_toShared(JSContext *cx, JSObject *wrapper, RegExpGuard *g)
{
    AutoCompartment call(cx, wrappedObject(wrapper));
    return Wrapper::regexp_toShared(cx, wrapper, g);
}

bool
CrossCompartmentWrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp)
{
    if (!Wrapper::defaultValue(cx, wrapper, hint, vp))
        return false;
    return cx->compartment->wrap(cx, vp);
}

bool
CrossCompartmentWrapper::getPrototypeOf(JSContext *cx, JSObject *proxy, JSObject **protop)
{
    assertSameCompartment(cx, proxy);

    if (!proxy->getTaggedProto().isLazy()) {
        *protop = proxy->getTaggedProto().toObjectOrNull();
        return true;
    }

    RootedObject proto(cx);
    {
        RootedObject wrapped(cx, wrappedObject(proxy));
        AutoCompartment call(cx, wrapped);
        if (!JSObject::getProto(cx, wrapped, &proto))
            return false;
        if (proto)
            proto->setDelegate(cx);
    }

    if (!proxy->compartment()->wrap(cx, proto.address()))
        return false;

    *protop = proto;
    return true;
}

CrossCompartmentWrapper CrossCompartmentWrapper::singleton(0u);

/* Security wrappers. */

template <class Base>
SecurityWrapper<Base>::SecurityWrapper(unsigned flags)
  : Base(flags)
{
    Base::setSafeToUnwrap(false);
}

template <class Base>
bool
SecurityWrapper<Base>::enter(JSContext *cx, JSObject *wrapper, jsid id,
                             Wrapper::Action act, bool *bp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNWRAP_DENIED);
    *bp = false;
    return false;
}

 template <class Base>
 bool
SecurityWrapper<Base>::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                  CallArgs args)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNWRAP_DENIED);
    return false;
}

template <class Base>
bool
SecurityWrapper<Base>::objectClassIs(JSObject *obj, ESClassValue classValue, JSContext *cx)
{
    return false;
}

template <class Base>
bool
SecurityWrapper<Base>::regexp_toShared(JSContext *cx, JSObject *obj, RegExpGuard *g)
{
    return Base::regexp_toShared(cx, obj, g);
}


template class js::SecurityWrapper<Wrapper>;
template class js::SecurityWrapper<CrossCompartmentWrapper>;

DeadObjectProxy::DeadObjectProxy()
  : BaseProxyHandler(&sDeadObjectFamily)
{
}

bool
DeadObjectProxy::getPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                       jsid id, bool set,
                                       PropertyDescriptor *desc)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                          jsid id, bool set,
                                          PropertyDescriptor *desc)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                PropertyDescriptor *desc)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                     AutoIdVector &props)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::enumerate(JSContext *cx, JSObject *wrapper,
                           AutoIdVector &props)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::construct(JSContext *cx, JSObject *wrapper, unsigned argc,
                           Value *vp, Value *rval)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v,
                             bool *bp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::objectClassIs(JSObject *obj, ESClassValue classValue, JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

JSString *
DeadObjectProxy::obj_toString(JSContext *cx, JSObject *wrapper)
{
    return JS_NewStringCopyZ(cx, "[object DeadObject]");
}

JSString *
DeadObjectProxy::fun_toString(JSContext *cx, JSObject *proxy, unsigned indent)
{
    return NULL;
}

bool
DeadObjectProxy::regexp_toShared(JSContext *cx, JSObject *proxy, RegExpGuard *g)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::defaultValue(JSContext *cx, JSObject *obj, JSType hint, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::getElementIfPresent(JSContext *cx, JSObject *obj, JSObject *receiver,
                                     uint32_t index, Value *vp, bool *present)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEAD_OBJECT);
    return false;
}

bool
DeadObjectProxy::getPrototypeOf(JSContext *cx, JSObject *proxy, JSObject **protop)
{
    *protop = NULL;
    return true;
}

DeadObjectProxy DeadObjectProxy::singleton;
int DeadObjectProxy::sDeadObjectFamily;

JSObject *
js::NewDeadProxyObject(JSContext *cx, JSObject *parent)
{
    return NewProxyObject(cx, &DeadObjectProxy::singleton, NullValue(),
                          NULL, parent, NULL, NULL);
}

bool
js::IsDeadProxyObject(RawObject obj)
{
    return IsProxy(obj) && GetProxyHandler(obj) == &DeadObjectProxy::singleton;
}

static void
NukeSlot(JSObject *wrapper, uint32_t slot, Value v)
{
    Value old = wrapper->getSlot(slot);
    if (old.isMarkable()) {
        Cell *cell = static_cast<Cell *>(old.toGCThing());
        AutoMarkInDeadCompartment amd(cell->compartment());
        wrapper->setReservedSlot(slot, v);
    } else {
        wrapper->setReservedSlot(slot, v);
    }
}

void
js::NukeCrossCompartmentWrapper(JSContext *cx, JSObject *wrapper)
{
    JS_ASSERT(IsCrossCompartmentWrapper(wrapper));

    NotifyGCNukeWrapper(wrapper);

    NukeSlot(wrapper, JSSLOT_PROXY_PRIVATE, NullValue());
    SetProxyHandler(wrapper, &DeadObjectProxy::singleton);

    if (IsFunctionProxy(wrapper)) {
        NukeSlot(wrapper, JSSLOT_PROXY_CALL, NullValue());
        NukeSlot(wrapper, JSSLOT_PROXY_CONSTRUCT, NullValue());
    }

    NukeSlot(wrapper, JSSLOT_PROXY_EXTRA + 0, NullValue());
    NukeSlot(wrapper, JSSLOT_PROXY_EXTRA + 1, NullValue());

    JS_ASSERT(IsDeadProxyObject(wrapper));
}

/*
 * NukeChromeCrossCompartmentWrappersForGlobal reaches into chrome and cuts
 * all of the cross-compartment wrappers that point to objects parented to
 * obj's global.  The snag here is that we need to avoid cutting wrappers that
 * point to the window object on page navigation (inner window destruction)
 * and only do that on tab close (outer window destruction).  Thus the
 * option of how to handle the global object.
 */
JS_FRIEND_API(JSBool)
js::NukeCrossCompartmentWrappers(JSContext* cx,
                                 const CompartmentFilter& sourceFilter,
                                 const CompartmentFilter& targetFilter,
                                 js::NukeReferencesToWindow nukeReferencesToWindow)
{
    CHECK_REQUEST(cx);
    JSRuntime *rt = cx->runtime;

    // Iterate through scopes looking for system cross compartment wrappers
    // that point to an object that shares a global with obj.

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (!sourceFilter.match(c))
            continue;

        // Iterate the wrappers looking for anything interesting.
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            // Some cross-compartment wrappers are for strings.  We're not
            // interested in those.
            const CrossCompartmentKey &k = e.front().key;
            if (k.kind != CrossCompartmentKey::ObjectWrapper)
                continue;

            AutoWrapperRooter wobj(cx, WrapperValue(e));
            JSObject *wrapped = UnwrapObject(wobj);

            if (nukeReferencesToWindow == DontNukeWindowReferences &&
                wrapped->getClass()->ext.innerObject)
                continue;

            if (targetFilter.match(wrapped->compartment())) {
                // We found a wrapper to nuke.
                e.removeFront();
                NukeCrossCompartmentWrapper(cx, wobj);
            }
        }
    }

    return JS_TRUE;
}

// Given a cross-compartment wrapper |wobj|, update it to point to
// |newTarget|. This recomputes the wrapper with JS_WrapValue, and thus can be
// useful even if wrapper already points to newTarget.
bool
js::RemapWrapper(JSContext *cx, JSObject *wobj, JSObject *newTarget)
{
    JS_ASSERT(IsCrossCompartmentWrapper(wobj));
    JS_ASSERT(!IsCrossCompartmentWrapper(newTarget));
    JSObject *origTarget = Wrapper::wrappedObject(wobj);
    JS_ASSERT(origTarget);
    Value origv = ObjectValue(*origTarget);
    JSCompartment *wcompartment = wobj->compartment();

    // If we're mapping to a different target (as opposed to just recomputing
    // for the same target), we must not have an existing wrapper for the new
    // target, otherwise this will break.
    JS_ASSERT_IF(origTarget != newTarget,
                 !wcompartment->lookupWrapper(ObjectValue(*newTarget)));

    // The old value should still be in the cross-compartment wrapper map, and
    // the lookup should return wobj.
    WrapperMap::Ptr p = wcompartment->lookupWrapper(origv);
    JS_ASSERT(&p->value.unsafeGet()->toObject() == wobj);
    wcompartment->removeWrapper(p);

    // When we remove origv from the wrapper map, its wrapper, wobj, must
    // immediately cease to be a cross-compartment wrapper. Neuter it.
    NukeCrossCompartmentWrapper(cx, wobj);

    // First, we wrap it in the new compartment. We try to use the existing
    // wrapper, |wobj|, since it's been nuked anyway. The wrap() function has
    // the choice to reuse |wobj| or not.
    JSObject *tobj = newTarget;
    AutoCompartment ac(cx, wobj);
    if (!wcompartment->wrap(cx, &tobj, wobj))
        MOZ_CRASH();

    // If wrap() reused |wobj|, it will have overwritten it and returned with
    // |tobj == wobj|. Otherwise, |tobj| will point to a new wrapper and |wobj|
    // will still be nuked. In the latter case, we replace |wobj| with the
    // contents of the new wrapper in |tobj|.
    if (tobj != wobj) {
        // Now, because we need to maintain object identity, we do a brain
        // transplant on the old object so that it contains the contents of the
        // new one.
        if (!wobj->swap(cx, tobj))
            MOZ_CRASH();
    }

    // Before swapping, this wrapper came out of wrap(), which enforces the
    // invariant that the wrapper in the map points directly to the key.
    JS_ASSERT(Wrapper::wrappedObject(wobj) == newTarget);

    // Update the entry in the compartment's wrapper map to point to the old
    // wrapper, which has now been updated (via reuse or swap).
    wcompartment->putWrapper(ObjectValue(*newTarget), ObjectValue(*wobj));
    return true;
}

// Remap all cross-compartment wrappers pointing to |oldTarget| to point to
// |newTarget|. All wrappers are recomputed.
JS_FRIEND_API(bool)
js::RemapAllWrappersForObject(JSContext *cx, JSObject *oldTarget,
                              JSObject *newTarget)
{
    Value origv = ObjectValue(*oldTarget);

    AutoWrapperVector toTransplant(cx);
    if (!toTransplant.reserve(cx->runtime->compartments.length()))
        return false;

    for (CompartmentsIter c(cx->runtime); !c.done(); c.next()) {
        if (WrapperMap::Ptr wp = c->lookupWrapper(origv)) {
            // We found a wrapper. Remember and root it.
            toTransplant.infallibleAppend(WrapperValue(wp));
        }
    }

    for (WrapperValue *begin = toTransplant.begin(), *end = toTransplant.end();
         begin != end; ++begin)
    {
        if (!RemapWrapper(cx, &begin->toObject(), newTarget))
            MOZ_CRASH();
    }

    return true;
}

JS_FRIEND_API(bool)
js::RecomputeWrappers(JSContext *cx, const CompartmentFilter &sourceFilter,
                      const CompartmentFilter &targetFilter)
{
    AutoMaybeTouchDeadCompartments agc(cx);

    AutoWrapperVector toRecompute(cx);

    for (CompartmentsIter c(cx->runtime); !c.done(); c.next()) {
        // Filter by source compartment.
        if (!sourceFilter.match(c))
            continue;

        // Iterate over the wrappers, filtering appropriately.
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            // Filter out non-objects.
            const CrossCompartmentKey &k = e.front().key;
            if (k.kind != CrossCompartmentKey::ObjectWrapper)
                continue;

            // Filter by target compartment.
            if (!targetFilter.match(k.wrapped->compartment()))
                continue;

            // Add it to the list.
            if (!toRecompute.append(WrapperValue(e)))
                return false;
        }
    }

    // Recompute all the wrappers in the list.
    for (WrapperValue *begin = toRecompute.begin(), *end = toRecompute.end(); begin != end; ++begin)
    {
        JSObject *wrapper = &begin->toObject();
        JSObject *wrapped = Wrapper::wrappedObject(wrapper);
        if (!RemapWrapper(cx, wrapper, wrapped))
            MOZ_CRASH();
    }

    return true;
}
