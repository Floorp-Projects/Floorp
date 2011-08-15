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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include <string.h>
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsprvtd.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsproxy.h"
#include "jsscope.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

namespace js {

static inline const Value &
GetCall(JSObject *proxy) {
    JS_ASSERT(proxy->isFunctionProxy());
    return proxy->getSlot(JSSLOT_PROXY_CALL);
}

static inline Value
GetConstruct(JSObject *proxy) {
    if (proxy->numSlots() <= JSSLOT_PROXY_CONSTRUCT)
        return UndefinedValue();
    return proxy->getSlot(JSSLOT_PROXY_CONSTRUCT);
}

static bool
OperationInProgress(JSContext *cx, JSObject *proxy)
{
    PendingProxyOperation *op = JS_THREAD_DATA(cx)->pendingProxyOperation;
    while (op) {
        if (op->object == proxy)
            return true;
        op = op->next;
    }
    return false;
}

JSProxyHandler::JSProxyHandler(void *family) : mFamily(family)
{
}

JSProxyHandler::~JSProxyHandler()
{
}

bool
JSProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
JSProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
JSProxyHandler::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, false, &desc))
        return false;
    if (!desc.obj) {
        vp->setUndefined();
        return true;
    }
    if (!desc.getter ||
        (!(desc.attrs & JSPROP_GETTER) && desc.getter == PropertyStub)) {
        *vp = desc.value;
        return true;
    }
    if (desc.attrs & JSPROP_GETTER) {
        return ExternalGetOrSet(cx, receiver, id, CastAsObjectJsval(desc.getter),
                                JSACC_READ, 0, NULL, vp);
    }
    if (!(desc.attrs & JSPROP_SHARED))
        *vp = desc.value;
    else
        vp->setUndefined();
    if (desc.attrs & JSPROP_SHORTID)
        id = INT_TO_JSID(desc.shortid);
    return CallJSPropertyOp(cx, desc.getter, receiver, id, vp);
}

bool
JSProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                    Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, true, &desc))
        return false;
    /* The control-flow here differs from ::get() because of the fall-through case below. */
    if (desc.obj) {
        if (desc.attrs & JSPROP_READONLY)
            return true;
        if (!desc.setter) {
            desc.setter = StrictPropertyStub;
        } else if ((desc.attrs & JSPROP_SETTER) || desc.setter != StrictPropertyStub) {
            if (!CallSetter(cx, receiver, id, desc.setter, desc.attrs, desc.shortid, strict, vp))
                return false;
            if (!proxy->isProxy() || proxy->getProxyHandler() != this)
                return true;
            if (desc.attrs & JSPROP_SHARED)
                return true;
        }
        if (!desc.getter)
            desc.getter = PropertyStub;
        desc.value = *vp;
        return defineProperty(cx, receiver, id, &desc);
    }
    if (!getPropertyDescriptor(cx, proxy, id, true, &desc))
        return false;
    if (desc.obj) {
        if (desc.attrs & JSPROP_READONLY)
            return true;
        if (!desc.setter) {
            desc.setter = StrictPropertyStub;
        } else if ((desc.attrs & JSPROP_SETTER) || desc.setter != StrictPropertyStub) {
            if (!CallSetter(cx, receiver, id, desc.setter, desc.attrs, desc.shortid, strict, vp))
                return false;
            if (!proxy->isProxy() || proxy->getProxyHandler() != this)
                return true;
            if (desc.attrs & JSPROP_SHARED)
                return true;
        }
        if (!desc.getter)
            desc.getter = PropertyStub;
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
JSProxyHandler::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
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
JSProxyHandler::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoIdVector props(cx);
    if ((flags & JSITER_OWNONLY)
        ? !keys(cx, proxy, props)
        : !enumerate(cx, proxy, props)) {
        return false;
    }
    return EnumeratedIdVectorToIterator(cx, proxy, flags, props, vp);
}

JSString *
JSProxyHandler::obj_toString(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(proxy->isProxy());

    return JS_NewStringCopyZ(cx, proxy->isFunctionProxy()
                                 ? "[object Function]"
                                 : "[object Object]");
}

