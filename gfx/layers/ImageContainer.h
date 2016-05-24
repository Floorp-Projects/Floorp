/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGECONTAINER_H
#define GFX_IMAGECONTAINER_H

#include <stdint.h>                     // for uint32_t, uint8_t, uint64_t
#include <sys/types.h>                  // for int32_t
#include "gfxTypes.h"
#include "ImageTypes.h"                 // for ImageFormat, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Mutex.h"              // for Mutex
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitorAutoEnter, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/gfx/Point.h"          // For IntSize
#include "mozilla/layers/GonkNativeHandle.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr, nsAutoArrayPtr, etc
#include "nsAutoRef.h"                  // for nsCountedRef
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::Release, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsTArray.h"                   // for nsTArray
#include "mozilla/Atomics.h"
#include "mozilla/WeakPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsDataHashtable.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/UniquePtr.h"

#ifndef XPCOM_GLUE_AVOID_NSPR
/**
 * We need to be able to hold a reference to a Moz2D SourceSurface from Image
 * subclasses. This is potentially a problem since Images can be addrefed
 * or released off the main thread. We can ensure that we never AddRef
 * a SourceSurface off the main thread, but we might want to Release due
 * to an Image being destroyed off the main thread.
 *
 * We use nsCountedRef<nsMainThreadSourceSurfaceRef> to reference the
 * SourceSurface. When AddRefing, we assert that we're on the main thread.
 * When Releasing, if we're not on the main thread, we post an event to
 * the main thread to do the actual release.
 */
class nsMainThreadSourceSurfaceRef;

template <>
class nsAutoRefTraits<nsMainThreadSourceSurfaceRef> {
public:
  typedef mozilla::gfx::SourceSurface* RawRef;

  /**
   * The XPCOM event that will do the actual release on the main thread.
   */
  class SurfaceReleaser : public mozilla::Runnable {
  public:
    explicit SurfaceReleaser(RawRef aRef) : mRef(aRef) {}
    NS_IMETHOD Run() {
      mRef->Release();
      return NS_OK;
    }
    RawRef mRef;
  };

  static RawRef Void() { return nullptr; }
  static void Release(RawRef aRawRef)
  {
    if (NS_IsMainThread()) {
      aRawRef->Release();
      return;
    }
    nsCOMPtr<nsIRunnable> runnable = new SurfaceReleaser(aRawRef);
    NS_DispatchToMainThread(runnable);
  }
  static void AddRef(RawRef aRawRef)
  {
    NS_ASSERTION(NS_IsMainThread(),
                 "Can only add a reference on the main thread");
    aRawRef->AddRef();
  }
};

class nsOwningThreadSourceSurfaceRef;

template <>
class nsAutoRefTraits<nsOwningThreadSourceSurfaceRef> {
public:
  typedef mozilla::gfx::SourceSurface* RawRef;

  /**
   * The XPCOM event that will do the actual release on the creation thread.
   */
  class SurfaceReleaser : public mozilla::Runnable {
  public:
    explicit SurfaceReleaser(RawRef aRef) : mRef(aRef) {}
    NS_IMETHOD Run() {
      mRef->Release();
      return NS_OK;
    }
    RawRef mRef;
  };

  static RawRef Void() { return nullptr; }
  void Release(RawRef aRawRef)
  {
    MOZ_ASSERT(mOwningThread);
    bool current;
    mOwningThread->IsOnCurrentThread(&current);
    if (current) {
      aRawRef->Release();
      return;
    }
    nsCOMPtr<nsIRunnable> runnable = new SurfaceReleaser(aRawRef);
    mOwningThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL);
  }
  void AddRef(RawRef aRawRef)
  {
    MOZ_ASSERT(!mOwningThread);
    NS_GetCurrentThread(getter_AddRefs(mOwningThread));
    aRawRef->AddRef();
  }

private:
  nsCOMPtr<nsIThread> mOwningThread;
};

#endif

#ifdef XP_WIN
struct ID3D10Texture2D;
struct ID3D10Device;
struct ID3D10ShaderResourceView;
#endif

typedef void* HANDLE;

