/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @file
 * This file declares the RasterImage class, which
 * handles static and animated rasterized images.
 *
 * @author  Stuart Parmenter <pavlov@netscape.com>
 * @author  Chris Saari <saari@netscape.com>
 * @author  Arron Mogge <paper@animecity.nu>
 * @author  Andrew Smith <asmith15@learn.senecac.on.ca>
 */

#ifndef mozilla_imagelib_RasterImage_h_
#define mozilla_imagelib_RasterImage_h_

#include "Image.h"
#include "FrameBlender.h"
#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsTArray.h"
#include "imgFrame.h"
#include "nsThreadUtils.h"
#include "DecodePool.h"
#include "Orientation.h"
#include "nsIObserver.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypedEnum.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/UniquePtr.h"
#ifdef DEBUG
  #include "imgIContainerDebug.h"
#endif

class nsIInputStream;
class nsIThreadPool;
class nsIRequest;

#define NS_RASTERIMAGE_CID \
{ /* 376ff2c1-9bf6-418a-b143-3340c00112f7 */         \
     0x376ff2c1,                                     \
     0x9bf6,                                         \
     0x418a,                                         \
    {0xb1, 0x43, 0x33, 0x40, 0xc0, 0x01, 0x12, 0xf7} \
}

/**
 * Handles static and animated image containers.
 *
 *
 * @par A Quick Walk Through
 * The decoder initializes this class and calls AppendFrame() to add a frame.
 * Once RasterImage detects more than one frame, it starts the animation
 * with StartAnimation(). Note that the invalidation events for RasterImage are
 * generated automatically using nsRefreshDriver.
 *
 * @par
 * StartAnimation() initializes the animation helper object and sets the time
 * the first frame was displayed to the current clock time.
 *
 * @par
 * When the refresh driver corresponding to the imgIContainer that this image is
 * a part of notifies the RasterImage that it's time to invalidate,
 * RequestRefresh() is called with a given TimeStamp to advance to. As long as
 * the timeout of the given frame (the frame's "delay") plus the time that frame
 * was first displayed is less than or equal to the TimeStamp given,
 * RequestRefresh() calls AdvanceFrame().
 *
 * @par
 * AdvanceFrame() is responsible for advancing a single frame of the animation.
 * It can return true, meaning that the frame advanced, or false, meaning that
 * the frame failed to advance (usually because the next frame hasn't been
 * decoded yet). It is also responsible for performing the final animation stop
 * procedure if the final frame of a non-looping animation is reached.
 *
 * @par
 * Each frame can have a different method of removing itself. These are
 * listed as imgIContainer::cDispose... constants.  Notify() calls
 * DoComposite() to handle any special frame destruction.
 *
 * @par
 * The basic path through DoComposite() is:
 * 1) Calculate Area that needs updating, which is at least the area of
 *    aNextFrame.
 * 2) Dispose of previous frame.
 * 3) Draw new image onto compositingFrame.
 * See comments in DoComposite() for more information and optimizations.
 *
 * @par
 * The rest of the RasterImage specific functions are used by DoComposite to
 * destroy the old frame and build the new one.
 *
 * @note
 * <li> "Mask", "Alpha", and "Alpha Level" are interchangeable phrases in
 * respects to RasterImage.
 *
 * @par
 * <li> GIFs never have more than a 1 bit alpha.
 * <li> APNGs may have a full alpha channel.
 *
 * @par
 * <li> Background color specified in GIF is ignored by web browsers.
 *
 * @par
 * <li> If Frame 3 wants to dispose by restoring previous, what it wants is to
 * restore the composition up to and including Frame 2, as well as Frame 2s
 * disposal.  So, in the middle of DoComposite when composing Frame 3, right
 * after destroying Frame 2's area, we copy compositingFrame to
 * prevCompositingFrame.  When DoComposite gets called to do Frame 4, we
 * copy prevCompositingFrame back, and then draw Frame 4 on top.
 *
 * @par
 * The mAnim structure has members only needed for animated images, so
 * it's not allocated until the second frame is added.
 */

