/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsexn.h"
#include "jswrapper.h"

#include "js/Proxy.h"
#include "vm/ErrorObject.h"
#include "vm/ProxyObject.h"
#include "vm/WrapperObject.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

bool
Wrapper::finalizeInBackground(Value priv) const
{
    if (!priv.isObject())
        return true;

    /*
     * Make the 'background-finalized-ness' of the wrapper the same as the
     * wrapped object, to allow transplanting between them.
     *
     * If the wrapped object is in the nursery then we know it doesn't have a
     * finalizer, and so background finalization is ok.
     */
    if (IsInsideNursery(&priv.toObject()))
        return true;
    return IsBackgroundFinalized(priv.toObject().asTenured().getAllocKind());
}

bool
Wrapper::getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                  MutableHandle<PropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, proxy, id, GET | SET | GET_PROPERTY_DESCRIPTOR);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetOwnPropertyDescriptor(cx, target, id, desc);
}

bool
Wrapper::defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                        Handle<PropertyDescriptor> desc, ObjectOpResult& result) const
{
    assertEnteredPolicy(cx, proxy, id, SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return DefineProperty(cx, target, id, desc, result);
}

bool
Wrapper::ownPropertyKeys(JSContext* cx, HandleObject proxy, AutoIdVector& props) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &props);
}

bool
Wrapper::delete_(JSContext* cx, HandleObject proxy, HandleId id, ObjectOpResult& result) const
{
    assertEnteredPolicy(cx, proxy, id, SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return DeleteProperty(cx, target, id, result);
}

bool
Wrapper::enumerate(JSContext* cx, HandleObject proxy, MutableHandleObject objp) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetIterator(cx, target, 0, objp);
}

bool
Wrapper::getPrototype(JSContext* cx, HandleObject proxy, MutableHandleObject protop) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPrototype(cx, target, protop);
}

bool
Wrapper::setPrototype(JSContext* cx, HandleObject proxy, HandleObject proto,
                                 ObjectOpResult& result) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return SetPrototype(cx, target, proto, result);
}

bool
Wrapper::getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy,
                                           bool* isOrdinary, MutableHandleObject protop) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPrototypeIfOrdinary(cx, target, isOrdinary, protop);
}

bool
Wrapper::setImmutablePrototype(JSContext* cx, HandleObject proxy, bool* succeeded) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return SetImmutablePrototype(cx, target, succeeded);
}

bool
Wrapper::preventExtensions(JSContext* cx, HandleObject proxy, ObjectOpResult& result) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return PreventExtensions(cx, target, result);
}

bool
Wrapper::isExtensible(JSContext* cx, HandleObject proxy, bool* extensible) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return IsExtensible(cx, target, extensible);
}

bool
Wrapper::has(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return HasProperty(cx, target, id, bp);
}

bool
Wrapper::get(JSContext* cx, HandleObject proxy, HandleValue receiver, HandleId id,
             MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetProperty(cx, target, receiver, id, vp);
}

bool
Wrapper::set(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v, HandleValue receiver,
             ObjectOpResult& result) const
{
    assertEnteredPolicy(cx, proxy, id, SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return SetProperty(cx, target, id, v, receiver, result);
}

bool
Wrapper::call(JSContext* cx, HandleObject proxy, const CallArgs& args) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, CALL);
    RootedValue target(cx, proxy->as<ProxyObject>().private_());

    InvokeArgs iargs(cx);
    if (!FillArgumentsFromArraylike(cx, iargs, args))
        return false;

    return js::Call(cx, target, args.thisv(), iargs, args.rval());
}

bool
Wrapper::construct(JSContext* cx, HandleObject proxy, const CallArgs& args) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, CALL);

    RootedValue target(cx, proxy->as<ProxyObject>().private_());
    if (!IsConstructor(target)) {
        ReportValueError(cx, JSMSG_NOT_CONSTRUCTOR, JSDVG_IGNORE_STACK, target, nullptr);
        return false;
    }

    ConstructArgs cargs(cx);
    if (!FillArgumentsFromArraylike(cx, cargs, args))
        return false;

    RootedObject obj(cx);
    if (!Construct(cx, target, cargs, args.newTarget(), &obj))
        return false;

    args.rval().setObject(*obj);
    return true;
}

bool
Wrapper::getPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                               MutableHandle<PropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, proxy, id, GET | SET | GET_PROPERTY_DESCRIPTOR);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyDescriptor(cx, target, id, desc);
}

bool
Wrapper::hasOwn(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return HasOwnProperty(cx, target, id, bp);
}

bool
Wrapper::getOwnEnumerablePropertyKeys(JSContext* cx, HandleObject proxy,
                                                 AutoIdVector& props) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyKeys(cx, target, JSITER_OWNONLY, &props);
}

bool
Wrapper::nativeCall(JSContext* cx, IsAcceptableThis test, NativeImpl impl,
                               const CallArgs& args) const
{
    args.setThis(ObjectValue(*args.thisv().toObject().as<ProxyObject>().target()));
    if (!test(args.thisv())) {
        ReportIncompatible(cx, args);
        return false;
    }

    return CallNativeImpl(cx, impl, args);
}

bool
Wrapper::hasInstance(JSContext* cx, HandleObject proxy, MutableHandleValue v,
                                bool* bp) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return HasInstance(cx, target, v, bp);
}

bool
Wrapper::getBuiltinClass(JSContext* cx, HandleObject proxy, ESClassValue* classValue) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetBuiltinClass(cx, target, classValue);
}

