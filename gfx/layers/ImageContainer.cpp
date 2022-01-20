/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageContainer.h"

#include <string.h>  // for memcpy, memset

#include "GLImages.h"    // for SurfaceTextureImage
#include "YCbCrUtils.h"  // for YCbCr conversions
#include "gfx2DGlue.h"
#include "gfxPlatform.h"  // for gfxPlatform
#include "gfxUtils.h"     // for gfxUtils
#include "libyuv.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefPtr.h"  // for already_AddRefed
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/CrossProcessMutex.h"  // for CrossProcessMutex, etc
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ImageBridgeChild.h"     // for ImageBridgeChild
#include "mozilla/layers/ImageClient.h"          // for ImageClient
#include "mozilla/layers/ImageDataSerializer.h"  // for SurfaceDescriptorBuffer
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "mozilla/layers/SharedRGBImage.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "nsProxyRelease.h"
#include "nsISupportsUtils.h"  // for NS_IF_ADDREF

#ifdef XP_MACOSX
#  include "MacIOSurfaceImage.h"
#  include "mozilla/gfx/QuartzSupport.h"
#endif

#ifdef XP_WIN
#  include <d3d10_1.h>

#  include "gfxWindowsPlatform.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/D3D11YCbCrImage.h"
#endif

namespace mozilla::layers {

using namespace mozilla::gfx;
using namespace mozilla::ipc;

Atomic<int32_t> Image::sSerialCounter(0);

Atomic<uint32_t> ImageContainer::sGenerationCounter(0);

static void CopyPlane(uint8_t* aDst, const uint8_t* aSrc,
                      const gfx::IntSize& aSize, int32_t aStride,
                      int32_t aSkip);

RefPtr<PlanarYCbCrImage> ImageFactory::CreatePlanarYCbCrImage(
    const gfx::IntSize& aScaleHint, BufferRecycleBin* aRecycleBin) {
  return new RecyclingPlanarYCbCrImage(aRecycleBin);
}

BufferRecycleBin::BufferRecycleBin()
    : mLock("mozilla.layers.BufferRecycleBin.mLock")
      // This member is only valid when the bin is not empty and will be
      // properly initialized in RecycleBuffer, but initializing it here avoids
      // static analysis noise.
      ,
      mRecycledBufferSize(0) {}

void BufferRecycleBin::RecycleBuffer(UniquePtr<uint8_t[]> aBuffer,
                                     uint32_t aSize) {
  MutexAutoLock lock(mLock);

  if (!mRecycledBuffers.IsEmpty() && aSize != mRecycledBufferSize) {
    mRecycledBuffers.Clear();
  }
  mRecycledBufferSize = aSize;
  mRecycledBuffers.AppendElement(std::move(aBuffer));
}

UniquePtr<uint8_t[]> BufferRecycleBin::GetBuffer(uint32_t aSize) {
  MutexAutoLock lock(mLock);

  if (mRecycledBuffers.IsEmpty() || mRecycledBufferSize != aSize) {
    return UniquePtr<uint8_t[]>(new (fallible) uint8_t[aSize]);
  }

  return mRecycledBuffers.PopLastElement();
}

void BufferRecycleBin::ClearRecycledBuffers() {
  MutexAutoLock lock(mLock);
  if (!mRecycledBuffers.IsEmpty()) {
    mRecycledBuffers.Clear();
  }
  mRecycledBufferSize = 0;
}

ImageContainerListener::ImageContainerListener(ImageContainer* aImageContainer)
    : mLock("mozilla.layers.ImageContainerListener.mLock"),
      mImageContainer(aImageContainer) {}

ImageContainerListener::~ImageContainerListener() = default;

void ImageContainerListener::NotifyComposite(
    const ImageCompositeNotification& aNotification) {
  MutexAutoLock lock(mLock);
  if (mImageContainer) {
    mImageContainer->NotifyComposite(aNotification);
  }
}

void ImageContainerListener::NotifyDropped(uint32_t aDropped) {
  MutexAutoLock lock(mLock);
  if (mImageContainer) {
    mImageContainer->NotifyDropped(aDropped);
  }
}

void ImageContainerListener::ClearImageContainer() {
  MutexAutoLock lock(mLock);
  mImageContainer = nullptr;
}

void ImageContainerListener::DropImageClient() {
  MutexAutoLock lock(mLock);
  if (mImageContainer) {
    mImageContainer->DropImageClient();
  }
}

already_AddRefed<ImageClient> ImageContainer::GetImageClient() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  EnsureImageClient();
  RefPtr<ImageClient> imageClient = mImageClient;
  return imageClient.forget();
}

