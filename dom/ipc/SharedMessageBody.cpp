/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedMessageBody.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/RefMessageBodyService.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

SharedMessageBody::SharedMessageBody(
    StructuredCloneHolder::TransferringSupport aSupportsTransferring,
    const Maybe<nsID>& aAgentClusterId)
    : mRefDataId(Nothing()),
      mSupportsTransferring(aSupportsTransferring),
      mAgentClusterId(aAgentClusterId) {}

void SharedMessageBody::Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
                              JS::Handle<JS::Value> aTransfers, nsID& aPortID,
                              RefMessageBodyService* aRefMessageBodyService,
                              ErrorResult& aRv) {
  MOZ_ASSERT(!mCloneData && !mRefData);
  MOZ_ASSERT(aRefMessageBodyService);

  JS::CloneDataPolicy cloneDataPolicy;
  // During a writing, we don't know the destination, so we assume it is part of
  // the same agent cluster.
  cloneDataPolicy.allowIntraClusterClonableSharedObjects();

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  MOZ_ASSERT(global);

  if (global->IsSharedMemoryAllowed()) {
    cloneDataPolicy.allowSharedMemoryObjects();
  }

  mCloneData = MakeUnique<ipc::StructuredCloneData>(
      JS::StructuredCloneScope::UnknownDestination, mSupportsTransferring);
  mCloneData->Write(aCx, aValue, aTransfers, cloneDataPolicy, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (mCloneData->CloneScope() == JS::StructuredCloneScope::DifferentProcess) {
    return;
  }

  MOZ_ASSERT(mCloneData->CloneScope() == JS::StructuredCloneScope::SameProcess);
  RefPtr<RefMessageBody> refData =
      new RefMessageBody(aPortID, std::move(mCloneData));

  mRefDataId.emplace(aRefMessageBodyService->Register(refData.forget(), aRv));
}

void SharedMessageBody::Read(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aValue,
                             RefMessageBodyService* aRefMessageBodyService,
                             SharedMessageBody::ReadMethod aReadMethod,
                             ErrorResult& aRv) {
  MOZ_ASSERT(aRefMessageBodyService);

  if (mCloneData) {
    // Use a default cloneDataPolicy here, because SharedArrayBuffers and WASM
    // are not supported.
    return mCloneData->Read(aCx, aValue, JS::CloneDataPolicy(), aRv);
  }

  JS::CloneDataPolicy cloneDataPolicy;

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  MOZ_ASSERT(global);

  // Clones within the same agent cluster are allowed to use shared array
  // buffers and WASM modules.
  if (mAgentClusterId.isSome()) {
    Maybe<nsID> agentClusterId = global->GetAgentClusterId();
    if (agentClusterId.isSome() &&
        mAgentClusterId.value().Equals(agentClusterId.value())) {
      cloneDataPolicy.allowIntraClusterClonableSharedObjects();
    }
  }

  if (global->IsSharedMemoryAllowed()) {
    cloneDataPolicy.allowSharedMemoryObjects();
  }

  MOZ_ASSERT(!mRefData);
  MOZ_ASSERT(mRefDataId.isSome());

  if (aReadMethod == SharedMessageBody::StealRefMessageBody) {
    mRefData = aRefMessageBodyService->Steal(mRefDataId.value());
  } else {
    MOZ_ASSERT(aReadMethod == SharedMessageBody::KeepRefMessageBody);
    mRefData = aRefMessageBodyService->GetAndCount(mRefDataId.value());
  }

  if (!mRefData) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  mRefData->CloneData()->Read(aCx, aValue, cloneDataPolicy, aRv);
}

bool SharedMessageBody::TakeTransferredPortsAsSequence(
    Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts) {
  if (mCloneData) {
    return mCloneData->TakeTransferredPortsAsSequence(aPorts);
  }

  MOZ_ASSERT(mRefData);
  return mRefData->CloneData()->TakeTransferredPortsAsSequence(aPorts);
}

/* static */
void SharedMessageBody::FromSharedToMessageChild(
    mozilla::ipc::PBackgroundChild* aManager, SharedMessageBody* aData,
    MessageData& aMessage) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aData);

  aMessage.agentClusterId() = aData->mAgentClusterId;

  if (aData->mCloneData) {
    ClonedMessageData clonedData;
    aData->mCloneData->BuildClonedMessageDataForBackgroundChild(aManager,
                                                                clonedData);
    aMessage.data() = std::move(clonedData);
    return;
  }

  MOZ_ASSERT(aData->mRefDataId.isSome());
  aMessage.data() = RefMessageData(aData->mRefDataId.value());
}

