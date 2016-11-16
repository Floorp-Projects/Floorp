/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortChild.h"
#include "MessagePort.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
MessagePortChild::RecvStopSendingDataConfirmed()
{
  MOZ_ASSERT(mPort);
  mPort->StopSendingDataConfirmed();
  MOZ_ASSERT(!mPort);
  return IPC_OK();
}

mozilla::ipc::IPCResult
MessagePortChild::RecvEntangled(nsTArray<MessagePortMessage>&& aMessages)
{
  if (mPort) {
    mPort->Entangled(aMessages);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
MessagePortChild::RecvReceiveData(nsTArray<MessagePortMessage>&& aMessages)
{
  if (mPort) {
    mPort->MessagesReceived(aMessages);
  }
  return IPC_OK();
}

void
MessagePortChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mPort) {
    mPort->Closed();
    MOZ_ASSERT(!mPort);
  }
}

} // namespace dom
} // namespace mozilla
