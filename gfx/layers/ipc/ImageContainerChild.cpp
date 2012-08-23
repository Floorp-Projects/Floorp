/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "ImageContainerChild.h"
#include "gfxSharedImageSurface.h"
#include "ShadowLayers.h"
#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/SharedImageUtils.h"
#include "ImageContainer.h"
#include "GonkIOSurfaceImage.h"
#include "GrallocImages.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

/*
 * - POOL_MAX_SHARED_IMAGES is the maximum number number of shared images to
 * store in the ImageContainerChild's pool.
 *
 * - MAX_ACTIVE_SHARED_IMAGES is the maximum number of active shared images.
 * the number of active shared images for a given ImageContainerChild is equal
 * to the number of shared images allocated minus the number of shared images
 * dealocated by this ImageContainerChild. What can happen is that the compositor
 * hangs for a moment, while the ImageBridgeChild keep sending images. In such a 
 * scenario the compositor is not sending back shared images so the 
 * ImageContinerChild allocates new ones, and if the compositor hangs for too 
 * long, we can run out of shared memory. MAX_ACTIVE_SHARED_IMAGES is there to
 * throttle that. So when the child side wants to allocate a new shared image 
 * but is already at its maximum of active shared images, it just discards the
 * image (which is therefore not allocated and not sent to the compositor).
 * (see ImageToSharedImage)
 *
 * The values for the two constants are arbitrary and should be tweaked if it 
 * happens that we run into shared memory problems.
 */
static const unsigned int POOL_MAX_SHARED_IMAGES = 5;
static const unsigned int MAX_ACTIVE_SHARED_IMAGES = 10;

/**
 * A YCbCr image that stores its data in shared memory directly so that it's
 * data can be sent to the compositor without being copied.
 */
class SharedPlanarYCbCrImage : public PlanarYCbCrImage
{
  friend class mozilla::layers::ImageContainerChild;
public:
  SharedPlanarYCbCrImage(ImageContainerChild* aProtocol)
  : PlanarYCbCrImage(nullptr), mImageDataAllocator(aProtocol), mSent(false)
  {
    MOZ_COUNT_CTOR(SharedPlanarYCbCrImage);
  }

  ~SharedPlanarYCbCrImage()
  {
    if (mYSurface) {
      SharedImage* sharedImg = new SharedImage();
      ToSharedImage(sharedImg);
      mImageDataAllocator->RecycleSharedImage(sharedImg);
    }
    MOZ_COUNT_DTOR(SharedPlanarYCbCrImage);
  }

  /**
   * To prevent a SharedPlanarYCbCrImage from being sent several times we
   * store a boolean state telling wether the image has already been sent to
   * the compositor.
   */
  bool IsSent() const
  {
    return mSent;
  }

  SharedPlanarYCbCrImage* AsSharedPlanarYCbCrImage() MOZ_OVERRIDE
  {
    return this;
  }

  void ToSharedImage(SharedImage* aImage)
  {
    NS_ABORT_IF_FALSE(aImage, "aImage must be allocated");
    *aImage = YUVImage(mYSurface->GetShmem(),
                      mCbSurface->GetShmem(),
                      mCrSurface->GetShmem(),
                      mData.GetPictureRect(),
                      reinterpret_cast<uintptr_t>(this));
  }

  /**
   * Returns nullptr because in most cases the YCbCrImage has lost ownership of
   * its data when this is called.
   */
  already_AddRefed<gfxASurface> GetAsSurface()
  {
    if (!mYSurface) {
      return nullptr;
    }
    return PlanarYCbCrImage::GetAsSurface();
  }

  /**
   * See PlanarYCbCrImage::SeData.
   * This proxies a synchronous call to the ImageBridgeChild thread.
   */
  virtual void Allocate(Data& aData) MOZ_OVERRIDE
  {
    // copy aData into mData to have the sizes and strides info
    // that will be needed by the ImageContainerChild to allocate
    // shared buffers.
    mData = aData;
    // mImageDataAllocator will allocate the shared buffers and
    // place them into 'this'
    mImageDataAllocator->AllocateSharedBufferForImage(this);
    // give aData the resulting buffers
    aData.mYChannel  = mYSurface->Data();
    aData.mCbChannel = mCbSurface->Data();
    aData.mCrChannel = mCrSurface->Data();
  }