void ImageContainer::DropImageClient() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  if (mImageClient) {
    mImageClient->ClearCachedResources();
    mImageClient = nullptr;
  }
}

void ImageContainer::EnsureImageClient() {
  // If we're not forcing a new ImageClient, then we can skip this if we don't
  // have an existing ImageClient, or if the existing one belongs to an IPC
  // actor that is still open.
  if (!mIsAsync) {
    return;
  }
  if (mImageClient &&
      mImageClient->GetForwarder()->GetLayersIPCActor()->IPCOpen()) {
    return;
  }

  RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton();
  if (imageBridge) {
    mImageClient =
        imageBridge->CreateImageClient(CompositableType::IMAGE, this);
    if (mImageClient) {
      mAsyncContainerHandle = mImageClient->GetAsyncHandle();
    } else {
      // It's okay to drop the async container handle since the ImageBridgeChild
      // is going to die anyway.
      mAsyncContainerHandle = CompositableHandle();
    }
  }
}

ImageContainer::ImageContainer(Mode flag)
    : mRecursiveMutex("ImageContainer.mRecursiveMutex"),
      mGenerationCounter(++sGenerationCounter),
      mPaintCount(0),
      mDroppedImageCount(0),
      mImageFactory(new ImageFactory()),
      mRecycleBin(new BufferRecycleBin()),
      mIsAsync(flag == ASYNCHRONOUS),
      mCurrentProducerID(-1) {
  if (flag == ASYNCHRONOUS) {
    mNotifyCompositeListener = new ImageContainerListener(this);
    EnsureImageClient();
  }
}

ImageContainer::ImageContainer(const CompositableHandle& aHandle)
    : mRecursiveMutex("ImageContainer.mRecursiveMutex"),
      mGenerationCounter(++sGenerationCounter),
      mPaintCount(0),
      mDroppedImageCount(0),
      mImageFactory(nullptr),
      mRecycleBin(nullptr),
      mIsAsync(true),
      mAsyncContainerHandle(aHandle),
      mCurrentProducerID(-1) {
  MOZ_ASSERT(mAsyncContainerHandle);
}

ImageContainer::~ImageContainer() {
  if (mNotifyCompositeListener) {
    mNotifyCompositeListener->ClearImageContainer();
  }
  if (mAsyncContainerHandle) {
    if (RefPtr<ImageBridgeChild> imageBridge =
            ImageBridgeChild::GetSingleton()) {
      imageBridge->ForgetImageContainer(mAsyncContainerHandle);
    }
  }
}

Maybe<SurfaceDescriptor> Image::GetDesc() { return {}; }

Maybe<SurfaceDescriptor> Image::GetDescFromTexClient(
    TextureClient* const forTc) {
  RefPtr<TextureClient> tc = forTc;
  if (!forTc) {
    tc = GetTextureClient(nullptr);
  }

  const auto& tcd = tc->GetInternalData();

  SurfaceDescriptor ret;
  if (!tcd->Serialize(ret)) {
    return {};
  }
  return Some(ret);
}

