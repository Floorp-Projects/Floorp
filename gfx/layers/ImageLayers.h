/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_IMAGELAYER_H
#define GFX_IMAGELAYER_H

#include "Layers.h"

#include "nsISupportsImpl.h"
#include "gfxPattern.h"
#include "nsThreadUtils.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Mutex.h"
#include "gfxPlatform.h"

#ifdef XP_MACOSX
#include "nsIOSurface.h"
#endif

namespace mozilla {

class CrossProcessMutex;
namespace ipc {
class Shmem;
}

namespace layers {

enum StereoMode {
  STEREO_MODE_MONO,
  STEREO_MODE_LEFT_RIGHT,
  STEREO_MODE_RIGHT_LEFT,
  STEREO_MODE_BOTTOM_TOP,
  STEREO_MODE_TOP_BOTTOM
};

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
class THEBES_API Image {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Image)

public:
  virtual ~Image() {}

  enum Format {
    /**
     * The PLANAR_YCBCR format creates a PlanarYCbCrImage. All backends should
     * support this format, because the Ogg video decoder depends on it.
     * The maximum image width and height is 16384.
     */
    PLANAR_YCBCR,

    /**
     * The CAIRO_SURFACE format creates a CairoImage. All backends should
     * support this format, because video rendering sometimes requires it.
     * 
     * This format is useful even though a ThebesLayer could be used.
     * It makes it easy to render a cairo surface when another Image format
     * could be used. It can also avoid copying the surface data in some
     * cases.
     * 
     * Images in CAIRO_SURFACE format should only be created and
     * manipulated on the main thread, since the underlying cairo surface
     * is main-thread-only.
     */
    CAIRO_SURFACE,

    /**
     * The MAC_IO_SURFACE format creates a MacIOSurfaceImage.
     *
     * It wraps an IOSurface object and binds it directly to a GL texture.
     */
    MAC_IO_SURFACE,

    /**
     * An bitmap image that can be shared with a remote process.
     */
    REMOTE_IMAGE_BITMAP
  };

  Format GetFormat() { return mFormat; }
  void* GetImplData() { return mImplData; }

  virtual already_AddRefed<gfxASurface> GetAsSurface() = 0;
  virtual gfxIntSize GetSize() = 0;

  ImageBackendData* GetBackendData(LayerManager::LayersBackend aBackend)
  { return mBackendData[aBackend]; }
  void SetBackendData(LayerManager::LayersBackend aBackend, ImageBackendData* aData)
  { mBackendData[aBackend] = aData; }

protected:
  Image(void* aImplData, Format aFormat) :
    mImplData(aImplData),
    mFormat(aFormat)
  {}

  nsAutoPtr<ImageBackendData> mBackendData[LayerManager::LAYERS_LAST];

  void* mImplData;
  Format mFormat;
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

  typedef mozilla::gl::GLContext GLContext;

public:
  BufferRecycleBin();

  void RecycleBuffer(PRUint8* aBuffer, PRUint32 aSize);
  // Returns a recycled buffer of the right size, or allocates a new buffer.
  PRUint8* GetBuffer(PRUint32 aSize);

private:
  typedef mozilla::Mutex Mutex;

  // This protects mRecycledBuffers, mRecycledBufferSize, mRecycledTextures
  // and mRecycledTextureSizes
  Mutex mLock;

  // We should probably do something to prune this list on a timer so we don't
  // eat excess memory while video is paused...
  nsTArray<nsAutoArrayPtr<PRUint8> > mRecycledBuffers;
  // This is only valid if mRecycledBuffers is non-empty
  PRUint32 mRecycledBufferSize;
};

/**
 * Returns true if aFormat is in the given format array.
 */
static inline bool
FormatInList(const Image::Format* aFormats, PRUint32 aNumFormats,
             Image::Format aFormat)
{
  for (PRUint32 i = 0; i < aNumFormats; ++i) {
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
};

/**
 * A class that manages Image creation for a LayerManager. The only reason
 * we need a separate class here is that LayerMananers aren't threadsafe
 * (because layers can only be used on the main thread) and we want to
 * be able to create images from any thread, to facilitate video playback
 * without involving the main thread, for example.
 * Different layer managers can implement child classes of this making it
 * possible to create layer manager specific images.
 * This class is not meant to be used directly but rather can be set on an
 * image container. This is usually done by the layer system internally and
 * not explicitly by users. For PlanarYCbCr or Cairo images the default
 * implementation will creates images whose data lives in system memory, for
 * MacIOSurfaces the default implementation will be a simple nsIOSurface
 * wrapper.
 */

class THEBES_API ImageFactory
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageFactory)
protected:
  friend class ImageContainer;

