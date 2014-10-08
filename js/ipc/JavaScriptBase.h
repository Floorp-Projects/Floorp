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

    bool AnswerPreventExtensions(const uint64_t &objId, ReturnStatus *rs) {
        return Answer::AnswerPreventExtensions(ObjectId::deserialize(objId), rs);
    }
    bool AnswerGetPropertyDescriptor(const uint64_t &objId, const JSIDVariant &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out) {
        return Answer::AnswerGetPropertyDescriptor(ObjectId::deserialize(objId), id, rs, out);
    }
    bool AnswerGetOwnPropertyDescriptor(const uint64_t &objId,
                                        const JSIDVariant &id,
                                        ReturnStatus *rs,
                                        PPropertyDescriptor *out) {
        return Answer::AnswerGetOwnPropertyDescriptor(ObjectId::deserialize(objId), id, rs, out);
    }
    bool AnswerDefineProperty(const uint64_t &objId, const JSIDVariant &id,
                              const PPropertyDescriptor &flags,
                              ReturnStatus *rs) {
        return Answer::AnswerDefineProperty(ObjectId::deserialize(objId), id, flags, rs);
    }
    bool AnswerDelete(const uint64_t &objId, const JSIDVariant &id,
                      ReturnStatus *rs, bool *success) {
        return Answer::AnswerDelete(ObjectId::deserialize(objId), id, rs, success);
    }

    bool AnswerHas(const uint64_t &objId, const JSIDVariant &id,
                   ReturnStatus *rs, bool *bp) {
        return Answer::AnswerHas(ObjectId::deserialize(objId), id, rs, bp);
    }
    bool AnswerHasOwn(const uint64_t &objId, const JSIDVariant &id,
                      ReturnStatus *rs, bool *bp) {
        return Answer::AnswerHasOwn(ObjectId::deserialize(objId), id, rs, bp);
    }
    bool AnswerGet(const uint64_t &objId, const ObjectVariant &receiverVar,
                   const JSIDVariant &id,
                   ReturnStatus *rs, JSVariant *result) {
        return Answer::AnswerGet(ObjectId::deserialize(objId), receiverVar, id, rs, result);
    }
    bool AnswerSet(const uint64_t &objId, const ObjectVariant &receiverVar,
                   const JSIDVariant &id, const bool &strict,
                   const JSVariant &value, ReturnStatus *rs, JSVariant *result) {
        return Answer::AnswerSet(ObjectId::deserialize(objId), receiverVar, id, strict, value, rs, result);
    }

    bool AnswerIsExtensible(const uint64_t &objId, ReturnStatus *rs,
                            bool *result) {
        return Answer::AnswerIsExtensible(ObjectId::deserialize(objId), rs, result);
    }
    bool AnswerCallOrConstruct(const uint64_t &objId, const nsTArray<JSParam> &argv,
                               const bool &construct, ReturnStatus *rs, JSVariant *result,
                               nsTArray<JSParam> *outparams) {
        return Answer::AnswerCallOrConstruct(ObjectId::deserialize(objId), argv, construct, rs, result, outparams);
    }
    bool AnswerHasInstance(const uint64_t &objId, const JSVariant &v, ReturnStatus *rs, bool *bp) {
        return Answer::AnswerHasInstance(ObjectId::deserialize(objId), v, rs, bp);
    }
    bool AnswerObjectClassIs(const uint64_t &objId, const uint32_t &classValue,
                             bool *result) {
        return Answer::AnswerObjectClassIs(ObjectId::deserialize(objId), classValue, result);
    }
    bool AnswerClassName(const uint64_t &objId, nsString *result) {
        return Answer::AnswerClassName(ObjectId::deserialize(objId), result);
    }
    bool AnswerRegExpToShared(const uint64_t &objId, ReturnStatus *rs, nsString *source, uint32_t *flags) {
        return Answer::AnswerRegExpToShared(ObjectId::deserialize(objId), rs, source, flags);
    }

    bool AnswerGetPropertyNames(const uint64_t &objId, const uint32_t &flags,
                                ReturnStatus *rs, nsTArray<nsString> *names) {
        return Answer::AnswerGetPropertyNames(ObjectId::deserialize(objId), flags, rs, names);
    }
    bool AnswerInstanceOf(const uint64_t &objId, const JSIID &iid,
                          ReturnStatus *rs, bool *instanceof) {
        return Answer::AnswerInstanceOf(ObjectId::deserialize(objId), iid, rs, instanceof);
    }
    bool AnswerDOMInstanceOf(const uint64_t &objId, const int &prototypeID, const int &depth,
                             ReturnStatus *rs, bool *instanceof) {
        return Answer::AnswerDOMInstanceOf(ObjectId::deserialize(objId), prototypeID, depth, rs, instanceof);
    }

    bool AnswerIsCallable(const uint64_t &objId, bool *result) {
        return Answer::AnswerIsCallable(ObjectId::deserialize(objId), result);
    }

    bool AnswerIsConstructor(const uint64_t &objId, bool *result) {
        return Answer::AnswerIsConstructor(ObjectId::deserialize(objId), result);
    }

    bool RecvDropObject(const uint64_t &objId) {
        return Answer::RecvDropObject(ObjectId::deserialize(objId));
    }

    /*** Dummy call handlers ***/

    bool SendDropObject(const ObjectId &objId) {
        return Base::SendDropObject(objId.serialize());
    }
    bool CallPreventExtensions(const ObjectId &objId, ReturnStatus *rs) {
        return Base::CallPreventExtensions(objId.serialize(), rs);
    }
    bool CallGetPropertyDescriptor(const ObjectId &objId, const JSIDVariant &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out) {
        return Base::CallGetPropertyDescriptor(objId.serialize(), id, rs, out);
    }
    bool CallGetOwnPropertyDescriptor(const ObjectId &objId,
                                      const JSIDVariant &id,
                                      ReturnStatus *rs,
                                      PPropertyDescriptor *out) {
        return Base::CallGetOwnPropertyDescriptor(objId.serialize(), id, rs, out);
    }
    bool CallDefineProperty(const ObjectId &objId, const JSIDVariant &id,
                            const PPropertyDescriptor &flags,
                              ReturnStatus *rs) {
        return Base::CallDefineProperty(objId.serialize(), id, flags, rs);
    }
    bool CallDelete(const ObjectId &objId, const JSIDVariant &id,
                    ReturnStatus *rs, bool *success) {
        return Base::CallDelete(objId.serialize(), id, rs, success);
    }

    bool CallHas(const ObjectId &objId, const JSIDVariant &id,
                   ReturnStatus *rs, bool *bp) {
        return Base::CallHas(objId.serialize(), id, rs, bp);
    }
    bool CallHasOwn(const ObjectId &objId, const JSIDVariant &id,
                    ReturnStatus *rs, bool *bp) {
        return Base::CallHasOwn(objId.serialize(), id, rs, bp);
    }
    bool CallGet(const ObjectId &objId, const ObjectVariant &receiverVar,
                 const JSIDVariant &id,
                 ReturnStatus *rs, JSVariant *result) {
        return Base::CallGet(objId.serialize(), receiverVar, id, rs, result);
    }
    bool CallSet(const ObjectId &objId, const ObjectVariant &receiverVar,
                 const JSIDVariant &id, const bool &strict,
                 const JSVariant &value, ReturnStatus *rs, JSVariant *result) {
        return Base::CallSet(objId.serialize(), receiverVar, id, strict, value, rs, result);
    }

    bool CallIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                          bool *result) {
        return Base::CallIsExtensible(objId.serialize(), rs, result);
    }
    bool CallCallOrConstruct(const ObjectId &objId, const nsTArray<JSParam> &argv,
                             const bool &construct, ReturnStatus *rs, JSVariant *result,
                             nsTArray<JSParam> *outparams) {
        return Base::CallCallOrConstruct(objId.serialize(), argv, construct, rs, result, outparams);
    }
    bool CallHasInstance(const ObjectId &objId, const JSVariant &v, ReturnStatus *rs, bool *bp) {
        return Base::CallHasInstance(objId.serialize(), v, rs, bp);
    }
    bool CallObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                           bool *result) {
        return Base::CallObjectClassIs(objId.serialize(), classValue, result);
    }
    bool CallClassName(const ObjectId &objId, nsString *result) {
        return Base::CallClassName(objId.serialize(), result);
    }

    bool CallRegExpToShared(const ObjectId &objId, ReturnStatus *rs,
                            nsString *source, uint32_t *flags) {
        return Base::CallRegExpToShared(objId.serialize(), rs, source, flags);
    }

    bool CallGetPropertyNames(const ObjectId &objId, const uint32_t &flags,
                              ReturnStatus *rs, nsTArray<nsString> *names) {
        return Base::CallGetPropertyNames(objId.serialize(), flags, rs, names);
    }
    bool CallInstanceOf(const ObjectId &objId, const JSIID &iid,
                        ReturnStatus *rs, bool *instanceof) {
        return Base::CallInstanceOf(objId.serialize(), iid, rs, instanceof);
    }
    bool CallDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                           ReturnStatus *rs, bool *instanceof) {
        return Base::CallDOMInstanceOf(objId.serialize(), prototypeID, depth, rs, instanceof);
    }

    bool CallIsCallable(const ObjectId &objId, bool *result) {
        return Base::CallIsCallable(objId.serialize(), result);
    }

    bool CallIsConstructor(const ObjectId &objId, bool *result) {
        return Base::CallIsConstructor(objId.serialize(), result);
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
