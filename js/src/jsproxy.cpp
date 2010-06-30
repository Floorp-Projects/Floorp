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
#include "jsprvtd.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsproxy.h"
#include "jsscope.h"

#include "jsobjinlines.h"

using namespace js;

namespace js {

static bool
OperationInProgress(JSContext *cx, JSObject *proxy)
{
    JSPendingProxyOperation *op = JS_THREAD_DATA(cx)->pendingProxyOperation;
    while (op) {
        if (op->object == proxy)
            return true;
        op = op->next;
    }
    return false;
}

JSProxyHandler::~JSProxyHandler()
{
}

bool
JSProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
JSProxyHandler::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
JSProxyHandler::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    if (!desc.obj) {
        vp->setUndefined();
        return true;
    }
    if (!desc.getter) {
        *vp = desc.value;
        return true;
    }
    if (desc.attrs & JSPROP_GETTER) {
        return InternalGetOrSet(cx, proxy, id, CastAsObjectJsval(desc.getter),
                                JSACC_READ, 0, 0, vp);
    }
    if (desc.attrs & JSPROP_SHORTID)
        id = INT_TO_JSID(desc.shortid);
    return desc.getter(cx, proxy, id, vp);
}

bool
JSProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    AutoPropertyDescriptorRooter desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    /* The control-flow here differs from ::get() because of the fall-through case below. */
    if (desc.obj) {
        if (desc.setter) {
            if (desc.attrs & JSPROP_SETTER) {
                return InternalGetOrSet(cx, proxy, id, CastAsObjectJsval(desc.setter),
                                        JSACC_READ, 0, 0, vp);
            }
            if (desc.attrs & JSPROP_SHORTID)
                id = INT_TO_JSID(desc.shortid);
            return desc.setter(cx, proxy, id, vp);
        }
        if (desc.attrs & JSPROP_READONLY)
            return true;
        desc.value = *vp;
        return defineProperty(cx, proxy, id, &desc);
    }
    if (!getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    if (desc.obj) {
        if (desc.setter) {
            if (desc.attrs & JSPROP_SETTER) {
                return InternalGetOrSet(cx, proxy, id, CastAsObjectJsval(desc.setter),
                                        JSACC_READ, 0, 0, vp);
            }
            if (desc.attrs & JSPROP_SHORTID)
                id = INT_TO_JSID(desc.shortid);
            return desc.setter(cx, proxy, id, vp);
        }
        if (desc.attrs & JSPROP_READONLY)
            return true;
        /* fall through */
    }
    desc.obj = proxy;
    desc.value = *vp;
    desc.attrs = 0;
    desc.getter = NULL;
    desc.setter = NULL;
    desc.shortid = 0;
    return defineProperty(cx, proxy, id, &desc);
}

bool
JSProxyHandler::enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    if (!getOwnPropertyNames(cx, proxy, idap))
        return false;
    AutoIdArray ida(cx, *idap);
    size_t w = 0;
    jsid *vector = (*idap)->vector;
    AutoPropertyDescriptorRooter desc(cx);
    for (size_t n = 0; n < ida.length(); ++n) {
        JS_ASSERT(n >= w);
        vector[w] = vector[n];
        if (!getOwnPropertyDescriptor(cx, proxy, vector[n], &desc))
            return false;
        if (desc.obj && (desc.attrs & JSPROP_ENUMERATE))
            ++w;
    }
    (*idap)->length = w;
    ida.steal();
    return true;
}

bool
JSProxyHandler::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    JS_ASSERT(OperationInProgress(cx, proxy));

    /* TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=572298 */
    JS_ASSERT((flags & JSITER_FOREACH) == 0);

    JSIdArray *ida;
    if (!enumerate(cx, proxy, &ida))
        return false;
    AutoIdArray idar(cx, ida);

    return JSIdArrayToIterator(cx, proxy, flags, ida, vp);
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
    return handler->getProperty(cx, ATOM_TO_JSID(atom), fvalp);
}

