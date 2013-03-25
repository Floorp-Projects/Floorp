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
#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsITimer.h"
#include "nsIRequest.h"
#include "nsTArray.h"
#include "imgFrame.h"
#include "nsThreadUtils.h"
#include "DiscardTracker.h"
#include "nsISupportsImpl.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/Mutex.h"
#include "gfx2DGlue.h"
#ifdef DEBUG
  #include "imgIContainerDebug.h"
#endif

class nsIInputStream;
class nsIThreadPool;

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
 *
 * @note
 * mAnimationMode and mLoopCount are not in the mAnim structure because
 * they have public setters.
 */

class ScaleRequest;

namespace mozilla {
namespace layers {
class LayerManager;
class ImageContainer;
class Image;
}
namespace image {

class Decoder;

class RasterImage : public ImageResource
                  , public nsIProperties
                  , public SupportsWeakPtr<RasterImage>
#ifdef DEBUG
                  , public imgIContainerDebug
#endif
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTIES
  NS_DECL_IMGICONTAINER
#ifdef DEBUG
  NS_DECL_IMGICONTAINERDEBUG
#endif

  // (no public constructor - use ImageFactory)
  virtual ~RasterImage();

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
  uint32_t GetNumFrames();

  virtual size_t HeapSizeOfSourceWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t NonHeapSizeOfDecoded() const;
  virtual size_t OutOfProcessSizeOfDecoded() const;

  /* Triggers discarding. */
  void Discard(bool force = false);
  void ForceDiscard() { Discard(/* force = */ true); }

  /* Callbacks for decoders */
  nsresult SetFrameAsNonPremult(uint32_t aFrameNum, bool aIsNonPremult);

  /**
   * Sets the size of the container. This should only be called by the
   * decoder. This function may be called multiple times, but will throw an
   * error if subsequent calls do not match the first.
   */
  nsresult SetSize(int32_t aWidth, int32_t aHeight);


  /**
   * Ensures that a given frame number exists with the given parameters, and
   * returns pointers to the data storage for that frame.
   * It is not possible to create sparse frame arrays; you can only append
   * frames to the current frame array.
   */
  nsresult EnsureFrame(uint32_t aFramenum, int32_t aX, int32_t aY,
                       int32_t aWidth, int32_t aHeight,
                       gfxASurface::gfxImageFormat aFormat,
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
                       gfxASurface::gfxImageFormat aFormat,
                       uint8_t** imageData,
                       uint32_t* imageLength,
                       imgFrame** aFrame);

  void FrameUpdated(uint32_t aFrameNum, nsIntRect& aUpdatedRect);

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

  // "Blend" method indicates how the current image is combined with the
  // previous image.
  enum FrameBlendMethod {
    // All color components of the frame, including alpha, overwrite the current
    // contents of the frame's output buffer region
    kBlendSource =  0,

    // The frame should be composited onto the output buffer based on its alpha,
    // using a simple OVER operation
    kBlendOver
  };

  enum FrameDisposalMethod {
    kDisposeClearAll         = -1, // Clear the whole image, revealing
                                   // what was there before the gif displayed
    kDisposeNotSpecified,   // Leave frame, let new frame draw on top
    kDisposeKeep,           // Leave frame, let new frame draw on top
    kDisposeClear,          // Clear the frame's area, revealing bg
    kDisposeRestorePrevious // Restore the previous (composited) frame
  };

  // A hint as to whether an individual frame is entirely opaque, or requires
  // alpha blending.
  enum FrameAlpha {
    kFrameHasAlpha,
    kFrameOpaque
  };

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

private:
  imgStatusTracker& CurrentStatusTracker()
  {
    if (mDecodeRequest) {
      return *mDecodeRequest->mStatusTracker;
    } else {
      return *mStatusTracker;
    }
  }

  nsresult OnImageDataCompleteCore(nsIRequest* aRequest, nsISupports*, nsresult aStatus);

  struct Anim
  {
    //! Area of the first frame that needs to be redrawn on subsequent loops.
    nsIntRect                  firstFrameRefreshArea;
    uint32_t                   currentAnimationFrameIndex; // 0 to numFrames-1

    // the time that the animation advanced to the current frame
    TimeStamp                  currentAnimationFrameTime;

    //! Track the last composited frame for Optimizations (See DoComposite code)
    int32_t                    lastCompositedFrameIndex;
    /** For managing blending of frames
     *
     * Some animations will use the compositingFrame to composite images
     * and just hand this back to the caller when it is time to draw the frame.
     * NOTE: When clearing compositingFrame, remember to set
     *       lastCompositedFrameIndex to -1.  Code assume that if
     *       lastCompositedFrameIndex >= 0 then compositingFrame exists.
     */
    nsAutoPtr<imgFrame>        compositingFrame;
    /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
     *
     * The Previous Frame (all frames composited up to the current) needs to be
     * stored in cases where the image specifies it wants the last frame back
     * when it's done with the current frame.
     */
    nsAutoPtr<imgFrame>        compositingPrevFrame;

