/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <string.h>
#include "jsapi.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsprvtd.h"
#include "jsnum.h"
#include "jsproxy.h"
#include "jsscope.h"

#include "gc/Marking.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/RegExpObject-inl.h"

using namespace js;
using namespace js::gc;

static inline HeapSlot &
GetCall(JSObject *proxy)
{
    JS_ASSERT(IsFunctionProxy(proxy));
    return proxy->getSlotRef(JSSLOT_PROXY_CALL);
}

static inline Value
GetConstruct(JSObject *proxy)
{
    if (proxy->slotSpan() <= JSSLOT_PROXY_CONSTRUCT)
        return UndefinedValue();
    return proxy->getSlot(JSSLOT_PROXY_CONSTRUCT);
}

static inline HeapSlot &
GetFunctionProxyConstruct(JSObject *proxy)
{
    JS_ASSERT(IsFunctionProxy(proxy));
    JS_ASSERT(proxy->slotSpan() > JSSLOT_PROXY_CONSTRUCT);
    return proxy->getSlotRef(JSSLOT_PROXY_CONSTRUCT);
}

BaseProxyHandler::BaseProxyHandler(void *family)
  : mFamily(family),
    mHasPrototype(false)
{
}

BaseProxyHandler::~BaseProxyHandler()
{
}

bool
BaseProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
BaseProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoPropertyDescriptorRooter desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
BaseProxyHandler::get(JSContext *cx, JSObject *proxy, JSObject *receiver_, jsid id_, Value *vp)
{
    RootedObject receiver(cx, receiver_);
    RootedId id(cx, id_);

    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    if (!desc.obj) {
        vp->setUndefined();
        return true;
    }
    if (!desc.getter ||
        (!(desc.attrs & JSPROP_GETTER) && desc.getter == JS_PropertyStub)) {
        *vp = desc.value;
        return true;
    }
    if (desc.attrs & JSPROP_GETTER)
        return InvokeGetterOrSetter(cx, receiver, CastAsObjectJsval(desc.getter), 0, NULL, vp);
    if (!(desc.attrs & JSPROP_SHARED))
        *vp = desc.value;
    else
        vp->setUndefined();
    if (desc.attrs & JSPROP_SHORTID)
        id = INT_TO_JSID(desc.shortid);

    RootedValue value(cx, *vp);
    if (!CallJSPropertyOp(cx, desc.getter, receiver, id, &value))
        return false;

    *vp = value;
    return true;
}

bool
BaseProxyHandler::getElementIfPresent(JSContext *cx, JSObject *proxy_, JSObject *receiver_, uint32_t index, Value *vp, bool *present)
{
    RootedObject proxy(cx, proxy_);
    RootedObject receiver(cx, receiver_);

    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    if (!has(cx, proxy, id, present))
        return false;

    if (!*present) {
        Debug_SetValueRangeToCrashOnTouch(vp, 1);
        return true;
    }

    return get(cx, proxy, receiver, id, vp);
}

bool
BaseProxyHandler::set(JSContext *cx, JSObject *proxy_, JSObject *receiver_, jsid id_, bool strict,
                      Value *vp)
{
    RootedObject proxy(cx, proxy_), receiver(cx, receiver_);
    RootedId id(cx, id_);

    AutoPropertyDescriptorRooter desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, true, &desc))
        return false;
    /* The control-flow here differs from ::get() because of the fall-through case below. */
    if (desc.obj) {
        // Check for read-only properties.
        if (desc.attrs & JSPROP_READONLY)
            return strict ? Throw(cx, id, JSMSG_CANT_REDEFINE_PROP) : true;
        if (!desc.setter) {
            // Be wary of the odd explicit undefined setter case possible through
            // Object.defineProperty.
            if (!(desc.attrs & JSPROP_SETTER))
                desc.setter = JS_StrictPropertyStub;
        } else if ((desc.attrs & JSPROP_SETTER) || desc.setter != JS_StrictPropertyStub) {
            RootedValue value(cx, *vp);
            if (!CallSetter(cx, receiver, id, desc.setter, desc.attrs, desc.shortid, strict, &value))
                return false;
            *vp = value;
            if (!proxy->isProxy() || GetProxyHandler(proxy) != this)
                return true;
            if (desc.attrs & JSPROP_SHARED)
                return true;
        }
        if (!desc.getter) {
            // Same as above for the null setter case.
            if (!(desc.attrs & JSPROP_GETTER))
                desc.getter = JS_PropertyStub;
        }
        desc.value = *vp;
        return defineProperty(cx, receiver, id, &desc);
    }
    if (!getPropertyDescriptor(cx, proxy, id, true, &desc))
        return false;
    if (desc.obj) {
        // Check for read-only properties.
        if (desc.attrs & JSPROP_READONLY)
            return strict ? Throw(cx, id, JSMSG_CANT_REDEFINE_PROP) : true;
        if (!desc.setter) {
            // Be wary of the odd explicit undefined setter case possible through
            // Object.defineProperty.
            if (!(desc.attrs & JSPROP_SETTER))
                desc.setter = JS_StrictPropertyStub;
        } else if ((desc.attrs & JSPROP_SETTER) || desc.setter != JS_StrictPropertyStub) {
            RootedValue value(cx, *vp);
            if (!CallSetter(cx, receiver, id, desc.setter, desc.attrs, desc.shortid, strict, &value))
                return false;
            *vp = value;
            if (!proxy->isProxy() || GetProxyHandler(proxy) != this)
                return true;
            if (desc.attrs & JSPROP_SHARED)
                return true;
        }
        if (!desc.getter) {
            // Same as above for the null setter case.
            if (!(desc.attrs & JSPROP_GETTER))
                desc.getter = JS_PropertyStub;
        }
        desc.value = *vp;
        return defineProperty(cx, receiver, id, &desc);
    }

    desc.obj = receiver;
    desc.value = *vp;
    desc.attrs = JSPROP_ENUMERATE;
    desc.shortid = 0;
    desc.getter = NULL;
    desc.setter = NULL; // Pick up the class getter/setter.
    return defineProperty(cx, receiver, id, &desc);
}

bool
BaseProxyHandler::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_ASSERT(props.length() == 0);

    if (!getOwnPropertyNames(cx, proxy, props))
        return false;

    /* Select only the enumerable properties through in-place iteration. */
    AutoPropertyDescriptorRooter desc(cx);
    size_t i = 0;
    for (size_t j = 0, len = props.length(); j < len; j++) {
        JS_ASSERT(i <= j);
        jsid id = props[j];
        if (!getOwnPropertyDescriptor(cx, proxy, id, false, &desc))
            return false;
        if (desc.obj && (desc.attrs & JSPROP_ENUMERATE))
            props[i++] = id;
    }

    JS_ASSERT(i <= props.length());
    props.resize(i);

    return true;
}

bool
BaseProxyHandler::iterate(JSContext *cx, JSObject *proxy_, unsigned flags, Value *vp)
{
    RootedObject proxy(cx, proxy_);

    AutoIdVector props(cx);
    if ((flags & JSITER_OWNONLY)
        ? !keys(cx, proxy, props)
        : !enumerate(cx, proxy, props)) {
        return false;
    }

    RootedValue value(cx);
    if (!EnumeratedIdVectorToIterator(cx, proxy, flags, props, &value))
        return false;

    *vp = value;
    return true;
}

bool
BaseProxyHandler::call(JSContext *cx, JSObject *proxy, unsigned argc,
                       Value *vp)
{
    return ReportIsNotFunction(cx, UndefinedValue());
}

bool
BaseProxyHandler::construct(JSContext *cx, JSObject *proxy, unsigned argc,
                            Value *argv, Value *rval)
{
    return ReportIsNotFunction(cx, UndefinedValue(), CONSTRUCT);
}

JSString *
BaseProxyHandler::obj_toString(JSContext *cx, JSObject *proxy)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO,
                         js_Object_str, js_toString_str,
                         "object");
    return NULL;
}

JSString *
BaseProxyHandler::fun_toString(JSContext *cx, JSObject *proxy, unsigned indent)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO,
                         js_Function_str, js_toString_str,
                         "object");
    return NULL;
}

bool
BaseProxyHandler::regexp_toShared(JSContext *cx, JSObject *proxy,
                                  RegExpGuard *g)
{
    JS_NOT_REACHED("This should have been a wrapped regexp");
    return false;
}

