/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedPlanarYCbCrImage.h"
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for printf
#include "gfx2DGlue.h"                  // for Moz2D transition helpers
#include "ISurfaceAllocator.h"          // for ISurfaceAllocator, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Types.h"          // for SurfaceFormat::SurfaceFormat::YUV
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureClient.h"  // for BufferTextureClient, etc
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsISupportsImpl.h"            // for Image::AddRef
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;

SharedPlanarYCbCrImage::SharedPlanarYCbCrImage(ImageClient* aCompositable)
: PlanarYCbCrImage(nullptr)
, mCompositable(aCompositable)
{
  MOZ_COUNT_CTOR(SharedPlanarYCbCrImage);
}

SharedPlanarYCbCrImage::~SharedPlanarYCbCrImage() {
  MOZ_COUNT_DTOR(SharedPlanarYCbCrImage);

  if (mCompositable->GetAsyncID() != 0 &&
      !InImageBridgeChildThread()) {
    if (mTextureClient) {
      ADDREF_MANUALLY(mTextureClient);
      ImageBridgeChild::DispatchReleaseTextureClient(mTextureClient);
      mTextureClient = nullptr;
    }

    ImageBridgeChild::DispatchReleaseImageClient(mCompositable.forget().take());
  }
}

size_t
SharedPlanarYCbCrImage::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  // NB: Explicitly skipping mTextureClient, the memory is already reported
  //     at time of allocation in GfxMemoryImageReporter.
  // Not owned:
  // - mCompositable
  size_t size = PlanarYCbCrImage::SizeOfExcludingThis(aMallocSizeOf);
  return size;
}

TextureClient*
SharedPlanarYCbCrImage::GetTextureClient(CompositableClient* aClient)
{
  return mTextureClient.get();
}

uint8_t*
SharedPlanarYCbCrImage::GetBuffer()
{
  return mTextureClient ? mTextureClient->GetBuffer() : nullptr;
}

already_AddRefed<gfx::SourceSurface>
SharedPlanarYCbCrImage::GetAsSourceSurface()
{
  if (!mTextureClient) {
    NS_WARNING("Can't get as surface");
    return nullptr;
  }
  return PlanarYCbCrImage::GetAsSourceSurface();
}

bool
SharedPlanarYCbCrImage::SetData(const PlanarYCbCrData& aData)
{
  // If mTextureClient has not already been allocated (through Allocate(aData))
  // allocate it. This code path is slower than the one used when Allocate has
  // been called since it will trigger a full copy.
  PlanarYCbCrData data = aData;
  if (!mTextureClient && !Allocate(data)) {
    return false;
  }

  MOZ_ASSERT(mTextureClient->AsTextureClientYCbCr());
  if (!mTextureClient->Lock(OpenMode::OPEN_WRITE_ONLY)) {
    MOZ_ASSERT(false, "Failed to lock the texture.");
    return false;
  }
  TextureClientAutoUnlock unlock(mTextureClient);
  if (!mTextureClient->AsTextureClientYCbCr()->UpdateYCbCr(aData)) {
    MOZ_ASSERT(false, "Failed to copy YCbCr data into the TextureClient");
    return false;
  }
  mTextureClient->MarkImmutable();
  return true;
}

// needs to be overriden because the parent class sets mBuffer which we
// do not want to happen.
uint8_t*
SharedPlanarYCbCrImage::AllocateAndGetNewBuffer(uint32_t aSize)
{
  MOZ_ASSERT(!mTextureClient, "This image already has allocated data");
  size_t size = YCbCrImageDataSerializer::ComputeMinBufferSize(aSize);
  if (!size) {
    return nullptr;
  }

  mTextureClient = TextureClient::CreateWithBufferSize(mCompositable->GetForwarder(),
                                                       gfx::SurfaceFormat::YUV, size,
                                                       mCompositable->GetTextureFlags());

  // get new buffer _without_ setting mBuffer.
  if (!mTextureClient) {
    return nullptr;
  }

  // update buffer size
  mBufferSize = size;

  YCbCrImageDataSerializer serializer(mTextureClient->GetBuffer(), mTextureClient->GetBufferSize());
  return serializer.GetData();
}