  /**
   * See PlanarYCbCrImage::SeData.
   */
  virtual void SetData(const Data& aData)
  {
    // do not set mBuffer like in PlanarYCbCrImage because the later
    // will try to manage this memory without knowing it belongs to a
    // shmem.
    mData = aData;
    mBufferSize = mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                   mData.mYStride * mData.mYSize.height;
    mSize = mData.mPicSize;

    // If m*Surface has not been allocated (through Allocate(aData)), allocate it.
    // This code path is slower than the one used when Allocate has been called
    // since it will trigger a full copy.
    if (!mYSurface) {
      Data data = aData;
      Allocate(data);
    }
    if (mYSurface->Data() != aData.mYChannel) {
      ImageContainer::CopyPlane(mYSurface->Data(), aData.mYChannel,
                                aData.mYSize,
                                aData.mYStride, aData.mYSize.width,
                                0,0);
      ImageContainer::CopyPlane(mCbSurface->Data(), aData.mCbChannel,
                                aData.mCbCrSize,
                                aData.mCbCrStride, aData.mCbCrSize.width,
                                0,0);
      ImageContainer::CopyPlane(mCrSurface->Data(), aData.mCrChannel,
                                aData.mCbCrSize,
                                aData.mCbCrStride, aData.mCbCrSize.width,
                                0,0);
    }
  }

  void MarkAsSent() {
    mSent = true;
  }

protected:
  nsRefPtr<gfxSharedImageSurface> mYSurface;
  nsRefPtr<gfxSharedImageSurface> mCbSurface;
  nsRefPtr<gfxSharedImageSurface> mCrSurface;
  nsRefPtr<ImageContainerChild>   mImageDataAllocator;
  bool mSent;
};


ImageContainerChild::ImageContainerChild()
: mImageContainerID(0), mActiveImageCount(0),
  mStop(false), mDispatchedDestroy(false) 
{
  MOZ_COUNT_CTOR(ImageContainerChild);
  // the Release corresponding to this AddRef is in 
  // ImageBridgeChild::DeallocPImageContainer
  AddRef();
}

ImageContainerChild::~ImageContainerChild()
{
  MOZ_COUNT_DTOR(ImageContainerChild);
}

void ImageContainerChild::DispatchStop()
{
  GetMessageLoop()->PostTask(FROM_HERE,
                  NewRunnableMethod(this, &ImageContainerChild::StopChildAndParent));
}

void ImageContainerChild::SetIdleNow() 
{
  if (mStop) return;

  SendFlush();
  ClearSharedImagePool();
}

void ImageContainerChild::DispatchSetIdle()
{
  if (mStop) return;

  GetMessageLoop()->PostTask(FROM_HERE, 
                    NewRunnableMethod(this, &ImageContainerChild::SetIdleNow));
}

void ImageContainerChild::StopChildAndParent()
{
  if (mStop) {
    return;
  }
  mStop = true;    

  SendStop(); // IPC message
  DispatchDestroy();
}

void ImageContainerChild::StopChild()
{
  if (mStop) {
    return;
  }
  mStop = true;

  DispatchDestroy();
}

bool ImageContainerChild::RecvReturnImage(const SharedImage& aImage)
{
  // Remove oldest image from the queue.
  if (mImageQueue.Length() > 0) {
    mImageQueue.RemoveElementAt(0);
  }

  if (aImage.type() == SharedImage::TYUVImage &&
      aImage.get_YUVImage().imageID() != 0) {
    // if the imageID is non-zero, it means that this image's buffers
    // are used in a SharedPlanarYCbCrImage that is maybe used in the
    // main thread. In this case we let ref counting take care of 
    // deciding when to recycle the shared memory.
    return true;
  }

  SharedImage* img = new SharedImage(aImage);
  RecycleSharedImageNow(img);

  return true;
}

