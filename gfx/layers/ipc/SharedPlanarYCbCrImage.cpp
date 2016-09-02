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
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsISupportsImpl.h"            // for Image::AddRef
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;

SharedPlanarYCbCrImage::SharedPlanarYCbCrImage(ImageClient* aCompositable)
: mCompositable(aCompositable)
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
  }
}

size_t
SharedPlanarYCbCrImage::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  // NB: Explicitly skipping mTextureClient, the memory is already reported
  //     at time of allocation in GfxMemoryImageReporter.
  // Not owned:
  // - mCompositable
  return 0;
}

TextureClient*
SharedPlanarYCbCrImage::GetTextureClient(CompositableClient* aClient)
{
  return mTextureClient.get();
}

uint8_t*
SharedPlanarYCbCrImage::GetBuffer()
{
  // This should never be used
  MOZ_ASSERT(false);
  return nullptr;
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
SharedPlanarYCbCrImage::CopyData(const PlanarYCbCrData& aData)
{
  // If mTextureClient has not already been allocated (through Allocate(aData))
  // allocate it. This code path is slower than the one used when Allocate has
  // been called since it will trigger a full copy.
  PlanarYCbCrData data = aData;
  if (!mTextureClient && !Allocate(data)) {
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

// needs to be overriden because the parent class sets mBuffer which we
// do not want to happen.
uint8_t*
SharedPlanarYCbCrImage::AllocateAndGetNewBuffer(uint32_t aSize)
{
  MOZ_ASSERT(!mTextureClient, "This image already has allocated data");
  size_t size = ImageDataSerializer::ComputeYCbCrBufferSize(aSize);
  if (!size) {
    return nullptr;
  }

  mTextureClient = TextureClient::CreateForYCbCrWithBufferSize(mCompositable->GetForwarder(),
                                                               size,
                                                               mCompositable->GetTextureFlags());

  // get new buffer _without_ setting mBuffer.
  if (!mTextureClient) {
    return nullptr;
  }

  // update buffer size
  mBufferSize = size;

  MappedYCbCrTextureData mapped;
  if (mTextureClient->BorrowMappedYCbCrData(mapped)) {
    // The caller expects a pointer to the beginning of the writable part of the
    // buffer which is where the y channel starts by default.
    return mapped.y.data;
  } else {
    MOZ_CRASH("GFX: Cannot borrow mapped YCbCr data");
  }
}

bool
SharedPlanarYCbCrImage::AdoptData(const Data &aData)
{
  // AdoptData is used to update YUV plane offsets without (re)allocating
  // memory previously allocated with AllocateAndGetNewBuffer().

  MOZ_ASSERT(mTextureClient, "This Image should have already allocated data");
  if (!mTextureClient) {
    return false;
  }
  mData = aData;
  mSize = aData.mPicSize;
  mOrigin = gfx::IntPoint(aData.mPicX, aData.mPicY);

  uint8_t *base = GetBuffer();
  uint32_t yOffset = aData.mYChannel - base;
  uint32_t cbOffset = aData.mCbChannel - base;
  uint32_t crOffset = aData.mCrChannel - base;

  auto fwd = mCompositable->GetForwarder()->AsCompositableForwarder();
  bool hasIntermediateBuffer = fwd ? ComputeHasIntermediateBuffer(gfx::SurfaceFormat::YUV,
                                                                  fwd->GetCompositorBackendType())
                                   : true;

  static_cast<BufferTextureData*>(mTextureClient->GetInternalData())->SetDesciptor(
    YCbCrDescriptor(aData.mYSize, aData.mCbCrSize, yOffset, cbOffset, crOffset,
                    aData.mStereoMode, hasIntermediateBuffer)
  );

  return true;
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
  static const uint32_t MAX_POOLED_VIDEO_COUNT = 5;

  if (!mCompositable->HasTextureClientRecycler()) {
    // Initialize TextureClientRecycler
    mCompositable->GetTextureClientRecycler()->SetMaxPoolSize(MAX_POOLED_VIDEO_COUNT);
  }

  {
    YCbCrTextureClientAllocationHelper helper(aData, mCompositable->GetTextureFlags());
    mTextureClient = mCompositable->GetTextureClientRecycler()->CreateOrRecycle(helper);
  }

  if (!mTextureClient) {
    NS_WARNING("SharedPlanarYCbCrImage::Allocate failed.");
    return false;
  }

  MappedYCbCrTextureData mapped;
  // The locking here is sort of a lie. The SharedPlanarYCbCrImage just pulls
  // pointers out of the TextureClient and keeps them around, which works only
  // because the underlyin BufferTextureData is always mapped in memory even outside
  // of the lock/unlock interval. That's sad and new code should follow this example.
  if (!mTextureClient->Lock(OpenMode::OPEN_READ) || !mTextureClient->BorrowMappedYCbCrData(mapped)) {
    MOZ_CRASH("GFX: Cannot lock or borrow mapped YCbCr");
  }

  aData.mYChannel = mapped.y.data;
  aData.mCbChannel = mapped.cb.data;
  aData.mCrChannel = mapped.cr.data;

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
  mBufferSize = ImageDataSerializer::ComputeYCbCrBufferSize(mData.mYSize, mData.mCbCrSize);
  mSize = mData.mPicSize;
  mOrigin = gfx::IntPoint(aData.mPicX, aData.mPicY);

  mTextureClient->Unlock();

  return mBufferSize > 0;
}

} // namespace layers
} // namespace mozilla
