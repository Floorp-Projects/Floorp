/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptBase_h__
#define mozilla_jsipc_JavaScriptBase_h__

#include "WrapperOwner.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/jsipc/PJavaScript.h"

namespace mozilla {
namespace jsipc {

template<class Base>
class JavaScriptBase : public WrapperOwner, public Base
{
    typedef WrapperOwner Shared;

  public:
    /*** Dummy call handlers ***/

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
};

} // namespace jsipc
} // namespace mozilla

#endif
