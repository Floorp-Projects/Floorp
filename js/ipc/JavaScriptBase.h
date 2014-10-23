/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptBase_h__
#define mozilla_jsipc_JavaScriptBase_h__

#include "WrapperAnswer.h"
#include "WrapperOwner.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/jsipc/PJavaScript.h"

namespace mozilla {
namespace jsipc {

template<class Base>
class JavaScriptBase : public WrapperOwner, public WrapperAnswer, public Base
{
    typedef WrapperAnswer Answer;

  public:
    explicit JavaScriptBase(JSRuntime *rt)
      : JavaScriptShared(rt),
        WrapperOwner(rt),
        WrapperAnswer(rt)
    {}
    virtual ~JavaScriptBase() {}

    virtual void ActorDestroy(WrapperOwner::ActorDestroyReason why) {
        WrapperOwner::ActorDestroy(why);
    }

    /*** IPC handlers ***/

    bool RecvPreventExtensions(const uint64_t &objId, ReturnStatus *rs,
                               bool *succeeded) {
        return Answer::RecvPreventExtensions(ObjectId::deserialize(objId), rs,
                                             succeeded);
    }
    bool RecvGetPropertyDescriptor(const uint64_t &objId, const JSIDVariant &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out) {
        return Answer::RecvGetPropertyDescriptor(ObjectId::deserialize(objId), id, rs, out);
    }
    bool RecvGetOwnPropertyDescriptor(const uint64_t &objId,
                                        const JSIDVariant &id,
                                        ReturnStatus *rs,
                                        PPropertyDescriptor *out) {
        return Answer::RecvGetOwnPropertyDescriptor(ObjectId::deserialize(objId), id, rs, out);
    }
    bool RecvDefineProperty(const uint64_t &objId, const JSIDVariant &id,
                              const PPropertyDescriptor &flags,
                              ReturnStatus *rs) {
        return Answer::RecvDefineProperty(ObjectId::deserialize(objId), id, flags, rs);
    }
    bool RecvDelete(const uint64_t &objId, const JSIDVariant &id,
                      ReturnStatus *rs, bool *success) {
        return Answer::RecvDelete(ObjectId::deserialize(objId), id, rs, success);
    }

    bool RecvHas(const uint64_t &objId, const JSIDVariant &id,
                   ReturnStatus *rs, bool *bp) {
        return Answer::RecvHas(ObjectId::deserialize(objId), id, rs, bp);
    }
    bool RecvHasOwn(const uint64_t &objId, const JSIDVariant &id,
                      ReturnStatus *rs, bool *bp) {
        return Answer::RecvHasOwn(ObjectId::deserialize(objId), id, rs, bp);
    }
    bool RecvGet(const uint64_t &objId, const ObjectVariant &receiverVar,
                   const JSIDVariant &id,
                   ReturnStatus *rs, JSVariant *result) {
        return Answer::RecvGet(ObjectId::deserialize(objId), receiverVar, id, rs, result);
    }
    bool RecvSet(const uint64_t &objId, const ObjectVariant &receiverVar,
                   const JSIDVariant &id, const bool &strict,
                   const JSVariant &value, ReturnStatus *rs, JSVariant *result) {
        return Answer::RecvSet(ObjectId::deserialize(objId), receiverVar, id, strict, value, rs, result);
    }

    bool RecvIsExtensible(const uint64_t &objId, ReturnStatus *rs,
                            bool *result) {
        return Answer::RecvIsExtensible(ObjectId::deserialize(objId), rs, result);
    }
    bool RecvCallOrConstruct(const uint64_t &objId, const nsTArray<JSParam> &argv,
                               const bool &construct, ReturnStatus *rs, JSVariant *result,
                               nsTArray<JSParam> *outparams) {
        return Answer::RecvCallOrConstruct(ObjectId::deserialize(objId), argv, construct, rs, result, outparams);
    }
    bool RecvHasInstance(const uint64_t &objId, const JSVariant &v, ReturnStatus *rs, bool *bp) {
        return Answer::RecvHasInstance(ObjectId::deserialize(objId), v, rs, bp);
    }
    bool RecvObjectClassIs(const uint64_t &objId, const uint32_t &classValue,
                             bool *result) {
        return Answer::RecvObjectClassIs(ObjectId::deserialize(objId), classValue, result);
    }
    bool RecvClassName(const uint64_t &objId, nsString *result) {
        return Answer::RecvClassName(ObjectId::deserialize(objId), result);
    }
    bool RecvRegExpToShared(const uint64_t &objId, ReturnStatus *rs, nsString *source, uint32_t *flags) {
        return Answer::RecvRegExpToShared(ObjectId::deserialize(objId), rs, source, flags);
    }

    bool RecvGetPropertyKeys(const uint64_t &objId, const uint32_t &flags,
                             ReturnStatus *rs, nsTArray<JSIDVariant> *ids) {
        return Answer::RecvGetPropertyKeys(ObjectId::deserialize(objId), flags, rs, ids);
    }
    bool RecvInstanceOf(const uint64_t &objId, const JSIID &iid,
                          ReturnStatus *rs, bool *instanceof) {
        return Answer::RecvInstanceOf(ObjectId::deserialize(objId), iid, rs, instanceof);
    }
    bool RecvDOMInstanceOf(const uint64_t &objId, const int &prototypeID, const int &depth,
                             ReturnStatus *rs, bool *instanceof) {
        return Answer::RecvDOMInstanceOf(ObjectId::deserialize(objId), prototypeID, depth, rs, instanceof);
    }