namespace mozilla {


namespace layers {

class ImageClient;
class ImageCompositeNotification;
class ImageContainerChild;
class PImageContainerChild;
class SharedPlanarYCbCrImage;
class PlanarYCbCrImage;
class TextureClient;
class CompositableClient;
class GrallocImage;

struct ImageBackendData
{
  virtual ~ImageBackendData() {}

protected:
  ImageBackendData() {}
};

/* Forward declarations for Image derivatives. */
class EGLImageImage;
class SharedRGBImage;
#ifdef MOZ_WIDGET_ANDROID
class SurfaceTextureImage;
#elif defined(XP_MACOSX)
class MacIOSurfaceImage;
#elif defined(MOZ_WIDGET_GONK)
class OverlayImage;
#endif

/**
 * A class representing a buffer of pixel data. The data can be in one
 * of various formats including YCbCr.
 * 
 * Create an image using an ImageContainer. Fill the image with data, and
 * then call ImageContainer::SetImage to display it. An image must not be
 * modified after calling SetImage. Image implementations do not need to
 * perform locking; when filling an Image, the Image client is responsible
 * for ensuring only one thread accesses the Image at a time, and after
 * SetImage the image is immutable.
 * 
 * When resampling an Image, only pixels within the buffer should be
 * sampled. For example, cairo images should be sampled in EXTEND_PAD mode.
 */
class Image {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Image)

public:
  ImageFormat GetFormat() { return mFormat; }
  void* GetImplData() { return mImplData; }

  virtual gfx::IntSize GetSize() = 0;
  virtual gfx::IntPoint GetOrigin()
  {
    return gfx::IntPoint(0, 0);
  }
  virtual gfx::IntRect GetPictureRect()
  {
    return gfx::IntRect(GetOrigin().x, GetOrigin().y, GetSize().width, GetSize().height);
  }

  ImageBackendData* GetBackendData(LayersBackend aBackend)
  { return mBackendData[aBackend]; }
  void SetBackendData(LayersBackend aBackend, ImageBackendData* aData)
  { mBackendData[aBackend] = aData; }

  int32_t GetSerial() { return mSerial; }

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() = 0;

  virtual GrallocImage* AsGrallocImage()
  {
    return nullptr;
  }

  virtual bool IsValid() { return true; }

  virtual uint8_t* GetBuffer() { return nullptr; }

  /**
   * For use with the CompositableClient only (so that the later can
   * synchronize the TextureClient with the TextureHost).
   */
  virtual TextureClient* GetTextureClient(CompositableClient* aClient) { return nullptr; }

  /* Access to derived classes. */
  virtual EGLImageImage* AsEGLImageImage() { return nullptr; }
#ifdef MOZ_WIDGET_ANDROID
  virtual SurfaceTextureImage* AsSurfaceTextureImage() { return nullptr; }
#endif
#ifdef XP_MACOSX
  virtual MacIOSurfaceImage* AsMacIOSurfaceImage() { return nullptr; }
#endif
  virtual PlanarYCbCrImage* AsPlanarYCbCrImage() { return nullptr; }

protected:
  Image(void* aImplData, ImageFormat aFormat) :
    mImplData(aImplData),
    mSerial(++sSerialCounter),
    mFormat(aFormat)
  {}

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~Image() {}

  mozilla::EnumeratedArray<mozilla::layers::LayersBackend,
                           mozilla::layers::LayersBackend::LAYERS_LAST,
                           nsAutoPtr<ImageBackendData>>
    mBackendData;

  void* mImplData;
  int32_t mSerial;
  ImageFormat mFormat;

  static mozilla::Atomic<int32_t> sSerialCounter;
};

/**
 * A RecycleBin is owned by an ImageContainer. We store buffers in it that we
 * want to recycle from one image to the next.It's a separate object from 
 * ImageContainer because images need to store a strong ref to their RecycleBin
 * and we must avoid creating a reference loop between an ImageContainer and
 * its active image.
 */
class BufferRecycleBin final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BufferRecycleBin)

  //typedef mozilla::gl::GLContext GLContext;

public:
  BufferRecycleBin();

  void RecycleBuffer(mozilla::UniquePtr<uint8_t[]> aBuffer, uint32_t aSize);
  // Returns a recycled buffer of the right size, or allocates a new buffer.
  mozilla::UniquePtr<uint8_t[]> GetBuffer(uint32_t aSize);

