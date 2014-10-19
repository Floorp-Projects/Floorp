/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsproxy.h"
#include "jswrapper.h"

#include "gc/Marking.h"
#include "proxy/DeadObjectProxy.h"
#include "proxy/ScriptedDirectProxyHandler.h"
#include "proxy/ScriptedIndirectProxyHandler.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;

void
js::AutoEnterPolicy::reportErrorIfExceptionIsNotPending(JSContext *cx, jsid id)
{
    if (JS_IsExceptionPending(cx))
        return;

    if (JSID_IS_VOID(id)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_OBJECT_ACCESS_DENIED);
    } else {
        JSString *str = IdToString(cx, id);
        AutoStableStringChars chars(cx);
        const char16_t *prop = nullptr;
        if (str->ensureFlat(cx) && chars.initTwoByte(cx, str))
            prop = chars.twoByteChars();

        JS_ReportErrorNumberUC(cx, js_GetErrorMessage, nullptr, JSMSG_PROPERTY_ACCESS_DENIED,
                               prop);
    }
}

#ifdef DEBUG
void
js::AutoEnterPolicy::recordEnter(JSContext *cx, HandleObject proxy, HandleId id, Action act)
{
    if (allowed()) {
        context = cx;
        enteredProxy.emplace(proxy);
        enteredId.emplace(id);
        enteredAction = act;
        prev = cx->runtime()->enteredPolicy;
        cx->runtime()->enteredPolicy = this;
    }
}

void
js::AutoEnterPolicy::recordLeave()
{
    if (enteredProxy) {
        MOZ_ASSERT(context->runtime()->enteredPolicy == this);
        context->runtime()->enteredPolicy = prev;
    }
}

JS_FRIEND_API(void)
js::assertEnteredPolicy(JSContext *cx, JSObject *proxy, jsid id,
                        BaseProxyHandler::Action act)
{
    MOZ_ASSERT(proxy->is<ProxyObject>());
    MOZ_ASSERT(cx->runtime()->enteredPolicy);
    MOZ_ASSERT(cx->runtime()->enteredPolicy->enteredProxy->get() == proxy);
    MOZ_ASSERT(cx->runtime()->enteredPolicy->enteredId->get() == id);
    MOZ_ASSERT(cx->runtime()->enteredPolicy->enteredAction & act);
}
#endif

#define INVOKE_ON_PROTOTYPE(cx, handler, proxy, protoCall)                   \
    JS_BEGIN_MACRO                                                           \
        RootedObject proto(cx);                                              \
        if (!JSObject::getProto(cx, proxy, &proto))                          \
            return false;                                                    \
        if (!proto)                                                          \
            return true;                                                     \
        assertSameCompartment(cx, proxy, proto);                             \
        return protoCall;                                                    \
    JS_END_MACRO                                                             \

bool
Proxy::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                             MutableHandle<PropertyDescriptor> desc)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    desc.object().set(nullptr); // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET_PROPERTY_DESCRIPTOR, true);
    if (!policy.allowed())
        return policy.returnValue();
    if (!handler->hasPrototype())
        return handler->getPropertyDescriptor(cx, proxy, id, desc);
    if (!handler->getOwnPropertyDescriptor(cx, proxy, id, desc))
        return false;
    if (desc.object())
        return true;
    INVOKE_ON_PROTOTYPE(cx, handler, proxy, JS_GetPropertyDescriptorById(cx, proto, id, desc));
}

bool
Proxy::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    return NewPropertyDescriptorObject(cx, desc, vp);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<PropertyDescriptor> desc)
{
    JS_CHECK_RECURSION(cx, return false);

    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    desc.object().set(nullptr); // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET_PROPERTY_DESCRIPTOR, true);
    if (!policy.allowed())
        return policy.returnValue();
    return handler->getOwnPropertyDescriptor(cx, proxy, id, desc);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    return NewPropertyDescriptorObject(cx, desc, vp);
}

bool
Proxy::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                      MutableHandle<PropertyDescriptor> desc)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::SET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->defineProperty(cx, proxy, id, desc);
}

bool
Proxy::ownPropertyKeys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::ENUMERATE, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->ownPropertyKeys(cx, proxy, props);
}

bool
Proxy::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    *bp = true; // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::SET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->delete_(cx, proxy, id, bp);
}

