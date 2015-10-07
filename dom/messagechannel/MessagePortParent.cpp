/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortParent.h"
#include "MessagePortService.h"
#include "SharedMessagePortMessage.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace dom {

MessagePortParent::MessagePortParent(const nsID& aUUID)
  : mService(MessagePortService::GetOrCreate())
  , mUUID(aUUID)
  , mEntangled(false)
  , mCanSendData(true)
{
  MOZ_ASSERT(mService);
}

MessagePortParent::~MessagePortParent()
{
  MOZ_ASSERT(!mService);
  MOZ_ASSERT(!mEntangled);
}

bool
MessagePortParent::Entangle(const nsID& aDestinationUUID,
                            const uint32_t& aSequenceID)
{
  if (!mService) {
    NS_WARNING("Entangle is called after a shutdown!");
    return false;
  }

  MOZ_ASSERT(!mEntangled);

  return mService->RequestEntangling(this, aDestinationUUID, aSequenceID);
}

bool
MessagePortParent::RecvPostMessages(nsTArray<MessagePortMessage>&& aMessages)
{
  // This converts the object in a data struct where we have BlobImpls.
  FallibleTArray<nsRefPtr<SharedMessagePortMessage>> messages;
  if (NS_WARN_IF(
      !SharedMessagePortMessage::FromMessagesToSharedParent(aMessages,
                                                            messages))) {
    return false;
  }

  if (!mEntangled) {
    return false;
  }

  if (!mService) {
    NS_WARNING("Entangle is called after a shutdown!");
    return false;
  }

  if (messages.IsEmpty()) {
    return false;
  }

  return mService->PostMessages(this, messages);
}

bool
MessagePortParent::RecvDisentangle(nsTArray<MessagePortMessage>&& aMessages)
{
  // This converts the object in a data struct where we have BlobImpls.
  FallibleTArray<nsRefPtr<SharedMessagePortMessage>> messages;
  if (NS_WARN_IF(
      !SharedMessagePortMessage::FromMessagesToSharedParent(aMessages,
                                                            messages))) {
    return false;
  }

  if (!mEntangled) {
    return false;
  }

  if (!mService) {
    NS_WARNING("Entangle is called after a shutdown!");
    return false;
  }

  if (!mService->DisentanglePort(this, messages)) {
    return false;
  }

  CloseAndDelete();
  return true;
}

bool
MessagePortParent::RecvStopSendingData()
{
  if (!mEntangled) {
    return true;
  }

  mCanSendData = false;
  unused << SendStopSendingDataConfirmed();
  return true;
}

bool
MessagePortParent::RecvClose()
{
  if (mService) {
    MOZ_ASSERT(mEntangled);

    if (!mService->ClosePort(this)) {
      return false;
    }

    Close();
  }

  MOZ_ASSERT(!mEntangled);

  unused << Send__delete__(this);
  return true;
}

void
MessagePortParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mService && mEntangled) {
    // When the last parent is deleted, this service is freed but this cannot
    // be done when the hashtables are written by CloseAll.
    nsRefPtr<MessagePortService> kungFuDeathGrip = mService;
    mService->ParentDestroy(this);
  }
}

bool
MessagePortParent::Entangled(const nsTArray<MessagePortMessage>& aMessages)
{
  MOZ_ASSERT(!mEntangled);
  mEntangled = true;
  return SendEntangled(aMessages);
}

void
MessagePortParent::CloseAndDelete()
{
  Close();
  unused << Send__delete__(this);
}

void
MessagePortParent::Close()
{
  mService = nullptr;
  mEntangled = false;
}

/* static */ bool
MessagePortParent::ForceClose(const nsID& aUUID,
                              const nsID& aDestinationUUID,
                              const uint32_t& aSequenceID)
{
  MessagePortService* service =  MessagePortService::Get();
  if (!service) {
    NS_WARNING("The service must exist if we want to close an existing MessagePort.");
    return false;
  }

  return service->ForceClose(aUUID, aDestinationUUID, aSequenceID);
}

} // namespace dom
} // namespace mozilla