private:
  typedef mozilla::Mutex Mutex;

  // Private destructor, to discourage deletion outside of Release():
  ~BufferRecycleBin()
  {
  }

  // This protects mRecycledBuffers, mRecycledBufferSize, mRecycledTextures
  // and mRecycledTextureSizes
  Mutex mLock;

  // We should probably do something to prune this list on a timer so we don't
  // eat excess memory while video is paused...
  nsTArray<mozilla::UniquePtr<uint8_t[]>> mRecycledBuffers;
  // This is only valid if mRecycledBuffers is non-empty
  uint32_t mRecycledBufferSize;
};

/**
 * A class that manages Image creation for a LayerManager. The only reason
 * we need a separate class here is that LayerManagers aren't threadsafe
 * (because layers can only be used on the main thread) and we want to
 * be able to create images from any thread, to facilitate video playback
 * without involving the main thread, for example.
 * Different layer managers can implement child classes of this making it
 * possible to create layer manager specific images.
 * This class is not meant to be used directly but rather can be set on an
 * image container. This is usually done by the layer system internally and
 * not explicitly by users. For PlanarYCbCr or Cairo images the default
 * implementation will creates images whose data lives in system memory, for
 * MacIOSurfaces the default implementation will be a simple MacIOSurface
 * wrapper.
 */

class ImageFactory
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageFactory)
protected:
  friend class ImageContainer;

  ImageFactory() {}
  virtual ~ImageFactory() {}

  virtual RefPtr<PlanarYCbCrImage> CreatePlanarYCbCrImage(
    const gfx::IntSize& aScaleHint,
    BufferRecycleBin *aRecycleBin);
};
 
/**
 * A class that manages Images for an ImageLayer. The only reason
 * we need a separate class here is that ImageLayers aren't threadsafe
 * (because layers can only be used on the main thread) and we want to
 * be able to set the current Image from any thread, to facilitate
 * video playback without involving the main thread, for example.
 *
 * An ImageContainer can operate in one of these modes:
 * 1) Normal. Triggered by constructing the ImageContainer with
 * DISABLE_ASYNC or when compositing is happening on the main thread.
 * SetCurrentImages changes ImageContainer state but nothing is sent to the
 * compositor until the next layer transaction.
 * 2) Asynchronous. Initiated by constructing the ImageContainer with
 * ENABLE_ASYNC when compositing is happening on the main thread.
 * SetCurrentImages sends a message through the ImageBridge to the compositor
 * thread to update the image, without going through the main thread or
 * a layer transaction.
 * The ImageContainer uses a shared memory block containing a cross-process mutex
 * to communicate with the compositor thread. SetCurrentImage synchronously
 * updates the shared state to point to the new image and the old image
 * is immediately released (not true in Normal or Asynchronous modes).
 */
class ImageContainer final : public SupportsWeakPtr<ImageContainer> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainer)
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(ImageContainer)

  enum Mode { SYNCHRONOUS = 0x0, ASYNCHRONOUS = 0x01 };

  explicit ImageContainer(ImageContainer::Mode flag = SYNCHRONOUS);

  typedef uint32_t FrameID;
  typedef uint32_t ProducerID;

  RefPtr<PlanarYCbCrImage> CreatePlanarYCbCrImage();

  // Factory methods for shared image types.
  RefPtr<SharedRGBImage> CreateSharedRGBImage();

#ifdef MOZ_WIDGET_GONK
  RefPtr<OverlayImage> CreateOverlayImage();
