/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Unused.h"
#include "mozilla/layers/PImageBridgeParent.h"

#include "MediaSystemResourceManagerParent.h"

namespace mozilla::media {

using namespace ipc;

MediaSystemResourceManagerParent::MediaSystemResourceManagerParent()
    : mDestroyed(false) {
  mMediaSystemResourceService = MediaSystemResourceService::Get();
}

MediaSystemResourceManagerParent::~MediaSystemResourceManagerParent() {
  MOZ_ASSERT(mDestroyed);
}

mozilla::ipc::IPCResult MediaSystemResourceManagerParent::RecvAcquire(
    const uint32_t& aId, const MediaSystemResourceType& aResourceType,
    const bool& aWillWait) {
  mResourceRequests.WithEntryHandle(aId, [&](auto&& request) {
    MOZ_ASSERT(!request);
    if (request) {
      // Send fail response
      mozilla::Unused << SendResponse(aId, false /* fail */);
      return;
    }

    request.Insert(MakeUnique<MediaSystemResourceRequest>(aId, aResourceType));
    mMediaSystemResourceService->Acquire(this, aId, aResourceType, aWillWait);
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult MediaSystemResourceManagerParent::RecvRelease(
    const uint32_t& aId) {
  MediaSystemResourceRequest* request = mResourceRequests.Get(aId);
  if (!request) {
    return IPC_OK();
  }

  mMediaSystemResourceService->ReleaseResource(this, aId,
                                               request->mResourceType);
  mResourceRequests.Remove(aId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
MediaSystemResourceManagerParent::RecvRemoveResourceManager() {
  IProtocol* mgr = Manager();
  if (!PMediaSystemResourceManagerParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

void MediaSystemResourceManagerParent::ActorDestroy(
    ActorDestroyReason aReason) {
  MOZ_ASSERT(!mDestroyed);

  // Release all resource requests of the MediaSystemResourceManagerParent.
  // Clears all remaining pointers to this object.
  mMediaSystemResourceService->ReleaseResource(this);

  mDestroyed = true;
}

}  // namespace mozilla::media
