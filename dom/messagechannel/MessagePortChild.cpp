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

bool
MessagePortChild::RecvStopSendingDataConfirmed()
{
  MOZ_ASSERT(mPort);
  mPort->StopSendingDataConfirmed();
  MOZ_ASSERT(!mPort);
  return true;
}

bool
MessagePortChild::RecvEntangled(nsTArray<MessagePortMessage>&& aMessages)
{
  MOZ_ASSERT(mPort);
  mPort->Entangled(aMessages);
  return true;
}

bool
MessagePortChild::RecvReceiveData(nsTArray<MessagePortMessage>&& aMessages)
{
  if (mPort) {
    mPort->MessagesReceived(aMessages);
  }
  return true;
}

void
MessagePortChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mPort) {
    mPort->Closed();
    MOZ_ASSERT(!mPort);
  }
}

} // dom namespace
} // mozilla namespace