  ImageFactory() {}
  virtual ~ImageFactory() {}

  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats,
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
    RAW_BITMAP
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
  };
};

/**
 * A class that manages Images for an ImageLayer. The only reason
 * we need a separate class here is that ImageLayers aren't threadsafe
 * (because layers can only be used on the main thread) and we want to
 * be able to set the current Image from any thread, to facilitate
 * video playback without involving the main thread, for example.
 */
class THEBES_API ImageContainer {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainer)

public:
  ImageContainer() :
    mReentrantMonitor("ImageContainer.mReentrantMonitor"),
    mPaintCount(0),
    mPreviousImagePainted(false),
    mImageFactory(new ImageFactory()),
    mRecycleBin(new BufferRecycleBin()),
    mRemoteData(nsnull),
    mRemoteDataMutex(nsnull),
    mCompositionNotifySink(nsnull)
  {}

  ~ImageContainer();

  /**
   * Create an Image in one of the given formats.
   * Picks the "best" format from the list and creates an Image of that
   * format.
   * Returns null if this backend does not support any of the formats.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   */
  already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                      PRUint32 aNumFormats);

  /**
   * Set an Image as the current image to display. The Image must have
   * been created by this ImageContainer.
   * Can be called on any thread. This method takes mReentrantMonitor
   * when accessing thread-shared state.
   * aImage can be null. While it's null, nothing will be painted.
   * 
   * The Image data must not be modified after this method is called!
   *
   * Implementations must call CurrentImageChanged() while holding
   * mReentrantMonitor.
   */
  void SetCurrentImage(Image* aImage);

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
                                                     Image** aCurrentImage = nsnull);

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
  PRUint32 GetPaintCount() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mPaintCount;
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
  PRUint32 mPaintCount;

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

  // This contains the remote image data for this container, if this is NULL
  // that means the container has no other process that may control its active
  // image.
  RemoteImageData *mRemoteData;

  // This cross-process mutex is used to synchronise access to mRemoteData.
  // When this mutex is held, we will always be inside the mReentrantMonitor
  // however the same is not true vice versa.
  CrossProcessMutex *mRemoteDataMutex;

  CompositionNotifySink *mCompositionNotifySink;
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
      mImage = nsnull;
      mContainer->UnlockCurrentImage();
      mContainer = nsnull;
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

/**
 * A Layer which renders an Image.
 */
class THEBES_API ImageLayer : public Layer {
public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the ImageContainer. aContainer must have the same layer manager
   * as this layer.
   */
  void SetContainer(ImageContainer* aContainer) 
  {
    mContainer = aContainer;  
  }
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the filter used to resample this image if necessary.
   */
  void SetFilter(gfxPattern::GraphicsFilter aFilter) { mFilter = aFilter; }

  ImageContainer* GetContainer() { return mContainer; }
  gfxPattern::GraphicsFilter GetFilter() { return mFilter; }

  MOZ_LAYER_DECL_NAME("ImageLayer", TYPE_IMAGE)

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    // Snap image edges to pixel boundaries
    gfxRect snap(0, 0, 0, 0);
    if (mContainer) {
      gfxIntSize size = mContainer->GetCurrentSize();
      snap.SizeTo(gfxSize(size.width, size.height));
    }
    // Snap our local transform first, and snap the inherited transform as well.
    // This makes our snapping equivalent to what would happen if our content
    // was drawn into a ThebesLayer (gfxContext would snap using the local
    // transform, then we'd snap again when compositing the ThebesLayer).
    mEffectiveTransform =
        SnapTransform(GetLocalTransform(), snap, nsnull)*
        SnapTransform(aTransformToSurface, gfxRect(0, 0, 0, 0), nsnull);
  }

protected:
  ImageLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData), mFilter(gfxPattern::FILTER_GOOD) {}

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  nsRefPtr<ImageContainer> mContainer;
  gfxPattern::GraphicsFilter mFilter;
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
 */
class THEBES_API PlanarYCbCrImage : public Image {
public:
  struct Data {
    // Luminance buffer
    PRUint8* mYChannel;
    PRInt32 mYStride;
    gfxIntSize mYSize;
    // Chroma buffers
    PRUint8* mCbChannel;
    PRUint8* mCrChannel;
    PRInt32 mCbCrStride;
    gfxIntSize mCbCrSize;
    // Picture region
    PRUint32 mPicX;
    PRUint32 mPicY;
    gfxIntSize mPicSize;
    StereoMode mStereoMode;