#endif

  struct NonOwningImage {
    explicit NonOwningImage(Image* aImage = nullptr,
                            TimeStamp aTimeStamp = TimeStamp(),
                            FrameID aFrameID = 0,
                            ProducerID aProducerID = 0)
      : mImage(aImage), mTimeStamp(aTimeStamp), mFrameID(aFrameID),
        mProducerID(aProducerID) {}
    Image* mImage;
    TimeStamp mTimeStamp;
    FrameID mFrameID;
    ProducerID mProducerID;
  };
  /**
   * Set aImages as the list of timestamped to display. The Images must have
   * been created by this ImageContainer.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   * aImages must be non-empty. The first timestamp in the list may be
   * null but the others must not be, and the timestamps must increase.
   * Every element of aImages must have non-null mImage.
   * mFrameID can be zero, in which case you won't get meaningful
   * painted/dropped frame counts. Otherwise you should use a unique and
   * increasing ID for each decoded and submitted frame (but it's OK to
   * pass the same frame to SetCurrentImages).
   * mProducerID is a unique ID for the stream of images. A change in the
   * mProducerID means changing to a new mFrameID namespace. All frames in
   * aImages must have the same mProducerID.
   * 
   * The Image data must not be modified after this method is called!
   * Note that this must not be called if ENABLE_ASYNC has not been set.
   *
   * The implementation calls CurrentImageChanged() while holding
   * mReentrantMonitor.
   *
   * If this ImageContainer has an ImageClient for async video:
   * Schedule a task to send the image to the compositor using the
   * PImageBridge protcol without using the main thread.
   */
  void SetCurrentImages(const nsTArray<NonOwningImage>& aImages);

  /**
   * Clear all images. Let ImageClient release all TextureClients.
   */
  void ClearAllImages();

  /**
   * Clear the current images.
   * This function is expect to be called only from a CompositableClient
   * that belongs to ImageBridgeChild. Created to prevent dead lock.
   * See Bug 901224.
   */
  void ClearImagesFromImageBridge();

  /**
   * Set an Image as the current image to display. The Image must have
   * been created by this ImageContainer.
   * Must be called on the main thread, within a layers transaction.
   * 
   * This method takes mReentrantMonitor
   * when accessing thread-shared state.
   * aImage can be null. While it's null, nothing will be painted.
   * 
   * The Image data must not be modified after this method is called!
   * Note that this must not be called if ENABLE_ASYNC been set.
   *
   * You won't get meaningful painted/dropped counts when using this method.
   */
  void SetCurrentImageInTransaction(Image* aImage);
  void SetCurrentImagesInTransaction(const nsTArray<NonOwningImage>& aImages);

  /**
   * Returns true if this ImageContainer uses the ImageBridge IPDL protocol.
   *
   * Can be called from any thread.
   */
  bool IsAsync() const;

  /**
   * If this ImageContainer uses ImageBridge, returns the ID associated to
   * this container, for use in the ImageBridge protocol.
   * Returns 0 if this ImageContainer does not use ImageBridge. Note that
   * 0 is always an invalid ID for asynchronous image containers.
   *
   * Can be called from any thread.
   */
  uint64_t GetAsyncContainerID() const;

  /**
   * Returns if the container currently has an image.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   */
  bool HasCurrentImage();

  struct OwningImage {
    OwningImage() : mFrameID(0), mProducerID(0), mComposited(false) {}
    RefPtr<Image> mImage;
    TimeStamp mTimeStamp;
    FrameID mFrameID;
    ProducerID mProducerID;
    bool mComposited;
  };
  /**
   * Copy the current Image list to aImages.
   * This has to add references since otherwise there are race conditions
   * where the current image is destroyed before the caller can add
   * a reference.
   * Can be called on any thread.
   * May return an empty list to indicate there is no current image.
   * If aGenerationCounter is non-null, sets *aGenerationCounter to a value
   * that's unique for this ImageContainer state.
   */
  void GetCurrentImages(nsTArray<OwningImage>* aImages,
                        uint32_t* aGenerationCounter = nullptr);

  /**
   * Returns the size of the image in pixels.
   * Can be called on any thread. This method takes mReentrantMonitor when accessing
   * thread-shared state.
   */
  gfx::IntSize GetCurrentSize();

  /**
   * Sets a size that the image is expected to be rendered at.
   * This is a hint for image backends to optimize scaling.
   * Default implementation in this class is to ignore the hint.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   */
  void SetScaleHint(const gfx::IntSize& aScaleHint)
  { mScaleHint = aScaleHint; }

  void SetImageFactory(ImageFactory *aFactory)
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mImageFactory = aFactory ? aFactory : new ImageFactory();
  }

  ImageFactory* GetImageFactory() const
  {
    return mImageFactory;
  }

  /**
   * Returns the delay between the last composited image's presentation
   * timestamp and when it was first composited. It's possible for the delay
   * to be negative if the first image in the list passed to SetCurrentImages
   * has a presentation timestamp greater than "now".
   * Returns 0 if the composited image had a null timestamp, or if no
   * image has been composited yet.
   */
  TimeDuration GetPaintDelay()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mPaintDelay;
  }

  /**
   * Returns the number of images which have been contained in this container
   * and painted at least once.  Can be called from any thread.
   */
  uint32_t GetPaintCount() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mPaintCount;
  }

  /**
   * An entry in the current image list "expires" when the entry has an
   * non-null timestamp, and in a SetCurrentImages call the new image list is
   * non-empty, the timestamp of the first new image is non-null and greater
   * than the timestamp associated with the image, and the first new image's
   * frameID is not the same as the entry's.
   * Every expired image that is never composited is counted as dropped.
   */
  uint32_t GetDroppedImageCount()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mDroppedImageCount;
  }

  PImageContainerChild* GetPImageContainerChild();
  static void NotifyComposite(const ImageCompositeNotification& aNotification);

  /**
   * Main thread only.
   */
  static ProducerID AllocateProducerID();

  /// ImageBridgeChild thread only.
  static void AsyncDestroyActor(PImageContainerChild* aActor);

  /// ImageBridgeChild thread only.
  static void DeallocActor(PImageContainerChild* aActor);