void ImageContainerChild::DestroySharedImage(const SharedImage& aImage)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");

  --mActiveImageCount;
  DeallocSharedImageData(this, aImage);
}

bool ImageContainerChild::CopyDataIntoSharedImage(Image* src, SharedImage* dest)
{
  if ((src->GetFormat() == PLANAR_YCBCR) && 
      (dest->type() == SharedImage::TYUVImage)) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(src);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
    YUVImage& yuv = dest->get_YUVImage();

    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    nsRefPtr<gfxSharedImageSurface> surfV =
      gfxSharedImageSurface::Open(yuv.Vdata());

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(surfY->Data() + i * surfY->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }
    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(surfU->Data() + i * surfU->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(surfV->Data() + i * surfV->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    return true;
  }
  return false; // TODO: support more image formats
}

SharedImage* ImageContainerChild::CreateSharedImageFromData(Image* image)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                  "Should be in ImageBridgeChild thread.");
  
  ++mActiveImageCount;

  if (image->GetFormat() == PLANAR_YCBCR ) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(image);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
    
    nsRefPtr<gfxSharedImageSurface> tempBufferY;
    nsRefPtr<gfxSharedImageSurface> tempBufferU;
    nsRefPtr<gfxSharedImageSurface> tempBufferV;
    
    if (!AllocateSharedBuffer(this, data->mYSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(tempBufferY)) ||
        !AllocateSharedBuffer(this, data->mCbCrSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(tempBufferU)) ||
        !AllocateSharedBuffer(this, data->mCbCrSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(tempBufferV))) {
      NS_RUNTIMEABORT("creating SharedImage failed!");
    }

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(tempBufferY->Data() + i * tempBufferY->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }
    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(tempBufferU->Data() + i * tempBufferU->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(tempBufferV->Data() + i * tempBufferV->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    SharedImage* result = new SharedImage( 
              YUVImage(tempBufferY->GetShmem(),
                       tempBufferU->GetShmem(),
                       tempBufferV->GetShmem(),
                       data->GetPictureRect(),
                       0));
    NS_ABORT_IF_FALSE(result->type() == SharedImage::TYUVImage,
                      "SharedImage type not set correctly");
    return result;
#ifdef MOZ_WIDGET_GONK
  } else if (image->GetFormat() == GONK_IO_SURFACE) {
    GonkIOSurfaceImage* gonkImage = static_cast<GonkIOSurfaceImage*>(image);
    SharedImage* result = new SharedImage(gonkImage->GetSurfaceDescriptor());
    return result;
  } else if (image->GetFormat() == GRALLOC_PLANAR_YCBCR) {
    GrallocPlanarYCbCrImage* GrallocImage = static_cast<GrallocPlanarYCbCrImage*>(image);
    SharedImage* result = new SharedImage(GrallocImage->GetSurfaceDescriptor());
    return result;
#endif
  } else {
    NS_RUNTIMEABORT("TODO: Only YUVImage is supported here right now.");
  }
  return nullptr;
}

bool ImageContainerChild::AddSharedImageToPool(SharedImage* img)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(), 
                    "AddSharedImageToPool must be called in the ImageBridgeChild thread");
  if (mStop) {
    return false;
  }

  if (mSharedImagePool.Length() >= POOL_MAX_SHARED_IMAGES) {
    return false;
  }
  if (img->type() == SharedImage::TYUVImage) {
    mSharedImagePool.AppendElement(img);
    return true;
  }
  return false; // TODO accept more image formats in the pool
}

static bool
SharedImageCompatibleWith(SharedImage* aSharedImage, Image* aImage)
{
  // TODO accept more image formats
  switch (aImage->GetFormat()) {
  case PLANAR_YCBCR: {
    if (aSharedImage->type() != SharedImage::TYUVImage) {
      return false;
    }
    const PlanarYCbCrImage::Data* data =
      static_cast<PlanarYCbCrImage*>(aImage)->GetData();
    const YUVImage& yuv = aSharedImage->get_YUVImage();

    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    if (surfY->GetSize() != data->mYSize) {
      return false;
    }

    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    if (surfU->GetSize() != data->mCbCrSize) {
      return false;
    }
    return true;
  }
  default:
    return false;
  }
}

