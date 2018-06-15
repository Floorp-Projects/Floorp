/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSystemResourceManager.h"

#include "MediaSystemResourceManagerChild.h"

namespace mozilla {
namespace media {

MediaSystemResourceManagerChild::MediaSystemResourceManagerChild()
  : mDestroyed(false)
  , mManager(nullptr)
{
}

MediaSystemResourceManagerChild::~MediaSystemResourceManagerChild()
{
}

mozilla::ipc::IPCResult
MediaSystemResourceManagerChild::RecvResponse(const uint32_t& aId,
                                              const bool& aSuccess)
{
  if (mManager) {
    mManager->RecvResponse(aId, aSuccess);
  }
  return IPC_OK();
}

void
MediaSystemResourceManagerChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  MOZ_ASSERT(!mDestroyed);
  if (mManager) {
    mManager->OnIpcClosed();
  }
  mDestroyed = true;
}

void
MediaSystemResourceManagerChild::Destroy()
{
  if (mDestroyed) {
    return;
  }
  SendRemoveResourceManager();
  // WARNING: |this| is dead, hands off
}

} // namespace media
} // namespace mozilla