bool
BaseProxyHandler::defaultValue(JSContext *cx, JSObject *proxy, JSType hint,
                               Value *vp)
{
    Rooted<JSObject*> obj(cx, proxy);
    RootedValue value(cx);
    if (!DefaultValue(cx, obj, hint, &value))
        return false;

    *vp = value;
    return true;
}

bool
BaseProxyHandler::iteratorNext(JSContext *cx, JSObject *proxy, Value *vp)
{
    vp->setMagic(JS_NO_ITER_VALUE);
    return true;
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
    RootedValue val(cx, ObjectValue(*proxy.get()));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, val, NullPtr());
    return false;
}

JSType
BaseProxyHandler::typeOf(JSContext *cx, JSObject *proxy)
{
    return IsFunctionProxy(proxy) ? JSTYPE_FUNCTION : JSTYPE_OBJECT;
}

bool
BaseProxyHandler::objectClassIs(JSObject *proxy, ESClassValue classValue, JSContext *cx)
{
    return false;
}

void
BaseProxyHandler::finalize(JSFreeOp *fop, JSObject *proxy)
{
}

bool
BaseProxyHandler::getPrototypeOf(JSContext *cx, JSObject *proxy, JSObject **proto)
{
    // The default implementation here just uses proto of the proxy object.
    JS_ASSERT(hasPrototype());
    *proto = proxy->getProto();
    return true;
}

IndirectProxyHandler::IndirectProxyHandler(void *family) : BaseProxyHandler(family)
{
}

bool
IndirectProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy,
                                            jsid id, bool set,
                                            PropertyDescriptor *desc)
{
    RootedObject target(cx, GetProxyTargetObject(proxy));
    return JS_GetPropertyDescriptorById(cx, target, id, JSRESOLVE_QUALIFIED, desc);
}

static bool
GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, jsid id, unsigned flags,
                         JSPropertyDescriptor *desc)
{
    // If obj is a proxy, we can do better than just guessing. This is
    // important for certain types of wrappers that wrap other wrappers.
    if (obj->isProxy())
        return Proxy::getOwnPropertyDescriptor(cx, obj, id,
                                               flags & JSRESOLVE_ASSIGNING,
                                               desc);

    if (!JS_GetPropertyDescriptorById(cx, obj, id, flags, desc))
        return false;
    if (desc->obj != obj)
        desc->obj = NULL;
    return true;
}

bool
IndirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy,
                                               jsid id, bool set,
                                               PropertyDescriptor *desc)
{
    RootedObject target(cx, GetProxyTargetObject(proxy));
    return GetOwnPropertyDescriptor(cx, target, id, JSRESOLVE_QUALIFIED, desc);
}

bool
IndirectProxyHandler::defineProperty(JSContext *cx, JSObject *proxy, jsid id_,
                                     PropertyDescriptor *desc)
{
    RootedObject obj(cx, GetProxyTargetObject(proxy));
    Rooted<jsid> id(cx, id_);
    Rooted<Value> v(cx, desc->value);
    return CheckDefineProperty(cx, obj, id, v, desc->getter, desc->setter, desc->attrs) &&
           JS_DefinePropertyById(cx, obj, id, v, desc->getter, desc->setter, desc->attrs);
}

bool
IndirectProxyHandler::getOwnPropertyNames(JSContext *cx, JSObject *proxy,
                                          AutoIdVector &props)
{
    RootedObject target(cx, GetProxyTargetObject(proxy));
    return GetPropertyNames(cx, target, JSITER_OWNONLY | JSITER_HIDDEN, &props);
}

bool
IndirectProxyHandler::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    Value v;
    RootedObject target(cx, GetProxyTargetObject(proxy));
    if (!JS_DeletePropertyById2(cx, target, id, &v))
        return false;
    JSBool b;
    if (!JS_ValueToBoolean(cx, v, &b))
        return false;
    *bp = !!b;
    return true;
}

bool
IndirectProxyHandler::enumerate(JSContext *cx, JSObject *proxy,
                                AutoIdVector &props)
{
    RootedObject target(cx, GetProxyTargetObject(proxy));
    return GetPropertyNames(cx, target, 0, &props);
}

bool
IndirectProxyHandler::call(JSContext *cx, JSObject *proxy, unsigned argc,
                   Value *vp)
{
    AutoValueRooter rval(cx);
    JSBool ok = Invoke(cx, vp[1], GetCall(proxy), argc, JS_ARGV(cx, vp), rval.addr());
    if (ok)
        JS_SET_RVAL(cx, vp, rval.value());
    return ok;
}

bool
IndirectProxyHandler::construct(JSContext *cx, JSObject *proxy, unsigned argc,
                                Value *argv, Value *rval)
{
    Value fval = GetConstruct(proxy);
    if (fval.isUndefined())
        fval = GetCall(proxy);
    return InvokeConstructor(cx, fval, argc, argv, rval);
}

bool
IndirectProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                    CallArgs args)
{
    args.setThis(ObjectValue(*GetProxyTargetObject(&args.thisv().toObject())));
    if (!test(args.thisv())) {
        ReportIncompatible(cx, args);
        return false;
    }

    return CallNativeImpl(cx, impl, args);
}

bool
IndirectProxyHandler::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v,
                                  bool *bp)
{
    JSBool b;
    RootedObject target(cx, GetProxyTargetObject(proxy));
    if (!JS_HasInstance(cx, target, v, &b))
        return false;
    *bp = !!b;
    return true;
}

JSType
IndirectProxyHandler::typeOf(JSContext *cx, JSObject *proxy)
{
    return TypeOfValue(cx, ObjectValue(*GetProxyTargetObject(proxy)));
}

bool
IndirectProxyHandler::objectClassIs(JSObject *proxy, ESClassValue classValue,
                                    JSContext *cx)
{
    return ObjectClassIs(*GetProxyTargetObject(proxy), classValue, cx);
}

JSString *
IndirectProxyHandler::obj_toString(JSContext *cx, JSObject *proxy)
{
    return obj_toStringHelper(cx, GetProxyTargetObject(proxy));
}

JSString *
IndirectProxyHandler::fun_toString(JSContext *cx, JSObject *proxy,
                                   unsigned indent)
{
    return fun_toStringHelper(cx, GetProxyTargetObject(proxy), indent);
}

bool
IndirectProxyHandler::regexp_toShared(JSContext *cx, JSObject *proxy,
                                      RegExpGuard *g)
{
    return RegExpToShared(cx, *GetProxyTargetObject(proxy), g);
}

bool
IndirectProxyHandler::defaultValue(JSContext *cx, JSObject *proxy, JSType hint,
                                   Value *vp)
{
    *vp = ObjectValue(*GetProxyTargetObject(proxy));
    if (hint == JSTYPE_VOID)
        return ToPrimitive(cx, vp);
    return ToPrimitive(cx, hint, vp);
}

bool
IndirectProxyHandler::iteratorNext(JSContext *cx, JSObject *proxy, Value *vp)
{
    Rooted<JSObject*> target(cx, GetProxyTargetObject(proxy));
    RootedValue value(cx);
    if (!js_IteratorMore(cx, target, &value))
        return false;
    *vp = value;
    if (vp->toBoolean()) {
        *vp = cx->iterValue;
        cx->iterValue = UndefinedValue();
    } else {
        *vp = MagicValue(JS_NO_ITER_VALUE);
    }
    return true;
}

DirectProxyHandler::DirectProxyHandler(void *family)
  : IndirectProxyHandler(family)
{
}

bool
DirectProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSBool found;
    RootedObject target(cx, GetProxyTargetObject(proxy));
    if (!JS_HasPropertyById(cx, target, id, &found))
        return false;
    *bp = !!found;
    return true;
}

bool
DirectProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    RootedObject target(cx, GetProxyTargetObject(proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, target, id, JSRESOLVE_QUALIFIED, &desc))
        return false;
    *bp = (desc.obj == target);
    return true;
}

bool
DirectProxyHandler::get(JSContext *cx, JSObject *proxy, JSObject *receiver_,
                        jsid id_, Value *vp)
{
    RootedObject receiver(cx, receiver_);
    RootedId id(cx, id_);
    RootedValue value(cx);
    RootedObject target(cx, GetProxyTargetObject(proxy));
    if (!JSObject::getGeneric(cx, target, receiver, id, &value))
        return false;

    *vp = value;
    return true;
}

