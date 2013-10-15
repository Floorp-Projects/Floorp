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
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/ObjectImpl-inl.h"

using namespace js;
using namespace js::gc;
using mozilla::ArrayLength;

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
        const jschar *prop = str ? str->getCharsZ(cx) : nullptr;
        JS_ReportErrorNumberUC(cx, js_GetErrorMessage, nullptr,
                               JSMSG_PROPERTY_ACCESS_DENIED, prop);
    }
}

#ifdef DEBUG
void
js::AutoEnterPolicy::recordEnter(JSContext *cx, HandleObject proxy, HandleId id)
{
    if (allowed()) {
        context = cx;
        enteredProxy.construct(proxy);
        enteredId.construct(id);
        prev = cx->runtime()->enteredPolicy;
        cx->runtime()->enteredPolicy = this;
    }
}

void
js::AutoEnterPolicy::recordLeave()
{
    if (!enteredProxy.empty()) {
        JS_ASSERT(context->runtime()->enteredPolicy == this);
        context->runtime()->enteredPolicy = prev;
    }
}

JS_FRIEND_API(void)
js::assertEnteredPolicy(JSContext *cx, JSObject *proxy, jsid id)
{
    MOZ_ASSERT(proxy->is<ProxyObject>());
    MOZ_ASSERT(cx->runtime()->enteredPolicy);
    MOZ_ASSERT(cx->runtime()->enteredPolicy->enteredProxy.ref().get() == proxy);
    MOZ_ASSERT(cx->runtime()->enteredPolicy->enteredId.ref().get() == id);
}
#endif

BaseProxyHandler::BaseProxyHandler(const void *family)
  : mFamily(family),
    mHasPrototype(false),
    mHasPolicy(false)
{
}

BaseProxyHandler::~BaseProxyHandler()
{
}

bool
BaseProxyHandler::enter(JSContext *cx, HandleObject wrapper, HandleId id, Action act,
                        bool *bp)
{
    *bp = true;
    return true;
}

bool
BaseProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    assertEnteredPolicy(cx, proxy, id);
    Rooted<PropertyDescriptor> desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, &desc, 0))
        return false;
    *bp = !!desc.object();
    return true;
}

bool
BaseProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    assertEnteredPolicy(cx, proxy, id);
    Rooted<PropertyDescriptor> desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, &desc, 0))
        return false;
    *bp = !!desc.object();
    return true;
}

bool
BaseProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                      HandleId id, MutableHandleValue vp)
{
    assertEnteredPolicy(cx, proxy, id);

    Rooted<PropertyDescriptor> desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, &desc, 0))
        return false;
    if (!desc.object()) {
        vp.setUndefined();
        return true;
    }
    if (!desc.getter() ||
        (!desc.hasGetterObject() && desc.getter() == JS_PropertyStub))
    {
        vp.set(desc.value());
        return true;
    }
    if (desc.hasGetterObject())
        return InvokeGetterOrSetter(cx, receiver, ObjectValue(*desc.getterObject()),
                                    0, nullptr, vp);
    if (!desc.isShared())
        vp.set(desc.value());
    else
        vp.setUndefined();

    if (desc.hasShortId()) {
        RootedId id(cx, INT_TO_JSID(desc.shortid()));
        return CallJSPropertyOp(cx, desc.getter(), receiver, id, vp);
    }
    return CallJSPropertyOp(cx, desc.getter(), receiver, id, vp);
}

bool
BaseProxyHandler::getElementIfPresent(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                      uint32_t index, MutableHandleValue vp, bool *present)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;

    assertEnteredPolicy(cx, proxy, id);

    if (!has(cx, proxy, id, present))
        return false;

    if (!*present) {
        Debug_SetValueRangeToCrashOnTouch(vp.address(), 1);
        return true;
    }

    return get(cx, proxy, receiver, id, vp);
}

bool
BaseProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                      HandleId id, bool strict, MutableHandleValue vp)
{
    assertEnteredPolicy(cx, proxy, id);

    Rooted<PropertyDescriptor> desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, &desc, JSRESOLVE_ASSIGNING))
        return false;
    /* The control-flow here differs from ::get() because of the fall-through case below. */
    if (desc.object()) {
        // Check for read-only properties.
        if (desc.isReadonly())
            return strict ? Throw(cx, id, JSMSG_CANT_REDEFINE_PROP) : true;
        if (!desc.setter()) {
            // Be wary of the odd explicit undefined setter case possible through
            // Object.defineProperty.
            if (!desc.hasSetterObject())
                desc.setSetter(JS_StrictPropertyStub);
        } else if (desc.hasSetterObject() || desc.setter() != JS_StrictPropertyStub) {
            if (!CallSetter(cx, receiver, id, desc.setter(), desc.attributes(), desc.shortid(), strict, vp))
                return false;
            if (!proxy->is<ProxyObject>() || proxy->as<ProxyObject>().handler() != this)
                return true;
            if (desc.isShared())
                return true;
        }
        if (!desc.getter()) {
            // Same as above for the null setter case.
            if (!desc.hasGetterObject())
                desc.setGetter(JS_PropertyStub);
        }
        desc.value().set(vp.get());
        return defineProperty(cx, receiver, id, &desc);
    }
    if (!getPropertyDescriptor(cx, proxy, id, &desc, JSRESOLVE_ASSIGNING))
        return false;
    if (desc.object()) {
        // Check for read-only properties.
        if (desc.isReadonly())
            return strict ? Throw(cx, id, JSMSG_CANT_REDEFINE_PROP) : true;
        if (!desc.setter()) {
            // Be wary of the odd explicit undefined setter case possible through
            // Object.defineProperty.
            if (!desc.hasSetterObject())
                desc.setSetter(JS_StrictPropertyStub);
        } else if (desc.hasSetterObject() || desc.setter() != JS_StrictPropertyStub) {
            if (!CallSetter(cx, receiver, id, desc.setter(), desc.attributes(), desc.shortid(), strict, vp))
                return false;
            if (!proxy->is<ProxyObject>() || proxy->as<ProxyObject>().handler() != this)
                return true;
            if (desc.isShared())
                return true;
        }
        if (!desc.getter()) {
            // Same as above for the null setter case.
            if (!desc.hasGetterObject())
                desc.setGetter(JS_PropertyStub);
        }
        desc.value().set(vp.get());
        return defineProperty(cx, receiver, id, &desc);
    }

    desc.object().set(receiver);
    desc.value().set(vp.get());
    desc.setAttributes(JSPROP_ENUMERATE);
    desc.setShortId(0);
    desc.setGetter(nullptr);
    desc.setSetter(nullptr); // Pick up the class getter/setter.
    return defineProperty(cx, receiver, id, &desc);
}

bool
BaseProxyHandler::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    JS_ASSERT(props.length() == 0);

    if (!getOwnPropertyNames(cx, proxy, props))
        return false;

    /* Select only the enumerable properties through in-place iteration. */
    Rooted<PropertyDescriptor> desc(cx);
    RootedId id(cx);
    size_t i = 0;
    for (size_t j = 0, len = props.length(); j < len; j++) {
        JS_ASSERT(i <= j);
        id = props[j];
        AutoWaivePolicy policy(cx, proxy, id);
        if (!getOwnPropertyDescriptor(cx, proxy, id, &desc, 0))
            return false;
        if (desc.object() && desc.isEnumerable())
            props[i++] = id;
    }

    JS_ASSERT(i <= props.length());
    props.resize(i);

    return true;
}

bool
BaseProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags, MutableHandleValue vp)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);

    AutoIdVector props(cx);
    if ((flags & JSITER_OWNONLY)
        ? !keys(cx, proxy, props)
        : !enumerate(cx, proxy, props)) {
        return false;
    }

    return EnumeratedIdVectorToIterator(cx, proxy, flags, props, vp);
}

bool
BaseProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    MOZ_ASSUME_UNREACHABLE("callable proxies should implement call trap");
}

bool
BaseProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    MOZ_ASSUME_UNREACHABLE("callable proxies should implement construct trap");
}

const char *
BaseProxyHandler::className(JSContext *cx, HandleObject proxy)
{
    return proxy->isCallable() ? "Function" : "Object";
}

