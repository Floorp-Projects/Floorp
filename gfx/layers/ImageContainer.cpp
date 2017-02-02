/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "ImageContainer.h"
#include <string.h>                     // for memcpy, memset
#include "GLImages.h"                   // for SurfaceTextureImage
#include "gfx2DGlue.h"
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxUtils.h"                   // for gfxUtils
#include "libyuv.h"
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/ipc/CrossProcessMutex.h"  // for CrossProcessMutex, etc
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "mozilla/layers/SharedRGBImage.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsISupportsUtils.h"           // for NS_IF_ADDREF
#include "YCbCrUtils.h"                 // for YCbCr conversions
#include "gfx2DGlue.h"
#include "mozilla/gfx/2D.h"

#ifdef XP_MACOSX
#include "mozilla/gfx/QuartzSupport.h"
#endif

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#include <d3d10_1.h>
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace android;
using namespace mozilla::gfx;

Atomic<int32_t> Image::sSerialCounter(0);

Atomic<uint32_t> ImageContainer::sGenerationCounter(0);

RefPtr<PlanarYCbCrImage>
ImageFactory::CreatePlanarYCbCrImage(const gfx::IntSize& aScaleHint, BufferRecycleBin *aRecycleBin)
{
  return new RecyclingPlanarYCbCrImage(aRecycleBin);
}

BufferRecycleBin::BufferRecycleBin()
  : mLock("mozilla.layers.BufferRecycleBin.mLock")
  // This member is only valid when the bin is not empty and will be properly
  // initialized in RecycleBuffer, but initializing it here avoids static analysis
  // noise.
  , mRecycledBufferSize(0)
{
}

void
BufferRecycleBin::RecycleBuffer(UniquePtr<uint8_t[]> aBuffer, uint32_t aSize)
{
  MutexAutoLock lock(mLock);

  if (!mRecycledBuffers.IsEmpty() && aSize != mRecycledBufferSize) {
    mRecycledBuffers.Clear();
  }
  mRecycledBufferSize = aSize;
  mRecycledBuffers.AppendElement(Move(aBuffer));
}

UniquePtr<uint8_t[]>
BufferRecycleBin::GetBuffer(uint32_t aSize)
{
  MutexAutoLock lock(mLock);

  if (mRecycledBuffers.IsEmpty() || mRecycledBufferSize != aSize)
    return MakeUnique<uint8_t[]>(aSize);

  uint32_t last = mRecycledBuffers.Length() - 1;
  UniquePtr<uint8_t[]> result = Move(mRecycledBuffers[last]);
  mRecycledBuffers.RemoveElementAt(last);
  return result;
}

void
BufferRecycleBin::ClearRecycledBuffers()
{
  MutexAutoLock lock(mLock);
  if (!mRecycledBuffers.IsEmpty()) {
    mRecycledBuffers.Clear();
  }
  mRecycledBufferSize = 0;
}

ImageContainerListener::ImageContainerListener(ImageContainer* aImageContainer)
  : mLock("mozilla.layers.ImageContainerListener.mLock")
  , mImageContainer(aImageContainer)
{
}

ImageContainerListener::~ImageContainerListener()
{
}

void
ImageContainerListener::NotifyComposite(const ImageCompositeNotification& aNotification)
{
  MutexAutoLock lock(mLock);
  if (mImageContainer) {
    mImageContainer->NotifyComposite(aNotification);
  }
}

void
ImageContainerListener::ClearImageContainer()
{
  MutexAutoLock lock(mLock);
  mImageContainer = nullptr;
}

void
ImageContainer::EnsureImageClient()
{
  // If we're not forcing a new ImageClient, then we can skip this if we don't have an existing
  // ImageClient, or if the existing one belongs to an IPC actor that is still open.
  if (!mIsAsync) {
    return;
  }
  if (mImageClient && mImageClient->GetForwarder()->GetLayersIPCActor()->IPCOpen()) {
    return;
  }

  RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton();
  if (imageBridge) {
    mImageClient = imageBridge->CreateImageClient(CompositableType::IMAGE, this);
    if (mImageClient) {
      mAsyncContainerHandle = mImageClient->GetAsyncHandle();
      mNotifyCompositeListener = new ImageContainerListener(this);
    } else {
      // It's okay to drop the async container handle since the ImageBridgeChild
      // is going to die anyway.
      mAsyncContainerHandle = CompositableHandle();
      mNotifyCompositeListener = nullptr;
    }
  }
}