RefPtr<PlanarYCbCrImage> ImageContainer::CreatePlanarYCbCrImage() {
  RecursiveMutexAutoLock lock(mRecursiveMutex);
  EnsureImageClient();
  if (mImageClient && mImageClient->AsImageClientSingle()) {
    return new SharedPlanarYCbCrImage(mImageClient);
  }
  if (mRecycleAllocator) {
    return new SharedPlanarYCbCrImage(mRecycleAllocator);
  }
  return mImageFactory->CreatePlanarYCbCrImage(mScaleHint, mRecycleBin);
}

RefPtr<SharedRGBImage> ImageContainer::CreateSharedRGBImage() {
  RecursiveMutexAutoLock lock(mRecursiveMutex);
  EnsureImageClient();
  if (mImageClient && mImageClient->AsImageClientSingle()) {
    return new SharedRGBImage(mImageClient);
  }
  if (mRecycleAllocator) {
    return new SharedRGBImage(mRecycleAllocator);
  }
  return nullptr;
}

void ImageContainer::SetCurrentImageInternal(
    const nsTArray<NonOwningImage>& aImages) {
  RecursiveMutexAutoLock lock(mRecursiveMutex);

  mGenerationCounter = ++sGenerationCounter;

  if (!aImages.IsEmpty()) {
    NS_ASSERTION(mCurrentImages.IsEmpty() ||
                     mCurrentImages[0].mProducerID != aImages[0].mProducerID ||
                     mCurrentImages[0].mFrameID <= aImages[0].mFrameID,
                 "frame IDs shouldn't go backwards");
    if (aImages[0].mProducerID != mCurrentProducerID) {
      mCurrentProducerID = aImages[0].mProducerID;
    }
  }

  nsTArray<OwningImage> newImages;

  for (uint32_t i = 0; i < aImages.Length(); ++i) {
    NS_ASSERTION(aImages[i].mImage, "image can't be null");
    NS_ASSERTION(!aImages[i].mTimeStamp.IsNull() || aImages.Length() == 1,
                 "Multiple images require timestamps");
    if (i > 0) {
      NS_ASSERTION(aImages[i].mTimeStamp >= aImages[i - 1].mTimeStamp,
                   "Timestamps must not decrease");
      NS_ASSERTION(aImages[i].mFrameID > aImages[i - 1].mFrameID,
                   "FrameIDs must increase");
      NS_ASSERTION(aImages[i].mProducerID == aImages[i - 1].mProducerID,
                   "ProducerIDs must be the same");
    }
    OwningImage* img = newImages.AppendElement();
    img->mImage = aImages[i].mImage;
    img->mTimeStamp = aImages[i].mTimeStamp;
    img->mFrameID = aImages[i].mFrameID;
    img->mProducerID = aImages[i].mProducerID;
    for (const auto& oldImg : mCurrentImages) {
      if (oldImg.mFrameID == img->mFrameID &&
          oldImg.mProducerID == img->mProducerID) {
        img->mComposited = oldImg.mComposited;
        break;
      }
    }
  }

  mCurrentImages = std::move(newImages);
}

void ImageContainer::ClearImagesFromImageBridge() {
  RecursiveMutexAutoLock lock(mRecursiveMutex);
  SetCurrentImageInternal(nsTArray<NonOwningImage>());
}

void ImageContainer::SetCurrentImages(const nsTArray<NonOwningImage>& aImages) {
  AUTO_PROFILER_LABEL("ImageContainer::SetCurrentImages", GRAPHICS);
  MOZ_ASSERT(!aImages.IsEmpty());
  RecursiveMutexAutoLock lock(mRecursiveMutex);
  if (mIsAsync) {
    if (RefPtr<ImageBridgeChild> imageBridge =
            ImageBridgeChild::GetSingleton()) {
      imageBridge->UpdateImageClient(this);
    }
  }
  SetCurrentImageInternal(aImages);
}