namespace mozilla {

namespace layers {
class LayerManager;
class ImageContainer;
class Image;
}

namespace image {

class Decoder;
class FrameAnimator;

class RasterImage MOZ_FINAL : public ImageResource
                            , public nsIProperties
                            , public SupportsWeakPtr<RasterImage>
#ifdef DEBUG
                            , public imgIContainerDebug
#endif
{
  // (no public constructor - use ImageFactory)
  virtual ~RasterImage();

public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(RasterImage)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROPERTIES
  NS_DECL_IMGICONTAINER
#ifdef DEBUG
  NS_DECL_IMGICONTAINERDEBUG
#endif

  virtual nsresult StartAnimation();
  virtual nsresult StopAnimation();

  // Methods inherited from Image
  nsresult Init(const char* aMimeType,
                uint32_t aFlags);
  virtual nsIntRect FrameRect(uint32_t aWhichFrame) MOZ_OVERRIDE;
  virtual void OnSurfaceDiscarded() MOZ_OVERRIDE;

  // Raster-specific methods
  static NS_METHOD WriteToRasterImage(nsIInputStream* aIn, void* aClosure,
                                      const char* aFromRawSegment,
                                      uint32_t aToOffset, uint32_t aCount,
                                      uint32_t* aWriteCount);

  /* The total number of frames in this image. */
  uint32_t GetNumFrames() const;

  virtual size_t SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfDecoded(gfxMemoryLocation aLocation,
                               MallocSizeOf aMallocSizeOf) const;

  /* Triggers discarding. */
  void Discard();

  /* Callbacks for decoders */
  /** Sets the size and inherent orientation of the container. This should only
   * be called by the decoder. This function may be called multiple times, but
   * will throw an error if subsequent calls do not match the first.
   */
  nsresult SetSize(int32_t aWidth, int32_t aHeight, Orientation aOrientation);

  /**
   * Ensures that a given frame number exists with the given parameters, and
   * returns a RawAccessFrameRef for that frame.
   * It is not possible to create sparse frame arrays; you can only append
   * frames to the current frame array, or if there is only one frame in the
   * array, replace that frame.
   * If a non-paletted frame is desired, pass 0 for aPaletteDepth.
   */
  RawAccessFrameRef EnsureFrame(uint32_t aFrameNum,
                                const nsIntRect& aFrameRect,
                                uint32_t aDecodeFlags,
                                gfx::SurfaceFormat aFormat,
                                uint8_t aPaletteDepth,
                                imgFrame* aPreviousFrame);

  /* notification that the entire image has been decoded */
  void DecodingComplete(imgFrame* aFinalFrame);

  /**
   * Number of times to loop the image.
   * @note -1 means forever.
   */
  void     SetLoopCount(int32_t aLoopCount);

  /* Add compressed source data to the imgContainer.
   *
   * The decoder will use this data, either immediately or at draw time, to
   * decode the image.
   *
   * XXX This method's only caller (WriteToContainer) ignores the return
   * value. Should this just return void?
   */
  nsresult AddSourceData(const char *aBuffer, uint32_t aCount);

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) MOZ_OVERRIDE;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) MOZ_OVERRIDE;
  virtual nsresult OnNewSourceData() MOZ_OVERRIDE;

  static already_AddRefed<nsIEventTarget> GetEventTarget();

  /**
   * A hint of the number of bytes of source data that the image contains. If
   * called early on, this can help reduce copying and reallocations by
   * appropriately preallocating the source data buffer.
   *
   * We take this approach rather than having the source data management code do
   * something more complicated (like chunklisting) because HTTP is by far the
   * dominant source of images, and the Content-Length header is quite reliable.
   * Thus, pre-allocation simplifies code and reduces the total number of
   * allocations.
   */
  nsresult SetSourceSizeHint(uint32_t sizeHint);

  /* Provide a hint for the requested resolution of the resulting image. */
  void SetRequestedResolution(const nsIntSize requestedResolution) {
    mRequestedResolution = requestedResolution;
  }

  nsIntSize GetRequestedResolution() {
    return mRequestedResolution;
  }
  /* Provide a hint for the requested dimension of the resulting image. */
  void SetRequestedSampleSize(int requestedSampleSize) {
    mRequestedSampleSize = requestedSampleSize;
  }

  int GetRequestedSampleSize() {
    return mRequestedSampleSize;
  }

 nsCString GetURIString() {
    nsCString spec;
    if (GetURI()) {
      GetURI()->GetSpec(spec);
    }
    return spec;
  }

  static void Initialize();

private:
  friend class DecodePool;
  friend class DecodeWorker;
  friend class FrameNeededWorker;
  friend class NotifyProgressWorker;

