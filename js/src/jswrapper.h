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

namespace js {

/* No-op wrapper handler base class. */
class JS_FRIEND_API(Wrapper) : public ProxyHandler
{
    uintN mFlags;
  public:
    uintN flags() const { return mFlags; }

    explicit Wrapper(uintN flags);

    typedef enum { PermitObjectAccess, PermitPropertyAccess, DenyAccess } Permission;

    virtual ~Wrapper();

    /* ES5 Harmony fundamental wrapper traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                       PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                          PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props);
    virtual bool fix(JSContext *cx, JSObject *wrapper, Value *vp);

    /* ES5 Harmony derived wrapper traps. */
    virtual bool has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp);
    virtual bool set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, bool strict,
                     Value *vp);
    virtual bool keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *wrapper, uintN flags, Value *vp);

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, JSObject *wrapper, uintN argc, Value *vp);
    virtual bool construct(JSContext *cx, JSObject *wrapper, uintN argc, Value *argv, Value *rval);
    virtual bool nativeCall(JSContext *cx, JSObject *wrapper, Class *clasp, Native native, CallArgs args);
    virtual bool hasInstance(JSContext *cx, JSObject *wrapper, const Value *vp, bool *bp);
    virtual JSType typeOf(JSContext *cx, JSObject *proxy);
    virtual bool objectClassIs(JSObject *obj, ESClassValue classValue, JSContext *cx);
    virtual JSString *obj_toString(JSContext *cx, JSObject *wrapper);
    virtual JSString *fun_toString(JSContext *cx, JSObject *wrapper, uintN indent);
    virtual bool defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp);

    virtual void trace(JSTracer *trc, JSObject *wrapper);

    /* Policy enforcement traps. */
    enum Action { GET, SET, CALL };
    virtual bool enter(JSContext *cx, JSObject *wrapper, jsid id, Action act, bool *bp);
    virtual void leave(JSContext *cx, JSObject *wrapper);

    static Wrapper singleton;

    static JSObject *New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
                         Wrapper *handler);

    static JSObject *wrappedObject(const JSObject *wrapper);
    static Wrapper *wrapperHandler(const JSObject *wrapper);

    enum {
        CROSS_COMPARTMENT = 1 << 0,
        LAST_USED_FLAG = CROSS_COMPARTMENT
    };

    static void *getWrapperFamily();
};

/* Base class for all cross compartment wrapper handlers. */
class JS_FRIEND_API(CrossCompartmentWrapper) : public Wrapper
{
  public:
    CrossCompartmentWrapper(uintN flags);

    virtual ~CrossCompartmentWrapper();

    /* ES5 Harmony fundamental wrapper traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                       PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                          PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props);

    /* ES5 Harmony derived wrapper traps. */
    virtual bool has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, Value *vp);
    virtual bool set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id, bool strict,
                     Value *vp);
    virtual bool keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *wrapper, uintN flags, Value *vp);

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, JSObject *wrapper, uintN argc, Value *vp);
    virtual bool construct(JSContext *cx, JSObject *wrapper, uintN argc, Value *argv, Value *rval);
    virtual bool nativeCall(JSContext *cx, JSObject *wrapper, Class *clasp, Native native, CallArgs args);
    virtual bool hasInstance(JSContext *cx, JSObject *wrapper, const Value *vp, bool *bp);
    virtual JSString *obj_toString(JSContext *cx, JSObject *wrapper);
    virtual JSString *fun_toString(JSContext *cx, JSObject *wrapper, uintN indent);
    virtual bool defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp);

    virtual void trace(JSTracer *trc, JSObject *wrapper);

    static CrossCompartmentWrapper singleton;
};

/*
 * A hacky class that lets a friend force a fake frame. We must already be
 * in the compartment of |target| when we enter the forced frame.
 */
class JS_FRIEND_API(ForceFrame)
{
  public:
    JSContext * const context;
    JSObject * const target;
  private:
    DummyFrameGuard *frame;

  public:
    ForceFrame(JSContext *cx, JSObject *target);
    ~ForceFrame();
    bool enter();
};

class AutoCompartment
{
  public:
    JSContext * const context;
    JSCompartment * const origin;
    JSObject * const target;
    JSCompartment * const destination;
  private:
    Maybe<DummyFrameGuard> frame;
    bool entered;

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

/*
 * Use this to change the behavior of an AutoCompartment slightly on error. If
 * the exception happens to be an Error object, copy it to the origin compartment
 * instead of wrapping it.
 */
class ErrorCopier
{
    AutoCompartment &ac;
    JSObject *scope;

  public:
    ErrorCopier(AutoCompartment &ac, JSObject *scope) : ac(ac), scope(scope) {
        JS_ASSERT(scope->compartment() == ac.origin);
    }
    ~ErrorCopier();
};

extern JSObject *
TransparentObjectWrapper(JSContext *cx, JSObject *obj, JSObject *wrappedProto, JSObject *parent,
                         uintN flags);

}

#endif
