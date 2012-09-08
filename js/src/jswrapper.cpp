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

namespace js {
int sWrapperFamily;
}

void *
DirectWrapper::getWrapperFamily()
{
    return &sWrapperFamily;
}

JSObject *
Wrapper::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
             Wrapper *handler)
{
    JS_ASSERT(parent);

#if JS_HAS_XML_SUPPORT
    if (obj->isXML()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_WRAP_XML_OBJECT);
        return NULL;
    }
#endif
    return NewProxyObject(cx, handler->toBaseProxyHandler(), ObjectValue(*obj),
                          proto, parent, obj->isCallable() ? obj : NULL, NULL);
}

Wrapper *
Wrapper::wrapperHandler(RawObject wrapper)
{
    JS_ASSERT(wrapper->isWrapper());
    return GetProxyHandler(wrapper)->toWrapper();
}

JSObject *
Wrapper::wrappedObject(RawObject wrapper)
{
    JS_ASSERT(wrapper->isWrapper());
    return GetProxyTargetObject(wrapper);
}

Wrapper::Wrapper(unsigned flags) : mFlags(flags)
{
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
js::UnwrapObjectChecked(JSContext *cx, RawObject objArg)
{
    RootedObject obj(cx, objArg);
    while (true) {
        JSObject *wrapper = obj;
        obj = UnwrapOneChecked(cx, obj);
        if (!obj || obj == wrapper)
            return obj;
    }
}

JS_FRIEND_API(JSObject *)
js::UnwrapOneChecked(JSContext *cx, HandleObject obj)
{
    // Checked unwraps should never unwrap outer windows.
    if (!obj->isWrapper() ||
        JS_UNLIKELY(!!obj->getClass()->ext.innerObject))
    {
        return obj;
    }

    Wrapper *handler = Wrapper::wrapperHandler(obj);
    bool rvOnFailure;
    if (!handler->enter(cx, obj, JSID_VOID, Wrapper::PUNCTURE, &rvOnFailure))
        return rvOnFailure ? (JSObject*) obj : NULL;
    JSObject *ret = Wrapper::wrappedObject(obj);
    JS_ASSERT(ret);

    return ret;
}

bool
js::IsCrossCompartmentWrapper(RawObject wrapper)
{
    return wrapper->isWrapper() &&
           !!(Wrapper::wrapperHandler(wrapper)->flags() & Wrapper::CROSS_COMPARTMENT);
}

IndirectWrapper::IndirectWrapper(unsigned flags) : Wrapper(flags),
    IndirectProxyHandler(&sWrapperFamily)
{
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

bool
IndirectWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                       jsid id, bool set,
                                       PropertyDescriptor *desc)
{
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(IndirectProxyHandler::getPropertyDescriptor(cx, wrapper, id, set, desc),
            set ? SET : GET);
}

bool
IndirectWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                          jsid id, bool set,
                                          PropertyDescriptor *desc)
{
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(IndirectProxyHandler::getOwnPropertyDescriptor(cx, wrapper, id, set, desc), GET);
}

bool
IndirectWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                PropertyDescriptor *desc)
{
    SET(IndirectProxyHandler::defineProperty(cx, wrapper, id, desc));
}

bool
IndirectWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                     AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    jsid id = JSID_VOID;
    GET(IndirectProxyHandler::getOwnPropertyNames(cx, wrapper, props));
}

bool
IndirectWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = true; // default result if we refuse to perform this action
    SET(IndirectProxyHandler::delete_(cx, wrapper, id, bp));
}

bool
IndirectWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    static jsid id = JSID_VOID;
    GET(IndirectProxyHandler::enumerate(cx, wrapper, props));
}

DirectWrapper::DirectWrapper(unsigned flags, bool hasPrototype) : Wrapper(flags),
        DirectProxyHandler(&sWrapperFamily)
{
    setHasPrototype(hasPrototype);
}

DirectWrapper::~DirectWrapper()
{
}

bool
DirectWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                     jsid id, bool set,
                                     PropertyDescriptor *desc)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(DirectProxyHandler::getPropertyDescriptor(cx, wrapper, id, set, desc),
            set ? SET : GET);
}

bool
DirectWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                        jsid id, bool set,
                                        PropertyDescriptor *desc)
{
    desc->obj = NULL; // default result if we refuse to perform this action
    CHECKED(DirectProxyHandler::getOwnPropertyDescriptor(cx, wrapper, id, set, desc), GET);
}

bool
DirectWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                              PropertyDescriptor *desc)
{
    SET(DirectProxyHandler::defineProperty(cx, wrapper, id, desc));
}

