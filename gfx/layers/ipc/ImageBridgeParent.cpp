/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/ImageContainerParent.h"
#include "mozilla/layers/CompositorParent.h"

#include "base/thread.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {


ImageBridgeParent::ImageBridgeParent(MessageLoop* aLoop)
: mMessageLoop(aLoop)
{
  ImageContainerParent::CreateSharedImageMap();
}

ImageBridgeParent::~ImageBridgeParent()
{
  ImageContainerParent::DestroySharedImageMap();
}

bool ImageBridgeParent::RecvStop()
{
  int numChildren = ManagedPImageContainerParent().Length();
  for (unsigned int i = 0; i < numChildren; ++i) {
    static_cast<ImageContainerParent*>(
      ManagedPImageContainerParent()[i]
    )->DoStop();
  }
  return true;
}

static  PRUint64 GenImageContainerID() {
  static PRUint64 sNextImageID = 1;
  
  ++sNextImageID;
  return sNextImageID;
}
  
PGrallocBufferParent*
ImageBridgeParent::AllocPGrallocBuffer(const gfxIntSize& aSize,
                                       const uint32_t& aFormat,
                                       const uint32_t& aUsage,
                                       MaybeMagicGrallocBufferHandle* aOutHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  return GrallocBufferActor::Create(aSize, aFormat, aUsage, aOutHandle);
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return nullptr;
#endif
}

bool
ImageBridgeParent::DeallocPGrallocBuffer(PGrallocBufferParent* actor)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  delete actor;
  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
}

PImageContainerParent* ImageBridgeParent::AllocPImageContainer(PRUint64* aID)
{
  PRUint64 id = GenImageContainerID();
  *aID = id;
  return new ImageContainerParent(id);
}

bool ImageBridgeParent::DeallocPImageContainer(PImageContainerParent* toDealloc)
{
  delete toDealloc;
  return true;
}


MessageLoop * ImageBridgeParent::GetMessageLoop() {
  return mMessageLoop;
}


} // layers
} // mozilla

