/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsprvtd.h"
#include "jsnum.h"
#include "jsobjinlines.h"
#include "jsproxy.h"
#include "jsscope.h"

#include "gc/Marking.h"
#include "vm/MethodGuard.h"
#include "vm/RegExpObject-inl.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

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

#ifdef DEBUG
static bool
OperationInProgress(JSContext *cx, JSObject *proxy)
{
    PendingProxyOperation *op = cx->runtime->pendingProxyOperation;
    while (op) {
        if (op->object == proxy)
            return true;
        op = op->next;
    }
    return false;
}
#endif

BaseProxyHandler::BaseProxyHandler(void *family) : mFamily(family)
{
}

BaseProxyHandler::~BaseProxyHandler()
{
}

bool
BaseProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
BaseProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
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

    JS_ASSERT(OperationInProgress(cx, proxy));
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
    return CallJSPropertyOp(cx, desc.getter, receiver, id, vp);
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
BaseProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiver_, jsid id_, bool strict,
                      Value *vp)
{
    RootedObject receiver(cx, receiver_);
    RootedId id(cx, id_);

    JS_ASSERT(OperationInProgress(cx, proxy));
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
            if (!CallSetter(cx, receiver, id, desc.setter, desc.attrs, desc.shortid, strict, vp))
                return false;
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
            if (!CallSetter(cx, receiver, id, desc.setter, desc.attrs, desc.shortid, strict, vp))
                return false;
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
    JS_ASSERT(OperationInProgress(cx, proxy));
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

    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoIdVector props(cx);
    if ((flags & JSITER_OWNONLY)
        ? !keys(cx, proxy, props)
        : !enumerate(cx, proxy, props)) {
        return false;
    }
    return EnumeratedIdVectorToIterator(cx, proxy, flags, props, vp);
}

bool
BaseProxyHandler::call(JSContext *cx, JSObject *proxy, unsigned argc,
                       Value *vp)
{
    Value v = UndefinedValue();
    js_ReportIsNotFunction(cx, &v, 0);
    return false;
}

bool
BaseProxyHandler::construct(JSContext *cx, JSObject *proxy, unsigned argc,
                            Value *argv, Value *rval)
{
    Value v = UndefinedValue();
    js_ReportIsNotFunction(cx, &v, JSV2F_CONSTRUCT);
    return false;
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
    return DefaultValue(cx, obj, hint, vp);
}

bool
BaseProxyHandler::iteratorNext(JSContext *cx, JSObject *proxy, Value *vp)
{
    vp->setMagic(JS_NO_ITER_VALUE);
    return true;
}

bool
BaseProxyHandler::nativeCall(JSContext *cx, JSObject *proxy, Class *clasp, Native native, CallArgs args)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    ReportIncompatibleMethod(cx, args, clasp);
    return false;
}

bool
BaseProxyHandler::hasInstance(JSContext *cx, JSObject *proxy, const Value *vp, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, ObjectValue(*proxy), NULL);
    return false;
}

JSType
BaseProxyHandler::typeOf(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    return IsFunctionProxy(proxy) ? JSTYPE_FUNCTION : JSTYPE_OBJECT;
}

bool
BaseProxyHandler::objectClassIs(JSObject *proxy, ESClassValue classValue, JSContext *cx)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    return false;
}

void
BaseProxyHandler::finalize(JSFreeOp *fop, JSObject *proxy)
{
}

IndirectProxyHandler::IndirectProxyHandler(void *family) : BaseProxyHandler(family)
{
}

bool
IndirectProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy,
                                            jsid id, bool set,
                                            PropertyDescriptor *desc)
{
    return JS_GetPropertyDescriptorById(cx, GetProxyTargetObject(proxy), id,
                                        JSRESOLVE_QUALIFIED, desc);
}

static bool
GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, unsigned flags,
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
    return GetOwnPropertyDescriptor(cx, GetProxyTargetObject(proxy), id,
                                    JSRESOLVE_QUALIFIED, desc);
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
    return GetPropertyNames(cx, GetProxyTargetObject(proxy),
                            JSITER_OWNONLY | JSITER_HIDDEN, &props);
}

bool
IndirectProxyHandler::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    Value v;
    if (!JS_DeletePropertyById2(cx, GetProxyTargetObject(proxy), id, &v))
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
    return GetPropertyNames(cx, GetProxyTargetObject(proxy), 0, &props);
}

