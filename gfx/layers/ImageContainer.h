/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGECONTAINER_H
#define GFX_IMAGECONTAINER_H

#include "mozilla/Mutex.h"
#include "mozilla/ReentrantMonitor.h"
#include "gfxASurface.h" // for gfxImageFormat
#include "mozilla/layers/LayersTypes.h" // for LayersBackend
#include "mozilla/TimeStamp.h"
#include "ImageTypes.h"
#include "nsTArray.h"
#include "pratom.h"

#ifdef XP_WIN
struct ID3D10Texture2D;
struct ID3D10Device;
struct ID3D10ShaderResourceView;
#endif

typedef void* HANDLE;

namespace mozilla {

class CrossProcessMutex;
namespace ipc {
class Shmem;
}

namespace layers {

class ImageClient;
class SharedPlanarYCbCrImage;
class DeprecatedSharedPlanarYCbCrImage;

struct ImageBackendData
{
  virtual ~ImageBackendData() {}

protected:
  ImageBackendData() {}
};

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
  virtual ~Image() {}


  ImageFormat GetFormat() { return mFormat; }
  void* GetImplData() { return mImplData; }

  virtual already_AddRefed<gfxASurface> GetAsSurface() = 0;
  virtual gfxIntSize GetSize() = 0;

  ImageBackendData* GetBackendData(LayersBackend aBackend)
  { return mBackendData[aBackend]; }
  void SetBackendData(LayersBackend aBackend, ImageBackendData* aData)
  { mBackendData[aBackend] = aData; }

  int32_t GetSerial() { return mSerial; }

  void MarkSent() { mSent = true; }
  bool IsSentToCompositor() { return mSent; }

protected:
  Image(void* aImplData, ImageFormat aFormat) :
    mImplData(aImplData),
    mSerial(PR_ATOMIC_INCREMENT(&sSerialCounter)),
    mFormat(aFormat),
    mSent(false)
  {}

  nsAutoPtr<ImageBackendData> mBackendData[mozilla::layers::LAYERS_LAST];

  void* mImplData;
  int32_t mSerial;
  ImageFormat mFormat;
  static int32_t sSerialCounter;
  bool mSent;
};

/**
 * A RecycleBin is owned by an ImageContainer. We store buffers in it that we
 * want to recycle from one image to the next.It's a separate object from 
 * ImageContainer because images need to store a strong ref to their RecycleBin
 * and we must avoid creating a reference loop between an ImageContainer and
 * its active image.
 */
class BufferRecycleBin {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RecycleBin)

  //typedef mozilla::gl::GLContext GLContext;

public:
  BufferRecycleBin();

  void RecycleBuffer(uint8_t* aBuffer, uint32_t aSize);
  // Returns a recycled buffer of the right size, or allocates a new buffer.
  uint8_t* GetBuffer(uint32_t aSize);

private:
  typedef mozilla::Mutex Mutex;

  // This protects mRecycledBuffers, mRecycledBufferSize, mRecycledTextures
  // and mRecycledTextureSizes
  Mutex mLock;

  // We should probably do something to prune this list on a timer so we don't
  // eat excess memory while video is paused...
  nsTArray<nsAutoArrayPtr<uint8_t> > mRecycledBuffers;
  // This is only valid if mRecycledBuffers is non-empty
  uint32_t mRecycledBufferSize;
};

/**
 * Returns true if aFormat is in the given format array.
 */
static inline bool
FormatInList(const ImageFormat* aFormats, uint32_t aNumFormats,
             ImageFormat aFormat)
{
  for (uint32_t i = 0; i < aNumFormats; ++i) {
    if (aFormats[i] == aFormat) {
      return true;
    }
  }
  return false;
}

class CompositionNotifySink
{
public:
  virtual void DidComposite() = 0;
  virtual ~CompositionNotifySink() {}
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

  virtual already_AddRefed<Image> CreateImage(const ImageFormat* aFormats,
                                              uint32_t aNumFormats,
                                              const gfxIntSize &aScaleHint,
                                              BufferRecycleBin *aRecycleBin);

};
 
/**
 * This struct is used to store RemoteImages, it is meant to be able to live in
 * shared memory. Therefor it should not contain a vtable pointer. Remote
 * users can manipulate the data in this structure to specify what image is to
 * be drawn by the container. When accessing this data users should make sure
 * the mutex synchronizing access to the structure is held!
 */