void ImageContainer::ClearAllImages() {
  if (mImageClient) {
    // Let ImageClient release all TextureClients. This doesn't return
    // until ImageBridge has called ClearCurrentImageFromImageBridge.
    if (RefPtr<ImageBridgeChild> imageBridge =
            ImageBridgeChild::GetSingleton()) {
      imageBridge->FlushAllImages(mImageClient, this);
    }
    return;
  }

  RecursiveMutexAutoLock lock(mRecursiveMutex);
  SetCurrentImageInternal(nsTArray<NonOwningImage>());
}

void ImageContainer::ClearCachedResources() {
  RecursiveMutexAutoLock lock(mRecursiveMutex);
  if (mImageClient && mImageClient->AsImageClientSingle()) {
    if (!mImageClient->HasTextureClientRecycler()) {
      return;
    }
    mImageClient->GetTextureClientRecycler()->ShrinkToMinimumSize();
    return;
  }
  return mRecycleBin->ClearRecycledBuffers();
}

void ImageContainer::SetCurrentImageInTransaction(Image* aImage) {
  AutoTArray<NonOwningImage, 1> images;
  images.AppendElement(NonOwningImage(aImage));
  SetCurrentImagesInTransaction(images);
}

void ImageContainer::SetCurrentImagesInTransaction(
    const nsTArray<NonOwningImage>& aImages) {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(!mImageClient,
               "Should use async image transfer with ImageBridge.");

  SetCurrentImageInternal(aImages);
}

bool ImageContainer::IsAsync() const { return mIsAsync; }

CompositableHandle ImageContainer::GetAsyncContainerHandle() {
  NS_ASSERTION(IsAsync(),
               "Shared image ID is only relevant to async ImageContainers");
  NS_ASSERTION(mAsyncContainerHandle, "Should have a shared image ID");
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  EnsureImageClient();
  return mAsyncContainerHandle;
}

bool ImageContainer::HasCurrentImage() {
  RecursiveMutexAutoLock lock(mRecursiveMutex);

  return !mCurrentImages.IsEmpty();
}

void ImageContainer::GetCurrentImages(nsTArray<OwningImage>* aImages,
                                      uint32_t* aGenerationCounter) {
  RecursiveMutexAutoLock lock(mRecursiveMutex);

  *aImages = mCurrentImages.Clone();
  if (aGenerationCounter) {
    *aGenerationCounter = mGenerationCounter;
  }
}

gfx::IntSize ImageContainer::GetCurrentSize() {
  RecursiveMutexAutoLock lock(mRecursiveMutex);

  if (mCurrentImages.IsEmpty()) {
    return gfx::IntSize(0, 0);
  }

  return mCurrentImages[0].mImage->GetSize();
}

void ImageContainer::NotifyComposite(
    const ImageCompositeNotification& aNotification) {
  RecursiveMutexAutoLock lock(mRecursiveMutex);

  // An image composition notification is sent the first time a particular
  // image is composited by an ImageHost. Thus, every time we receive such
  // a notification, a new image has been painted.
  ++mPaintCount;

  if (aNotification.producerID() == mCurrentProducerID) {
    for (auto& img : mCurrentImages) {
      if (img.mFrameID == aNotification.frameID()) {
        img.mComposited = true;
      }
    }
  }

  if (!aNotification.imageTimeStamp().IsNull()) {
    mPaintDelay = aNotification.firstCompositeTimeStamp() -
                  aNotification.imageTimeStamp();
  }
}

void ImageContainer::NotifyDropped(uint32_t aDropped) {
  mDroppedImageCount += aDropped;
}

void ImageContainer::EnsureRecycleAllocatorForRDD(
    KnowsCompositor* aKnowsCompositor) {
  MOZ_ASSERT(!mIsAsync);
  MOZ_ASSERT(!mImageClient);
  MOZ_ASSERT(XRE_IsRDDProcess());

  if (mRecycleAllocator &&
      aKnowsCompositor == mRecycleAllocator->GetKnowsCompositor()) {
    return;
  }

  if (!StaticPrefs::layers_recycle_allocator_rdd_AtStartup()) {
    return;
  }

  static const uint32_t MAX_POOLED_VIDEO_COUNT = 5;

  mRecycleAllocator =
      new layers::TextureClientRecycleAllocator(aKnowsCompositor);
  mRecycleAllocator->SetMaxPoolSize(MAX_POOLED_VIDEO_COUNT);
}