JS_FRIEND_API(bool)
js::AppendUnique(JSContext *cx, AutoIdVector &base, AutoIdVector &others)
{
    AutoIdVector uniqueOthers(cx);
    if (!uniqueOthers.reserve(others.length()))
        return false;
    for (size_t i = 0; i < others.length(); ++i) {
        bool unique = true;
        for (size_t j = 0; j < base.length(); ++j) {
            if (others[i].get() == base[j]) {
                unique = false;
                break;
            }
        }
        if (unique)
            uniqueOthers.append(others[i]);
    }
    return base.appendAll(uniqueOthers);
}

bool
Proxy::enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::ENUMERATE, true);
    if (!policy.allowed())
        return policy.returnValue();
    if (!handler->hasPrototype())
        return proxy->as<ProxyObject>().handler()->enumerate(cx, proxy, props);
    if (!handler->getOwnEnumerablePropertyKeys(cx, proxy, props))
        return false;
    AutoIdVector protoProps(cx);
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        GetPropertyKeys(cx, proto, 0, &protoProps) &&
                        AppendUnique(cx, props, protoProps));
}

bool
Proxy::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    *bp = false; // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    if (!handler->hasPrototype())
        return handler->has(cx, proxy, id, bp);
    if (!handler->hasOwn(cx, proxy, id, bp))
        return false;
    if (*bp)
        return true;
    bool Bp;
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        JS_HasPropertyById(cx, proto, id, &Bp) &&
                        ((*bp = Bp) || true));
}

bool
Proxy::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    *bp = false; // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return handler->hasOwn(cx, proxy, id, bp);
}

bool
Proxy::get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
           MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    vp.setUndefined(); // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    bool own;
    if (!handler->hasPrototype()) {
        own = true;
    } else {
        if (!handler->hasOwn(cx, proxy, id, &own))
            return false;
    }
    if (own)
        return handler->get(cx, proxy, receiver, id, vp);
    INVOKE_ON_PROTOTYPE(cx, handler, proxy, JSObject::getGeneric(cx, proto, receiver, id, vp));
}

bool
Proxy::callProp(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                MutableHandleValue vp)
{
    // The inline caches need an access point for JSOP_CALLPROP sites that accounts
    // for the possibility of __noSuchMethod__
    if (!Proxy::get(cx, proxy, receiver, id, vp))
        return false;

#if JS_HAS_NO_SUCH_METHOD
    if (MOZ_UNLIKELY(vp.isPrimitive())) {
        if (!OnUnknownMethod(cx, proxy, IdToValue(id), vp))
            return false;
    }
#endif

    return true;
}

bool
Proxy::set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id, bool strict,
           MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::SET, true);
    if (!policy.allowed())
        return policy.returnValue();

    // If the proxy doesn't require that we consult its prototype for the
    // non-own cases, we can sink to the |set| trap.
    if (!handler->hasPrototype())
        return handler->set(cx, proxy, receiver, id, strict, vp);

    // If we have an existing (own or non-own) property with a setter, we want
    // to invoke that.
    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    if (desc.object() && desc.setter() && desc.setter() != JS_StrictPropertyStub)
        return CallSetter(cx, receiver, id, desc.setter(), desc.attributes(), strict, vp);

    if (desc.isReadonly()) {
        return strict ? Throw(cx, id, JSMSG_READ_ONLY) : true;
    }

    // Ok. Either there was no pre-existing property, or it was a value prop
    // that we're going to shadow. Make a property descriptor and define it.
    //
    // Note that for pre-existing own value properties, we inherit the existing
    // attributes, since we're really just changing the value and not trying to
    // reconfigure the property.
    Rooted<PropertyDescriptor> newDesc(cx);
    if (desc.object() == proxy)
        newDesc.setAttributes(desc.attributes());
    else
        newDesc.setAttributes(JSPROP_ENUMERATE);
    newDesc.value().set(vp);
    return handler->defineProperty(cx, receiver, id, &newDesc);
}

bool
Proxy::getOwnEnumerablePropertyKeys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::ENUMERATE, true);
    if (!policy.allowed())
        return policy.returnValue();
    return handler->getOwnEnumerablePropertyKeys(cx, proxy, props);
}

