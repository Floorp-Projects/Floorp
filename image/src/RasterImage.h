/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Chris Saari <saari@netscape.com>
 *   Federico Mena-Quintero <federico@novell.com>
 *   Bobby Holley <bobbyholley@gmail.com>
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
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "imgFrame.h"
#include "nsThreadUtils.h"
#include "DiscardTracker.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/LinkedList.h"
#ifdef DEBUG
  #include "imgIContainerDebug.h"
#endif

class imgIDecoder;
class imgIContainerObserver;
class nsIInputStream;

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
 * mAnimationMode, mLoopCount and mObserver are not in the mAnim structure
 * because the first two have public setters and the observer we only get
 * in Init().
 */

namespace mozilla {
namespace layers {
class LayerManager;
class ImageContainer;
}
namespace image {

class Decoder;

class RasterImage : public Image
                  , public nsIProperties
                  , public nsSupportsWeakReference
#ifdef DEBUG
                  , public imgIContainerDebug
#endif
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTIES
#ifdef DEBUG
  NS_DECL_IMGICONTAINERDEBUG
#endif

  // BEGIN NS_DECL_IMGICONTAINER (minus GetAnimationMode/SetAnimationMode)
  // ** Don't edit this chunk except to mirror changes in imgIContainer.idl **
  NS_SCRIPTABLE NS_IMETHOD GetWidth(PRInt32 *aWidth);
  NS_SCRIPTABLE NS_IMETHOD GetHeight(PRInt32 *aHeight);
  NS_SCRIPTABLE NS_IMETHOD GetType(PRUint16 *aType);
  NS_IMETHOD_(PRUint16) GetType(void);
  NS_SCRIPTABLE NS_IMETHOD GetAnimated(bool *aAnimated);
  NS_SCRIPTABLE NS_IMETHOD GetCurrentFrameIsOpaque(bool *aCurrentFrameIsOpaque);
  NS_IMETHOD GetFrame(PRUint32 aWhichFrame, PRUint32 aFlags, gfxASurface **_retval NS_OUTPARAM);
  NS_IMETHOD GetImageContainer(mozilla::layers::ImageContainer **_retval NS_OUTPARAM);
  NS_IMETHOD CopyFrame(PRUint32 aWhichFrame, PRUint32 aFlags, gfxImageSurface **_retval NS_OUTPARAM);
  NS_IMETHOD ExtractFrame(PRUint32 aWhichFrame, const nsIntRect & aRect, PRUint32 aFlags, imgIContainer **_retval NS_OUTPARAM);
  NS_IMETHOD Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter, const gfxMatrix & aUserSpaceToImageSpace, const gfxRect & aFill, const nsIntRect & aSubimage, const nsIntSize & aViewportSize, PRUint32 aFlags);
  NS_IMETHOD_(nsIFrame *) GetRootLayoutFrame(void);
  NS_SCRIPTABLE NS_IMETHOD RequestDecode(void);
  NS_SCRIPTABLE NS_IMETHOD LockImage(void);
  NS_SCRIPTABLE NS_IMETHOD UnlockImage(void);
  NS_SCRIPTABLE NS_IMETHOD RequestDiscard(void);
  NS_SCRIPTABLE NS_IMETHOD ResetAnimation(void);
  NS_IMETHOD_(void) RequestRefresh(const mozilla::TimeStamp& aTime);
  // END NS_DECL_IMGICONTAINER

  RasterImage(imgStatusTracker* aStatusTracker = nsnull);
  virtual ~RasterImage();

  virtual nsresult StartAnimation();
  virtual nsresult StopAnimation();

  // Methods inherited from Image
  nsresult Init(imgIDecoderObserver* aObserver,
                const char* aMimeType,
                const char* aURIString,
                PRUint32 aFlags);
  void     GetCurrentFrameRect(nsIntRect& aRect);

  // Raster-specific methods
  static NS_METHOD WriteToRasterImage(nsIInputStream* aIn, void* aClosure,
                                      const char* aFromRawSegment,
                                      PRUint32 aToOffset, PRUint32 aCount,
                                      PRUint32* aWriteCount);

  /* The index of the current frame that would be drawn if the image was to be
   * drawn now. */
  PRUint32 GetCurrentFrameIndex();

