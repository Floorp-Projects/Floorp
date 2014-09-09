/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsproxy.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jswrapper.h"

#include "gc/Marking.h"
#include "proxy/ScriptedDirectProxyHandler.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/ObjectImpl-inl.h"

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
        JS_ASSERT(context->runtime()->enteredPolicy == this);
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

static bool
GetFundamentalTrap(JSContext *cx, HandleObject handler, HandlePropertyName name,
                   MutableHandleValue fvalp)
{
    JS_CHECK_RECURSION(cx, return false);

    return JSObject::getProperty(cx, handler, handler, name, fvalp);
}

static bool
GetDerivedTrap(JSContext *cx, HandleObject handler, HandlePropertyName name,
               MutableHandleValue fvalp)
{
    JS_ASSERT(name == cx->names().has ||
              name == cx->names().hasOwn ||
              name == cx->names().get ||
              name == cx->names().set ||
              name == cx->names().keys ||
              name == cx->names().iterate);

    return JSObject::getProperty(cx, handler, handler, name, fvalp);
}

static bool
Trap(JSContext *cx, HandleObject handler, HandleValue fval, unsigned argc, Value* argv,
     MutableHandleValue rval)
{
    return Invoke(cx, ObjectValue(*handler), fval, argc, argv, rval);
}

static bool
Trap1(JSContext *cx, HandleObject handler, HandleValue fval, HandleId id, MutableHandleValue rval)
{
    if (!IdToStringOrSymbol(cx, id, rval))
        return false;
    return Trap(cx, handler, fval, 1, rval.address(), rval);
}

static bool
Trap2(JSContext *cx, HandleObject handler, HandleValue fval, HandleId id, Value v_,
      MutableHandleValue rval)
{
    RootedValue v(cx, v_);
    if (!IdToStringOrSymbol(cx, id, rval))
        return false;
    JS::AutoValueArray<2> argv(cx);
    argv[0].set(rval);
    argv[1].set(v);
    return Trap(cx, handler, fval, 2, argv.begin(), rval);
}

static bool
IndicatePropertyNotFound(MutableHandle<PropertyDescriptor> desc)
{
    desc.object().set(nullptr);
    return true;
}

static bool
ValueToBool(HandleValue v, bool *bp)
{
    *bp = ToBoolean(v);
    return true;
}

static bool
ArrayToIdVector(JSContext *cx, const Value &array, AutoIdVector &props)
{
    JS_ASSERT(props.length() == 0);

    if (array.isPrimitive())
        return true;

    RootedObject obj(cx, &array.toObject());
    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    RootedValue v(cx);
    for (uint32_t n = 0; n < length; ++n) {
        if (!CheckForInterrupt(cx))
            return false;
        if (!JSObject::getElement(cx, obj, obj, n, &v))
            return false;
        RootedId id(cx);
        if (!ValueToId<CanGC>(cx, v, &id))
            return false;
        if (!props.append(id))
            return false;
    }

    return true;
}

namespace {

/* Derived class for all scripted indirect proxy handlers. */
class ScriptedIndirectProxyHandler : public BaseProxyHandler
{
  public:
    MOZ_CONSTEXPR ScriptedIndirectProxyHandler()
      : BaseProxyHandler(&family)
    { }

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool preventExtensions(JSContext *cx, HandleObject proxy) const MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                       MutableHandle<PropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<PropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, HandleObject proxy,
                                     AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) const MOZ_OVERRIDE;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     MutableHandleValue vp) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     bool strict, MutableHandleValue vp) const MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, HandleObject proxy, AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                         MutableHandleValue vp) const MOZ_OVERRIDE;

    /* Spidermonkey extensions. */
    virtual bool isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) const MOZ_OVERRIDE;
    virtual bool call(JSContext *cx, HandleObject proxy, const CallArgs &args) const MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, HandleObject proxy, const CallArgs &args) const MOZ_OVERRIDE;
    virtual bool nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                            CallArgs args) const MOZ_OVERRIDE;
    virtual JSString *fun_toString(JSContext *cx, HandleObject proxy, unsigned indent) const MOZ_OVERRIDE;
    virtual bool isScripted() const MOZ_OVERRIDE { return true; }

    static const char family;
    static const ScriptedIndirectProxyHandler singleton;
};

/*
 * Old-style indirect proxies allow callers to specify distinct scripted
 * [[Call]] and [[Construct]] traps. We use an intermediate object so that we
 * can stash this information in a single reserved slot on the proxy object.
 *
 * Note - Currently this is slightly unnecesary, because we actually have 2
 * extra slots, neither of which are used for ScriptedIndirectProxy. But we're
 * eventually moving towards eliminating one of those slots, and so we don't
 * want to add a dependency here.
 */