bool
Proxy::iterate(JSContext *cx, HandleObject proxy, unsigned flags, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    vp.setUndefined(); // default result if we refuse to perform this action
    if (!handler->hasPrototype()) {
        AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE,
                               BaseProxyHandler::ENUMERATE, true);
        // If the policy denies access but wants us to return true, we need
        // to hand a valid (empty) iterator object to the caller.
        if (!policy.allowed()) {
            AutoIdVector props(cx);
            return policy.returnValue() &&
                   EnumeratedIdVectorToIterator(cx, proxy, flags, props, vp);
        }
        return handler->iterate(cx, proxy, flags, vp);
    }
    AutoIdVector props(cx);
    // The other Proxy::foo methods do the prototype-aware work for us here.
    if ((flags & JSITER_OWNONLY)
        ? !Proxy::getOwnEnumerablePropertyKeys(cx, proxy, props)
        : !Proxy::enumerate(cx, proxy, props)) {
        return false;
    }
    return EnumeratedIdVectorToIterator(cx, proxy, flags, props, vp);
}

bool
Proxy::isExtensible(JSContext *cx, HandleObject proxy, bool *extensible)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->isExtensible(cx, proxy, extensible);
}

bool
Proxy::preventExtensions(JSContext *cx, HandleObject proxy, bool *succeeded)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    return handler->preventExtensions(cx, proxy, succeeded);
}

bool
Proxy::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();

    // Because vp[0] is JS_CALLEE on the way in and JS_RVAL on the way out, we
    // can only set our default value once we're sure that we're not calling the
    // trap.
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE,
                           BaseProxyHandler::CALL, true);
    if (!policy.allowed()) {
        args.rval().setUndefined();
        return policy.returnValue();
    }

    return handler->call(cx, proxy, args);
}

bool
Proxy::construct(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();

    // Because vp[0] is JS_CALLEE on the way in and JS_RVAL on the way out, we
    // can only set our default value once we're sure that we're not calling the
    // trap.
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE,
                           BaseProxyHandler::CALL, true);
    if (!policy.allowed()) {
        args.rval().setUndefined();
        return policy.returnValue();
    }

    return handler->construct(cx, proxy, args);
}

bool
Proxy::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, &args.thisv().toObject());
    // Note - we don't enter a policy here because our security architecture
    // guards against nativeCall by overriding the trap itself in the right
    // circumstances.
    return proxy->as<ProxyObject>().handler()->nativeCall(cx, test, impl, args);
}

bool
Proxy::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    *bp = false; // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->hasInstance(cx, proxy, v, bp);
}

bool
Proxy::objectClassIs(HandleObject proxy, ESClassValue classValue, JSContext *cx)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->objectClassIs(proxy, classValue, cx);
}

const char *
Proxy::className(JSContext *cx, HandleObject proxy)
{
    // Check for unbounded recursion, but don't signal an error; className
    // needs to be infallible.
    int stackDummy;
    if (!JS_CHECK_STACK_SIZE(GetNativeStackLimit(cx), &stackDummy))
        return "too much recursion";

    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE,
                           BaseProxyHandler::GET, /* mayThrow = */ false);
    // Do the safe thing if the policy rejects.
    if (!policy.allowed()) {
        return handler->BaseProxyHandler::className(cx, proxy);
    }
    return handler->className(cx, proxy);
}

JSString *
Proxy::fun_toString(JSContext *cx, HandleObject proxy, unsigned indent)
{
    JS_CHECK_RECURSION(cx, return nullptr);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE,
                           BaseProxyHandler::GET, /* mayThrow = */ false);
    // Do the safe thing if the policy rejects.
    if (!policy.allowed())
        return handler->BaseProxyHandler::fun_toString(cx, proxy, indent);
    return handler->fun_toString(cx, proxy, indent);
}

bool
Proxy::regexp_toShared(JSContext *cx, HandleObject proxy, RegExpGuard *g)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->regexp_toShared(cx, proxy, g);
}

bool
Proxy::boxedValue_unbox(JSContext *cx, HandleObject proxy, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->boxedValue_unbox(cx, proxy, vp);
}

bool
Proxy::defaultValue(JSContext *cx, HandleObject proxy, JSType hint, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->defaultValue(cx, proxy, hint, vp);
}

JSObject * const TaggedProto::LazyProto = reinterpret_cast<JSObject *>(0x1);

/* static */ bool
Proxy::getPrototypeOf(JSContext *cx, HandleObject proxy, MutableHandleObject proto)
{
    MOZ_ASSERT(proxy->hasLazyPrototype());
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->getPrototypeOf(cx, proxy, proto);
}

/* static */ bool
Proxy::setPrototypeOf(JSContext *cx, HandleObject proxy, HandleObject proto, bool *bp)
{
    MOZ_ASSERT(proxy->hasLazyPrototype());
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->setPrototypeOf(cx, proxy, proto, bp);
}

/* static */ bool
Proxy::setImmutablePrototype(JSContext *cx, HandleObject proxy, bool *succeeded)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    return handler->setImmutablePrototype(cx, proxy, succeeded);
}