static bool
FundamentalTrap(JSContext *cx, JSObject *handler, JSAtom *atom, Value *fvalp)
{
    if (!GetTrap(cx, handler, atom, fvalp))
        return false;

    if (!js_IsCallable(*fvalp)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_FUNCTION,
                             js_AtomToPrintableString(cx, atom));
        return false;
    }

    return true;
}

static bool
DerivedTrap(JSContext *cx, JSObject *handler, JSAtom *atom, Value *fvalp)
{
    JS_ASSERT(atom == ATOM(has) ||
              atom == ATOM(hasOwn) ||
              atom == ATOM(get) ||
              atom == ATOM(set) ||
              atom == ATOM(enumerateOwn) ||
              atom == ATOM(iterate));
    
    return GetTrap(cx, handler, atom, fvalp);
}

static bool
Trap(JSContext *cx, JSObject *handler, Value fval, uintN argc, Value* argv, Value *rval)
{
    JS_CHECK_RECURSION(cx, return false);

    return InternalCall(cx, handler, fval, argc, argv, rval);
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
    if (!d || !d->initialize(cx, id, v))
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
MakePropertyDescriptorObject(JSContext *cx, jsid id, PropertyDescriptor *desc, Value *vp)
{
    if (!desc->obj) {
        vp->setUndefined();
        return true;
    }
    uintN attrs = desc->attrs;
    Value getter = (attrs & JSPROP_GETTER) ? CastAsObjectJsval(desc->getter) : undefinedValue();
    Value setter = (attrs & JSPROP_SETTER) ? CastAsObjectJsval(desc->setter) : undefinedValue();
    return js_NewPropertyDescriptorObject(cx, id, attrs, getter, setter, desc->value, vp);
}

static bool
ValueToBool(JSContext *cx, const Value &v, bool *bp)
{
    *bp = !!js_ValueToBoolean(v);
    return true;
}

bool
ArrayToJSIdArray(JSContext *cx, const Value &array, JSIdArray **idap)
{
    if (array.isPrimitive())
        return (*idap = NewIdArray(cx, 0)) != NULL;

    JSObject *obj = &array.asObject();
    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }
    AutoIdArray ida(cx, *idap = NewIdArray(cx, length));
    if (!ida)
        return false;
    AutoValueRooter tvr(cx);
    jsid *vector = (*idap)->vector;
    for (jsuint n = 0; n < length; ++n) {
        if (!js_IndexToId(cx, n, &vector[n]))
            return false;
        if (!obj->getProperty(cx, vector[n], tvr.addr()))
            return false;
        if (!ValueToId(cx, tvr.value(), &vector[n]))
            return false;
        vector[n] = js_CheckForStringIndex(vector[n]);
    }
    *idap = ida.steal();
    return true;
}

/* Derived class for all scripted proxy handlers. */
class JSScriptedProxyHandler : public JSProxyHandler {
  public:
    JSScriptedProxyHandler();
    virtual ~JSScriptedProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                       PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                          PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    virtual bool fix(JSContext *cx, JSObject *proxy, Value *vp);

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp);
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp);
    virtual bool enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    virtual bool iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp);

    /* Spidermonkey extensions. */
    virtual const void *family();

    static JSScriptedProxyHandler singleton;
};

JSScriptedProxyHandler::JSScriptedProxyHandler()
{
}

JSScriptedProxyHandler::~JSScriptedProxyHandler()
{
}

static bool
ReturnedValueMustNotBePrimitive(JSContext *cx, JSObject *proxy, JSAtom *atom, const Value &v)
{
    if (v.isPrimitive()) {
        js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                             JSDVG_SEARCH_STACK, ObjectOrNullTag(proxy), NULL,
                             js_AtomToPrintableString(cx, atom));
        return false;
    }
    return true;
}

static JSObject *
GetProxyHandlerObject(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(OperationInProgress(cx, proxy));
    return proxy->getProxyHandler().asObjectOrNull();
}