bool
DirectWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                   AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    jsid id = JSID_VOID;
    GET(DirectProxyHandler::getOwnPropertyNames(cx, wrapper, props));
}

bool
DirectWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = true; // default result if we refuse to perform this action
    SET(DirectProxyHandler::delete_(cx, wrapper, id, bp));
}

bool
DirectWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    // if we refuse to perform this action, props remains empty
    static jsid id = JSID_VOID;
    GET(DirectProxyHandler::enumerate(cx, wrapper, props));
}

bool
DirectWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    *bp = false; // default result if we refuse to perform this action
    GET(DirectProxyHandler::has(cx, wrapper, id, bp));
}

bool
DirectWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    GET(DirectProxyHandler::hasOwn(cx, wrapper, id, bp));
}

bool
DirectWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    GET(DirectProxyHandler::get(cx, wrapper, receiver, id, vp));
}

bool
DirectWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, bool strict,
                   Value *vp)
{
    SET(DirectProxyHandler::set(cx, wrapper, receiver, id, strict, vp));
}

bool
DirectWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    // if we refuse to perform this action, props remains empty
    const jsid id = JSID_VOID;
    GET(DirectProxyHandler::keys(cx, wrapper, props));
}

bool
DirectWrapper::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    JS_ASSERT(!hasPrototype()); // Should never be called when there's a prototype.
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(DirectProxyHandler::iterate(cx, wrapper, flags, vp));
}

bool
DirectWrapper::call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    CHECKED(IndirectProxyHandler::call(cx, wrapper, argc, vp), CALL);
}

bool
DirectWrapper::construct(JSContext *cx, JSObject *wrapper, unsigned argc, Value *argv, Value *vp)
{
    vp->setUndefined(); // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    CHECKED(IndirectProxyHandler::construct(cx, wrapper, argc, argv, vp), CALL);
}

bool
DirectWrapper::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args)
{
    const jsid id = JSID_VOID;
    Rooted<JSObject*> wrapper(cx, &args.thisv().toObject());
    CHECKED(IndirectProxyHandler::nativeCall(cx, test, impl, args), CALL);
}

bool
DirectWrapper::hasInstance(JSContext *cx, HandleObject wrapper, MutableHandleValue v, bool *bp)
{
    *bp = false; // default result if we refuse to perform this action
    const jsid id = JSID_VOID;
    GET(IndirectProxyHandler::hasInstance(cx, wrapper, v, bp));
}

JSString *
DirectWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    bool status;
    if (!enter(cx, wrapper, JSID_VOID, GET, &status)) {
        if (status) {
            // Perform some default behavior that doesn't leak any information.
            return JS_NewStringCopyZ(cx, "[object Object]");
        }
        return NULL;
    }
    JSString *str = IndirectProxyHandler::obj_toString(cx, wrapper);
    return str;
}

JSString *
DirectWrapper::fun_toString(JSContext *cx, JSObject *wrapper, unsigned indent)
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
    JSString *str = IndirectProxyHandler::fun_toString(cx, wrapper, indent);
    return str;
}

DirectWrapper DirectWrapper::singleton((unsigned)0);
DirectWrapper DirectWrapper::singletonWithPrototype((unsigned)0, true);

/* Compartments. */

extern JSObject *
js::TransparentObjectWrapper(JSContext *cx, JSObject *objArg, JSObject *wrappedProtoArg, JSObject *parentArg,
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
  : DirectWrapper(CROSS_COMPARTMENT | flags, hasPrototype)
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
           DirectWrapper::getPropertyDescriptor(cx, wrapper, id, set, desc),
           cx->compartment->wrap(cx, desc));
}

bool
CrossCompartmentWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                  bool set, PropertyDescriptor *desc)
{
    PIERCE(cx, wrapper, set ? SET : GET,
           cx->compartment->wrapId(cx, &id),
           DirectWrapper::getOwnPropertyDescriptor(cx, wrapper, id, set, desc),
           cx->compartment->wrap(cx, desc));
}

bool
CrossCompartmentWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc)
{
    AutoPropertyDescriptorRooter desc2(cx, desc);
    PIERCE(cx, wrapper, SET,
           cx->compartment->wrapId(cx, &id) && cx->compartment->wrap(cx, &desc2),
           DirectWrapper::defineProperty(cx, wrapper, id, &desc2),
           NOTHING);
}

bool
CrossCompartmentWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           DirectWrapper::getOwnPropertyNames(cx, wrapper, props),
           cx->compartment->wrap(cx, props));
}

