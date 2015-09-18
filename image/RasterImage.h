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

#ifndef mozilla_image_RasterImage_h
#define mozilla_image_RasterImage_h

#include "Image.h"
#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsTArray.h"
#include "imgFrame.h"
#include "LookupResult.h"
#include "nsThreadUtils.h"
#include "DecodePool.h"
#include "DecoderFactory.h"
#include "Orientation.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Pair.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/UniquePtr.h"
#ifdef DEBUG
  #include "imgIContainerDebug.h"
#endif

class nsIInputStream;
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
class ImageContainer;
class Image;
} // namespace layers

namespace image {

class Decoder;
class FrameAnimator;
class ImageMetadata;
class SourceBuffer;

class RasterImage final : public ImageResource
                        , public nsIProperties
                        , public SupportsWeakPtr<RasterImage>
#ifdef DEBUG
                        , public imgIContainerDebug
#endif
{
  // (no public constructor - use ImageFactory)
  virtual ~RasterImage();

public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(RasterImage)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROPERTIES
  NS_DECL_IMGICONTAINER
#ifdef DEBUG
  NS_DECL_IMGICONTAINERDEBUG
#endif

  virtual nsresult StartAnimation() override;
  virtual nsresult StopAnimation() override;

  // Methods inherited from Image
  virtual void OnSurfaceDiscarded() override;

  /* The total number of frames in this image. */
  uint32_t GetNumFrames() const { return mFrameCount; }

  virtual size_t SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf)
    const override;
  virtual void CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                     MallocSizeOf aMallocSizeOf) const override;

  /* Triggers discarding. */
  void Discard();


  //////////////////////////////////////////////////////////////////////////////
  // Decoder callbacks.
  //////////////////////////////////////////////////////////////////////////////

  void OnAddedFrame(uint32_t aNewFrameCount, const nsIntRect& aNewRefreshArea);

  /**
   * Sends the provided progress notifications to ProgressTracker.
   *
   * Main-thread only.
   *
   * @param aProgress    The progress notifications to send.
   * @param aInvalidRect An invalidation rect to send.
   * @param aFlags       The surface flags used by the decoder that generated
   *                     these notifications, or DefaultSurfaceFlags() if the
   *                     notifications don't come from a decoder.
   */
  void NotifyProgress(Progress aProgress,
                      const nsIntRect& aInvalidRect = nsIntRect(),
                      SurfaceFlags aSurfaceFlags = DefaultSurfaceFlags());

  /**
   * Records telemetry and does final teardown of the provided decoder.
   *
   * Main-thread only.
   */
  void FinalizeDecoder(Decoder* aDecoder);

  // Helper method for FinalizeDecoder.
  void ReportDecoderError(Decoder* aDecoder);


  //////////////////////////////////////////////////////////////////////////////
  // Network callbacks.
  //////////////////////////////////////////////////////////////////////////////

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) override;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) override;

  void NotifyForLoadEvent(Progress aProgress);

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
  nsresult SetSourceSizeHint(uint32_t aSizeHint);

  /* Provide a hint for the requested resolution of the resulting image. */
  void SetRequestedResolution(const nsIntSize requestedResolution) {
    mRequestedResolution = requestedResolution;
  }

  /* Provide a hint for the requested dimension of the resulting image. */
  void SetRequestedSampleSize(int requestedSampleSize) {
    mRequestedSampleSize = requestedSampleSize;
  }

 nsCString GetURIString() {
    nsCString spec;
    if (GetURI()) {
      GetURI()->GetSpec(spec);
    }
    return spec;
  }

