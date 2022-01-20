/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGECONTAINER_H
#define GFX_IMAGECONTAINER_H

#include <stdint.h>      // for int32_t, uint32_t, uint8_t, uint64_t
#include "ImageTypes.h"  // for ImageFormat, etc
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"      // for MOZ_ASSERT_HELPER2
#include "mozilla/Mutex.h"           // for Mutex
#include "mozilla/RecursiveMutex.h"  // for RecursiveMutex, etc
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/TimeStamp.h"  // for TimeStamp
#include "mozilla/gfx/Point.h"  // For IntSize
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"           // For ColorDepth
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/mozalloc.h"  // for operator delete, etc
#include "nsDebug.h"           // for NS_ASSERTION
#include "nsISupportsImpl.h"   // for Image::Release, etc
#include "nsTArray.h"          // for nsTArray
#include "mozilla/Atomics.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/UniquePtr.h"
#include "MediaInfo.h"
#include "nsTHashMap.h"

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
class ImageContainer;
class ImageContainerChild;
class SharedPlanarYCbCrImage;
class SurfaceDescriptor;
class PlanarYCbCrImage;
class TextureClient;
class TextureClientRecycleAllocator;
class KnowsCompositor;
class NVImage;
class MemoryOrShmem;
#ifdef XP_WIN
class D3D11YCbCrRecycleAllocator;
#endif
#ifdef XP_MACOSX
class MacIOSurfaceRecycleAllocator;
#endif
class SurfaceDescriptorBuffer;

struct ImageBackendData {
  virtual ~ImageBackendData() = default;

 protected:
  ImageBackendData() = default;
};

/* Forward declarations for Image derivatives. */
class GLImage;
class SharedRGBImage;
#ifdef MOZ_WIDGET_ANDROID
class SurfaceTextureImage;
#elif defined(XP_MACOSX)
class MacIOSurfaceImage;
#elif MOZ_WAYLAND
class DMABUFSurfaceImage;
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
  ImageFormat GetFormat() const { return mFormat; }
  void* GetImplData() const { return mImplData; }

  virtual gfx::IntSize GetSize() const = 0;
  virtual gfx::IntPoint GetOrigin() const { return gfx::IntPoint(0, 0); }
  virtual gfx::IntRect GetPictureRect() const {
    return gfx::IntRect(GetOrigin().x, GetOrigin().y, GetSize().width,
                        GetSize().height);
  }

  ImageBackendData* GetBackendData(LayersBackend aBackend) {
    return mBackendData[aBackend].get();
  }
  void SetBackendData(LayersBackend aBackend, ImageBackendData* aData) {
    mBackendData[aBackend] = mozilla::WrapUnique(aData);
  }

  int32_t GetSerial() const { return mSerial; }

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() = 0;

  virtual bool IsValid() const { return true; }

  /**
   * For use with the TextureForwarder only (so that the later can
   * synchronize the TextureClient with the TextureHost).
   */
  virtual TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) {
    return nullptr;
  }

  /* Access to derived classes. */
  virtual GLImage* AsGLImage() { return nullptr; }
#ifdef MOZ_WIDGET_ANDROID
  virtual SurfaceTextureImage* AsSurfaceTextureImage() { return nullptr; }
#endif
#ifdef XP_MACOSX
  virtual MacIOSurfaceImage* AsMacIOSurfaceImage() { return nullptr; }
#endif
  virtual PlanarYCbCrImage* AsPlanarYCbCrImage() { return nullptr; }
#ifdef MOZ_WAYLAND
  virtual DMABUFSurfaceImage* AsDMABUFSurfaceImage() { return nullptr; }
#endif

  virtual NVImage* AsNVImage() { return nullptr; }

  virtual Maybe<SurfaceDescriptor> GetDesc();

 protected:
  Maybe<SurfaceDescriptor> GetDescFromTexClient(
      TextureClient* tcOverride = nullptr);

  Image(void* aImplData, ImageFormat aFormat)
      : mImplData(aImplData), mSerial(++sSerialCounter), mFormat(aFormat) {}

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~Image() = default;

  mozilla::EnumeratedArray<mozilla::layers::LayersBackend,
                           mozilla::layers::LayersBackend::LAYERS_LAST,
                           UniquePtr<ImageBackendData>>
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

  // typedef mozilla::gl::GLContext GLContext;

 public:
  BufferRecycleBin();

  void RecycleBuffer(mozilla::UniquePtr<uint8_t[]> aBuffer, uint32_t aSize);
  // Returns a recycled buffer of the right size, or allocates a new buffer.
  mozilla::UniquePtr<uint8_t[]> GetBuffer(uint32_t aSize);
  virtual void ClearRecycledBuffers();

 private:
  typedef mozilla::Mutex Mutex;

  // Private destructor, to discourage deletion outside of Release():
  ~BufferRecycleBin() = default;

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

