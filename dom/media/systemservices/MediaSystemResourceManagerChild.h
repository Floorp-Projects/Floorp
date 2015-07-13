/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSystemResourceManagerChild_h_)
#define MediaSystemResourceManagerChild_h_

#include "mozilla/media/PMediaSystemResourceManagerChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {

class MediaSystemResourceManager;

namespace ipc {
class BackgroundChildImpl;
} // namespace ipc

namespace media {

/**
 * Handle MediaSystemResourceManager's IPC
 */
class MediaSystemResourceManagerChild final : public PMediaSystemResourceManagerChild
{
public:
  struct ResourceListener {
    /* The resource is reserved and can be granted.
     * The client can allocate the requested resource.
     */
    virtual void resourceReserved() = 0;
    /* The resource is not reserved any more.
     * The client should release the resource as soon as possible if the
     * resource is still being held.
     */
    virtual void resourceCanceled() = 0;
  };

  MediaSystemResourceManagerChild();
  virtual ~MediaSystemResourceManagerChild();

  void Destroy();

  void SetManager(MediaSystemResourceManager* aManager)
  {
    mManager = aManager;
  }

protected:
  bool RecvResponse(const uint32_t& aId,
                    const bool& aSuccess) override;

private:
  void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  bool mDestroyed;
  MediaSystemResourceManager* mManager;

  friend class mozilla::ipc::BackgroundChildImpl;
};

} // namespace media
} // namespace mozilla

#endif
