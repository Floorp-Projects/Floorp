/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkCameraImage.h"
#include "stagefright/MediaBuffer.h"

namespace mozilla {

GonkCameraImage::GonkCameraImage()
  : GrallocImage()
  , mMonitor("GonkCameraImage.Monitor")
  , mMediaBuffer(nullptr)
#ifdef DEBUG
  , mThread(nullptr)
#endif
{
  mFormat = ImageFormat::GONK_CAMERA_IMAGE;
}

GonkCameraImage::~GonkCameraImage()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  // mMediaBuffer must be cleared before destructor.
  MOZ_ASSERT(mMediaBuffer == nullptr);
}

nsresult
GonkCameraImage::GetMediaBuffer(android::MediaBuffer** aBuffer)
{
  ReentrantMonitorAutoEnter mon(mMonitor);

  if (!mMediaBuffer) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(NS_GetCurrentThread() == mThread);

  *aBuffer = mMediaBuffer;
  mMediaBuffer->add_ref();

  return NS_OK;
}

bool
GonkCameraImage::HasMediaBuffer()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  return mMediaBuffer != nullptr;
}

nsresult
GonkCameraImage::SetMediaBuffer(android::MediaBuffer* aBuffer)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  MOZ_ASSERT(!mMediaBuffer);

  mMediaBuffer = aBuffer;
  mMediaBuffer->add_ref();
#ifdef DEBUG
  mThread = NS_GetCurrentThread();
#endif
  return NS_OK;
}

nsresult
GonkCameraImage::ClearMediaBuffer()
{
  ReentrantMonitorAutoEnter mon(mMonitor);

  if (mMediaBuffer) {
    MOZ_ASSERT(NS_GetCurrentThread() == mThread);
    mMediaBuffer->release();
    mMediaBuffer = nullptr;
#ifdef DEBUG
    mThread = nullptr;
#endif
  }
  return NS_OK;
}

} // namespace mozilla