struct RemoteImageData {
  enum Type {
    /**
     * This is a format that uses raw bitmap data.
     */
    RAW_BITMAP,

    /**
     * This is a format that uses a pointer to a texture do draw directly
     * from a shared texture. Any process may have created this texture handle,
     * the process creating the texture handle is responsible for managing it's
     * lifetime by managing the lifetime of the first D3D texture object this
     * handle was created for. It must also ensure the handle is not set
     * current anywhere when the last reference to this object is released.
     */
    DXGI_TEXTURE_HANDLE
  };
  /* These formats describe the format in the memory byte-order */
  enum Format {
    /* 8 bits per channel */
    BGRA32,
    /* 8 bits per channel, alpha channel is ignored */
    BGRX32
  };

  // This should be set to true if a change was made so that the ImageContainer
  // knows to throw out any cached RemoteImage objects.
  bool mWasUpdated;
  Type mType;
  Format mFormat;
  gfxIntSize mSize;
  union {
    struct {
      /* This pointer is set by a remote process, however it will be set to
       * the container process' address the memory of the raw bitmap resides
       * at.
       */
      unsigned char *mData;
      int mStride;
    } mBitmap;
#ifdef XP_WIN
    HANDLE mTextureHandle;
#endif
  };
};

/**
 * A class that manages Images for an ImageLayer. The only reason
 * we need a separate class here is that ImageLayers aren't threadsafe
 * (because layers can only be used on the main thread) and we want to
 * be able to set the current Image from any thread, to facilitate
 * video playback without involving the main thread, for example.
 *
 * An ImageContainer can operate in one of three modes:
 * 1) Normal. Triggered by constructing the ImageContainer with
 * DISABLE_ASYNC or when compositing is happening on the main thread.
 * SetCurrentImage changes ImageContainer state but nothing is sent to the
 * compositor until the next layer transaction.
 * 2) Asynchronous. Initiated by constructing the ImageContainer with
 * ENABLE_ASYNC when compositing is happening on the main thread.
 * SetCurrentImage sends a message through the ImageBridge to the compositor
 * thread to update the image, without going through the main thread or
 * a layer transaction.
 * 3) Remote. Initiated by calling SetRemoteImageData on the ImageContainer
 * before any other activity.
 * The ImageContainer uses a shared memory block containing a cross-process mutex
 * to communicate with the compositor thread. SetCurrentImage synchronously
 * updates the shared state to point to the new image and the old image
 * is immediately released (not true in Normal or Asynchronous modes).
 */