  nsresult FinishedSomeDecoding(ShutdownReason aReason = ShutdownReason::DONE,
                                Progress aProgress = NoProgress);

  void DrawWithPreDownscaleIfNeeded(DrawableFrameRef&& aFrameRef,
                                    gfxContext* aContext,
                                    const nsIntSize& aSize,
                                    const ImageRegion& aRegion,
                                    GraphicsFilter aFilter,
                                    uint32_t aFlags);

  TemporaryRef<gfx::SourceSurface> CopyFrame(uint32_t aWhichFrame,
                                             uint32_t aFlags,
                                             bool aShouldSyncNotify = true);
  TemporaryRef<gfx::SourceSurface> GetFrameInternal(uint32_t aWhichFrame,
                                                    uint32_t aFlags,
                                                    bool aShouldSyncNotify = true);

  DrawableFrameRef LookupFrameInternal(uint32_t aFrameNum,
                                       const nsIntSize& aSize,
                                       uint32_t aFlags);
  DrawableFrameRef LookupFrame(uint32_t aFrameNum,
                               const nsIntSize& aSize,
                               uint32_t aFlags,
                               bool aShouldSyncNotify = true);
  uint32_t GetCurrentFrameIndex() const;
  uint32_t GetRequestedFrameIndex(uint32_t aWhichFrame) const;

  nsIntRect GetFirstFrameRect();

  size_t SizeOfDecodedWithComputedFallbackIfHeap(gfxMemoryLocation aLocation,
                                                 MallocSizeOf aMallocSizeOf) const;

  RawAccessFrameRef InternalAddFrame(uint32_t aFrameNum,
                                     const nsIntRect& aFrameRect,
                                     uint32_t aDecodeFlags,
                                     gfx::SurfaceFormat aFormat,
                                     uint8_t aPaletteDepth,
                                     imgFrame* aPreviousFrame);
  nsresult DoImageDataComplete();

  already_AddRefed<layers::Image> GetCurrentImage();
  void UpdateImageContainer();

  enum RequestDecodeType {
      ASYNCHRONOUS,
      SYNCHRONOUS_NOTIFY,
      SYNCHRONOUS_NOTIFY_AND_SOME_DECODE
  };
  NS_IMETHOD RequestDecodeCore(RequestDecodeType aDecodeType);

  // We would like to just check if we have a zero lock count, but we can't do
  // that for animated images because in EnsureAnimExists we lock the image and
  // never unlock so that animated images always have their lock count >= 1. In
  // that case we use our animation consumers count as a proxy for lock count.
  bool IsUnlocked() { return (mLockCount == 0 || (mAnim && mAnimationConsumers == 0)); }

private: // data
  nsIntSize                  mSize;
  Orientation                mOrientation;

  // Whether our frames were decoded using any special flags.
  // Some flags (e.g. unpremultiplied data) may not be compatible
  // with the browser's needs for displaying the image to the user.
  // As such, we may need to redecode if we're being asked for
  // a frame with different flags.  0 indicates default flags.
  //
  // Valid flag bits are imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA
  // and imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION.
  uint32_t                   mFrameDecodeFlags;

  //! All the frames of the image.
  Maybe<FrameBlender>       mFrameBlender;

  //! The last frame we decoded for multipart images.
  DrawableFrameRef          mMultipartDecodedFrame;

  nsCOMPtr<nsIProperties>   mProperties;

  // IMPORTANT: if you use mAnim in a method, call EnsureImageIsDecoded() first to ensure
  // that the frames actually exist (they may have been discarded to save memory, or
  // we maybe decoding on draw).
  UniquePtr<FrameAnimator> mAnim;

  // Image locking.
  uint32_t                   mLockCount;

  // Source data members
  nsCString                  mSourceDataMimeType;

  // How many times we've decoded this image.
  // This is currently only used for statistics
  int32_t                        mDecodeCount;

  // If the image contains multiple resolutions, a hint as to which one should be used
  nsIntSize                  mRequestedResolution;

  // A hint for image decoder that directly scale the image to smaller buffer
  int                        mRequestedSampleSize;

  // Cached value for GetImageContainer.
  nsRefPtr<layers::ImageContainer> mImageContainer;

  // If not cached in mImageContainer, this might have our image container
  WeakPtr<layers::ImageContainer> mImageContainerCache;

#ifdef DEBUG
  uint32_t                       mFramesNotified;
#endif

