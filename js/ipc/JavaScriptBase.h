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
    JavaScriptBase(JSRuntime *rt)
      : JavaScriptShared(rt),
        WrapperOwner(rt),
        WrapperAnswer(rt)
    {}
    virtual ~JavaScriptBase() {}

    virtual void ActorDestroy(WrapperOwner::ActorDestroyReason why) {
        WrapperOwner::ActorDestroy(why);
    }

    /*** IPC handlers ***/

    bool AnswerPreventExtensions(const ObjectId &objId, ReturnStatus *rs) {
        return Answer::AnswerPreventExtensions(objId, rs);
    }
    bool AnswerGetPropertyDescriptor(const ObjectId &objId, const nsString &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out) {
        return Answer::AnswerGetPropertyDescriptor(objId, id, rs, out);
    }
    bool AnswerGetOwnPropertyDescriptor(const ObjectId &objId,
                                        const nsString &id,
                                        ReturnStatus *rs,
                                        PPropertyDescriptor *out) {
        return Answer::AnswerGetOwnPropertyDescriptor(objId, id, rs, out);
    }
    bool AnswerDefineProperty(const ObjectId &objId, const nsString &id,
                              const PPropertyDescriptor &flags,
                              ReturnStatus *rs) {
        return Answer::AnswerDefineProperty(objId, id, flags, rs);
    }
    bool AnswerDelete(const ObjectId &objId, const nsString &id,
                      ReturnStatus *rs, bool *success) {
        return Answer::AnswerDelete(objId, id, rs, success);
    }

    bool AnswerHas(const ObjectId &objId, const nsString &id,
                   ReturnStatus *rs, bool *bp) {
        return Answer::AnswerHas(objId, id, rs, bp);
    }
    bool AnswerHasOwn(const ObjectId &objId, const nsString &id,
                      ReturnStatus *rs, bool *bp) {
        return Answer::AnswerHasOwn(objId, id, rs, bp);
    }
    bool AnswerGet(const ObjectId &objId, const ObjectId &receiverId,
                   const nsString &id,
                   ReturnStatus *rs, JSVariant *result) {
        return Answer::AnswerGet(objId, receiverId, id, rs, result);
    }
    bool AnswerSet(const ObjectId &objId, const ObjectId &receiverId,
                   const nsString &id, const bool &strict,
                   const JSVariant &value, ReturnStatus *rs, JSVariant *result) {
        return Answer::AnswerSet(objId, receiverId, id, strict, value, rs, result);
    }

    bool AnswerIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                            bool *result) {
        return Answer::AnswerIsExtensible(objId, rs, result);
    }
    bool AnswerCall(const ObjectId &objId, const nsTArray<JSParam> &argv,
                    ReturnStatus *rs, JSVariant *result,
                    nsTArray<JSParam> *outparams) {
        return Answer::AnswerCall(objId, argv, rs, result, outparams);
    }
    bool AnswerObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                             bool *result) {
        return Answer::AnswerObjectClassIs(objId, classValue, result);
    }
    bool AnswerClassName(const ObjectId &objId, nsString *result) {
        return Answer::AnswerClassName(objId, result);
    }

    bool AnswerGetPropertyNames(const ObjectId &objId, const uint32_t &flags,
                                ReturnStatus *rs, nsTArray<nsString> *names) {
        return Answer::AnswerGetPropertyNames(objId, flags, rs, names);
    }
    bool AnswerInstanceOf(const ObjectId &objId, const JSIID &iid,
                          ReturnStatus *rs, bool *instanceof) {
        return Answer::AnswerInstanceOf(objId, iid, rs, instanceof);
    }
    bool AnswerDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                             ReturnStatus *rs, bool *instanceof) {
        return Answer::AnswerDOMInstanceOf(objId, prototypeID, depth, rs, instanceof);
    }

    bool RecvDropObject(const ObjectId &objId) {
        return Answer::RecvDropObject(objId);
    }

    /*** Dummy call handlers ***/

    bool SendDropObject(const ObjectId &objId) {
        return Base::SendDropObject(objId);
    }
    bool CallPreventExtensions(const ObjectId &objId, ReturnStatus *rs) {
        return Base::CallPreventExtensions(objId, rs);
    }
    bool CallGetPropertyDescriptor(const ObjectId &objId, const nsString &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out) {
        return Base::CallGetPropertyDescriptor(objId, id, rs, out);
    }
    bool CallGetOwnPropertyDescriptor(const ObjectId &objId,
                                      const nsString &id,
                                      ReturnStatus *rs,
                                      PPropertyDescriptor *out) {
        return Base::CallGetOwnPropertyDescriptor(objId, id, rs, out);
    }
    bool CallDefineProperty(const ObjectId &objId, const nsString &id,
                            const PPropertyDescriptor &flags,
                              ReturnStatus *rs) {
        return Base::CallDefineProperty(objId, id, flags, rs);
    }
    bool CallDelete(const ObjectId &objId, const nsString &id,
                    ReturnStatus *rs, bool *success) {
        return Base::CallDelete(objId, id, rs, success);
    }

    bool CallHas(const ObjectId &objId, const nsString &id,
                   ReturnStatus *rs, bool *bp) {
        return Base::CallHas(objId, id, rs, bp);
    }
    bool CallHasOwn(const ObjectId &objId, const nsString &id,
                    ReturnStatus *rs, bool *bp) {
        return Base::CallHasOwn(objId, id, rs, bp);
    }
    bool CallGet(const ObjectId &objId, const ObjectId &receiverId,
                 const nsString &id,
                 ReturnStatus *rs, JSVariant *result) {
        return Base::CallGet(objId, receiverId, id, rs, result);
    }
    bool CallSet(const ObjectId &objId, const ObjectId &receiverId,
                 const nsString &id, const bool &strict,
                 const JSVariant &value, ReturnStatus *rs, JSVariant *result) {
        return Base::CallSet(objId, receiverId, id, strict, value, rs, result);
    }

    bool CallIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                          bool *result) {
        return Base::CallIsExtensible(objId, rs, result);
    }
    bool CallCall(const ObjectId &objId, const nsTArray<JSParam> &argv,
                  ReturnStatus *rs, JSVariant *result,
                  nsTArray<JSParam> *outparams) {
        return Base::CallCall(objId, argv, rs, result, outparams);
    }
    bool CallObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                           bool *result) {
        return Base::CallObjectClassIs(objId, classValue, result);
    }
    bool CallClassName(const ObjectId &objId, nsString *result) {
        return Base::CallClassName(objId, result);
    }

    bool CallGetPropertyNames(const ObjectId &objId, const uint32_t &flags,
                              ReturnStatus *rs, nsTArray<nsString> *names) {
        return Base::CallGetPropertyNames(objId, flags, rs, names);
    }
    bool CallInstanceOf(const ObjectId &objId, const JSIID &iid,
                        ReturnStatus *rs, bool *instanceof) {
        return Base::CallInstanceOf(objId, iid, rs, instanceof);
    }
    bool CallDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                           ReturnStatus *rs, bool *instanceof) {
        return Base::CallDOMInstanceOf(objId, prototypeID, depth, rs, instanceof);
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
