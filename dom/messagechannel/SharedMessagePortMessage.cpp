/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefMessageBodyService.h"
#include "SharedMessagePortMessage.h"
#include "MessagePort.h"
#include "MessagePortChild.h"
#include "MessagePortParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

SharedMessagePortMessage::SharedMessagePortMessage() : mRefDataId({}) {}

void SharedMessagePortMessage::Write(
    JSContext* aCx, JS::Handle<JS::Value> aValue,
    JS::Handle<JS::Value> aTransfers, nsID& aPortID,
    RefMessageBodyService* aRefMessageBodyService, ErrorResult& aRv) {
  MOZ_ASSERT(!mCloneData && !mRefData);
  MOZ_ASSERT(aRefMessageBodyService);

  mCloneData = MakeUnique<ipc::StructuredCloneData>(
      JS::StructuredCloneScope::UnknownDestination);
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

void SharedMessagePortMessage::Read(
    JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
    RefMessageBodyService* aRefMessageBodyService, ErrorResult& aRv) {
  MOZ_ASSERT(aRefMessageBodyService);

  if (mCloneData) {
    return mCloneData->Read(aCx, aValue, aRv);
  }

  MOZ_ASSERT(!mRefData);
  mRefData = aRefMessageBodyService->Steal(mRefDataId);
  if (!mRefData) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  mRefData->CloneData()->Read(aCx, aValue, aRv);
}

bool SharedMessagePortMessage::TakeTransferredPortsAsSequence(
    Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts) {
  if (mCloneData) {
    return mCloneData->TakeTransferredPortsAsSequence(aPorts);
  }

  MOZ_ASSERT(mRefData);
  return mRefData->CloneData()->TakeTransferredPortsAsSequence(aPorts);
}

/* static */
void SharedMessagePortMessage::FromSharedToMessagesChild(
    MessagePortChild* aActor,
    const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
    nsTArray<MessageData>& aArray) {
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aArray.IsEmpty());
  aArray.SetCapacity(aData.Length());

  PBackgroundChild* backgroundManager = aActor->Manager();
  MOZ_ASSERT(backgroundManager);

  for (auto& data : aData) {
    MessageData* message = aArray.AppendElement();

    if (data->mCloneData) {
      ClonedMessageData clonedData;
      data->mCloneData->BuildClonedMessageDataForBackgroundChild(
          backgroundManager, clonedData);
      *message = clonedData;
      continue;
    }

    *message = RefMessageData(data->mRefDataId);
  }
}

/* static */
bool SharedMessagePortMessage::FromMessagesToSharedChild(
    nsTArray<MessageData>& aArray,
    FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData) {
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();

    if (message.type() == MessageData::TClonedMessageData) {
      data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
          JS::StructuredCloneScope::UnknownDestination);
      data->mCloneData->StealFromClonedMessageDataForBackgroundChild(message);
    } else {
      MOZ_ASSERT(message.type() == MessageData::TRefMessageData);
      data->mRefDataId = message.get_RefMessageData().uuid();
    }

    if (!aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

/* static */
bool SharedMessagePortMessage::FromSharedToMessagesParent(
    MessagePortParent* aActor,
    const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
    FallibleTArray<MessageData>& aArray) {
  MOZ_ASSERT(aArray.IsEmpty());

  if (NS_WARN_IF(!aArray.SetCapacity(aData.Length(), mozilla::fallible))) {
    return false;
  }

  PBackgroundParent* backgroundManager = aActor->Manager();
  MOZ_ASSERT(backgroundManager);

  for (auto& data : aData) {
    MessageData* message = aArray.AppendElement(mozilla::fallible);

    if (data->mCloneData) {
      ClonedMessageData clonedData;
      data->mCloneData->BuildClonedMessageDataForBackgroundParent(
          backgroundManager, clonedData);
      *message = clonedData;
      continue;
    }

    *message = RefMessageData(data->mRefDataId);
  }

  return true;
}

/* static */
bool SharedMessagePortMessage::FromMessagesToSharedParent(
    nsTArray<MessageData>& aArray,
    FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData) {
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();

    if (message.type() == MessageData::TClonedMessageData) {
      data->mCloneData = MakeUnique<ipc::StructuredCloneData>(
          JS::StructuredCloneScope::UnknownDestination);
      data->mCloneData->StealFromClonedMessageDataForBackgroundParent(message);
    } else {
      MOZ_ASSERT(message.type() == MessageData::TRefMessageData);
      data->mRefDataId = message.get_RefMessageData().uuid();
    }

    if (!aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

}  // namespace dom
}  // namespace mozilla