  // Below are the pieces of data that can be accessed on more than one thread
  // at once, and hence need to be locked by mDecodingMonitor.

  // BEGIN LOCKED MEMBER VARIABLES
  ReentrantMonitor           mDecodingMonitor;

  FallibleTArray<char>       mSourceData;

  // Decoder and friends
  nsRefPtr<Decoder>          mDecoder;
  DecodeStatus               mDecodeStatus;
  // END LOCKED MEMBER VARIABLES

  // Notification state. Used to avoid recursive notifications.
  Progress                   mNotifyProgress;
  nsIntRect                  mNotifyInvalidRect;
  bool                       mNotifying:1;

  // Boolean flags (clustered together to conserve space):
  bool                       mHasSize:1;       // Has SetSize() been called?
  bool                       mDecodeOnDraw:1;  // Decoding on draw?
  bool                       mMultipart:1;     // Multipart?
  bool                       mDiscardable:1;   // Is container discardable?
  bool                       mHasSourceData:1; // Do we have source data?

  // Do we have the frames in decoded form?
  bool                       mDecoded:1;
  bool                       mHasFirstFrame:1;
  bool                       mHasBeenDecoded:1;

  // Whether we're waiting to start animation. If we get a StartAnimation() call
  // but we don't yet have more than one frame, mPendingAnimation is set so that
  // we know to start animation later if/when we have more frames.
  bool                       mPendingAnimation:1;

  // Whether the animation can stop, due to running out
  // of frames, or no more owning request
  bool                       mAnimationFinished:1;

  // Whether, once we are done doing a size decode, we should immediately kick
  // off a full decode.
  bool                       mWantFullDecode:1;

  // Set when a decode worker detects an error off-main-thread. Once the error
  // is handled on the main thread, mError is set, but mPendingError is used to
  // stop decode work immediately.
  bool                       mPendingError:1;

  // Decoding
  nsresult RequestDecodeIfNeeded(nsresult aStatus, ShutdownReason aReason,
                                 bool aDone, bool aWasSize);
  nsresult WantDecodedFrames(uint32_t aFlags, bool aShouldSyncNotify);
  nsresult SyncDecode();
  nsresult InitDecoder(bool aDoSizeDecode);
  nsresult WriteToDecoder(const char *aBuffer, uint32_t aCount, DecodeStrategy aStrategy);
  nsresult DecodeSomeData(size_t aMaxBytes, DecodeStrategy aStrategy);
  bool     IsDecodeFinished();
  TimeStamp mDrawStartTime;

  // Initializes ProgressTracker and resets it on RasterImage destruction.
  nsAutoPtr<ProgressTrackerInit> mProgressTrackerInit;

  nsresult ShutdownDecoder(ShutdownReason aReason);


  //////////////////////////////////////////////////////////////////////////////
  // Scaling.
  //////////////////////////////////////////////////////////////////////////////

  // Initiates an HQ scale for the given frame, if possible.
  void RequestScale(imgFrame* aFrame, uint32_t aFlags, const nsIntSize& aSize);

  // Determines whether we can perform an HQ scale with the given parameters.
  bool CanScale(GraphicsFilter aFilter, const nsIntSize& aSize, uint32_t aFlags);

  // Called by the HQ scaler when a new scaled frame is ready.
  void NotifyNewScaledFrame();

  friend class ScaleRunner;


  // Error handling.
  void DoError();

  class HandleErrorWorker : public nsRunnable
  {
  public:
    /**
     * Called from decoder threads when DoError() is called, since errors can't
     * be handled safely off-main-thread. Dispatches an event which reinvokes
     * DoError on the main thread if there isn't one already pending.
     */
    static void DispatchIfNeeded(RasterImage* aImage);

    NS_IMETHOD Run();

  private:
    explicit HandleErrorWorker(RasterImage* aImage);

    nsRefPtr<RasterImage> mImage;
  };

  // Helpers
  bool CanDiscard();
  bool StoringSourceData() const;

protected:
  explicit RasterImage(ProgressTracker* aProgressTracker = nullptr,
                       ImageURL* aURI = nullptr);

  bool ShouldAnimate();

  friend class ImageFactory;
};

inline NS_IMETHODIMP RasterImage::GetAnimationMode(uint16_t *aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_RasterImage_h_ */