ImageContainer::ImageContainer(Mode flag)
: mReentrantMonitor("ImageContainer.mReentrantMonitor"),
  mGenerationCounter(++sGenerationCounter),
  mPaintCount(0),
  mDroppedImageCount(0),
  mImageFactory(new ImageFactory()),
  mRecycleBin(new BufferRecycleBin()),
  mIsAsync(flag == ASYNCHRONOUS),
  mCurrentProducerID(-1)
{
  if (flag == ASYNCHRONOUS) {
    EnsureImageClient();
  }
}

ImageContainer::ImageContainer(const CompositableHandle& aHandle)
  : mReentrantMonitor("ImageContainer.mReentrantMonitor"),
  mGenerationCounter(++sGenerationCounter),
  mPaintCount(0),
  mDroppedImageCount(0),
  mImageFactory(nullptr),
  mRecycleBin(nullptr),
  mIsAsync(true),
  mAsyncContainerHandle(aHandle),
  mCurrentProducerID(-1)
{
  MOZ_ASSERT(mAsyncContainerHandle);
}

ImageContainer::~ImageContainer()
{
  if (mNotifyCompositeListener) {
    mNotifyCompositeListener->ClearImageContainer();
  }
  if (mAsyncContainerHandle) {
    if (RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton()) {
      imageBridge->ForgetImageContainer(mAsyncContainerHandle);
    }
  }
}

RefPtr<PlanarYCbCrImage>
ImageContainer::CreatePlanarYCbCrImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  EnsureImageClient();
  if (mImageClient && mImageClient->AsImageClientSingle()) {
    return new SharedPlanarYCbCrImage(mImageClient);
  }
  return mImageFactory->CreatePlanarYCbCrImage(mScaleHint, mRecycleBin);
}

RefPtr<SharedRGBImage>
ImageContainer::CreateSharedRGBImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  EnsureImageClient();
  if (!mImageClient || !mImageClient->AsImageClientSingle()) {
    return nullptr;
  }
  return new SharedRGBImage(mImageClient);
}

void
ImageContainer::SetCurrentImageInternal(const nsTArray<NonOwningImage>& aImages)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mGenerationCounter = ++sGenerationCounter;

  if (!aImages.IsEmpty()) {
    NS_ASSERTION(mCurrentImages.IsEmpty() ||
                 mCurrentImages[0].mProducerID != aImages[0].mProducerID ||
                 mCurrentImages[0].mFrameID <= aImages[0].mFrameID,
                 "frame IDs shouldn't go backwards");
    if (aImages[0].mProducerID != mCurrentProducerID) {
      mFrameIDsNotYetComposited.Clear();
      mCurrentProducerID = aImages[0].mProducerID;
    } else if (!aImages[0].mTimeStamp.IsNull()) {
      // Check for expired frames
      for (auto& img : mCurrentImages) {
        if (img.mProducerID != aImages[0].mProducerID ||
            img.mTimeStamp.IsNull() ||
            img.mTimeStamp >= aImages[0].mTimeStamp) {
          break;
        }
        if (!img.mComposited && !img.mTimeStamp.IsNull() &&
            img.mFrameID != aImages[0].mFrameID) {
          mFrameIDsNotYetComposited.AppendElement(img.mFrameID);
        }
      }

      // Remove really old frames, assuming they'll never be composited.
      const uint32_t maxFrames = 100;
      if (mFrameIDsNotYetComposited.Length() > maxFrames) {
        uint32_t dropFrames = mFrameIDsNotYetComposited.Length() - maxFrames;
        mDroppedImageCount += dropFrames;
        mFrameIDsNotYetComposited.RemoveElementsAt(0, dropFrames);
      }
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
    for (auto& oldImg : mCurrentImages) {
      if (oldImg.mFrameID == img->mFrameID &&
          oldImg.mProducerID == img->mProducerID) {
        img->mComposited = oldImg.mComposited;
        break;
      }
    }
  }

  mCurrentImages.SwapElements(newImages);
}

void
ImageContainer::ClearImagesFromImageBridge()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  SetCurrentImageInternal(nsTArray<NonOwningImage>());
}

void
ImageContainer::SetCurrentImages(const nsTArray<NonOwningImage>& aImages)
{
  MOZ_ASSERT(!aImages.IsEmpty());
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mImageClient) {
    if (RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton()) {
      imageBridge->UpdateImageClient(mImageClient, this);
    }
  }
  SetCurrentImageInternal(aImages);
}

