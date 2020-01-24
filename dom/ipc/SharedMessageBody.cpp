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
    StructuredCloneHolder::TransferringSupport aSupportsTransferring)
    : mRefDataId({}), mSupportsTransferring(aSupportsTransferring) {}

void SharedMessageBody::Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
                              JS::Handle<JS::Value> aTransfers, nsID& aPortID,
                              RefMessageBodyService* aRefMessageBodyService,
                              ErrorResult& aRv) {
  MOZ_ASSERT(!mCloneData && !mRefData);
  MOZ_ASSERT(aRefMessageBodyService);

  mCloneData = MakeUnique<ipc::StructuredCloneData>(
      JS::StructuredCloneScope::UnknownDestination, mSupportsTransferring);
  mCloneData->Write(aCx, aValue, aTransfers, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (mCloneData->CloneScope() == JS::StructuredCloneScope::DifferentProcess) {
    return;
  }

  MOZ_ASSERT(mCloneData->CloneScope() == JS::StructuredCloneScope::SameProcess);
  RefPtr<RefMessageBody> refData =
      new RefMessageBody(aPortID, std::move(mCloneData));

  mRefDataId = aRefMessageBodyService->Register(refData.forget(), aRv);
}

void SharedMessageBody::Read(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aValue,
                             RefMessageBodyService* aRefMessageBodyService,
                             SharedMessageBody::ReadMethod aReadMethod,
                             ErrorResult& aRv) {
  MOZ_ASSERT(aRefMessageBodyService);

  if (mCloneData) {
    return mCloneData->Read(aCx, aValue, aRv);
  }

  MOZ_ASSERT(!mRefData);

  if (aReadMethod == SharedMessageBody::StealRefMessageBody) {
    mRefData = aRefMessageBodyService->Steal(mRefDataId);
  } else {
    MOZ_ASSERT(aReadMethod == SharedMessageBody::KeepRefMessageBody);
    mRefData = aRefMessageBodyService->GetAndCount(mRefDataId);
  }

  if (!mRefData) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  mRefData->CloneData()->Read(aCx, aValue, aRv);
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

  if (aData->mCloneData) {
    ClonedMessageData clonedData;
    aData->mCloneData->BuildClonedMessageDataForBackgroundChild(aManager,
                                                                clonedData);
    aMessage = clonedData;
    return;
  }

  aMessage = RefMessageData(aData->mRefDataId);
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
  RefPtr<SharedMessageBody> data = new SharedMessageBody(aSupportsTransferring);

  if (aMessage.type() == MessageData::TClonedMessageData) {
    data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
        JS::StructuredCloneScope::UnknownDestination, aSupportsTransferring);
    data->mCloneData->StealFromClonedMessageDataForBackgroundChild(
        aMessage.get_ClonedMessageData());
  } else {
    MOZ_ASSERT(aMessage.type() == MessageData::TRefMessageData);
    data->mRefDataId = aMessage.get_RefMessageData().uuid();
  }

  return data.forget();
}

/* static */
already_AddRefed<SharedMessageBody> SharedMessageBody::FromMessageToSharedChild(
    const MessageData& aMessage,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  RefPtr<SharedMessageBody> data = new SharedMessageBody(aSupportsTransferring);

  if (aMessage.type() == MessageData::TClonedMessageData) {
    data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
        JS::StructuredCloneScope::UnknownDestination, aSupportsTransferring);
    data->mCloneData->BorrowFromClonedMessageDataForBackgroundChild(aMessage);
  } else {
    MOZ_ASSERT(aMessage.type() == MessageData::TRefMessageData);
    data->mRefDataId = aMessage.get_RefMessageData().uuid();
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
    FallibleTArray<MessageData>& aArray) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aArray.IsEmpty());

  if (NS_WARN_IF(!aArray.SetCapacity(aData.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& data : aData) {
    MessageData* message = aArray.AppendElement(mozilla::fallible);

    if (data->mCloneData) {
      ClonedMessageData clonedData;
      data->mCloneData->BuildClonedMessageDataForBackgroundParent(aManager,
                                                                  clonedData);
      *message = clonedData;
      continue;
    }

    *message = RefMessageData(data->mRefDataId);
  }

  return true;
}

/* static */
already_AddRefed<SharedMessageBody>
SharedMessageBody::FromMessageToSharedParent(
    MessageData& aMessage,
    StructuredCloneHolder::TransferringSupport aSupportsTransferring) {
  RefPtr<SharedMessageBody> data = new SharedMessageBody(aSupportsTransferring);

  if (aMessage.type() == MessageData::TClonedMessageData) {
    data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
        JS::StructuredCloneScope::UnknownDestination, aSupportsTransferring);
    data->mCloneData->StealFromClonedMessageDataForBackgroundParent(aMessage);
  } else {
    MOZ_ASSERT(aMessage.type() == MessageData::TRefMessageData);
    data->mRefDataId = aMessage.get_RefMessageData().uuid();
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