    Anim() :
      firstFrameRefreshArea(),
      currentAnimationFrameIndex(0),
      lastCompositedFrameIndex(-1) {}
    ~Anim() {}
  };

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
      mStatusTracker = aImage->mStatusTracker->CloneForRecording();
    }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodeRequest)

    // The status tracker that is associated with a given decode request, to
    // ensure their lifetimes are linked.
    nsRefPtr<imgStatusTracker> mStatusTracker;

    RasterImage* mImage;

    uint32_t mBytesToDecode;

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
    NS_DECL_ISUPPORTS
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
    void DecodeABitOf(RasterImage* aImg);

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

    virtual ~DecodePool();

  private: /* statics */
    static StaticRefPtr<DecodePool> sSingleton;

  private: /* methods */
    DecodePool();

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

    private:
      nsRefPtr<DecodeRequest> mRequest;
      nsRefPtr<RasterImage> mImage;
    };

  private: /* members */

    nsCOMPtr<nsIThreadPool> mThreadPool;
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

  void DrawWithPreDownscaleIfNeeded(imgFrame *aFrame,
                                    gfxContext *aContext,
                                    gfxPattern::GraphicsFilter aFilter,
                                    const gfxMatrix &aUserSpaceToImageSpace,
                                    const gfxRect &aFill,
                                    const nsIntRect &aSubimage);

  nsresult CopyFrame(uint32_t aWhichFrame,
                     uint32_t aFlags,
                     gfxImageSurface **_retval);
  /**
   * Advances the animation. Typically, this will advance a single frame, but it
   * may advance multiple frames. This may happen if we have infrequently
   * "ticking" refresh drivers (e.g. in background tabs), or extremely short-
   * lived animation frames.
   *
   * @param aTime the time that the animation should advance to. This will
   *              typically be <= TimeStamp::Now().
   *
   * @param [out] aDirtyRect a pointer to an nsIntRect which encapsulates the
   *        area to be repainted after the frame is advanced.
   *
   * @returns true, if the frame was successfully advanced, false if it was not
   *          able to be advanced (e.g. the frame to which we want to advance is
   *          still decoding). Note: If false is returned, then aDirtyRect will
   *          remain unmodified.
   */
  bool AdvanceFrame(mozilla::TimeStamp aTime, nsIntRect* aDirtyRect);

  /**
   * Deletes and nulls out the frame in mFrames[framenum].
   *
   * Does not change the size of mFrames.
   *
   * @param framenum The index of the frame to be deleted. 
   *                 Must lie in [0, mFrames.Length() )
   */
  void DeleteImgFrame(uint32_t framenum);

  imgFrame* GetImgFrameNoDecode(uint32_t framenum);
  imgFrame* GetImgFrame(uint32_t framenum);
  imgFrame* GetDrawableImgFrame(uint32_t framenum);
  imgFrame* GetCurrentImgFrame();
  imgFrame* GetCurrentDrawableImgFrame();
  uint32_t GetCurrentImgFrameIndex() const;
  mozilla::TimeStamp GetCurrentImgFrameEndTime() const;

  size_t SizeOfDecodedWithComputedFallbackIfHeap(gfxASurface::MemoryLocation aLocation,
                                                 nsMallocSizeOfFun aMallocSizeOf) const;

  inline void EnsureAnimExists()
  {
    if (!mAnim) {

      // Create the animation context
      mAnim = new Anim();

      // We don't support discarding animated images (See bug 414259).
      // Lock the image and throw away the key.
      // 
      // Note that this is inefficient, since we could get rid of the source
      // data too. However, doing this is actually hard, because we're probably
      // calling ensureAnimExists mid-decode, and thus we're decoding out of
      // the source buffer. Since we're going to fix this anyway later, and
      // since we didn't kill the source data in the old world either, locking
      // is acceptable for the moment.
      LockImage();

      // Notify our observers that we are starting animation.
      CurrentStatusTracker().RecordImageIsAnimated();
    }
  }

  /** Function for doing the frame compositing of animations
   *
   * @param aDirtyRect  Area that the display will need to update
   * @param aPrevFrame  Last Frame seen/processed
   * @param aNextFrame  Frame we need to incorperate/display
   * @param aNextFrameIndex Position of aNextFrame in mFrames list
   */
  nsresult DoComposite(nsIntRect* aDirtyRect,
                       imgFrame* aPrevFrame,
                       imgFrame* aNextFrame,
                       int32_t aNextFrameIndex);

  /** Clears an area of <aFrame> with transparent black.
   *
   * @param aFrame Target Frame
   *
   * @note Does also clears the transparancy mask
   */
  static void ClearFrame(imgFrame* aFrame);

  //! @overload
  static void ClearFrame(imgFrame* aFrame, nsIntRect &aRect);
  
  //! Copy one frames's image and mask into another
  static bool CopyFrameImage(imgFrame *aSrcFrame,
                               imgFrame *aDstFrame);
  
  /** Draws one frames's image to into another,
   * at the position specified by aRect
   *
   * @param aSrcFrame  Frame providing the source image
   * @param aDstFrame  Frame where the image is drawn into
   * @param aRect      The position and size to draw the image
   */
  static nsresult DrawFrameTo(imgFrame *aSrcFrame,
                              imgFrame *aDstFrame,
                              nsIntRect& aRect);

  nsresult InternalAddFrameHelper(uint32_t framenum, imgFrame *frame,
                                  uint8_t **imageData, uint32_t *imageLength,
                                  uint32_t **paletteData, uint32_t *paletteLength,
                                  imgFrame** aRetFrame);
  nsresult InternalAddFrame(uint32_t framenum, int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight,
                            gfxASurface::gfxImageFormat aFormat, uint8_t aPaletteDepth,
                            uint8_t **imageData, uint32_t *imageLength,
                            uint32_t **paletteData, uint32_t *paletteLength,
                            imgFrame** aRetFrame);

  nsresult DoImageDataComplete();

  bool ApplyDecodeFlags(uint32_t aNewFlags);

  already_AddRefed<layers::Image> GetCurrentImage();
  void UpdateImageContainer();

  void SetInUpdateImageContainer(bool aInUpdate) { mInUpdateImageContainer = aInUpdate; }
  bool IsInUpdateImageContainer() { return mInUpdateImageContainer; }
  enum RequestDecodeType {
      ASYNCHRONOUS,
      SOMEWHAT_SYNCHRONOUS
  };
  NS_IMETHOD RequestDecodeCore(RequestDecodeType aDecodeType);

