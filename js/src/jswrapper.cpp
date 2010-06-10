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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "jsapi.h"
#include "jscntxt.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jswrapper.h"

#include "jsobjinlines.h"

using namespace js;

JSWrapper::JSWrapper(JSObject *obj) : mWrappedObject(obj)
{
}

JSWrapper::~JSWrapper()
{
}

bool
JSWrapper::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                          JSPropertyDescriptor *desc)
{
    JSObject *wobj = wrappedObject(proxy);
    return JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED, desc);
}

bool
JSWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                             JSPropertyDescriptor *desc)
{
    return JS_GetPropertyDescriptorById(cx, wrappedObject(proxy), id, JSRESOLVE_QUALIFIED, desc);
}

bool
JSWrapper::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                   JSPropertyDescriptor *desc)
{
    return JS_DefinePropertyById(cx, wrappedObject(proxy), id, desc->value,
                                 desc->getter, desc->setter, desc->attrs);
}

bool
JSWrapper::getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    return GetPropertyNames(cx, wrappedObject(proxy), JSITER_OWNONLY | JSITER_HIDDEN, idap);
}

bool
JSWrapper::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    AutoValueRooter tvr(cx);
    if (!JS_DeletePropertyById2(cx, wrappedObject(proxy), id, tvr.addr()))
        return false;
    *bp = js_ValueToBoolean(tvr.value());
    return true;
}

bool
JSWrapper::enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    return GetPropertyNames(cx, wrappedObject(proxy), 0, idap);
}

bool
JSWrapper::fix(JSContext *cx, JSObject *proxy, jsval *vp)
{
    *vp = JSVAL_VOID;
    return true;
}

bool
JSWrapper::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSBool found;
    if (!JS_HasPropertyById(cx, wrappedObject(proxy), id, &found))
        return false;
    *bp = !!found;
    return true;
}

bool
JSWrapper::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSPropertyDescriptor desc;
    JSObject *wobj = wrappedObject(proxy);
    if (!JS_GetPropertyDescriptorById(cx, wobj, id, JSRESOLVE_QUALIFIED, &desc))
        return false;
    *bp = (desc.obj == wobj);
    return true;
}

bool
JSWrapper::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp)
{
    return JS_GetPropertyById(cx, wrappedObject(proxy), id, vp);
}

bool
JSWrapper::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp)
{
    return JS_SetPropertyById(cx, wrappedObject(proxy), id, vp);
}

bool
JSWrapper::enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap)
{
    return GetPropertyNames(cx, wrappedObject(proxy), JSITER_OWNONLY, idap);
}

bool
JSWrapper::iterate(JSContext *cx, JSObject *proxy, uintN flags, jsval *vp)
{
    return GetIterator(cx, wrappedObject(proxy), flags, vp);
}

void
JSWrapper::finalize(JSContext *cx, JSObject *proxy)
{
    if (mWrappedObject)
        delete this;
}

void
JSWrapper::trace(JSTracer *trc, JSObject *proxy)
{
    if (mWrappedObject)
        JS_CALL_OBJECT_TRACER(trc, mWrappedObject, "wrappedObject");
}

const void *
JSWrapper::family()
{
    return &singleton;
}

JSWrapper JSWrapper::singleton(NULL);

JSObject *
JSWrapper::wrap(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent, JSString *className)
{
    JSObject *wobj;
    if (obj->isCallable()) {
        JSWrapper *handler = new JSWrapper(obj);
        if (!handler)
            return NULL;
        wobj = NewFunctionProxy(cx, PRIVATE_TO_JSVAL(handler), proto, parent, obj, NULL);
        if (!wobj) {
            delete handler;
            return NULL;
        }
    } else {
        wobj = NewObjectProxy(cx, PRIVATE_TO_JSVAL(&singleton), proto, parent, className);
        if (!wobj)
            return NULL;
        wobj->setProxyPrivate(OBJECT_TO_JSVAL(obj));
    }
    return wobj;
}