JSString *
JSProxyHandler::fun_toString(JSContext *cx, JSObject *proxy, uintN indent)
{
    JS_ASSERT(proxy->isProxy());
    Value fval = GetCall(proxy);
    if (proxy->isFunctionProxy() &&
        (fval.isPrimitive() || !fval.toObject().isFunction())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return NULL;
    }
    return fun_toStringHelper(cx, &fval.toObject(), indent);
}

bool
JSProxyHandler::defaultValue(JSContext *cx, JSObject *proxy, JSType hint, Value *vp)
{
    return DefaultValue(cx, proxy, hint, vp);
}

bool
JSProxyHandler::call(JSContext *cx, JSObject *proxy, uintN argc, Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoValueRooter rval(cx);
    JSBool ok = ExternalInvoke(cx, vp[1], GetCall(proxy), argc, JS_ARGV(cx, vp),
                               rval.addr());
    if (ok)
        JS_SET_RVAL(cx, vp, rval.value());
    return ok;
}

bool
JSProxyHandler::construct(JSContext *cx, JSObject *proxy,
                          uintN argc, Value *argv, Value *rval)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    Value fval = GetConstruct(proxy);
    if (fval.isUndefined())
        return ExternalInvokeConstructor(cx, GetCall(proxy), argc, argv, rval);
    return ExternalInvoke(cx, UndefinedValue(), fval, argc, argv, rval);
}

bool
JSProxyHandler::hasInstance(JSContext *cx, JSObject *proxy, const Value *vp, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, ObjectValue(*proxy), NULL);
    return false;
}

JSType
JSProxyHandler::typeOf(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    return proxy->isFunctionProxy() ? JSTYPE_FUNCTION : JSTYPE_OBJECT;
}

void
JSProxyHandler::finalize(JSContext *cx, JSObject *proxy)
{
}

void
JSProxyHandler::trace(JSTracer *trc, JSObject *proxy)
{
}

static bool
GetTrap(JSContext *cx, JSObject *handler, JSAtom *atom, Value *fvalp)
{
    JS_CHECK_RECURSION(cx, return false);

    return handler->getProperty(cx, ATOM_TO_JSID(atom), fvalp);
}

static bool
GetFundamentalTrap(JSContext *cx, JSObject *handler, JSAtom *atom, Value *fvalp)
{
    if (!GetTrap(cx, handler, atom, fvalp))
        return false;

    if (!js_IsCallable(*fvalp)) {
        JSAutoByteString bytes;
        if (js_AtomToPrintableString(cx, atom, &bytes))
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_FUNCTION, bytes.ptr());
        return false;
    }

    return true;
}

static bool
GetDerivedTrap(JSContext *cx, JSObject *handler, JSAtom *atom, Value *fvalp)
{
    JS_ASSERT(atom == ATOM(has) ||
              atom == ATOM(hasOwn) ||
              atom == ATOM(get) ||
              atom == ATOM(set) ||
              atom == ATOM(keys) ||
              atom == ATOM(iterate));

    return GetTrap(cx, handler, atom, fvalp);
}

static bool
Trap(JSContext *cx, JSObject *handler, Value fval, uintN argc, Value* argv, Value *rval)
{
    return ExternalInvoke(cx, ObjectValue(*handler), fval, argc, argv, rval);
}

static bool
Trap1(JSContext *cx, JSObject *handler, Value fval, jsid id, Value *rval)
{
    JSString *str = js_ValueToString(cx, IdToValue(id));
    if (!str)
        return false;
    rval->setString(str);
    return Trap(cx, handler, fval, 1, rval, rval);
}