private:
  nsresult Init(const char* aMimeType, uint32_t aFlags);

  DrawResult DrawWithPreDownscaleIfNeeded(DrawableFrameRef&& aFrameRef,
                                          gfxContext* aContext,
                                          const nsIntSize& aSize,
                                          const ImageRegion& aRegion,
                                          GraphicsFilter aFilter,
                                          uint32_t aFlags);

  already_AddRefed<gfx::SourceSurface> CopyFrame(uint32_t aWhichFrame,
                                             uint32_t aFlags);

  Pair<DrawResult, RefPtr<gfx::SourceSurface>>
    GetFrameInternal(uint32_t aWhichFrame, uint32_t aFlags);

  LookupResult LookupFrameInternal(uint32_t aFrameNum,
                                   const gfx::IntSize& aSize,
                                   uint32_t aFlags);
  DrawableFrameRef LookupFrame(uint32_t aFrameNum,
                               const nsIntSize& aSize,
                               uint32_t aFlags);
  uint32_t GetCurrentFrameIndex() const;
  uint32_t GetRequestedFrameIndex(uint32_t aWhichFrame) const;

  nsIntRect GetFirstFrameRect();

  Pair<DrawResult, nsRefPtr<layers::Image>>
    GetCurrentImage(layers::ImageContainer* aContainer, uint32_t aFlags);

  void UpdateImageContainer();

  // We would like to just check if we have a zero lock count, but we can't do
  // that for animated images because in EnsureAnimExists we lock the image and
  // never unlock so that animated images always have their lock count >= 1. In
  // that case we use our animation consumers count as a proxy for lock count.
  bool IsUnlocked() {
    return (mLockCount == 0 || (mAnim && mAnimationConsumers == 0));
  }


  //////////////////////////////////////////////////////////////////////////////
  // Decoding.
  //////////////////////////////////////////////////////////////////////////////

  /**
   * Creates and runs a decoder, either synchronously or asynchronously
   * according to @aFlags. Decodes at the provided target size @aSize, using
   * decode flags @aFlags.
   *
   * It's an error to call Decode() before this image's intrinsic size is
   * available. A metadata decode must successfully complete first.
   *
   * If downscale-during-decode is not enabled for this image (i.e., if
   * mDownscaleDuringDecode is false), it is an error to pass an @aSize value
   * different from this image's intrinsic size.
   */
  NS_IMETHOD Decode(const gfx::IntSize& aSize, uint32_t aFlags);

  /**
   * Creates and runs a metadata decoder, either synchronously or
   * asynchronously according to @aFlags.
   */
  NS_IMETHOD DecodeMetadata(uint32_t aFlags);

  /**
   * Sets the size, inherent orientation, animation metadata, and other
   * information about the image gathered during decoding.
   *
   * This function may be called multiple times, but will throw an error if
   * subsequent calls do not match the first.
   *
   * @param aMetadata The metadata to set on this image.
   * @param aFromMetadataDecode True if this metadata came from a metadata
   *                            decode; false if it came from a full decode.
   */
  void SetMetadata(const ImageMetadata& aMetadata, bool aFromMetadataDecode);

  /**
   * In catastrophic circumstances like a GPU driver crash, the contents of our
   * frames may become invalid.  If the information we gathered during the
   * metadata decode proves to be wrong due to image corruption, the frames we
   * have may violate this class's invariants. Either way, we need to
   * immediately discard the invalid frames and redecode so that callers don't
   * perceive that we've entered an invalid state. 
   *
   * RecoverFromInvalidFrames discards all existing frames and redecodes using
   * the provided @aSize and @aFlags.
   */
  void RecoverFromInvalidFrames(const nsIntSize& aSize, uint32_t aFlags);

private: // data
  nsIntSize                  mSize;
  Orientation                mOrientation;

  /// If this has a value, we're waiting for SetSize() to send the load event.
  Maybe<Progress>            mLoadProgress;

  nsCOMPtr<nsIProperties>   mProperties;

  /// If this image is animated, a FrameAnimator which manages its animation.
  UniquePtr<FrameAnimator> mAnim;

  // Image locking.
  uint32_t                   mLockCount;

  // The type of decoder this image needs. Computed from the MIME type in Init().
  DecoderType                mDecoderType;

  // How many times we've decoded this image.
  // This is currently only used for statistics
  int32_t                        mDecodeCount;

  // If the image contains multiple resolutions, a hint as to which one
  // should be used
  nsIntSize                  mRequestedResolution;

  // A hint for image decoder that directly scale the image to smaller buffer
  int                        mRequestedSampleSize;

  // A weak pointer to our ImageContainer, which stays alive only as long as
  // the layer system needs it.
  WeakPtr<layers::ImageContainer> mImageContainer;

  // If mImageContainer is non-null, this contains the DrawResult we obtained
  // the last time we updated it.
  DrawResult mLastImageContainerDrawResult;

#ifdef DEBUG
  uint32_t                       mFramesNotified;
#endif

  // The source data for this image.
  nsRefPtr<SourceBuffer>     mSourceBuffer;

  // The number of frames this image has.
  uint32_t                   mFrameCount;

  // Boolean flags (clustered together to conserve space):
  bool                       mHasSize:1;       // Has SetSize() been called?
  bool                       mTransient:1;     // Is the image short-lived?
  bool                       mSyncLoad:1;      // Are we loading synchronously?
  bool                       mDiscardable:1;   // Is container discardable?
  bool                       mHasSourceData:1; // Do we have source data?
  bool                       mHasBeenDecoded:1; // Decoded at least once?
  bool                       mDownscaleDuringDecode:1;

  // Whether we're waiting to start animation. If we get a StartAnimation() call
  // but we don't yet have more than one frame, mPendingAnimation is set so that
  // we know to start animation later if/when we have more frames.
  bool                       mPendingAnimation:1;

  // Whether the animation can stop, due to running out
  // of frames, or no more owning request
  bool                       mAnimationFinished:1;

  // Whether, once we are done doing a metadata decode, we should immediately
  // kick off a full decode.
  bool                       mWantFullDecode:1;

  TimeStamp mDrawStartTime;


  //////////////////////////////////////////////////////////////////////////////
  // Scaling.
  //////////////////////////////////////////////////////////////////////////////

  // Initiates an HQ scale for the given frame, if possible.
  void RequestScale(imgFrame* aFrame, uint32_t aFlags, const nsIntSize& aSize);

  // Determines whether we can perform an HQ scale with the given parameters.
  bool CanScale(GraphicsFilter aFilter, const nsIntSize& aSize,
                uint32_t aFlags);

  // Determines whether we can downscale during decode with the given
  // parameters.
  bool CanDownscaleDuringDecode(const nsIntSize& aSize, uint32_t aFlags);

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

protected:
  explicit RasterImage(ImageURL* aURI = nullptr);

  bool ShouldAnimate() override;

  friend class ImageFactory;
};

inline NS_IMETHODIMP
RasterImage::GetAnimationMode(uint16_t* aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

} // namespace image
} // namespace mozilla

#endif /* mozilla_image_RasterImage_h */