JSString *
BaseProxyHandler::fun_toString(JSContext *cx, HandleObject proxy, unsigned indent)
{
    if (proxy->isCallable())
        return JS_NewStringCopyZ(cx, "function () {\n    [native code]\n}");
    ReportIsNotFunction(cx, ObjectValue(*proxy));
    return nullptr;
}

bool
BaseProxyHandler::regexp_toShared(JSContext *cx, HandleObject proxy,
                                  RegExpGuard *g)
{
    MOZ_ASSUME_UNREACHABLE("This should have been a wrapped regexp");
}

bool
BaseProxyHandler::defaultValue(JSContext *cx, HandleObject proxy, JSType hint,
                               MutableHandleValue vp)
{
    return DefaultValue(cx, proxy, hint, vp);
}

bool
BaseProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args)
{
    ReportIncompatible(cx, args);
    return false;
}

bool
BaseProxyHandler::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v, bool *bp)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedValue val(cx, ObjectValue(*proxy.get()));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, val, NullPtr());
    return false;
}

bool
BaseProxyHandler::objectClassIs(HandleObject proxy, ESClassValue classValue, JSContext *cx)
{
    return false;
}

void
BaseProxyHandler::finalize(JSFreeOp *fop, JSObject *proxy)
{
}

JSObject *
BaseProxyHandler::weakmapKeyDelegate(JSObject *proxy)
{
    return nullptr;
}

bool
BaseProxyHandler::getPrototypeOf(JSContext *cx, HandleObject proxy, MutableHandleObject protop)
{
    // The default implementation here just uses proto of the proxy object.
    protop.set(proxy->getTaggedProto().toObjectOrNull());
    return true;
}


bool
DirectProxyHandler::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc, unsigned flags)
{
    assertEnteredPolicy(cx, proxy, id);
    JS_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JS_GetPropertyDescriptorById(cx, target, id, 0, desc);
}

static bool
GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                         MutableHandle<PropertyDescriptor> desc)
{
    // If obj is a proxy, we can do better than just guessing. This is
    // important for certain types of wrappers that wrap other wrappers.
    if (obj->is<ProxyObject>())
        return Proxy::getOwnPropertyDescriptor(cx, obj, id, desc, flags);

    if (!JS_GetPropertyDescriptorById(cx, obj, id, flags, desc))
        return false;
    if (desc.object() != obj)
        desc.object().set(nullptr);
    return true;
}

bool
DirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy,
                                             HandleId id, MutableHandle<PropertyDescriptor> desc,
                                             unsigned flags)
{
    assertEnteredPolicy(cx, proxy, id);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetOwnPropertyDescriptor(cx, target, id, 0, desc);
}

bool
DirectProxyHandler::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                   MutableHandle<PropertyDescriptor> desc)
{
    assertEnteredPolicy(cx, proxy, id);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    RootedValue v(cx, desc.value());
    return CheckDefineProperty(cx, target, id, v, desc.getter(), desc.setter(), desc.attributes()) &&
           JS_DefinePropertyById(cx, target, id, v, desc.getter(), desc.setter(), desc.attributes());
}

bool
DirectProxyHandler::getOwnPropertyNames(JSContext *cx, HandleObject proxy,
                                        AutoIdVector &props)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyNames(cx, target, JSITER_OWNONLY | JSITER_HIDDEN, &props);
}

bool
DirectProxyHandler::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    assertEnteredPolicy(cx, proxy, id);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JS_DeletePropertyById2(cx, target, id, bp);
}

bool
DirectProxyHandler::enumerate(JSContext *cx, HandleObject proxy,
                              AutoIdVector &props)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    JS_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyNames(cx, target, 0, &props);
}

bool
DirectProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedValue target(cx, proxy->as<ProxyObject>().private_());
    return Invoke(cx, args.thisv(), target, args.length(), args.array(), args.rval());
}

bool
DirectProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedValue target(cx, proxy->as<ProxyObject>().private_());
    return InvokeConstructor(cx, target, args.length(), args.array(), args.rval().address());
}

bool
DirectProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                               CallArgs args)
{
    args.setThis(ObjectValue(*args.thisv().toObject().as<ProxyObject>().target()));
    if (!test(args.thisv())) {
        ReportIncompatible(cx, args);
        return false;
    }

    return CallNativeImpl(cx, impl, args);
}

bool
DirectProxyHandler::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v,
                                bool *bp)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    bool b;
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!JS_HasInstance(cx, target, v, &b))
        return false;
    *bp = !!b;
    return true;
}


bool
DirectProxyHandler::objectClassIs(HandleObject proxy, ESClassValue classValue,
                                  JSContext *cx)
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return ObjectClassIs(target, classValue, cx);
}

const char *
DirectProxyHandler::className(JSContext *cx, HandleObject proxy)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::className(cx, target);
}

JSString *
DirectProxyHandler::fun_toString(JSContext *cx, HandleObject proxy,
                                 unsigned indent)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return fun_toStringHelper(cx, target, indent);
}

bool
DirectProxyHandler::regexp_toShared(JSContext *cx, HandleObject proxy,
                                    RegExpGuard *g)
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return RegExpToShared(cx, target, g);
}

JSObject *
DirectProxyHandler::weakmapKeyDelegate(JSObject *proxy)
{
    return UncheckedUnwrap(proxy);
}

DirectProxyHandler::DirectProxyHandler(const void *family)
  : BaseProxyHandler(family)
{
}

bool
DirectProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    assertEnteredPolicy(cx, proxy, id);
    JS_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    bool found;
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!JS_HasPropertyById(cx, target, id, &found))
        return false;
    *bp = !!found;
    return true;
}

bool
DirectProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    assertEnteredPolicy(cx, proxy, id);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    Rooted<PropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, target, id, 0, &desc))
        return false;
    *bp = (desc.object() == target);
    return true;
}

bool
DirectProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                        HandleId id, MutableHandleValue vp)
{
    assertEnteredPolicy(cx, proxy, id);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::getGeneric(cx, target, receiver, id, vp);
}

bool
DirectProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                        HandleId id, bool strict, MutableHandleValue vp)
{
    assertEnteredPolicy(cx, proxy, id);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::setGeneric(cx, target, receiver, id, vp, strict);
}

bool
DirectProxyHandler::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyNames(cx, target, JSITER_OWNONLY, &props);
}

bool
DirectProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                            MutableHandleValue vp)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    JS_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetIterator(cx, target, flags, vp);
}

bool
DirectProxyHandler::isExtensible(JSContext *cx, HandleObject proxy, bool *extensible)
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::isExtensible(cx, target, extensible);
}

bool
DirectProxyHandler::preventExtensions(JSContext *cx, HandleObject proxy)
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::preventExtensions(cx, target);
}

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
    rval.set(IdToValue(id)); // Re-use out-param to avoid Rooted overhead.
    JSString *str = ToString<CanGC>(cx, rval);
    if (!str)
        return false;
    rval.setString(str);
    return Trap(cx, handler, fval, 1, rval.address(), rval);
}

static bool
Trap2(JSContext *cx, HandleObject handler, HandleValue fval, HandleId id, Value v_,
      MutableHandleValue rval)
{
    RootedValue v(cx, v_);
    rval.set(IdToValue(id)); // Re-use out-param to avoid Rooted overhead.
    JSString *str = ToString<CanGC>(cx, rval);
    if (!str)
        return false;
    rval.setString(str);
    Value argv[2] = { rval.get(), v };
    AutoValueArray ava(cx, argv, 2);
    return Trap(cx, handler, fval, 2, argv, rval);
}

static bool
ParsePropertyDescriptorObject(JSContext *cx, HandleObject obj, const Value &v,
                              MutableHandle<PropertyDescriptor> desc, bool complete = false)
{
    AutoPropDescArrayRooter descs(cx);
    PropDesc *d = descs.append();
    if (!d || !d->initialize(cx, v))
        return false;
    if (complete)
        d->complete();
    desc.object().set(obj);
    desc.value().set(d->hasValue() ? d->value() : UndefinedValue());
    JS_ASSERT(!(d->attributes() & JSPROP_SHORTID));
    desc.setAttributes(d->attributes());
    desc.setGetter(d->getter());
    desc.setSetter(d->setter());
    desc.setShortId(0);
    return true;
}

static bool
IndicatePropertyNotFound(MutableHandle<PropertyDescriptor> desc)
{
    desc.object().set(nullptr);
    return true;
}

