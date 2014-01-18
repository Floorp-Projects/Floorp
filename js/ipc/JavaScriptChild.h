/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptChild_h_
#define mozilla_jsipc_JavaScriptChild_h_

#include "JavaScriptShared.h"
#include "mozilla/jsipc/PJavaScriptChild.h"

namespace mozilla {
namespace jsipc {

class JavaScriptChild
  : public PJavaScriptChild,
    public JavaScriptShared
{
  public:
    JavaScriptChild(JSRuntime *rt);
    ~JavaScriptChild();

    bool init();
    void trace(JSTracer *trc);

    bool RecvDropObject(const ObjectId &objId) MOZ_OVERRIDE;

    bool AnswerPreventExtensions(const ObjectId &objId, ReturnStatus *rs) MOZ_OVERRIDE;
    bool AnswerGetPropertyDescriptor(const ObjectId &objId, const nsString &id,
                                     const uint32_t &flags, ReturnStatus *rs,
                                     PPropertyDescriptor *out) MOZ_OVERRIDE;
    bool AnswerGetOwnPropertyDescriptor(const ObjectId &objId,
                                        const nsString &id,
                                        const uint32_t &flags,
                                        ReturnStatus *rs,
                                        PPropertyDescriptor *out) MOZ_OVERRIDE;
    bool AnswerDefineProperty(const ObjectId &objId, const nsString &id,
                              const PPropertyDescriptor &flags,
                              ReturnStatus *rs) MOZ_OVERRIDE;
    bool AnswerDelete(const ObjectId &objId, const nsString &id,
                      ReturnStatus *rs, bool *success) MOZ_OVERRIDE;

    bool AnswerHas(const ObjectId &objId, const nsString &id,
                       ReturnStatus *rs, bool *bp) MOZ_OVERRIDE;
    bool AnswerHasOwn(const ObjectId &objId, const nsString &id,
                          ReturnStatus *rs, bool *bp) MOZ_OVERRIDE;
    bool AnswerGet(const ObjectId &objId, const ObjectId &receiverId,
                       const nsString &id,
                       ReturnStatus *rs, JSVariant *result) MOZ_OVERRIDE;
    bool AnswerSet(const ObjectId &objId, const ObjectId &receiverId,
                   const nsString &id, const bool &strict,
                   const JSVariant &value, ReturnStatus *rs, JSVariant *result) MOZ_OVERRIDE;

    bool AnswerIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                            bool *result) MOZ_OVERRIDE;
    bool AnswerCall(const ObjectId &objId, const nsTArray<JSParam> &argv,
                    ReturnStatus *rs, JSVariant *result,
                    nsTArray<JSParam> *outparams) MOZ_OVERRIDE;
    bool AnswerObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                             bool *result) MOZ_OVERRIDE;
    bool AnswerClassName(const ObjectId &objId, nsString *result) MOZ_OVERRIDE;

    bool AnswerGetPropertyNames(const ObjectId &objId, const uint32_t &flags,
                                ReturnStatus *rs, nsTArray<nsString> *names) MOZ_OVERRIDE;
    bool AnswerInstanceOf(const ObjectId &objId, const JSIID &iid,
                          ReturnStatus *rs, bool *instanceof) MOZ_OVERRIDE;
    bool AnswerDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                             ReturnStatus *rs, bool *instanceof) MOZ_OVERRIDE;

  protected:
    JSObject *unwrap(JSContext *cx, ObjectId id);

  private:
    bool makeId(JSContext *cx, JSObject *obj, ObjectId *idp);
    bool fail(JSContext *cx, ReturnStatus *rs);
    bool ok(ReturnStatus *rs);

  private:
    ObjectId lastId_;
    JSRuntime *rt_;
    ObjectIdCache ids_;
};

} // mozilla
} // jsipc

#endif