bool
DirectProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiverArg,
                        jsid id_, bool strict, Value *vp)
{
    RootedId id(cx, id_);
    Rooted<JSObject*> receiver(cx, receiverArg);
    RootedValue value(cx, *vp);
    RootedObject target(cx, GetProxyTargetObject(proxy));
    if (!JSObject::setGeneric(cx, target, receiver, id, &value, strict))
        return false;

    *vp = value;
    return true;
}

bool
DirectProxyHandler::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return GetPropertyNames(cx, GetProxyTargetObject(proxy), JSITER_OWNONLY, &props);
}

bool
DirectProxyHandler::iterate(JSContext *cx, JSObject *proxy, unsigned flags,
                            Value *vp)
{
    Rooted<JSObject*> target(cx, GetProxyTargetObject(proxy));
    RootedValue value(cx);
    if (!GetIterator(cx, target, flags, &value))
        return false;

    *vp = value;
    return true;
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
    JS_ASSERT(name == ATOM(has) ||
              name == ATOM(hasOwn) ||
              name == ATOM(get) ||
              name == ATOM(set) ||
              name == ATOM(keys) ||
              name == ATOM(iterate));

    return JSObject::getProperty(cx, handler, handler, name, fvalp);
}

static bool
Trap(JSContext *cx, HandleObject handler, HandleValue fval, unsigned argc, Value* argv, Value *rval)
{
    return Invoke(cx, ObjectValue(*handler), fval, argc, argv, rval);
}

static bool
Trap1(JSContext *cx, HandleObject handler, HandleValue fval, HandleId id, Value *rval)
{
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    rval->setString(str);
    return Trap(cx, handler, fval, 1, rval, rval);
}

static bool
Trap2(JSContext *cx, HandleObject handler, HandleValue fval, HandleId id, Value v_, Value *rval)
{
    RootedValue v(cx, v_);
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    rval->setString(str);
    Value argv[2] = { *rval, v };
    AutoValueArray ava(cx, argv, 2);
    return Trap(cx, handler, fval, 2, argv, rval);
}

static bool
ParsePropertyDescriptorObject(JSContext *cx, HandleObject obj, const Value &v,
                              PropertyDescriptor *desc, bool complete = false)
{
    AutoPropDescArrayRooter descs(cx);
    PropDesc *d = descs.append();
    if (!d || !d->initialize(cx, v))
        return false;
    if (complete)
        d->complete();
    desc->obj = obj;
    desc->value = d->hasValue() ? d->value() : UndefinedValue();
    JS_ASSERT(!(d->attributes() & JSPROP_SHORTID));
    desc->attrs = d->attributes();
    desc->getter = d->getter();
    desc->setter = d->setter();
    desc->shortid = 0;
    return true;
}

static bool
IndicatePropertyNotFound(JSContext *cx, PropertyDescriptor *desc)
{
    desc->obj = NULL;
    return true;
}

static bool
ValueToBool(JSContext *cx, const Value &v, bool *bp)
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
        jsid id;
        if (!ValueToId(cx, v, &id))
            return false;
        if (!props.append(id))
            return false;
    }

    return true;
}

/* Derived class for all scripted indirect proxy handlers. */
class ScriptedIndirectProxyHandler : public IndirectProxyHandler {
  public:
    ScriptedIndirectProxyHandler();
    virtual ~ScriptedIndirectProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                       PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                          PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id,
                     Value *vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                     Value *vp) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JSObject *proxy, unsigned flags, Value *vp) MOZ_OVERRIDE;

    /* Spidermonkey extensions. */
    virtual bool nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                            CallArgs args) MOZ_OVERRIDE;
    virtual JSType typeOf(JSContext *cx, JSObject *proxy);
    virtual bool defaultValue(JSContext *cx, JSObject *obj, JSType hint, Value *vp) MOZ_OVERRIDE;

    static ScriptedIndirectProxyHandler singleton;
};

static int sScriptedIndirectProxyHandlerFamily = 0;

ScriptedIndirectProxyHandler::ScriptedIndirectProxyHandler()
        : IndirectProxyHandler(&sScriptedIndirectProxyHandlerFamily)
{
}

ScriptedIndirectProxyHandler::~ScriptedIndirectProxyHandler()
{
}

static bool
ReturnedValueMustNotBePrimitive(JSContext *cx, JSObject *proxy, JSAtom *atom, const Value &v)
{
    if (v.isPrimitive()) {
        JSAutoByteString bytes;
        if (js_AtomToPrintableString(cx, atom, &bytes)) {
            RootedValue val(cx, ObjectOrNullValue(proxy));
            js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                                 JSDVG_SEARCH_STACK, val, NullPtr(), bytes.ptr());
        }
        return false;
    }
    return true;
}

static JSObject *
GetIndirectProxyHandlerObject(JSContext *cx, JSObject *proxy)
{
    return GetProxyPrivate(proxy).toObjectOrNull();
}

bool
ScriptedIndirectProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_, bool set,
                                                    PropertyDescriptor *desc)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getPropertyDescriptor), &fval) &&
           Trap1(cx, handler, fval, id, value.address()) &&
           ((value.get().isUndefined() && IndicatePropertyNotFound(cx, desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), value) &&
             ParsePropertyDescriptorObject(cx, proxy, value, desc)));
}

bool
ScriptedIndirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_, bool set,
                                                       PropertyDescriptor *desc)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getOwnPropertyDescriptor), &fval) &&
           Trap1(cx, handler, fval, id, value.address()) &&
           ((value.get().isUndefined() && IndicatePropertyNotFound(cx, desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), value) &&
             ParsePropertyDescriptorObject(cx, proxy, value, desc)));
}

bool
ScriptedIndirectProxyHandler::defineProperty(JSContext *cx, JSObject *proxy, jsid id_,
                                             PropertyDescriptor *desc)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    RootedId id(cx, id_);
    return GetFundamentalTrap(cx, handler, ATOM(defineProperty), &fval) &&
           NewPropertyDescriptorObject(cx, desc, value.address()) &&
           Trap2(cx, handler, fval, id, value, value.address());
}

bool
ScriptedIndirectProxyHandler::getOwnPropertyNames(JSContext *cx, JSObject *proxy,
                                                  AutoIdVector &props)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getOwnPropertyNames), &fval) &&
           Trap(cx, handler, fval, 0, NULL, value.address()) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::delete_(JSContext *cx, JSObject *proxy, jsid id_, bool *bp)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedId id(cx, id_);
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(delete_), &fval) &&
           Trap1(cx, handler, fval, id, value.address()) &&
           ValueToBool(cx, value, bp);
}

bool
ScriptedIndirectProxyHandler::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(enumerate), &fval) &&
           Trap(cx, handler, fval, 0, NULL, value.address()) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::has(JSContext *cx, JSObject *proxy_, jsid id_, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(has), &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::has(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, value.address()) &&
           ValueToBool(cx, value, bp);
}

bool
ScriptedIndirectProxyHandler::hasOwn(JSContext *cx, JSObject *proxy_, jsid id_, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(hasOwn), &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, value.address()) &&
           ValueToBool(cx, value, bp);
}

bool
ScriptedIndirectProxyHandler::get(JSContext *cx, JSObject *proxy_, JSObject *receiver_, jsid id_,
                                  Value *vp)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_), receiver(cx, receiver_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    RootedValue value(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), value };
    AutoValueArray ava(cx, argv, 2);
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(get), &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::get(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval, 2, argv, vp);
}

bool
ScriptedIndirectProxyHandler::set(JSContext *cx, JSObject *proxy_, JSObject *receiver_, jsid id_, bool strict,
                                  Value *vp)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_), receiver(cx, receiver_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    RootedValue value(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), value, *vp };
    AutoValueArray ava(cx, argv, 3);
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(set), &fval))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::set(cx, proxy, receiver, id, strict, vp);
    return Trap(cx, handler, fval, 3, argv, value.address());
}

bool
ScriptedIndirectProxyHandler::keys(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(keys), &value))
        return false;
    if (!js_IsCallable(value))
        return BaseProxyHandler::keys(cx, proxy, props);
    return Trap(cx, handler, value, 0, NULL, value.address()) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedIndirectProxyHandler::iterate(JSContext *cx, JSObject *proxy_, unsigned flags, Value *vp)
{
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetIndirectProxyHandlerObject(cx, proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(iterate), &value))
        return false;
    if (!js_IsCallable(value))
        return BaseProxyHandler::iterate(cx, proxy, flags, vp);
    return Trap(cx, handler, value, 0, NULL, vp) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(iterate), *vp);
}