static bool
ValueToBool(const Value &v, bool *bp)
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
        if (!JS_CHECK_OPERATION_LIMIT(cx))
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
    ScriptedIndirectProxyHandler();
    virtual ~ScriptedIndirectProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool preventExtensions(JSContext *cx, HandleObject proxy) MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                       MutableHandle<PropertyDescriptor> desc,
                                       unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc,
                                          unsigned flags) MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<PropertyDescriptor> desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     MutableHandleValue vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     bool strict, MutableHandleValue vp) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                         MutableHandleValue vp) MOZ_OVERRIDE;

    /* Spidermonkey extensions. */
    virtual bool isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) MOZ_OVERRIDE;
    virtual bool call(JSContext *cx, HandleObject proxy, const CallArgs &args) MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, HandleObject proxy, const CallArgs &args) MOZ_OVERRIDE;
    virtual bool nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                            CallArgs args) MOZ_OVERRIDE;
    virtual JSString *fun_toString(JSContext *cx, HandleObject proxy, unsigned indent) MOZ_OVERRIDE;

    static ScriptedIndirectProxyHandler singleton;
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
static Class CallConstructHolder = {
    "CallConstructHolder",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS
};

} /* anonymous namespace */

// This variable exists solely to provide a unique address for use as an identifier.
static const char sScriptedIndirectProxyHandlerFamily = 0;

ScriptedIndirectProxyHandler::ScriptedIndirectProxyHandler()
        : BaseProxyHandler(&sScriptedIndirectProxyHandlerFamily)
{
}

ScriptedIndirectProxyHandler::~ScriptedIndirectProxyHandler()
{
}

bool
ScriptedIndirectProxyHandler::isExtensible(JSContext *cx, HandleObject proxy, bool *extensible)
{
    // Scripted indirect proxies don't support extensibility changes.
    *extensible = true;
    return true;
}

bool
ScriptedIndirectProxyHandler::preventExtensions(JSContext *cx, HandleObject proxy)
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
                                 JSDVG_SEARCH_STACK, val, NullPtr(), bytes.ptr());
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
                                                    MutableHandle<PropertyDescriptor> desc,
                                                    unsigned flags)
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
                                                       MutableHandle<PropertyDescriptor> desc,
                                                       unsigned flags)
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
                                             MutableHandle<PropertyDescriptor> desc)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().defineProperty, &fval) &&
           NewPropertyDescriptorObject(cx, desc, &value) &&
           Trap2(cx, handler, fval, id, value, &value);
}

bool
ScriptedIndirectProxyHandler::getOwnPropertyNames(JSContext *cx, HandleObject proxy,
                                                  AutoIdVector &props)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().getOwnPropertyNames, &fval) &&
           Trap(cx, handler, fval, 0, nullptr, &value) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().delete_, &fval) &&
           Trap1(cx, handler, fval, id, &value) &&
           ValueToBool(value, bp);
}

bool
ScriptedIndirectProxyHandler::enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, cx->names().enumerate, &fval) &&
           Trap(cx, handler, fval, 0, nullptr, &value) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().has, &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::has(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, &value) &&
           ValueToBool(value, bp);
}

bool
ScriptedIndirectProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().hasOwn, &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, &value) &&
           ValueToBool(value, bp);
}

bool
ScriptedIndirectProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                  HandleId id, MutableHandleValue vp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue idv(cx, IdToValue(id));
    JSString *str = ToString<CanGC>(cx, idv);
    if (!str)
        return false;
    RootedValue value(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), value };
    AutoValueArray ava(cx, argv, 2);
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().get, &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::get(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval, 2, argv, vp);
}

bool
ScriptedIndirectProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                  HandleId id, bool strict, MutableHandleValue vp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue idv(cx, IdToValue(id));
    JSString *str = ToString<CanGC>(cx, idv);
    if (!str)
        return false;
    RootedValue value(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), value, vp.get() };
    AutoValueArray ava(cx, argv, 3);
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().set, &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::set(cx, proxy, receiver, id, strict, vp);
    return Trap(cx, handler, fval, 3, argv, &value);
}

bool
ScriptedIndirectProxyHandler::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().keys, &value))
        return false;
    if (!js_IsCallable(value))
        return BaseProxyHandler::keys(cx, proxy, props);
    return Trap(cx, handler, value, 0, nullptr, &value) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                                      MutableHandleValue vp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, cx->names().iterate, &value))
        return false;
    if (!js_IsCallable(value))
        return BaseProxyHandler::iterate(cx, proxy, flags, vp);
    return Trap(cx, handler, value, 0, nullptr, vp) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, cx->names().iterate, vp);
}

bool
ScriptedIndirectProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedObject ccHolder(cx, &proxy->as<ProxyObject>().extra(0).toObject());
    JS_ASSERT(ccHolder->getClass() == &CallConstructHolder);
    RootedValue call(cx, ccHolder->getReservedSlot(0));
    JS_ASSERT(call.isObject() && call.toObject().isCallable());
    return Invoke(cx, args.thisv(), call, args.length(), args.array(), args.rval());
}

bool
ScriptedIndirectProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
    RootedObject ccHolder(cx, &proxy->as<ProxyObject>().extra(0).toObject());
    JS_ASSERT(ccHolder->getClass() == &CallConstructHolder);
    RootedValue construct(cx, ccHolder->getReservedSlot(1));
    JS_ASSERT(construct.isObject() && construct.toObject().isCallable());
    return InvokeConstructor(cx, construct, args.length(), args.array(),
                             args.rval().address());
}

bool
ScriptedIndirectProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                         CallArgs args)
{
    return BaseProxyHandler::nativeCall(cx, test, impl, args);
}

JSString *
ScriptedIndirectProxyHandler::fun_toString(JSContext *cx, HandleObject proxy, unsigned indent)
{
    assertEnteredPolicy(cx, proxy, JSID_VOID);
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

ScriptedIndirectProxyHandler ScriptedIndirectProxyHandler::singleton;

static JSObject *
GetDirectProxyHandlerObject(JSObject *proxy)
{
    return proxy->as<ProxyObject>().extra(0).toObjectOrNull();
}

/* Derived class for all scripted direct proxy handlers. */
class ScriptedDirectProxyHandler : public DirectProxyHandler {
  public:
    ScriptedDirectProxyHandler();
    virtual ~ScriptedDirectProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool preventExtensions(JSContext *cx, HandleObject proxy) MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                       MutableHandle<PropertyDescriptor> desc,
                                       unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc,
                                          unsigned flags) MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<PropertyDescriptor> desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx,HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     MutableHandleValue vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     bool strict, MutableHandleValue vp) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                         MutableHandleValue vp) MOZ_OVERRIDE;

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, HandleObject proxy, const CallArgs &args) MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, HandleObject proxy, const CallArgs &args) MOZ_OVERRIDE;

    static ScriptedDirectProxyHandler singleton;
};

// This variable exists solely to provide a unique address for use as an identifier.
static const char sScriptedDirectProxyHandlerFamily = 0;

// Aux.2 FromGenericPropertyDescriptor(Desc)
static bool
FromGenericPropertyDescriptor(JSContext *cx, PropDesc *desc, MutableHandleValue rval)
{
    // Aux.2 step 1
    if (desc->isUndefined()) {
        rval.setUndefined();
        return true;
    }

    // steps 3-9
    if (!desc->makeObject(cx))
        return false;
    rval.set(desc->pd());
    return true;
}

/*
 * Aux.3 NormalizePropertyDescriptor(Attributes)
 *
 * NOTE: to minimize code duplication, the code for this function is shared with
 * that for Aux.4 NormalizeAndCompletePropertyDescriptor (see below). The
 * argument complete is used to distinguish between the two.
 */