/* static */ bool
Proxy::watch(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleObject callable)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->watch(cx, proxy, id, callable);
}

/* static */ bool
Proxy::unwatch(JSContext *cx, JS::HandleObject proxy, JS::HandleId id)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->unwatch(cx, proxy, id);
}

/* static */ bool
Proxy::slice(JSContext *cx, HandleObject proxy, uint32_t begin, uint32_t end,
             HandleObject result)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::GET,
                           /* mayThrow = */ true);
    if (!policy.allowed()) {
        if (policy.returnValue()) {
            MOZ_ASSERT(!cx->isExceptionPending());
            return js::SliceSlowly(cx, proxy, proxy, begin, end, result);
        }
        return false;
    }
    return handler->slice(cx, proxy, begin, end, result);
}

JSObject *
js::proxy_innerObject(JSObject *obj)
{
    return obj->as<ProxyObject>().private_().toObjectOrNull();
}

bool
js::proxy_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                        MutableHandleObject objp, MutableHandleShape propp)
{
    bool found;
    if (!Proxy::has(cx, obj, id, &found))
        return false;

    if (found) {
        MarkNonNativePropertyFound(propp);
        objp.set(obj);
    } else {
        objp.set(nullptr);
        propp.set(nullptr);
    }
    return true;
}

bool
js::proxy_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                         MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

bool
js::proxy_LookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                        MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

bool
js::proxy_DefineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                        PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<PropertyDescriptor> desc(cx);
    desc.object().set(obj);
    desc.value().set(value);
    desc.setAttributes(attrs);
    desc.setGetter(getter);
    desc.setSetter(setter);
    return Proxy::defineProperty(cx, obj, id, &desc);
}

bool
js::proxy_DefineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
                         PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

bool
js::proxy_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                        PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

bool
js::proxy_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                     MutableHandleValue vp)
{
    return Proxy::get(cx, obj, receiver, id, vp);
}

bool
js::proxy_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                      MutableHandleValue vp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

bool
js::proxy_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                     MutableHandleValue vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

bool
js::proxy_SetGeneric(JSContext *cx, HandleObject obj, HandleId id,
                     MutableHandleValue vp, bool strict)
{
    return Proxy::set(cx, obj, obj, id, strict, vp);
}

bool
js::proxy_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                      MutableHandleValue vp, bool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

bool
js::proxy_SetElement(JSContext *cx, HandleObject obj, uint32_t index,
                     MutableHandleValue vp, bool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

bool
js::proxy_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, obj, id, &desc))
        return false;
    *attrsp = desc.attributes();
    return true;
}

bool
js::proxy_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    /* Lookup the current property descriptor so we have setter/getter/value. */
    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, obj, id, &desc))
        return false;
    desc.setAttributes(*attrsp);
    return Proxy::defineProperty(cx, obj, id, &desc);
}

bool
js::proxy_DeleteGeneric(JSContext *cx, HandleObject obj, HandleId id, bool *succeeded)
{
    bool deleted;
    if (!Proxy::delete_(cx, obj, id, &deleted))
        return false;
    *succeeded = deleted;
    return SuppressDeletedProperty(cx, obj, id);
}

void
js::proxy_Trace(JSTracer *trc, JSObject *obj)
{
    MOZ_ASSERT(obj->is<ProxyObject>());
    ProxyObject::trace(trc, obj);
}

/* static */ void
ProxyObject::trace(JSTracer *trc, JSObject *obj)
{
    ProxyObject *proxy = &obj->as<ProxyObject>();

#ifdef DEBUG
    if (trc->runtime()->gc.isStrictProxyCheckingEnabled() && proxy->is<WrapperObject>()) {
        JSObject *referent = MaybeForwarded(&proxy->private_().toObject());
        if (referent->compartment() != proxy->compartment()) {
            /*
             * Assert that this proxy is tracked in the wrapper map. We maintain
             * the invariant that the wrapped object is the key in the wrapper map.
             */
            Value key = ObjectValue(*referent);
            WrapperMap::Ptr p = proxy->compartment()->lookupWrapper(key);
            MOZ_ASSERT(p);
            MOZ_ASSERT(*p->value().unsafeGet() == ObjectValue(*proxy));
        }
    }
#endif

    // Note: If you add new slots here, make sure to change
    // nuke() to cope.
    MarkCrossCompartmentSlot(trc, obj, proxy->slotOfPrivate(), "private");
    MarkValue(trc, proxy->slotOfExtra(0), "extra0");

    /*
     * The GC can use the second reserved slot to link the cross compartment
     * wrappers into a linked list, in which case we don't want to trace it.
     */
    if (!proxy->is<CrossCompartmentWrapperObject>())
        MarkValue(trc, proxy->slotOfExtra(1), "extra1");
}