bool
ScriptedIndirectProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                                         CallArgs args)
{
    return BaseProxyHandler::nativeCall(cx, test, impl, args);
}


JSType
ScriptedIndirectProxyHandler::typeOf(JSContext *cx, JSObject *proxy)
{
    /*
     * This function is only here to prevent a regression in
     * js1_8_5/extensions/scripted-proxies.js. It will be removed when the
     * direct proxy refactor is complete.
     */
    return BaseProxyHandler::typeOf(cx, proxy);
}

bool
ScriptedIndirectProxyHandler::defaultValue(JSContext *cx, JSObject *proxy, JSType hint, Value *vp)
{
    /*
     * This function is only here to prevent bug 757063. It will be removed when
     * the direct proxy refactor is complete.
     */
    return BaseProxyHandler::defaultValue(cx, proxy, hint, vp);
}

ScriptedIndirectProxyHandler ScriptedIndirectProxyHandler::singleton;

static JSObject *
GetDirectProxyHandlerObject(JSObject *proxy)
{
    return GetProxyExtra(proxy, 0).toObjectOrNull();
}

/* Derived class for all scripted direct proxy handlers. */
class ScriptedDirectProxyHandler : public DirectProxyHandler {
  public:
    ScriptedDirectProxyHandler();
    virtual ~ScriptedDirectProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                       PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                          PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id,
                     Value *vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                     Value *vp) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JSObject *proxy, unsigned flags, Value *vp) MOZ_OVERRIDE;

    virtual bool call(JSContext *cx, JSObject *proxy, unsigned argc, Value *vp) MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, JSObject *proxy, unsigned argc, Value *argv,
                           Value *rval) MOZ_OVERRIDE;

    static ScriptedDirectProxyHandler singleton;
};

static int sScriptedDirectProxyHandlerFamily = 0;

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
            const JSAtomState &atomState = cx->runtime->atomState;
            if (atom == atomState.valueAtom || atom == atomState.writableAtom ||
                atom == atomState.getAtom || atom == atomState.setAtom ||
                atom == atomState.enumerableAtom || atom == atomState.configurableAtom)
            {
                continue;
            }
        }

        RootedValue v(cx);
        if (!JSObject::getGeneric(cx, descObj, attributes, id, &v))
            return false;
        if (!JSObject::defineGeneric(cx, descObj, id, v, NULL, NULL, JSPROP_ENUMERATE))
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
    AutoPropertyDescriptorRooter current(cx);
    if (!GetOwnPropertyDescriptor(cx, obj, id, 0, &current))
        return false;

    /*
     * steps 2-4 are redundant since ValidateProperty is never called unless
     * target.[[HasOwn]](P) is true
     */
    JS_ASSERT(current.obj);

    // step 5
    if (!desc->hasValue() && !desc->hasWritable() && !desc->hasGet() && !desc->hasSet() &&
        !desc->hasEnumerable() && !desc->hasConfigurable())
    {
        *bp = true;
        return true;
    }

    // step 6
    if ((!desc->hasWritable() || desc->writable() == !(current.attrs & JSPROP_READONLY)) &&
        (!desc->hasGet() || desc->getter() == current.getter) &&
        (!desc->hasSet() || desc->setter() == current.setter) &&
        (!desc->hasEnumerable() || desc->enumerable() == bool(current.attrs & JSPROP_ENUMERATE)) &&
        (!desc->hasConfigurable() || desc->configurable() == !(current.attrs & JSPROP_PERMANENT)))
    {
        if (!desc->hasValue()) {
            *bp = true;
            return true;
        }
        bool same = false;
        if (!SameValue(cx, desc->value(), current.value, &same))
            return false;
        if (same) {
            *bp = true;
            return true;
        }
    }

    // step 7
    if (current.attrs & JSPROP_PERMANENT) {
        if (desc->hasConfigurable() && desc->configurable()) {
            *bp = false;
            return true;
        }

        if (desc->hasEnumerable() &&
            desc->enumerable() != bool(current.attrs & JSPROP_ENUMERATE))
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
        *bp = !(current.attrs & JSPROP_PERMANENT);
        return true;
    }

    // step 10
    if (IsDataDescriptor(current)) {
        JS_ASSERT(desc->isDataDescriptor()); // by step 9
        if ((current.attrs & JSPROP_PERMANENT) && (current.attrs & JSPROP_READONLY)) {
            if (desc->hasWritable() && desc->writable()) {
                *bp = false;
                return true;
            }

            if (desc->hasValue()) {
                bool same;
                if (!SameValue(cx, desc->value(), current.value, &same))
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
    *bp = (!(current.attrs & JSPROP_PERMANENT) ||
           ((!desc->hasSet() || desc->setter() == current.setter) &&
            (!desc->hasGet() || desc->getter() == current.getter)));
    return true;
}

// Aux.6 IsSealed(O, P)
static bool
IsSealed(JSContext* cx, HandleObject obj, HandleId id, bool *bp)
{
    // step 1
    AutoPropertyDescriptorRooter desc(cx);
    if (!GetOwnPropertyDescriptor(cx, obj, id, 0, &desc))
        return false;

    // steps 2-3
    *bp = desc.obj && (desc.attrs & JSPROP_PERMANENT);
    return true;
}

static bool
HasOwn(JSContext *cx, HandleObject obj, HandleId id, bool *bp)
{
    AutoPropertyDescriptorRooter desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, obj, id, JSRESOLVE_QUALIFIED, &desc))
        return false;
    *bp = (desc.obj == obj);
    return true;
}

static bool
IdToValue(JSContext *cx, HandleId id, MutableHandleValue value)
{
    JSString *name = ToString(cx, IdToValue(id));
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
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(getOwnPropertyDescriptor), &trap))
        return false;

    // step 4
    if (trap.isUndefined()) {
        AutoPropertyDescriptorRooter desc(cx);
        if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
            return false;
        return NewPropertyDescriptorObject(cx, &desc, rval.address());
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
    if (!Invoke(cx, ObjectValue(*handler), trap, 2, argv, trapResult.address()))
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        if (!target->isExtensible()) {
            bool found;
            if (!HasOwn(cx, target, id, &found))
                return false;
            if (found) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_E_AS_NE);
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
    if (target->isExtensible() && !isFixed) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NEW);
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_INVALID);
            return false;
        }
    }

    // step 11
    if (!desc->configurable() && !isFixed) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NE_AS_NC);
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
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(defineProperty), &trap))
        return false;

    // step 4
    if (trap.isUndefined()) {
        AutoPropertyDescriptorRooter desc(cx);
        if (!ParsePropertyDescriptorObject(cx, proxy, vp, &desc))
            return false;
        return JS_DefinePropertyById(cx, target, id, desc.value, desc.getter, desc.setter,
                                     desc.attrs);
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
    if (!Invoke(cx, ObjectValue(*handler), trap, 3, argv, trapResult.address()))
        return false;

    // steps 7-8
    if (ToBoolean(trapResult)) {
        bool isFixed;
        if (!HasOwn(cx, target, id, &isFixed))
            return false;

        if (!target->isExtensible() && !isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_DEFINE_NEW);
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
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_DEFINE_INVALID);
                return false;
            }
        }

        if (!desc->configurable() && !isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_DEFINE_NE_AS_NC);
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
    if (!js_AtomToPrintableString(cx, atom, &bytes))
        return;
    js_ReportValueError2(cx, JSMSG_INVALID_TRAP_RESULT, JSDVG_IGNORE_STACK, v,
                         NullPtr(), bytes.ptr());
}

// This function is shared between getOwnPropertyNames, enumerate, and keys
static bool
ArrayToIdVector(JSContext *cx, HandleObject proxy, HandleObject target, HandleValue v,
                AutoIdVector &props, unsigned flags, JSAtom *trapName)
{
    JS_ASSERT(v.isObject());
    RootedObject array(cx, &v.toObject());

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
        if (!ValueToId(cx, v, id.address()))
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
        if (!target->isExtensible() && !isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NEW);
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SKIP_NC);
            return false;
        }

        // step ii
        bool isFixed;
        if (!HasOwn(cx, target, id, &isFixed))
            return false;

        // step iii
        if (!target->isExtensible() && isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_E_AS_NE);
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

