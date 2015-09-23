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

namespace dom {
class AutoJSAPI;
} // namespace dom

namespace jsipc {

class WrapperAnswer : public virtual JavaScriptShared
{
  public:
    explicit WrapperAnswer(JSRuntime* rt) : JavaScriptShared(rt) {}

    bool RecvPreventExtensions(const ObjectId& objId, ReturnStatus* rs);
    bool RecvGetPropertyDescriptor(const ObjectId& objId, const JSIDVariant& id,
                                   ReturnStatus* rs,
                                   PPropertyDescriptor* out);
    bool RecvGetOwnPropertyDescriptor(const ObjectId& objId,
                                      const JSIDVariant& id,
                                      ReturnStatus* rs,
                                      PPropertyDescriptor* out);
    bool RecvDefineProperty(const ObjectId& objId, const JSIDVariant& id,
                            const PPropertyDescriptor& flags, ReturnStatus* rs);
    bool RecvDelete(const ObjectId& objId, const JSIDVariant& id, ReturnStatus* rs);

    bool RecvHas(const ObjectId& objId, const JSIDVariant& id,
                 ReturnStatus* rs, bool* foundp);
    bool RecvHasOwn(const ObjectId& objId, const JSIDVariant& id,
                    ReturnStatus* rs, bool* foundp);
    bool RecvGet(const ObjectId& objId, const JSVariant& receiverVar,
                 const JSIDVariant& id,
                 ReturnStatus* rs, JSVariant* result);
    bool RecvSet(const ObjectId& objId, const JSIDVariant& id, const JSVariant& value,
                 const JSVariant& receiverVar, ReturnStatus* rs);

    bool RecvIsExtensible(const ObjectId& objId, ReturnStatus* rs,
                          bool* result);
    bool RecvCallOrConstruct(const ObjectId& objId, InfallibleTArray<JSParam>&& argv,
                             const bool& construct, ReturnStatus* rs, JSVariant* result,
                             nsTArray<JSParam>* outparams);
    bool RecvHasInstance(const ObjectId& objId, const JSVariant& v, ReturnStatus* rs, bool* bp);
    bool RecvGetBuiltinClass(const ObjectId& objId, ReturnStatus* rs,
                             uint32_t* classValue);
    bool RecvIsArray(const ObjectId& objId, ReturnStatus* rs, uint32_t* ans);
    bool RecvClassName(const ObjectId& objId, nsCString* result);
    bool RecvGetPrototype(const ObjectId& objId, ReturnStatus* rs, ObjectOrNullVariant* result);
    bool RecvRegExpToShared(const ObjectId& objId, ReturnStatus* rs, nsString* source, uint32_t* flags);

    bool RecvGetPropertyKeys(const ObjectId& objId, const uint32_t& flags,
                             ReturnStatus* rs, nsTArray<JSIDVariant>* ids);
    bool RecvInstanceOf(const ObjectId& objId, const JSIID& iid,
                        ReturnStatus* rs, bool* instanceof);
    bool RecvDOMInstanceOf(const ObjectId& objId, const int& prototypeID, const int& depth,
                           ReturnStatus* rs, bool* instanceof);

    bool RecvDropObject(const ObjectId& objId);

  private:
    bool fail(dom::AutoJSAPI& jsapi, ReturnStatus* rs);
    bool ok(ReturnStatus* rs);
    bool ok(ReturnStatus* rs, const JS::ObjectOpResult& result);
};

} // namespace jsipc
} // namespace mozilla

#endif
