/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageContainerChild.h"
#include "ImageContainer.h"
#include "mozilla/Assertions.h"
#include "mozilla/layers/ImageBridgeChild.h"

namespace mozilla {
namespace layers {

ImageContainerChild::ImageContainerChild(ImageContainer* aImageContainer)
  : mLock("ImageContainerChild")
  , mImageContainer(aImageContainer)
  , mImageContainerReleased(false)
  , mIPCOpen(true)
{
}

void
ImageContainerChild::ForgetImageContainer()
{
  MutexAutoLock lock(mLock);
  mImageContainer = nullptr;
}

void
ImageContainerChild::NotifyComposite(const ImageCompositeNotification& aNotification)
{
  MOZ_ASSERT(InImageBridgeChildThread());

  MutexAutoLock lock(mLock);
  if (mImageContainer) {
    mImageContainer->NotifyCompositeInternal(aNotification);
  }
}

} // namespace layers
} // namespace mozilla