bool
JSScriptedProxyHandler::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                              PropertyDescriptor *desc)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return FundamentalTrap(cx, handler, ATOM(getPropertyDescriptor), tvr.addr()) &&
           Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), tvr.value()) &&
           ParsePropertyDescriptorObject(cx, proxy, id, tvr.value(), desc);
}

bool
JSScriptedProxyHandler::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                                 PropertyDescriptor *desc)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return FundamentalTrap(cx, handler, ATOM(getOwnPropertyDescriptor), tvr.addr()) &&
           Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(getPropertyDescriptor), tvr.value()) &&
           ParsePropertyDescriptorObject(cx, proxy, id, tvr.value(), desc);
}

bool
JSScriptedProxyHandler::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                       PropertyDescriptor *desc)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    AutoValueRooter fval(cx);
    return FundamentalTrap(cx, handler, ATOM(defineProperty), fval.addr()) &&
           MakePropertyDescriptorObject(cx, id, desc, tvr.addr()) &&
           Trap2(cx, handler, fval.value(), id, tvr.value(), tvr.addr());
}

bool
JSScriptedProxyHandler::getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return FundamentalTrap(cx, handler, ATOM(getOwnPropertyNames), tvr.addr()) &&
           Trap(cx, handler, tvr.value(), 0, NULL, tvr.addr()) &&
           ArrayToJSIdArray(cx, tvr.value(), idap);
}

bool
JSScriptedProxyHandler::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return FundamentalTrap(cx, handler, ATOM(delete), tvr.addr()) &&
           Trap1(cx, handler, tvr.value(), id, tvr.addr()) &&
           ValueToBool(cx, tvr.value(), bp);
}

bool
JSScriptedProxyHandler::enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    return FundamentalTrap(cx, handler, ATOM(enumerate), tvr.addr()) &&
           Trap(cx, handler, tvr.value(), 0, NULL, tvr.addr()) &&
           ArrayToJSIdArray(cx, tvr.value(), idap);
}

bool
JSScriptedProxyHandler::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    return FundamentalTrap(cx, handler, ATOM(fix), vp) &&
           Trap(cx, handler, *vp, 0, NULL, vp);
}

bool
JSScriptedProxyHandler::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!DerivedTrap(cx, handler, ATOM(has), tvr.addr()))
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
    if (!DerivedTrap(cx, handler, ATOM(hasOwn), tvr.addr()))
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
    AutoValueRooter tvr(cx, StringTag(str));
    Value argv[] = { ObjectOrNullTag(receiver), tvr.value() };
    AutoValueRooter fval(cx);
    if (!DerivedTrap(cx, handler, ATOM(get), fval.addr()))
        return false;
    if (!js_IsCallable(fval.value()))
        return JSProxyHandler::get(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval.value(), 2, argv, vp);
}

bool
JSScriptedProxyHandler::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    JSString *str = js_ValueToString(cx, IdToValue(id));
    if (!str)
        return false;
    AutoValueRooter tvr(cx, StringTag(str));
    Value argv[] = { ObjectOrNullTag(receiver), tvr.value(), *vp };
    AutoValueRooter fval(cx);
    if (!DerivedTrap(cx, handler, ATOM(set), fval.addr()))
        return false;
    if (!js_IsCallable(fval.value()))
        return JSProxyHandler::set(cx, proxy, receiver, id, vp);
    return Trap(cx, handler, fval.value(), 3, argv, tvr.addr());
}

bool
JSScriptedProxyHandler::enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!DerivedTrap(cx, handler, ATOM(enumerateOwn), tvr.addr()))
        return false;
    if (!js_IsCallable(tvr.value()))
        return JSProxyHandler::enumerateOwn(cx, proxy, idap);
    return Trap(cx, handler, tvr.value(), 0, NULL, tvr.addr()) &&
           ArrayToJSIdArray(cx, tvr.value(), idap);
}

