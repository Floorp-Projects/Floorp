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
#include "DecodeStrategy.h"
#include "DiscardTracker.h"
#include "Orientation.h"
#include "nsIObserver.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WeakPtr.h"
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

class ScaleRequest;
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

  // Raster-specific methods
  static NS_METHOD WriteToRasterImage(nsIInputStream* aIn, void* aClosure,
                                      const char* aFromRawSegment,
                                      uint32_t aToOffset, uint32_t aCount,
                                      uint32_t* aWriteCount);

  /* The index of the current frame that would be drawn if the image was to be
   * drawn now. */
  uint32_t GetCurrentFrameIndex();

  /* The total number of frames in this image. */
  uint32_t GetNumFrames() const;

  virtual size_t HeapSizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(MallocSizeOf aMallocSizeOf) const;
  virtual size_t NonHeapSizeOfDecoded() const;
  virtual size_t OutOfProcessSizeOfDecoded() const;

  virtual size_t HeapSizeOfVectorImageDocument(nsACString* aDocURL = nullptr) const MOZ_OVERRIDE {
    return 0;
  }

  /* Triggers discarding. */
  void Discard(bool force = false);
  void ForceDiscard() { Discard(/* force = */ true); }

  /* Callbacks for decoders */
  nsresult SetFrameAsNonPremult(uint32_t aFrameNum, bool aIsNonPremult);

  /** Sets the size and inherent orientation of the container. This should only
   * be called by the decoder. This function may be called multiple times, but
   * will throw an error if subsequent calls do not match the first.
   */
  nsresult SetSize(int32_t aWidth, int32_t aHeight, Orientation aOrientation);

  /**
   * Ensures that a given frame number exists with the given parameters, and
   * returns pointers to the data storage for that frame.
   * It is not possible to create sparse frame arrays; you can only append
   * frames to the current frame array.
   */
  nsresult EnsureFrame(uint32_t aFramenum, int32_t aX, int32_t aY,
                       int32_t aWidth, int32_t aHeight,
                       gfx::SurfaceFormat aFormat,
                       uint8_t aPaletteDepth,
                       uint8_t** imageData,
                       uint32_t* imageLength,
                       uint32_t** paletteData,
                       uint32_t* paletteLength,
                       imgFrame** aFrame);

  /**
   * A shorthand for EnsureFrame, above, with aPaletteDepth = 0 and paletteData
   * and paletteLength set to null.
   */
  nsresult EnsureFrame(uint32_t aFramenum, int32_t aX, int32_t aY,
                       int32_t aWidth, int32_t aHeight,
                       gfx::SurfaceFormat aFormat,
                       uint8_t** imageData,
                       uint32_t* imageLength,
                       imgFrame** aFrame);

  /* notification that the entire image has been decoded */
  nsresult DecodingComplete();

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

  static already_AddRefed<nsIEventTarget> GetEventTarget() {
    return DecodePool::Singleton()->GetEventTarget();
  }

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

  // Called from module startup. Sets up RasterImage to be used.
  static void Initialize();

  enum ScaleStatus
  {
    SCALE_INVALID,
    SCALE_PENDING,
    SCALE_DONE
  };

  // Call this with a new ScaleRequest to mark this RasterImage's scale result
  // as waiting for the results of this request. You call to ScalingDone before
  // request is destroyed!
  void ScalingStart(ScaleRequest* request);

  // Call this with a finished ScaleRequest to set this RasterImage's scale
  // result. Give it a ScaleStatus of SCALE_DONE if everything succeeded, and
  // SCALE_INVALID otherwise.
  void ScalingDone(ScaleRequest* request, ScaleStatus status);

  // Decoder shutdown
  enum eShutdownIntent {
    eShutdownIntent_Done        = 0,
    eShutdownIntent_NotNeeded   = 1,
    eShutdownIntent_Error       = 2,
    eShutdownIntent_AllCount    = 3
  };

  // Decode strategy

