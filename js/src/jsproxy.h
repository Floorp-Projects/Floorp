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
#include "jscntxt.h"
#include "jsobj.h"

namespace js {

/* Base class for all C++ proxy handlers. */
class JS_FRIEND_API(JSProxyHandler) {
    void *mFamily;
  public:
    explicit JSProxyHandler(void *family);
    virtual ~JSProxyHandler();

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                       PropertyDescriptor *desc) = 0;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                          PropertyDescriptor *desc) = 0;
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                PropertyDescriptor *desc) = 0;
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, js::AutoIdVector &props) = 0;
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) = 0;
    virtual bool enumerate(JSContext *cx, JSObject *proxy, js::AutoIdVector &props) = 0;
    virtual bool fix(JSContext *cx, JSObject *proxy, Value *vp) = 0;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, js::Value *vp);
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, js::Value *vp);
    virtual bool enumerateOwn(JSContext *cx, JSObject *proxy, js::AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *proxy, uintN flags, js::Value *vp);

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, JSObject *proxy, uintN argc, js::Value *vp);
    virtual bool construct(JSContext *cx, JSObject *proxy,
                                          uintN argc, js::Value *argv, js::Value *rval);
    virtual bool hasInstance(JSContext *cx, JSObject *proxy, const js::Value *vp, bool *bp);
    virtual JSString *obj_toString(JSContext *cx, JSObject *proxy);
    virtual JSString *fun_toString(JSContext *cx, JSObject *proxy, uintN indent);
    virtual void finalize(JSContext *cx, JSObject *proxy);
    virtual void trace(JSTracer *trc, JSObject *proxy);

    virtual bool isOuterWindow() {
        return false;
    }

    inline void *family() {
        return mFamily;
    }
};

/* Dispatch point for handlers that executes the appropriate C++ or scripted traps. */
class JSProxy {
  public:
    /* ES5 Harmony fundamental proxy traps. */
    static bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                      PropertyDescriptor *desc);
    static bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set, Value *vp);
    static bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                         PropertyDescriptor *desc);
    static bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                         Value *vp);
    static bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc);
    static bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, const Value &v);
    static bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, js::AutoIdVector &props);
    static bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    static bool enumerate(JSContext *cx, JSObject *proxy, js::AutoIdVector &props);
    static bool fix(JSContext *cx, JSObject *proxy, Value *vp);

    /* ES5 Harmony derived proxy traps. */
    static bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    static bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    static bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp);
    static bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp);
    static bool enumerateOwn(JSContext *cx, JSObject *proxy, js::AutoIdVector &props);
    static bool iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp);

    /* Spidermonkey extensions. */
    static bool call(JSContext *cx, JSObject *proxy, uintN argc, js::Value *vp);
    static bool construct(JSContext *cx, JSObject *proxy, uintN argc, js::Value *argv, js::Value *rval);
    static JSString *obj_toString(JSContext *cx, JSObject *proxy);
    static JSString *fun_toString(JSContext *cx, JSObject *proxy, uintN indent);
};

/* Shared between object and function proxies. */
const uint32 JSSLOT_PROXY_HANDLER = 0;
const uint32 JSSLOT_PROXY_PRIVATE = 1;
const uint32 JSSLOT_PROXY_EXTRA   = 2;
/* Function proxies only. */
const uint32 JSSLOT_PROXY_CALL = 3;
const uint32 JSSLOT_PROXY_CONSTRUCT = 4;

extern JS_FRIEND_API(js::Class) ObjectProxyClass;
extern JS_FRIEND_API(js::Class) FunctionProxyClass;
extern JS_FRIEND_API(js::Class) OuterWindowProxyClass;
extern js::Class CallableObjectClass;

}

inline bool
JSObject::isObjectProxy() const
{
    return getClass() == &js::ObjectProxyClass ||
           getClass() == &js::OuterWindowProxyClass;
}

inline bool
JSObject::isFunctionProxy() const
{
    return getClass() == &js::FunctionProxyClass;
}

inline bool
JSObject::isProxy() const
{
    return isObjectProxy() || isFunctionProxy();
}

inline js::JSProxyHandler *
JSObject::getProxyHandler() const
{
    JS_ASSERT(isProxy());
    return (js::JSProxyHandler *) getSlot(js::JSSLOT_PROXY_HANDLER).toPrivate();
}

inline const js::Value &
JSObject::getProxyPrivate() const
{
    JS_ASSERT(isProxy());
    return getSlot(js::JSSLOT_PROXY_PRIVATE);
}

inline void
JSObject::setProxyPrivate(const js::Value &priv)
{
    JS_ASSERT(isProxy());
    setSlot(js::JSSLOT_PROXY_PRIVATE, priv);
}

inline const js::Value &
JSObject::getProxyExtra() const
{
    JS_ASSERT(isProxy());
    return getSlot(js::JSSLOT_PROXY_EXTRA);
}

inline void
JSObject::setProxyExtra(const js::Value &extra)
{
    JS_ASSERT(isProxy());
    setSlot(js::JSSLOT_PROXY_EXTRA, extra);
}

namespace js {

JS_FRIEND_API(JSObject *)
NewProxyObject(JSContext *cx, JSProxyHandler *handler, const js::Value &priv,
               JSObject *proto, JSObject *parent,
               JSObject *call = NULL, JSObject *construct = NULL);

JS_FRIEND_API(JSBool)
FixProxy(JSContext *cx, JSObject *proxy, JSBool *bp);

}

JS_BEGIN_EXTERN_C

extern js::Class js_ProxyClass;

extern JS_FRIEND_API(JSObject *)
js_InitProxyClass(JSContext *cx, JSObject *obj);

JS_END_EXTERN_C

#endif