bool
JSScriptedProxyHandler::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    JSObject *handler = GetProxyHandlerObject(cx, proxy);
    AutoValueRooter tvr(cx);
    if (!DerivedTrap(cx, handler, ATOM(iterate), tvr.addr()))
        return false;
    if (!js_IsCallable(tvr.value()))
        return JSProxyHandler::iterate(cx, proxy, flags, vp);
    return Trap(cx, handler, tvr.value(), 0, NULL, vp) &&
           ReturnedValueMustNotBePrimitive(cx, proxy, ATOM(iterate), *vp);
}

const void *
JSScriptedProxyHandler::family()
{
    return &singleton;
}

JSScriptedProxyHandler JSScriptedProxyHandler::singleton;

static JSProxyHandler *
JSVAL_TO_HANDLER(const Value &handler)
{
    return (JSProxyHandler *) handler.asPrivate();
}

class AutoPendingProxyOperation {
    JSThreadData *data;
    JSPendingProxyOperation op;
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
JSProxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.getPropertyDescriptor(cx, proxy, id, desc);
    return JSVAL_TO_HANDLER(handler)->getPropertyDescriptor(cx, proxy, id, desc);
}

bool
JSProxy::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, Value *vp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return JSProxy::getPropertyDescriptor(cx, proxy, id, &desc) &&
           MakePropertyDescriptorObject(cx, id, &desc, vp);
}

bool
JSProxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                  PropertyDescriptor *desc)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.getOwnPropertyDescriptor(cx, proxy, id, desc);
    return JSVAL_TO_HANDLER(handler)->getOwnPropertyDescriptor(cx, proxy, id, desc);
}

bool
JSProxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, Value *vp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return JSProxy::getOwnPropertyDescriptor(cx, proxy, id, &desc) &&
           MakePropertyDescriptorObject(cx, id, &desc, vp);
}

bool
JSProxy::defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.defineProperty(cx, proxy, id, desc);
    return JSVAL_TO_HANDLER(handler)->defineProperty(cx, proxy, id, desc);
}

bool
JSProxy::defineProperty(JSContext *cx, JSObject *proxy, jsid id, const Value &v)
{
    AutoPendingProxyOperation pending(cx, proxy);
    AutoPropertyDescriptorRooter desc(cx);
    return ParsePropertyDescriptorObject(cx, proxy, id, v, &desc) &&
           JSProxy::defineProperty(cx, proxy, id, &desc);
}

bool
JSProxy::getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.getOwnPropertyNames(cx, proxy, idap);
    return JSVAL_TO_HANDLER(handler)->getOwnPropertyNames(cx, proxy, idap);
}

bool
JSProxy::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.delete_(cx, proxy, id, bp);
    return JSVAL_TO_HANDLER(handler)->delete_(cx, proxy, id, bp);
}

bool
JSProxy::enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.enumerate(cx, proxy, idap);
    return JSVAL_TO_HANDLER(handler)->enumerate(cx, proxy, idap);
}

bool
JSProxy::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.fix(cx, proxy, vp);
    return JSVAL_TO_HANDLER(handler)->fix(cx, proxy, vp);
}

bool
JSProxy::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.has(cx, proxy, id, bp);
    return JSVAL_TO_HANDLER(handler)->has(cx, proxy, id, bp);
}

bool
JSProxy::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.hasOwn(cx, proxy, id, bp);
    return JSVAL_TO_HANDLER(handler)->hasOwn(cx, proxy, id, bp);
}

bool
JSProxy::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.get(cx, proxy, receiver, id, vp);
    return JSVAL_TO_HANDLER(handler)->get(cx, proxy, receiver, id, vp);
}

bool
JSProxy::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.set(cx, proxy, receiver, id, vp);
    return JSVAL_TO_HANDLER(handler)->set(cx, proxy, receiver, id, vp);
}