SharedImage*
ImageContainerChild::GetSharedImageFor(Image* aImage)
{
  while (mSharedImagePool.Length() > 0) {
    // i.e., img = mPool.pop()
    nsAutoPtr<SharedImage> img(mSharedImagePool.LastElement());
    mSharedImagePool.RemoveElementAt(mSharedImagePool.Length() - 1);

    if (SharedImageCompatibleWith(img, aImage)) {
      return img.forget();
    }
    // The cached image is stale, throw it out.
    DeallocSharedImageData(this, *img);
  }
  
  return nullptr;
}

void ImageContainerChild::ClearSharedImagePool()
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  for(unsigned int i = 0; i < mSharedImagePool.Length(); ++i) {
    DeallocSharedImageData(this, *mSharedImagePool[i]);
  }
  mSharedImagePool.Clear();
}

class ImageBridgeCopyAndSendTask : public Task
{
public:
  ImageBridgeCopyAndSendTask(ImageContainerChild * child, 
                             ImageContainer * aContainer, 
                             Image * aImage)
  : mChild(child), mImageContainer(aContainer), mImage(aImage) {}

  void Run()
  { 
    SharedImage* img = mChild->ImageToSharedImage(mImage.get());
    if (img) {
      mChild->SendPublishImage(*img);
    }
  }

  ImageContainerChild *mChild;
  nsRefPtr<ImageContainer> mImageContainer;
  nsRefPtr<Image> mImage;
};

SharedImage* ImageContainerChild::ImageToSharedImage(Image* aImage)
{
  if (mStop) {
    return nullptr;
  }

  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  if (aImage->GetFormat() == ImageFormat::PLANAR_YCBCR) {
    SharedPlanarYCbCrImage* sharedYCbCr =
      static_cast<PlanarYCbCrImage*>(aImage)->AsSharedPlanarYCbCrImage();
    if (sharedYCbCr) {
      if (sharedYCbCr->IsSent()) {
        // don't send the same image twice
        return nullptr;
      }
      // the image is already using shared memory, this means that we don't 
      // need to copy it
      SharedImage* result = new SharedImage();
      sharedYCbCr->ToSharedImage(result);
      sharedYCbCr->MarkAsSent();
      return result;
    }
  }

  if (mActiveImageCount > (int)MAX_ACTIVE_SHARED_IMAGES) {
    // Too many active shared images, perhaps the compositor is hanging.
    // Skipping current image
    return nullptr;
  }

  SharedImage *img = GetSharedImageFor(aImage);
  if (img) {
    CopyDataIntoSharedImage(aImage, img);  
  } else {
    img = CreateSharedImageFromData(aImage);
  }
  // Keep a reference to the image we sent to compositor to maintain a
  // correct reference count.
  mImageQueue.AppendElement(aImage);
  return img;
}

void ImageContainerChild::SendImageAsync(ImageContainer* aContainer,
                                         Image* aImage)
{
  if(!aContainer || !aImage) {
      return;
  }

  if (mStop) {
    return;
  }

  if (InImageBridgeChildThread()) {
    SharedImage *img = ImageToSharedImage(aImage);
    if (img) {
      SendPublishImage(*img);
    }
    delete img;
    return;
  }

  // Sending images and (potentially) allocating shmems must be done 
  // on the ImageBridgeChild thread.
  Task *t = new ImageBridgeCopyAndSendTask(this, aContainer, aImage);   
  GetMessageLoop()->PostTask(FROM_HERE, t);
}

void ImageContainerChild::DestroyNow()
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  NS_ABORT_IF_FALSE(mDispatchedDestroy,
                    "Incorrect state in the destruction sequence.");

  ClearSharedImagePool();

  // will decrease the refcount and, in most cases, delete the ImageContainerChild
  Send__delete__(this);
  Release(); // corresponds to the AddRef in DispatchDestroy
}

