/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortService.h"
#include "MessagePortParent.h"
#include "SharedMessagePortMessage.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

using mozilla::ipc::AssertIsOnBackgroundThread;

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<MessagePortService> gInstance;

void
AssertIsInMainProcess()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
}

} // namespace

class MessagePortService::MessagePortServiceData final
{
public:
  explicit MessagePortServiceData(const nsID& aDestinationUUID)
    : mDestinationUUID(aDestinationUUID)
    , mSequenceID(1)
    , mParent(nullptr)
    // By default we don't know the next parent.
    , mWaitingForNewParent(true)
    , mNextStepCloseAll(false)
  {
    MOZ_COUNT_CTOR(MessagePortServiceData);
  }

  MessagePortServiceData(const MessagePortServiceData& aOther) = delete;
  MessagePortServiceData& operator=(const MessagePortServiceData&) = delete;

  ~MessagePortServiceData()
  {
    MOZ_COUNT_DTOR(MessagePortServiceData);
  }

  nsID mDestinationUUID;

  uint32_t mSequenceID;
  MessagePortParent* mParent;

  struct NextParent
  {
    uint32_t mSequenceID;
    // MessagePortParent keeps the service alive, and we don't want a cycle.
    MessagePortParent* mParent;
  };

  FallibleTArray<NextParent> mNextParents;
  FallibleTArray<RefPtr<SharedMessagePortMessage>> mMessages;

  bool mWaitingForNewParent;
  bool mNextStepCloseAll;
};

/* static */ MessagePortService*
MessagePortService::Get()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  return gInstance;
}

/* static */ MessagePortService*
MessagePortService::GetOrCreate()
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!gInstance) {
    gInstance = new MessagePortService();
  }

  return gInstance;
}

bool
MessagePortService::RequestEntangling(MessagePortParent* aParent,
                                      const nsID& aDestinationUUID,
                                      const uint32_t& aSequenceID)
{
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
    FallibleTArray<MessagePortMessage> array;
    if (!SharedMessagePortMessage::FromSharedToMessagesParent(aParent,
                                                              data->mMessages,
                                                              array)) {
      CloseAll(aParent->ID());
      return false;
    }

    data->mMessages.Clear();

    // We can entangle the port.
    if (!aParent->Entangled(array)) {
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
  MessagePortServiceData::NextParent* nextParent =
    data->mNextParents.AppendElement(mozilla::fallible);
  if (!nextParent) {
    CloseAll(aParent->ID());
    return false;
  }

  nextParent->mSequenceID = aSequenceID;
  nextParent->mParent = aParent;

  return true;
}

bool
MessagePortService::DisentanglePort(
                  MessagePortParent* aParent,
                  FallibleTArray<RefPtr<SharedMessagePortMessage>>& aMessages)
{
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    MOZ_ASSERT(false, "Unknown MessagePortParent should not happen.");
    return false;
  }

  if (data->mParent != aParent) {
    MOZ_ASSERT(false, "DisentanglePort() should be called just from the correct parent.");
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

  FallibleTArray<MessagePortMessage> array;
  if (!SharedMessagePortMessage::FromSharedToMessagesParent(data->mParent,
                                                            aMessages,
                                                            array)) {
    return false;
  }

  Unused << data->mParent->Entangled(array);
  return true;
}

bool
MessagePortService::ClosePort(MessagePortParent* aParent)
{
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    MOZ_ASSERT(false, "Unknown MessagePortParent should not happend.");
    return false;
  }

  if (data->mParent != aParent) {
    MOZ_ASSERT(false, "ClosePort() should be called just from the correct parent.");
    return false;
  }

  if (!data->mNextParents.IsEmpty()) {
    MOZ_ASSERT(false, "ClosePort() should be called when there are not next parents.");
    return false;
  }

  // We don't want to send a message to this parent.
  data->mParent = nullptr;

  CloseAll(aParent->ID());
  return true;
}

#ifdef DEBUG
PLDHashOperator
MessagePortService::CloseAllDebugCheck(const nsID& aID,
                                       MessagePortServiceData* aData,
                                       void* aPtr)
{
  nsID* id = static_cast<nsID*>(aPtr);
  MOZ_ASSERT(!id->Equals(aID));
  return PL_DHASH_NEXT;
}
#endif

void
MessagePortService::CloseAll(const nsID& aUUID, bool aForced)
{
  MessagePortServiceData* data;
  if (!mPorts.Get(aUUID, &data)) {
    MaybeShutdown();
    return;
  }

  if (data->mParent) {
    data->mParent->Close();
  }

  for (const MessagePortServiceData::NextParent& parent : data->mNextParents) {
    parent.mParent->CloseAndDelete();
  }

  nsID destinationUUID = data->mDestinationUUID;

  // If we have informations about the other port and that port has some
  // pending messages to deliver but the parent has not processed them yet,
  // because its entangling request didn't arrive yet), we cannot close this
  // channel.
  MessagePortServiceData* destinationData;
  if (!aForced &&
      mPorts.Get(destinationUUID, &destinationData) &&
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
  mPorts.EnumerateRead(CloseAllDebugCheck, const_cast<nsID*>(&aUUID));
#endif

  MaybeShutdown();
}

// This service can be dismissed when there are not active ports.
void
MessagePortService::MaybeShutdown()
{
  if (mPorts.Count() == 0) {
    gInstance = nullptr;
  }
}

bool
MessagePortService::PostMessages(
                  MessagePortParent* aParent,
                  FallibleTArray<RefPtr<SharedMessagePortMessage>>& aMessages)
{
  MessagePortServiceData* data;
  if (!mPorts.Get(aParent->ID(), &data)) {
    MOZ_ASSERT(false, "Unknown MessagePortParent should not happend.");
    return false;
  }

  if (data->mParent != aParent) {
    MOZ_ASSERT(false, "PostMessages() should be called just from the correct parent.");
    return false;
  }

  MOZ_ALWAYS_TRUE(mPorts.Get(data->mDestinationUUID, &data));

  if (!data->mMessages.AppendElements(aMessages, mozilla::fallible)) {
    return false;
  }

  // If the parent can send data to the child, let's proceed.
  if (data->mParent && data->mParent->CanSendData()) {
    FallibleTArray<MessagePortMessage> messages;
    if (!SharedMessagePortMessage::FromSharedToMessagesParent(data->mParent,
                                                              data->mMessages,
                                                              messages)) {
      return false;
    }

    data->mMessages.Clear();
    Unused << data->mParent->SendReceiveData(messages);
  }

  return true;
}

void
MessagePortService::ParentDestroy(MessagePortParent* aParent)
{
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

bool
MessagePortService::ForceClose(const nsID& aUUID,
                               const nsID& aDestinationUUID,
                               const uint32_t& aSequenceID)
{
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

} // namespace dom
} // namespace mozilla