class ImageContainer {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainer)
public:

  enum { DISABLE_ASYNC = 0x0, ENABLE_ASYNC = 0x01 };

  ImageContainer(int flag = 0);

  ~ImageContainer();

  /**
   * Create an Image in one of the given formats.
   * Picks the "best" format from the list and creates an Image of that
   * format.
   * Returns null if this backend does not support any of the formats.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   */
  already_AddRefed<Image> CreateImage(const ImageFormat* aFormats,
                                      uint32_t aNumFormats);

  /**
   * Set an Image as the current image to display. The Image must have
   * been created by this ImageContainer.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   * aImage can be null. While it's null, nothing will be painted.
   * 
   * The Image data must not be modified after this method is called!
   * Note that this must not be called if ENABLE_ASYNC has not been set.
   *
   * Implementations must call CurrentImageChanged() while holding
   * mReentrantMonitor.
   *
   * If this ImageContainer has an ImageClient for async video:
   * Schelude a task to send the image to the compositor using the 
   * PImageBridge protcol without using the main thread.
   */
  void SetCurrentImage(Image* aImage);

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
   * Implementations must call CurrentImageChanged() while holding
   * mReentrantMonitor.
   */
  void SetCurrentImageInTransaction(Image* aImage);

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

  /**
   * Lock the current Image.
   * This has to add a reference since otherwise there are race conditions
   * where the current image is destroyed before the caller can add
   * a reference. This lock strictly guarantees the underlying image remains
   * valid, it does not mean the current image cannot change.
   * Can be called on any thread. This method will lock the cross-process
   * mutex to ensure remote processes cannot alter underlying data. This call
   * -must- be balanced by a call to UnlockCurrentImage and users should avoid
   * holding the image locked for a long time.
   */
  already_AddRefed<Image> LockCurrentImage();

  /**
   * This call unlocks the image. For remote images releasing the cross-process
   * mutex.
   */
  void UnlockCurrentImage();

  /**
   * Get the current image as a gfxASurface. This is useful for fallback
   * rendering.
   * This can only be called from the main thread, since cairo objects
   * can only be used from the main thread.
   * This is defined here and not on Image because it's possible (likely)
   * that some backends will make an Image "ready to draw" only when it
   * becomes the current image for an image container.
   * Returns null if there is no current image.
   * Returns the size in aSize.
   * The returned surface will never be modified. The caller must not
   * modify it.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   * If the current image is a remote image, that is, if it is an image that
   * may be shared accross processes, calling this function will make
   * a copy of the image data while holding the mRemoteDataMutex. If possible,
   * the lock methods should be used to avoid the copy, however this should be
   * avoided if the surface is required for a long period of time.
   */
  already_AddRefed<gfxASurface> GetCurrentAsSurface(gfxIntSize* aSizeResult);

  /**
   * This is similar to GetCurrentAsSurface, however this does not make a copy
   * of the image data and requires the user to call UnlockCurrentImage when
   * done with the image data. Once UnlockCurrentImage has been called the
   * surface returned by this function is no longer valid! This works for any
   * type of image. Optionally a pointer can be passed to receive the current
   * image.
   */
  already_AddRefed<gfxASurface> LockCurrentAsSurface(gfxIntSize* aSizeResult,
                                                     Image** aCurrentImage = nullptr);

  /**
   * Returns the size of the image in pixels.
   * Can be called on any thread. This method takes mReentrantMonitor when accessing
   * thread-shared state.
   */
  gfxIntSize GetCurrentSize();

  /**
   * Sets a size that the image is expected to be rendered at.
   * This is a hint for image backends to optimize scaling.
   * Default implementation in this class is to ignore the hint.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   */
  void SetScaleHint(const gfxIntSize& aScaleHint)
  { mScaleHint = aScaleHint; }

  void SetImageFactory(ImageFactory *aFactory)
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mImageFactory = aFactory ? aFactory : new ImageFactory();
  }

  /**
   * Returns the time at which the currently contained image was first
   * painted.  This is reset every time a new image is set as the current
   * image.  Note this may return a null timestamp if the current image
   * has not yet been painted.  Can be called from any thread.
   */
  TimeStamp GetPaintTime() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mPaintTime;
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
   * Resets the paint count to zero.
   * Can be called from any thread.
   */
  void ResetPaintCount() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mPaintCount = 0;
  }

  /**
   * Increments mPaintCount if this is the first time aPainted has been
   * painted, and sets mPaintTime if the painted image is the current image.
   * current image.  Can be called from any thread.
   */
  void NotifyPaintedImage(Image* aPainted) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    nsRefPtr<Image> current = mActiveImage;
    if (aPainted == current) {
      if (mPaintTime.IsNull()) {
        mPaintTime = TimeStamp::Now();
        mPaintCount++;
      }
    } else if (!mPreviousImagePainted) {
      // While we were painting this image, the current image changed. We
      // still must count it as painted, but can't set mPaintTime, since we're
      // no longer the current image.
      mPaintCount++;
      mPreviousImagePainted = true;
    }

    if (mCompositionNotifySink) {
      mCompositionNotifySink->DidComposite();
    }
  }

  void SetCompositionNotifySink(CompositionNotifySink *aSink) {
    mCompositionNotifySink = aSink;
  }

  /**
   * This function is called to tell the ImageContainer where the
   * (cross-process) segment lives where the shared data about possible
   * remote images are stored. In addition to this a CrossProcessMutex object
   * is passed telling the container how to synchronize access to this data.
   * NOTE: This should be called during setup of the container and not after
   * usage has started.
   */
  void SetRemoteImageData(RemoteImageData *aRemoteData,
                          CrossProcessMutex *aRemoteDataMutex);
  /**
   * This can be used to check if the container has RemoteData set.
   */
  RemoteImageData *GetRemoteImageData() { return mRemoteData; }