  /* The total number of frames in this image. */
  PRUint32 GetNumFrames();

  virtual size_t HeapSizeOfSourceWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t NonHeapSizeOfDecoded() const;
  virtual size_t OutOfProcessSizeOfDecoded() const;

  /* Triggers discarding. */
  void Discard(bool force = false);
  void ForceDiscard() { Discard(/* force = */ true); }

  /* Callbacks for decoders */
  nsresult SetFrameDisposalMethod(PRUint32 aFrameNum,
                                  PRInt32 aDisposalMethod);
  nsresult SetFrameTimeout(PRUint32 aFrameNum, PRInt32 aTimeout);
  nsresult SetFrameBlendMethod(PRUint32 aFrameNum, PRInt32 aBlendMethod);
  nsresult SetFrameHasNoAlpha(PRUint32 aFrameNum);
  nsresult SetFrameAsNonPremult(PRUint32 aFrameNum, bool aIsNonPremult);

  /**
   * Sets the size of the container. This should only be called by the
   * decoder. This function may be called multiple times, but will throw an
   * error if subsequent calls do not match the first.
   */
  nsresult SetSize(PRInt32 aWidth, PRInt32 aHeight);


  /**
   * Ensures that a given frame number exists with the given parameters, and
   * returns pointers to the data storage for that frame.
   * It is not possible to create sparse frame arrays; you can only append
   * frames to the current frame array.
   */
  nsresult EnsureFrame(PRUint32 aFramenum, PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       gfxASurface::gfxImageFormat aFormat,
                       PRUint8 aPaletteDepth,
                       PRUint8** imageData,
                       PRUint32* imageLength,
                       PRUint32** paletteData,
                       PRUint32* paletteLength);

  /**
   * A shorthand for EnsureFrame, above, with aPaletteDepth = 0 and paletteData
   * and paletteLength set to null.
   */
  nsresult EnsureFrame(PRUint32 aFramenum, PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       gfxASurface::gfxImageFormat aFormat,
                       PRUint8** imageData,
                       PRUint32* imageLength);

  void FrameUpdated(PRUint32 aFrameNum, nsIntRect& aUpdatedRect);

  /* notification that the entire image has been decoded */
  nsresult DecodingComplete();

  /**
   * Number of times to loop the image.
   * @note -1 means forever.
   */
  void     SetLoopCount(PRInt32 aLoopCount);

  /* Add compressed source data to the imgContainer.
   *
   * The decoder will use this data, either immediately or at draw time, to
   * decode the image.
   *
   * XXX This method's only caller (WriteToContainer) ignores the return
   * value. Should this just return void?
   */
  nsresult AddSourceData(const char *aBuffer, PRUint32 aCount);

  /* Called after the all the source data has been added with addSourceData. */
  nsresult SourceDataComplete();

  /* Called for multipart images when there's a new source image to add. */
  nsresult NewSourceData();

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
  nsresult SetSourceSizeHint(PRUint32 sizeHint);

  // "Blend" method indicates how the current image is combined with the
  // previous image.
  enum {
    // All color components of the frame, including alpha, overwrite the current
    // contents of the frame's output buffer region
    kBlendSource =  0,

    // The frame should be composited onto the output buffer based on its alpha,
    // using a simple OVER operation
    kBlendOver
  };

  enum {
    kDisposeClearAll         = -1, // Clear the whole image, revealing
                                   // what was there before the gif displayed
    kDisposeNotSpecified,   // Leave frame, let new frame draw on top
    kDisposeKeep,           // Leave frame, let new frame draw on top
    kDisposeClear,          // Clear the frame's area, revealing bg
    kDisposeRestorePrevious // Restore the previous (composited) frame
  };

  const char* GetURIString() { return mURIString.get();}

private:
  struct Anim
  {
    //! Area of the first frame that needs to be redrawn on subsequent loops.
    nsIntRect                  firstFrameRefreshArea;
    PRUint32                   currentAnimationFrameIndex; // 0 to numFrames-1

    // the time that the animation advanced to the current frame
    TimeStamp                  currentAnimationFrameTime;