static bool
NormalizePropertyDescriptor(JSContext *cx, MutableHandleValue vp, bool complete = false)
{
    // Aux.4 step 1
    if (complete && vp.isUndefined())
        return true;

    // Aux.3 steps 1-2 / Aux.4 steps 2-3
    AutoPropDescArrayRooter descs(cx);
    PropDesc *desc = descs.append();
    if (!desc || !desc->initialize(cx, vp.get()))
        return false;
    if (complete)
        desc->complete();
    JS_ASSERT(!vp.isPrimitive()); // due to desc->initialize
    RootedObject attributes(cx, &vp.toObject());

    /*
     * Aux.3 step 3 / Aux.4 step 4
     *
     * NOTE: Aux.4 step 4 actually specifies FromPropertyDescriptor here.
     * However, the way FromPropertyDescriptor is implemented (PropDesc::
     * makeObject) is actually closer to FromGenericPropertyDescriptor,
     * and is in fact used to implement the latter, so we might as well call it
     * directly.
     */
    if (!FromGenericPropertyDescriptor(cx, desc, vp))
        return false;
    if (vp.isUndefined())
        return true;
    RootedObject descObj(cx, &vp.toObject());

    // Aux.3 steps 4-5 / Aux.4 steps 5-6
    AutoIdVector props(cx);
    if (!GetPropertyNames(cx, attributes, 0, &props))
        return false;
    size_t n = props.length();
    for (size_t i = 0; i < n; ++i) {
        RootedId id(cx, props[i]);
        if (JSID_IS_ATOM(id)) {
            JSAtom *atom = JSID_TO_ATOM(id);
            const JSAtomState &atomState = cx->runtime()->atomState;
            if (atom == atomState.value || atom == atomState.writable ||
                atom == atomState.get || atom == atomState.set ||
                atom == atomState.enumerable || atom == atomState.configurable)
            {
                continue;
            }
        }

        RootedValue v(cx);
        if (!JSObject::getGeneric(cx, descObj, attributes, id, &v))
            return false;
        if (!JSObject::defineGeneric(cx, descObj, id, v, nullptr, nullptr, JSPROP_ENUMERATE))
            return false;
    }
    return true;
}

// Aux.4 NormalizeAndCompletePropertyDescriptor(Attributes)
static inline bool
NormalizeAndCompletePropertyDescriptor(JSContext *cx, MutableHandleValue vp)
{
    return NormalizePropertyDescriptor(cx, vp, true);
}

static inline bool
IsDataDescriptor(const PropertyDescriptor &desc)
{
    return desc.obj && !(desc.attrs & (JSPROP_GETTER | JSPROP_SETTER));
}

static inline bool
IsAccessorDescriptor(const PropertyDescriptor &desc)
{
    return desc.obj && desc.attrs & (JSPROP_GETTER | JSPROP_SETTER);
}

// Aux.5 ValidateProperty(O, P, Desc)
static bool
ValidateProperty(JSContext *cx, HandleObject obj, HandleId id, PropDesc *desc, bool *bp)
{
    // step 1
    Rooted<PropertyDescriptor> current(cx);
    if (!GetOwnPropertyDescriptor(cx, obj, id, 0, &current))
        return false;

    /*
     * steps 2-4 are redundant since ValidateProperty is never called unless
     * target.[[HasOwn]](P) is true
     */
    JS_ASSERT(current.object());

    // step 5
    if (!desc->hasValue() && !desc->hasWritable() && !desc->hasGet() && !desc->hasSet() &&
        !desc->hasEnumerable() && !desc->hasConfigurable())
    {
        *bp = true;
        return true;
    }

    // step 6
    if ((!desc->hasWritable() || desc->writable() == !current.isReadonly()) &&
        (!desc->hasGet() || desc->getter() == current.getter()) &&
        (!desc->hasSet() || desc->setter() == current.setter()) &&
        (!desc->hasEnumerable() || desc->enumerable() == current.isEnumerable()) &&
        (!desc->hasConfigurable() || desc->configurable() == !current.isPermanent()))
    {
        if (!desc->hasValue()) {
            *bp = true;
            return true;
        }
        bool same = false;
        if (!SameValue(cx, desc->value(), current.value(), &same))
            return false;
        if (same) {
            *bp = true;
            return true;
        }
    }

    // step 7
    if (current.isPermanent()) {
        if (desc->hasConfigurable() && desc->configurable()) {
            *bp = false;
            return true;
        }

        if (desc->hasEnumerable() &&
            desc->enumerable() != current.isEnumerable())
        {
            *bp = false;
            return true;
        }
    }

    // step 8
    if (desc->isGenericDescriptor()) {
        *bp = true;
        return true;
    }

    // step 9
    if (IsDataDescriptor(current) != desc->isDataDescriptor()) {
        *bp = !current.isPermanent();
        return true;
    }

    // step 10
    if (IsDataDescriptor(current)) {
        JS_ASSERT(desc->isDataDescriptor()); // by step 9
        if (current.isPermanent() && current.isReadonly()) {
            if (desc->hasWritable() && desc->writable()) {
                *bp = false;
                return true;
            }

            if (desc->hasValue()) {
                bool same;
                if (!SameValue(cx, desc->value(), current.value(), &same))
                    return false;
                if (!same) {
                    *bp = false;
                    return true;
                }
            }
        }

        *bp = true;
        return true;
    }

    // steps 11-12
    JS_ASSERT(IsAccessorDescriptor(current)); // by step 10
    JS_ASSERT(desc->isAccessorDescriptor()); // by step 9
    *bp = (!current.isPermanent() ||
           ((!desc->hasSet() || desc->setter() == current.setter()) &&
            (!desc->hasGet() || desc->getter() == current.getter())));
    return true;
}

// Aux.6 IsSealed(O, P)
static bool
IsSealed(JSContext* cx, HandleObject obj, HandleId id, bool *bp)
{
    // step 1
    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, obj, id, 0, &desc))
        return false;

    // steps 2-3
    *bp = desc.object() && desc.isPermanent();
    return true;
}

static bool
HasOwn(JSContext *cx, HandleObject obj, HandleId id, bool *bp)
{
    Rooted<PropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, obj, id, 0, &desc))
        return false;
    *bp = (desc.object() == obj);
    return true;
}

static bool
IdToValue(JSContext *cx, HandleId id, MutableHandleValue value)
{
    value.set(IdToValue(id)); // Re-use out-param to avoid Rooted overhead.
    JSString *name = ToString<CanGC>(cx, value);
    if (!name)
        return false;
    value.set(StringValue(name));
    return true;
}

// TrapGetOwnProperty(O, P)
static bool
TrapGetOwnProperty(JSContext *cx, HandleObject proxy, HandleId id, MutableHandleValue rval)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().getOwnPropertyDescriptor, &trap))
        return false;

    // step 4
    if (trap.isUndefined()) {
        Rooted<PropertyDescriptor> desc(cx);
        if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
            return false;
        return NewPropertyDescriptorObject(cx, desc, rval);
    }

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectValue(*target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step 6
    if (!NormalizeAndCompletePropertyDescriptor(cx, &trapResult))
        return false;

    // step 7
    if (trapResult.isUndefined()) {
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible) {
            bool found;
            if (!HasOwn(cx, target, id, &found))
                return false;
            if (found) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
                return false;
            }
        }

        rval.set(UndefinedValue());
        return true;
    }

    // step 8
    bool isFixed;
    if (!HasOwn(cx, target, id, &isFixed))
        return false;

    // step 9
    bool extensible;
    if (!JSObject::isExtensible(cx, target, &extensible))
        return false;
    if (extensible && !isFixed) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NEW);
        return false;
    }

    AutoPropDescArrayRooter descs(cx);
    PropDesc *desc = descs.append();
    if (!desc || !desc->initialize(cx, trapResult))
        return false;

    /* step 10 */
    if (isFixed) {
        bool valid;
        if (!ValidateProperty(cx, target, id, desc, &valid))
            return false;

        if (!valid) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_INVALID);
            return false;
        }
    }

    // step 11
    if (!desc->configurable() && !isFixed) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NE_AS_NC);
        return false;
    }

    // step 12
    rval.set(trapResult);
    return true;
}

