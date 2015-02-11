/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_WrapperOwner_h__
#define mozilla_jsipc_WrapperOwner_h__

#include "JavaScriptShared.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "js/Class.h"
#include "js/Proxy.h"

namespace mozilla {
namespace jsipc {

class WrapperOwner : public virtual JavaScriptShared
{
  public:
    typedef mozilla::ipc::IProtocolManager<
                       mozilla::ipc::IProtocol>::ActorDestroyReason
           ActorDestroyReason;

    explicit WrapperOwner(JSRuntime *rt);
    bool init();

    // Standard internal methods.
    // (The traps should be in the same order like js/Proxy.h)
    bool getOwnPropertyDescriptor(JSContext *cx, JS::HandleObject proxy, JS::HandleId id,
                                  JS::MutableHandle<JSPropertyDescriptor> desc);
    bool defineProperty(JSContext *cx, JS::HandleObject proxy, JS::HandleId id,
                        JS::MutableHandle<JSPropertyDescriptor> desc);
    bool ownPropertyKeys(JSContext *cx, JS::HandleObject proxy, JS::AutoIdVector &props);
    bool delete_(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, bool *bp);
    bool preventExtensions(JSContext *cx, JS::HandleObject proxy, bool *succeeded);
    bool isExtensible(JSContext *cx, JS::HandleObject proxy, bool *extensible);
    bool has(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, bool *bp);
    bool get(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
             JS::HandleId id, JS::MutableHandleValue vp);
    bool set(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
             JS::HandleId id, bool strict, JS::MutableHandleValue vp);
    bool callOrConstruct(JSContext *cx, JS::HandleObject proxy, const JS::CallArgs &args,
                         bool construct);

    // SpiderMonkey extensions.
    bool getPropertyDescriptor(JSContext *cx, JS::HandleObject proxy, JS::HandleId id,
                               JS::MutableHandle<JSPropertyDescriptor> desc);
    bool hasOwn(JSContext *cx, JS::HandleObject proxy, JS::HandleId id, bool *bp);
    bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::HandleObject proxy,
                                      JS::AutoIdVector &props);
    bool hasInstance(JSContext *cx, JS::HandleObject proxy, JS::MutableHandleValue v, bool *bp);
    bool objectClassIs(JSContext *cx, JS::HandleObject obj, js::ESClassValue classValue);
    const char* className(JSContext *cx, JS::HandleObject proxy);
    bool getPrototypeOf(JSContext *cx, JS::HandleObject proxy, JS::MutableHandleObject protop);

    bool regexp_toShared(JSContext *cx, JS::HandleObject proxy, js::RegExpGuard *g);

    nsresult instanceOf(JSObject *obj, const nsID *id, bool *bp);

    bool toString(JSContext *cx, JS::HandleObject callee, JS::CallArgs &args);

    /*
     * Check that |obj| is a DOM wrapper whose prototype chain contains
     * |prototypeID| at depth |depth|.
     */
    bool domInstanceOf(JSContext *cx, JSObject *obj, int prototypeID, int depth, bool *bp);

    bool active() { return !inactive_; }

    void drop(JSObject *obj);
    void updatePointer(JSObject *obj, const JSObject *old);

    virtual void ActorDestroy(ActorDestroyReason why);

    virtual bool toObjectVariant(JSContext *cx, JSObject *obj, ObjectVariant *objVarp);
    virtual JSObject *fromObjectVariant(JSContext *cx, ObjectVariant objVar);
    JSObject *fromRemoteObjectVariant(JSContext *cx, RemoteObject objVar);
    JSObject *fromLocalObjectVariant(JSContext *cx, LocalObject objVar);

  protected:
    ObjectId idOf(JSObject *obj);

  private:
    ObjectId idOfUnchecked(JSObject *obj);

    bool getPropertyKeys(JSContext *cx, JS::HandleObject proxy, uint32_t flags,
                         JS::AutoIdVector &props);

    // Catastrophic IPC failure.
    bool ipcfail(JSContext *cx);

    // Check whether a return status is okay, and if not, propagate its error.
    bool ok(JSContext *cx, const ReturnStatus &status);

    bool inactive_;

    /*** Dummy call handlers ***/
  public:
    virtual bool SendDropObject(const ObjectId &objId) = 0;
    virtual bool SendPreventExtensions(const ObjectId &objId, ReturnStatus *rs,
                                       bool *succeeded) = 0;
    virtual bool SendGetPropertyDescriptor(const ObjectId &objId, const JSIDVariant &id,
                                           ReturnStatus *rs,
                                           PPropertyDescriptor *out) = 0;
    virtual bool SendGetOwnPropertyDescriptor(const ObjectId &objId,
                                              const JSIDVariant &id,
                                              ReturnStatus *rs,
                                              PPropertyDescriptor *out) = 0;
    virtual bool SendDefineProperty(const ObjectId &objId, const JSIDVariant &id,
                                    const PPropertyDescriptor &flags,
                                    ReturnStatus *rs) = 0;
    virtual bool SendDelete(const ObjectId &objId, const JSIDVariant &id,
                            ReturnStatus *rs, bool *success) = 0;

    virtual bool SendHas(const ObjectId &objId, const JSIDVariant &id,
                         ReturnStatus *rs, bool *bp) = 0;
    virtual bool SendHasOwn(const ObjectId &objId, const JSIDVariant &id,
                            ReturnStatus *rs, bool *bp) = 0;
    virtual bool SendGet(const ObjectId &objId, const ObjectVariant &receiverVar,
                         const JSIDVariant &id,
                         ReturnStatus *rs, JSVariant *result) = 0;
    virtual bool SendSet(const ObjectId &objId, const ObjectVariant &receiverVar,
                         const JSIDVariant &id, const bool &strict,
                         const JSVariant &value, ReturnStatus *rs, JSVariant *result) = 0;

    virtual bool SendIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                                  bool *result) = 0;
    virtual bool SendCallOrConstruct(const ObjectId &objId, const nsTArray<JSParam> &argv,
                                     const bool &construct, ReturnStatus *rs, JSVariant *result,
                                     nsTArray<JSParam> *outparams) = 0;
    virtual bool SendHasInstance(const ObjectId &objId, const JSVariant &v,
                                 ReturnStatus *rs, bool *bp) = 0;
    virtual bool SendObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                                   bool *result) = 0;
    virtual bool SendClassName(const ObjectId &objId, nsString *result) = 0;
    virtual bool SendGetPrototypeOf(const ObjectId &objId, ReturnStatus *rs, ObjectOrNullVariant *result) = 0;
    virtual bool SendRegExpToShared(const ObjectId &objId, ReturnStatus *rs, nsString *source,
                                    uint32_t *flags) = 0;

    virtual bool SendGetPropertyKeys(const ObjectId &objId, const uint32_t &flags,
                                     ReturnStatus *rs, nsTArray<JSIDVariant> *ids) = 0;
    virtual bool SendInstanceOf(const ObjectId &objId, const JSIID &iid,
                                ReturnStatus *rs, bool *instanceof) = 0;
    virtual bool SendDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                                   ReturnStatus *rs, bool *instanceof) = 0;
};

} // jsipc
} // mozilla

#endif // mozilla_jsipc_WrapperOwner_h__