private:
  typedef mozilla::ReentrantMonitor ReentrantMonitor;

  // Private destructor, to discourage deletion outside of Release():
  ~ImageContainer();

  void SetCurrentImageInternal(const nsTArray<NonOwningImage>& aImages);

  // This is called to ensure we have an active image, this may not be true
  // when we're storing image information in a RemoteImageData structure.
  // NOTE: If we have remote data mRemoteDataMutex should be locked when
  // calling this function!
  void EnsureActiveImage();

  void NotifyCompositeInternal(const ImageCompositeNotification& aNotification);

  // ReentrantMonitor to protect thread safe access to the "current
  // image", and any other state which is shared between threads.
  ReentrantMonitor mReentrantMonitor;

  nsTArray<OwningImage> mCurrentImages;

  // Updates every time mActiveImage changes
  uint32_t mGenerationCounter;

  // Number of contained images that have been painted at least once.  It's up
  // to the ImageContainer implementation to ensure accesses to this are
  // threadsafe.
  uint32_t mPaintCount;

  // See GetPaintDelay. Accessed only with mReentrantMonitor held.
  TimeDuration mPaintDelay;

  // See GetDroppedImageCount. Accessed only with mReentrantMonitor held.
  uint32_t mDroppedImageCount;

  // This is the image factory used by this container, layer managers using
  // this container can set an alternative image factory that will be used to
  // create images for this container.
  RefPtr<ImageFactory> mImageFactory;

  gfx::IntSize mScaleHint;

  RefPtr<BufferRecycleBin> mRecycleBin;

  // This member points to an ImageClient if this ImageContainer was
  // sucessfully created with ENABLE_ASYNC, or points to null otherwise.
  // 'unsuccessful' in this case only means that the ImageClient could not
  // be created, most likely because off-main-thread compositing is not enabled.
  // In this case the ImageContainer is perfectly usable, but it will forward
  // frames to the compositor through transactions in the main thread rather than
  // asynchronusly using the ImageBridge IPDL protocol.
  ImageClient* mImageClient;

  nsTArray<FrameID> mFrameIDsNotYetComposited;
  // ProducerID for last current image(s), including the frames in
  // mFrameIDsNotYetComposited
  ProducerID mCurrentProducerID;

  // Object must be released on the ImageBridge thread. Field is immutable
  // after creation of the ImageContainer.
  ImageContainerChild* mIPDLChild;

  static mozilla::Atomic<uint32_t> sGenerationCounter;
};

class AutoLockImage
{
public:
  explicit AutoLockImage(ImageContainer *aContainer)
  {
    aContainer->GetCurrentImages(&mImages);
  }