    //! Track the last composited frame for Optimizations (See DoComposite code)
    PRInt32                    lastCompositedFrameIndex;
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
   * DecodeWorker keeps a linked list of DecodeRequests to keep track of the
   * images it needs to decode.
   *
   * Each RasterImage has a single DecodeRequest member.
   */
  struct DecodeRequest : public LinkedListElement<DecodeRequest>
  {
    DecodeRequest(RasterImage* aImage)
      : mImage(aImage)
      , mIsASAP(false)
    {
    }

    RasterImage* const mImage;

    /* Keeps track of how much time we've burned decoding this particular decode
     * request. */
    TimeDuration mDecodeTime;

    /* True if we need to handle this decode as soon as possible. */
    bool mIsASAP;
  };

  /*
   * DecodeWorker is a singleton class we use when decoding large images.
   *
   * When we wish to decode an image larger than
   * image.mem.max_bytes_for_sync_decode, we call DecodeWorker::RequestDecode()
   * for the image.  This adds the image to a queue of pending requests and posts
   * the DecodeWorker singleton to the event queue, if it's not already pending
   * there.
   *
   * When the DecodeWorker is run from the event queue, it decodes the image (and
   * all others it's managing) in chunks, periodically yielding control back to
   * the event loop.
   *
   * An image being decoded may have one of two priorities: normal or ASAP.  ASAP
   * images are always decoded before normal images.  (We currently give ASAP
   * priority to images which appear onscreen but are not yet decoded.)
   */
  class DecodeWorker : public nsRunnable
  {
  public:
    static DecodeWorker* Singleton();

    /**
     * Ask the DecodeWorker to asynchronously decode this image.
     */
    void RequestDecode(RasterImage* aImg);

    /**
     * Give this image ASAP priority; it will be decoded before all non-ASAP
     * images.  You can call MarkAsASAP before or after you call RequestDecode
     * for the image, but if you MarkAsASAP before you call RequestDecode, you
     * still need to call RequestDecode.
     *
     * StopDecoding() resets the image's ASAP flag.
     */
    void MarkAsASAP(RasterImage* aImg);

    /**
     * Ask the DecodeWorker to stop decoding this image.  Internally, we also
     * call this function when we finish decoding an image.
     *
     * Since the DecodeWorker keeps raw pointers to RasterImages, make sure you
     * call this before a RasterImage is destroyed!
     */
    void StopDecoding(RasterImage* aImg);

    /**
     * Synchronously decode the beginning of the image until we run out of
     * bytes or we get the image's size.  Note that this done on a best-effort
     * basis; if the size is burried too deep in the image, we'll give up.
     *
     * @return NS_ERROR if an error is encountered, and NS_OK otherwise.  (Note
     *         that we return NS_OK even when the size was not found.)
     */
    nsresult DecodeUntilSizeAvailable(RasterImage* aImg);

    NS_IMETHOD Run();

  private: /* statics */
    static nsRefPtr<DecodeWorker> sSingleton;

  private: /* methods */
    DecodeWorker()
      : mPendingInEventLoop(false)
    {}

    /* Post ourselves to the event loop if we're not currently pending. */
    void EnsurePendingInEventLoop();

    /* Add the given request to the appropriate list of decode requests, but
     * don't ensure that we're pending in the event loop. */
    void AddDecodeRequest(DecodeRequest* aRequest);

    enum DecodeType {
      DECODE_TYPE_NORMAL,
      DECODE_TYPE_UNTIL_SIZE
    };

    /* Decode some chunks of the given image.  If aDecodeType is UNTIL_SIZE,
     * decode until we have the image's size, then stop. */
    nsresult DecodeSomeOfImage(RasterImage* aImg,
                               DecodeType aDecodeType = DECODE_TYPE_NORMAL);

  private: /* members */

    LinkedList<DecodeRequest> mASAPDecodeRequests;
    LinkedList<DecodeRequest> mNormalDecodeRequests;

    /* True if we've posted ourselves to the event loop and expect Run() to
     * be called sometime in the future. */
    bool mPendingInEventLoop;
  };

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
  void DeleteImgFrame(PRUint32 framenum);