static const Class CallConstructHolder = {
    "CallConstructHolder",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS
};

} /* anonymous namespace */

// This variable exists solely to provide a unique address for use as an identifier.
const char ScriptedIndirectProxyHandler::family = 0;

bool
ScriptedIndirectProxyHandler::isExtensible(JSContext *cx, HandleObject proxy,
                                           bool *extensible) const
{
    // Scripted indirect proxies don't support extensibility changes.
    *extensible = true;
    return true;
}

bool
ScriptedIndirectProxyHandler::preventExtensions(JSContext *cx, HandleObject proxy) const
{
    // See above.
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CHANGE_EXTENSIBILITY);
    return false;
}

static bool
ReturnedValueMustNotBePrimitive(JSContext *cx, HandleObject proxy, JSAtom *atom, const Value &v)
{
    if (v.isPrimitive()) {
        JSAutoByteString bytes;
        if (AtomToPrintableString(cx, atom, &bytes)) {
            RootedValue val(cx, ObjectOrNullValue(proxy));
            js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                                 JSDVG_SEARCH_STACK, val, js::NullPtr(), bytes.ptr());
        }
        return false;
    }
    return true;
}

static JSObject *
GetIndirectProxyHandlerObject(JSObject *proxy)
{
    return proxy->as<ProxyObject>().private_().toObjectOrNull();
}

bool
ScriptedIndirectProxyHandler::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                                    MutableHandle<PropertyDescriptor> desc) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().getPropertyDescriptor, &fval) &&
           Trap1(cx, handler, fval, id, &value) &&
           ((value.get().isUndefined() && IndicatePropertyNotFound(desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, cx->names().getPropertyDescriptor, value) &&
             ParsePropertyDescriptorObject(cx, proxy, value, desc)));
}

bool
ScriptedIndirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                                       MutableHandle<PropertyDescriptor> desc) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().getOwnPropertyDescriptor, &fval) &&
           Trap1(cx, handler, fval, id, &value) &&
           ((value.get().isUndefined() && IndicatePropertyNotFound(desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, cx->names().getPropertyDescriptor, value) &&
             ParsePropertyDescriptorObject(cx, proxy, value, desc)));
}

bool
ScriptedIndirectProxyHandler::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                             MutableHandle<PropertyDescriptor> desc) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().defineProperty, &fval) &&
           NewPropertyDescriptorObject(cx, desc, &value) &&
           Trap2(cx, handler, fval, id, value, &value);
}

bool
ScriptedIndirectProxyHandler::getOwnPropertyNames(JSContext *cx, HandleObject proxy,
                                                  AutoIdVector &props) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().getOwnPropertyNames, &fval) &&
           Trap(cx, handler, fval, 0, nullptr, &value) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().delete_, &fval) &&
           Trap1(cx, handler, fval, id, &value) &&
           ValueToBool(value, bp);
}

bool
ScriptedIndirectProxyHandler::enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().enumerate, &fval) &&
           Trap(cx, handler, fval, 0, nullptr, &value) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().has, &fval))
        return false;
    if (!IsCallable(fval))
        return BaseProxyHandler::has(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, &value) &&
           ValueToBool(value, bp);
}

bool
ScriptedIndirectProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().hasOwn, &fval))
        return false;
    if (!IsCallable(fval))
        return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, &value) &&
           ValueToBool(value, bp);
}

bool
ScriptedIndirectProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                  HandleId id, MutableHandleValue vp) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue idv(cx);
    if (!IdToStringOrSymbol(cx, id, &idv))
        return false;
    JS::AutoValueArray<2> argv(cx);
    argv[0].setObjectOrNull(receiver);
    argv[1].set(idv);
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().get, &fval))
        return false;
    if (!IsCallable(fval))
        return BaseProxyHandler::get(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval, 2, argv.begin(), vp);
}

bool
ScriptedIndirectProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                  HandleId id, bool strict, MutableHandleValue vp) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue idv(cx);
    if (!IdToStringOrSymbol(cx, id, &idv))
        return false;
    JS::AutoValueArray<3> argv(cx);
    argv[0].setObjectOrNull(receiver);
    argv[1].set(idv);
    argv[2].set(vp);
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().set, &fval))
        return false;
    if (!IsCallable(fval))
        return BaseProxyHandler::set(cx, proxy, receiver, id, strict, vp);
    return Trap(cx, handler, fval, 3, argv.begin(), &idv);
}