  bool HasImage() const { return !mImages.IsEmpty(); }
  Image* GetImage() const
  {
    return mImages.IsEmpty() ? nullptr : mImages[0].mImage.get();
  }

private:
  AutoTArray<ImageContainer::OwningImage,4> mImages;
};

struct PlanarYCbCrData {
  // Luminance buffer
  uint8_t* mYChannel;
  int32_t mYStride;
  gfx::IntSize mYSize;
  int32_t mYSkip;
  // Chroma buffers
  uint8_t* mCbChannel;
  uint8_t* mCrChannel;
  int32_t mCbCrStride;
  gfx::IntSize mCbCrSize;
  int32_t mCbSkip;
  int32_t mCrSkip;
  // Picture region
  uint32_t mPicX;
  uint32_t mPicY;
  gfx::IntSize mPicSize;
  StereoMode mStereoMode;

  gfx::IntRect GetPictureRect() const {
    return gfx::IntRect(mPicX, mPicY,
                     mPicSize.width,
                     mPicSize.height);
  }

  PlanarYCbCrData()
    : mYChannel(nullptr), mYStride(0), mYSize(0, 0), mYSkip(0)
    , mCbChannel(nullptr), mCrChannel(nullptr)
    , mCbCrStride(0), mCbCrSize(0, 0) , mCbSkip(0), mCrSkip(0)
    , mPicX(0), mPicY(0), mPicSize(0, 0), mStereoMode(StereoMode::MONO)
  {}
};

/****** Image subtypes for the different formats ******/

/**
 * We assume that the image data is in the REC 470M color space (see
 * Theora specification, section 4.3.1).
 *
 * The YCbCr format can be:
 *
 * 4:4:4 - CbCr width/height are the same as Y.
 * 4:2:2 - CbCr width is half that of Y. Height is the same.
 * 4:2:0 - CbCr width and height is half that of Y.
 *
 * The color format is detected based on the height/width ratios
 * defined above.
 * 
 * The Image that is rendered is the picture region defined by
 * mPicX, mPicY and mPicSize. The size of the rendered image is
 * mPicSize, not mYSize or mCbCrSize.
 *
 * mYSkip, mCbSkip, mCrSkip are added to support various output
 * formats from hardware decoder. They are per-pixel skips in the
 * source image.
 *
 * For example when image width is 640, mYStride is 670, mYSkip is 3,
 * the mYChannel buffer looks like:
 *
 * |<----------------------- mYStride ----------------------------->|
 * |<----------------- mYSize.width --------------->|
 *  0   3   6   9   12  15  18  21                659             669
 * |----------------------------------------------------------------|
 * |Y___Y___Y___Y___Y___Y___Y___Y...                      |%%%%%%%%%|
 * |Y___Y___Y___Y___Y___Y___Y___Y...                      |%%%%%%%%%|
 * |Y___Y___Y___Y___Y___Y___Y___Y...                      |%%%%%%%%%|
 * |            |<->|
 *                mYSkip
 */
class PlanarYCbCrImage : public Image {
public:
  typedef PlanarYCbCrData Data;

  enum {
    MAX_DIMENSION = 16384
  };

  virtual ~PlanarYCbCrImage() {}

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual bool CopyData(const Data& aData) = 0;

  /**
   * This doesn't make a copy of the data buffers. Can be used when mBuffer is
   * pre allocated with AllocateAndGetNewBuffer(size) and then AdoptData is
   * called to only update the picture size, planes etc. fields in mData.
   * The GStreamer media backend uses this to decode into PlanarYCbCrImage(s)
   * directly.
   */
  virtual bool AdoptData(const Data &aData);

  /**
   * This allocates and returns a new buffer
   */
  virtual uint8_t* AllocateAndGetNewBuffer(uint32_t aSize) = 0;

  /**
   * Ask this Image to not convert YUV to RGB during SetData, and make
   * the original data available through GetData. This is optional,
   * and not all PlanarYCbCrImages will support it.
   */
  virtual void SetDelayedConversion(bool aDelayed) { }

  /**
   * Grab the original YUV data. This is optional.
   */
  virtual const Data* GetData() { return &mData; }