#ifdef XP_WIN
D3D11YCbCrRecycleAllocator* ImageContainer::GetD3D11YCbCrRecycleAllocator(
    KnowsCompositor* aKnowsCompositor) {
  if (mD3D11YCbCrRecycleAllocator &&
      aKnowsCompositor == mD3D11YCbCrRecycleAllocator->GetKnowsCompositor()) {
    return mD3D11YCbCrRecycleAllocator;
  }

  if (!aKnowsCompositor->SupportsD3D11() ||
      !gfx::DeviceManagerDx::Get()->GetImageDevice()) {
    return nullptr;
  }

  mD3D11YCbCrRecycleAllocator =
      new D3D11YCbCrRecycleAllocator(aKnowsCompositor);
  return mD3D11YCbCrRecycleAllocator;
}
#endif

#ifdef XP_MACOSX
MacIOSurfaceRecycleAllocator*
ImageContainer::GetMacIOSurfaceRecycleAllocator() {
  if (!mMacIOSurfaceRecycleAllocator) {
    mMacIOSurfaceRecycleAllocator = new MacIOSurfaceRecycleAllocator();
  }

  return mMacIOSurfaceRecycleAllocator;
}
#endif

PlanarYCbCrImage::PlanarYCbCrImage()
    : Image(nullptr, ImageFormat::PLANAR_YCBCR),
      mOffscreenFormat(SurfaceFormat::UNKNOWN),
      mBufferSize(0) {}

nsresult PlanarYCbCrImage::BuildSurfaceDescriptorBuffer(
    SurfaceDescriptorBuffer& aSdBuffer,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  const PlanarYCbCrData* pdata = GetData();
  MOZ_ASSERT(pdata, "must have PlanarYCbCrData");
  MOZ_ASSERT(pdata->mYSkip == 0 && pdata->mCbSkip == 0 && pdata->mCrSkip == 0,
             "YCbCrDescriptor doesn't hold skip values");

  uint32_t yOffset;
  uint32_t cbOffset;
  uint32_t crOffset;
  ImageDataSerializer::ComputeYCbCrOffsets(
      pdata->mYStride, pdata->mYSize.height, pdata->mCbCrStride,
      pdata->mCbCrSize.height, yOffset, cbOffset, crOffset);

  uint32_t bufferSize = ImageDataSerializer::ComputeYCbCrBufferSize(
      pdata->mYSize, pdata->mYStride, pdata->mCbCrSize, pdata->mCbCrStride,
      yOffset, cbOffset, crOffset);

  aSdBuffer.data() = aAllocate(bufferSize);

  uint8_t* buffer = nullptr;
  const MemoryOrShmem& memOrShmem = aSdBuffer.data();
  switch (memOrShmem.type()) {
    case MemoryOrShmem::Tuintptr_t:
      buffer = reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
      break;
    case MemoryOrShmem::TShmem:
      buffer = memOrShmem.get_Shmem().get<uint8_t>();
      break;
    default:
      buffer = nullptr;
      break;
  }
  if (!buffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aSdBuffer.desc() = YCbCrDescriptor(
      pdata->GetPictureRect(), pdata->mYSize, pdata->mYStride, pdata->mCbCrSize,
      pdata->mCbCrStride, yOffset, cbOffset, crOffset, pdata->mStereoMode,
      pdata->mColorDepth, pdata->mYUVColorSpace, pdata->mColorRange);

  CopyPlane(buffer + yOffset, pdata->mYChannel, pdata->mYSize, pdata->mYStride,
            pdata->mYSkip);
  CopyPlane(buffer + cbOffset, pdata->mCbChannel, pdata->mCbCrSize,
            pdata->mCbCrStride, pdata->mCbSkip);
  CopyPlane(buffer + crOffset, pdata->mCrChannel, pdata->mCbCrSize,
            pdata->mCbCrStride, pdata->mCrSkip);
  return NS_OK;
}

RecyclingPlanarYCbCrImage::~RecyclingPlanarYCbCrImage() {
  if (mBuffer) {
    mRecycleBin->RecycleBuffer(std::move(mBuffer), mBufferSize);
  }
}

size_t RecyclingPlanarYCbCrImage::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // Ignoring:
  // - mData - just wraps mBuffer
  // - Surfaces should be reported under gfx-surfaces-*:
  //   - mSourceSurface
  // - Base class:
  //   - mImplData is not used
  // Not owned:
  // - mRecycleBin
  size_t size = aMallocSizeOf(mBuffer.get());

  // Could add in the future:
  // - mBackendData (from base class)

  return size;
}