bool
ScriptedIndirectProxyHandler::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().keys, &value))
        return false;
    if (!IsCallable(value))
        return BaseProxyHandler::keys(cx, proxy, props);
    return Trap(cx, handler, value, 0, nullptr, &value) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                                      MutableHandleValue vp) const
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().iterate, &value))
        return false;
    if (!IsCallable(value))
        return BaseProxyHandler::iterate(cx, proxy, flags, vp);
    return Trap(cx, handler, value, 0, nullptr, vp) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, cx->names().iterate, vp);
}

bool
ScriptedIndirectProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, CALL);
    RootedObject ccHolder(cx, &proxy->as<ProxyObject>().extra(0).toObject());
    JS_ASSERT(ccHolder->getClass() == &CallConstructHolder);
    RootedValue call(cx, ccHolder->getReservedSlot(0));
    JS_ASSERT(call.isObject() && call.toObject().isCallable());
    return Invoke(cx, args.thisv(), call, args.length(), args.array(), args.rval());
}

bool
ScriptedIndirectProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, CALL);
    RootedObject ccHolder(cx, &proxy->as<ProxyObject>().extra(0).toObject());
    JS_ASSERT(ccHolder->getClass() == &CallConstructHolder);
    RootedValue construct(cx, ccHolder->getReservedSlot(1));
    JS_ASSERT(construct.isObject() && construct.toObject().isCallable());
    return InvokeConstructor(cx, construct, args.length(), args.array(),
                             args.rval().address());
}

bool
ScriptedIndirectProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                         CallArgs args) const
{
    return BaseProxyHandler::nativeCall(cx, test, impl, args);
}

JSString *
ScriptedIndirectProxyHandler::fun_toString(JSContext *cx, HandleObject proxy, unsigned indent) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    if (!proxy->isCallable()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return nullptr;
    }
    RootedObject obj(cx, &proxy->as<ProxyObject>().extra(0).toObject().getReservedSlot(0).toObject());
    return fun_toStringHelper(cx, obj, indent);
}

const ScriptedIndirectProxyHandler ScriptedIndirectProxyHandler::singleton;


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
Proxy::getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::ENUMERATE, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->getOwnPropertyNames(cx, proxy, props);
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
    if (!handler->keys(cx, proxy, props))
        return false;
    AutoIdVector protoProps(cx);
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        GetPropertyNames(cx, proto, 0, &protoProps) &&
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
Proxy::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::ENUMERATE, true);
    if (!policy.allowed())
        return policy.returnValue();
    return handler->keys(cx, proxy, props);
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
        ? !Proxy::keys(cx, proxy, props)
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
Proxy::preventExtensions(JSContext *cx, HandleObject proxy)
{
    JS_CHECK_RECURSION(cx, return false);
    const BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    return handler->preventExtensions(cx, proxy);
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

bool
Proxy::getPrototypeOf(JSContext *cx, HandleObject proxy, MutableHandleObject proto)
{
    JS_ASSERT(proxy->getTaggedProto().isLazy());
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->getPrototypeOf(cx, proxy, proto);
}

bool
Proxy::setPrototypeOf(JSContext *cx, HandleObject proxy, HandleObject proto, bool *bp)
{
    JS_ASSERT(proxy->getTaggedProto().isLazy());
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->setPrototypeOf(cx, proxy, proto, bp);
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
            JS_ASSERT(!cx->isExceptionPending());
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
    return js_SuppressDeletedProperty(cx, obj, id);
}

void
js::proxy_Trace(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->is<ProxyObject>());
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
            JS_ASSERT(p);
            JS_ASSERT(*p->value().unsafeGet() == ObjectValue(*proxy));
        }
    }
#endif

    // Note: If you add new slots here, make sure to change
    // nuke() to cope.
    MarkCrossCompartmentSlot(trc, obj, proxy->slotOfPrivate(), "private");
    MarkSlot(trc, proxy->slotOfExtra(0), "extra0");

    /*
     * The GC can use the second reserved slot to link the cross compartment
     * wrappers into a linked list, in which case we don't want to trace it.
     */
    if (!proxy->is<CrossCompartmentWrapperObject>())
        MarkSlot(trc, proxy->slotOfExtra(1), "extra1");

    /*
     * Allow for people to add extra slots to "proxy" classes, without allowing
     * them to set their own trace hook. Trace the extras.
     */
    unsigned numSlots = JSCLASS_RESERVED_SLOTS(proxy->getClass());
    for (unsigned i = PROXY_MINIMUM_SLOTS; i < numSlots; i++)
        MarkSlot(trc, proxy->slotOfClassSpecific(i), "class-specific");
}

JSObject *
js::proxy_WeakmapKeyDelegate(JSObject *obj)
{
    JS_ASSERT(obj->is<ProxyObject>());
    return obj->as<ProxyObject>().handler()->weakmapKeyDelegate(obj);
}

