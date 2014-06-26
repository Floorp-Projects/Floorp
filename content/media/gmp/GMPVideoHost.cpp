/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoHost.h"
#include "mozilla/Assertions.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPVideoEncodedFrameImpl.h"

namespace mozilla {
namespace gmp {

GMPVideoHostImpl::GMPVideoHostImpl(GMPSharedMemManager* aSharedMemMgr)
: mSharedMemMgr(aSharedMemMgr)
{
}

GMPVideoHostImpl::~GMPVideoHostImpl()
{
}

GMPVideoErr
GMPVideoHostImpl::CreateFrame(GMPVideoFrameFormat aFormat, GMPVideoFrame** aFrame)
{
  if (!mSharedMemMgr) {
    return GMPVideoGenericErr;
  }

  if (!aFrame) {
    return GMPVideoGenericErr;
  }
  *aFrame = nullptr;

  switch (aFormat) {
    case kGMPI420VideoFrame:
      *aFrame = new GMPVideoi420FrameImpl(this);
      return GMPVideoNoErr;
    case kGMPEncodedVideoFrame:
      *aFrame = new GMPVideoEncodedFrameImpl(this);
      return GMPVideoNoErr;
    default:
      NS_NOTREACHED("Unknown frame format!");
  }

  return GMPVideoGenericErr;
}

GMPVideoErr
GMPVideoHostImpl::CreatePlane(GMPPlane** aPlane)
{
  if (!mSharedMemMgr) {
    return GMPVideoGenericErr;
  }

  if (!aPlane) {
    return GMPVideoGenericErr;
  }
  *aPlane = nullptr;

  auto p = new GMPPlaneImpl(this);

  *aPlane = p;

  return GMPVideoNoErr;
}

GMPSharedMemManager*
GMPVideoHostImpl::SharedMemMgr()
{
  return mSharedMemMgr;
}

void
GMPVideoHostImpl::DoneWithAPI()
{
  for (uint32_t i = mPlanes.Length(); i > 0; i--) {
    mPlanes[i - 1]->DoneWithAPI();
    mPlanes.RemoveElementAt(i - 1);
  }
  for (uint32_t i = mEncodedFrames.Length(); i > 0; i--) {
    mEncodedFrames[i - 1]->DoneWithAPI();
    mEncodedFrames.RemoveElementAt(i - 1);
  }
  mSharedMemMgr = nullptr;
}

void
GMPVideoHostImpl::ActorDestroyed()
{
  for (uint32_t i = mPlanes.Length(); i > 0; i--) {
    mPlanes[i - 1]->ActorDestroyed();
    mPlanes.RemoveElementAt(i - 1);
  }
  for (uint32_t i = mEncodedFrames.Length(); i > 0; i--) {
    mEncodedFrames[i - 1]->ActorDestroyed();
    mEncodedFrames.RemoveElementAt(i - 1);
  }
  mSharedMemMgr = nullptr;  
}

void
GMPVideoHostImpl::PlaneCreated(GMPPlaneImpl* aPlane)
{
  mPlanes.AppendElement(aPlane);
}

void
GMPVideoHostImpl::PlaneDestroyed(GMPPlaneImpl* aPlane)
{
  MOZ_ALWAYS_TRUE(mPlanes.RemoveElement(aPlane));
}

void
GMPVideoHostImpl::EncodedFrameCreated(GMPVideoEncodedFrameImpl* aEncodedFrame)
{
  mEncodedFrames.AppendElement(aEncodedFrame);
}

void
GMPVideoHostImpl::EncodedFrameDestroyed(GMPVideoEncodedFrameImpl* aFrame)
{
  MOZ_ALWAYS_TRUE(mEncodedFrames.RemoveElement(aFrame));
}

} // namespace gmp
} // namespace mozilla
