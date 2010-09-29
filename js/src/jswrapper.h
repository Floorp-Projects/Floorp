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

JS_BEGIN_EXTERN_C

/* No-op wrapper handler base class. */
class JSWrapper : public js::JSProxyHandler {
    uintN mFlags;
  protected:
    // XXX Hack to let wrappers derive from either cross compartment
    // wrappers or JSProxyHandlers.
    JS_FRIEND_API(JSWrapper(void *family));
  public:
    uintN flags() const { return mFlags; }

    explicit JS_FRIEND_API(JSWrapper(uintN flags));

    typedef enum { PermitObjectAccess, PermitPropertyAccess, DenyAccess } Permission;

    JS_FRIEND_API(virtual ~JSWrapper());

    /* ES5 Harmony fundamental wrapper traps. */
    virtual JS_FRIEND_API(bool) getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                      bool set, js::PropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                         bool set, js::PropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                               js::PropertyDescriptor *desc);
    virtual JS_FRIEND_API(bool) getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                                    js::AutoIdVector &props);
    virtual JS_FRIEND_API(bool) delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual JS_FRIEND_API(bool) fix(JSContext *cx, JSObject *wrapper, js::Value *vp);

    /* ES5 Harmony derived wrapper traps. */
    virtual JS_FRIEND_API(bool) has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual JS_FRIEND_API(bool) get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                                    js::Value *vp);
    virtual JS_FRIEND_API(bool) set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                                    js::Value *vp);
    virtual JS_FRIEND_API(bool) enumerateOwn(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual JS_FRIEND_API(bool) iterate(JSContext *cx, JSObject *wrapper, uintN flags, js::Value *vp);

    /* Spidermonkey extensions. */
    virtual JS_FRIEND_API(bool) call(JSContext *cx, JSObject *wrapper, uintN argc, js::Value *vp);
    virtual JS_FRIEND_API(bool) construct(JSContext *cx, JSObject *wrapper,
                                          uintN argc, js::Value *argv, js::Value *rval);
    virtual JS_FRIEND_API(JSString *) obj_toString(JSContext *cx, JSObject *wrapper);
    virtual JS_FRIEND_API(JSString *) fun_toString(JSContext *cx, JSObject *wrapper, uintN indent);

    virtual JS_FRIEND_API(void) trace(JSTracer *trc, JSObject *wrapper);

    /* Policy enforcement traps. */
    enum Action { GET, SET, CALL };
    virtual JS_FRIEND_API(bool) enter(JSContext *cx, JSObject *wrapper, jsid id, Action act);
    virtual JS_FRIEND_API(void) leave(JSContext *cx, JSObject *wrapper);

    static JS_FRIEND_API(JSWrapper) singleton;

    static JS_FRIEND_API(JSObject *) New(JSContext *cx, JSObject *obj,
                                         JSObject *proto, JSObject *parent,
                                         JSWrapper *handler);

    static inline JSObject *wrappedObject(JSObject *wrapper) {
        return wrapper->getProxyPrivate().toObjectOrNull();
    }

    static JS_FRIEND_API(void *) getWrapperFamily();
};

/* Base class for all cross compartment wrapper handlers. */
class JS_FRIEND_API(JSCrossCompartmentWrapper) : public JSWrapper {
  protected:
    // XXX Hack to let Xray wrappers derive from either cross compartment
    // wrappers or JSProxyHandlers.
    JSCrossCompartmentWrapper(void *family);

  public:
    JSCrossCompartmentWrapper(uintN flags);

    virtual ~JSCrossCompartmentWrapper();

    /* ES5 Harmony fundamental wrapper traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                       js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                          js::PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);

    /* ES5 Harmony derived wrapper traps. */
    virtual bool has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, js::Value *vp);
    virtual bool set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, js::Value *vp);
    virtual bool enumerateOwn(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *wrapper, uintN flags, js::Value *vp);

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, JSObject *wrapper, uintN argc, js::Value *vp);
    virtual bool construct(JSContext *cx, JSObject *wrapper,
                           uintN argc, js::Value *argv, js::Value *rval);
    virtual JSString *obj_toString(JSContext *cx, JSObject *wrapper);
    virtual JSString *fun_toString(JSContext *cx, JSObject *wrapper, uintN indent);

    static bool isCrossCompartmentWrapper(JSObject *obj);

    static JSCrossCompartmentWrapper singleton;
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
    LazilyConstructed<DummyFrameGuard> frame;
    JSFrameRegs regs;
    AutoStringRooter input;
    bool entered;
#ifdef DEBUG
    bool wasSane;
#endif

  public:
    AutoCompartment(JSContext *cx, JSObject *target);
    ~AutoCompartment();

    bool enter();
    void leave();

  private:
    // Prohibit copying.
    AutoCompartment(const AutoCompartment &);
    AutoCompartment & operator=(const AutoCompartment &);
};

extern JSObject *
TransparentObjectWrapper(JSContext *cx, JSObject *obj, JSObject *wrappedProto, JSObject *parent,
                         uintN flags);

}

JS_END_EXTERN_C

#endif