static bool
Trap2(JSContext *cx, JSObject *handler, Value fval, jsid id, Value v, Value *rval)
{
    JSString *str = js_ValueToString(cx, IdToValue(id));
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
    desc->value = d->value;
    JS_ASSERT(!(d->attrs & JSPROP_SHORTID));
    desc->attrs = d->attrs;
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

bool
ArrayToIdVector(JSContext *cx, const Value &array, AutoIdVector &props)
{
    JS_ASSERT(props.length() == 0);

    if (array.isPrimitive())
        return true;

    JSObject *obj = &array.toObject();
    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return false;

    AutoIdRooter idr(cx);
    AutoValueRooter tvr(cx);
    for (jsuint n = 0; n < length; ++n) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;
        if (!IndexToId(cx, n, idr.addr()))
            return false;
        if (!obj->getProperty(cx, idr.id(), tvr.addr()))
            return false;
        if (!ValueToId(cx, tvr.value(), idr.addr()))
            return false;
        if (!props.append(js_CheckForStringIndex(idr.id())))
            return false;
    }

    return true;
}

/* Derived class for all scripted proxy handlers. */
class JSScriptedProxyHandler : public JSProxyHandler {
  public:
    JSScriptedProxyHandler();
    virtual ~JSScriptedProxyHandler();

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
    virtual bool fix(JSContext *cx, JSObject *proxy, Value *vp);

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp);
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                     Value *vp);
    virtual bool keys(JSContext *cx, JSObject *proxy, AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp);

    static JSScriptedProxyHandler singleton;
};

static int sScriptedProxyHandlerFamily = 0;

JSScriptedProxyHandler::JSScriptedProxyHandler() : JSProxyHandler(&sScriptedProxyHandlerFamily)
{
}

JSScriptedProxyHandler::~JSScriptedProxyHandler()
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
    return proxy->getProxyPrivate().toObjectOrNull();
}

bool
JSScriptedProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                              PropertyDescriptor *desc)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getPropertyDescriptor), tvr.addr()) &&
           Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ((tvr.value().isUndefined() && IndicatePropertyNotFound(cx, desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), tvr.value()) &&
             ParsePropertyDescriptorObject(cx, proxy, id, tvr.value(), desc)));
}

bool
JSScriptedProxyHandler::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                                 PropertyDescriptor *desc)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getOwnPropertyDescriptor), tvr.addr()) &&
           Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ((tvr.value().isUndefined() && IndicatePropertyNotFound(cx, desc)) ||
            (ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), tvr.value()) &&
             ParsePropertyDescriptorObject(cx, proxy, id, tvr.value(), desc)));
}

bool
JSScriptedProxyHandler::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                       PropertyDescriptor *desc)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    AutoValueRooter fval(cx);
    return GetFundamentalTrap(cx, handler, ATOM(defineProperty), fval.addr()) &&
           NewPropertyDescriptorObject(cx, desc, tvr.addr()) &&
           Trap2(cx, handler, fval.value(), id, tvr.value(), tvr.addr());
}

bool
JSScriptedProxyHandler::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return GetFundamentalTrap(cx, handler, ATOM(getOwnPropertyNames), tvr.addr()) &&
           Trap(cx, handler, tvr.value(), 0, NULL, tvr.addr()) &&
           ArrayToIdVector(cx, tvr.value(), props);
}

bool
JSScriptedProxyHandler::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return GetFundamentalTrap(cx, handler, ATOM(delete), tvr.addr()) &&
           Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ValueToBool(cx, tvr.value(), bp);
}

bool
JSScriptedProxyHandler::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return GetFundamentalTrap(cx, handler, ATOM(enumerate), tvr.addr()) &&
           Trap(cx, handler, tvr.value(), 0, NULL, tvr.addr()) &&
           ArrayToIdVector(cx, tvr.value(), props);
}

bool
JSScriptedProxyHandler::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    return GetFundamentalTrap(cx, handler, ATOM(fix), vp) &&
           Trap(cx, handler, *vp, 0, NULL, vp);
}

bool
JSScriptedProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(has), tvr.addr()))
        return false;
    if (!js_IsCallable(tvr.value()))
        return JSProxyHandler::has(cx, proxy, id, bp);
    return Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ValueToBool(cx, tvr.value(), bp);
}