  imgFrame* GetImgFrameNoDecode(PRUint32 framenum);
  imgFrame* GetImgFrame(PRUint32 framenum);
  imgFrame* GetDrawableImgFrame(PRUint32 framenum);
  imgFrame* GetCurrentImgFrame();
  imgFrame* GetCurrentDrawableImgFrame();
  PRUint32 GetCurrentImgFrameIndex() const;
  mozilla::TimeStamp GetCurrentImgFrameEndTime() const;
  
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
      mStatusTracker->RecordImageIsAnimated();
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
                       PRInt32 aNextFrameIndex);

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

  nsresult InternalAddFrameHelper(PRUint32 framenum, imgFrame *frame,
                                  PRUint8 **imageData, PRUint32 *imageLength,
                                  PRUint32 **paletteData, PRUint32 *paletteLength);
  nsresult InternalAddFrame(PRUint32 framenum, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                            gfxASurface::gfxImageFormat aFormat, PRUint8 aPaletteDepth,
                            PRUint8 **imageData, PRUint32 *imageLength,
                            PRUint32 **paletteData, PRUint32 *paletteLength);

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
  PRUint32                   mFrameDecodeFlags;

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
  PRInt32                    mLoopCount;
  
  //! imgIDecoderObserver
  nsWeakPtr                  mObserver;

  // Discard members
  PRUint32                   mLockCount;
  DiscardTracker::Node       mDiscardTrackerNode;

  // Source data members
  FallibleTArray<char>       mSourceData;
  nsCString                  mSourceDataMimeType;
  nsCString                  mURIString;

  friend class DiscardTracker;

  // Decoder and friends
  nsRefPtr<Decoder>              mDecoder;
  DecodeRequest                  mDecodeRequest;
  PRUint32                       mBytesDecoded;

  // How many times we've decoded this image.
  // This is currently only used for statistics
  PRInt32                        mDecodeCount;

  // Cached value for GetImageContainer.
  nsRefPtr<mozilla::layers::ImageContainer> mImageContainer;

#ifdef DEBUG
  PRUint32                       mFramesNotified;
#endif

  // Boolean flags (clustered together to conserve space):
  bool                       mHasSize:1;       // Has SetSize() been called?
  bool                       mDecodeOnDraw:1;  // Decoding on draw?
  bool                       mMultipart:1;     // Multipart?
  bool                       mDiscardable:1;   // Is container discardable?
  bool                       mHasSourceData:1; // Do we have source data?

  // Do we have the frames in decoded form?
  bool                       mDecoded:1;
  bool                       mHasBeenDecoded:1;

  bool                       mInDecoder:1;

  // Whether the animation can stop, due to running out
  // of frames, or no more owning request
  bool                       mAnimationFinished:1;

  // Decoding
  nsresult WantDecodedFrames();
  nsresult SyncDecode();
  nsresult InitDecoder(bool aDoSizeDecode);
  nsresult WriteToDecoder(const char *aBuffer, PRUint32 aCount);
  nsresult DecodeSomeData(PRUint32 aMaxBytes);
  bool     IsDecodeFinished();
  TimeStamp mDrawStartTime;

  // Decoder shutdown
  enum eShutdownIntent {
    eShutdownIntent_Done        = 0,
    eShutdownIntent_Interrupted = 1,
    eShutdownIntent_Error       = 2,
    eShutdownIntent_AllCount    = 3
  };
  nsresult ShutdownDecoder(eShutdownIntent aIntent);

  // Helpers
  void DoError();
  bool CanDiscard();
  bool CanForciblyDiscard();
  bool DiscardingActive();
  bool StoringSourceData() const;

protected:
  bool ShouldAnimate();
};

// Asynchronous Decode Requestor
//
// We use this class when someone calls requestDecode() from within a decode
// notification. Since requestDecode() involves modifying the decoder's state
// (for example, possibly shutting down a header-only decode and starting a
// full decode), we don't want to do this from inside a decoder.
class imgDecodeRequestor : public nsRunnable
{
  public:
    imgDecodeRequestor(imgIContainer *aContainer) {
      mContainer = do_GetWeakReference(aContainer);
    }
    NS_IMETHOD Run() {
      nsCOMPtr<imgIContainer> con = do_QueryReferent(mContainer);
      if (con)
        con->RequestDecode();
      return NS_OK;
    }

  private:
    nsWeakPtr mContainer;
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_RasterImage_h_ */