bool
JSProxy::enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.enumerateOwn(cx, proxy, idap);
    return JSVAL_TO_HANDLER(handler)->enumerateOwn(cx, proxy, idap);
}

bool
JSProxy::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    AutoPendingProxyOperation pending(cx, proxy);
    const Value &handler = proxy->getProxyHandler();
    if (handler.isObject())
        return JSScriptedProxyHandler::singleton.iterate(cx, proxy, flags, vp);
    return JSVAL_TO_HANDLER(handler)->iterate(cx, proxy, flags, vp);
}

JS_FRIEND_API(JSBool)
GetProxyObjectClass(JSContext *cx, JSObject *proxy, const char **namep)
{
    if (!proxy->isProxy()) {
        char *bytes = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK,
                                                 OBJECT_TO_JSVAL(proxy), NULL);
        if (!bytes)
            return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNEXPECTED_TYPE, bytes, "not a proxy");
        return false;
    }
    if (proxy->isFunctionProxy()) {
        *namep = "Function";
        return true;
    }
    const Value &nameval = proxy->fslots[JSSLOT_PROXY_CLASS];
    if (nameval.isUndefined()) {
        *namep ="Object";
        return true;
    }
    *namep = JS_GetStringBytesZ(cx, nameval.asString());
    return *namep != NULL;
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
                     PropertyOp getter, PropertyOp setter, uintN attrs)
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
proxy_GetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return JSProxy::get(cx, obj, obj, id, vp);
}

static JSBool
proxy_SetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return JSProxy::set(cx, obj, obj, id, vp);
}

static JSBool
proxy_GetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    AutoPropertyDescriptorRooter desc(cx);
    if (!JSProxy::getOwnPropertyDescriptor(cx, obj, id, &desc))
        return false;
    *attrsp = desc.attrs;
    return true;
}

static JSBool
proxy_SetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    /* Lookup the current property descriptor so we have setter/getter/value. */
    AutoPropertyDescriptorRooter desc(cx);
    if (!JSProxy::getOwnPropertyDescriptor(cx, obj, id, &desc))
        return false;
    desc.attrs = (*attrsp & (~JSPROP_SHORTID));
    return JSProxy::defineProperty(cx, obj, id, &desc);
}

static JSBool
proxy_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval)
{
    bool deleted;
    if (!JSProxy::delete_(cx, obj, id, &deleted))
        return false;
    rval->setBoolean(deleted);
    return true;
}

static void
proxy_TraceObject(JSTracer *trc, JSObject *obj)
{
    JSContext *cx = trc->context;

    if (!JS_CLIST_IS_EMPTY(&cx->runtime->watchPointList))
        js_TraceWatchPoints(trc, obj);

    Class *clasp = obj->getClass();
    if (clasp->mark) {
        if (clasp->flags & JSCLASS_MARK_IS_TRACE)
            ((JSTraceOp) clasp->mark)(trc, obj);
        else if (IS_GC_MARKING_TRACER(trc))
            (void) clasp->mark(cx, obj, trc);
    }

    obj->traceProtoAndParent(trc);

    const Value &handler = obj->fslots[JSSLOT_PROXY_HANDLER];
    if (handler.isObject())
        MarkObject(trc, &handler.asObject(), "handler");
    else
        JSVAL_TO_HANDLER(handler)->trace(trc, obj);
    if (obj->isFunctionProxy()) {
        MarkValue(trc, obj->fslots[JSSLOT_PROXY_CALL], "call");
        MarkValue(trc, obj->fslots[JSSLOT_PROXY_CONSTRUCT], "construct");
    } else {
        MarkValue(trc, obj->fslots[JSSLOT_PROXY_PRIVATE], "private");
    }
}