bool
Wrapper::isArray(JSContext* cx, HandleObject proxy, JS::IsArrayAnswer* answer) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return IsArray(cx, target, answer);
}

const char*
Wrapper::className(JSContext* cx, HandleObject proxy) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetObjectClassName(cx, target);
}

JSString*
Wrapper::fun_toString(JSContext* cx, HandleObject proxy, unsigned indent) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return fun_toStringHelper(cx, target, indent);
}

bool
Wrapper::regexp_toShared(JSContext* cx, HandleObject proxy, RegExpGuard* g) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return RegExpToShared(cx, target, g);
}

bool
Wrapper::boxedValue_unbox(JSContext* cx, HandleObject proxy, MutableHandleValue vp) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return Unbox(cx, target, vp);
}

bool
Wrapper::isCallable(JSObject* obj) const
{
    JSObject * target = obj->as<ProxyObject>().target();
    return target->isCallable();
}

bool
Wrapper::isConstructor(JSObject* obj) const
{
    // For now, all wrappers are constructable if they are callable. We will want to eventually
    // decouple this behavior, but none of the Wrapper infrastructure is currently prepared for
    // that.
    return isCallable(obj);
}

JSObject*
Wrapper::weakmapKeyDelegate(JSObject* proxy) const
{
    return UncheckedUnwrap(proxy);
}

JSObject*
Wrapper::New(JSContext* cx, JSObject* obj, const Wrapper* handler,
             const WrapperOptions& options)
{
    RootedValue priv(cx, ObjectValue(*obj));
    return NewProxyObject(cx, handler, priv, options.proto(), options);
}

JSObject*
Wrapper::Renew(JSContext* cx, JSObject* existing, JSObject* obj, const Wrapper* handler)
{
    existing->as<ProxyObject>().renew(cx, handler, ObjectValue(*obj));
    return existing;
}

const Wrapper*
Wrapper::wrapperHandler(JSObject* wrapper)
{
    MOZ_ASSERT(wrapper->is<WrapperObject>());
    return static_cast<const Wrapper*>(wrapper->as<ProxyObject>().handler());
}

JSObject*
Wrapper::wrappedObject(JSObject* wrapper)
{
    MOZ_ASSERT(wrapper->is<WrapperObject>());
    return wrapper->as<ProxyObject>().target();
}

JS_FRIEND_API(JSObject*)
js::UncheckedUnwrap(JSObject* wrapped, bool stopAtWindowProxy, unsigned* flagsp)
{
    unsigned flags = 0;
    while (true) {
        if (!wrapped->is<WrapperObject>() ||
            MOZ_UNLIKELY(stopAtWindowProxy && IsWindowProxy(wrapped)))
        {
            break;
        }
        flags |= Wrapper::wrapperHandler(wrapped)->flags();
        wrapped = wrapped->as<ProxyObject>().private_().toObjectOrNull();

        // This can be called from Wrapper::weakmapKeyDelegate() on a wrapper
        // whose referent has been moved while it is still unmarked.
        if (wrapped)
            wrapped = MaybeForwarded(wrapped);
    }
    if (flagsp)
        *flagsp = flags;
    return wrapped;
}

JS_FRIEND_API(JSObject*)
js::CheckedUnwrap(JSObject* obj, bool stopAtWindowProxy)
{
    while (true) {
        JSObject* wrapper = obj;
        obj = UnwrapOneChecked(obj, stopAtWindowProxy);
        if (!obj || obj == wrapper)
            return obj;
    }
}

JS_FRIEND_API(JSObject*)
js::UnwrapOneChecked(JSObject* obj, bool stopAtWindowProxy)
{
    if (!obj->is<WrapperObject>() ||
        MOZ_UNLIKELY(IsWindowProxy(obj) && stopAtWindowProxy))
    {
        return obj;
    }

    const Wrapper* handler = Wrapper::wrapperHandler(obj);
    return handler->hasSecurityPolicy() ? nullptr : Wrapper::wrappedObject(obj);
}

const char Wrapper::family = 0;
const Wrapper Wrapper::singleton((unsigned)0);
const Wrapper Wrapper::singletonWithPrototype((unsigned)0, true);
JSObject* Wrapper::defaultProto = TaggedProto::LazyProto;

/* Compartments. */

JSObject*
js::TransparentObjectWrapper(JSContext* cx, HandleObject existing, HandleObject obj)
{
    // Allow wrapping outer window proxies.
    MOZ_ASSERT(!obj->is<WrapperObject>() || IsWindowProxy(obj));
    return Wrapper::New(cx, obj, &CrossCompartmentWrapper::singleton);
}

ErrorCopier::~ErrorCopier()
{
    JSContext* cx = ac->context()->asJSContext();

    // The provenance of Debugger.DebuggeeWouldRun is the topmost locking
    // debugger compartment; it should not be copied around.
    if (ac->origin() != cx->compartment() &&
        cx->isExceptionPending() &&
        !cx->isThrowingDebuggeeWouldRun())
    {
        RootedValue exc(cx);
        if (cx->getPendingException(&exc) && exc.isObject() && exc.toObject().is<ErrorObject>()) {
            cx->clearPendingException();
            ac.reset();
            Rooted<ErrorObject*> errObj(cx, &exc.toObject().as<ErrorObject>());
            JSObject* copyobj = CopyErrorObject(cx, errObj);
            if (copyobj)
                cx->setPendingException(ObjectValue(*copyobj));
        }
    }
}