/* static */
void SharedMessageBody::FromSharedToMessagesChild(
    PBackgroundChild* aManager,
    const nsTArray<RefPtr<SharedMessageBody>>& aData,
    nsTArray<MessageData>& aArray) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aArray.IsEmpty());
  aArray.SetCapacity(aData.Length());

  for (auto& data : aData) {
    MessageData* message = aArray.AppendElement();
    FromSharedToMessageChild(aManager, data, *message);
  }
}

/* static */
already_AddRefed<SharedMessageBody> SharedMessageBody::FromMessageToSharedChild(
    MessageData& aMessage,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  RefPtr<SharedMessageBody> data =
      new SharedMessageBody(aSupportsTransferring, aMessage.agentClusterId());

  if (aMessage.data().type() == MessageDataType::TClonedMessageData) {
    data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
        JS::StructuredCloneScope::UnknownDestination, aSupportsTransferring);
    data->mCloneData->StealFromClonedMessageDataForBackgroundChild(
        aMessage.data().get_ClonedMessageData());
  } else {
    MOZ_ASSERT(aMessage.data().type() == MessageDataType::TRefMessageData);
    data->mRefDataId.emplace(aMessage.data().get_RefMessageData().uuid());
  }

  return data.forget();
}

/* static */
already_AddRefed<SharedMessageBody> SharedMessageBody::FromMessageToSharedChild(
    const MessageData& aMessage,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  RefPtr<SharedMessageBody> data =
      new SharedMessageBody(aSupportsTransferring, aMessage.agentClusterId());

  if (aMessage.data().type() == MessageDataType::TClonedMessageData) {
    data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
        JS::StructuredCloneScope::UnknownDestination, aSupportsTransferring);
    data->mCloneData->BorrowFromClonedMessageDataForBackgroundChild(
        aMessage.data().get_ClonedMessageData());
  } else {
    MOZ_ASSERT(aMessage.data().type() == MessageDataType::TRefMessageData);
    data->mRefDataId.emplace(aMessage.data().get_RefMessageData().uuid());
  }

  return data.forget();
}

/* static */
bool SharedMessageBody::FromMessagesToSharedChild(
    nsTArray<MessageData>& aArray,
    FallibleTArray<RefPtr<SharedMessageBody>>& aData,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessageBody> data =
        FromMessageToSharedChild(message, aSupportsTransferring);
    if (!data || !aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

/* static */
bool SharedMessageBody::FromSharedToMessagesParent(
    PBackgroundParent* aManager,
    const nsTArray<RefPtr<SharedMessageBody>>& aData,
    nsTArray<MessageData>& aArray) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aArray.IsEmpty());

  if (NS_WARN_IF(!aArray.SetCapacity(aData.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& data : aData) {
    MessageData* message = aArray.AppendElement();
    message->agentClusterId() = data->mAgentClusterId;

    if (data->mCloneData) {
      ClonedMessageData clonedData;
      data->mCloneData->BuildClonedMessageDataForBackgroundParent(aManager,
                                                                  clonedData);
      message->data() = std::move(clonedData);
      continue;
    }

    MOZ_ASSERT(data->mRefDataId.isSome());
    message->data() = RefMessageData(data->mRefDataId.value());
  }

  return true;
}

/* static */
already_AddRefed<SharedMessageBody>
SharedMessageBody::FromMessageToSharedParent(
    MessageData& aMessage,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  RefPtr<SharedMessageBody> data =
      new SharedMessageBody(aSupportsTransferring, aMessage.agentClusterId());

  if (aMessage.data().type() == MessageDataType::TClonedMessageData) {
    data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
        JS::StructuredCloneScope::UnknownDestination, aSupportsTransferring);
    data->mCloneData->StealFromClonedMessageDataForBackgroundParent(
        aMessage.data().get_ClonedMessageData());
  } else {
    MOZ_ASSERT(aMessage.data().type() == MessageDataType::TRefMessageData);
    data->mRefDataId.emplace(aMessage.data().get_RefMessageData().uuid());
  }

  return data.forget();
}

bool SharedMessageBody::FromMessagesToSharedParent(
    nsTArray<MessageData>& aArray,
    FallibleTArray<RefPtr<SharedMessageBody>>& aData,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessageBody> data = FromMessageToSharedParent(message);
    if (!data || !aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

}  // namespace dom
}  // namespace mozilla