class ImageFactory {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageFactory)
 protected:
  friend class ImageContainer;

  ImageFactory() = default;
  virtual ~ImageFactory() = default;

  virtual RefPtr<PlanarYCbCrImage> CreatePlanarYCbCrImage(
      const gfx::IntSize& aScaleHint, BufferRecycleBin* aRecycleBin);
};

// Used to notify ImageContainer::NotifyComposite()
class ImageContainerListener final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainerListener)

 public:
  explicit ImageContainerListener(ImageContainer* aImageContainer);

  void NotifyComposite(const ImageCompositeNotification& aNotification);
  void NotifyDropped(uint32_t aDropped);
  void ClearImageContainer();
  void DropImageClient();

 private:
  typedef mozilla::Mutex Mutex;

  ~ImageContainerListener();

  Mutex mLock;
  ImageContainer* mImageContainer;
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
 * The ImageContainer uses a shared memory block containing a cross-process
 * mutex to communicate with the compositor thread. SetCurrentImage
 * synchronously updates the shared state to point to the new image and the old
 * image is immediately released (not true in Normal or Asynchronous modes).
 */
class ImageContainer final : public SupportsThreadSafeWeakPtr<ImageContainer> {
  friend class ImageContainerChild;

 public:
  MOZ_DECLARE_THREADSAFEWEAKREFERENCE_TYPENAME(ImageContainer)
  MOZ_DECLARE_REFCOUNTED_TYPENAME(ImageContainer)

  enum Mode { SYNCHRONOUS = 0x0, ASYNCHRONOUS = 0x01 };

  static const uint64_t sInvalidAsyncContainerId = 0;

  explicit ImageContainer(ImageContainer::Mode flag = SYNCHRONOUS);

  /**
   * Create ImageContainer just to hold another ASYNCHRONOUS ImageContainer's
   * async container ID.
   * @param aAsyncContainerID async container ID for which we are a proxy
   */
  explicit ImageContainer(const CompositableHandle& aHandle);

  ~ImageContainer();

  typedef ContainerFrameID FrameID;
  typedef ContainerProducerID ProducerID;

  RefPtr<PlanarYCbCrImage> CreatePlanarYCbCrImage();

  // Factory methods for shared image types.
  RefPtr<SharedRGBImage> CreateSharedRGBImage();

  struct NonOwningImage {
    explicit NonOwningImage(Image* aImage = nullptr,
                            TimeStamp aTimeStamp = TimeStamp(),
                            FrameID aFrameID = 0, ProducerID aProducerID = 0)
        : mImage(aImage),
          mTimeStamp(aTimeStamp),
          mFrameID(aFrameID),
          mProducerID(aProducerID) {}
    Image* mImage;
    TimeStamp mTimeStamp;
    FrameID mFrameID;
    ProducerID mProducerID;
  };
  /**
   * Set aImages as the list of timestamped to display. The Images must have
   * been created by this ImageContainer.
   * Can be called on any thread. This method takes mRecursiveMutex
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
   * mRecursiveMutex.
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
   * Clear any resources that are not immediately necessary. This may be called
   * in low-memory conditions.
   */
  void ClearCachedResources();

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
   * This method takes mRecursiveMutex
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
  CompositableHandle GetAsyncContainerHandle();

  /**
   * Returns if the container currently has an image.
   * Can be called on any thread. This method takes mRecursiveMutex
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
   * Can be called on any thread. This method takes mRecursiveMutex when
   * accessing thread-shared state.
   */
  gfx::IntSize GetCurrentSize();

  /**
   * Sets a size that the image is expected to be rendered at.
   * This is a hint for image backends to optimize scaling.
   * Default implementation in this class is to ignore the hint.
   * Can be called on any thread. This method takes mRecursiveMutex
   * when accessing thread-shared state.
   */
  void SetScaleHint(const gfx::IntSize& aScaleHint) { mScaleHint = aScaleHint; }

  const gfx::IntSize& GetScaleHint() const { return mScaleHint; }