bool
IndirectProxyHandler::call(JSContext *cx, JSObject *proxy, unsigned argc,
                   Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
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
    JS_ASSERT(OperationInProgress(cx, proxy));
    Value fval = GetConstruct(proxy);
    if (fval.isUndefined())
        return InvokeConstructor(cx, GetCall(proxy), argc, argv, rval);
    return Invoke(cx, UndefinedValue(), fval, argc, argv, rval);
}

bool
IndirectProxyHandler::nativeCall(JSContext *cx, JSObject *proxy, Class *clasp,
                                 Native native, CallArgs args)
{
    args.thisv() = ObjectValue(*GetProxyTargetObject(proxy));
    return CallJSNative(cx, native, args);
}

bool
IndirectProxyHandler::hasInstance(JSContext *cx, JSObject *proxy, const Value *vp,
                                  bool *bp)
{
    JSBool b;
    if (!JS_HasInstance(cx, GetProxyTargetObject(proxy), *vp, &b))
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
    if (!js_IteratorMore(cx, target, vp))
        return false;
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
    if (!JS_HasPropertyById(cx, GetProxyTargetObject(proxy), id, &found))
        return false;
    *bp = !!found;
    return true;
}

bool
DirectProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *target = GetProxyTargetObject(proxy);
    PropertyDescriptor desc;
    if (!JS_GetPropertyDescriptorById(cx, target, id, JSRESOLVE_QUALIFIED,
                                      &desc))
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
    return GetProxyTargetObject(proxy)->getGeneric(cx, receiver, id, vp);
}

bool
DirectProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiver,
                        jsid id_, bool strict, Value *vp)
{
    RootedId id(cx, id_);
    return GetProxyTargetObject(proxy)->setGeneric(cx, id, vp, strict);
}

bool
DirectProxyHandler::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return GetPropertyNames(cx, GetProxyTargetObject(proxy), JSITER_OWNONLY,
                            &props);
}

bool
DirectProxyHandler::iterate(JSContext *cx, JSObject *proxy, unsigned flags,
                            Value *vp)
{
    Rooted<JSObject*> target(cx, GetProxyTargetObject(proxy));
    return GetIterator(cx, target, flags, vp);
}

static bool
GetTrap(JSContext *cx, JSObject *handler, PropertyName *name, Value *fvalp)
{
    JS_CHECK_RECURSION(cx, return false);

    Rooted<PropertyName*> propname(cx, name);
    return handler->getProperty(cx, propname, fvalp);
}

static bool
GetFundamentalTrap(JSContext *cx, JSObject *handler, PropertyName *name, Value *fvalp)
{
    if (!GetTrap(cx, handler, name, fvalp))
        return false;

    if (!js_IsCallable(*fvalp)) {
        JSAutoByteString bytes;
        if (js_AtomToPrintableString(cx, name, &bytes))
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_FUNCTION, bytes.ptr());
        return false;
    }

    return true;
}

static bool
GetDerivedTrap(JSContext *cx, JSObject *handler, PropertyName *name, Value *fvalp)
{
    JS_ASSERT(name == ATOM(has) ||
              name == ATOM(hasOwn) ||
              name == ATOM(get) ||
              name == ATOM(set) ||
              name == ATOM(keys) ||
              name == ATOM(iterate));

    return GetTrap(cx, handler, name, fvalp);
}

static bool
Trap(JSContext *cx, HandleObject handler, HandleValue fval, unsigned argc, Value* argv, Value *rval)
{
    return Invoke(cx, ObjectValue(*handler), fval, argc, argv, rval);
}

static bool
Trap1(JSContext *cx, HandleObject handler, HandleValue fval, jsid id, Value *rval)
{
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    rval->setString(str);
    return Trap(cx, handler, fval, 1, rval, rval);
}

static bool
Trap2(JSContext *cx, HandleObject handler, HandleValue fval, jsid id, Value v, Value *rval)
{
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    rval->setString(str);
    Value argv[2] = { *rval, v };
    return Trap(cx, handler, fval, 2, argv, rval);
}

static bool
ParsePropertyDescriptorObject(JSContext *cx, JSObject *obj, jsid id, const Value &v,
                              PropertyDescriptor *desc)
{
    AutoPropDescArrayRooter descs(cx);
    PropDesc *d = descs.append();
    if (!d || !d->initialize(cx, v))
        return false;
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
    *bp = !!js_ValueToBoolean(v);
    return true;
}