UniquePtr<uint8_t[]> RecyclingPlanarYCbCrImage::AllocateBuffer(uint32_t aSize) {
  return mRecycleBin->GetBuffer(aSize);
}

static void CopyPlane(uint8_t* aDst, const uint8_t* aSrc,
                      const gfx::IntSize& aSize, int32_t aStride,
                      int32_t aSkip) {
  int32_t height = aSize.height;
  int32_t width = aSize.width;

  MOZ_RELEASE_ASSERT(width <= aStride);

  if (!aSkip) {
    // Fast path: planar input.
    memcpy(aDst, aSrc, height * aStride);
  } else {
    for (int y = 0; y < height; ++y) {
      const uint8_t* src = aSrc;
      uint8_t* dst = aDst;
      // Slow path
      for (int x = 0; x < width; ++x) {
        *dst++ = *src++;
        src += aSkip;
      }
      aSrc += aStride;
      aDst += aStride;
    }
  }
}

bool RecyclingPlanarYCbCrImage::CopyData(const Data& aData) {
  // update buffer size
  // Use uint32_t throughout to match AllocateBuffer's param and mBufferSize
  const auto checkedSize =
      CheckedInt<uint32_t>(aData.mCbCrStride) * aData.mCbCrSize.height * 2 +
      CheckedInt<uint32_t>(aData.mYStride) * aData.mYSize.height;

  if (!checkedSize.isValid()) return false;

  const auto size = checkedSize.value();

  // get new buffer
  mBuffer = AllocateBuffer(size);
  if (!mBuffer) return false;

  // update buffer size
  mBufferSize = size;

  mData = aData;
  mData.mYChannel = mBuffer.get();
  mData.mCbChannel = mData.mYChannel + mData.mYStride * mData.mYSize.height;
  mData.mCrChannel =
      mData.mCbChannel + mData.mCbCrStride * mData.mCbCrSize.height;
  mData.mYSkip = mData.mCbSkip = mData.mCrSkip = 0;

  CopyPlane(mData.mYChannel, aData.mYChannel, aData.mYSize, aData.mYStride,
            aData.mYSkip);
  CopyPlane(mData.mCbChannel, aData.mCbChannel, aData.mCbCrSize,
            aData.mCbCrStride, aData.mCbSkip);
  CopyPlane(mData.mCrChannel, aData.mCrChannel, aData.mCbCrSize,
            aData.mCbCrStride, aData.mCrSkip);

  mSize = aData.mPicSize;
  mOrigin = gfx::IntPoint(aData.mPicX, aData.mPicY);
  return true;
}

gfxImageFormat PlanarYCbCrImage::GetOffscreenFormat() const {
  return mOffscreenFormat == SurfaceFormat::UNKNOWN ? gfxVars::OffscreenFormat()
                                                    : mOffscreenFormat;
}

bool PlanarYCbCrImage::AdoptData(const Data& aData) {
  mData = aData;
  mSize = aData.mPicSize;
  mOrigin = gfx::IntPoint(aData.mPicX, aData.mPicY);
  return true;
}