bool
JSScriptedProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(hasOwn), tvr.addr()))
        return false;
    if (!js_IsCallable(tvr.value()))
        return JSProxyHandler::hasOwn(cx, proxy, id, bp);
    return Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ValueToBool(cx, tvr.value(), bp);
}

bool
JSScriptedProxyHandler::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    JSString *str = js_ValueToString(cx, IdToValue(id));
    if (!str)
        return false;
    AutoValueRooter tvr(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), tvr.value() };
    AutoValueRooter fval(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(get), fval.addr()))
        return false;
    if (!js_IsCallable(fval.value()))
        return JSProxyHandler::get(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval.value(), 2, argv, vp);
}

bool
JSScriptedProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                            Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    JSString *str = js_ValueToString(cx, IdToValue(id));
    if (!str)
        return false;
    AutoValueRooter tvr(cx, StringValue(str));
    Value argv[] = { ObjectOrNullValue(receiver), tvr.value(), *vp };
    AutoValueRooter fval(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(set), fval.addr()))
        return false;
    if (!js_IsCallable(fval.value()))
        return JSProxyHandler::set(cx, proxy, receiver, id, strict, vp);
    return Trap(cx, handler, fval.value(), 3, argv, tvr.addr());
}

bool
JSScriptedProxyHandler::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(keys), tvr.addr()))
        return false;
    if (!js_IsCallable(tvr.value()))
        return JSProxyHandler::keys(cx, proxy, props);
    return Trap(cx, handler, tvr.value(), 0, NULL, tvr.addr()) &&
           ArrayToIdVector(cx, tvr.value(), props);
}

bool
JSScriptedProxyHandler::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!GetDerivedTrap(cx, handler, ATOM(iterate), tvr.addr()))
        return false;
    if (!js_IsCallable(tvr.value()))
        return JSProxyHandler::iterate(cx, proxy, flags, vp);
    return Trap(cx, handler, tvr.value(), 0, NULL, vp) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(iterate), *vp);
}

JSScriptedProxyHandler JSScriptedProxyHandler::singleton;

class AutoPendingProxyOperation {
    ThreadData              *data;
    PendingProxyOperation   op;
  public:
    AutoPendingProxyOperation(JSContext *cx, JSObject *proxy) : data(JS_THREAD_DATA(cx)) {
        op.next = data->pendingProxyOperation;
        op.object = proxy;
        data->pendingProxyOperation = &op;
    }

    ~AutoPendingProxyOperation() {
        JS_ASSERT(data->pendingProxyOperation == &op);
        data->pendingProxyOperation = op.next;
    }
};

bool
JSProxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                               PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->getPropertyDescriptor(cx, proxy, id, set, desc);
}

bool
JSProxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return JSProxy::getPropertyDescriptor(cx, proxy, id, set, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

bool
JSProxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                  PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->getOwnPropertyDescriptor(cx, proxy, id, set, desc);
}

bool
JSProxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return JSProxy::getOwnPropertyDescriptor(cx, proxy, id, set, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

bool
JSProxy::defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->defineProperty(cx, proxy, id, desc);
}

bool
JSProxy::defineProperty(JSContext *cx, JSObject *proxy, jsid id, const Value &v)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return ParsePropertyDescriptorObject(cx, proxy, id, v, &desc) &&
           JSProxy::defineProperty(cx, proxy, id, &desc);
}

bool
JSProxy::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->getOwnPropertyNames(cx, proxy, props);
}

bool
JSProxy::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->delete_(cx, proxy, id, bp);
}

bool
JSProxy::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->enumerate(cx, proxy, props);
}

bool
JSProxy::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->fix(cx, proxy, vp);
}

bool
JSProxy::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->has(cx, proxy, id, bp);
}

bool
JSProxy::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->hasOwn(cx, proxy, id, bp);
}

bool
JSProxy::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->get(cx, proxy, receiver, id, vp);
}

bool
JSProxy::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->set(cx, proxy, receiver, id, strict, vp);
}

bool
JSProxy::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->keys(cx, proxy, props);
}

bool
JSProxy::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->iterate(cx, proxy, flags, vp);
}