static bool
ArrayToIdVector(JSContext *cx, const Value &array, AutoIdVector &props)
{
    JS_ASSERT(props.length() == 0);

    if (array.isPrimitive())
        return true;

    JSObject *obj = &array.toObject();
    uint32_t length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return false;

    for (uint32_t n = 0; n < length; ++n) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;
        Value v;
        if (!obj->getElement(cx, n, &v))
            return false;
        jsid id;
        if (!ValueToId(cx, v, &id))
            return false;
        if (!props.append(id))
            return false;
    }

    return true;
}

/* Derived class for all scripted proxy handlers. */
class ScriptedProxyHandler : public IndirectProxyHandler {
  public:
    ScriptedProxyHandler();
    virtual ~ScriptedProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                       PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                          PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props);

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp);
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                     Value *vp);
    virtual bool keys(JSContext *cx, JSObject *proxy, AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *proxy, unsigned flags, Value *vp);

    virtual JSType typeOf(JSContext *cx, JSObject *proxy);
    virtual bool defaultValue(JSContext *cx, JSObject *obj, JSType hint, Value *vp);

    static ScriptedProxyHandler singleton;
};

static int sScriptedProxyHandlerFamily = 0;

ScriptedProxyHandler::ScriptedProxyHandler() : IndirectProxyHandler(&sScriptedProxyHandlerFamily)
{
}

ScriptedProxyHandler::~ScriptedProxyHandler()
{
}

static bool
ReturnedValueMustNotBePrimitive(JSContext *cx, JSObject *proxy, JSAtom *atom, const Value &v)
{
    if (v.isPrimitive()) {
        JSAutoByteString bytes;
        if (js_AtomToPrintableString(cx, atom, &bytes)) {
            js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                                 JSDVG_SEARCH_STACK, ObjectOrNullValue(proxy), NULL, bytes.ptr());
        }
        return false;
    }
    return true;
}

static JSObject *
GetProxyHandlerObject(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    return GetProxyPrivate(proxy).toObjectOrNull();
}

bool
ScriptedProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_, bool set,
                                            PropertyDescriptor *desc)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getPropertyDescriptor), fval.address()) &&
           Trap1(cx, handler, fval, id, value.address()) &&
           ((value.reference().isUndefined() && IndicatePropertyNotFound(cx, desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), value) &&
             ParsePropertyDescriptorObject(cx, proxy, id, value, desc)));
}

bool
ScriptedProxyHandler::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy_, jsid id_, bool set,
                                               PropertyDescriptor *desc)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getOwnPropertyDescriptor), fval.address()) &&
           Trap1(cx, handler, fval, id, value.address()) &&
           ((value.reference().isUndefined() && IndicatePropertyNotFound(cx, desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), value) &&
             ParsePropertyDescriptorObject(cx, proxy, id, value, desc)));
}

bool
ScriptedProxyHandler::defineProperty(JSContext *cx, JSObject *proxy, jsid id_,
                                     PropertyDescriptor *desc)
{
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    RootedId id(cx, id_);
    return GetFundamentalTrap(cx, handler, ATOM(defineProperty), fval.address()) &&
           NewPropertyDescriptorObject(cx, desc, value.address()) &&
           Trap2(cx, handler, fval, id, value, value.address());
}

bool
ScriptedProxyHandler::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getOwnPropertyNames), fval.address()) &&
           Trap(cx, handler, fval, 0, NULL, value.address()) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedProxyHandler::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(delete), fval.address()) &&
           Trap1(cx, handler, fval, id, value.address()) &&
           ValueToBool(cx, value, bp);
}

bool
ScriptedProxyHandler::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    return GetFundamentalTrap(cx, handler, ATOM(enumerate), fval.address()) &&
           Trap(cx, handler, fval, 0, NULL, value.address()) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedProxyHandler::has(JSContext *cx, JSObject *proxy_, jsid id, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(has), fval.address()))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::has(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, value.address()) &&
           ValueToBool(cx, value, bp);
}

bool
ScriptedProxyHandler::hasOwn(JSContext *cx, JSObject *proxy_, jsid id, bool *bp)
{
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue fval(cx), value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(hasOwn), fval.address()))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
    return Trap1(cx, handler, fval, id, value.address()) &&
           ValueToBool(cx, value, bp);
}