static JSType
proxy_TypeOf_obj(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

extern JSObjectOps js_ObjectProxyObjectOps;

static const JSObjectMap SharedObjectProxyMap(&js_ObjectProxyObjectOps, JSObjectMap::SHAPELESS);

JSObjectOps js_ObjectProxyObjectOps = {
    &SharedObjectProxyMap,
    proxy_LookupProperty,
    proxy_DefineProperty,
    proxy_GetProperty,
    proxy_SetProperty,
    proxy_GetAttributes,
    proxy_SetAttributes,
    proxy_DeleteProperty,
    js_DefaultValue,
    js_Enumerate,
    js_CheckAccess,
    proxy_TypeOf_obj,
    proxy_TraceObject,
    NULL,   /* thisObject */
    NULL,   /* call */
    NULL,   /* construct */
    js_HasInstance,
    NULL
};

static JSObjectOps *
obj_proxy_getObjectOps(JSContext *cx, Class *clasp)
{
    return &js_ObjectProxyObjectOps;
}

JS_FRIEND_API(Class) ObjectProxyClass = {
    "Proxy",
    JSCLASS_HAS_RESERVED_SLOTS(3),
    PropertyStub,           PropertyStub,    PropertyStub,    PropertyStub,
    EnumerateStub,          ResolveStub,     ConvertStub,     NULL,
    obj_proxy_getObjectOps, NULL,            NULL,            NULL,
    NULL,                   NULL,            NULL,            NULL
};

JSBool
proxy_Call(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval)
{
    JSObject *proxy = &argv[-2].asObject();
    JS_ASSERT(proxy->isProxy());
    AutoPendingProxyOperation pending(cx, proxy);
    return InternalCall(cx, obj, proxy->fslots[JSSLOT_PROXY_CALL], argc, argv, rval);
}

JSBool
proxy_Construct(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval)
{
    JSObject *proxy = &argv[-2].asObject();
    JS_ASSERT(proxy->isProxy());
    AutoPendingProxyOperation pending(cx, proxy);
    Value fval = proxy->fslots[JSSLOT_PROXY_CONSTRUCT];
    if (fval.isUndefined()) {
        /*
         * We don't have an explicit constructor trap so allocate a new
         * object and use the call trap.
         */
        fval = proxy->fslots[JSSLOT_PROXY_CALL];
        JS_ASSERT(fval.isObjectOrNull());

        /*
         * proxy is the constructor, so get proxy.prototype as the proto
         * of the new object.
         */
        if (!JSProxy::get(cx, proxy, obj, ATOM_TO_JSID(ATOM(classPrototype)), rval))
            return false;
        JSObject *proto = rval->isObject() ? &rval->asObject() : NULL;

        JSObject *newobj = NewObject(cx, &js_ObjectClass, proto, NULL);
        if (!newobj)
            return false;
        rval->setObject(*newobj);

        /* If the call returns an object, return that, otherwise the original newobj. */
        if (!InternalCall(cx, newobj, proxy->fslots[JSSLOT_PROXY_CALL], argc, argv, rval))
            return false;
        if (rval->isPrimitive())
            rval->setObject(*newobj);

        return true;
    }
    return InternalCall(cx, obj, fval, argc, argv, rval);
}

static JSType
proxy_TypeOf_fun(JSContext *cx, JSObject *obj)
{
    return JSTYPE_FUNCTION;
}

extern JSObjectOps js_FunctionProxyObjectOps;

static const JSObjectMap SharedFunctionProxyMap(&js_FunctionProxyObjectOps, JSObjectMap::SHAPELESS);

#define proxy_HasInstance js_FunctionClass.hasInstance

JSObjectOps js_FunctionProxyObjectOps = {
    &SharedFunctionProxyMap,
    proxy_LookupProperty,
    proxy_DefineProperty,
    proxy_GetProperty,
    proxy_SetProperty,
    proxy_GetAttributes,
    proxy_SetAttributes,
    proxy_DeleteProperty,
    js_DefaultValue,
    js_Enumerate,
    js_CheckAccess,
    proxy_TypeOf_fun,
    proxy_TraceObject,
    NULL,   /* thisObject */
    proxy_Call,
    proxy_Construct,
    proxy_HasInstance,
    NULL
};

static JSObjectOps *
fun_proxy_getObjectOps(JSContext *cx, Class *clasp)
{
    return &js_FunctionProxyObjectOps;
}

JS_FRIEND_API(Class) FunctionProxyClass = {
    "Proxy",
    JSCLASS_HAS_RESERVED_SLOTS(3),
    PropertyStub,           PropertyStub,    PropertyStub,    PropertyStub,
    EnumerateStub,          ResolveStub,     ConvertStub,     NULL,
    fun_proxy_getObjectOps, NULL,            NULL,            NULL,
    NULL,                   NULL,            NULL,            NULL
};

JS_FRIEND_API(JSObject *)
NewObjectProxy(JSContext *cx, const Value &handler, JSObject *proto, JSObject *parent, JSString *className)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &ObjectProxyClass, proto, parent);
    if (!obj)
        return NULL;
    obj->fslots[JSSLOT_PROXY_HANDLER] = handler;
    obj->fslots[JSSLOT_PROXY_CLASS] = className ? Value(StringTag(className)) : undefinedValue();
    obj->fslots[JSSLOT_PROXY_PRIVATE] = undefinedValue();
    return obj;
}