// FIXME: Move to Proxy::getPropertyDescriptor once ScriptedIndirectProxy is removed
bool
ScriptedDirectProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_,
                                                  bool set, PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    Rooted<JSObject*> proxy(cx, proxy_);
    Rooted<jsid> id(cx, id_);
    if (!GetOwnPropertyDescriptor(cx, proxy, id, desc))
        return false;
    if (desc->obj)
        return true;
    JSObject *proto = proxy->getProto();
    if (!proto) {
        JS_ASSERT(!desc->obj);
        return true;
    }
    return JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, desc);
}

bool
ScriptedDirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_,
                                                     bool set, PropertyDescriptor *desc)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);

    // step 1
    RootedValue v(cx);
    if (!TrapGetOwnProperty(cx, proxy, id, &v))
        return false;

    // step 2
    if (v.isUndefined()) {
        desc->obj = NULL;
        return true;
    }

    // steps 3-4
    return ParsePropertyDescriptorObject(cx, proxy, v, desc, true);
}

bool
ScriptedDirectProxyHandler::defineProperty(JSContext *cx, JSObject *proxy_, jsid id_,
                                           PropertyDescriptor *desc)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);

    // step 1
    AutoPropDescArrayRooter descs(cx);
    PropDesc *d = descs.append();
    d->initFromPropertyDescriptor(*desc);
    RootedValue v(cx);
    if (!FromGenericPropertyDescriptor(cx, d, &v))
        return false;

    // step 2
    return TrapDefineOwnProperty(cx, proxy, id, &v);
}

bool
ScriptedDirectProxyHandler::getOwnPropertyNames(JSContext *cx, JSObject *proxy_,
                                                AutoIdVector &props)
{
    RootedObject proxy(cx, proxy_);

    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(getOwnPropertyNames), &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::getOwnPropertyNames(cx, proxy_, props);

    // step e
    Value argv[] = {
        ObjectValue(*target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 1, argv, trapResult.address()))
        return false;

    // step f
    if (trapResult.isPrimitive()) {
        ReportInvalidTrapResult(cx, proxy, ATOM(getOwnPropertyNames));
        return false;
    }

    // steps g to n are shared
    return ArrayToIdVector(cx, proxy, target, trapResult, props, JSITER_OWNONLY | JSITER_HIDDEN,
                           ATOM(getOwnPropertyNames));
}

// Proxy.[[Delete]](P, Throw)
bool
ScriptedDirectProxyHandler::delete_(JSContext *cx, JSObject *proxy_, jsid id_, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(deleteProperty), &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::delete_(cx, proxy_, id_, bp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectValue(*target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 2, argv, trapResult.address()))
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
ScriptedDirectProxyHandler::enumerate(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    RootedObject proxy(cx, proxy_);

    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(enumerate), &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::enumerate(cx, proxy_, props);

    // step e
    Value argv[] = {
        ObjectOrNullValue(target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 1, argv, trapResult.address()))
        return false;

    // step f
    if (trapResult.isPrimitive()) {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, ATOM(enumerate), &bytes))
            return false;
        RootedValue v(cx, ObjectOrNullValue(proxy));
        js_ReportValueError2(cx, JSMSG_INVALID_TRAP_RESULT, JSDVG_SEARCH_STACK,
                             v, NullPtr(), bytes.ptr());
        return false;
    }

    // steps g-m are shared
    // FIXME: the trap should return an iterator object, see bug 783826
    return ArrayToIdVector(cx, proxy, target, trapResult, props, 0, ATOM(enumerate));
}

// Proxy.[[HasProperty]](P)
bool
ScriptedDirectProxyHandler::has(JSContext *cx, JSObject *proxy_, jsid id_, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(has), &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::has(cx, proxy_, id_, bp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 2, argv, trapResult.address()))
        return false;

    // step 6
    bool success = ToBoolean(trapResult);;

    // step 7
    if (!success) {
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        if (!target->isExtensible()) {
            bool isFixed;
            if (!HasOwn(cx, target, id, &isFixed))
                return false;
            if (isFixed) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_E_AS_NE);
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
ScriptedDirectProxyHandler::hasOwn(JSContext *cx, JSObject *proxy_, jsid id_, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(hasOwn), &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::hasOwn(cx, proxy_, id_, bp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 2, argv, trapResult.address()))
        return false;

    // step 6
    bool success = ToBoolean(trapResult);

    // steps 7-8
    if (!success) {
        bool sealed;
        if (!IsSealed(cx, target, id, &sealed))
            return false;
        if (sealed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        if (!target->isExtensible()) {
            bool isFixed;
            if (!HasOwn(cx, target, id, &isFixed))
                return false;
            if (isFixed) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_E_AS_NE);
                return false;
            }
        }
    } else if (!target->isExtensible()) {
        bool isFixed;
        if (!HasOwn(cx, target, id, &isFixed))
            return false;
        if (!isFixed) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REPORT_NEW);
            return false;
        }
    }

    // step 9
    *bp = !!success;
    return true;
}

// Proxy.[[GetP]](P, Receiver)
bool
ScriptedDirectProxyHandler::get(JSContext *cx, JSObject *proxy_, JSObject *receiver_, jsid id_,
                                Value *vp)
{
    RootedObject proxy(cx, proxy_);
    RootedObject receiver(cx, receiver_);
    RootedId id(cx, id_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(get), &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::get(cx, proxy_, receiver_, id_, vp);

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
    if (!Invoke(cx, ObjectValue(*handler), trap, 3, argv, trapResult.address()))
        return false;

    // step 6
    AutoPropertyDescriptorRooter desc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
        return false;

    // step 7
    if (desc.obj) {
        if (IsDataDescriptor(desc) &&
            (desc.attrs & JSPROP_PERMANENT) &&
            (desc.attrs & JSPROP_READONLY))
        {
            bool same;
            if (!SameValue(cx, *vp, desc.value, &same))
                return false;
            if (!same) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MUST_REPORT_SAME_VALUE);
                return false;
            }
        }

        if (IsAccessorDescriptor(desc) &&
            (desc.attrs & JSPROP_PERMANENT) &&
            !(desc.attrs & JSPROP_GETTER))
        {
            if (!trapResult.isUndefined()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MUST_REPORT_UNDEFINED);
                return false;
            }
        }
    }

    // step 8
    *vp = trapResult;
    return true;
}

// Proxy.[[SetP]](P, V, Receiver)
bool
ScriptedDirectProxyHandler::set(JSContext *cx, JSObject *proxy_, JSObject *receiver_, jsid id_,
                                bool strict, Value *vp)
{
    RootedObject proxy(cx, proxy_);
    RootedObject receiver(cx, receiver_);
    RootedId id(cx, id_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step 3
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(set), &trap))
        return false;

    // step 4
    if (trap.isUndefined())
        return DirectProxyHandler::set(cx, proxy_, receiver_, id_, strict, vp);

    // step 5
    RootedValue value(cx);
    if (!IdToValue(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value,
        *vp,
        ObjectValue(*receiver)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 4, argv, trapResult.address()))
        return false;

    // step 6
    bool success = ToBoolean(trapResult);

    // step 7
    if (success) {
        AutoPropertyDescriptorRooter desc(cx);
        if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
            return false;

        if (desc.obj) {
            if (IsDataDescriptor(desc) && (desc.attrs & JSPROP_PERMANENT) &&
                (desc.attrs & JSPROP_READONLY)) {
                bool same;
                if (!SameValue(cx, *vp, desc.value, &same))
                    return false;
                if (!same) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_NW_NC);
                    return false;
                }
            }

            if (IsAccessorDescriptor(desc) && (desc.attrs & JSPROP_PERMANENT)) {
                if (!(desc.attrs & JSPROP_SETTER)) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_WO_SETTER);
                    return false;
                }
            }
        }
    }

    // step 8
    *vp = BooleanValue(success);
    return true;
}

// 15.2.3.14 Object.keys (O), step 2
bool
ScriptedDirectProxyHandler::keys(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    RootedObject proxy(cx, proxy_);

    // step a
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step b
    RootedObject target(cx, GetProxyTargetObject(proxy));

    // step c
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(keys), &trap))
        return false;

    // step d
    if (trap.isUndefined())
        return DirectProxyHandler::keys(cx, proxy_, props);

    // step e
    Value argv[] = {
        ObjectOrNullValue(target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, 1, argv, trapResult.address()))
        return false;

    // step f
    if (trapResult.isPrimitive()) {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, ATOM(keys), &bytes))
            return false;
        RootedValue v(cx, ObjectOrNullValue(proxy));
        js_ReportValueError2(cx, JSMSG_INVALID_TRAP_RESULT, JSDVG_IGNORE_STACK,
                             v, NullPtr(), bytes.ptr());
        return false;
    }

    // steps g-n are shared
    return ArrayToIdVector(cx, proxy, target, trapResult, props, JSITER_OWNONLY, ATOM(keys));
}