    bool RecvIsCallable(const uint64_t &objId, bool *result) {
        return Answer::RecvIsCallable(ObjectId::deserialize(objId), result);
    }

    bool RecvIsConstructor(const uint64_t &objId, bool *result) {
        return Answer::RecvIsConstructor(ObjectId::deserialize(objId), result);
    }

    bool RecvDropObject(const uint64_t &objId) {
        return Answer::RecvDropObject(ObjectId::deserialize(objId));
    }

    /*** Dummy call handlers ***/

    bool SendDropObject(const ObjectId &objId) {
        return Base::SendDropObject(objId.serialize());
    }
    bool SendPreventExtensions(const ObjectId &objId, ReturnStatus *rs,
                               bool *succeeded) {
        return Base::SendPreventExtensions(objId.serialize(), rs, succeeded);
    }
    bool SendGetPropertyDescriptor(const ObjectId &objId, const JSIDVariant &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out) {
        return Base::SendGetPropertyDescriptor(objId.serialize(), id, rs, out);
    }
    bool SendGetOwnPropertyDescriptor(const ObjectId &objId,
                                      const JSIDVariant &id,
                                      ReturnStatus *rs,
                                      PPropertyDescriptor *out) {
        return Base::SendGetOwnPropertyDescriptor(objId.serialize(), id, rs, out);
    }
    bool SendDefineProperty(const ObjectId &objId, const JSIDVariant &id,
                            const PPropertyDescriptor &flags,
                              ReturnStatus *rs) {
        return Base::SendDefineProperty(objId.serialize(), id, flags, rs);
    }
    bool SendDelete(const ObjectId &objId, const JSIDVariant &id,
                    ReturnStatus *rs, bool *success) {
        return Base::SendDelete(objId.serialize(), id, rs, success);
    }

    bool SendHas(const ObjectId &objId, const JSIDVariant &id,
                   ReturnStatus *rs, bool *bp) {
        return Base::SendHas(objId.serialize(), id, rs, bp);
    }
    bool SendHasOwn(const ObjectId &objId, const JSIDVariant &id,
                    ReturnStatus *rs, bool *bp) {
        return Base::SendHasOwn(objId.serialize(), id, rs, bp);
    }
    bool SendGet(const ObjectId &objId, const ObjectVariant &receiverVar,
                 const JSIDVariant &id,
                 ReturnStatus *rs, JSVariant *result) {
        return Base::SendGet(objId.serialize(), receiverVar, id, rs, result);
    }
    bool SendSet(const ObjectId &objId, const ObjectVariant &receiverVar,
                 const JSIDVariant &id, const bool &strict,
                 const JSVariant &value, ReturnStatus *rs, JSVariant *result) {
        return Base::SendSet(objId.serialize(), receiverVar, id, strict, value, rs, result);
    }

    bool SendIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                          bool *result) {
        return Base::SendIsExtensible(objId.serialize(), rs, result);
    }
    bool SendCallOrConstruct(const ObjectId &objId, const nsTArray<JSParam> &argv,
                             const bool &construct, ReturnStatus *rs, JSVariant *result,
                             nsTArray<JSParam> *outparams) {
        return Base::SendCallOrConstruct(objId.serialize(), argv, construct, rs, result, outparams);
    }
    bool SendHasInstance(const ObjectId &objId, const JSVariant &v, ReturnStatus *rs, bool *bp) {
        return Base::SendHasInstance(objId.serialize(), v, rs, bp);
    }
    bool SendObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                           bool *result) {
        return Base::SendObjectClassIs(objId.serialize(), classValue, result);
    }
    bool SendClassName(const ObjectId &objId, nsString *result) {
        return Base::SendClassName(objId.serialize(), result);
    }

    bool SendRegExpToShared(const ObjectId &objId, ReturnStatus *rs,
                            nsString *source, uint32_t *flags) {
        return Base::SendRegExpToShared(objId.serialize(), rs, source, flags);
    }

    bool SendGetPropertyKeys(const ObjectId &objId, const uint32_t &flags,
                             ReturnStatus *rs, nsTArray<JSIDVariant> *ids) {
        return Base::SendGetPropertyKeys(objId.serialize(), flags, rs, ids);
    }
    bool SendInstanceOf(const ObjectId &objId, const JSIID &iid,
                        ReturnStatus *rs, bool *instanceof) {
        return Base::SendInstanceOf(objId.serialize(), iid, rs, instanceof);
    }
    bool SendDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                           ReturnStatus *rs, bool *instanceof) {
        return Base::SendDOMInstanceOf(objId.serialize(), prototypeID, depth, rs, instanceof);
    }

    bool SendIsCallable(const ObjectId &objId, bool *result) {
        return Base::SendIsCallable(objId.serialize(), result);
    }

    bool SendIsConstructor(const ObjectId &objId, bool *result) {
        return Base::SendIsConstructor(objId.serialize(), result);
    }

    /* The following code is needed to suppress a bogus MSVC warning (C4250). */

    virtual bool toObjectVariant(JSContext *cx, JSObject *obj, ObjectVariant *objVarp) {
        return WrapperOwner::toObjectVariant(cx, obj, objVarp);
    }
    virtual JSObject *fromObjectVariant(JSContext *cx, ObjectVariant objVar) {
        return WrapperOwner::fromObjectVariant(cx, objVar);
    }
};

} // namespace jsipc
} // namespace mozilla

#endif