JS_FRIEND_API(JSObject *)
NewFunctionProxy(JSContext *cx, const Value &handler, JSObject *proto, JSObject *parent,
                 JSObject *call, JSObject *construct)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &FunctionProxyClass, proto, parent);
    if (!obj)
        return NULL;
    obj->fslots[JSSLOT_PROXY_HANDLER] = handler;
    obj->fslots[JSSLOT_PROXY_CALL] = call ? Value(ObjectTag(*call)) : undefinedValue();
    obj->fslots[JSSLOT_PROXY_CONSTRUCT] = construct ? Value(ObjectTag(*construct)) : undefinedValue();
    return obj;
}

static JSObject *
NonNullObject(JSContext *cx, const Value &v)
{
    if (v.isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return NULL;
    }
    return &v.asObject();
}

static JSBool
proxy_create(JSContext *cx, uintN argc, Value *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "create", "0", "s");
        return false;
    }
    JSObject *handler;
    if (!(handler = NonNullObject(cx, vp[2])))
        return false;
    JSObject *proto, *parent;
    if (argc > 1 && vp[3].isObject()) {
        proto = &vp[3].asObject();
        parent = proto->getParent();
    } else {
        JS_ASSERT(IsFunctionObject(vp[0]));
        proto = NULL;
        parent = vp[0].asObject().getParent();
    }
    JSString *className = (argc > 2 && vp[4].isString()) ? vp[4].asString() : NULL;
    JSObject *proxy = NewObjectProxy(cx, ObjectTag(*handler), proto, parent, className);
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
    JSObject *handler;
    if (!(handler = NonNullObject(cx, vp[2])))
        return false;
    JSObject *proto, *parent;
    parent = vp[0].asObject().getParent();
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

    JSObject *proxy = NewFunctionProxy(cx, ObjectTag(*handler), proto, parent,
                                       call, construct);
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
    JSObject *obj;
    if (!(obj = NonNullObject(cx, vp[2])))
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
    JSObject *obj;
    if (!(obj = NonNullObject(cx, vp[2])))
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

static const uint32 JSSLOT_CALLABLE_CALL = JSSLOT_PRIVATE;
static const uint32 JSSLOT_CALLABLE_CONSTRUCT = JSSLOT_PRIVATE + 1;

static JSBool
callable_Call(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval)
{
    JSObject *callable = &argv[-2].asObject();
    JS_ASSERT(callable->getClass() == &CallableObjectClass);
    const Value &fval = callable->fslots[JSSLOT_CALLABLE_CALL];
    return InternalCall(cx, obj, fval, argc, argv, rval);
}

