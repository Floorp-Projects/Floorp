/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptParent__
#define mozilla_jsipc_JavaScriptParent__

#include "JavaScriptShared.h"
#include "mozilla/jsipc/PJavaScriptParent.h"
#include "jsclass.h"

#ifdef XP_WIN
#undef GetClassName
#undef GetClassInfo
#endif

namespace mozilla {
namespace jsipc {

class JavaScriptParent
  : public PJavaScriptParent,
    public JavaScriptShared
{
  public:
    JavaScriptParent();

    bool init();

  public:
    // Fundamental proxy traps. These are required.
    // (The traps should be in the same order like js/src/jsproxy.h)
    bool preventExtensions(JSContext *cx, JS::HandleObject proxy);
    bool getPropertyDescriptor(JSContext *cx, JS::HandleObject proxy, JS::HandleId id,
                               JS::MutableHandle<JSPropertyDescriptor> desc, unsigned flags);
    bool getOwnPropertyDescriptor(JSContext *cx, JS::HandleObject proxy, JS::HandleId id,
                                  JS::MutableHandle<JSPropertyDescriptor> desc, unsigned flags);
    bool defineProperty(JSContext *cx, JS::HandleObject proxy, JS::HandleId id,
                        JS::MutableHandle<JSPropertyDescriptor> desc);
    bool getOwnPropertyNames(JSContext *cx, JS::HandleObject proxy, js::AutoIdVector &props);
    bool delete_(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, bool *bp);
    bool enumerate(JSContext *cx, JS::HandleObject proxy, js::AutoIdVector &props);

    // Derived proxy traps. Implementing these is useful for perfomance.
    bool has(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, bool *bp);
    bool hasOwn(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, bool *bp);
    bool get(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
             JS::HandleId id, JS::MutableHandleValue vp);
    bool set(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
             JS::HandleId id, bool strict, JS::MutableHandleValue vp);
    bool keys(JSContext *cx, JS::HandleObject proxy, js::AutoIdVector &props);
    // We use "iterate" provided by the base class here.

    // SpiderMonkey Extensions.
    bool isExtensible(JSContext *cx, JS::HandleObject proxy, bool *extensible);
    bool call(JSContext *cx, JS::HandleObject proxy, const JS::CallArgs &args);
    bool objectClassIs(JSContext *cx, JS::HandleObject obj, js::ESClassValue classValue);
    const char* className(JSContext *cx, JS::HandleObject proxy);

    void decref();
    void incref();
    void destroyFromContent();

    void drop(JSObject *obj);

    static bool IsCPOW(JSObject *obj);

    static nsresult InstanceOf(JSObject *obj, const nsID *id, bool *bp);
    nsresult instanceOf(JSObject *obj, const nsID *id, bool *bp);

    /*
     * Check that |obj| is a DOM wrapper whose prototype chain contains
     * |prototypeID| at depth |depth|.
     */
    static bool DOMInstanceOf(JSObject *obj, int prototypeID, int depth, bool *bp);
    bool domInstanceOf(JSObject *obj, int prototypeID, int depth, bool *bp);

  protected:
    JSObject *unwrap(JSContext *cx, ObjectId objId);

  private:
    bool makeId(JSContext *cx, JSObject *obj, ObjectId *idp);
    bool getPropertyNames(JSContext *cx, JS::HandleObject proxy, uint32_t flags, js::AutoIdVector &props);
    ObjectId idOf(JSObject *obj);

    // Catastrophic IPC failure.
    bool ipcfail(JSContext *cx);

    // Check whether a return status is okay, and if not, propagate its error.
    bool ok(JSContext *cx, const ReturnStatus &status);

  private:
    uintptr_t refcount_;
    bool inactive_;
};

} // jsipc
} // mozilla

#endif // mozilla_jsipc_JavaScriptWrapper_h__

