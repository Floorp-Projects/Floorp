/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSystemResourceManagerParent.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/unused.h"

#include "MediaSystemResourceService.h"

using namespace mozilla::layers;

namespace mozilla {

/* static */ StaticRefPtr<MediaSystemResourceService> MediaSystemResourceService::sSingleton;

/* static */ MediaSystemResourceService*
MediaSystemResourceService::Get()
{
  if (sSingleton) {
    return sSingleton;
  }
  Init();
  return sSingleton;
}

/* static */ void
MediaSystemResourceService::Init()
{
  if (!sSingleton) {
    sSingleton = new MediaSystemResourceService();
  }
}

/* static */ void
MediaSystemResourceService::Shutdown()
{
  if (sSingleton) {
    sSingleton->Destroy();
    sSingleton = nullptr;
  }
}

MediaSystemResourceService::MediaSystemResourceService()
  : mDestroyed(false)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
#ifdef MOZ_WIDGET_GONK
  // The maximum number of hardware resoureces available.
  // XXX need to hange to a dynamic way.
  enum
  {
    VIDEO_DECODER_COUNT = 1,
    VIDEO_ENCODER_COUNT = 1
  };

  MediaSystemResource* resource;

  resource = new MediaSystemResource(VIDEO_DECODER_COUNT);
  mResources.Put(static_cast<uint32_t>(MediaSystemResourceType::VIDEO_DECODER), resource);

  resource = new MediaSystemResource(VIDEO_ENCODER_COUNT);
  mResources.Put(static_cast<uint32_t>(MediaSystemResourceType::VIDEO_ENCODER), resource);
#endif
}

MediaSystemResourceService::~MediaSystemResourceService()
{
}

void
MediaSystemResourceService::Destroy()
{
  mDestroyed = true;
}

void
MediaSystemResourceService::Acquire(media::MediaSystemResourceManagerParent* aParent,
                                    uint32_t aId,
                                    MediaSystemResourceType aResourceType,
                                    bool aWillWait)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MOZ_ASSERT(aParent);

  if (mDestroyed) {
    return;
  }

  MediaSystemResource* resource = mResources.Get(static_cast<uint32_t>(aResourceType));

  if (!resource ||
      resource->mResourceCount == 0) {
    // Resource does not exit
    // Send fail response
    mozilla::unused << aParent->SendResponse(aId, false /* fail */);
    return;
  }

  // Try to acquire a resource
  if (resource->mAcquiredRequests.size() < resource->mResourceCount) {
    // Resource is available
    resource->mAcquiredRequests.push_back(
      MediaSystemResourceRequest(aParent, aId));
    // Send success response
    mozilla::unused << aParent->SendResponse(aId, true /* success */);
    return;
  } else if (!aWillWait) {
    // Resource is not available and do not wait.
    // Send fail response
    mozilla::unused << aParent->SendResponse(aId, false /* fail */);
    return;
  }
  // Wait until acquire.
  resource->mWaitingRequests.push_back(
    MediaSystemResourceRequest(aParent, aId));
}

void
MediaSystemResourceService::ReleaseResource(media::MediaSystemResourceManagerParent* aParent,
                                            uint32_t aId,
                                            MediaSystemResourceType aResourceType)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MOZ_ASSERT(aParent);

  if (mDestroyed) {
    return;
  }

  MediaSystemResource* resource = mResources.Get(static_cast<uint32_t>(aResourceType));

  if (!resource ||
      resource->mResourceCount == 0) {
    // Resource does not exit
    return;
  }
  RemoveRequest(aParent, aId, aResourceType);
  UpdateRequests(aResourceType);
}

void
MediaSystemResourceService::ReleaseResource(media::MediaSystemResourceManagerParent* aParent)
{
  MOZ_ASSERT(aParent);

  if (mDestroyed) {
    return;
  }

  for (auto iter = mResources.Iter(); !iter.Done(); iter.Next()) {
    const uint32_t& key = iter.Key();
    RemoveRequests(aParent, static_cast<MediaSystemResourceType>(key));
    UpdateRequests(static_cast<MediaSystemResourceType>(key));
  }
}

void
MediaSystemResourceService::RemoveRequest(media::MediaSystemResourceManagerParent* aParent,
                                          uint32_t aId,
                                          MediaSystemResourceType aResourceType)
{
  MOZ_ASSERT(aParent);

  MediaSystemResource* resource = mResources.Get(static_cast<uint32_t>(aResourceType));
  if (!resource) {
    return;
  }

  std::deque<MediaSystemResourceRequest>::iterator it;
  std::deque<MediaSystemResourceRequest>& acquiredRequests =
    resource->mAcquiredRequests;
  for (it = acquiredRequests.begin(); it != acquiredRequests.end(); it++) {
    if (((*it).mParent == aParent) && ((*it).mId == aId)) {
      acquiredRequests.erase(it);
      return;
    }
  }

  std::deque<MediaSystemResourceRequest>& waitingRequests =
    resource->mWaitingRequests;
  for (it = waitingRequests.begin(); it != waitingRequests.end(); it++) {
    if (((*it).mParent == aParent) && ((*it).mId == aId)) {
      waitingRequests.erase(it);
      return;
    }
  }
}

void
MediaSystemResourceService::RemoveRequests(media::MediaSystemResourceManagerParent* aParent,
                                           MediaSystemResourceType aResourceType)
{
  MOZ_ASSERT(aParent);

  MediaSystemResource* resource = mResources.Get(static_cast<uint32_t>(aResourceType));

  if (!resource ||
      resource->mResourceCount == 0) {
    // Resource does not exit
    return;
  }

  std::deque<MediaSystemResourceRequest>::iterator it;
  std::deque<MediaSystemResourceRequest>& acquiredRequests =
    resource->mAcquiredRequests;
  for (it = acquiredRequests.begin(); it != acquiredRequests.end();) {
    if ((*it).mParent == aParent) {
      it = acquiredRequests.erase(it);
    } else {
      it++;
    }
  }

  std::deque<MediaSystemResourceRequest>& waitingRequests =
    resource->mWaitingRequests;
  for (it = waitingRequests.begin(); it != waitingRequests.end();) {
    if ((*it).mParent == aParent) {
      it = waitingRequests.erase(it);
    } else {
      it++;
    }
  }
}

void
MediaSystemResourceService::UpdateRequests(MediaSystemResourceType aResourceType)
{
  MediaSystemResource* resource = mResources.Get(static_cast<uint32_t>(aResourceType));

  if (!resource ||
      resource->mResourceCount == 0) {
    // Resource does not exit
    return;
  }

  std::deque<MediaSystemResourceRequest>& acquiredRequests =
    resource->mAcquiredRequests;
  std::deque<MediaSystemResourceRequest>& waitingRequests =
    resource->mWaitingRequests;

  while ((acquiredRequests.size() < resource->mResourceCount) &&
         (waitingRequests.size() > 0)) {
    MediaSystemResourceRequest& request = waitingRequests.front();
    MOZ_ASSERT(request.mParent);
    // Send response
    mozilla::unused << request.mParent->SendResponse(request.mId, true /* success */);
    // Move request to mAcquiredRequests
    acquiredRequests.push_back(waitingRequests.front());
    waitingRequests.pop_front();
  }
}

} // namespace mozilla
