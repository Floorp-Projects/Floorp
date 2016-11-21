/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortParent.h"
#include "MessagePortService.h"
#include "SharedMessagePortMessage.h"
#include "mozilla/Unused.h"

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

mozilla::ipc::IPCResult
MessagePortParent::RecvPostMessages(nsTArray<ClonedMessageData>&& aMessages)
{
  // This converts the object in a data struct where we have BlobImpls.
  FallibleTArray<RefPtr<SharedMessagePortMessage>> messages;
  if (NS_WARN_IF(
      !SharedMessagePortMessage::FromMessagesToSharedParent(aMessages,
                                                            messages))) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mEntangled) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mService) {
    NS_WARNING("Entangle is called after a shutdown!");
    return IPC_FAIL_NO_REASON(this);
  }

  if (messages.IsEmpty()) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mService->PostMessages(this, messages)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
MessagePortParent::RecvDisentangle(nsTArray<ClonedMessageData>&& aMessages)
{
  // This converts the object in a data struct where we have BlobImpls.
  FallibleTArray<RefPtr<SharedMessagePortMessage>> messages;
  if (NS_WARN_IF(
      !SharedMessagePortMessage::FromMessagesToSharedParent(aMessages,
                                                            messages))) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mEntangled) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mService) {
    NS_WARNING("Entangle is called after a shutdown!");
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mService->DisentanglePort(this, messages)) {
    return IPC_FAIL_NO_REASON(this);
  }

  CloseAndDelete();
  return IPC_OK();
}

mozilla::ipc::IPCResult
MessagePortParent::RecvStopSendingData()
{
  if (!mEntangled) {
    return IPC_OK();
  }

  mCanSendData = false;
  Unused << SendStopSendingDataConfirmed();
  return IPC_OK();
}

mozilla::ipc::IPCResult
MessagePortParent::RecvClose()
{
  if (mService) {
    MOZ_ASSERT(mEntangled);

    if (!mService->ClosePort(this)) {
      return IPC_FAIL_NO_REASON(this);
    }

    Close();
  }

  MOZ_ASSERT(!mEntangled);

  Unused << Send__delete__(this);
  return IPC_OK();
}

void
MessagePortParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mService && mEntangled) {
    // When the last parent is deleted, this service is freed but this cannot
    // be done when the hashtables are written by CloseAll.
    RefPtr<MessagePortService> kungFuDeathGrip = mService;
    kungFuDeathGrip->ParentDestroy(this);
  }
}

bool
MessagePortParent::Entangled(const nsTArray<ClonedMessageData>& aMessages)
{
  MOZ_ASSERT(!mEntangled);
  mEntangled = true;
  return SendEntangled(aMessages);
}

void
MessagePortParent::CloseAndDelete()
{
  Close();
  Unused << Send__delete__(this);
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