static JSBool
callable_Construct(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval)
{
    JSObject *callable = &argv[-2].asObject();
    JS_ASSERT(callable->getClass() == &CallableObjectClass);
    Value fval = callable->fslots[JSSLOT_CALLABLE_CONSTRUCT];
    if (fval.isUndefined()) {
        /* We don't have an explicit constructor so allocate a new object and use the call. */
        fval = callable->fslots[JSSLOT_CALLABLE_CALL];
        JS_ASSERT(fval.isObject());

        /* callable is the constructor, so get callable.prototype is the proto of the new object. */
        if (!callable->getProperty(cx, ATOM_TO_JSID(ATOM(classPrototype)), rval))
            return false;
        JSObject *proto = rval->isObject() ? &rval->asObject() : NULL;

        JSObject *newobj = NewObject(cx, &js_ObjectClass, proto, NULL);
        if (!newobj)
            return false;
        rval->setObject(*newobj);

        /* If the call returns an object, return that, otherwise the original newobj. */
        if (!InternalCall(cx, newobj, callable->fslots[JSSLOT_CALLABLE_CALL], argc, argv, rval))
            return false;
        if (rval->isPrimitive())
            rval->setObject(*newobj);

        return true;
    }
    return InternalCall(cx, obj, fval, argc, argv, rval);
}

Class CallableObjectClass = {
    "Function",
    JSCLASS_HAS_RESERVED_SLOTS(2),
    PropertyStub,           PropertyStub,    PropertyStub,    PropertyStub,
    EnumerateStub,          ResolveStub,     ConvertStub,     NULL,
    NULL,                   NULL,            callable_Call,   callable_Construct,
    NULL,                   NULL,            NULL,            NULL
};

JS_FRIEND_API(JSBool)
FixProxy(JSContext *cx, JSObject *proxy, JSBool *bp)
{
    AutoValueRooter tvr(cx);
    if (!JSProxy::fix(cx, proxy, tvr.addr()))
        return false;
    if (tvr.value().isUndefined()) {
        *bp = false;
        return true;
    }

    if (OperationInProgress(cx, proxy)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROXY_FIX);
        return false;
    }

    JSObject *props;
    if (!(props = NonNullObject(cx, tvr.value())))
        return false;

    JSObject *proto = proxy->getProto();
    JSObject *parent = proxy->getParent();
    Class *clasp = proxy->isFunctionProxy() ? &CallableObjectClass : &js_ObjectClass;

    /* Make a blank object from the recipe fix provided to us. */
    JSObject *newborn = NewObjectWithGivenProto(cx, clasp, proto, parent);
    if (!newborn)
        return NULL;
    AutoObjectRooter tvr2(cx, newborn);

    if (clasp == &CallableObjectClass) {
        newborn->fslots[JSSLOT_CALLABLE_CALL] = proxy->fslots[JSSLOT_PROXY_CALL];
        newborn->fslots[JSSLOT_CALLABLE_CONSTRUCT] = proxy->fslots[JSSLOT_PROXY_CONSTRUCT];
    }

    {
        AutoPendingProxyOperation pending(cx, proxy);
        if (!js_PopulateObject(cx, newborn, props))
            return false;
    }

    /* Trade spaces between the newborn object and the proxy. */
    proxy->swap(newborn);

    /* The GC will dispose of the proxy object. */

    *bp = true;
    return true;
}

}

Class js_ProxyClass = {
    "Proxy",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Proxy),
    PropertyStub,           PropertyStub,    PropertyStub,    PropertyStub,
    EnumerateStub,          ResolveStub,     ConvertStub,     NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, JSObject *obj)
{
    JSObject *module = NewObject(cx, &js_ProxyClass, NULL, obj);
    if (!module)
        return NULL;
    if (!JS_DefineProperty(cx, obj, "Proxy", OBJECT_TO_JSVAL(module),
                           JS_PropertyStub, JS_PropertyStub, 0)) {
        return NULL;
    }
    if (!JS_DefineFunctions(cx, module, static_methods))
        return NULL;
    return obj;
}
