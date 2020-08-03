/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortService.h"
#include "MessagePortParent.h"
#include "mozilla/dom/SharedMessageBody.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"

using mozilla::ipc::AssertIsOnBackgroundThread;

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<MessagePortService> gInstance;

void AssertIsInMainProcess() {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
}

}  // namespace

struct MessagePortService::NextParent {
  uint32_t mSequenceID;
  // MessagePortParent keeps the service alive, and we don't want a cycle.
  CheckedUnsafePtr<MessagePortParent> mParent;
};

}  // namespace dom
}  // namespace mozilla

// Need to call CheckedUnsafePtr's copy constructor and destructor when
// resizing dynamic arrays containing NextParent (by calling NextParent's
// implicit copy constructor/destructor rather than memmove-ing NextParents).
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::MessagePortService::NextParent);

namespace mozilla {
namespace dom {

class MessagePortService::MessagePortServiceData final {
 public:
  explicit MessagePortServiceData(const nsID& aDestinationUUID)
      : mDestinationUUID(aDestinationUUID),
        mSequenceID(1),
        mParent(nullptr)
        // By default we don't know the next parent.
        ,
        mWaitingForNewParent(true),
        mNextStepCloseAll(false) {
    MOZ_COUNT_CTOR(MessagePortServiceData);
  }

  MessagePortServiceData(const MessagePortServiceData& aOther) = delete;
  MessagePortServiceData& operator=(const MessagePortServiceData&) = delete;

  MOZ_COUNTED_DTOR(MessagePortServiceData)

  nsID mDestinationUUID;

  uint32_t mSequenceID;
  CheckedUnsafePtr<MessagePortParent> mParent;

  FallibleTArray<NextParent> mNextParents;
  FallibleTArray<RefPtr<SharedMessageBody>> mMessages;

  bool mWaitingForNewParent;
  bool mNextStepCloseAll;
};

/* static */
MessagePortService* MessagePortService::Get() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return gInstance;
}

/* static */
MessagePortService* MessagePortService::GetOrCreate() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!gInstance) {
    gInstance = new MessagePortService();
  }

  return gInstance;
}

bool MessagePortService::RequestEntangling(MessagePortParent* aParent,
                                           const nsID& aDestinationUUID,
                                           const uint32_t& aSequenceID) {
  MOZ_ASSERT(aParent);
  MessagePortServiceData* data;

  // If we don't have a MessagePortServiceData, we must create 2 of them for
  // both ports.
  if (!mPorts.Get(aParent->ID(), &data)) {
    // Create the MessagePortServiceData for the destination.
    if (mPorts.Get(aDestinationUUID, nullptr)) {
      MOZ_ASSERT(false, "The creation of the 2 ports should be in sync.");
      return false;
    }

    data = new MessagePortServiceData(aParent->ID());
    mPorts.Put(aDestinationUUID, data);

    data = new MessagePortServiceData(aDestinationUUID);
    mPorts.Put(aParent->ID(), data);
  }

  // This is a security check.
  if (!data->mDestinationUUID.Equals(aDestinationUUID)) {
    MOZ_ASSERT(false, "DestinationUUIDs do not match!");
    CloseAll(aParent->ID());
    return false;
  }

  if (aSequenceID < data->mSequenceID) {
    MOZ_ASSERT(false, "Invalid sequence ID!");
    CloseAll(aParent->ID());
    return false;
  }

  if (aSequenceID == data->mSequenceID) {
    if (data->mParent) {
      MOZ_ASSERT(false, "Two ports cannot have the same sequenceID.");
      CloseAll(aParent->ID());
      return false;
    }

    // We activate this port, sending all the messages.
    data->mParent = aParent;
    data->mWaitingForNewParent = false;

    // We want to ensure we clear data->mMessages even if we early return, while
    // also ensuring that its contents remain alive until after array's contents
    // are destroyed because of JSStructuredCloneData borrowing.  So we use
    // Move to initialize things swapped and do it before we declare `array` so
    // that reverse destruction order works for us.
    FallibleTArray<RefPtr<SharedMessageBody>> messages(
        std::move(data->mMessages));
    nsTArray<MessageData> array;
    if (!SharedMessageBody::FromSharedToMessagesParent(aParent->Manager(),
                                                       messages, array)) {
      CloseAll(aParent->ID());
      return false;
    }

    // We can entangle the port.
    if (!aParent->Entangled(std::move(array))) {
      CloseAll(aParent->ID());
      return false;
    }

    // If we were waiting for this parent in order to close this channel, this
    // is the time to do it.
    if (data->mNextStepCloseAll) {
      CloseAll(aParent->ID());
    }

    return true;
  }

  // This new parent will be the next one when a Disentangle request is
  // received from the current parent.
  auto nextParent = data->mNextParents.AppendElement(mozilla::fallible);
  if (!nextParent) {
    CloseAll(aParent->ID());
    return false;
  }

  nextParent->mSequenceID = aSequenceID;
  nextParent->mParent = aParent;

  return true;
}