void
ImageContainer::ClearAllImages()
{
  if (mImageClient) {
    // Let ImageClient release all TextureClients. This doesn't return
    // until ImageBridge has called ClearCurrentImageFromImageBridge.
    if (RefPtr<ImageBridgeChild> imageBridge = ImageBridgeChild::GetSingleton()) {
      imageBridge->FlushAllImages(mImageClient, this);
    }
    return;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  SetCurrentImageInternal(nsTArray<NonOwningImage>());
}

void
ImageContainer::ClearCachedResources()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mImageClient && mImageClient->AsImageClientSingle()) {
    if (!mImageClient->HasTextureClientRecycler()) {
      return;
    }
    mImageClient->GetTextureClientRecycler()->ShrinkToMinimumSize();
    return;
  }
  return mRecycleBin->ClearRecycledBuffers();
}

void
ImageContainer::SetCurrentImageInTransaction(Image *aImage)
{
  AutoTArray<NonOwningImage,1> images;
  images.AppendElement(NonOwningImage(aImage));
  SetCurrentImagesInTransaction(images);
}

void
ImageContainer::SetCurrentImagesInTransaction(const nsTArray<NonOwningImage>& aImages)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(!mImageClient, "Should use async image transfer with ImageBridge.");

  SetCurrentImageInternal(aImages);
}

bool ImageContainer::IsAsync() const
{
  return mIsAsync;
}

CompositableHandle ImageContainer::GetAsyncContainerHandle()
{
  NS_ASSERTION(IsAsync(), "Shared image ID is only relevant to async ImageContainers");
  NS_ASSERTION(mAsyncContainerHandle, "Should have a shared image ID");
  EnsureImageClient();
  return mAsyncContainerHandle;
}

bool
ImageContainer::HasCurrentImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  return !mCurrentImages.IsEmpty();
}

void
ImageContainer::GetCurrentImages(nsTArray<OwningImage>* aImages,
                                 uint32_t* aGenerationCounter)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  *aImages = mCurrentImages;
  if (aGenerationCounter) {
    *aGenerationCounter = mGenerationCounter;
  }
}

gfx::IntSize
ImageContainer::GetCurrentSize()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mCurrentImages.IsEmpty()) {
    return gfx::IntSize(0, 0);
  }

  return mCurrentImages[0].mImage->GetSize();
}