void ImageContainerChild::DispatchDestroy()
{
  NS_ABORT_IF_FALSE(mStop, "The state should be 'stopped' when destroying");

  if (mDispatchedDestroy) {
    return;
  }
  mDispatchedDestroy = true;
  AddRef(); // corresponds to the Release in DestroyNow
  GetMessageLoop()->PostTask(FROM_HERE, 
                    NewRunnableMethod(this, &ImageContainerChild::DestroyNow));
}


already_AddRefed<Image> ImageContainerChild::CreateImage(const ImageFormat *aFormats,
                                                         uint32_t aNumFormats)
{
  // TODO: Add more image formats
  nsRefPtr<Image> img = new SharedPlanarYCbCrImage(this);
  return img.forget();
}

void ImageContainerChild::AllocateSharedBufferForImageNow(Image* aImage)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  NS_ABORT_IF_FALSE(aImage && aImage->GetFormat() == ImageFormat::PLANAR_YCBCR,
                    "Only YUV images supported now");

  SharedPlanarYCbCrImage* sharedYCbCr =
    static_cast<PlanarYCbCrImage*>(aImage)->AsSharedPlanarYCbCrImage();

  // try to reuse shared images from the pool first...
  SharedImage* fromPool = GetSharedImageFor(aImage);
  if (fromPool) {
    YUVImage yuv = fromPool->get_YUVImage();
    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    nsRefPtr<gfxSharedImageSurface> surfV =
      gfxSharedImageSurface::Open(yuv.Vdata());
    sharedYCbCr->mYSurface  = surfY;
    sharedYCbCr->mCbSurface = surfU;
    sharedYCbCr->mCrSurface = surfV;
  } else {
    // ...else Allocate a shared image
    nsRefPtr<gfxSharedImageSurface> surfY;
    nsRefPtr<gfxSharedImageSurface> surfU;
    nsRefPtr<gfxSharedImageSurface> surfV;
    const PlanarYCbCrImage::Data* data = sharedYCbCr->GetData();
    if (!AllocateSharedBuffer(this, data->mYSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(surfY)) ||
        !AllocateSharedBuffer(this, data->mCbCrSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(surfU)) ||
        !AllocateSharedBuffer(this, data->mCbCrSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(surfV))) {
      NS_RUNTIMEABORT("creating SharedImage failed!");
    }
    sharedYCbCr->mYSurface  = surfY;
    sharedYCbCr->mCbSurface = surfU;
    sharedYCbCr->mCrSurface = surfV;
  }

}

void ImageContainerChild::AllocateSharedBufferForImageSync(ReentrantMonitor* aBarrier,
                                                           bool* aDone,
                                                           Image* aImage)
{
  AllocateSharedBufferForImageNow(aImage);
  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *aDone = true;
  aBarrier->NotifyAll();
}


void ImageContainerChild::AllocateSharedBufferForImage(Image* aImage)
{
  
  if (InImageBridgeChildThread()) {
    AllocateSharedBufferForImageNow(aImage);
    return;
  }

  bool done = false;
  ReentrantMonitor barrier("ImageBridgeChild::AllocateSharedBufferForImage");
  ReentrantMonitorAutoEnter autoMon(barrier);
  GetMessageLoop()->PostTask(FROM_HERE, 
                             NewRunnableMethod(this,
                                               &ImageContainerChild::AllocateSharedBufferForImageSync,
                                               &barrier,
                                               &done,
                                               aImage));
  while (!done) {
    barrier.Wait();
  }
}

void ImageContainerChild::RecycleSharedImageNow(SharedImage* aImage)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),"Must be in the ImageBridgeChild Thread.");
  
  if (mStop || !AddSharedImageToPool(aImage)) {
    DestroySharedImage(*aImage);
    delete aImage;
  }
}

void ImageContainerChild::RecycleSharedImage(SharedImage* aImage)
{
  if (InImageBridgeChildThread()) {
    RecycleSharedImageNow(aImage);
    return;
  }
  GetMessageLoop()->PostTask(FROM_HERE, 
                             NewRunnableMethod(this,
                                               &ImageContainerChild::RecycleSharedImageNow,
                                               aImage));
}

} // namespace
} // namespace
