/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/Endpoint.h"
#include "chrome/common/ipc_message.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsThreadUtils.h"
#include "mozilla/ipc/ProtocolMessageUtils.h"

namespace mozilla::ipc {

UntypedManagedEndpoint::UntypedManagedEndpoint(IProtocol* aActor)
    : mInner(Some(Inner{
          /* mOtherSide */ aActor->GetWeakLifecycleProxy(),
          /* mToplevel */ nullptr,
          aActor->Id(),
          aActor->GetProtocolId(),
          aActor->Manager()->Id(),
          aActor->Manager()->GetProtocolId(),
      })) {}

UntypedManagedEndpoint::~UntypedManagedEndpoint() {
  if (!IsValid()) {
    return;
  }

  if (mInner->mOtherSide) {
    // If this ManagedEndpoint was never sent over IPC, deliver a fake
    // MANAGED_ENDPOINT_DROPPED_MESSAGE_TYPE message directly to the other side
    // actor.
    mInner->mOtherSide->ActorEventTarget()->Dispatch(NS_NewRunnableFunction(
        "~ManagedEndpoint (Local)",
        [otherSide = mInner->mOtherSide, id = mInner->mId] {
          if (IProtocol* actor = otherSide->Get(); actor && actor->CanRecv()) {
            MOZ_DIAGNOSTIC_ASSERT(actor->Id() == id, "Wrong Actor?");
            RefPtr<ActorLifecycleProxy> strongProxy(actor->GetLifecycleProxy());
            strongProxy->Get()->OnMessageReceived(
                IPC::Message(id, MANAGED_ENDPOINT_DROPPED_MESSAGE_TYPE));
          }
        }));
  } else if (mInner->mToplevel) {
    // If it was sent over IPC, we'll need to send the message to the sending
    // side. Let's send the message async.
    mInner->mToplevel->ActorEventTarget()->Dispatch(NS_NewRunnableFunction(
        "~ManagedEndpoint (Remote)",
        [toplevel = mInner->mToplevel, id = mInner->mId] {
          if (IProtocol* actor = toplevel->Get();
              actor && actor->CanSend() && actor->GetIPCChannel()) {
            actor->GetIPCChannel()->Send(MakeUnique<IPC::Message>(
                id, MANAGED_ENDPOINT_DROPPED_MESSAGE_TYPE));
          }
        }));
  }
}

bool UntypedManagedEndpoint::BindCommon(IProtocol* aActor,
                                        IProtocol* aManager) {
  MOZ_ASSERT(aManager);
  if (!mInner) {
    NS_WARNING("Cannot bind to invalid endpoint");
    return false;
  }

  // Perform thread assertions.
  if (mInner->mToplevel) {
    MOZ_DIAGNOSTIC_ASSERT(
        mInner->mToplevel->ActorEventTarget()->IsOnCurrentThread());
    MOZ_DIAGNOSTIC_ASSERT(aManager->ToplevelProtocol() ==
                          mInner->mToplevel->Get());
  }

  if (NS_WARN_IF(aManager->Id() != mInner->mManagerId) ||
      NS_WARN_IF(aManager->GetProtocolId() != mInner->mManagerType) ||
      NS_WARN_IF(aActor->GetProtocolId() != mInner->mType)) {
    MOZ_ASSERT_UNREACHABLE("Actor and manager do not match Endpoint");
    return false;
  }

  if (!aManager->CanSend() || !aManager->GetIPCChannel()) {
    NS_WARNING("Manager cannot send");
    return false;
  }

  int32_t id = mInner->mId;
  mInner.reset();

  // Our typed caller will insert the actor into the managed container.
  aActor->SetManagerAndRegister(aManager, id);

  aManager->GetIPCChannel()->Send(
      MakeUnique<IPC::Message>(id, MANAGED_ENDPOINT_BOUND_MESSAGE_TYPE));
  return true;
}

/* static */
void IPDLParamTraits<UntypedManagedEndpoint>::Write(IPC::MessageWriter* aWriter,
                                                    IProtocol* aActor,
                                                    paramType&& aParam) {
  bool isValid = aParam.mInner.isSome();
  WriteIPDLParam(aWriter, aActor, isValid);
  if (!isValid) {
    return;
  }

  auto inner = std::move(*aParam.mInner);
  aParam.mInner.reset();

  MOZ_RELEASE_ASSERT(inner.mOtherSide, "Has not been sent over IPC yet");
  MOZ_RELEASE_ASSERT(inner.mOtherSide->ActorEventTarget()->IsOnCurrentThread(),
                     "Must be being sent from the correct thread");
  MOZ_RELEASE_ASSERT(
      inner.mOtherSide->Get() && inner.mOtherSide->Get()->ToplevelProtocol() ==
                                     aActor->ToplevelProtocol(),
      "Must be being sent over the same toplevel protocol");

  WriteIPDLParam(aWriter, aActor, inner.mId);
  WriteIPDLParam(aWriter, aActor, inner.mType);
  WriteIPDLParam(aWriter, aActor, inner.mManagerId);
  WriteIPDLParam(aWriter, aActor, inner.mManagerType);
}

/* static */
bool IPDLParamTraits<UntypedManagedEndpoint>::Read(IPC::MessageReader* aReader,
                                                   IProtocol* aActor,
                                                   paramType* aResult) {
  *aResult = UntypedManagedEndpoint{};
  bool isValid = false;
  if (!aActor || !ReadIPDLParam(aReader, aActor, &isValid)) {
    return false;
  }
  if (!isValid) {
    return true;
  }

  aResult->mInner.emplace();
  auto& inner = *aResult->mInner;
  inner.mToplevel = aActor->ToplevelProtocol()->GetWeakLifecycleProxy();
  return ReadIPDLParam(aReader, aActor, &inner.mId) &&
         ReadIPDLParam(aReader, aActor, &inner.mType) &&
         ReadIPDLParam(aReader, aActor, &inner.mManagerId) &&
         ReadIPDLParam(aReader, aActor, &inner.mManagerType);
}

}  // namespace mozilla::ipc

namespace IPC {

void ParamTraits<mozilla::ipc::UntypedEndpoint>::Write(MessageWriter* aWriter,
                                                       paramType&& aParam) {
  IPC::WriteParam(aWriter, std::move(aParam.mPort));
  IPC::WriteParam(aWriter, aParam.mMessageChannelId);
  IPC::WriteParam(aWriter, aParam.mMyPid);
  IPC::WriteParam(aWriter, aParam.mOtherPid);
}

bool ParamTraits<mozilla::ipc::UntypedEndpoint>::Read(MessageReader* aReader,
                                                      paramType* aResult) {
  return IPC::ReadParam(aReader, &aResult->mPort) &&
         IPC::ReadParam(aReader, &aResult->mMessageChannelId) &&
         IPC::ReadParam(aReader, &aResult->mMyPid) &&
         IPC::ReadParam(aReader, &aResult->mOtherPid);
}

}  // namespace IPC