already_AddRefed<gfx::SourceSurface> PlanarYCbCrImage::GetAsSourceSurface() {
  if (mSourceSurface) {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  gfx::IntSize size(mSize);
  gfx::SurfaceFormat format =
      gfx::ImageFormatToSurfaceFormat(GetOffscreenFormat());
  gfx::GetYCbCrToRGBDestFormatAndSize(mData, format, size);
  if (mSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      mSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> surface =
      gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap mapping(surface, DataSourceSurface::WRITE);
  if (NS_WARN_IF(!mapping.IsMapped())) {
    return nullptr;
  }

  gfx::ConvertYCbCrToRGB(mData, format, size, mapping.GetData(),
                         mapping.GetStride());

  mSourceSurface = surface;

  return surface.forget();
}

PlanarYCbCrImage::~PlanarYCbCrImage() {
  NS_ReleaseOnMainThread("PlanarYCbCrImage::mSourceSurface",
                         mSourceSurface.forget());
}

NVImage::NVImage() : Image(nullptr, ImageFormat::NV_IMAGE), mBufferSize(0) {}

NVImage::~NVImage() {
  NS_ReleaseOnMainThread("NVImage::mSourceSurface", mSourceSurface.forget());
}

IntSize NVImage::GetSize() const { return mSize; }

IntRect NVImage::GetPictureRect() const { return mData.GetPictureRect(); }

already_AddRefed<SourceSurface> NVImage::GetAsSourceSurface() {
  if (mSourceSurface) {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  // Convert the current NV12 or NV21 data to YUV420P so that we can follow the
  // logics in PlanarYCbCrImage::GetAsSourceSurface().
  const int bufferLength = mData.mYSize.height * mData.mYStride +
                           mData.mCbCrSize.height * mData.mCbCrSize.width * 2;
  UniquePtr<uint8_t[]> buffer(new uint8_t[bufferLength]);

  Data aData = mData;
  aData.mCbCrStride = aData.mCbCrSize.width;
  aData.mCbSkip = 0;
  aData.mCrSkip = 0;
  aData.mYChannel = buffer.get();
  aData.mCbChannel = aData.mYChannel + aData.mYSize.height * aData.mYStride;
  aData.mCrChannel =
      aData.mCbChannel + aData.mCbCrSize.height * aData.mCbCrStride;

  if (mData.mCbChannel < mData.mCrChannel) {  // NV12
    libyuv::NV12ToI420(mData.mYChannel, mData.mYStride, mData.mCbChannel,
                       mData.mCbCrStride, aData.mYChannel, aData.mYStride,
                       aData.mCbChannel, aData.mCbCrStride, aData.mCrChannel,
                       aData.mCbCrStride, aData.mYSize.width,
                       aData.mYSize.height);
  } else {  // NV21
    libyuv::NV21ToI420(mData.mYChannel, mData.mYStride, mData.mCrChannel,
                       mData.mCbCrStride, aData.mYChannel, aData.mYStride,
                       aData.mCbChannel, aData.mCbCrStride, aData.mCrChannel,
                       aData.mCbCrStride, aData.mYSize.width,
                       aData.mYSize.height);
  }

  // The logics in PlanarYCbCrImage::GetAsSourceSurface().
  gfx::IntSize size(mSize);
  gfx::SurfaceFormat format = gfx::ImageFormatToSurfaceFormat(
      gfxPlatform::GetPlatform()->GetOffscreenFormat());
  gfx::GetYCbCrToRGBDestFormatAndSize(aData, format, size);
  if (mSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      mSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> surface =
      gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap mapping(surface, DataSourceSurface::WRITE);
  if (NS_WARN_IF(!mapping.IsMapped())) {
    return nullptr;
  }

  gfx::ConvertYCbCrToRGB(aData, format, size, mapping.GetData(),
                         mapping.GetStride());

  mSourceSurface = surface;

  return surface.forget();
}

bool NVImage::IsValid() const { return !!mBufferSize; }

uint32_t NVImage::GetBufferSize() const { return mBufferSize; }

NVImage* NVImage::AsNVImage() { return this; };

bool NVImage::SetData(const Data& aData) {
  MOZ_ASSERT(aData.mCbSkip == 1 && aData.mCrSkip == 1);
  MOZ_ASSERT((int)std::abs(aData.mCbChannel - aData.mCrChannel) == 1);

  // Calculate buffer size
  // Use uint32_t throughout to match AllocateBuffer's param and mBufferSize
  const auto checkedSize =
      CheckedInt<uint32_t>(aData.mYSize.height) * aData.mYStride +
      CheckedInt<uint32_t>(aData.mCbCrSize.height) * aData.mCbCrStride;

  if (!checkedSize.isValid()) return false;

  const auto size = checkedSize.value();

  // Allocate a new buffer.
  mBuffer = AllocateBuffer(size);
  if (!mBuffer) {
    return false;
  }

  // Update mBufferSize.
  mBufferSize = size;

  // Update mData.
  mData = aData;
  mData.mYChannel = mBuffer.get();
  mData.mCbChannel = mData.mYChannel + (aData.mCbChannel - aData.mYChannel);
  mData.mCrChannel = mData.mYChannel + (aData.mCrChannel - aData.mYChannel);

  // Update mSize.
  mSize = aData.mPicSize;

  // Copy the input data into mBuffer.
  // This copies the y-channel and the interleaving CbCr-channel.
  memcpy(mData.mYChannel, aData.mYChannel, mBufferSize);

  return true;
}

const NVImage::Data* NVImage::GetData() const { return &mData; }

UniquePtr<uint8_t[]> NVImage::AllocateBuffer(uint32_t aSize) {
  UniquePtr<uint8_t[]> buffer(new uint8_t[aSize]);
  return buffer;
}

SourceSurfaceImage::SourceSurfaceImage(const gfx::IntSize& aSize,
                                       gfx::SourceSurface* aSourceSurface)
    : Image(nullptr, ImageFormat::CAIRO_SURFACE),
      mSize(aSize),
      mSourceSurface(aSourceSurface),
      mTextureFlags(TextureFlags::DEFAULT) {}

SourceSurfaceImage::SourceSurfaceImage(gfx::SourceSurface* aSourceSurface)
    : Image(nullptr, ImageFormat::CAIRO_SURFACE),
      mSize(aSourceSurface->GetSize()),
      mSourceSurface(aSourceSurface),
      mTextureFlags(TextureFlags::DEFAULT) {}

SourceSurfaceImage::~SourceSurfaceImage() {
  NS_ReleaseOnMainThread("SourceSurfaceImage::mSourceSurface",
                         mSourceSurface.forget());
}

TextureClient* SourceSurfaceImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  if (!aKnowsCompositor) {
    return nullptr;
  }

  return mTextureClients.WithEntryHandle(
      aKnowsCompositor->GetSerial(), [&](auto&& entry) -> TextureClient* {
        if (entry) {
          return entry->get();
        }

        RefPtr<TextureClient> textureClient;
        RefPtr<SourceSurface> surface = GetAsSourceSurface();
        MOZ_ASSERT(surface);
        if (surface) {
          // gfx::BackendType::NONE means default to content backend
          textureClient = TextureClient::CreateFromSurface(
              aKnowsCompositor, surface, BackendSelector::Content,
              mTextureFlags, ALLOC_DEFAULT);
        }
        if (textureClient) {
          textureClient->SyncWithObject(aKnowsCompositor->GetSyncObject());
          return entry.Insert(std::move(textureClient)).get();
        }

        return nullptr;
      });
}

ImageContainer::ProducerID ImageContainer::AllocateProducerID() {
  // Callable on all threads.
  static Atomic<ImageContainer::ProducerID> sProducerID(0u);
  return ++sProducerID;
}

}  // namespace mozilla::layers