bool
SharedPlanarYCbCrImage::SetDataNoCopy(const Data &aData)
{
  MOZ_ASSERT(mTextureClient, "This Image should have already allocated data");
  if (!mTextureClient) {
    return false;
  }
  mData = aData;
  mSize = aData.mPicSize;
  /* SetDataNoCopy is used to update YUV plane offsets without (re)allocating
   * memory previously allocated with AllocateAndGetNewBuffer().
   * serializer.GetData() returns the address of the memory previously allocated
   * with AllocateAndGetNewBuffer(), that we subtract from the Y, Cb, Cr
   * channels to compute 0-based offsets to pass to InitializeBufferInfo.
   */
  YCbCrImageDataSerializer serializer(mTextureClient->GetBuffer(), mTextureClient->GetBufferSize());
  uint8_t *base = serializer.GetData();
  uint32_t yOffset = aData.mYChannel - base;
  uint32_t cbOffset = aData.mCbChannel - base;
  uint32_t crOffset = aData.mCrChannel - base;
  serializer.InitializeBufferInfo(yOffset,
                                  cbOffset,
                                  crOffset,
                                  aData.mYStride,
                                  aData.mCbCrStride,
                                  aData.mYSize,
                                  aData.mCbCrSize,
                                  aData.mStereoMode);
  return true;
}

uint8_t*
SharedPlanarYCbCrImage::AllocateBuffer(uint32_t aSize)
{
  MOZ_ASSERT(!mTextureClient,
             "This image already has allocated data");
  mTextureClient = TextureClient::CreateWithBufferSize(mCompositable->GetForwarder(),
                                                       gfx::SurfaceFormat::YUV, aSize,
                                                       mCompositable->GetTextureFlags());
  if (!mTextureClient) {
    return nullptr;
  }
  return mTextureClient->GetBuffer();
}

bool
SharedPlanarYCbCrImage::IsValid() {
  return !!mTextureClient;
}

bool
SharedPlanarYCbCrImage::Allocate(PlanarYCbCrData& aData)
{
  MOZ_ASSERT(!mTextureClient,
             "This image already has allocated data");

  mTextureClient = TextureClient::CreateForYCbCr(mCompositable->GetForwarder(),
                                                 aData.mYSize, aData.mCbCrSize,
                                                 aData.mStereoMode,
                                                 mCompositable->GetTextureFlags());
  if (!mTextureClient) {
    NS_WARNING("SharedPlanarYCbCrImage::Allocate failed.");
    return false;
  }

  YCbCrImageDataSerializer serializer(mTextureClient->GetBuffer(), mTextureClient->GetBufferSize());
  serializer.InitializeBufferInfo(aData.mYSize,
                                  aData.mCbCrSize,
                                  aData.mStereoMode);
  MOZ_ASSERT(serializer.IsValid());

  aData.mYChannel = serializer.GetYData();
  aData.mCbChannel = serializer.GetCbData();
  aData.mCrChannel = serializer.GetCrData();

  // copy some of aData's values in mData (most of them)
  mData.mYChannel = aData.mYChannel;
  mData.mCbChannel = aData.mCbChannel;
  mData.mCrChannel = aData.mCrChannel;
  mData.mYSize = aData.mYSize;
  mData.mCbCrSize = aData.mCbCrSize;
  mData.mPicX = aData.mPicX;
  mData.mPicY = aData.mPicY;
  mData.mPicSize = aData.mPicSize;
  mData.mStereoMode = aData.mStereoMode;
  // those members are not always equal to aData's, due to potentially different
  // packing.
  mData.mYSkip = 0;
  mData.mCbSkip = 0;
  mData.mCrSkip = 0;
  mData.mYStride = mData.mYSize.width;
  mData.mCbCrStride = mData.mCbCrSize.width;

  // do not set mBuffer like in PlanarYCbCrImage because the later
  // will try to manage this memory without knowing it belongs to a
  // shmem.
  mBufferSize = YCbCrImageDataSerializer::ComputeMinBufferSize(mData.mYSize,
                                                               mData.mCbCrSize);
  mSize = mData.mPicSize;

  return mBufferSize > 0;
}

} // namespace layers
} // namespace mozilla