  void SetTransformHint(const gfx::Matrix& aTransformHint) {
    mTransformHint = aTransformHint;
  }

  const gfx::Matrix& GetTransformHint() const { return mTransformHint; }

  void SetRotation(VideoInfo::Rotation aRotation) { mRotation = aRotation; }

  VideoInfo::Rotation GetRotation() const { return mRotation; }

  void SetImageFactory(ImageFactory* aFactory) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    mImageFactory = aFactory ? aFactory : new ImageFactory();
  }

  ImageFactory* GetImageFactory() const { return mImageFactory; }

  void EnsureRecycleAllocatorForRDD(KnowsCompositor* aKnowsCompositor);

#ifdef XP_WIN
  D3D11YCbCrRecycleAllocator* GetD3D11YCbCrRecycleAllocator(
      KnowsCompositor* aKnowsCompositor);
#endif

#ifdef XP_MACOSX
  MacIOSurfaceRecycleAllocator* GetMacIOSurfaceRecycleAllocator();
#endif

  /**
   * Returns the delay between the last composited image's presentation
   * timestamp and when it was first composited. It's possible for the delay
   * to be negative if the first image in the list passed to SetCurrentImages
   * has a presentation timestamp greater than "now".
   * Returns 0 if the composited image had a null timestamp, or if no
   * image has been composited yet.
   */
  TimeDuration GetPaintDelay() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return mPaintDelay;
  }

  /**
   * Returns the number of images which have been contained in this container
   * and painted at least once.  Can be called from any thread.
   */
  uint32_t GetPaintCount() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
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
  uint32_t GetDroppedImageCount() { return mDroppedImageCount; }

  void NotifyComposite(const ImageCompositeNotification& aNotification);
  void NotifyDropped(uint32_t aDropped);

  ImageContainerListener* GetImageContainerListener() {
    return mNotifyCompositeListener;
  }

  /**
   * Get the ImageClient associated with this container. Returns only after
   * validating, and it will recreate the image client if that fails.
   * Returns nullptr if not applicable.
   */
  already_AddRefed<ImageClient> GetImageClient();

  /**
   * Main thread only.
   */
  static ProducerID AllocateProducerID();

  void DropImageClient();

 private:
  typedef mozilla::RecursiveMutex RecursiveMutex;

  void SetCurrentImageInternal(const nsTArray<NonOwningImage>& aImages);

  // This is called to ensure we have an active image, this may not be true
  // when we're storing image information in a RemoteImageData structure.
  // NOTE: If we have remote data mRemoteDataMutex should be locked when
  // calling this function!
  void EnsureActiveImage();

  void EnsureImageClient();

  // RecursiveMutex to protect thread safe access to the "current
  // image", and any other state which is shared between threads.
  RecursiveMutex mRecursiveMutex;

  RefPtr<TextureClientRecycleAllocator> mRecycleAllocator;

#ifdef XP_WIN
  RefPtr<D3D11YCbCrRecycleAllocator> mD3D11YCbCrRecycleAllocator;
#endif
#ifdef XP_MACOSX
  RefPtr<MacIOSurfaceRecycleAllocator> mMacIOSurfaceRecycleAllocator;
#endif

  nsTArray<OwningImage> mCurrentImages;

  // Updates every time mActiveImage changes
  uint32_t mGenerationCounter;

  // Number of contained images that have been painted at least once.  It's up
  // to the ImageContainer implementation to ensure accesses to this are
  // threadsafe.
  uint32_t mPaintCount;

  // See GetPaintDelay. Accessed only with mRecursiveMutex held.
  TimeDuration mPaintDelay;

  // See GetDroppedImageCount.
  mozilla::Atomic<uint32_t> mDroppedImageCount;

  // This is the image factory used by this container, layer managers using
  // this container can set an alternative image factory that will be used to
  // create images for this container.
  RefPtr<ImageFactory> mImageFactory;

  gfx::IntSize mScaleHint;

  gfx::Matrix mTransformHint;

  VideoInfo::Rotation mRotation = VideoInfo::Rotation::kDegree_0;

  RefPtr<BufferRecycleBin> mRecycleBin;

  // This member points to an ImageClient if this ImageContainer was
  // sucessfully created with ENABLE_ASYNC, or points to null otherwise.
  // 'unsuccessful' in this case only means that the ImageClient could not
  // be created, most likely because off-main-thread compositing is not enabled.
  // In this case the ImageContainer is perfectly usable, but it will forward
  // frames to the compositor through transactions in the main thread rather
  // than asynchronusly using the ImageBridge IPDL protocol.
  RefPtr<ImageClient> mImageClient;

  bool mIsAsync;
  CompositableHandle mAsyncContainerHandle;

  // ProducerID for last current image(s)
  ProducerID mCurrentProducerID;

  RefPtr<ImageContainerListener> mNotifyCompositeListener;

  static mozilla::Atomic<uint32_t> sGenerationCounter;
};