protected:
  typedef mozilla::ReentrantMonitor ReentrantMonitor;

  void SetCurrentImageInternal(Image* aImage);

  // This is called to ensure we have an active image, this may not be true
  // when we're storing image information in a RemoteImageData structure.
  // NOTE: If we have remote data mRemoteDataMutex should be locked when
  // calling this function!
  void EnsureActiveImage();

  // ReentrantMonitor to protect thread safe access to the "current
  // image", and any other state which is shared between threads.
  ReentrantMonitor mReentrantMonitor;

  // Performs necessary housekeeping to ensure the painted frame statistics
  // are accurate. Must be called by SetCurrentImage() implementations with
  // mReentrantMonitor held.
  void CurrentImageChanged() {
    mReentrantMonitor.AssertCurrentThreadIn();
    mPreviousImagePainted = !mPaintTime.IsNull();
    mPaintTime = TimeStamp();
  }

  nsRefPtr<Image> mActiveImage;

  // Number of contained images that have been painted at least once.  It's up
  // to the ImageContainer implementation to ensure accesses to this are
  // threadsafe.
  uint32_t mPaintCount;

  // Time stamp at which the current image was first painted.  It's up to the
  // ImageContainer implementation to ensure accesses to this are threadsafe.
  TimeStamp mPaintTime;

  // Denotes whether the previous image was painted.
  bool mPreviousImagePainted;

  // This is the image factory used by this container, layer managers using
  // this container can set an alternative image factory that will be used to
  // create images for this container.
  nsRefPtr<ImageFactory> mImageFactory;

  gfxIntSize mScaleHint;

  nsRefPtr<BufferRecycleBin> mRecycleBin;

  // This contains the remote image data for this container, if this is nullptr
  // that means the container has no other process that may control its active
  // image.
  RemoteImageData *mRemoteData;

  // This cross-process mutex is used to synchronise access to mRemoteData.
  // When this mutex is held, we will always be inside the mReentrantMonitor
  // however the same is not true vice versa.
  CrossProcessMutex *mRemoteDataMutex;

  CompositionNotifySink *mCompositionNotifySink;

  // This member points to an ImageClient if this ImageContainer was
  // sucessfully created with ENABLE_ASYNC, or points to null otherwise.
  // 'unsuccessful' in this case only means that the ImageClient could not
  // be created, most likely because off-main-thread compositing is not enabled.
  // In this case the ImageContainer is perfectly usable, but it will forward
  // frames to the compositor through transactions in the main thread rather than
  // asynchronusly using the ImageBridge IPDL protocol.
  ImageClient* mImageClient;
};

class AutoLockImage
{
public:
  AutoLockImage(ImageContainer *aContainer) : mContainer(aContainer) { mImage = mContainer->LockCurrentImage(); }
  AutoLockImage(ImageContainer *aContainer, gfxASurface **aSurface) : mContainer(aContainer) {
    *aSurface = mContainer->LockCurrentAsSurface(&mSize, getter_AddRefs(mImage)).get();
  }
  ~AutoLockImage() { if (mContainer) { mContainer->UnlockCurrentImage(); } }

  Image* GetImage() { return mImage; }
  const gfxIntSize &GetSize() { return mSize; }

  void Unlock() { 
    if (mContainer) {
      mImage = nullptr;
      mContainer->UnlockCurrentImage();
      mContainer = nullptr;
    }
  }

  /** Things get a little tricky here, because our underlying image can -still-
   * change, and OS X requires a complicated callback mechanism to update this
   * we need to support staying the lock and getting the new image in a proper
   * way. This method makes any images retrieved with GetImage invalid!
   */
  void Refresh() {
    if (mContainer) {
      mContainer->UnlockCurrentImage();
      mImage = mContainer->LockCurrentImage();
    }
  }

private:
  ImageContainer *mContainer;
  nsRefPtr<Image> mImage;
  gfxIntSize mSize;
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
  struct Data {
    // Luminance buffer
    uint8_t* mYChannel;
    int32_t mYStride;
    gfxIntSize mYSize;
    int32_t mYSkip;
    // Chroma buffers
    uint8_t* mCbChannel;
    uint8_t* mCrChannel;
    int32_t mCbCrStride;
    gfxIntSize mCbCrSize;
    int32_t mCbSkip;
    int32_t mCrSkip;
    // Picture region
    uint32_t mPicX;
    uint32_t mPicY;
    gfxIntSize mPicSize;
    StereoMode mStereoMode;