private:
  // Initiates an HQ scale for the given frame, if possible.
  void RequestScale(imgFrame* aFrame, nsIntSize aScale);

  already_AddRefed<imgStatusTracker> CurrentStatusTracker()
  {
    mDecodingMonitor.AssertCurrentThreadIn();
    nsRefPtr<imgStatusTracker> statusTracker;
    statusTracker = mDecodeRequest ? mDecodeRequest->mStatusTracker
                                   : mStatusTracker;
    MOZ_ASSERT(statusTracker);
    return statusTracker.forget();
  }

  nsresult OnImageDataCompleteCore(nsIRequest* aRequest, nsISupports*, nsresult aStatus);

  /**
   * Each RasterImage has a pointer to one or zero heap-allocated
   * DecodeRequests.
   */
  struct DecodeRequest
  {
    DecodeRequest(RasterImage* aImage)
      : mImage(aImage)
      , mBytesToDecode(0)
      , mRequestStatus(REQUEST_INACTIVE)
      , mChunkCount(0)
      , mAllocatedNewFrame(false)
    {
      MOZ_ASSERT(aImage, "aImage cannot be null");
      MOZ_ASSERT(aImage->mStatusTracker,
                 "aImage should have an imgStatusTracker");
      mStatusTracker = aImage->mStatusTracker->CloneForRecording();
    }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodeRequest)

    // The status tracker that is associated with a given decode request, to
    // ensure their lifetimes are linked.
    nsRefPtr<imgStatusTracker> mStatusTracker;

    RasterImage* mImage;

    size_t mBytesToDecode;

    enum DecodeRequestStatus
    {
      REQUEST_INACTIVE,
      REQUEST_PENDING,
      REQUEST_ACTIVE,
      REQUEST_WORK_DONE,
      REQUEST_STOPPED
    } mRequestStatus;

    /* Keeps track of how much time we've burned decoding this particular decode
     * request. */
    TimeDuration mDecodeTime;

    /* The number of chunks it took to decode this image. */
    int32_t mChunkCount;

    /* True if a new frame has been allocated, but DecodeSomeData hasn't yet
     * been called to flush data to it */
    bool mAllocatedNewFrame;

  private:
    ~DecodeRequest() {}
  };

  /*
   * DecodePool is a singleton class we use when decoding large images.
   *
   * When we wish to decode an image larger than
   * image.mem.max_bytes_for_sync_decode, we call DecodePool::RequestDecode()
   * for the image.  This adds the image to a queue of pending requests and posts
   * the DecodePool singleton to the event queue, if it's not already pending
   * there.
   *
   * When the DecodePool is run from the event queue, it decodes the image (and
   * all others it's managing) in chunks, periodically yielding control back to
   * the event loop.
   */
  class DecodePool : public nsIObserver
  {
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER

    static DecodePool* Singleton();

    /**
     * Ask the DecodePool to asynchronously decode this image.
     */
    void RequestDecode(RasterImage* aImg);

    /**
     * Decode aImg for a short amount of time, and post the remainder to the
     * queue.
     */
    void DecodeABitOf(RasterImage* aImg, DecodeStrategy aStrategy);

    /**
     * Ask the DecodePool to stop decoding this image.  Internally, we also
     * call this function when we finish decoding an image.
     *
     * Since the DecodePool keeps raw pointers to RasterImages, make sure you
     * call this before a RasterImage is destroyed!
     */
    static void StopDecoding(RasterImage* aImg);

    /**
     * Synchronously decode the beginning of the image until we run out of
     * bytes or we get the image's size.  Note that this done on a best-effort
     * basis; if the size is burried too deep in the image, we'll give up.
     *
     * @return NS_ERROR if an error is encountered, and NS_OK otherwise.  (Note
     *         that we return NS_OK even when the size was not found.)
     */
    nsresult DecodeUntilSizeAvailable(RasterImage* aImg);

    /**
     * Returns an event target interface to the thread pool; primarily for
     * OnDataAvailable delivery off main thread.
     *
     * @return An nsIEventTarget interface to mThreadPool.
     */
    already_AddRefed<nsIEventTarget> GetEventTarget();

  private: /* statics */
    static StaticRefPtr<DecodePool> sSingleton;

  private: /* methods */
    DecodePool();
    virtual ~DecodePool();

    enum DecodeType {
      DECODE_TYPE_UNTIL_TIME,
      DECODE_TYPE_UNTIL_SIZE,
      DECODE_TYPE_UNTIL_DONE_BYTES
    };

    /* Decode some chunks of the given image.  If aDecodeType is UNTIL_SIZE,
     * decode until we have the image's size, then stop. If bytesToDecode is
     * non-0, at most bytesToDecode bytes will be decoded. if aDecodeType is
     * UNTIL_DONE_BYTES, decode until all bytesToDecode bytes are decoded.
     */
    nsresult DecodeSomeOfImage(RasterImage* aImg,
                               DecodeStrategy aStrategy,
                               DecodeType aDecodeType = DECODE_TYPE_UNTIL_TIME,
                               uint32_t bytesToDecode = 0);

    /* A decode job dispatched to a thread pool by DecodePool.
     */
    class DecodeJob : public nsRunnable
    {
    public:
      DecodeJob(DecodeRequest* aRequest, RasterImage* aImg)
        : mRequest(aRequest)
        , mImage(aImg)
      {}

      NS_IMETHOD Run();

    protected:
      virtual ~DecodeJob();

    private:
      nsRefPtr<DecodeRequest> mRequest;
      nsRefPtr<RasterImage> mImage;
    };

  private: /* members */

    // mThreadPoolMutex protects mThreadPool. For all RasterImages R,
    // R::mDecodingMonitor must be acquired before mThreadPoolMutex
    // if both are acquired; the other order may cause deadlock.
    Mutex                     mThreadPoolMutex;
    nsCOMPtr<nsIThreadPool>   mThreadPool;
  };

  class DecodeDoneWorker : public nsRunnable
  {
  public:
    /**
     * Called by the DecodePool with an image when it's done some significant
     * portion of decoding that needs to be notified about.
     *
     * Ensures the decode state accumulated by the decoding process gets
     * applied to the image.
     */
    static void NotifyFinishedSomeDecoding(RasterImage* image, DecodeRequest* request);

    NS_IMETHOD Run();

  private: /* methods */
    DecodeDoneWorker(RasterImage* image, DecodeRequest* request);

  private: /* members */

    nsRefPtr<RasterImage> mImage;
    nsRefPtr<DecodeRequest> mRequest;
  };

  class FrameNeededWorker : public nsRunnable
  {
  public:
    /**
     * Called by the DecodeJob with an image when it's been told by the
     * decoder that it needs a new frame to be allocated on the main thread.
     *
     * Dispatches an event to do so, which will further dispatch a
     * DecodeRequest event to continue decoding.
     */
    static void GetNewFrame(RasterImage* image);

    NS_IMETHOD Run();

  private: /* methods */
    FrameNeededWorker(RasterImage* image);

  private: /* members */

    nsRefPtr<RasterImage> mImage;
  };

  nsresult FinishedSomeDecoding(eShutdownIntent intent = eShutdownIntent_Done,
                                DecodeRequest* request = nullptr);

  bool DrawWithPreDownscaleIfNeeded(imgFrame *aFrame,
                                    gfxContext *aContext,
                                    const nsIntSize& aSize,
                                    const ImageRegion& aRegion,
                                    GraphicsFilter aFilter,
                                    uint32_t aFlags);

  TemporaryRef<gfx::SourceSurface> CopyFrame(uint32_t aWhichFrame,
                                             uint32_t aFlags);

  /**
   * Deletes and nulls out the frame in mFrames[framenum].
   *
   * Does not change the size of mFrames.
   *
   * @param framenum The index of the frame to be deleted.
   *                 Must lie in [0, mFrames.Length() )
   */
  void DeleteImgFrame(uint32_t framenum);

  already_AddRefed<imgFrame> GetImgFrameNoDecode(uint32_t framenum);
  already_AddRefed<imgFrame> GetImgFrame(uint32_t framenum);
  already_AddRefed<imgFrame> GetDrawableImgFrame(uint32_t framenum);
  already_AddRefed<imgFrame> GetCurrentImgFrame();
  uint32_t GetCurrentImgFrameIndex() const;

  size_t SizeOfDecodedWithComputedFallbackIfHeap(gfxMemoryLocation aLocation,
                                                 MallocSizeOf aMallocSizeOf) const;

  void EnsureAnimExists();

  nsresult InternalAddFrameHelper(uint32_t framenum, imgFrame *frame,
                                  uint8_t **imageData, uint32_t *imageLength,
                                  uint32_t **paletteData, uint32_t *paletteLength,
                                  imgFrame** aRetFrame);
  nsresult InternalAddFrame(uint32_t framenum, int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight,
                            gfx::SurfaceFormat aFormat, uint8_t aPaletteDepth,
                            uint8_t **imageData, uint32_t *imageLength,
                            uint32_t **paletteData, uint32_t *paletteLength,
                            imgFrame** aRetFrame);

  nsresult DoImageDataComplete();

  bool ApplyDecodeFlags(uint32_t aNewFlags, uint32_t aWhichFrame);

  already_AddRefed<layers::Image> GetCurrentImage();
  void UpdateImageContainer();

  void SetInUpdateImageContainer(bool aInUpdate) { mInUpdateImageContainer = aInUpdate; }
  bool IsInUpdateImageContainer() { return mInUpdateImageContainer; }
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

  //! All the frames of the image
  FrameBlender              mFrameBlender;

  // The last frame we decoded for multipart images.
  nsRefPtr<imgFrame>        mMultipartDecodedFrame;

  nsCOMPtr<nsIProperties>   mProperties;

  // IMPORTANT: if you use mAnim in a method, call EnsureImageIsDecoded() first to ensure
  // that the frames actually exist (they may have been discarded to save memory, or
  // we maybe decoding on draw).
  FrameAnimator* mAnim;

  // Discard members
  uint32_t                   mLockCount;
  DiscardTracker::Node       mDiscardTrackerNode;

  // Source data members
  nsCString                  mSourceDataMimeType;

  friend class DiscardTracker;

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
  nsRefPtr<DecodeRequest>    mDecodeRequest;
  size_t                     mBytesDecoded;

  bool                       mInDecoder;
  // END LOCKED MEMBER VARIABLES

  // Notification state. Used to avoid recursive notifications.
  ImageStatusDiff            mStatusDiff;
  bool                       mNotifying:1;

  // Boolean flags (clustered together to conserve space):
  bool                       mHasSize:1;       // Has SetSize() been called?
  bool                       mDecodeOnDraw:1;  // Decoding on draw?
  bool                       mMultipart:1;     // Multipart?
  bool                       mDiscardable:1;   // Is container discardable?
  bool                       mHasSourceData:1; // Do we have source data?

  // Do we have the frames in decoded form?
  bool                       mDecoded:1;
  bool                       mHasBeenDecoded:1;


  // Whether the animation can stop, due to running out
  // of frames, or no more owning request
  bool                       mAnimationFinished:1;

  // Whether we're calling Decoder::Finish() from ShutdownDecoder.
  bool                       mFinishing:1;

  bool                       mInUpdateImageContainer:1;

  // Whether, once we are done doing a size decode, we should immediately kick
  // off a full decode.
  bool                       mWantFullDecode:1;

  // Set when a decode worker detects an error off-main-thread. Once the error
  // is handled on the main thread, mError is set, but mPendingError is used to
  // stop decode work immediately.
  bool                       mPendingError:1;

  // Decoding
  nsresult RequestDecodeIfNeeded(nsresult aStatus,
                                 eShutdownIntent aIntent,
                                 bool aDone,
                                 bool aWasSize);
  nsresult WantDecodedFrames();
  nsresult SyncDecode();
  nsresult InitDecoder(bool aDoSizeDecode);
  nsresult WriteToDecoder(const char *aBuffer, uint32_t aCount, DecodeStrategy aStrategy);
  nsresult DecodeSomeData(size_t aMaxBytes, DecodeStrategy aStrategy);
  bool     IsDecodeFinished();
  TimeStamp mDrawStartTime;

  inline bool CanQualityScale(const gfx::Size& scale);
  inline bool CanScale(GraphicsFilter aFilter, gfx::Size aScale, uint32_t aFlags);

  struct ScaleResult
  {
    ScaleResult()
     : status(SCALE_INVALID)
    {}

    nsIntSize scaledSize;
    nsRefPtr<imgFrame> frame;
    ScaleStatus status;
  };

  ScaleResult mScaleResult;

  // We hold on to a bare pointer to a ScaleRequest while it's outstanding so
  // we can mark it as stopped if necessary. The ScaleWorker/DrawWorker duo
  // will inform us when to let go of this pointer.
  ScaleRequest* mScaleRequest;

  // Initializes imgStatusTracker and resets it on RasterImage destruction.
  nsAutoPtr<imgStatusTrackerInit> mStatusTrackerInit;

  nsresult ShutdownDecoder(eShutdownIntent aIntent);

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
    HandleErrorWorker(RasterImage* aImage);

    nsRefPtr<RasterImage> mImage;
  };

  // Helpers
  bool CanDiscard();
  bool CanForciblyDiscard();
  bool CanForciblyDiscardAndRedecode();
  bool DiscardingActive();
  bool StoringSourceData() const;

protected:
  RasterImage(imgStatusTracker* aStatusTracker = nullptr,
              ImageURL* aURI = nullptr);

  bool ShouldAnimate();

  friend class ImageFactory;
};

inline NS_IMETHODIMP RasterImage::GetAnimationMode(uint16_t *aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

// Asynchronous Decode Requestor
//
// We use this class when someone calls requestDecode() from within a decode
// notification. Since requestDecode() involves modifying the decoder's state
// (for example, possibly shutting down a header-only decode and starting a
// full decode), we don't want to do this from inside a decoder.
class imgDecodeRequestor : public nsRunnable
{
  public:
    imgDecodeRequestor(RasterImage &aContainer) {
      mContainer = &aContainer;
    }
    NS_IMETHOD Run() {
      if (mContainer)
        mContainer->StartDecoding();
      return NS_OK;
    }

  private:
    WeakPtr<RasterImage> mContainer;
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_RasterImage_h_ */