// TrapDefineOwnProperty(O, P, DescObj, Throw)
static bool
TrapDefineOwnProperty(JSContext *cx, HandleObject proxy, HandleId id, MutableHandleValue vp)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().defineProperty, &trap))
        return false;

    // step 4
    if (trap.isUndefined()) {
        Rooted<PropertyDescriptor> desc(cx);
        if (!ParsePropertyDescriptorObject(cx, proxy, vp, &desc))
            return false;
        return JS_DefinePropertyById(cx, target, id, desc.value(), desc.getter(), desc.setter(),
                                     desc.attributes());
    }

    // step 5
    RootedValue normalizedDesc(cx, vp);
    if (!NormalizePropertyDescriptor(cx, &normalizedDesc))
        return false;

    // step 6
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectValue(*target),
        value,
        normalizedDesc
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // steps 7-8
    if (ToBoolean(trapResult)) {
        bool isFixed;
        if (!HasOwn(cx, target, id, &isFixed))
            return false;

        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible && !isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_DEFINE_NEW);
            return false;
        }

        AutoPropDescArrayRooter descs(cx);
        PropDesc *desc = descs.append();
        if (!desc || !desc->initialize(cx, normalizedDesc))
            return false;

        if (isFixed) {
            bool valid;
            if (!ValidateProperty(cx, target, id, desc, &valid))
                return false;
            if (!valid) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_DEFINE_INVALID);
                return false;
            }
        }

        if (!desc->configurable() && !isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_DEFINE_NE_AS_NC);
            return false;
        }

        vp.set(BooleanValue(true));
        return true;
    }

    // step 9
    // FIXME: API does not include a Throw parameter
    vp.set(BooleanValue(false));
    return true;
}

static inline void
ReportInvalidTrapResult(JSContext *cx, JSObject *proxy, JSAtom *atom)
{
    RootedValue v(cx, ObjectOrNullValue(proxy));
    JSAutoByteString bytes;
    if (!AtomToPrintableString(cx, atom, &bytes))
        return;
    js_ReportValueError2(cx, JSMSG_INVALID_TRAP_RESULT, JSDVG_IGNORE_STACK, v,
                         NullPtr(), bytes.ptr());
}

// This function is shared between getOwnPropertyNames, enumerate, and keys
static bool
ArrayToIdVector(JSContext *cx, HandleObject proxy, HandleObject target, HandleValue v,
                AutoIdVector &props, unsigned flags, JSAtom *trapName_)
{
    JS_ASSERT(v.isObject());
    RootedObject array(cx, &v.toObject());
    RootedAtom trapName(cx, trapName_);

    // steps g-h
    uint32_t n;
    if (!GetLengthProperty(cx, array, &n))
        return false;

    // steps i-k
    for (uint32_t i = 0; i < n; ++i) {
        // step i
        RootedValue v(cx);
        if (!JSObject::getElement(cx, array, array, i, &v))
            return false;

        // step ii
        RootedId id(cx);
        if (!ValueToId<CanGC>(cx, v, &id))
            return false;

        // step iii
        for (uint32_t j = 0; j < i; ++j) {
            if (props[j] == id) {
                ReportInvalidTrapResult(cx, proxy, trapName);
                return false;
            }
        }

        // step iv
        bool isFixed;
        if (!HasOwn(cx, target, id, &isFixed))
            return false;

        // step v
        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible && !isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NEW);
            return false;
        }

        // step vi
        if (!props.append(id))
            return false;
    }

    // step l
    AutoIdVector ownProps(cx);
    if (!GetPropertyNames(cx, target, flags, &ownProps))
        return false;

    // step m
    for (size_t i = 0; i < ownProps.length(); ++i) {
        RootedId id(cx, ownProps[i]);

        bool found = false;
        for (size_t j = 0; j < props.length(); ++j) {
            if (props[j] == id) {
                found = true;
               break;
            }
        }
        if (found)
            continue;

        // step i
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_SKIP_NC);
            return false;
        }

        // step ii
        bool isFixed;
        if (!HasOwn(cx, target, id, &isFixed))
            return false;

        // step iii
        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible && isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
            return false;
        }
    }

    // step n
    return true;
}

ScriptedDirectProxyHandler::ScriptedDirectProxyHandler()
        : DirectProxyHandler(&sScriptedDirectProxyHandlerFamily)
{
}

ScriptedDirectProxyHandler::~ScriptedDirectProxyHandler()
{
}

bool
ScriptedDirectProxyHandler::preventExtensions(JSContext *cx, HandleObject proxy)
{
    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().preventExtensions, &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::preventExtensions(cx, proxy);

    // step e
    Value argv[] = {
        ObjectValue(*target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step f
    bool success = ToBoolean(trapResult);
    if (success) {
        // step g
        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (extensible) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_AS_NON_EXTENSIBLE);
            return false;
        }
        return true;
    }

    // step h
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CHANGE_EXTENSIBILITY);
    return false;
}

// FIXME: Move to Proxy::getPropertyDescriptor once ScriptedIndirectProxy is removed
bool
ScriptedDirectProxyHandler::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                                  MutableHandle<PropertyDescriptor> desc,
                                                  unsigned flags)
{
    JS_CHECK_RECURSION(cx, return false);

    if (!GetOwnPropertyDescriptor(cx, proxy, id, desc))
        return false;
    if (desc.object())
        return true;
    RootedObject proto(cx);
    if (!JSObject::getProto(cx, proxy, &proto))
        return false;
    if (!proto) {
        JS_ASSERT(!desc.object());
        return true;
    }
    return JS_GetPropertyDescriptorById(cx, proto, id, 0, desc);
}

bool
ScriptedDirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                                     MutableHandle<PropertyDescriptor> desc,
                                                     unsigned flags)
{
    // step 1
    RootedValue v(cx);
    if (!TrapGetOwnProperty(cx, proxy, id, &v))
        return false;

    // step 2
    if (v.isUndefined()) {
        desc.object().set(nullptr);
        return true;
    }

    // steps 3-4
    return ParsePropertyDescriptorObject(cx, proxy, v, desc, true);
}

bool
ScriptedDirectProxyHandler::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                           MutableHandle<PropertyDescriptor> desc)
{
    // step 1
    AutoPropDescArrayRooter descs(cx);
    PropDesc *d = descs.append();
    d->initFromPropertyDescriptor(desc);
    RootedValue v(cx);
    if (!FromGenericPropertyDescriptor(cx, d, &v))
        return false;

    // step 2
    return TrapDefineOwnProperty(cx, proxy, id, &v);
}

bool
ScriptedDirectProxyHandler::getOwnPropertyNames(JSContext *cx, HandleObject proxy,
                                                AutoIdVector &props)
{
    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().getOwnPropertyNames, &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::getOwnPropertyNames(cx, proxy, props);

    // step e
    Value argv[] = {
        ObjectValue(*target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step f
    if (trapResult.isPrimitive()) {
        ReportInvalidTrapResult(cx, proxy, cx->names().getOwnPropertyNames);
        return false;
    }

    // steps g to n are shared
    return ArrayToIdVector(cx, proxy, target, trapResult, props, JSITER_OWNONLY | JSITER_HIDDEN,
                           cx->names().getOwnPropertyNames);
}

// Proxy.[[Delete]](P, Throw)
bool
ScriptedDirectProxyHandler::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().deleteProperty, &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::delete_(cx, proxy, id, bp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectValue(*target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step 6-7
    if (ToBoolean(trapResult)) {
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            RootedValue v(cx, IdToValue(id));
            js_ReportValueError(cx, JSMSG_CANT_DELETE, JSDVG_IGNORE_STACK, v, NullPtr());
            return false;
        }

        *bp = true;
        return true;
    }

    // step 8
    // FIXME: API does not include a Throw parameter
    *bp = false;
    return true;
}

// 12.6.4 The for-in Statement, step 6
bool
ScriptedDirectProxyHandler::enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().enumerate, &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::enumerate(cx, proxy, props);

    // step e
    Value argv[] = {
        ObjectOrNullValue(target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step f
    if (trapResult.isPrimitive()) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, cx->names().enumerate, &bytes))
            return false;
        RootedValue v(cx, ObjectOrNullValue(proxy));
        js_ReportValueError2(cx, JSMSG_INVALID_TRAP_RESULT, JSDVG_SEARCH_STACK,
                             v, NullPtr(), bytes.ptr());
        return false;
    }

    // steps g-m are shared
    // FIXME: the trap should return an iterator object, see bug 783826
    return ArrayToIdVector(cx, proxy, target, trapResult, props, 0, cx->names().enumerate);
}

// Proxy.[[HasProperty]](P)
bool
ScriptedDirectProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().has, &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::has(cx, proxy, id, bp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step 6
    bool success = ToBoolean(trapResult);;

    // step 7
    if (!success) {
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible) {
            bool isFixed;
            if (!HasOwn(cx, target, id, &isFixed))
                return false;
            if (isFixed) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
                return false;
            }
        }
    }

    // step 8
    *bp = success;
    return true;
}

