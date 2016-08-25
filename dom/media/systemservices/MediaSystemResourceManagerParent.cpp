/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Unused.h"

#include "MediaSystemResourceManagerParent.h"

namespace mozilla {
namespace media {

using namespace ipc;

MediaSystemResourceManagerParent::MediaSystemResourceManagerParent()
  : mDestroyed(false)
{
  mMediaSystemResourceService = MediaSystemResourceService::Get();
}

MediaSystemResourceManagerParent::~MediaSystemResourceManagerParent()
{
  MOZ_ASSERT(mDestroyed);
}

bool
MediaSystemResourceManagerParent::RecvAcquire(const uint32_t& aId,
                                              const MediaSystemResourceType& aResourceType,
                                              const bool& aWillWait)
{
  MediaSystemResourceRequest* request = mResourceRequests.Get(aId);
  MOZ_ASSERT(!request);
  if (request) {
    // Send fail response
    mozilla::Unused << SendResponse(aId, false /* fail */);
    return true;
  }

  request = new MediaSystemResourceRequest(aId, aResourceType);
  mResourceRequests.Put(aId, request);
  mMediaSystemResourceService->Acquire(this, aId, aResourceType, aWillWait);
  return true;
}

bool
MediaSystemResourceManagerParent::RecvRelease(const uint32_t& aId)
{
  MediaSystemResourceRequest* request = mResourceRequests.Get(aId);
  if (!request) {
    return true;
  }

  mMediaSystemResourceService->ReleaseResource(this, aId, request->mResourceType);
  mResourceRequests.Remove(aId);
  return true;
}

bool
MediaSystemResourceManagerParent::RecvRemoveResourceManager()
{
  return PMediaSystemResourceManagerParent::Send__delete__(this);
}

void
MediaSystemResourceManagerParent::ActorDestroy(ActorDestroyReason aReason)
{
  MOZ_ASSERT(!mDestroyed);

  // Release all resource requests of the MediaSystemResourceManagerParent.
  // Clears all remaining pointers to this object. 
  mMediaSystemResourceService->ReleaseResource(this);

  mDestroyed = true;
}

} // namespace media
} // namespace mozilla
