/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_WrapperAnswer_h_
#define mozilla_jsipc_WrapperAnswer_h_

#include "JavaScriptShared.h"

namespace mozilla {
namespace jsipc {

class WrapperAnswer : public virtual JavaScriptShared
{
  public:
    explicit WrapperAnswer(JSRuntime *rt) : JavaScriptShared(rt) {}

    bool RecvPreventExtensions(const ObjectId &objId, ReturnStatus *rs);
    bool RecvGetPropertyDescriptor(const ObjectId &objId, const JSIDVariant &id,
                                   ReturnStatus *rs,
                                   PPropertyDescriptor *out);
    bool RecvGetOwnPropertyDescriptor(const ObjectId &objId,
                                      const JSIDVariant &id,
                                      ReturnStatus *rs,
                                      PPropertyDescriptor *out);
    bool RecvDefineProperty(const ObjectId &objId, const JSIDVariant &id,
                            const PPropertyDescriptor &flags,
                            ReturnStatus *rs);
    bool RecvDelete(const ObjectId &objId, const JSIDVariant &id,
                    ReturnStatus *rs, bool *success);

    bool RecvHas(const ObjectId &objId, const JSIDVariant &id,
                 ReturnStatus *rs, bool *bp);
    bool RecvHasOwn(const ObjectId &objId, const JSIDVariant &id,
                    ReturnStatus *rs, bool *bp);
    bool RecvGet(const ObjectId &objId, const ObjectVariant &receiverVar,
                 const JSIDVariant &id,
                 ReturnStatus *rs, JSVariant *result);
    bool RecvSet(const ObjectId &objId, const ObjectVariant &receiverVar,
                 const JSIDVariant &id, const bool &strict,
                 const JSVariant &value, ReturnStatus *rs, JSVariant *result);

    bool RecvIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                          bool *result);
    bool RecvCallOrConstruct(const ObjectId &objId, const nsTArray<JSParam> &argv,
                             const bool &construct, ReturnStatus *rs, JSVariant *result,
                             nsTArray<JSParam> *outparams);
    bool RecvHasInstance(const ObjectId &objId, const JSVariant &v, ReturnStatus *rs, bool *bp);
    bool RecvObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                           bool *result);
    bool RecvClassName(const ObjectId &objId, nsString *result);
    bool RecvRegExpToShared(const ObjectId &objId, ReturnStatus *rs, nsString *source, uint32_t *flags);

    bool RecvGetPropertyNames(const ObjectId &objId, const uint32_t &flags,
                              ReturnStatus *rs, nsTArray<nsString> *names);
    bool RecvInstanceOf(const ObjectId &objId, const JSIID &iid,
                        ReturnStatus *rs, bool *instanceof);
    bool RecvDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                           ReturnStatus *rs, bool *instanceof);

    bool RecvIsCallable(const ObjectId &objId, bool *result);
    bool RecvIsConstructor(const ObjectId &objId, bool *result);

    bool RecvDropObject(const ObjectId &objId);

  private:
    bool fail(JSContext *cx, ReturnStatus *rs);
    bool ok(ReturnStatus *rs);
};

} // mozilla
} // jsipc

#endif