// Proxy.[[HasOwnProperty]](P)
bool
ScriptedDirectProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().hasOwn, &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::hasOwn(cx, proxy, id, bp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step 6
    bool success = ToBoolean(trapResult);

    // steps 7-8
    if (!success) {
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible) {
            bool isFixed;
            if (!HasOwn(cx, target, id, &isFixed))
                return false;
            if (isFixed) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
                return false;
            }
        }
    } else {
        bool extensible;
        if (!JSObject::isExtensible(cx, target, &extensible))
            return false;
        if (!extensible) {
            bool isFixed;
            if (!HasOwn(cx, target, id, &isFixed))
                return false;
            if (!isFixed) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NEW);
                return false;
            }
        }
    }

    // step 9
    *bp = !!success;
    return true;
}

// Proxy.[[GetP]](P, Receiver)
bool
ScriptedDirectProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                HandleId id, MutableHandleValue vp)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().get, &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::get(cx, proxy, receiver, id, vp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value,
        ObjectOrNullValue(receiver)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step 6
    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
        return false;

    // step 7
    if (desc.object()) {
        if (IsDataDescriptor(desc) && desc.isPermanent() && desc.isReadonly()) {
            bool same;
            if (!SameValue(cx, vp, desc.value(), &same))
                return false;
            if (!same) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MUST_REPORT_SAME_VALUE);
                return false;
            }
        }

        if (IsAccessorDescriptor(desc) && desc.isPermanent() && !desc.hasGetterObject()) {
            if (!trapResult.isUndefined()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MUST_REPORT_UNDEFINED);
                return false;
            }
        }
    }

    // step 8
    vp.set(trapResult);
    return true;
}

// Proxy.[[SetP]](P, V, Receiver)
bool
ScriptedDirectProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                                HandleId id, bool strict, MutableHandleValue vp)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().set, &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::set(cx, proxy, receiver, id, strict, vp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value,
        vp.get(),
        ObjectValue(*receiver)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step 6
    bool success = ToBoolean(trapResult);

    // step 7
    if (success) {
        Rooted<PropertyDescriptor> desc(cx);
        if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
            return false;

        if (desc.object()) {
            if (IsDataDescriptor(desc) && desc.isPermanent() && desc.isReadonly()) {
                bool same;
                if (!SameValue(cx, vp, desc.value(), &same))
                    return false;
                if (!same) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_SET_NW_NC);
                    return false;
                }
            }

            if (IsAccessorDescriptor(desc) && desc.isPermanent() && !desc.hasSetterObject()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_SET_WO_SETTER);
                return false;
            }
        }
    }

    // step 8
    vp.set(BooleanValue(success));
    return true;
}

// 15.2.3.14 Object.keys (O), step 2
bool
ScriptedDirectProxyHandler::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().keys, &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::keys(cx, proxy, props);

    // step e
    Value argv[] = {
        ObjectOrNullValue(target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // step f
    if (trapResult.isPrimitive()) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, cx->names().keys, &bytes))
            return false;
        RootedValue v(cx, ObjectOrNullValue(proxy));
        js_ReportValueError2(cx, JSMSG_INVALID_TRAP_RESULT, JSDVG_IGNORE_STACK,
                             v, NullPtr(), bytes.ptr());
        return false;
    }

    // steps g-n are shared
    return ArrayToIdVector(cx, proxy, target, trapResult, props, JSITER_OWNONLY, cx->names().keys);
}

bool
ScriptedDirectProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                                    MutableHandleValue vp)
{
    // FIXME: Provide a proper implementation for this trap, see bug 787004
    return DirectProxyHandler::iterate(cx, proxy, flags, vp);
}

bool
ScriptedDirectProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    /*
     * NB: Remember to throw a TypeError here if we change NewProxyObject so that this trap can get
     * called for non-callable objects
     */

    // step 3
    RootedObject argsArray(cx, NewDenseCopiedArray(cx, args.length(), args.array()));
    if (!argsArray)
        return false;

    // step 4
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().apply, &trap))
        return false;

    // step 5
    if (trap.isUndefined())
        return DirectProxyHandler::call(cx, proxy, args);

    // step 6
    Value argv[] = {
        ObjectValue(*target),
        args.thisv(),
        ObjectValue(*argsArray)
    };
    RootedValue thisValue(cx, ObjectValue(*handler));
    return Invoke(cx, thisValue, trap, ArrayLength(argv), argv, args.rval());
}

bool
ScriptedDirectProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, proxy->as<ProxyObject>().target());

    /*
     * NB: Remember to throw a TypeError here if we change NewProxyObject so that this trap can get
     * called for non-callable objects
     */

    // step 3
    RootedObject argsArray(cx, NewDenseCopiedArray(cx, args.length(), args.array()));
    if (!argsArray)
        return false;

    // step 4
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, cx->names().construct, &trap))
        return false;

    // step 5
    if (trap.isUndefined())
        return DirectProxyHandler::construct(cx, proxy, args);

    // step 6
    Value constructArgv[] = {
        ObjectValue(*target),
        ObjectValue(*argsArray)
    };
    RootedValue thisValue(cx, ObjectValue(*handler));
    return Invoke(cx, thisValue, trap, ArrayLength(constructArgv), constructArgv, args.rval());
}

ScriptedDirectProxyHandler ScriptedDirectProxyHandler::singleton;

#define INVOKE_ON_PROTOTYPE(cx, handler, proxy, protoCall)                   \
    JS_BEGIN_MACRO                                                           \
        RootedObject proto(cx);                                              \
        if (!handler->getPrototypeOf(cx, proxy, &proto))                     \
            return false;                                                    \
        if (!proto)                                                          \
            return true;                                                     \
        assertSameCompartment(cx, proxy, proto);                             \
        return protoCall;                                                    \
    JS_END_MACRO                                                             \

bool
Proxy::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                             MutableHandle<PropertyDescriptor> desc, unsigned flags)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    desc.object().set(nullptr); // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    if (!handler->hasPrototype())
        return handler->getPropertyDescriptor(cx, proxy, id, desc, flags);
    if (!handler->getOwnPropertyDescriptor(cx, proxy, id, desc, flags))
        return false;
    if (desc.object())
        return true;
    INVOKE_ON_PROTOTYPE(cx, handler, proxy, JS_GetPropertyDescriptorById(cx, proto, id, 0, desc));
}

bool
Proxy::getPropertyDescriptor(JSContext *cx, HandleObject proxy, unsigned flags,
                             HandleId id, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getPropertyDescriptor(cx, proxy, id, &desc, flags))
        return false;
    return NewPropertyDescriptorObject(cx, desc, vp);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<PropertyDescriptor> desc, unsigned flags)
{
    JS_CHECK_RECURSION(cx, return false);

    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    desc.object().set(nullptr); // default result if we refuse to perform this action
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return handler->getOwnPropertyDescriptor(cx, proxy, id, desc, flags);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, unsigned flags, HandleId id,
                                MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, proxy, id, &desc, flags))
        return false;
    return NewPropertyDescriptorObject(cx, desc, vp);
}

bool
Proxy::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                      MutableHandle<PropertyDescriptor> desc)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::SET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->defineProperty(cx, proxy, id, desc);
}

bool
Proxy::defineProperty(JSContext *cx, HandleObject proxy, HandleId id, HandleValue v)
{
    JS_CHECK_RECURSION(cx, return false);
    Rooted<PropertyDescriptor> desc(cx);
    return ParsePropertyDescriptorObject(cx, proxy, v, &desc) &&
           Proxy::defineProperty(cx, proxy, id, &desc);
}

bool
Proxy::getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return proxy->as<ProxyObject>().handler()->getOwnPropertyNames(cx, proxy, props);
}

bool
Proxy::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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
            if (others[i] == base[j]) {
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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::GET, true);
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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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
Proxy::getElementIfPresent(JSContext *cx, HandleObject proxy, HandleObject receiver, uint32_t index,
                           MutableHandleValue vp, bool *present)
{
    JS_CHECK_RECURSION(cx, return false);

    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;

    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();

    if (!handler->hasPrototype()) {
        return handler->getElementIfPresent(cx, proxy, receiver, index,
                                            vp, present);
    }

    bool hasOwn;
    if (!handler->hasOwn(cx, proxy, id, &hasOwn))
        return false;

    if (hasOwn) {
        *present = true;
        return proxy->as<ProxyObject>().handler()->get(cx, proxy, receiver, id, vp);
    }

    *present = false;
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        JSObject::getElementIfPresent(cx, proto, receiver, index, vp, present));
}