bool
JSProxy::call(JSContext *cx, JSObject *proxy, uintN argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->call(cx, proxy, argc, vp);
}

bool
JSProxy::construct(JSContext *cx, JSObject *proxy, uintN argc, Value *argv, Value *rval)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->construct(cx, proxy, argc, argv, rval);
}

bool
JSProxy::hasInstance(JSContext *cx, JSObject *proxy, const js::Value *vp, bool *bp)
{
    JS_CHECK_RECURSION(cx, return false);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->hasInstance(cx, proxy, vp, bp);
}

JSType
JSProxy::typeOf(JSContext *cx, JSObject *proxy)
{
    // FIXME: API doesn't allow us to report error (bug 618906).
    JS_CHECK_RECURSION(cx, return JSTYPE_OBJECT);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->typeOf(cx, proxy);
}

JSString *
JSProxy::obj_toString(JSContext *cx, JSObject *proxy)
{
    JS_CHECK_RECURSION(cx, return NULL);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->obj_toString(cx, proxy);
}

JSString *
JSProxy::fun_toString(JSContext *cx, JSObject *proxy, uintN indent)
{
    JS_CHECK_RECURSION(cx, return NULL);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->fun_toString(cx, proxy, indent);
}

bool
JSProxy::defaultValue(JSContext *cx, JSObject *proxy, JSType hint, Value *vp)
{
    JS_CHECK_RECURSION(cx, return NULL);
    AutoPendingProxyOperation pending(cx, proxy);
    return proxy->getProxyHandler()->defaultValue(cx, proxy, hint, vp);
}

static JSObject *
proxy_innerObject(JSContext *cx, JSObject *obj)
{
    return obj->getProxyPrivate().toObjectOrNull();
}

static JSBool
proxy_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                     JSProperty **propp)
{
    bool found;
    if (!JSProxy::has(cx, obj, id, &found))
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
proxy_DefineProperty(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                     PropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    AutoPropertyDescriptorRooter desc(cx);
    desc.obj = obj;
    desc.value = *value;
    desc.attrs = (attrs & (~JSPROP_SHORTID));
    desc.getter = getter;
    desc.setter = setter;
    desc.shortid = 0;
    return JSProxy::defineProperty(cx, obj, id, &desc);
}

static JSBool
proxy_GetProperty(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    return JSProxy::get(cx, obj, receiver, id, vp);
}

static JSBool
proxy_SetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    return JSProxy::set(cx, obj, obj, id, strict, vp);
}

static JSBool
proxy_GetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    AutoPropertyDescriptorRooter desc(cx);
    if (!JSProxy::getOwnPropertyDescriptor(cx, obj, id, false, &desc))
        return false;
    *attrsp = desc.attrs;
    return true;
}

static JSBool
proxy_SetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    /* Lookup the current property descriptor so we have setter/getter/value. */
    AutoPropertyDescriptorRooter desc(cx);
    if (!JSProxy::getOwnPropertyDescriptor(cx, obj, id, true, &desc))
        return false;
    desc.attrs = (*attrsp & (~JSPROP_SHORTID));
    return JSProxy::defineProperty(cx, obj, id, &desc);
}

static JSBool
proxy_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    // TODO: throwing away strict
    bool deleted;
    if (!JSProxy::delete_(cx, obj, id, &deleted) || !js_SuppressDeletedProperty(cx, obj, id))
        return false;
    rval->setBoolean(deleted);
    return true;
}

static void
proxy_TraceObject(JSTracer *trc, JSObject *obj)
{
    obj->getProxyHandler()->trace(trc, obj);
    MarkCrossCompartmentValue(trc, obj->getProxyPrivate(), "private");
    MarkCrossCompartmentValue(trc, obj->getProxyExtra(), "extra");
    if (obj->isFunctionProxy()) {
        MarkCrossCompartmentValue(trc, GetCall(obj), "call");
        MarkCrossCompartmentValue(trc, GetConstruct(obj), "construct");
    }
}