JSObject *
js::proxy_WeakmapKeyDelegate(JSObject *obj)
{
    MOZ_ASSERT(obj->is<ProxyObject>());
    return obj->as<ProxyObject>().handler()->weakmapKeyDelegate(obj);
}

bool
js::proxy_Convert(JSContext *cx, HandleObject proxy, JSType hint, MutableHandleValue vp)
{
    MOZ_ASSERT(proxy->is<ProxyObject>());
    return Proxy::defaultValue(cx, proxy, hint, vp);
}

void
js::proxy_Finalize(FreeOp *fop, JSObject *obj)
{
    // Suppress a bogus warning about finalize().
    JS::AutoSuppressGCAnalysis nogc;

    MOZ_ASSERT(obj->is<ProxyObject>());
    obj->as<ProxyObject>().handler()->finalize(fop, obj);
    js_free(GetProxyDataLayout(obj)->values);
}

void
js::proxy_ObjectMoved(JSObject *obj, const JSObject *old)
{
    MOZ_ASSERT(obj->is<ProxyObject>());
    obj->as<ProxyObject>().handler()->objectMoved(obj, old);
}

bool
js::proxy_HasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v, bool *bp)
{
    bool b;
    if (!Proxy::hasInstance(cx, proxy, v, &b))
        return false;
    *bp = !!b;
    return true;
}

bool
js::proxy_Call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject proxy(cx, &args.callee());
    MOZ_ASSERT(proxy->is<ProxyObject>());
    return Proxy::call(cx, proxy, args);
}

bool
js::proxy_Construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject proxy(cx, &args.callee());
    MOZ_ASSERT(proxy->is<ProxyObject>());
    return Proxy::construct(cx, proxy, args);
}

bool
js::proxy_Watch(JSContext *cx, HandleObject obj, HandleId id, HandleObject callable)
{
    return Proxy::watch(cx, obj, id, callable);
}

bool
js::proxy_Unwatch(JSContext *cx, HandleObject obj, HandleId id)
{
    return Proxy::unwatch(cx, obj, id);
}

bool
js::proxy_Slice(JSContext *cx, HandleObject proxy, uint32_t begin, uint32_t end,
                HandleObject result)
{
    return Proxy::slice(cx, proxy, begin, end, result);
}

const Class js::ProxyObject::class_ =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy));

const Class* const js::ProxyClassPtr = &js::ProxyObject::class_;

JS_FRIEND_API(JSObject *)
js::NewProxyObject(JSContext *cx, const BaseProxyHandler *handler, HandleValue priv, JSObject *proto_,
                   JSObject *parent_, const ProxyOptions &options)
{
    return ProxyObject::New(cx, handler, priv, TaggedProto(proto_), parent_,
                            options);
}

void
ProxyObject::renew(JSContext *cx, const BaseProxyHandler *handler, Value priv)
{
    MOZ_ASSERT_IF(IsCrossCompartmentWrapper(this), IsDeadProxyObject(this));
    MOZ_ASSERT(getParent() == cx->global());
    MOZ_ASSERT(getClass() == &ProxyObject::class_);
    MOZ_ASSERT(!getClass()->ext.innerObject);
    MOZ_ASSERT(hasLazyPrototype());

    setHandler(handler);
    setCrossCompartmentPrivate(priv);
    setExtra(0, UndefinedValue());
    setExtra(1, UndefinedValue());
}

JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, HandleObject obj)
{
    static const JSFunctionSpec static_methods[] = {
        JS_FN("create",         proxy_create,          2, 0),
        JS_FN("createFunction", proxy_createFunction,  3, 0),
        JS_FN("revocable",      proxy_revocable,       2, 0),
        JS_FS_END
    };

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, proxy, cx->names().Proxy, 2);
    if (!ctor)
        return nullptr;

    if (!JS_DefineFunctions(cx, ctor, static_methods))
        return nullptr;
    if (!JS_DefineProperty(cx, obj, "Proxy", ctor, 0,
                           JS_PropertyStub, JS_StrictPropertyStub)) {
        return nullptr;
    }

    global->setConstructor(JSProto_Proxy, ObjectValue(*ctor));
    return ctor;
}