bool
Proxy::set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id, bool strict,
           MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, id, BaseProxyHandler::SET, true);
    if (!policy.allowed())
        return policy.returnValue();
    if (handler->hasPrototype()) {
        // If we're using a prototype, we still want to use the proxy trap unless
        // we have a non-own property with a setter.
        bool hasOwn;
        if (!handler->hasOwn(cx, proxy, id, &hasOwn))
            return false;
        if (!hasOwn) {
            RootedObject proto(cx);
            if (!handler->getPrototypeOf(cx, proxy, &proto))
                return false;
            if (proto) {
                Rooted<PropertyDescriptor> desc(cx);
                if (!JS_GetPropertyDescriptorById(cx, proto, id, 0, &desc))
                    return false;
                if (desc.object() && desc.setter())
                    return JSObject::setGeneric(cx, proto, receiver, id, vp, strict);
            }
        }
    }
    return handler->set(cx, proxy, receiver, id, strict, vp);
}

bool
Proxy::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE, BaseProxyHandler::GET, true);
    if (!policy.allowed())
        return policy.returnValue();
    return handler->keys(cx, proxy, props);
}

bool
Proxy::iterate(JSContext *cx, HandleObject proxy, unsigned flags, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    vp.setUndefined(); // default result if we refuse to perform this action
    if (!handler->hasPrototype()) {
        AutoEnterPolicy policy(cx, handler, proxy, JSID_VOIDHANDLE,
                               BaseProxyHandler::GET, true);
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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
    return handler->preventExtensions(cx, proxy);
}

bool
Proxy::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    JS_CHECK_RECURSION(cx, return false);
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();

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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();

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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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

    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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
    BaseProxyHandler *handler = proxy->as<ProxyObject>().handler();
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
Proxy::defaultValue(JSContext *cx, HandleObject proxy, JSType hint, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->defaultValue(cx, proxy, hint, vp);
}

bool
Proxy::getPrototypeOf(JSContext *cx, HandleObject proxy, MutableHandleObject proto)
{
    JS_CHECK_RECURSION(cx, return false);
    return proxy->as<ProxyObject>().handler()->getPrototypeOf(cx, proxy, proto);
}

JSObject * const Proxy::LazyProto = reinterpret_cast<JSObject *>(0x1);

static JSObject *
proxy_innerObject(JSContext *cx, HandleObject obj)
{
    return obj->as<ProxyObject>().private_().toObjectOrNull();
}

static bool
proxy_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
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

static bool
proxy_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                     MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static bool
proxy_LookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static bool
proxy_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static bool
proxy_DefineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<PropertyDescriptor> desc(cx);
    desc.object().set(obj);
    desc.value().set(value);
    desc.setAttributes(attrs & (~JSPROP_SHORTID));
    desc.setGetter(getter);
    desc.setSetter(setter);
    desc.setShortId(0);
    return Proxy::defineProperty(cx, obj, id, &desc);
}

static bool
proxy_DefineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static bool
proxy_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static bool
proxy_DefineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static bool
proxy_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                 MutableHandleValue vp)
{
    return Proxy::get(cx, obj, receiver, id, vp);
}

static bool
proxy_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                  MutableHandleValue vp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static bool
proxy_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                 MutableHandleValue vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static bool
proxy_GetElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                          MutableHandleValue vp, bool *present)
{
    return Proxy::getElementIfPresent(cx, obj, receiver, index, vp, present);
}

static bool
proxy_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                 MutableHandleValue vp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static bool
proxy_SetGeneric(JSContext *cx, HandleObject obj, HandleId id,
                 MutableHandleValue vp, bool strict)
{
    return Proxy::set(cx, obj, obj, id, strict, vp);
}

static bool
proxy_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                  MutableHandleValue vp, bool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static bool
proxy_SetElement(JSContext *cx, HandleObject obj, uint32_t index,
                 MutableHandleValue vp, bool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static bool
proxy_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                 MutableHandleValue vp, bool strict)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static bool
proxy_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, obj, id, &desc, 0))
        return false;
    *attrsp = desc.attributes();
    return true;
}

static bool
proxy_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    /* Lookup the current property descriptor so we have setter/getter/value. */
    Rooted<PropertyDescriptor> desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, obj, id, &desc, JSRESOLVE_ASSIGNING))
        return false;
    desc.setAttributes(*attrsp & (~JSPROP_SHORTID));
    return Proxy::defineProperty(cx, obj, id, &desc);
}

static bool
proxy_DeleteGeneric(JSContext *cx, HandleObject obj, HandleId id, bool *succeeded)
{
    bool deleted;
    if (!Proxy::delete_(cx, obj, id, &deleted))
        return false;
    *succeeded = deleted;
    return js_SuppressDeletedProperty(cx, obj, id);
}

static bool
proxy_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, bool *succeeded)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DeleteGeneric(cx, obj, id, succeeded);
}

static bool
proxy_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index, bool *succeeded)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return proxy_DeleteGeneric(cx, obj, id, succeeded);
}

static bool
proxy_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, bool *succeeded)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_DeleteGeneric(cx, obj, id, succeeded);
}

/* static */ void
ProxyObject::trace(JSTracer *trc, JSObject *obj)
{
    ProxyObject *proxy = &obj->as<ProxyObject>();

#ifdef DEBUG
    if (!trc->runtime->gcDisableStrictProxyCheckingCount && proxy->is<WrapperObject>()) {
        JSObject *referent = &proxy->private_().toObject();
        if (referent->compartment() != proxy->compartment()) {
            /*
             * Assert that this proxy is tracked in the wrapper map. We maintain
             * the invariant that the wrapped object is the key in the wrapper map.
             */
            Value key = ObjectValue(*referent);
            WrapperMap::Ptr p = proxy->compartment()->lookupWrapper(key);
            JS_ASSERT(*p->value.unsafeGet() == ObjectValue(*proxy));
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
}

static JSObject *
proxy_WeakmapKeyDelegate(JSObject *obj)
{
    JS_ASSERT(obj->is<ProxyObject>());
    return obj->as<ProxyObject>().handler()->weakmapKeyDelegate(obj);
}

static bool
proxy_Convert(JSContext *cx, HandleObject proxy, JSType hint, MutableHandleValue vp)
{
    JS_ASSERT(proxy->is<ProxyObject>());
    return Proxy::defaultValue(cx, proxy, hint, vp);
}

static void
proxy_Finalize(FreeOp *fop, JSObject *obj)
{
    JS_ASSERT(obj->is<ProxyObject>());
    obj->as<ProxyObject>().handler()->finalize(fop, obj);
}

static bool
proxy_HasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v, bool *bp)
{
    bool b;
    if (!Proxy::hasInstance(cx, proxy, v, &b))
        return false;
    *bp = !!b;
    return true;
}

static bool
proxy_Call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject proxy(cx, &args.callee());
    JS_ASSERT(proxy->is<ProxyObject>());
    return Proxy::call(cx, proxy, args);
}

static bool
proxy_Construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject proxy(cx, &args.callee());
    JS_ASSERT(proxy->is<ProxyObject>());
    return Proxy::construct(cx, proxy, args);
}

#define PROXY_CLASS_EXT                             \
    {                                               \
        nullptr,             /* outerObject */      \
        nullptr,             /* innerObject */      \
        nullptr,             /* iteratorObject */   \
        false,               /* isWrappedNative */  \
        proxy_WeakmapKeyDelegate                    \
    }