bool MessagePortService::DisentanglePort(
    MessagePortParent* aParent,
    FallibleTArray<RefPtr<SharedMessageBody>>& aMessages) {
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    MOZ_ASSERT(false, "Unknown MessagePortParent should not happen.");
    return false;
  }

  if (data->mParent != aParent) {
    MOZ_ASSERT(
        false,
        "DisentanglePort() should be called just from the correct parent.");
    return false;
  }

  // Let's put the messages in the correct order. |aMessages| contains the
  // unsent messages so they have to go first.
  if (!aMessages.AppendElements(data->mMessages, mozilla::fallible)) {
    return false;
  }

  data->mMessages.Clear();

  ++data->mSequenceID;

  // If we don't have a parent, we have to store the pending messages and wait.
  uint32_t index = 0;
  MessagePortParent* nextParent = nullptr;
  for (; index < data->mNextParents.Length(); ++index) {
    if (data->mNextParents[index].mSequenceID == data->mSequenceID) {
      nextParent = data->mNextParents[index].mParent;
      break;
    }
  }

  // We didn't find the parent.
  if (!nextParent) {
    data->mMessages.SwapElements(aMessages);
    data->mWaitingForNewParent = true;
    data->mParent = nullptr;
    return true;
  }

  data->mParent = nextParent;
  data->mNextParents.RemoveElementAt(index);

  nsTArray<MessageData> array;
  if (!SharedMessageBody::FromSharedToMessagesParent(data->mParent->Manager(),
                                                     aMessages, array)) {
    return false;
  }

  Unused << data->mParent->Entangled(std::move(array));
  return true;
}

bool MessagePortService::ClosePort(MessagePortParent* aParent) {
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    MOZ_ASSERT(false, "Unknown MessagePortParent should not happend.");
    return false;
  }

  if (data->mParent != aParent) {
    MOZ_ASSERT(false,
               "ClosePort() should be called just from the correct parent.");
    return false;
  }

  if (!data->mNextParents.IsEmpty()) {
    MOZ_ASSERT(false,
               "ClosePort() should be called when there are not next parents.");
    return false;
  }

  // We don't want to send a message to this parent.
  data->mParent = nullptr;

  CloseAll(aParent->ID());
  return true;
}

void MessagePortService::CloseAll(const nsID& aUUID, bool aForced) {
  MessagePortServiceData* data;
  if (!mPorts.Get(aUUID, &data)) {
    MaybeShutdown();
    return;
  }

  if (data->mParent) {
    data->mParent->Close();
    data->mParent = nullptr;
  }

  for (auto& nextParent : data->mNextParents) {
    // CloseAndDelete may delete the pointee, so ensure no CheckedUnsafePtrs
    // exist when that happens.
    MessagePortParent* parent = nextParent.mParent;
    nextParent.mParent = nullptr;

    parent->CloseAndDelete();
  }

  nsID destinationUUID = data->mDestinationUUID;

  // If we have informations about the other port and that port has some
  // pending messages to deliver but the parent has not processed them yet,
  // because its entangling request didn't arrive yet), we cannot close this
  // channel.
  MessagePortServiceData* destinationData;
  if (!aForced && mPorts.Get(destinationUUID, &destinationData) &&
      !destinationData->mMessages.IsEmpty() &&
      destinationData->mWaitingForNewParent) {
    MOZ_ASSERT(!destinationData->mNextStepCloseAll);
    destinationData->mNextStepCloseAll = true;
    return;
  }

  mPorts.Remove(aUUID);

  CloseAll(destinationUUID, aForced);

  // CloseAll calls itself recursively and it can happen that it deletes
  // itself. Before continuing we must check if we are still alive.
  if (!gInstance) {
    return;
  }

#ifdef DEBUG
  for (auto iter = mPorts.Iter(); !iter.Done(); iter.Next()) {
    MOZ_ASSERT(!aUUID.Equals(iter.Key()));
  }
#endif

  MaybeShutdown();
}

// This service can be dismissed when there are not active ports.
void MessagePortService::MaybeShutdown() {
  if (mPorts.Count() == 0) {
    gInstance = nullptr;
  }
}

bool MessagePortService::PostMessages(
    MessagePortParent* aParent,
    FallibleTArray<RefPtr<SharedMessageBody>>& aMessages) {
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    MOZ_ASSERT(false, "Unknown MessagePortParent should not happend.");
    return false;
  }

  if (data->mParent != aParent) {
    MOZ_ASSERT(false,
               "PostMessages() should be called just from the correct parent.");
    return false;
  }

  MOZ_ALWAYS_TRUE(mPorts.Get(data->mDestinationUUID, &data));

  if (!data->mMessages.AppendElements(aMessages, mozilla::fallible)) {
    return false;
  }

  // If the parent can send data to the child, let's proceed.
  if (data->mParent && data->mParent->CanSendData()) {
    {
      nsTArray<MessageData> messages;
      if (!SharedMessageBody::FromSharedToMessagesParent(
              data->mParent->Manager(), data->mMessages, messages)) {
        return false;
      }

      Unused << data->mParent->SendReceiveData(messages);
    }
    // `messages` borrows the underlying JSStructuredCloneData so we need to
    // avoid destroying the `mMessages` until after we've destroyed `messages`.
    data->mMessages.Clear();
  }

  return true;
}

void MessagePortService::ParentDestroy(MessagePortParent* aParent) {
  // This port has already been destroyed.
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    return;
  }

  if (data->mParent != aParent) {
    // We don't want to send a message to this parent.
    for (uint32_t i = 0; i < data->mNextParents.Length(); ++i) {
      if (aParent == data->mNextParents[i].mParent) {
        data->mNextParents.RemoveElementAt(i);
        break;
      }
    }
  }

  CloseAll(aParent->ID());
}

bool MessagePortService::ForceClose(const nsID& aUUID,
                                    const nsID& aDestinationUUID,
                                    const uint32_t& aSequenceID) {
  MessagePortServiceData* data;
  if (!mPorts.Get(aUUID, &data)) {
    NS_WARNING("Unknown MessagePort in ForceClose()");
    return true;
  }

  if (!data->mDestinationUUID.Equals(aDestinationUUID) ||
      data->mSequenceID != aSequenceID) {
    NS_WARNING("DestinationUUID and/or sequenceID do not match.");
    return false;
  }

  CloseAll(aUUID, true);
  return true;
}

}  // namespace dom
}  // namespace mozilla