bool
ScriptedProxyHandler::get(JSContext *cx, JSObject *proxy_, JSObject *receiver, jsid id_, Value *vp)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    RootedValue value(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), value };
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(get), fval.address()))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::get(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval, 2, argv, vp);
}

bool
ScriptedProxyHandler::set(JSContext *cx, JSObject *proxy_, JSObject *receiver, jsid id_, bool strict,
                          Value *vp)
{
    RootedId id(cx, id_);
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    JSString *str = ToString(cx, IdToValue(id));
    if (!str)
        return false;
    RootedValue value(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), value, *vp };
    RootedValue fval(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(set), fval.address()))
        return false;
    if (!js_IsCallable(fval))
        return BaseProxyHandler::set(cx, proxy, receiver, id, strict, vp);
    return Trap(cx, handler, fval, 3, argv, value.address());
}

bool
ScriptedProxyHandler::keys(JSContext *cx, JSObject *proxy_, AutoIdVector &props)
{
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(keys), value.address()))
        return false;
    if (!js_IsCallable(value))
        return BaseProxyHandler::keys(cx, proxy, props);
    return Trap(cx, handler, value, 0, NULL, value.address()) &&
           ArrayToIdVector(cx, value, props);
}

bool
ScriptedProxyHandler::iterate(JSContext *cx, JSObject *proxy_, unsigned flags, Value *vp)
{
    RootedObject proxy(cx, proxy_);
    RootedObject handler(cx, GetProxyHandlerObject(cx, proxy));
    RootedValue value(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(iterate), value.address()))
        return false;
    if (!js_IsCallable(value))
        return BaseProxyHandler::iterate(cx, proxy, flags, vp);
    return Trap(cx, handler, value, 0, NULL, vp) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(iterate), *vp);
}

JSType
ScriptedProxyHandler::typeOf(JSContext *cx, JSObject *proxy)
{
    /*
     * This function is only here to prevent a regression in
     * js1_8_5/extensions/scripted-proxies.js. It will be removed when the
     * direct proxy refactor is complete.
     */
    return BaseProxyHandler::typeOf(cx, proxy);
}

bool
ScriptedProxyHandler::defaultValue(JSContext *cx, JSObject *proxy, JSType hint,
                                   Value *vp)
{
    /*
     * This function is only here to prevent bug 757063. It will be removed when
     * the direct proxy refactor is complete.
     */
    return BaseProxyHandler::defaultValue(cx, proxy, hint, vp);
}

ScriptedProxyHandler ScriptedProxyHandler::singleton;

class AutoPendingProxyOperation {
    JSRuntime               *rt;
    PendingProxyOperation   op;
  public:
    AutoPendingProxyOperation(JSContext *cx, JSObject *proxy)
        : rt(cx->runtime), op(cx, proxy)
    {
        op.next = rt->pendingProxyOperation;
        rt->pendingProxyOperation = &op;
    }

    ~AutoPendingProxyOperation() {
        JS_ASSERT(rt->pendingProxyOperation == &op);
        rt->pendingProxyOperation = op.next;
    }
};

bool
Proxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                             PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->getPropertyDescriptor(cx, proxy, id, set, desc);
}

bool
Proxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return Proxy::getPropertyDescriptor(cx, proxy, id, set, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->getOwnPropertyDescriptor(cx, proxy, id, set, desc);
}

bool
Proxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return Proxy::getOwnPropertyDescriptor(cx, proxy, id, set, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

bool
Proxy::defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->defineProperty(cx, proxy, id, desc);
}

bool
Proxy::defineProperty(JSContext *cx, JSObject *proxy, jsid id, const Value &v)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return ParsePropertyDescriptorObject(cx, proxy, id, v, &desc) &&
           Proxy::defineProperty(cx, proxy, id, &desc);
}

bool
Proxy::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->getOwnPropertyNames(cx, proxy, props);
}

bool
Proxy::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->delete_(cx, proxy, id, bp);
}

bool
Proxy::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->enumerate(cx, proxy, props);
}

bool
Proxy::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->has(cx, proxy, id, bp);
}

bool
Proxy::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->hasOwn(cx, proxy, id, bp);
}

bool
Proxy::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->get(cx, proxy, receiver, id, vp);
}

