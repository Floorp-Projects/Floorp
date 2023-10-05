/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedPlanarYCbCrImage.h"
#include <stddef.h>                    // for size_t
#include <stdio.h>                     // for printf
#include "gfx2DGlue.h"                 // for Moz2D transition helpers
#include "ISurfaceAllocator.h"         // for ISurfaceAllocator, etc
#include "mozilla/Assertions.h"        // for MOZ_ASSERT, etc
#include "mozilla/gfx/Types.h"         // for SurfaceFormat::SurfaceFormat::YUV
#include "mozilla/ipc/SharedMemory.h"  // for SharedMemory, etc
#include "mozilla/layers/ImageClient.h"     // for ImageClient
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/mozalloc.h"                 // for operator delete
#include "nsISupportsImpl.h"                  // for Image::AddRef
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;

SharedPlanarYCbCrImage::SharedPlanarYCbCrImage(ImageClient* aCompositable)
    : mCompositable(aCompositable) {
  MOZ_COUNT_CTOR(SharedPlanarYCbCrImage);
}

SharedPlanarYCbCrImage::SharedPlanarYCbCrImage(
    TextureClientRecycleAllocator* aRecycleAllocator)
    : mRecycleAllocator(aRecycleAllocator) {
  MOZ_COUNT_CTOR(SharedPlanarYCbCrImage);
}

SharedPlanarYCbCrImage::~SharedPlanarYCbCrImage() {
  MOZ_COUNT_DTOR(SharedPlanarYCbCrImage);
}

TextureClientRecycleAllocator* SharedPlanarYCbCrImage::RecycleAllocator() {
  static const uint32_t MAX_POOLED_VIDEO_COUNT = 5;

  if (!mRecycleAllocator && mCompositable) {
    if (!mCompositable->HasTextureClientRecycler()) {
      // Initialize TextureClientRecycler
      mCompositable->GetTextureClientRecycler()->SetMaxPoolSize(
          MAX_POOLED_VIDEO_COUNT);
    }
    mRecycleAllocator = mCompositable->GetTextureClientRecycler();
  }
  return mRecycleAllocator;
}

size_t SharedPlanarYCbCrImage::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // NB: Explicitly skipping mTextureClient, the memory is already reported
  //     at time of allocation in GfxMemoryImageReporter.
  // Not owned:
  // - mCompositable
  return 0;
}

TextureClient* SharedPlanarYCbCrImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  return mTextureClient.get();
}

already_AddRefed<gfx::SourceSurface>
SharedPlanarYCbCrImage::GetAsSourceSurface() {
  if (!IsValid()) {
    NS_WARNING("Can't get as surface");
    return nullptr;
  }
  return PlanarYCbCrImage::GetAsSourceSurface();
}

bool SharedPlanarYCbCrImage::CopyData(const PlanarYCbCrData& aData) {
  // If mTextureClient has not already been allocated by CreateEmptyBuffer,
  // allocate it. This code path is slower than the one used when
  // CreateEmptyBuffer has been called since it will trigger a full copy.
  if (!mTextureClient &&
      !CreateEmptyBuffer(aData, aData.YDataSize(), aData.CbCrDataSize())) {
    return false;
  }

  TextureClientAutoLock autoLock(mTextureClient, OpenMode::OPEN_WRITE_ONLY);
  if (!autoLock.Succeeded()) {
    MOZ_ASSERT(false, "Failed to lock the texture.");
    return false;
  }

  if (!UpdateYCbCrTextureClient(mTextureClient, aData)) {
    MOZ_ASSERT(false, "Failed to copy YCbCr data into the TextureClient");
    return false;
  }
  mTextureClient->MarkImmutable();
  return true;
}

bool SharedPlanarYCbCrImage::AdoptData(const Data& aData) {
  MOZ_ASSERT(false, "This shouldn't be used.");
  return false;
}

bool SharedPlanarYCbCrImage::IsValid() const {
  return mTextureClient && mTextureClient->IsValid();
}

bool SharedPlanarYCbCrImage::CreateEmptyBuffer(const PlanarYCbCrData& aData,
                                               const gfx::IntSize& aYSize,
                                               const gfx::IntSize& aCbCrSize) {
  MOZ_ASSERT(!mTextureClient, "This image already has allocated data");

  TextureFlags flags =
      mCompositable ? mCompositable->GetTextureFlags() : TextureFlags::DEFAULT;
  {
    YCbCrTextureClientAllocationHelper helper(aData, aYSize, aCbCrSize, flags);
    mTextureClient = RecycleAllocator()->CreateOrRecycle(helper);
  }

  if (!mTextureClient) {
    NS_WARNING("SharedPlanarYCbCrImage::Allocate failed.");
    return false;
  }

  MappedYCbCrTextureData mapped;
  // The locking here is sort of a lie. The SharedPlanarYCbCrImage just pulls
  // pointers out of the TextureClient and keeps them around, which works only
  // because the underlyin BufferTextureData is always mapped in memory even
  // outside of the lock/unlock interval. That's sad and new code should follow
  // this example.
  if (!mTextureClient->Lock(OpenMode::OPEN_READ) ||
      !mTextureClient->BorrowMappedYCbCrData(mapped)) {
    MOZ_CRASH("GFX: Cannot lock or borrow mapped YCbCr");
  }

  // copy some of aData's values in mData (most of them)
  mData.mYChannel = mapped.y.data;
  mData.mCbChannel = mapped.cb.data;
  mData.mCrChannel = mapped.cr.data;
  mData.mPictureRect = aData.mPictureRect;
  mData.mStereoMode = aData.mStereoMode;
  mData.mYUVColorSpace = aData.mYUVColorSpace;
  mData.mColorDepth = aData.mColorDepth;
  mData.mChromaSubsampling = aData.mChromaSubsampling;
  // those members are not always equal to aData's, due to potentially different
  // packing.
  mData.mYSkip = 0;
  mData.mCbSkip = 0;
  mData.mCrSkip = 0;
  mData.mYStride = aData.mYStride;
  mData.mCbCrStride = aData.mCbCrStride;

  // do not set mBuffer like in PlanarYCbCrImage because the later
  // will try to manage this memory without knowing it belongs to a
  // shmem.
  mBufferSize = ImageDataSerializer::ComputeYCbCrBufferSize(
      aYSize, mData.mYStride, aCbCrSize, mData.mCbCrStride);
  mSize = mData.mPictureRect.Size();
  mOrigin = mData.mPictureRect.TopLeft();

  mTextureClient->Unlock();

  return mBufferSize > 0;
}

void SharedPlanarYCbCrImage::SetIsDRM(bool aIsDRM) {
  Image::SetIsDRM(aIsDRM);
  if (mTextureClient) {
    mTextureClient->AddFlags(TextureFlags::DRM_SOURCE);
  }
}

}  // namespace layers
}  // namespace mozilla
