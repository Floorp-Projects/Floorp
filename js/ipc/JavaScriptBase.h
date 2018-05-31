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
    virtual ~JavaScriptBase() {}

    virtual void ActorDestroy(WrapperOwner::ActorDestroyReason why) override {
        WrapperOwner::ActorDestroy(why);
    }

    /*** IPC handlers ***/

    mozilla::ipc::IPCResult RecvPreventExtensions(const uint64_t& objId, ReturnStatus* rs) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvPreventExtensions(obj.value(), rs)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvGetPropertyDescriptor(const uint64_t& objId, const JSIDVariant& id,
                                                      ReturnStatus* rs,
                                                      PPropertyDescriptor* out) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGetPropertyDescriptor(obj.value(), id, rs, out)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvGetOwnPropertyDescriptor(const uint64_t& objId,
                                                         const JSIDVariant& id,
                                                         ReturnStatus* rs,
                                                         PPropertyDescriptor* out) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGetOwnPropertyDescriptor(obj.value(), id, rs, out)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvDefineProperty(const uint64_t& objId, const JSIDVariant& id,
                                               const PPropertyDescriptor& flags, ReturnStatus* rs) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvDefineProperty(obj.value(), id, flags, rs)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvDelete(const uint64_t& objId, const JSIDVariant& id,
                                       ReturnStatus* rs) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvDelete(obj.value(), id, rs)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }

    mozilla::ipc::IPCResult RecvHas(const uint64_t& objId, const JSIDVariant& id,
                                    ReturnStatus* rs, bool* bp) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvHas(obj.value(), id, rs, bp)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvHasOwn(const uint64_t& objId, const JSIDVariant& id,
                                       ReturnStatus* rs, bool* bp) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvHasOwn(obj.value(), id, rs, bp)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvGet(const uint64_t& objId, const JSVariant& receiverVar, const JSIDVariant& id,
                                    ReturnStatus* rs, JSVariant* result) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGet(obj.value(), receiverVar, id, rs, result)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvSet(const uint64_t& objId, const JSIDVariant& id, const JSVariant& value,
                                    const JSVariant& receiverVar, ReturnStatus* rs) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvSet(obj.value(), id, value, receiverVar, rs)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }

    mozilla::ipc::IPCResult RecvIsExtensible(const uint64_t& objId, ReturnStatus* rs,
                                             bool* result) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvIsExtensible(obj.value(), rs, result)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvCallOrConstruct(const uint64_t& objId, InfallibleTArray<JSParam>&& argv,
                                                const bool& construct, ReturnStatus* rs, JSVariant* result,
                                                nsTArray<JSParam>* outparams) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvCallOrConstruct(obj.value(), std::move(argv), construct, rs, result, outparams)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvHasInstance(const uint64_t& objId, const JSVariant& v, ReturnStatus* rs, bool* bp) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvHasInstance(obj.value(), v, rs, bp)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvGetBuiltinClass(const uint64_t& objId, ReturnStatus* rs, uint32_t* classValue) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGetBuiltinClass(obj.value(), rs, classValue)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvIsArray(const uint64_t& objId, ReturnStatus* rs, uint32_t* answer) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvIsArray(obj.value(), rs, answer)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvClassName(const uint64_t& objId, nsCString* result) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvClassName(obj.value(), result)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvGetPrototype(const uint64_t& objId, ReturnStatus* rs, ObjectOrNullVariant* result) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGetPrototype(obj.value(), rs, result)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvGetPrototypeIfOrdinary(const uint64_t& objId, ReturnStatus* rs, bool* isOrdinary,
                                                       ObjectOrNullVariant* result) override
    {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGetPrototypeIfOrdinary(obj.value(), rs, isOrdinary, result)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvRegExpToShared(const uint64_t& objId, ReturnStatus* rs, nsString* source, uint32_t* flags) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvRegExpToShared(obj.value(), rs, source, flags)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }

    mozilla::ipc::IPCResult RecvGetPropertyKeys(const uint64_t& objId, const uint32_t& flags,
                                                ReturnStatus* rs, nsTArray<JSIDVariant>* ids) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvGetPropertyKeys(obj.value(), flags, rs, ids)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvInstanceOf(const uint64_t& objId, const JSIID& iid,
                                           ReturnStatus* rs, bool* instanceof) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvInstanceOf(obj.value(), iid, rs, instanceof)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }
    mozilla::ipc::IPCResult RecvDOMInstanceOf(const uint64_t& objId, const int& prototypeID, const int& depth,
                                              ReturnStatus* rs, bool* instanceof) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvDOMInstanceOf(obj.value(), prototypeID, depth, rs, instanceof)) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }

    mozilla::ipc::IPCResult RecvDropObject(const uint64_t& objId) override {
        Maybe<ObjectId> obj(ObjectId::deserialize(objId));
        if (obj.isNothing() || !Answer::RecvDropObject(obj.value())) {
            return IPC_FAIL_NO_REASON(this);
        }
        return IPC_OK();
    }

    /*** Dummy call handlers ***/

    bool SendDropObject(const ObjectId& objId) override {
        return Base::SendDropObject(objId.serialize());
    }
    bool SendPreventExtensions(const ObjectId& objId, ReturnStatus* rs) override {
        return Base::SendPreventExtensions(objId.serialize(), rs);
    }
    bool SendGetPropertyDescriptor(const ObjectId& objId, const JSIDVariant& id,
                                   ReturnStatus* rs,
                                   PPropertyDescriptor* out) override {
        return Base::SendGetPropertyDescriptor(objId.serialize(), id, rs, out);
    }
    bool SendGetOwnPropertyDescriptor(const ObjectId& objId,
                                      const JSIDVariant& id,
                                      ReturnStatus* rs,
                                      PPropertyDescriptor* out) override {
        return Base::SendGetOwnPropertyDescriptor(objId.serialize(), id, rs, out);
    }
    bool SendDefineProperty(const ObjectId& objId, const JSIDVariant& id,
                            const PPropertyDescriptor& flags,
                            ReturnStatus* rs) override {
        return Base::SendDefineProperty(objId.serialize(), id, flags, rs);
    }
    bool SendDelete(const ObjectId& objId, const JSIDVariant& id, ReturnStatus* rs) override {
        return Base::SendDelete(objId.serialize(), id, rs);
    }

    bool SendHas(const ObjectId& objId, const JSIDVariant& id,
                 ReturnStatus* rs, bool* bp) override {
        return Base::SendHas(objId.serialize(), id, rs, bp);
    }
    bool SendHasOwn(const ObjectId& objId, const JSIDVariant& id,
                    ReturnStatus* rs, bool* bp) override {
        return Base::SendHasOwn(objId.serialize(), id, rs, bp);
    }
    bool SendGet(const ObjectId& objId, const JSVariant& receiverVar, const JSIDVariant& id,
                 ReturnStatus* rs, JSVariant* result) override {
        return Base::SendGet(objId.serialize(), receiverVar, id, rs, result);
    }
    bool SendSet(const ObjectId& objId, const JSIDVariant& id, const JSVariant& value,
                 const JSVariant& receiverVar, ReturnStatus* rs) override {
        return Base::SendSet(objId.serialize(), id, value, receiverVar, rs);
    }

    bool SendIsExtensible(const ObjectId& objId, ReturnStatus* rs, bool* result) override {
        return Base::SendIsExtensible(objId.serialize(), rs, result);
    }
    bool SendCallOrConstruct(const ObjectId& objId, const nsTArray<JSParam>& argv,
                             const bool& construct, ReturnStatus* rs, JSVariant* result,
                             nsTArray<JSParam>* outparams) override {
        return Base::SendCallOrConstruct(objId.serialize(), argv, construct, rs, result, outparams);
    }
    bool SendHasInstance(const ObjectId& objId, const JSVariant& v, ReturnStatus* rs, bool* bp) override {
        return Base::SendHasInstance(objId.serialize(), v, rs, bp);
    }
    bool SendGetBuiltinClass(const ObjectId& objId, ReturnStatus* rs, uint32_t* classValue) override {
        return Base::SendGetBuiltinClass(objId.serialize(), rs, classValue);
    }
    bool SendIsArray(const ObjectId& objId, ReturnStatus* rs, uint32_t* answer) override
    {
        return Base::SendIsArray(objId.serialize(), rs, answer);
    }
    bool SendClassName(const ObjectId& objId, nsCString* result) override {
        return Base::SendClassName(objId.serialize(), result);
    }
    bool SendGetPrototype(const ObjectId& objId, ReturnStatus* rs, ObjectOrNullVariant* result) override {
        return Base::SendGetPrototype(objId.serialize(), rs, result);
    }
    bool SendGetPrototypeIfOrdinary(const ObjectId& objId, ReturnStatus* rs, bool* isOrdinary,
                                    ObjectOrNullVariant* result) override
    {
        return Base::SendGetPrototypeIfOrdinary(objId.serialize(), rs, isOrdinary, result);
    }

    bool SendRegExpToShared(const ObjectId& objId, ReturnStatus* rs,
                            nsString* source, uint32_t* flags) override {
        return Base::SendRegExpToShared(objId.serialize(), rs, source, flags);
    }

    bool SendGetPropertyKeys(const ObjectId& objId, const uint32_t& flags,
                             ReturnStatus* rs, nsTArray<JSIDVariant>* ids) override {
        return Base::SendGetPropertyKeys(objId.serialize(), flags, rs, ids);
    }
    bool SendInstanceOf(const ObjectId& objId, const JSIID& iid,
                        ReturnStatus* rs, bool* instanceof) override {
        return Base::SendInstanceOf(objId.serialize(), iid, rs, instanceof);
    }
    bool SendDOMInstanceOf(const ObjectId& objId, const int& prototypeID, const int& depth,
                           ReturnStatus* rs, bool* instanceof) override {
        return Base::SendDOMInstanceOf(objId.serialize(), prototypeID, depth, rs, instanceof);
    }

    /* The following code is needed to suppress a bogus MSVC warning (C4250). */

    virtual bool toObjectVariant(JSContext* cx, JSObject* obj, ObjectVariant* objVarp) override {
        return WrapperOwner::toObjectVariant(cx, obj, objVarp);
    }
    virtual JSObject* fromObjectVariant(JSContext* cx, const ObjectVariant& objVar) override {
        return WrapperOwner::fromObjectVariant(cx, objVar);
    }
};

} // namespace jsipc
} // namespace mozilla

#endif