bool
ScriptedDirectProxyHandler::iterate(JSContext *cx, JSObject *proxy, unsigned flags, Value *vp)
{
    // FIXME: Provide a proper implementation for this trap, see bug 787004
    return DirectProxyHandler::iterate(cx, proxy, flags, vp);
}

bool
ScriptedDirectProxyHandler::call(JSContext *cx, JSObject *proxy_, unsigned argc, Value *vp)
{
    RootedObject proxy(cx, proxy_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    /*
     * NB: Remember to throw a TypeError here if we change NewProxyObject so that this trap can get
     * called for non-callable objects
     */

    // step 3
    RootedObject args(cx, NewDenseCopiedArray(cx, argc, vp + 2));
    if (!args)
        return false;

    // step 4
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(apply), &trap))
        return false;

    // step 5
    if (trap.isUndefined())
        return DirectProxyHandler::call(cx, proxy_, argc, vp);

    // step 6
    Value argv[] = {
        ObjectValue(*target),
        vp[1],
        ObjectValue(*args)
    };
    RootedValue thisValue(cx, ObjectValue(*handler));
    return Invoke(cx, thisValue, trap, 3, argv, vp);
}

bool
ScriptedDirectProxyHandler::construct(JSContext *cx, JSObject *proxy_, unsigned argc, Value *argv,
                                      Value *rval)
{
    RootedObject proxy(cx, proxy_);

    // step 1
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));

    // step 2
    RootedObject target(cx, GetProxyTargetObject(proxy));

    /*
     * NB: Remember to throw a TypeError here if we change NewProxyObject so that this trap can get
     * called for non-callable objects
     */

    // step 3
    RootedObject args(cx, NewDenseCopiedArray(cx, argc, argv));
    if (!args)
        return false;

    // step 4
    RootedValue trap(cx);
    if (!JSObject::getProperty(cx, handler, handler, ATOM(construct), &trap))
        return false;

    // step 5
    if (trap.isUndefined())
        return DirectProxyHandler::construct(cx, proxy_, argc, argv, rval);

    // step 6
    Value constructArgv[] = {
        ObjectValue(*target),
        ObjectValue(*args)
    };
    RootedValue thisValue(cx, ObjectValue(*handler));
    return Invoke(cx, thisValue, trap, 2, constructArgv, rval);
}

ScriptedDirectProxyHandler ScriptedDirectProxyHandler::singleton;

#define INVOKE_ON_PROTOTYPE(cx, handler, proxy, protoCall)                   \
    JS_BEGIN_MACRO                                                           \
        RootedObject proto(cx);                                              \
        if (!handler->getPrototypeOf(cx, proxy, proto.address()))            \
            return false;                                                    \
        if (!proto)                                                          \
            return true;                                                     \
        assertSameCompartment(cx, proxy, proto);                             \
        return protoCall;                                                    \
    JS_END_MACRO                                                             \


bool
Proxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_, bool set,
                             PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    if (!handler->hasPrototype())
        return handler->getPropertyDescriptor(cx, proxy, id, set, desc);
    if (!handler->getOwnPropertyDescriptor(cx, proxy, id, set, desc))
        return false;
    if (desc->obj)
        return true;
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        JS_GetPropertyDescriptorById(cx, proto, id,
                                                     JSRESOLVE_QUALIFIED, desc));
}

bool
Proxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id, bool set, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    AutoPropertyDescriptorRooter desc(cx);
    return Proxy::getPropertyDescriptor(cx, proxy, id, set, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id, bool set,
                                PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->getOwnPropertyDescriptor(cx, proxy, id, set, desc);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id, bool set, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    AutoPropertyDescriptorRooter desc(cx);
    return Proxy::getOwnPropertyDescriptor(cx, proxy, id, set, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

bool
Proxy::defineProperty(JSContext *cx, JSObject *proxy_, jsid id, PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->defineProperty(cx, proxy, id, desc);
}

bool
Proxy::defineProperty(JSContext *cx, JSObject *proxy_, jsid id_, const Value &v)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);
    AutoPropertyDescriptorRooter desc(cx);
    return ParsePropertyDescriptorObject(cx, proxy, v, &desc) &&
           Proxy::defineProperty(cx, proxy, id, &desc);
}

bool
Proxy::getOwnPropertyNames(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->getOwnPropertyNames(cx, proxy, props);
}

bool
Proxy::delete_(JSContext *cx, JSObject *proxy_, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->delete_(cx, proxy, id, bp);
}

static bool
AppendUnique(JSContext *cx, AutoIdVector &base, AutoIdVector &others)
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
    return base.append(uniqueOthers);
}

bool
Proxy::enumerate(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    if (!handler->hasPrototype())
        return GetProxyHandler(proxy)->enumerate(cx, proxy, props);
    if (!handler->keys(cx, proxy, props))
        return false;
    AutoIdVector protoProps(cx);
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        GetPropertyNames(cx, proto, 0, &protoProps) &&
                        AppendUnique(cx, props, protoProps));
}

bool
Proxy::has(JSContext *cx, JSObject *proxy_, jsid id_, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    RootedId id(cx, id_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    if (!handler->hasPrototype())
        return handler->has(cx, proxy, id, bp);
    if (!handler->hasOwn(cx, proxy, id, bp))
        return false;
    if (*bp)
        return true;
    JSBool Bp;
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        JS_HasPropertyById(cx, proto, id, &Bp) &&
                        ((*bp = Bp) || true));
}

bool
Proxy::hasOwn(JSContext *cx, JSObject *proxy_, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->hasOwn(cx, proxy, id, bp);
}

bool
Proxy::get(JSContext *cx, HandleObject proxy_, HandleObject receiver, HandleId id,
           MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    bool own = false;
    if (!handler->hasPrototype() || (handler->hasOwn(cx, proxy, id, &own) && own))
        return handler->get(cx, proxy, receiver, id, vp.address());
    INVOKE_ON_PROTOTYPE(cx, handler, proxy, JSObject::getGeneric(cx, proto, receiver, id, vp));
}

bool
Proxy::getElementIfPresent(JSContext *cx, HandleObject proxy_, HandleObject receiver, uint32_t index,
                           MutableHandleValue vp, bool *present)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    bool hasOwn, status = true;
    if (!handler->hasPrototype() ||
        ((status = handler->hasOwn(cx, proxy, INT_TO_JSID(index), &hasOwn)) && hasOwn))
    {
        return GetProxyHandler(proxy)->getElementIfPresent(cx, proxy, receiver,
                                                           index, vp.address(), present);
    } else if (!status) {
        return false;
    }
    INVOKE_ON_PROTOTYPE(cx, handler, proxy,
                        JSObject::getElementIfPresent(cx, proto, receiver, index, vp, present));
}

bool
Proxy::set(JSContext *cx, HandleObject proxy_, HandleObject receiver, HandleId id, bool strict,
           MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    RootedObject proto(cx);
    if (handler->hasPrototype()) {
        // If we're using a prototype, we still want to use the proxy trap unless
        // we have a non-own property with a setter.
        bool hasOwn;
        AutoPropertyDescriptorRooter desc(cx);
        if (handler->hasOwn(cx, proxy, id, &hasOwn) && !hasOwn &&
            handler->getPrototypeOf(cx, proxy, proto.address()) && proto &&
            JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, &desc) &&
            desc.obj && desc.setter)
        {
            return JSObject::setGeneric(cx, proto, receiver, id, vp, strict);
        } else if (cx->isExceptionPending()) {
            return false;
        }
    }
    return handler->set(cx, proxy, receiver, id, strict, vp.address());
}

bool
Proxy::keys(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->keys(cx, proxy, props);
}

bool
Proxy::iterate(JSContext *cx, HandleObject proxy_, unsigned flags, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    BaseProxyHandler *handler = GetProxyHandler(proxy);
    if (!handler->hasPrototype())
        return GetProxyHandler(proxy)->iterate(cx, proxy, flags, vp.address());
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
Proxy::call(JSContext *cx, JSObject *proxy_, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->call(cx, proxy, argc, vp);
}

bool
Proxy::construct(JSContext *cx, JSObject *proxy_, unsigned argc, Value *argv, Value *rval)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->construct(cx, proxy, argc, argv, rval);
}

