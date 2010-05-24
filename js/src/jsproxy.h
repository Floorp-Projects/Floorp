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

#ifndef jsproxy_h___
#define jsproxy_h___

#include "jsapi.h"
#include "jsobj.h"

/* Base class for all C++ proxy handlers. */
class JSProxyHandler {
  public:
    virtual ~JSProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc) = 0;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc) = 0;
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc) = 0;
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap) = 0;
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) = 0;
    virtual bool enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap) = 0;
    virtual bool fix(JSContext *cx, JSObject *proxy, jsval *vp) = 0;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    virtual bool enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap);

    /* Spidermonkey extensions. */
    virtual void finalize(JSContext *cx, JSObject *proxy);
    virtual void trace(JSTracer *trc, JSObject *proxy);
    virtual void *family() = 0;
};

/* No-op wrapper handler base class. */
class JSNoopProxyHandler {
    JSObject *mWrappedObject;

  protected:
    JS_FRIEND_API(JSNoopProxyHandler(JSObject *));

  public:
    JS_FRIEND_API(virtual ~JSNoopProxyHandler());

    /* ES5 Harmony fundamental proxy traps. */
    virtual JS_FRIEND_API(bool) getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) defineProperty(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    virtual JS_FRIEND_API(bool) delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    virtual JS_FRIEND_API(bool) fix(JSContext *cx, JSObject *proxy, jsval *vp);

    /* ES5 Harmony derived proxy traps. */
    virtual JS_FRIEND_API(bool) has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    virtual JS_FRIEND_API(bool) set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    virtual JS_FRIEND_API(bool) enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap);

    /* Spidermonkey extensions. */
    virtual JS_FRIEND_API(void) finalize(JSContext *cx, JSObject *proxy);
    virtual JS_FRIEND_API(void) trace(JSTracer *trc, JSObject *proxy);
    virtual JS_FRIEND_API(void) *family();

    static JSNoopProxyHandler singleton;

    template <class T>
    static JSObject *wrap(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent, JSString *className);

    inline JSObject *wrappedObject(JSObject *proxy) {
        return mWrappedObject ? mWrappedObject : JSVAL_TO_OBJECT(proxy->getProxyPrivate());
    }
};

/* Dispatch point for handlers that executes the appropriate C++ or scripted traps. */
class JSProxy {
  public:
    /* ES5 Harmony fundamental proxy traps. */
    static bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc);
    static bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, jsval *vp);
    static bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc);
    static bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, jsval *vp);
    static bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc);
    static bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, jsval v);
    static bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    static bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    static bool enumerate(JSContext *cx, JSObject *proxy, JSIdArray **idap);
    static bool fix(JSContext *cx, JSObject *proxy, jsval *vp);

    /* ES5 Harmony derived proxy traps. */
    static bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    static bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    static bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    static bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    static bool enumerateOwn(JSContext *cx, JSObject *proxy, JSIdArray **idap);
};

/* Shared between object and function proxies. */
const uint32 JSSLOT_PROXY_HANDLER = JSSLOT_PRIVATE + 0;
/* Object proxies only. */
const uint32 JSSLOT_PROXY_CLASS = JSSLOT_PRIVATE + 1;
const uint32 JSSLOT_PROXY_PRIVATE = JSSLOT_PRIVATE + 2;
/* Function proxies only. */
const uint32 JSSLOT_PROXY_CALL = JSSLOT_PRIVATE + 1;
const uint32 JSSLOT_PROXY_CONSTRUCT = JSSLOT_PRIVATE + 2;

extern JS_FRIEND_API(JSClass) js_ObjectProxyClass;
extern JS_FRIEND_API(JSClass) js_FunctionProxyClass;
extern JSClass js_CallableObjectClass;

inline bool
JSObject::isObjectProxy() const
{
    return getClass() == &js_ObjectProxyClass;
}

inline bool
JSObject::isFunctionProxy() const
{
    return getClass() == &js_FunctionProxyClass;
}

inline bool
JSObject::isProxy() const
{
    return isObjectProxy() || isFunctionProxy();
}

inline jsval
JSObject::getProxyHandler() const
{
    JS_ASSERT(isProxy());
    jsval handler = fslots[JSSLOT_PROXY_HANDLER];
    JS_ASSERT(JSVAL_IS_OBJECT(handler) || JSVAL_IS_INT(handler));
    return handler;
}

inline jsval
JSObject::getProxyPrivate() const
{
    JS_ASSERT(isObjectProxy());
    return fslots[JSSLOT_PROXY_PRIVATE];
}

inline void
JSObject::setProxyPrivate(jsval priv)
{
    JS_ASSERT(isObjectProxy());
    fslots[JSSLOT_PROXY_PRIVATE] = priv;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectProxy(JSContext *cx, jsval handler, JSObject *proto, JSObject *parent, JSString *className);

JS_PUBLIC_API(JSObject *)
JS_NewFunctionProxy(JSContext *cx, jsval handler, JSObject *proto, JSObject *parent, JSObject *call, JSObject *construct);

JS_PUBLIC_API(JSBool)
JS_GetProxyObjectClass(JSContext *cx, JSObject *proxy, const char **namep);

JS_PUBLIC_API(JSBool)
JS_FixProxy(JSContext *cx, JSObject *proxy, JSBool *bp);

JS_PUBLIC_API(JSBool)
JS_Becomes(JSContext *cx, JSObject *obj, JSObject *obj2);

template <class T>
JSObject *
JSNoopProxyHandler::wrap(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent, JSString *className)
{
    if (obj->isCallable()) {
        JSNoopProxyHandler *handler = new T(obj);
        if (!handler)
            return NULL;
        JSObject *wrapper = JS_NewFunctionProxy(cx, PRIVATE_TO_JSVAL(handler), proto, parent, obj, NULL);
        if (!wrapper)
            delete handler;
        return wrapper;
    }
    JSObject *wrapper = JS_NewObjectProxy(cx, PRIVATE_TO_JSVAL(&T::singleton), proto, parent, className);
    if (wrapper)
        wrapper->setProxyPrivate(OBJECT_TO_JSVAL(obj));
    return wrapper;
}

JS_BEGIN_EXTERN_C

extern JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, JSObject *obj);

JS_END_EXTERN_C

#endif
