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

#ifndef jswrapper_h___
#define jswrapper_h___

#include "jsapi.h"
#include "jsproxy.h"

/* No-op wrapper handler base class. */
class JSWrapper : public js::JSProxyHandler {
    void *mKind;
  protected:
    explicit JS_FRIEND_API(JSWrapper(void *kind));

  public:
    JS_FRIEND_API(virtual ~JSWrapper());

    /* ES5 Harmony fundamental proxy traps. */
    virtual JS_FRIEND_API(bool) getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                                      PropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                                         PropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                               PropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) getOwnPropertyNames(JSContext *cx, JSObject *proxy,
                                                    js::AutoValueVector &props);
    virtual JS_FRIEND_API(bool) delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) enumerate(JSContext *cx, JSObject *proxy, js::AutoValueVector &props);
    virtual JS_FRIEND_API(bool) fix(JSContext *cx, JSObject *proxy, Value *vp);

    /* ES5 Harmony derived proxy traps. */
    virtual JS_FRIEND_API(bool) has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id,
                                    Value *vp);
    virtual JS_FRIEND_API(bool) set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id,
                                    Value *vp);
    virtual JS_FRIEND_API(bool) enumerateOwn(JSContext *cx, JSObject *proxy, js::AutoValueVector &props);
    virtual JS_FRIEND_API(bool) iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp);

    /* Spidermonkey extensions. */
    virtual JS_FRIEND_API(void) trace(JSTracer *trc, JSObject *proxy);

    static JS_FRIEND_API(JSWrapper) singleton;

    static JS_FRIEND_API(JSObject *) New(JSContext *cx, JSObject *obj,
                                         JSObject *proto, JSObject *parent,
                                         JSProxyHandler *handler);

    inline void *kind() {
        return mKind;
    }

    static inline JSObject *wrappedObject(JSObject *proxy) {
        return JSVAL_TO_OBJECT(proxy->getProxyPrivate());
    }
};

/* Base class for all cross compartment wrapper handlers. */
class JSCrossCompartmentWrapper : public JSWrapper {
  protected:
    JS_FRIEND_API(JSCrossCompartmentWrapper());

  public:
    typedef enum { GET, SET } Mode;

    virtual JS_FRIEND_API(~JSCrossCompartmentWrapper());

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                       JSPropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                          JSPropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                                JSPropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, js::AutoValueVector &props);
    virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *proxy, js::AutoValueVector &props);

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, jsval *vp);
    virtual bool enumerateOwn(JSContext *cx, JSObject *proxy, js::AutoValueVector &props);
    virtual bool iterate(JSContext *cx, JSObject *proxy, uintN flags, jsval *vp);

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, JSObject *proxy, uintN argc, jsval *vp);
    virtual bool construct(JSContext *cx, JSObject *proxy, JSObject *receiver,
                           uintN argc, jsval *argv, jsval *rval);
    virtual JSString *obj_toString(JSContext *cx, JSObject *proxy);
    virtual JSString *fun_toString(JSContext *cx, JSObject *proxy, uintN indent);

    /* Policy enforcement traps. */
    virtual bool enter(JSContext *cx, JSObject *proxy, jsid id, Mode mode);
    virtual void leave(JSContext *cx, JSObject *proxy);

    static JSCrossCompartmentWrapper singleton;

    /* Default id used for filter when the trap signature does not contain an id. */
    static const jsid id = JSVAL_VOID;
};

namespace js {

class AutoCompartment
{
  public:
    JSContext * const context;
    JSCompartment * const origin;
    JSObject * const target;
    JSCompartment * const destination;
  private:
    LazilyConstructed<ExecuteFrameGuard> frame;
    JSFrameRegs regs;
    JSRegExpStatics statics;
    AutoValueRooter input;

  public:
    AutoCompartment(JSContext *cx, JSObject *target);
    ~AutoCompartment();

    bool entered() const { return context->compartment == destination; }
    bool enter();
    void leave();

    jsval *getvp() {
        JS_ASSERT(entered());
        return frame.ref().getvp();
    }

  private:
    // Prohibit copying.
    AutoCompartment(const AutoCompartment &);
    AutoCompartment & operator=(const AutoCompartment &);
};

extern JSObject *
TransparentObjectWrapper(JSContext *cx, JSObject *obj, JSObject *wrappedProto);

}

#endif