bool
Proxy::getElementIfPresent(JSContext *cx, JSObject *proxy, JSObject *receiver, uint32_t index,
                           Value *vp, bool *present)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->getElementIfPresent(cx, proxy, receiver, index, vp, present);
}

bool
Proxy::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->set(cx, proxy, receiver, id, strict, vp);
}

bool
Proxy::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->keys(cx, proxy, props);
}

bool
Proxy::iterate(JSContext *cx, JSObject *proxy, unsigned flags, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->iterate(cx, proxy, flags, vp);
}

bool
Proxy::call(JSContext *cx, JSObject *proxy, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->call(cx, proxy, argc, vp);
}

bool
Proxy::construct(JSContext *cx, JSObject *proxy, unsigned argc, Value *argv, Value *rval)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->construct(cx, proxy, argc, argv, rval);
}

bool
Proxy::nativeCall(JSContext *cx, JSObject *proxy, Class *clasp, Native native, CallArgs args)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->nativeCall(cx, proxy, clasp, native, args);
}

bool
Proxy::hasInstance(JSContext *cx, JSObject *proxy, const js::Value *vp, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->hasInstance(cx, proxy, vp, bp);
}

JSType
Proxy::typeOf(JSContext *cx, JSObject *proxy)
{
    // FIXME: API doesn't allow us to report error (bug 618906).
    JS_CHECK_RECURSION(cx, return JSTYPE_OBJECT);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->typeOf(cx, proxy);
}

bool
Proxy::objectClassIs(JSObject *proxy, ESClassValue classValue, JSContext *cx)
{
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->objectClassIs(proxy, classValue, cx);
}

JSString *
Proxy::obj_toString(JSContext *cx, JSObject *proxy)
{
    JS_CHECK_RECURSION(cx, return NULL);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->obj_toString(cx, proxy);
}

JSString *
Proxy::fun_toString(JSContext *cx, JSObject *proxy, unsigned indent)
{
    JS_CHECK_RECURSION(cx, return NULL);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->fun_toString(cx, proxy, indent);
}

bool
Proxy::regexp_toShared(JSContext *cx, JSObject *proxy, RegExpGuard *g)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->regexp_toShared(cx, proxy, g);
}

bool
Proxy::defaultValue(JSContext *cx, JSObject *proxy, JSType hint, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->defaultValue(cx, proxy, hint, vp);
}

bool
Proxy::iteratorNext(JSContext *cx, JSObject *proxy, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return GetProxyHandler(proxy)->iteratorNext(cx, proxy, vp);
}

static JSObject *
proxy_innerObject(JSContext *cx, HandleObject obj)
{
    return GetProxyPrivate(obj).toObjectOrNull();
}

static JSBool
proxy_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id, JSObject **objp,
                    JSProperty **propp)
{
    bool found;
    if (!Proxy::has(cx, obj, id, &found))
        return false;

    if (found) {
        *propp = (JSProperty *)0x1;
        *objp = obj;
    } else {
        *objp = NULL;
        *propp = NULL;
    }
    return true;
}

static JSBool
proxy_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, JSObject **objp,
                     JSProperty **propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
proxy_LookupElement(JSContext *cx, HandleObject obj, uint32_t index, JSObject **objp,
                    JSProperty **propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
proxy_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, JSObject **objp, JSProperty **propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
proxy_DefineGeneric(JSContext *cx, HandleObject obj, HandleId id, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoPropertyDescriptorRooter desc(cx);
    desc.obj = obj;
    desc.value = *value;
    desc.attrs = (attrs & (~JSPROP_SHORTID));
    desc.getter = getter;
    desc.setter = setter;
    desc.shortid = 0;
    return Proxy::defineProperty(cx, obj, id, &desc);
}

static JSBool
proxy_DefineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, const Value *value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
proxy_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
proxy_DefineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
proxy_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id, Value *vp)
{
    return Proxy::get(cx, obj, receiver, id, vp);
}

static JSBool
proxy_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name, Value *vp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
proxy_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, Value *vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
proxy_GetElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                          Value *vp, bool *present)
{
    return Proxy::getElementIfPresent(cx, obj, receiver, index, vp, present);
}

static JSBool
proxy_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid, Value *vp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return proxy_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
proxy_SetGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *vp, JSBool strict)
{
    return Proxy::set(cx, obj, obj, id, strict, vp);
}