    nsIntRect GetPictureRect() const {
      return nsIntRect(mPicX, mPicY,
                       mPicSize.width,
                       mPicSize.height);
    }
  };

  enum {
    MAX_DIMENSION = 16384
  };

  ~PlanarYCbCrImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual void SetData(const Data& aData);

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
  virtual PRUint8* AllocateBuffer(PRUint32 aSize);

  /**
   * Return the number of bytes of heap memory used to store this image.
   */
  virtual PRUint32 GetDataSize() { return mBufferSize; }

  already_AddRefed<gfxASurface> GetAsSurface();

  virtual gfxIntSize GetSize() { return mSize; }

  void SetOffscreenFormat(gfxImageFormat aFormat) { mOffscreenFormat = aFormat; }
  gfxImageFormat GetOffscreenFormat() { return mOffscreenFormat; }

  // XXX - not easy to protect these sadly.
  nsAutoArrayPtr<PRUint8> mBuffer;
  PRUint32 mBufferSize;
  Data mData;
  gfxIntSize mSize;
  gfxImageFormat mOffscreenFormat;
  nsCountedRef<nsMainThreadSurfaceRef> mSurface;
  nsRefPtr<BufferRecycleBin> mRecycleBin;

  PlanarYCbCrImage(BufferRecycleBin *aRecycleBin);
};

/**
 * Currently, the data in a CairoImage surface is treated as being in the
 * device output color space. This class is very simple as all backends
 * have to know about how to deal with drawing a cairo image.
 */
class THEBES_API CairoImage : public Image {
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

  CairoImage() : Image(NULL, CAIRO_SURFACE) {}

  nsCountedRef<nsMainThreadSurfaceRef> mSurface;
  gfxIntSize mSize;
};

#ifdef XP_MACOSX
class THEBES_API MacIOSurfaceImage : public Image {
public:
  struct Data {
    nsIOSurface* mIOSurface;
  };

  MacIOSurfaceImage()
    : Image(NULL, MAC_IO_SURFACE)
    , mSize(0, 0)
    , mPluginInstanceOwner(NULL)
    , mUpdateCallback(NULL)
    , mDestroyCallback(NULL)
    {}

  virtual ~MacIOSurfaceImage()
  {
    if (mDestroyCallback) {
      mDestroyCallback(mPluginInstanceOwner);
    }
  }

 /**
  * This can only be called on the main thread. It may add a reference
  * to the surface (which will eventually be released on the main thread).
  * The surface must not be modified after this call!!!
  */
  virtual void SetData(const Data& aData);

  /**
   * Temporary hacks to force plugin drawing during an empty transaction.
   * This should not be used for anything else, and will be removed
   * when async plugin rendering is complete.
   */
  typedef void (*UpdateSurfaceCallback)(ImageContainer* aContainer, void* aInstanceOwner);
  virtual void SetUpdateCallback(UpdateSurfaceCallback aCallback, void* aInstanceOwner)
  {
    mUpdateCallback = aCallback;
    mPluginInstanceOwner = aInstanceOwner;
  }

  typedef void (*DestroyCallback)(void* aInstanceOwner);
  virtual void SetDestroyCallback(DestroyCallback aCallback)
  {
    mDestroyCallback = aCallback;
  }

  virtual gfxIntSize GetSize()
  {
    return mSize;
  }

  nsIOSurface* GetIOSurface()
  {
    return mIOSurface;
  }

  void Update(ImageContainer* aContainer);

  virtual already_AddRefed<gfxASurface> GetAsSurface();

private:
  gfxIntSize mSize;
  nsRefPtr<nsIOSurface> mIOSurface;
  void* mPluginInstanceOwner;
  UpdateSurfaceCallback mUpdateCallback;
  DestroyCallback mDestroyCallback;
};
#endif

class RemoteBitmapImage : public Image {
public:
  RemoteBitmapImage() : Image(NULL, REMOTE_IMAGE_BITMAP) {}

  already_AddRefed<gfxASurface> GetAsSurface();

  gfxIntSize GetSize() { return mSize; }

  unsigned char *mData;
  int mStride;
  gfxIntSize mSize;
  RemoteImageData::Format mFormat;
};

}
}

#endif /* GFX_IMAGELAYER_H */