    nsIntRect GetPictureRect() const {
      return nsIntRect(mPicX, mPicY,
                       mPicSize.width,
                       mPicSize.height);
    }

    Data()
      : mYChannel(nullptr), mYStride(0), mYSize(0, 0), mYSkip(0)
      , mCbChannel(nullptr), mCrChannel(nullptr)
      , mCbCrStride(0), mCbCrSize(0, 0) , mCbSkip(0), mCrSkip(0)
      , mPicX(0), mPicY(0), mPicSize(0, 0), mStereoMode(STEREO_MODE_MONO)
    {}
  };

  enum {
    MAX_DIMENSION = 16384
  };

  virtual ~PlanarYCbCrImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual void SetData(const Data& aData);

  /**
   * This doesn't make a copy of the data buffers. Can be used when mBuffer is
   * pre allocated with AllocateAndGetNewBuffer(size) and then SetDataNoCopy is
   * called to only update the picture size, planes etc. fields in mData.
   * The GStreamer media backend uses this to decode into PlanarYCbCrImage(s)
   * directly.
   */
  virtual void SetDataNoCopy(const Data &aData);

  /**
   * This allocates and returns a new buffer
   */
  virtual uint8_t* AllocateAndGetNewBuffer(uint32_t aSize);

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

  virtual gfxIntSize GetSize() { return mSize; }

  PlanarYCbCrImage(BufferRecycleBin *aRecycleBin);

  virtual SharedPlanarYCbCrImage *AsSharedPlanarYCbCrImage() { return nullptr; }
  virtual DeprecatedSharedPlanarYCbCrImage *AsDeprecatedSharedPlanarYCbCrImage() { return nullptr; }

protected:
  /**
   * Make a copy of the YCbCr data into local storage.
   *
   * @param aData           Input image data.
   */
  void CopyData(const Data& aData);

  /**
   * Return a buffer to store image data in.
   * The default implementation returns memory that can
   * be freed wit delete[]
   */
  virtual uint8_t* AllocateBuffer(uint32_t aSize);

  already_AddRefed<gfxASurface> GetAsSurface();

  void SetOffscreenFormat(gfxASurface::gfxImageFormat aFormat) { mOffscreenFormat = aFormat; }
  gfxASurface::gfxImageFormat GetOffscreenFormat();

  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mBufferSize;
  Data mData;
  gfxIntSize mSize;
  gfxASurface::gfxImageFormat mOffscreenFormat;
  nsCountedRef<nsMainThreadSurfaceRef> mSurface;
  nsRefPtr<BufferRecycleBin> mRecycleBin;
};

/**
 * Currently, the data in a CairoImage surface is treated as being in the
 * device output color space. This class is very simple as all backends
 * have to know about how to deal with drawing a cairo image.
 */
class CairoImage : public Image {
public:
  struct Data {
    gfxASurface* mSurface;
    gfxIntSize mSize;
  };

  /**
   * This can only be called on the main thread. It may add a reference
   * to the surface (which will eventually be released on the main thread).
   * The surface must not be modified after this call!!!
   */
  void SetData(const Data& aData)
  {
    mSurface = aData.mSurface;
    mSize = aData.mSize;
  }


  virtual already_AddRefed<gfxASurface> GetAsSurface()
  {
    NS_ASSERTION(NS_IsMainThread(), "Must be main thread");
    nsRefPtr<gfxASurface> surface = mSurface.get();
    return surface.forget();
  }

  gfxIntSize GetSize() { return mSize; }

  CairoImage() : Image(nullptr, CAIRO_SURFACE) {}

  nsCountedRef<nsMainThreadSurfaceRef> mSurface;
  gfxIntSize mSize;
};

class RemoteBitmapImage : public Image {
public:
  RemoteBitmapImage() : Image(nullptr, REMOTE_IMAGE_BITMAP) {}

  already_AddRefed<gfxASurface> GetAsSurface();

  gfxIntSize GetSize() { return mSize; }

  unsigned char *mData;
  int mStride;
  gfxIntSize mSize;
  RemoteImageData::Format mFormat;
};

} //namespace
} //namespace

#endif