bool
CrossCompartmentWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, SET,
           cx->compartment->wrapId(cx, &id),
           DirectWrapper::delete_(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           DirectWrapper::enumerate(cx, wrapper, props),
           cx->compartment->wrap(cx, props));
}

bool
CrossCompartmentWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           cx->compartment->wrapId(cx, &id),
           DirectWrapper::has(cx, wrapper, id, bp),
           NOTHING);
}

bool
CrossCompartmentWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    PIERCE(cx, wrapper, GET,
           cx->compartment->wrapId(cx, &id),
           DirectWrapper::hasOwn(cx, wrapper, id, bp),
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
           DirectWrapper::get(cx, wrapper, receiver, id, vp),
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
           DirectWrapper::set(cx, wrapper, receiver, id, strict, value.address()),
           NOTHING);
}

bool
CrossCompartmentWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           DirectWrapper::keys(cx, wrapper, props),
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
    PropertyIteratorObject *iterObj = &vp->toObject().asPropertyIterator();
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
           DirectWrapper::iterate(cx, wrapper, flags, vp),
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
        if (!DirectWrapper::call(cx, wrapper, argc, vp))
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
        if (!DirectWrapper::construct(cx, wrapper, argc, argv, rval))
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
    return DirectWrapper::hasInstance(cx, wrapper, v, bp);
}

JSString *
CrossCompartmentWrapper::obj_toString(JSContext *cx, JSObject *wrapper)
{
    JSString *str = NULL;
    {
        AutoCompartment call(cx, wrappedObject(wrapper));
        str = DirectWrapper::obj_toString(cx, wrapper);
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
        str = DirectWrapper::fun_toString(cx, wrapper, indent);
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
    return DirectWrapper::regexp_toShared(cx, wrapper, g);
}

bool
CrossCompartmentWrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp)
{
    {
        AutoCompartment call(cx, wrappedObject(wrapper));
        if (!IndirectProxyHandler::defaultValue(cx, wrapper, hint, vp))
            return false;
    }
    return cx->compartment->wrap(cx, vp);
}

bool
CrossCompartmentWrapper::iteratorNext(JSContext *cx, JSObject *wrapper, Value *vp)
{
    PIERCE(cx, wrapper, GET,
           NOTHING,
           IndirectProxyHandler::iteratorNext(cx, wrapper, vp),
           cx->compartment->wrap(cx, vp));
}

CrossCompartmentWrapper CrossCompartmentWrapper::singleton(0u);

/* Security wrappers. */

template <class Base>
SecurityWrapper<Base>::SecurityWrapper(unsigned flags)
  : Base(flags)
{}

