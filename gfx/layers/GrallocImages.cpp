/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ImageBridgeChild.h"

#include "nsDebug.h"
#include "ImageContainer.h"
#include "GrallocImages.h"

using namespace mozilla::ipc;
using namespace android;

namespace mozilla {
namespace layers {

GrallocPlanarYCbCrImage::GrallocPlanarYCbCrImage()
  : PlanarYCbCrImage(nullptr)
{
  mFormat = GRALLOC_PLANAR_YCBCR;
}

GrallocPlanarYCbCrImage::~GrallocPlanarYCbCrImage()
{
  ImageBridgeChild *ibc = ImageBridgeChild::GetSingleton();
  ibc->DeallocSurfaceDescriptorGralloc(mSurfaceDescriptor);
}

void
GrallocPlanarYCbCrImage::SetData(const Data& aData)
{
  NS_PRECONDITION(aData.mYSize.width % 2 == 0, "Image should have even width");
  NS_PRECONDITION(aData.mYSize.height % 2 == 0, "Image should have even height");
  NS_PRECONDITION(aData.mYStride % 16 == 0, "Image should have stride of multiple of 16 pixels");

  mData = aData;
  mSize = aData.mPicSize;

  if (mSurfaceDescriptor.type() == SurfaceDescriptor::T__None) {
    ImageBridgeChild *ibc = ImageBridgeChild::GetSingleton();
    ibc->AllocSurfaceDescriptorGralloc(aData.mYSize,
                                       HAL_PIXEL_FORMAT_YV12,
                                       GraphicBuffer::USAGE_SW_READ_OFTEN |
                                       GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                                       GraphicBuffer::USAGE_HW_TEXTURE,
                                       &mSurfaceDescriptor);
  }
  sp<GraphicBuffer> graphicBuffer =
    GrallocBufferActor::GetFrom(mSurfaceDescriptor.get_SurfaceDescriptorGralloc());
  if (!graphicBuffer.get()) {
    return;
  }

  if (graphicBuffer->initCheck() != NO_ERROR) {
    return;
  }

  void* vaddr;
  if (graphicBuffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
                          &vaddr) != OK) {
    return;
  }

  uint8_t* yChannel = static_cast<uint8_t*>(vaddr);
  gfxIntSize ySize = gfxIntSize(aData.mYSize.width,
                                aData.mYSize.height);
  int32_t yStride = graphicBuffer->getStride();

  uint8_t* vChannel = yChannel + (yStride * ySize.height);
  gfxIntSize uvSize = gfxIntSize(ySize.width / 2,
                                 ySize.height / 2);
  // Align to 16 bytes boundary
  int32_t uvStride = ((yStride / 2) + 15) & ~0x0F;
  uint8_t* uChannel = vChannel + (uvStride * uvSize.height);

  // Memory outside of the image width may not writable. If the stride
  // equals to the image width then we can use only one copy.
  if (yStride == mData.mYStride &&
      yStride == ySize.width) {
    memcpy(yChannel, mData.mYChannel, yStride * ySize.height);
  } else {
    for (int i = 0; i < ySize.height; i++) {
      memcpy(yChannel + i * yStride,
             mData.mYChannel + i * mData.mYStride,
             ySize.width);
    }
  }
  if (uvStride == mData.mCbCrStride &&
      uvStride == uvSize.width) {
    memcpy(uChannel, mData.mCbChannel, uvStride * uvSize.height);
    memcpy(vChannel, mData.mCrChannel, uvStride * uvSize.height);
  } else {
    for (int i = 0; i < uvSize.height; i++) {
      memcpy(uChannel + i * uvStride,
             mData.mCbChannel + i * mData.mCbCrStride,
             uvSize.width);
      memcpy(vChannel + i * uvStride,
             mData.mCrChannel + i * mData.mCbCrStride,
             uvSize.width);
    }
  }
  graphicBuffer->unlock();
}

} // namespace layers
} // namespace mozilla