class AutoLockImage {
 public:
  explicit AutoLockImage(ImageContainer* aContainer) {
    aContainer->GetCurrentImages(&mImages);
  }

  bool HasImage() const { return !mImages.IsEmpty(); }
  Image* GetImage() const {
    return mImages.IsEmpty() ? nullptr : mImages[0].mImage.get();
  }

  Image* GetImage(TimeStamp aTimeStamp) const {
    if (mImages.IsEmpty()) {
      return nullptr;
    }

    MOZ_ASSERT(!aTimeStamp.IsNull());
    uint32_t chosenIndex = 0;

    while (chosenIndex + 1 < mImages.Length() &&
           mImages[chosenIndex + 1].mTimeStamp <= aTimeStamp) {
      ++chosenIndex;
    }

    return mImages[chosenIndex].mImage.get();
  }

 private:
  AutoTArray<ImageContainer::OwningImage, 4> mImages;
};

struct PlanarYCbCrData {
  // Luminance buffer
  uint8_t* mYChannel = nullptr;
  int32_t mYStride = 0;
  gfx::IntSize mYSize = gfx::IntSize(0, 0);
  int32_t mYSkip = 0;
  // Chroma buffers
  uint8_t* mCbChannel = nullptr;
  uint8_t* mCrChannel = nullptr;
  int32_t mCbCrStride = 0;
  gfx::IntSize mCbCrSize = gfx::IntSize(0, 0);
  int32_t mCbSkip = 0;
  int32_t mCrSkip = 0;
  // Picture region
  uint32_t mPicX = 0;
  uint32_t mPicY = 0;
  gfx::IntSize mPicSize = gfx::IntSize(0, 0);
  StereoMode mStereoMode = StereoMode::MONO;
  gfx::ColorDepth mColorDepth = gfx::ColorDepth::COLOR_8;
  gfx::YUVColorSpace mYUVColorSpace = gfx::YUVColorSpace::Default;
  gfx::ColorRange mColorRange = gfx::ColorRange::LIMITED;

  gfx::IntRect GetPictureRect() const {
    return gfx::IntRect(mPicX, mPicY, mPicSize.width, mPicSize.height);
  }
};

// This type is currently only used for AVIF and therefore makes some
// AVIF-specific assumptions (e.g., Alpha's bpc and stride is equal to Y's one)
struct PlanarAlphaData {
  uint8_t* mChannel = nullptr;
  gfx::IntSize mSize = gfx::IntSize(0, 0);
  gfx::ColorDepth mDepth = gfx::ColorDepth::COLOR_8;
  bool mPremultiplied = false;
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
 * For example when image width is 640, mYStride is 670, mYSkip is 2,
 * the mYChannel buffer looks like:
 *
 * |<----------------------- mYStride ----------------------------->|
 * |<----------------- mYSize.width --------------->|
 *  0   3   6   9   12  15  18  21                639             669
 * |----------------------------------------------------------------|
 * |Y___Y___Y___Y___Y___Y___Y___Y...                |%%%%%%%%%%%%%%%|
 * |Y___Y___Y___Y___Y___Y___Y___Y...                |%%%%%%%%%%%%%%%|
 * |Y___Y___Y___Y___Y___Y___Y___Y...                |%%%%%%%%%%%%%%%|
 * |            |<->|
 *                mYSkip
 */
class PlanarYCbCrImage : public Image {
 public:
  typedef PlanarYCbCrData Data;

  enum { MAX_DIMENSION = 16384 };

  virtual ~PlanarYCbCrImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual bool CopyData(const Data& aData) = 0;

  /**
   * This doesn't make a copy of the data buffers.
   */
  virtual bool AdoptData(const Data& aData);

  /**
   * Ask this Image to not convert YUV to RGB during SetData, and make
   * the original data available through GetData. This is optional,
   * and not all PlanarYCbCrImages will support it.
   */
  virtual void SetDelayedConversion(bool aDelayed) {}

