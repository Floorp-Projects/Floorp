/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoHost_h_
#define GMPVideoHost_h_

#include "gmp-video-host.h"
#include "gmp-video-plane.h"
#include "gmp-video-frame.h"
#include "gmp-video-host.h"
#include "nsTArray.h"

namespace mozilla {
namespace gmp {

class GMPSharedMemManager;
class GMPPlaneImpl;
class GMPVideoEncodedFrameImpl;

class GMPVideoHostImpl : public GMPVideoHost
{
public:
  explicit GMPVideoHostImpl(GMPSharedMemManager* aSharedMemMgr);
  virtual ~GMPVideoHostImpl();

  // Used for shared memory allocation and deallocation.
  GMPSharedMemManager* SharedMemMgr();
  void DoneWithAPI();
  void ActorDestroyed();
  void PlaneCreated(GMPPlaneImpl* aPlane);
  void PlaneDestroyed(GMPPlaneImpl* aPlane);
  void EncodedFrameCreated(GMPVideoEncodedFrameImpl* aEncodedFrame);
  void EncodedFrameDestroyed(GMPVideoEncodedFrameImpl* aFrame);

  // GMPVideoHost
  GMPErr CreateFrame(GMPVideoFrameFormat aFormat, GMPVideoFrame** aFrame) override;
  GMPErr CreatePlane(GMPPlane** aPlane) override;

private:
  // All shared memory allocations have to be made by an IPDL actor.
  // This is a reference to the owning actor. If this reference is
  // null then the actor has died and all allocations must fail.
  GMPSharedMemManager* mSharedMemMgr;

  // We track all of these things because they need to handle further
  // allocations through us and we need to notify them when they
  // can't use us any more.
  nsTArray<GMPPlaneImpl*> mPlanes;
  nsTArray<GMPVideoEncodedFrameImpl*> mEncodedFrames;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoHost_h_