#define PROXY_CLASS(callOp, constructOp) {          \
    "Proxy",                                        \
    Class::NON_NATIVE |                             \
           JSCLASS_IMPLEMENTS_BARRIERS |            \
           JSCLASS_HAS_RESERVED_SLOTS(4) |          \
    JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy),        \
    JS_PropertyStub,         /* addProperty */      \
    JS_DeletePropertyStub,   /* delProperty */      \
    JS_PropertyStub,         /* getProperty */      \
    JS_StrictPropertyStub,   /* setProperty */      \
    JS_EnumerateStub,                               \
    JS_ResolveStub,                                 \
    proxy_Convert,                                  \
    proxy_Finalize,          /* finalize    */      \
    nullptr,                 /* checkAccess */      \
    callOp,                  /* call        */      \
    proxy_HasInstance,       /* hasInstance */      \
    constructOp,             /* construct   */      \
    ProxyObject::trace,      /* trace       */      \
    PROXY_CLASS_EXT,                                \
    {                                               \
        proxy_LookupGeneric,                        \
        proxy_LookupProperty,                       \
        proxy_LookupElement,                        \
        proxy_LookupSpecial,                        \
        proxy_DefineGeneric,                        \
        proxy_DefineProperty,                       \
        proxy_DefineElement,                        \
        proxy_DefineSpecial,                        \
        proxy_GetGeneric,                           \
        proxy_GetProperty,                          \
        proxy_GetElement,                           \
        proxy_GetElementIfPresent,                  \
        proxy_GetSpecial,                           \
        proxy_SetGeneric,                           \
        proxy_SetProperty,                          \
        proxy_SetElement,                           \
        proxy_SetSpecial,                           \
        proxy_GetGenericAttributes,                 \
        proxy_SetGenericAttributes,                 \
        proxy_DeleteProperty,                       \
        proxy_DeleteElement,                        \
        proxy_DeleteSpecial,                        \
        nullptr,             /* enumerate       */  \
        nullptr,             /* thisObject      */  \
    }                                               \
}

const Class js::ProxyObject::uncallableClass_ = PROXY_CLASS(nullptr, nullptr);
const Class js::ProxyObject::callableClass_ = PROXY_CLASS(proxy_Call, proxy_Construct);

const Class* const js::CallableProxyClassPtr = &ProxyObject::callableClass_;
const Class* const js::UncallableProxyClassPtr = &ProxyObject::uncallableClass_;

const Class js::OuterWindowProxyObject::class_ = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(4),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    proxy_Finalize,          /* finalize    */
    nullptr,                 /* checkAccess */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    ProxyObject::trace,      /* trace       */
    {
        nullptr,             /* outerObject */
        proxy_innerObject,
        nullptr,             /* iteratorObject */
        false,               /* isWrappedNative */
        proxy_WeakmapKeyDelegate
    },
    {
        proxy_LookupGeneric,
        proxy_LookupProperty,
        proxy_LookupElement,
        proxy_LookupSpecial,
        proxy_DefineGeneric,
        proxy_DefineProperty,
        proxy_DefineElement,
        proxy_DefineSpecial,
        proxy_GetGeneric,
        proxy_GetProperty,
        proxy_GetElement,
        proxy_GetElementIfPresent,
        proxy_GetSpecial,
        proxy_SetGeneric,
        proxy_SetProperty,
        proxy_SetElement,
        proxy_SetSpecial,
        proxy_GetGenericAttributes,
        proxy_SetGenericAttributes,
        proxy_DeleteProperty,
        proxy_DeleteElement,
        proxy_DeleteSpecial,
        nullptr,             /* enumerate       */
        nullptr,             /* thisObject      */
    }
};

const Class* const js::OuterWindowProxyClassPtr = &OuterWindowProxyObject::class_;

JS_FRIEND_API(JSObject *)
js::NewProxyObject(JSContext *cx, BaseProxyHandler *handler, HandleValue priv, JSObject *proto_,
                   JSObject *parent_, const ProxyOptions &options)
{
    return ProxyObject::New(cx, handler, priv, TaggedProto(proto_), parent_,
                            options);
}

void
ProxyObject::renew(JSContext *cx, BaseProxyHandler *handler, Value priv)
{
    JS_ASSERT_IF(IsCrossCompartmentWrapper(this), IsDeadProxyObject(this));
    JS_ASSERT(getParent() == cx->global());
    JS_ASSERT(getClass() == &uncallableClass_);
    JS_ASSERT(getTaggedProto().isLazy());
#ifdef DEBUG
    AutoSuppressGC suppressGC(cx);
    JS_ASSERT(!handler->isOuterWindow());
#endif

    setSlot(HANDLER_SLOT, PrivateValue(handler));
    setCrossCompartmentSlot(PRIVATE_SLOT, priv);
    setSlot(EXTRA_SLOT + 0, UndefinedValue());
    setSlot(EXTRA_SLOT + 1, UndefinedValue());
}

static bool
proxy(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "Proxy", "1", "s");
        return false;
    }
    RootedObject target(cx, NonNullObject(cx, args[0]));
    if (!target)
        return false;
    RootedObject handler(cx, NonNullObject(cx, args[1]));
    if (!handler)
        return false;
    RootedObject proto(cx);
    if (!JSObject::getProto(cx, target, &proto))
        return false;
    RootedValue priv(cx, ObjectValue(*target));
    ProxyOptions options;
    options.setCallable(target->isCallable());
    ProxyObject *proxy =
        ProxyObject::New(cx, &ScriptedDirectProxyHandler::singleton,
                         priv, TaggedProto(proto), cx->global(),
                         options);
    if (!proxy)
        return false;
    proxy->setExtra(0, ObjectOrNullValue(handler));
    vp->setObject(*proxy);
    return true;
}

static bool
proxy_create(JSContext *cx, unsigned argc, Value *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "create", "0", "s");
        return false;
    }
    JSObject *handler = NonNullObject(cx, vp[2]);
    if (!handler)
        return false;
    JSObject *proto, *parent = nullptr;
    if (argc > 1 && vp[3].isObject()) {
        proto = &vp[3].toObject();
        parent = proto->getParent();
    } else {
        JS_ASSERT(IsFunctionObject(vp[0]));
        proto = nullptr;
    }
    if (!parent)
        parent = vp[0].toObject().getParent();
    RootedValue priv(cx, ObjectValue(*handler));
    JSObject *proxy = NewProxyObject(cx, &ScriptedIndirectProxyHandler::singleton,
                                     priv, proto, parent);
    if (!proxy)
        return false;

    vp->setObject(*proxy);
    return true;
}

static bool
proxy_createFunction(JSContext *cx, unsigned argc, Value *vp)
{
    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "createFunction", "1", "");
        return false;
    }
    RootedObject handler(cx, NonNullObject(cx, vp[2]));
    if (!handler)
        return false;
    RootedObject proto(cx), parent(cx);
    parent = vp[0].toObject().getParent();
    proto = parent->global().getOrCreateFunctionPrototype(cx);
    if (!proto)
        return false;
    parent = proto->getParent();

    RootedObject call(cx, ValueToCallable(cx, vp[3], argc - 2));
    if (!call)
        return false;
    RootedObject construct(cx, nullptr);
    if (argc > 2) {
        construct = ValueToCallable(cx, vp[4], argc - 3);
        if (!construct)
            return false;
    } else {
        construct = call;
    }

    // Stash the call and construct traps on a holder object that we can stick
    // in a slot on the proxy.
    RootedObject ccHolder(cx, JS_NewObjectWithGivenProto(cx, Jsvalify(&CallConstructHolder),
                                                         nullptr, cx->global()));
    if (!ccHolder)
        return false;
    ccHolder->setReservedSlot(0, ObjectValue(*call));
    ccHolder->setReservedSlot(1, ObjectValue(*construct));

    RootedValue priv(cx, ObjectValue(*handler));
    ProxyOptions options;
    options.setCallable(true);
    JSObject *proxy =
        ProxyObject::New(cx, &ScriptedIndirectProxyHandler::singleton,
                         priv, TaggedProto(proto), parent, options);
    if (!proxy)
        return false;
    proxy->as<ProxyObject>().setExtra(0, ObjectValue(*ccHolder));

    vp->setObject(*proxy);
    return true;
}

static const JSFunctionSpec static_methods[] = {
    JS_FN("create",         proxy_create,          2, 0),
    JS_FN("createFunction", proxy_createFunction,  3, 0),
    JS_FS_END
};

JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, HandleObject obj)
{
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, proxy, cx->names().Proxy, 2);
    if (!ctor)
        return nullptr;

    if (!JS_DefineFunctions(cx, ctor, static_methods))
        return nullptr;
    if (!JS_DefineProperty(cx, obj, "Proxy", OBJECT_TO_JSVAL(ctor),
                           JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return nullptr;
    }

    global->markStandardClassInitializedNoProto(&ProxyObject::uncallableClass_);
    return ctor;
}
