/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSystemResourceManagerParent_h_)
#define MediaSystemResourceManagerParent_h_

#include "MediaSystemResourceManager.h"
#include "MediaSystemResourceService.h"
#include "MediaSystemResourceTypes.h"
#include "mozilla/media/PMediaSystemResourceManagerParent.h"

namespace mozilla {
namespace media {

/**
 * Handle MediaSystemResourceManager's IPC
 */
class MediaSystemResourceManagerParent final : public PMediaSystemResourceManagerParent
{
public:
  MediaSystemResourceManagerParent();
  virtual ~MediaSystemResourceManagerParent();

protected:
  bool RecvAcquire(const uint32_t& aId,
                   const MediaSystemResourceType& aResourceType,
                   const bool& aWillWait) override;

  bool RecvRelease(const uint32_t& aId) override;

  bool RecvRemoveResourceManager() override;

private:
  void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  struct MediaSystemResourceRequest {
    MediaSystemResourceRequest()
      : mId(-1), mResourceType(MediaSystemResourceType::INVALID_RESOURCE) {}
    MediaSystemResourceRequest(uint32_t aId, MediaSystemResourceType aResourceType)
      : mId(aId), mResourceType(aResourceType) {}
    int32_t mId;
    MediaSystemResourceType mResourceType;
  };

  bool mDestroyed;

  nsRefPtr<MediaSystemResourceService> mMediaSystemResourceService;

  nsClassHashtable<nsUint32HashKey, MediaSystemResourceRequest> mResourceRequests;
};

} // namespace media
} // namespace mozilla

#endif