static void
proxy_TraceFunction(JSTracer *trc, JSObject *obj)
{
    proxy_TraceObject(trc, obj);
    MarkCrossCompartmentValue(trc, GetCall(obj), "call");
    MarkCrossCompartmentValue(trc, GetConstruct(obj), "construct");
}

static JSBool
proxy_Convert(JSContext *cx, JSObject *proxy, JSType hint, Value *vp)
{
    JS_ASSERT(proxy->isProxy());
    return JSProxy::defaultValue(cx, proxy, hint, vp);
}

static JSBool
proxy_Fix(JSContext *cx, JSObject *obj, bool *fixed, AutoIdVector *props)
{
    JS_ASSERT(obj->isProxy());
    JSBool isFixed;
    bool ok = FixProxy(cx, obj, &isFixed);
    if (ok) {
        *fixed = isFixed;
        return GetPropertyNames(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, props);
    }
    return false;
}

static void
proxy_Finalize(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isProxy());
    if (!obj->getSlot(JSSLOT_PROXY_HANDLER).isUndefined())
        obj->getProxyHandler()->finalize(cx, obj);
}

static JSBool
proxy_HasInstance(JSContext *cx, JSObject *proxy, const Value *v, JSBool *bp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    bool b;
    if (!JSProxy::hasInstance(cx, proxy, v, &b))
        return false;
    *bp = !!b;
    return true;
}

static JSType
proxy_TypeOf(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(proxy->isProxy());
    return JSProxy::typeOf(cx, proxy);
}

JS_FRIEND_API(Class) ObjectProxyClass = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_HAS_RESERVED_SLOTS(3),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    proxy_Convert,
    proxy_Finalize,       /* finalize    */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    proxy_HasInstance,    /* hasInstance */
    proxy_TraceObject,    /* trace       */
    JS_NULL_CLASS_EXT,
    {
        proxy_LookupProperty,
        proxy_DefineProperty,
        proxy_GetProperty,
        proxy_SetProperty,
        proxy_GetAttributes,
        proxy_SetAttributes,
        proxy_DeleteProperty,
        NULL,             /* enumerate       */
        proxy_TypeOf,
        proxy_Fix,        /* fix             */
        NULL,             /* thisObject      */
        NULL,             /* clear           */
    }
};

JS_FRIEND_API(Class) OuterWindowProxyClass = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_HAS_RESERVED_SLOTS(3),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    proxy_Finalize,       /* finalize    */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    proxy_TraceObject,    /* trace       */
    {
        NULL,             /* equality    */
        NULL,             /* outerObject */
        proxy_innerObject,
        NULL        /* unused */
    },
    {
        proxy_LookupProperty,
        proxy_DefineProperty,
        proxy_GetProperty,
        proxy_SetProperty,
        proxy_GetAttributes,
        proxy_SetAttributes,
        proxy_DeleteProperty,
        NULL,             /* enumerate       */
        NULL,             /* typeof          */
        NULL,             /* fix             */
        NULL,             /* thisObject      */
        NULL,             /* clear           */
    }
};

JSBool
proxy_Call(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *proxy = &JS_CALLEE(cx, vp).toObject();
    JS_ASSERT(proxy->isProxy());
    return JSProxy::call(cx, proxy, argc, vp);
}

JSBool
proxy_Construct(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *proxy = &JS_CALLEE(cx, vp).toObject();
    JS_ASSERT(proxy->isProxy());
    Value rval;
    bool ok = JSProxy::construct(cx, proxy, argc, JS_ARGV(cx, vp), &rval);
    *vp = rval;
    return ok;
}

JS_FRIEND_API(Class) FunctionProxyClass = {
    "Proxy",
    Class::NON_NATIVE | JSCLASS_HAS_RESERVED_SLOTS(5),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    NULL,                 /* finalize */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    proxy_Call,
    proxy_Construct,
    NULL,                 /* xdrObject   */
    js_FunctionClass.hasInstance,
    proxy_TraceFunction,  /* trace       */
    JS_NULL_CLASS_EXT,
    {
        proxy_LookupProperty,
        proxy_DefineProperty,
        proxy_GetProperty,
        proxy_SetProperty,
        proxy_GetAttributes,
        proxy_SetAttributes,
        proxy_DeleteProperty,
        NULL,             /* enumerate       */
        proxy_TypeOf,
        NULL,             /* fix             */
        NULL,             /* thisObject      */
        NULL,             /* clear           */
    }
};