bool
Proxy::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args)
{
    JS_CHECK_RECURSION(cx, return false);
    Rooted<JSObject*> proxy(cx, &args.thisv().toObject());
    return GetProxyHandler(proxy)->nativeCall(cx, test, impl, args);
}

bool
Proxy::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    return GetProxyHandler(proxy)->hasInstance(cx, proxy, v, bp);
}

JSType
Proxy::typeOf(JSContext *cx, JSObject *proxy_)
{
    // FIXME: API doesn't allow us to report error (bug 618906).
    JS_CHECK_RECURSION(cx, return JSTYPE_OBJECT);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->typeOf(cx, proxy);
}

bool
Proxy::objectClassIs(JSObject *proxy_, ESClassValue classValue, JSContext *cx)
{
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->objectClassIs(proxy, classValue, cx);
}

JSString *
Proxy::obj_toString(JSContext *cx, JSObject *proxy_)
{
    JS_CHECK_RECURSION(cx, return NULL);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->obj_toString(cx, proxy);
}

JSString *
Proxy::fun_toString(JSContext *cx, JSObject *proxy_, unsigned indent)
{
    JS_CHECK_RECURSION(cx, return NULL);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->fun_toString(cx, proxy, indent);
}

bool
Proxy::regexp_toShared(JSContext *cx, JSObject *proxy_, RegExpGuard *g)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->regexp_toShared(cx, proxy, g);
}

bool
Proxy::defaultValue(JSContext *cx, JSObject *proxy_, JSType hint, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->defaultValue(cx, proxy, hint, vp);
}

bool
Proxy::iteratorNext(JSContext *cx, JSObject *proxy_, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    RootedObject proxy(cx, proxy_);
    return GetProxyHandler(proxy)->iteratorNext(cx, proxy, vp);
}

static JSObject *
proxy_innerObject(JSContext *cx, HandleObject obj)
{
    return GetProxyPrivate(obj).toObjectOrNull();
}

static JSBool
proxy_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    bool found;
    if (!Proxy::has(cx, obj, id, &found))
        return false;

    if (found) {
        MarkNonNativePropertyFound(obj, propp);
        objp.set(obj);
    } else {
        objp.set(NULL);
        propp.set(NULL);
    }
    return true;
}

static JSBool
proxy_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                     MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
proxy_LookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
proxy_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
proxy_DefineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoPropertyDescriptorRooter desc(cx);
    desc.obj = obj;
    desc.value = value;
    desc.attrs = (attrs & (~JSPROP_SHORTID));
    desc.getter = getter;
    desc.setter = setter;
    desc.shortid = 0;
    return Proxy::defineProperty(cx, obj, id, &desc);
}

static JSBool
proxy_DefineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
proxy_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
proxy_DefineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
proxy_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                 MutableHandleValue vp)
{
    return Proxy::get(cx, obj, receiver, id, vp);
}

static JSBool
proxy_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                  MutableHandleValue vp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
proxy_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                 MutableHandleValue vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
proxy_GetElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                          MutableHandleValue vp, bool *present)
{
    return Proxy::getElementIfPresent(cx, obj, receiver, index, vp, present);
}

static JSBool
proxy_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                 MutableHandleValue vp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
proxy_SetGeneric(JSContext *cx, HandleObject obj, HandleId id,
                 MutableHandleValue vp, JSBool strict)
{
    return Proxy::set(cx, obj, obj, id, strict, vp);
}

static JSBool
proxy_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                  MutableHandleValue vp, JSBool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static JSBool
proxy_SetElement(JSContext *cx, HandleObject obj, uint32_t index,
                 MutableHandleValue vp, JSBool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static JSBool
proxy_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                 MutableHandleValue vp, JSBool strict)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static JSBool
proxy_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    AutoPropertyDescriptorRooter desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, obj, id, false, &desc))
        return false;
    *attrsp = desc.attrs;
    return true;
}

static JSBool
proxy_GetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_GetGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
proxy_GetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_GetGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
proxy_GetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_GetGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
proxy_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    /* Lookup the current property descriptor so we have setter/getter/value. */
    AutoPropertyDescriptorRooter desc(cx);
    if (!Proxy::getOwnPropertyDescriptor(cx, obj, id, true, &desc))
        return false;
    desc.attrs = (*attrsp & (~JSPROP_SHORTID));
    return Proxy::defineProperty(cx, obj, id, &desc);
}

static JSBool
proxy_SetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_SetGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
proxy_SetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_SetGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
proxy_SetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_SetGenericAttributes(cx, obj, id, attrsp);
}

static JSBool
proxy_DeleteGeneric(JSContext *cx, HandleObject obj, HandleId id,
                    MutableHandleValue rval, JSBool strict)
{
    // TODO: throwing away strict
    bool deleted;
    if (!Proxy::delete_(cx, obj, id, &deleted) || !js_SuppressDeletedProperty(cx, obj, id))
        return false;
    rval.setBoolean(deleted);
    return true;
}

static JSBool
proxy_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                     MutableHandleValue rval, JSBool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DeleteGeneric(cx, obj, id, rval, strict);
}

static JSBool
proxy_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                    MutableHandleValue rval, JSBool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_DeleteGeneric(cx, obj, id, rval, strict);
}

static JSBool
proxy_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                    MutableHandleValue rval, JSBool strict)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_DeleteGeneric(cx, obj, id, rval, strict);
}

static void
proxy_TraceObject(JSTracer *trc, JSObject *obj)
{
#ifdef DEBUG
    if (!trc->runtime->gcDisableStrictProxyCheckingCount && obj->isWrapper()) {
        JSObject *referent = &GetProxyPrivate(obj).toObject();
        if (referent->compartment() != obj->compartment()) {
            /*
             * Assert that this proxy is tracked in the wrapper map. We maintain
             * the invariant that the wrapped object is the key in the wrapper map.
             */
            Value key = ObjectValue(*referent);
            WrapperMap::Ptr p = obj->compartment()->crossCompartmentWrappers.lookup(key);
            JS_ASSERT(*p->value.unsafeGet() == ObjectValue(*obj));
        }
    }
#endif

    // NB: If you add new slots here, make sure to change
    // js::NukeChromeCrossCompartmentWrappers to cope.
    MarkCrossCompartmentSlot(trc, &obj->getReservedSlotRef(JSSLOT_PROXY_PRIVATE), "private");
    MarkSlot(trc, &obj->getReservedSlotRef(JSSLOT_PROXY_EXTRA + 0), "extra0");
    MarkSlot(trc, &obj->getReservedSlotRef(JSSLOT_PROXY_EXTRA + 1), "extra1");
}

static void
proxy_TraceFunction(JSTracer *trc, JSObject *obj)
{
    // NB: If you add new slots here, make sure to change
    // js::NukeChromeCrossCompartmentWrappers to cope.
    MarkCrossCompartmentSlot(trc, &GetCall(obj), "call");
    MarkSlot(trc, &GetFunctionProxyConstruct(obj), "construct");
    proxy_TraceObject(trc, obj);
}

static JSBool
proxy_Convert(JSContext *cx, HandleObject proxy, JSType hint, MutableHandleValue vp)
{
    JS_ASSERT(proxy->isProxy());
    return Proxy::defaultValue(cx, proxy, hint, vp.address());
}

static void
proxy_Finalize(FreeOp *fop, JSObject *obj)
{
    JS_ASSERT(obj->isProxy());
    if (!obj->getSlot(JSSLOT_PROXY_HANDLER).isUndefined())
        GetProxyHandler(obj)->finalize(fop, obj);
}

static JSBool
proxy_HasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v, JSBool *bp)
{
    bool b;
    if (!Proxy::hasInstance(cx, proxy, v, &b))
        return false;
    *bp = !!b;
    return true;
}

static JSType
proxy_TypeOf(JSContext *cx, HandleObject proxy)
{
    JS_ASSERT(proxy->isProxy());
    return Proxy::typeOf(cx, proxy);
}