bool
js::proxy_Convert(JSContext *cx, HandleObject proxy, JSType hint, MutableHandleValue vp)
{
    JS_ASSERT(proxy->is<ProxyObject>());
    return Proxy::defaultValue(cx, proxy, hint, vp);
}

void
js::proxy_Finalize(FreeOp *fop, JSObject *obj)
{
    JS_ASSERT(obj->is<ProxyObject>());
    obj->as<ProxyObject>().handler()->finalize(fop, obj);
}

void
js::proxy_ObjectMoved(JSObject *obj, const JSObject *old)
{
    JS_ASSERT(obj->is<ProxyObject>());
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
    JS_ASSERT(proxy->is<ProxyObject>());
    return Proxy::call(cx, proxy, args);
}

bool
js::proxy_Construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject proxy(cx, &args.callee());
    JS_ASSERT(proxy->is<ProxyObject>());
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

#define PROXY_CLASS(callOp, constructOp)                        \
    PROXY_CLASS_DEF("Proxy",                                    \
                    0, /* additional slots */                   \
                    JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy),    \
                    callOp,                                     \
                    constructOp)

const Class js::ProxyObject::uncallableClass_ = PROXY_CLASS(nullptr, nullptr);
const Class js::ProxyObject::callableClass_ = PROXY_CLASS(proxy_Call, proxy_Construct);

const Class* const js::CallableProxyClassPtr = &ProxyObject::callableClass_;
const Class* const js::UncallableProxyClassPtr = &ProxyObject::uncallableClass_;

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
    JS_ASSERT_IF(IsCrossCompartmentWrapper(this), IsDeadProxyObject(this));
    JS_ASSERT(getParent() == cx->global());
    JS_ASSERT(getClass() == &uncallableClass_);
    JS_ASSERT(!getClass()->ext.innerObject);
    JS_ASSERT(getTaggedProto().isLazy());

    setHandler(handler);
    setCrossCompartmentSlot(PRIVATE_SLOT, priv);
    setSlot(EXTRA_SLOT + 0, UndefinedValue());
    setSlot(EXTRA_SLOT + 1, UndefinedValue());
}

static bool
proxy_create(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "create", "0", "s");
        return false;
    }
    JSObject *handler = NonNullObject(cx, args[0]);
    if (!handler)
        return false;
    JSObject *proto, *parent = nullptr;
    if (args.get(1).isObject()) {
        proto = &args[1].toObject();
        parent = proto->getParent();
    } else {
        JS_ASSERT(IsFunctionObject(&args.callee()));
        proto = nullptr;
    }
    if (!parent)
        parent = args.callee().getParent();
    RootedValue priv(cx, ObjectValue(*handler));
    JSObject *proxy = NewProxyObject(cx, &ScriptedIndirectProxyHandler::singleton,
                                     priv, proto, parent);
    if (!proxy)
        return false;

    args.rval().setObject(*proxy);
    return true;
}

static bool
proxy_createFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "createFunction", "1", "");
        return false;
    }
    RootedObject handler(cx, NonNullObject(cx, args[0]));
    if (!handler)
        return false;
    RootedObject proto(cx), parent(cx);
    parent = args.callee().getParent();
    proto = parent->global().getOrCreateFunctionPrototype(cx);
    if (!proto)
        return false;
    parent = proto->getParent();

    RootedObject call(cx, ValueToCallable(cx, args[1], args.length() - 2));
    if (!call)
        return false;
    RootedObject construct(cx, nullptr);
    if (args.length() > 2) {
        construct = ValueToCallable(cx, args[2], args.length() - 3);
        if (!construct)
            return false;
    } else {
        construct = call;
    }

    // Stash the call and construct traps on a holder object that we can stick
    // in a slot on the proxy.
    RootedObject ccHolder(cx, JS_NewObjectWithGivenProto(cx, Jsvalify(&CallConstructHolder),
                                                         js::NullPtr(), cx->global()));
    if (!ccHolder)
        return false;
    ccHolder->setReservedSlot(0, ObjectValue(*call));
    ccHolder->setReservedSlot(1, ObjectValue(*construct));

    RootedValue priv(cx, ObjectValue(*handler));
    ProxyOptions options;
    options.selectDefaultClass(true);
    JSObject *proxy =
        ProxyObject::New(cx, &ScriptedIndirectProxyHandler::singleton,
                         priv, TaggedProto(proto), parent, options);
    if (!proxy)
        return false;
    proxy->as<ProxyObject>().setExtra(0, ObjectValue(*ccHolder));

    args.rval().setObject(*proxy);
    return true;
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