JS_FRIEND_API(JSObject *)
NewProxyObject(JSContext *cx, JSProxyHandler *handler, const Value &priv, JSObject *proto,
               JSObject *parent, JSObject *call, JSObject *construct)
{
    JS_ASSERT_IF(proto, cx->compartment == proto->compartment());
    JS_ASSERT_IF(parent, cx->compartment == parent->compartment());
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
    if (proto)
        proto->getNewType(cx, NULL, /* markUnknown = */ true);

    JSObject *obj = NewNonFunction<WithProto::Given>(cx, clasp, proto, parent);
    if (!obj || !obj->ensureInstanceReservedSlots(cx, 0))
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
proxy_create(JSContext *cx, uintN argc, Value *vp)
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
    JSObject *proxy = NewProxyObject(cx, &JSScriptedProxyHandler::singleton, ObjectValue(*handler),
                                     proto, parent);
    if (!proxy)
        return false;

    vp->setObject(*proxy);
    return true;
}

static JSBool
proxy_createFunction(JSContext *cx, uintN argc, Value *vp)
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
    if (!js_GetClassPrototype(cx, parent, JSProto_Function, &proto))
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

    JSObject *proxy = NewProxyObject(cx, &JSScriptedProxyHandler::singleton,
                                     ObjectValue(*handler),
                                     proto, parent, call, construct);
    if (!proxy)
        return false;

    vp->setObject(*proxy);
    return true;
}

#ifdef DEBUG

static JSBool
proxy_isTrapping(JSContext *cx, uintN argc, Value *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "isTrapping", "0", "s");
        return false;
    }
    JSObject *obj = NonNullObject(cx, vp[2]);
    if (!obj)
        return false;
    vp->setBoolean(obj->isProxy());
    return true;
}

static JSBool
proxy_fix(JSContext *cx, uintN argc, Value *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "fix", "0", "s");
        return false;
    }
    JSObject *obj = NonNullObject(cx, vp[2]);
    if (!obj)
        return false;
    if (obj->isProxy()) {
        JSBool flag;
        if (!FixProxy(cx, obj, &flag))
            return false;
        vp->setBoolean(flag);
    } else {
        vp->setBoolean(true);
    }
    return true;
}

#endif

static JSFunctionSpec static_methods[] = {
    JS_FN("create",         proxy_create,          2, 0),
    JS_FN("createFunction", proxy_createFunction,  3, 0),
#ifdef DEBUG
    JS_FN("isTrapping",     proxy_isTrapping,      1, 0),
    JS_FN("fix",            proxy_fix,             1, 0),
#endif
    JS_FS_END
};

extern Class CallableObjectClass;

static const uint32 JSSLOT_CALLABLE_CALL = 0;
static const uint32 JSSLOT_CALLABLE_CONSTRUCT = 1;

static JSBool
callable_Call(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *callable = &JS_CALLEE(cx, vp).toObject();
    JS_ASSERT(callable->getClass() == &CallableObjectClass);
    const Value &fval = callable->getSlot(JSSLOT_CALLABLE_CALL);
    const Value &thisval = vp[1];
    Value rval;
    bool ok = ExternalInvoke(cx, thisval, fval, argc, JS_ARGV(cx, vp), &rval);
    *vp = rval;
    return ok;
}