JS_FRIEND_DATA(Class) js::ObjectProxyClass = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(4),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    proxy_Convert,
    proxy_Finalize,          /* finalize    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    proxy_HasInstance,       /* hasInstance */
    NULL,                    /* construct   */
    proxy_TraceObject,       /* trace       */
    JS_NULL_CLASS_EXT,
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
        proxy_GetPropertyAttributes,
        proxy_GetElementAttributes,
        proxy_GetSpecialAttributes,
        proxy_SetGenericAttributes,
        proxy_SetPropertyAttributes,
        proxy_SetElementAttributes,
        proxy_SetSpecialAttributes,
        proxy_DeleteProperty,
        proxy_DeleteElement,
        proxy_DeleteSpecial,
        NULL,                /* enumerate       */
        proxy_TypeOf,
        NULL,                /* thisObject      */
    }
};

JS_FRIEND_DATA(Class) js::OuterWindowProxyClass = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(4),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    proxy_Finalize,          /* finalize    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    proxy_TraceObject,       /* trace       */
    {
        NULL,                /* equality    */
        NULL,                /* outerObject */
        proxy_innerObject,
        NULL                 /* unused */
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
        proxy_GetPropertyAttributes,
        proxy_GetElementAttributes,
        proxy_GetSpecialAttributes,
        proxy_SetGenericAttributes,
        proxy_SetPropertyAttributes,
        proxy_SetElementAttributes,
        proxy_SetSpecialAttributes,
        proxy_DeleteProperty,
        proxy_DeleteElement,
        proxy_DeleteSpecial,
        NULL,                /* enumerate       */
        NULL,                /* typeof          */
        NULL,                /* thisObject      */
    }
};

static JSBool
proxy_Call(JSContext *cx, unsigned argc, Value *vp)
{
    JSObject *proxy = &JS_CALLEE(cx, vp).toObject();
    JS_ASSERT(proxy->isProxy());
    return Proxy::call(cx, proxy, argc, vp);
}

static JSBool
proxy_Construct(JSContext *cx, unsigned argc, Value *vp)
{
    JSObject *proxy = &JS_CALLEE(cx, vp).toObject();
    JS_ASSERT(proxy->isProxy());
    bool ok = Proxy::construct(cx, proxy, argc, JS_ARGV(cx, vp), vp);
    return ok;
}

JS_FRIEND_DATA(Class) js::FunctionProxyClass = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(6),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    proxy_Finalize,          /* finalize */
    NULL,                    /* checkAccess */
    proxy_Call,
    FunctionClass.hasInstance,
    proxy_Construct,
    proxy_TraceFunction,     /* trace       */
    JS_NULL_CLASS_EXT,
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
        proxy_GetPropertyAttributes,
        proxy_GetElementAttributes,
        proxy_GetSpecialAttributes,
        proxy_SetGenericAttributes,
        proxy_SetPropertyAttributes,
        proxy_SetElementAttributes,
        proxy_SetSpecialAttributes,
        proxy_DeleteProperty,
        proxy_DeleteElement,
        proxy_DeleteSpecial,
        NULL,                /* enumerate       */
        proxy_TypeOf,
        NULL,                /* thisObject      */
    }
};

JS_FRIEND_API(JSObject *)
js::NewProxyObject(JSContext *cx, BaseProxyHandler *handler, const Value &priv_, JSObject *proto_,
                   JSObject *parent_, JSObject *call_, JSObject *construct_)
{
    RootedValue priv(cx, priv_);
    RootedObject proto(cx, proto_), parent(cx, parent_), call(cx, call_), construct(cx, construct_);

    JS_ASSERT_IF(proto, cx->compartment == proto->compartment());
    JS_ASSERT_IF(parent, cx->compartment == parent->compartment());
    JS_ASSERT_IF(construct, cx->compartment == construct->compartment());
    JS_ASSERT_IF(call && cx->compartment != call->compartment(), priv.get() == ObjectValue(*call));
    bool fun = call || construct;
    Class *clasp;
    if (fun)
        clasp = &FunctionProxyClass;
    else
        clasp = handler->isOuterWindow() ? &OuterWindowProxyClass : &ObjectProxyClass;

    /*
     * Eagerly mark properties unknown for proxies, so we don't try to track
     * their properties and so that we don't need to walk the compartment if
     * their prototype changes later.
     */
    if (proto && !proto->setNewTypeUnknown(cx))
        return NULL;

    RootedObject obj(cx, NewObjectWithGivenProto(cx, clasp, proto, parent));
    if (!obj)
        return NULL;
    obj->initSlot(JSSLOT_PROXY_HANDLER, PrivateValue(handler));
    obj->initCrossCompartmentSlot(JSSLOT_PROXY_PRIVATE, priv);
    if (fun) {
        obj->initCrossCompartmentSlot(JSSLOT_PROXY_CALL, call ? ObjectValue(*call) : UndefinedValue());
        if (construct) {
            obj->initSlot(JSSLOT_PROXY_CONSTRUCT, ObjectValue(*construct));
        }
    }

    /* Don't track types of properties of proxies. */
    MarkTypeObjectUnknownProperties(cx, obj->type());

    /* Mark the new proxy as having singleton type. */
    if (clasp == &OuterWindowProxyClass && !JSObject::setSingletonType(cx, obj))
        return NULL;

    return obj;
}

static JSBool
proxy(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Proxy", "1", "s");
        return false;
    }
    JSObject *target = NonNullObject(cx, args[0]);
    if (!target)
        return false;
    JSObject *handler = NonNullObject(cx, args[1]);
    if (!handler)
        return false;
    JSObject *proto = target->getProto();
    JSObject *fun = target->isCallable() ? target : NULL;
    JSObject *proxy = NewProxyObject(cx, &ScriptedDirectProxyHandler::singleton,
                                     ObjectValue(*target), proto, proto->getParent(),
                                     fun, fun);
    if (!proxy)
        return false;
    SetProxyExtra(proxy, 0, ObjectOrNullValue(handler));
    vp->setObject(*proxy);
    return true;
}

Class js::ProxyClass = {
    "Proxy",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize */
    NULL,                    /* checkAccess */
    NULL,                    /* call */
    NULL,                    /* hasInstance */
    proxy                    /* construct */
};

static JSBool
proxy_create(JSContext *cx, unsigned argc, Value *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "create", "0", "s");
        return false;
    }
    JSObject *handler = NonNullObject(cx, vp[2]);
    if (!handler)
        return false;
    JSObject *proto, *parent = NULL;
    if (argc > 1 && vp[3].isObject()) {
        proto = &vp[3].toObject();
        parent = proto->getParent();
    } else {
        JS_ASSERT(IsFunctionObject(vp[0]));
        proto = NULL;
    }
    if (!parent)
        parent = vp[0].toObject().getParent();
    JSObject *proxy = NewProxyObject(cx, &ScriptedIndirectProxyHandler::singleton,
                                     ObjectValue(*handler), proto, parent);
    if (!proxy)
        return false;

    vp->setObject(*proxy);
    return true;
}

static JSBool
proxy_createFunction(JSContext *cx, unsigned argc, Value *vp)
{
    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "createFunction", "1", "");
        return false;
    }
    JSObject *handler = NonNullObject(cx, vp[2]);
    if (!handler)
        return false;
    JSObject *proto, *parent;
    parent = vp[0].toObject().getParent();
    proto = parent->global().getOrCreateFunctionPrototype(cx);
    if (!proto)
        return false;
    parent = proto->getParent();

    JSObject *call = ValueToCallable(cx, &vp[3]);
    if (!call)
        return false;
    JSObject *construct = NULL;
    if (argc > 2) {
        construct = ValueToCallable(cx, &vp[4]);
        if (!construct)
            return false;
    }

    JSObject *proxy = NewProxyObject(cx, &ScriptedIndirectProxyHandler::singleton,
                                     ObjectValue(*handler), proto, parent, call, construct);
    if (!proxy)
        return false;

    vp->setObject(*proxy);
    return true;
}

static JSFunctionSpec static_methods[] = {
    JS_FN("create",         proxy_create,          2, 0),
    JS_FN("createFunction", proxy_createFunction,  3, 0),
    JS_FS_END
};

JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, JSObject *obj_)
{
    RootedObject obj(cx, obj_);
    RootedObject module(cx, NewObjectWithClassProto(cx, &ProxyClass, NULL, obj));
    if (!module || !JSObject::setSingletonType(cx, module))
        return NULL;

    if (!JS_DefineProperty(cx, obj, "Proxy", OBJECT_TO_JSVAL(module),
                           JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }
    if (!JS_DefineFunctions(cx, module, static_methods))
        return NULL;

    MarkStandardClassInitializedNoProto(obj, &ProxyClass);

    return module;
}