static JSBool
proxy_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *vp, JSBool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static JSBool
proxy_SetElement(JSContext *cx, HandleObject obj, uint32_t index, Value *vp, JSBool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_SetGeneric(cx, obj, id, vp, strict);
}

static JSBool
proxy_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *vp, JSBool strict)
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
proxy_DeleteGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *rval, JSBool strict)
{
    // TODO: throwing away strict
    bool deleted;
    if (!Proxy::delete_(cx, obj, id, &deleted) || !js_SuppressDeletedProperty(cx, obj, id))
        return false;
    rval->setBoolean(deleted);
    return true;
}

static JSBool
proxy_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *rval, JSBool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return proxy_DeleteGeneric(cx, obj, id, rval, strict);
}

static JSBool
proxy_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index, Value *rval, JSBool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return proxy_DeleteGeneric(cx, obj, id, rval, strict);
}

static JSBool
proxy_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *rval, JSBool strict)
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
             * Assert that this proxy is tracked in the wrapper map. Normally,
             * the wrapped object will be the key in the wrapper map. However,
             * sometimes when wrapping Location objects, the wrapped object is
             * not the key. In that case, we unwrap to find the key.
             */
            Value key = ObjectValue(*referent);
            WrapperMap::Ptr p = obj->compartment()->crossCompartmentWrappers.lookup(key);
            if (!p) {
                key = ObjectValue(*UnwrapObject(referent));
                p = obj->compartment()->crossCompartmentWrappers.lookup(key);
                JS_ASSERT(p.found());
            }
            JS_ASSERT(p->value.get() == ObjectValue(*obj));
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
proxy_Convert(JSContext *cx, HandleObject proxy, JSType hint, Value *vp)
{
    JS_ASSERT(proxy->isProxy());
    return Proxy::defaultValue(cx, proxy, hint, vp);
}

static void
proxy_Finalize(FreeOp *fop, JSObject *obj)
{
    JS_ASSERT(obj->isProxy());
    if (!obj->getSlot(JSSLOT_PROXY_HANDLER).isUndefined())
        GetProxyHandler(obj)->finalize(fop, obj);
}

static JSBool
proxy_HasInstance(JSContext *cx, HandleObject proxy, const Value *v, JSBool *bp)
{
    AutoPendingProxyOperation pending(cx, proxy);
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
        NULL,                /* clear           */
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
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
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
        NULL,                /* clear           */
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
    NULL,                    /* finalize */
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
        NULL,                /* clear           */
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

    JSObject *obj = NewObjectWithGivenProto(cx, clasp, proto, parent);
    if (!obj)
        return NULL;
    obj->setSlot(JSSLOT_PROXY_HANDLER, PrivateValue(handler));
    obj->setSlot(JSSLOT_PROXY_PRIVATE, priv);
    if (fun) {
        obj->setSlot(JSSLOT_PROXY_CALL, call ? ObjectValue(*call) : UndefinedValue());
        if (construct) {
            obj->setSlot(JSSLOT_PROXY_CONSTRUCT, ObjectValue(*construct));
        }
    }

    /* Don't track types of properties of proxies. */
    MarkTypeObjectUnknownProperties(cx, obj->type());

    return obj;
}

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
    JSObject *proxy = NewProxyObject(cx, &ScriptedProxyHandler::singleton, ObjectValue(*handler),
                                     proto, parent);
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

    JSObject *call = js_ValueToCallableObject(cx, &vp[3], JSV2F_SEARCH_STACK);
    if (!call)
        return false;
    JSObject *construct = NULL;
    if (argc > 2) {
        construct = js_ValueToCallableObject(cx, &vp[4], JSV2F_SEARCH_STACK);
        if (!construct)
            return false;
    }

    JSObject *proxy = NewProxyObject(cx, &ScriptedProxyHandler::singleton,
                                     ObjectValue(*handler),
                                     proto, parent, call, construct);
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

Class js::CallableObjectClass = {
    "Function",
    JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL                     /* construct   */
};

Class js::ProxyClass = {
    "Proxy",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, JSObject *obj_)
{
    RootedObject obj(cx, obj_);

    RootedObject module(cx, NewObjectWithClassProto(cx, &ProxyClass, NULL, obj));
    if (!module || !module->setSingletonType(cx))
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