JSBool
callable_Construct(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *thisobj = js_CreateThis(cx, &JS_CALLEE(cx, vp).toObject());
    if (!thisobj)
        return false;

    JSObject *callable = &vp[0].toObject();
    JS_ASSERT(callable->getClass() == &CallableObjectClass);
    Value fval = callable->getSlot(JSSLOT_CALLABLE_CONSTRUCT);
    if (fval.isUndefined()) {
        /* We don't have an explicit constructor so allocate a new object and use the call. */
        fval = callable->getSlot(JSSLOT_CALLABLE_CALL);
        JS_ASSERT(fval.isObject());

        /* callable is the constructor, so get callable.prototype is the proto of the new object. */
        Value protov;
        if (!callable->getProperty(cx, ATOM_TO_JSID(ATOM(classPrototype)), &protov))
            return false;

        JSObject *proto;
        if (protov.isObject()) {
            proto = &protov.toObject();
        } else {
            if (!js_GetClassPrototype(cx, NULL, JSProto_Object, &proto))
                return false;
        }

        JSObject *newobj = NewNativeClassInstance(cx, &js_ObjectClass, proto, proto->getParent());
        if (!newobj)
            return false;

        /* If the call returns an object, return that, otherwise the original newobj. */
        Value rval;
        if (!ExternalInvoke(cx, ObjectValue(*newobj), callable->getSlot(JSSLOT_CALLABLE_CALL),
                            argc, vp + 2, &rval)) {
            return false;
        }
        if (rval.isPrimitive())
            vp->setObject(*newobj);
        else
            *vp = rval;
        return true;
    }

    Value rval;
    bool ok = ExternalInvoke(cx, ObjectValue(*thisobj), fval, argc, vp + 2, &rval);
    *vp = rval;
    return ok;
}

Class CallableObjectClass = {
    "Function",
    JSCLASS_HAS_RESERVED_SLOTS(2),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    NULL,                 /* finalize    */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    callable_Call,
    callable_Construct,
};

JS_FRIEND_API(JSBool)
FixProxy(JSContext *cx, JSObject *proxy, JSBool *bp)
{
    if (OperationInProgress(cx, proxy)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROXY_FIX);
        return false;
    }

    AutoValueRooter tvr(cx);
    if (!JSProxy::fix(cx, proxy, tvr.addr()))
        return false;
    if (tvr.value().isUndefined()) {
        *bp = false;
        return true;
    }

    JSObject *props = NonNullObject(cx, tvr.value());
    if (!props)
        return false;

    JSObject *proto = proxy->getProto();
    JSObject *parent = proxy->getParent();
    Class *clasp = proxy->isFunctionProxy() ? &CallableObjectClass : &js_ObjectClass;

    /*
     * Make a blank object from the recipe fix provided to us.  This must have
     * number of fixed slots as the proxy so that we can swap their contents.
     */
    gc::FinalizeKind kind = gc::FinalizeKind(proxy->arenaHeader()->getThingKind());
    JSObject *newborn = NewNonFunction<WithProto::Given>(cx, clasp, proto, parent, kind);
    if (!newborn)
        return false;
    AutoObjectRooter tvr2(cx, newborn);

    if (clasp == &CallableObjectClass) {
        newborn->setSlot(JSSLOT_CALLABLE_CALL, GetCall(proxy));
        newborn->setSlot(JSSLOT_CALLABLE_CONSTRUCT, GetConstruct(proxy));
    }

    {
        AutoPendingProxyOperation pending(cx, proxy);
        if (!js_PopulateObject(cx, newborn, props))
            return false;
    }

    /* Trade contents between the newborn object and the proxy. */
    if (!proxy->swap(cx, newborn))
        return false;

    /* The GC will dispose of the proxy object. */

    *bp = true;
    return true;
}

}

Class js_ProxyClass = {
    "Proxy",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, JSObject *obj)
{
    JSObject *module = NewNonFunction<WithProto::Class>(cx, &js_ProxyClass, NULL, obj);
    if (!module || !module->setSingletonType(cx))
        return NULL;

    if (!JS_DefineProperty(cx, obj, "Proxy", OBJECT_TO_JSVAL(module),
                           JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }
    if (!JS_DefineFunctions(cx, module, static_methods))
        return NULL;

    MarkStandardClassInitializedNoProto(obj, &js_ProxyClass);

    return module;
}