template <class Base>
bool
SecurityWrapper<Base>::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                  CallArgs args)
{
    /*
     * Let this through until compartment-per-global lets us have stronger
     * invariants wrt document.domain (bug 714547).
     */
    return Base::nativeCall(cx, test, impl, args);
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


template class js::SecurityWrapper<DirectWrapper>;
template class js::SecurityWrapper<CrossCompartmentWrapper>;

namespace js {

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
DeadObjectProxy::iteratorNext(JSContext *cx, JSObject *proxy, Value *vp)
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

DeadObjectProxy DeadObjectProxy::singleton;
int DeadObjectProxy::sDeadObjectFamily;

} // namespace js

JSObject *
js::NewDeadProxyObject(JSContext *cx, JSObject *parent)
{
    return NewProxyObject(cx, &DeadObjectProxy::singleton, NullValue(),
                          NULL, parent, NULL, NULL);
}

void
js::NukeCrossCompartmentWrapper(JSObject *wrapper)
{
    JS_ASSERT(IsCrossCompartmentWrapper(wrapper));

    SetProxyPrivate(wrapper, NullValue());
    SetProxyHandler(wrapper, &DeadObjectProxy::singleton);

    if (IsFunctionProxy(wrapper)) {
        wrapper->setReservedSlot(JSSLOT_PROXY_CALL, NullValue());
        wrapper->setReservedSlot(JSSLOT_PROXY_CONSTRUCT, NullValue());
    }

    wrapper->setReservedSlot(JSSLOT_PROXY_EXTRA + 0, NullValue());
    wrapper->setReservedSlot(JSSLOT_PROXY_EXTRA + 1, NullValue());
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
        WrapperMap &pmap = c->crossCompartmentWrappers;
        for (WrapperMap::Enum e(pmap); !e.empty(); e.popFront()) {
            // Some cross-compartment wrappers are for strings.  We're not
            // interested in those.
            const CrossCompartmentKey &k = e.front().key;
            if (k.kind != CrossCompartmentKey::ObjectWrapper)
                continue;

            JSObject *wobj = &e.front().value.get().toObject();
            JSObject *wrapped = UnwrapObject(wobj);

            if (nukeReferencesToWindow == DontNukeWindowReferences &&
                wrapped->getClass()->ext.innerObject)
                continue;

            if (targetFilter.match(wrapped->compartment())) {
                // We found a wrapper to nuke.
                e.removeFront();
                NukeCrossCompartmentWrapper(wobj);
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
    WrapperMap &pmap = wcompartment->crossCompartmentWrappers;

    // If we're mapping to a different target (as opposed to just recomputing
    // for the same target), we must not have an existing wrapper for the new
    // target, otherwise this will break.
    JS_ASSERT_IF(origTarget != newTarget, !pmap.has(ObjectValue(*newTarget)));

    // The old value should still be in the cross-compartment wrapper map, and
    // the lookup should return wobj.
    JS_ASSERT(&pmap.lookup(origv)->value.toObject() == wobj);
    pmap.remove(origv);

    // When we remove origv from the wrapper map, its wrapper, wobj, must
    // immediately cease to be a cross-compartment wrapper. Neuter it.
    NukeCrossCompartmentWrapper(wobj);

    // First, we wrap it in the new compartment. This will return
    // a new wrapper.
    JSObject *tobj = newTarget;
    AutoCompartment ac(cx, wobj);
    if (!wcompartment->wrap(cx, &tobj))
        return false;

    // Now, because we need to maintain object identity, we do a
    // brain transplant on the old object. At the same time, we
    // update the entry in the compartment's wrapper map to point
    // to the old wrapper.
    JS_ASSERT(tobj != wobj);
    if (!wobj->swap(cx, tobj))
        return false;

    // Before swapping, this wrapper came out of wrap(), which enforces the
    // invariant that the wrapper in the map points directly to the key.
    JS_ASSERT(Wrapper::wrappedObject(wobj) == newTarget);

    pmap.put(ObjectValue(*newTarget), ObjectValue(*wobj));
    return true;
}

// Remap all cross-compartment wrappers pointing to |oldTarget| to point to
// |newTarget|. All wrappers are recomputed.
JS_FRIEND_API(bool)
js::RemapAllWrappersForObject(JSContext *cx, JSObject *oldTarget,
                              JSObject *newTarget)
{
    Value origv = ObjectValue(*oldTarget);

    AutoValueVector toTransplant(cx);
    if (!toTransplant.reserve(cx->runtime->compartments.length()))
        return false;

    for (CompartmentsIter c(cx->runtime); !c.done(); c.next()) {
        WrapperMap &pmap = c->crossCompartmentWrappers;
        if (WrapperMap::Ptr wp = pmap.lookup(origv)) {
            // We found a wrapper. Remember and root it.
            toTransplant.infallibleAppend(wp->value);
        }
    }

    for (Value *begin = toTransplant.begin(), *end = toTransplant.end();
         begin != end; ++begin)
    {
        if (!RemapWrapper(cx, &begin->toObject(), newTarget))
            return false;
    }

    return true;
}

JS_FRIEND_API(bool)
js::RecomputeWrappers(JSContext *cx, const CompartmentFilter &sourceFilter,
                      const CompartmentFilter &targetFilter)
{
    AutoValueVector toRecompute(cx);

    for (CompartmentsIter c(cx->runtime); !c.done(); c.next()) {
        // Filter by source compartment.
        if (!sourceFilter.match(c))
            continue;

        // Iterate over the wrappers, filtering appropriately.
        WrapperMap &pmap = c->crossCompartmentWrappers;
        for (WrapperMap::Enum e(pmap); !e.empty(); e.popFront()) {
            // Filter out non-objects.
            const CrossCompartmentKey &k = e.front().key;
            if (k.kind != CrossCompartmentKey::ObjectWrapper)
                continue;

            // Filter by target compartment.
            Value wrapper = e.front().value.get();
            if (!targetFilter.match(k.wrapped->compartment()))
                continue;

            // Add it to the list.
            if (!toRecompute.append(wrapper))
                return false;
        }
    }

    // Recompute all the wrappers in the list.
    for (Value *begin = toRecompute.begin(), *end = toRecompute.end(); begin != end; ++begin)
    {
        JSObject *wrapper = &begin->toObject();
        JSObject *wrapped = Wrapper::wrappedObject(wrapper);
        if (!RemapWrapper(cx, wrapper, wrapped))
            return false;
    }

    return true;
}