  /**
   * Return the number of bytes of heap memory used to store this image.
   */
  virtual uint32_t GetDataSize() { return mBufferSize; }

  virtual bool IsValid() { return !!mBufferSize; }

  virtual gfx::IntSize GetSize() { return mSize; }

  virtual gfx::IntPoint GetOrigin() { return mOrigin; }

  explicit PlanarYCbCrImage();

  virtual SharedPlanarYCbCrImage *AsSharedPlanarYCbCrImage() { return nullptr; }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const = 0;

  PlanarYCbCrImage* AsPlanarYCbCrImage() { return this; }

protected:
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface();

  void SetOffscreenFormat(gfxImageFormat aFormat) { mOffscreenFormat = aFormat; }
  gfxImageFormat GetOffscreenFormat();

  Data mData;
  gfx::IntPoint mOrigin;
  gfx::IntSize mSize;
  gfxImageFormat mOffscreenFormat;
  nsCountedRef<nsMainThreadSourceSurfaceRef> mSourceSurface;
  uint32_t mBufferSize;
};

class RecyclingPlanarYCbCrImage: public PlanarYCbCrImage {
public:
  explicit RecyclingPlanarYCbCrImage(BufferRecycleBin *aRecycleBin) : mRecycleBin(aRecycleBin) {}
  virtual ~RecyclingPlanarYCbCrImage() override;
  virtual bool CopyData(const Data& aData) override;
  virtual uint8_t* AllocateAndGetNewBuffer(uint32_t aSize) override;
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
protected:

  /**
   * Return a buffer to store image data in.
   */
  mozilla::UniquePtr<uint8_t[]> AllocateBuffer(uint32_t aSize);

  RefPtr<BufferRecycleBin> mRecycleBin;
  mozilla::UniquePtr<uint8_t[]> mBuffer;
};

/**
 * Currently, the data in a SourceSurfaceImage surface is treated as being in the
 * device output color space. This class is very simple as all backends
 * have to know about how to deal with drawing a cairo image.
 */
class SourceSurfaceImage final : public Image {
public:
  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override
  {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  virtual TextureClient* GetTextureClient(CompositableClient* aClient) override;

  virtual gfx::IntSize GetSize() override { return mSize; }

  SourceSurfaceImage(const gfx::IntSize& aSize, gfx::SourceSurface* aSourceSurface);
  explicit SourceSurfaceImage(gfx::SourceSurface* aSourceSurface);
  ~SourceSurfaceImage();

private:
  gfx::IntSize mSize;
  nsCountedRef<nsOwningThreadSourceSurfaceRef> mSourceSurface;
  nsDataHashtable<nsUint32HashKey, RefPtr<TextureClient> >  mTextureClients;
};

#ifdef MOZ_WIDGET_GONK
class OverlayImage : public Image {
  /**
   * OverlayImage is a special Image type that does not hold any buffer.
   * It only hold an Id as identifier to the real content of the Image.
   * Therefore, OverlayImage must be handled by some specialized hardware(e.g. HWC) 
   * to show its content.
   */
public:
  struct Data {
    int32_t mOverlayId;
    gfx::IntSize mSize;
  };

  struct SidebandStreamData {
    GonkNativeHandle mStream;
    gfx::IntSize mSize;
  };

  OverlayImage() : Image(nullptr, ImageFormat::OVERLAY_IMAGE) { mOverlayId = INVALID_OVERLAY; }

  void SetData(const Data& aData)
  {
    mOverlayId = aData.mOverlayId;
    mSize = aData.mSize;
    mSidebandStream = GonkNativeHandle();
  }

  void SetData(const SidebandStreamData& aData)
  {
    mSidebandStream = aData.mStream;
    mSize = aData.mSize;
    mOverlayId = INVALID_OVERLAY;
  }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() { return nullptr; } ;
  int32_t GetOverlayId() { return mOverlayId; }
  GonkNativeHandle& GetSidebandStream() { return mSidebandStream; }

  gfx::IntSize GetSize() { return mSize; }

private:
  int32_t mOverlayId;
  GonkNativeHandle mSidebandStream;
  gfx::IntSize mSize;
};
#endif

} // namespace layers
} // namespace mozilla

#endif