private: // data
  nsIntSize                  mSize;

  // Whether mFrames below were decoded using any special flags.
  // Some flags (e.g. unpremultiplied data) may not be compatible
  // with the browser's needs for displaying the image to the user.
  // As such, we may need to redecode if we're being asked for
  // a frame with different flags.  0 indicates default flags.
  //
  // Valid flag bits are imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA
  // and imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION.
  uint32_t                   mFrameDecodeFlags;

  //! All the frames of the image
  // IMPORTANT: if you use mFrames in a method, call EnsureImageIsDecoded() first 
  // to ensure that the frames actually exist (they may have been discarded to save
  // memory, or we may be decoding on draw).
  nsTArray<imgFrame *>       mFrames;
  
  nsCOMPtr<nsIProperties>    mProperties;

  // IMPORTANT: if you use mAnim in a method, call EnsureImageIsDecoded() first to ensure
  // that the frames actually exist (they may have been discarded to save memory, or
  // we maybe decoding on draw).
  RasterImage::Anim*        mAnim;
  
  //! # loops remaining before animation stops (-1 no stop)
  int32_t                    mLoopCount;
  
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

  // Cached value for GetImageContainer.
  nsRefPtr<mozilla::layers::ImageContainer> mImageContainer;

#ifdef DEBUG
  uint32_t                       mFramesNotified;
#endif

  // Below are the pieces of data that can be accessed on more than one thread
  // at once, and hence need to be locked by mDecodingMutex.

  // BEGIN LOCKED MEMBER VARIABLES
  mozilla::Mutex             mDecodingMutex;

  FallibleTArray<char>       mSourceData;

  // Decoder and friends
  nsRefPtr<Decoder>          mDecoder;
  nsRefPtr<DecodeRequest>    mDecodeRequest;
  uint32_t                   mBytesDecoded;
  // END LOCKED MEMBER VARIABLES

  bool                       mInDecoder;

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

  // Decoding
  nsresult WantDecodedFrames();
  nsresult SyncDecode();
  nsresult InitDecoder(bool aDoSizeDecode, bool aIsSynchronous = false);
  nsresult WriteToDecoder(const char *aBuffer, uint32_t aCount);
  nsresult DecodeSomeData(uint32_t aMaxBytes);
  bool     IsDecodeFinished();
  TimeStamp mDrawStartTime;

  inline bool CanScale(gfxPattern::GraphicsFilter aFilter, gfxSize aScale);

  struct ScaleResult
  {
    ScaleResult()
     : status(SCALE_INVALID)
    {}

    gfxSize scale;
    nsAutoPtr<imgFrame> frame;
    ScaleStatus status;
  };

  ScaleResult mScaleResult;

  // We hold on to a bare pointer to a ScaleRequest while it's outstanding so
  // we can mark it as stopped if necessary. The ScaleWorker/DrawWorker duo
  // will inform us when to let go of this pointer.
  ScaleRequest* mScaleRequest;

  nsresult ShutdownDecoder(eShutdownIntent aIntent);

  // Helpers
  void DoError();
  bool CanDiscard();
  bool CanForciblyDiscard();
  bool DiscardingActive();
  bool StoringSourceData() const;

protected:
  RasterImage(imgStatusTracker* aStatusTracker = nullptr, nsIURI* aURI = nullptr);

  bool ShouldAnimate();

  friend class ImageFactory;
};

inline NS_IMETHODIMP RasterImage::GetAnimationMode(uint16_t *aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

inline NS_IMETHODIMP RasterImage::SetAnimationMode(uint16_t aAnimationMode) {
  return SetAnimationModeInternal(aAnimationMode);
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
      mContainer = aContainer.asWeakPtr();
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