void
ImageContainer::NotifyComposite(const ImageCompositeNotification& aNotification)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // An image composition notification is sent the first time a particular
  // image is composited by an ImageHost. Thus, every time we receive such
  // a notification, a new image has been painted.
  ++mPaintCount;

  if (aNotification.producerID() == mCurrentProducerID) {
    uint32_t i;
    for (i = 0; i < mFrameIDsNotYetComposited.Length(); ++i) {
      if (mFrameIDsNotYetComposited[i] <= aNotification.frameID()) {
        if (mFrameIDsNotYetComposited[i] < aNotification.frameID()) {
          ++mDroppedImageCount;
        }
      } else {
        break;
      }
    }
    mFrameIDsNotYetComposited.RemoveElementsAt(0, i);
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

PlanarYCbCrImage::PlanarYCbCrImage()
  : Image(nullptr, ImageFormat::PLANAR_YCBCR)
  , mOffscreenFormat(SurfaceFormat::UNKNOWN)
  , mBufferSize(0)
{
}

RecyclingPlanarYCbCrImage::~RecyclingPlanarYCbCrImage()
{
  if (mBuffer) {
    mRecycleBin->RecycleBuffer(Move(mBuffer), mBufferSize);
  }
}

size_t
RecyclingPlanarYCbCrImage::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
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

UniquePtr<uint8_t[]>
RecyclingPlanarYCbCrImage::AllocateBuffer(uint32_t aSize)
{
  return mRecycleBin->GetBuffer(aSize);
}

static void
CopyPlane(uint8_t *aDst, const uint8_t *aSrc,
          const gfx::IntSize &aSize, int32_t aStride, int32_t aSkip)
{
  if (!aSkip) {
    // Fast path: planar input.
    memcpy(aDst, aSrc, aSize.height * aStride);
  } else {
    int32_t height = aSize.height;
    int32_t width = aSize.width;
    for (int y = 0; y < height; ++y) {
      const uint8_t *src = aSrc;
      uint8_t *dst = aDst;
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

bool
RecyclingPlanarYCbCrImage::CopyData(const Data& aData)
{
  mData = aData;

  // update buffer size
  size_t size = mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                mData.mYStride * mData.mYSize.height;

  // get new buffer
  mBuffer = AllocateBuffer(size);
  if (!mBuffer)
    return false;

  // update buffer size
  mBufferSize = size;

  mData.mYChannel = mBuffer.get();
  mData.mCbChannel = mData.mYChannel + mData.mYStride * mData.mYSize.height;
  mData.mCrChannel = mData.mCbChannel + mData.mCbCrStride * mData.mCbCrSize.height;

  CopyPlane(mData.mYChannel, aData.mYChannel,
            mData.mYSize, mData.mYStride, mData.mYSkip);
  CopyPlane(mData.mCbChannel, aData.mCbChannel,
            mData.mCbCrSize, mData.mCbCrStride, mData.mCbSkip);
  CopyPlane(mData.mCrChannel, aData.mCrChannel,
            mData.mCbCrSize, mData.mCbCrStride, mData.mCrSkip);

  mSize = aData.mPicSize;
  mOrigin = gfx::IntPoint(aData.mPicX, aData.mPicY);
  return true;
}

gfxImageFormat
PlanarYCbCrImage::GetOffscreenFormat()
{
  return mOffscreenFormat == SurfaceFormat::UNKNOWN ?
    gfxVars::OffscreenFormat() :
    mOffscreenFormat;
}

bool
PlanarYCbCrImage::AdoptData(const Data &aData)
{
  mData = aData;
  mSize = aData.mPicSize;
  mOrigin = gfx::IntPoint(aData.mPicX, aData.mPicY);
  return true;
}

uint8_t*
RecyclingPlanarYCbCrImage::AllocateAndGetNewBuffer(uint32_t aSize)
{
  // get new buffer
  mBuffer = AllocateBuffer(aSize);
  if (mBuffer) {
    // update buffer size
    mBufferSize = aSize;
  }
  return mBuffer.get();
}

already_AddRefed<gfx::SourceSurface>
PlanarYCbCrImage::GetAsSourceSurface()
{
  if (mSourceSurface) {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  gfx::IntSize size(mSize);
  gfx::SurfaceFormat format = gfx::ImageFormatToSurfaceFormat(GetOffscreenFormat());
  gfx::GetYCbCrToRGBDestFormatAndSize(mData, format, size);
  if (mSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      mSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> surface = gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap mapping(surface, DataSourceSurface::WRITE);
  if (NS_WARN_IF(!mapping.IsMapped())) {
    return nullptr;
  }

  gfx::ConvertYCbCrToRGB(mData, format, size, mapping.GetData(), mapping.GetStride());

  mSourceSurface = surface;

  return surface.forget();
}

NVImage::NVImage()
  : Image(nullptr, ImageFormat::NV_IMAGE)
  , mBufferSize(0)
{
}

NVImage::~NVImage() = default;

IntSize
NVImage::GetSize()
{
  return mSize;
}

IntRect
NVImage::GetPictureRect()
{
  return mData.GetPictureRect();
}

already_AddRefed<SourceSurface>
NVImage::GetAsSourceSurface()
{
  if (mSourceSurface) {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  // Convert the current NV12 or NV21 data to YUV420P so that we can follow the
  // logics in PlanarYCbCrImage::GetAsSourceSurface().
  const int bufferLength = mData.mYSize.height * mData.mYStride +
                           mData.mCbCrSize.height * mData.mCbCrSize.width * 2;
  auto *buffer = new uint8_t[bufferLength];

  Data aData = mData;
  aData.mCbCrStride = aData.mCbCrSize.width;
  aData.mCbSkip = 0;
  aData.mCrSkip = 0;
  aData.mYChannel = buffer;
  aData.mCbChannel = aData.mYChannel + aData.mYSize.height * aData.mYStride;
  aData.mCrChannel = aData.mCbChannel + aData.mCbCrSize.height * aData.mCbCrStride;

  if (mData.mCbChannel < mData.mCrChannel) {  // NV12
    libyuv::NV12ToI420(mData.mYChannel, mData.mYStride,
                       mData.mCbChannel, mData.mCbCrStride,
                       aData.mYChannel, aData.mYStride,
                       aData.mCbChannel, aData.mCbCrStride,
                       aData.mCrChannel, aData.mCbCrStride,
                       aData.mYSize.width, aData.mYSize.height);
  } else {  // NV21
    libyuv::NV21ToI420(mData.mYChannel, mData.mYStride,
                       mData.mCrChannel, mData.mCbCrStride,
                       aData.mYChannel, aData.mYStride,
                       aData.mCbChannel, aData.mCbCrStride,
                       aData.mCrChannel, aData.mCbCrStride,
                       aData.mYSize.width, aData.mYSize.height);
  }

  // The logics in PlanarYCbCrImage::GetAsSourceSurface().
  gfx::IntSize size(mSize);
  gfx::SurfaceFormat format =
    gfx::ImageFormatToSurfaceFormat(gfxPlatform::GetPlatform()->GetOffscreenFormat());
  gfx::GetYCbCrToRGBDestFormatAndSize(aData, format, size);
  if (mSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      mSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> surface = gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap mapping(surface, DataSourceSurface::WRITE);
  if (NS_WARN_IF(!mapping.IsMapped())) {
    return nullptr;
  }

  gfx::ConvertYCbCrToRGB(aData, format, size, mapping.GetData(), mapping.GetStride());

  mSourceSurface = surface;

  // Release the temporary buffer.
  delete[] buffer;

  return surface.forget();
}

bool
NVImage::IsValid()
{
  return !!mBufferSize;
}

uint32_t
NVImage::GetBufferSize() const
{
  return mBufferSize;
}

NVImage*
NVImage::AsNVImage()
{
  return this;
};

bool
NVImage::SetData(const Data& aData)
{
  MOZ_ASSERT(aData.mCbSkip == 1 && aData.mCrSkip == 1);
  MOZ_ASSERT((int)std::abs(aData.mCbChannel - aData.mCrChannel) == 1);

  // Calculate buffer size
  const uint32_t size = aData.mYSize.height * aData.mYStride +
                        aData.mCbCrSize.height * aData.mCbCrStride;

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

const NVImage::Data*
NVImage::GetData() const
{
  return &mData;
}

UniquePtr<uint8_t>
NVImage::AllocateBuffer(uint32_t aSize)
{
  UniquePtr<uint8_t> buffer(new uint8_t[aSize]);
  return buffer;
}

SourceSurfaceImage::SourceSurfaceImage(const gfx::IntSize& aSize, gfx::SourceSurface* aSourceSurface)
  : Image(nullptr, ImageFormat::CAIRO_SURFACE),
    mSize(aSize),
    mSourceSurface(aSourceSurface),
    mTextureFlags(TextureFlags::DEFAULT)
{}

SourceSurfaceImage::SourceSurfaceImage(gfx::SourceSurface* aSourceSurface)
  : Image(nullptr, ImageFormat::CAIRO_SURFACE),
    mSize(aSourceSurface->GetSize()),
    mSourceSurface(aSourceSurface),
    mTextureFlags(TextureFlags::DEFAULT)
{}

SourceSurfaceImage::~SourceSurfaceImage() = default;

TextureClient*
SourceSurfaceImage::GetTextureClient(KnowsCompositor* aForwarder)
{
  if (!aForwarder) {
    return nullptr;
  }

  RefPtr<TextureClient> textureClient = mTextureClients.Get(aForwarder->GetSerial());
  if (textureClient) {
    return textureClient;
  }

  RefPtr<SourceSurface> surface = GetAsSourceSurface();
  MOZ_ASSERT(surface);
  if (!surface) {
    return nullptr;
  }

  if (!textureClient) {
    // gfx::BackendType::NONE means default to content backend
    textureClient = TextureClient::CreateFromSurface(aForwarder,
                                                     surface,
                                                     BackendSelector::Content,
                                                     mTextureFlags,
                                                     ALLOC_DEFAULT);
  }
  if (!textureClient) {
    return nullptr;
  }

  textureClient->SyncWithObject(aForwarder->GetSyncObject());

  mTextureClients.Put(aForwarder->GetSerial(), textureClient);
  return textureClient;
}

ImageContainer::ProducerID
ImageContainer::AllocateProducerID()
{
  // Callable on all threads.
  static Atomic<ImageContainer::ProducerID> sProducerID(0u);
  return ++sProducerID;
}

} // namespace layers
} // namespace mozilla