  /**
   * Grab the original YUV data. This is optional.
   */
  virtual const Data* GetData() const { return &mData; }

  /**
   * Return the number of bytes of heap memory used to store this image.
   */
  uint32_t GetDataSize() const { return mBufferSize; }

  bool IsValid() const override { return !!mBufferSize; }

  gfx::IntSize GetSize() const override { return mSize; }

  gfx::IntPoint GetOrigin() const override { return mOrigin; }

  PlanarYCbCrImage();

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const = 0;

  PlanarYCbCrImage* AsPlanarYCbCrImage() override { return this; }

  /**
   * Build a SurfaceDescriptorBuffer with this image.  A function to allocate
   * a MemoryOrShmem with the given capacity must be provided.
   */
  virtual nsresult BuildSurfaceDescriptorBuffer(
      SurfaceDescriptorBuffer& aSdBuffer,
      const std::function<MemoryOrShmem(uint32_t)>& aAllocate);

 protected:
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  void SetOffscreenFormat(gfxImageFormat aFormat) {
    mOffscreenFormat = aFormat;
  }
  gfxImageFormat GetOffscreenFormat() const;

  Data mData;
  gfx::IntPoint mOrigin;
  gfx::IntSize mSize;
  gfxImageFormat mOffscreenFormat;
  RefPtr<gfx::SourceSurface> mSourceSurface;
  uint32_t mBufferSize;
};

class RecyclingPlanarYCbCrImage : public PlanarYCbCrImage {
 public:
  explicit RecyclingPlanarYCbCrImage(BufferRecycleBin* aRecycleBin)
      : mRecycleBin(aRecycleBin) {}
  virtual ~RecyclingPlanarYCbCrImage();
  bool CopyData(const Data& aData) override;
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;

 protected:
  /**
   * Return a buffer to store image data in.
   */
  mozilla::UniquePtr<uint8_t[]> AllocateBuffer(uint32_t aSize);

  RefPtr<BufferRecycleBin> mRecycleBin;
  mozilla::UniquePtr<uint8_t[]> mBuffer;
};

/**
 * NVImage is used to store YUV420SP_NV12 and YUV420SP_NV21 data natively, which
 * are not supported by PlanarYCbCrImage. (PlanarYCbCrImage only stores YUV444P,
 * YUV422P and YUV420P, it converts YUV420SP_NV12 and YUV420SP_NV21 data into
 * YUV420P in its PlanarYCbCrImage::SetData() method.)
 *
 * PlanarYCbCrData is able to express all the YUV family and so we keep use it
 * in NVImage.
 */
class NVImage final : public Image {
  typedef PlanarYCbCrData Data;

 public:
  NVImage();
  virtual ~NVImage();

  // Methods inherited from layers::Image.
  gfx::IntSize GetSize() const override;
  gfx::IntRect GetPictureRect() const override;
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  bool IsValid() const override;
  NVImage* AsNVImage() override;

  // Methods mimic layers::PlanarYCbCrImage.
  bool SetData(const Data& aData);
  const Data* GetData() const;
  uint32_t GetBufferSize() const;

 protected:
  /**
   * Return a buffer to store image data in.
   */
  mozilla::UniquePtr<uint8_t[]> AllocateBuffer(uint32_t aSize);

  mozilla::UniquePtr<uint8_t[]> mBuffer;
  uint32_t mBufferSize;
  gfx::IntSize mSize;
  Data mData;
  RefPtr<gfx::SourceSurface> mSourceSurface;
};

/**
 * Currently, the data in a SourceSurfaceImage surface is treated as being in
 * the device output color space. This class is very simple as all backends have
 * to know about how to deal with drawing a cairo image.
 */
class SourceSurfaceImage final : public Image {
 public:
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  void SetTextureFlags(TextureFlags aTextureFlags) {
    mTextureFlags = aTextureFlags;
  }
  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;

  gfx::IntSize GetSize() const override { return mSize; }

  SourceSurfaceImage(const gfx::IntSize& aSize,
                     gfx::SourceSurface* aSourceSurface);
  explicit SourceSurfaceImage(gfx::SourceSurface* aSourceSurface);
  virtual ~SourceSurfaceImage();

 private:
  gfx::IntSize mSize;
  RefPtr<gfx::SourceSurface> mSourceSurface;
  nsTHashMap<uint32_t, RefPtr<TextureClient>> mTextureClients;
  TextureFlags mTextureFlags;
};

}  // namespace layers
}  // namespace mozilla

#endif
