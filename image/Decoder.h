/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Decoder_h
#define mozilla_image_Decoder_h

#include "FrameAnimator.h"
#include "RasterImage.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "AnimationParams.h"
#include "DecoderFlags.h"
#include "ImageMetadata.h"
#include "Orientation.h"
#include "Resolution.h"
#include "SourceBuffer.h"
#include "StreamingLexer.h"
#include "SurfaceFlags.h"
#include "qcms.h"

enum class CMSMode : int32_t;

namespace mozilla {

namespace Telemetry {
enum HistogramID : uint32_t;
}  // namespace Telemetry

namespace image {

class imgFrame;

struct DecoderFinalStatus final {
  DecoderFinalStatus(bool aWasMetadataDecode, bool aFinished, bool aHadError,
                     bool aShouldReportError)
      : mWasMetadataDecode(aWasMetadataDecode),
        mFinished(aFinished),
        mHadError(aHadError),
        mShouldReportError(aShouldReportError) {}

  /// True if this was a metadata decode.
  const bool mWasMetadataDecode : 1;

  /// True if this decoder finished, whether successfully or due to failure.
  const bool mFinished : 1;

  /// True if this decoder encountered an error.
  const bool mHadError : 1;

  /// True if this decoder encountered the kind of error that should be reported
  /// to the console.
  const bool mShouldReportError : 1;
};

struct DecoderTelemetry final {
  DecoderTelemetry(const Maybe<Telemetry::HistogramID>& aSpeedHistogram,
                   size_t aBytesDecoded, uint32_t aChunkCount,
                   TimeDuration aDecodeTime)
      : mSpeedHistogram(aSpeedHistogram),
        mBytesDecoded(aBytesDecoded),
        mChunkCount(aChunkCount),
        mDecodeTime(aDecodeTime) {}

  /// @return our decoder's speed, in KBps.
  int32_t Speed() const {
    return mBytesDecoded / (1024 * mDecodeTime.ToSeconds());
  }

  /// @return our decoder's decode time, in microseconds.
  int32_t DecodeTimeMicros() { return mDecodeTime.ToMicroseconds(); }

  /// The per-image-format telemetry ID for recording our decoder's speed, or
  /// Nothing() if we don't record speed telemetry for this kind of decoder.
  const Maybe<Telemetry::HistogramID> mSpeedHistogram;

  /// The number of bytes of input our decoder processed.
  const size_t mBytesDecoded;

  /// The number of chunks our decoder's input was divided into.
  const uint32_t mChunkCount;

  /// The amount of time our decoder spent inside DoDecode().
  const TimeDuration mDecodeTime;
};

/**
 * Interface which owners of an animated Decoder object must implement in order
 * to use recycling. It allows the decoder to get a handle to the recycled
 * frames.
 */
class IDecoderFrameRecycler {
 public:
  /**
   * Request the next available recycled imgFrame from the recycler.
   *
   * @param aRecycleRect  If a frame is returned, this must be set to the
   *                      accumulated dirty rect between the frame being
   *                      recycled, and the frame being generated.
   *
   * @returns The recycled frame, if any is available.
   */
  virtual RawAccessFrameRef RecycleFrame(gfx::IntRect& aRecycleRect) = 0;
};

class Decoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Decoder)

  explicit Decoder(RasterImage* aImage);

  /**
   * Initialize an image decoder. Decoders may not be re-initialized.
   *
   * @return NS_OK if the decoder could be initialized successfully.
   */
  nsresult Init();

  /**
   * Decodes, reading all data currently available in the SourceBuffer.
   *
   * If more data is needed and @aOnResume is non-null, Decode() will schedule
   * @aOnResume to be called when more data is available.
   *
   * @return a LexerResult which may indicate:
   *   - the image has been successfully decoded (TerminalState::SUCCESS), or
   *   - the image has failed to decode (TerminalState::FAILURE), or
   *   - the decoder is yielding until it gets more data
   *     (Yield::NEED_MORE_DATA), or
   *   - the decoder is yielding to allow the caller to access intermediate
   *     output (Yield::OUTPUT_AVAILABLE).
   */
  LexerResult Decode(IResumable* aOnResume = nullptr);

  /**
   * Terminate this decoder in a failure state, just as if the decoder
   * implementation had returned TerminalState::FAILURE from DoDecode().
   *
   * XXX(seth): This method should be removed ASAP; it exists only because
   * RasterImage::FinalizeDecoder() requires an actual Decoder object as an
   * argument, so we can't simply tell RasterImage a decode failed except via an
   * intervening decoder. We'll fix this in bug 1291071.
   */
  LexerResult TerminateFailure();

  /**
   * Given a maximum number of bytes we're willing to decode, @aByteLimit,
   * returns true if we should attempt to run this decoder synchronously.
   */
  bool ShouldSyncDecode(size_t aByteLimit);

  /**
   * Gets the invalidation region accumulated by the decoder so far, and clears
   * the decoder's invalidation region. This means that each call to
   * TakeInvalidRect() returns only the invalidation region accumulated since
   * the last call to TakeInvalidRect().
   */
  OrientedIntRect TakeInvalidRect() {
    OrientedIntRect invalidRect = mInvalidRect;
    mInvalidRect.SetEmpty();
    return invalidRect;
  }

  /**
   * Gets the progress changes accumulated by the decoder so far, and clears
   * them. This means that each call to TakeProgress() returns only the changes
   * accumulated since the last call to TakeProgress().
   */
  Progress TakeProgress() {
    Progress progress = mProgress;
    mProgress = NoProgress;
    return progress;
  }

  /**
   * Returns true if there's any progress to report.
   */
  bool HasProgress() const {
    return mProgress != NoProgress || !mInvalidRect.IsEmpty() ||
           mFinishedNewFrame;
  }

  /*
   * State.
   */

  /**
   * If we're doing a metadata decode, we only decode the image's headers, which
   * is enough to determine the image's intrinsic size. A metadata decode is
   * enabled by calling SetMetadataDecode() *before* calling Init().
   */
  void SetMetadataDecode(bool aMetadataDecode) {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mMetadataDecode = aMetadataDecode;
  }
  bool IsMetadataDecode() const { return mMetadataDecode; }

  /**
   * Sets the output size of this decoder. If this is smaller than the intrinsic
   * size of the image, we'll downscale it while decoding. For memory usage
   * reasons, upscaling is forbidden and will trigger assertions in debug
   * builds.
   *
   * Not calling SetOutputSize() means that we should just decode at the
   * intrinsic size, whatever it is.
   *
   * If SetOutputSize() was called, ExplicitOutputSize() can be used to
   * determine the value that was passed to it.
   *
   * This must be called before Init() is called.
   */
  void SetOutputSize(const OrientedIntSize& aSize);

  /**
   * @return the output size of this decoder. If this is smaller than the
   * intrinsic size, then the image will be downscaled during the decoding
   * process.
   *
   * Illegal to call if HasSize() returns false.
   */
  OrientedIntSize OutputSize() const {
    MOZ_ASSERT(HasSize());
    return *mOutputSize;
  }

  /**
   * @return either the size passed to SetOutputSize() or Nothing(), indicating
   * that SetOutputSize() was not explicitly called.
   */
  Maybe<OrientedIntSize> ExplicitOutputSize() const;

  /**
   * Sets the expected image size of this decoder. Decoding will fail if this
   * does not match.
   */
  void SetExpectedSize(const OrientedIntSize& aSize) {
    mExpectedSize.emplace(aSize);
  }

  /**
   * Is the image size what was expected, if specified?
   */
  bool IsExpectedSize() const {
    return mExpectedSize.isNothing() || *mExpectedSize == Size();
  }

  /**
   * Set an iterator to the SourceBuffer which will feed data to this decoder.
   * This must always be called before calling Init(). (And only before Init().)
   *
   * XXX(seth): We should eliminate this method and pass a SourceBufferIterator
   * to the various decoder constructors instead.
   */
  void SetIterator(SourceBufferIterator&& aIterator) {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mIterator.emplace(std::move(aIterator));
  }

  SourceBuffer* GetSourceBuffer() const { return mIterator->Owner(); }

  /**
   * Should this decoder send partial invalidations?
   */
  bool ShouldSendPartialInvalidations() const {
    return !(mDecoderFlags & DecoderFlags::IS_REDECODE);
  }

  /**
   * Should we stop decoding after the first frame?
   */
  bool IsFirstFrameDecode() const {
    return bool(mDecoderFlags & DecoderFlags::FIRST_FRAME_ONLY);
  }

  /**
   * @return the number of complete animation frames which have been decoded so
   * far, if it has changed since the last call to TakeCompleteFrameCount();
   * otherwise, returns Nothing().
   */
  Maybe<uint32_t> TakeCompleteFrameCount();

  // The number of frames we have, including anything in-progress. Thus, this
  // is only 0 if we haven't begun any frames.
  uint32_t GetFrameCount() { return mFrameCount; }

  // Did we discover that the image we're decoding is animated?
  bool HasAnimation() const { return mImageMetadata.HasAnimation(); }

  // Error tracking
  bool HasError() const { return mError; }
  bool ShouldReportError() const { return mShouldReportError; }

  // Finalize frames
  void SetFinalizeFrames(bool aFinalize) { mFinalizeFrames = aFinalize; }
  bool GetFinalizeFrames() const { return mFinalizeFrames; }

  /// Did we finish decoding enough that calling Decode() again would be
  /// useless?
  bool GetDecodeDone() const {
    return mReachedTerminalState || mDecodeDone ||
           (mMetadataDecode && HasSize()) || HasError();
  }

  /// Are we in the middle of a frame right now? Used for assertions only.
  bool InFrame() const { return mInFrame; }

  /// Is the image valid if embedded inside an ICO.
  virtual bool IsValidICOResource() const { return false; }

  /// Type of decoder.
  virtual DecoderType GetType() const { return DecoderType::UNKNOWN; }

  enum DecodeStyle {
    PROGRESSIVE,  // produce intermediate frames representing the partial
                  // state of the image
    SEQUENTIAL    // decode to final image immediately
  };

  /**
   * Get or set the DecoderFlags that influence the behavior of this decoder.
   */
  void SetDecoderFlags(DecoderFlags aDecoderFlags) {
    MOZ_ASSERT(!mInitialized);
    mDecoderFlags = aDecoderFlags;
  }
  DecoderFlags GetDecoderFlags() const { return mDecoderFlags; }

  /**
   * Get or set the SurfaceFlags that select the kind of output this decoder
   * will produce.
   */
  void SetSurfaceFlags(SurfaceFlags aSurfaceFlags);
  SurfaceFlags GetSurfaceFlags() const { return mSurfaceFlags; }

  /// @return true if we know the intrinsic size of the image we're decoding.
  bool HasSize() const { return mImageMetadata.HasSize(); }

  /**
   * @return the intrinsic size of the image we're decoding.
   *
   * Illegal to call if HasSize() returns false.
   */
  OrientedIntSize Size() const {
    MOZ_ASSERT(HasSize());
    return mImageMetadata.GetSize();
  }

  /**
   * @return an IntRect which covers the entire area of this image at its
   * intrinsic size, appropriate for use as a frame rect when the image itself
   * does not specify one.
   *
   * Illegal to call if HasSize() returns false.
   */
  OrientedIntRect FullFrame() const {
    return OrientedIntRect(OrientedIntPoint(), Size());
  }

  /**
   * @return an IntRect which covers the entire area of this image at its size
   * after scaling - that is, at its output size.
   *
   * XXX(seth): This is only used for decoders which are using the old
   * Downscaler code instead of SurfacePipe, since the old AllocateFrame() and
   * Downscaler APIs required that the frame rect be specified in output space.
   * We should remove this once all decoders use SurfacePipe.
   *
   * Illegal to call if HasSize() returns false.
   */
  OrientedIntRect FullOutputFrame() const {
    return OrientedIntRect(OrientedIntPoint(), OutputSize());
  }

  /**
   * @return the orientation of the image.
   *
   * Illegal to call if HasSize() returns false.
   */
  Orientation GetOrientation() const {
    MOZ_ASSERT(HasSize());
    return mImageMetadata.GetOrientation();
  }

  /// @return final status information about this decoder. Should be called
  /// after we decide we're not going to run the decoder anymore.
  DecoderFinalStatus FinalStatus() const;

  /// @return the metadata we collected about this image while decoding.
  const ImageMetadata& GetImageMetadata() { return mImageMetadata; }

  /// @return performance telemetry we collected while decoding.
  DecoderTelemetry Telemetry() const;

  /**
   * @return a weak pointer to the image associated with this decoder. Illegal
   * to call if this decoder is not associated with an image.
   */
  NotNull<RasterImage*> GetImage() const { return WrapNotNull(mImage.get()); }

  /**
   * @return a possibly-null weak pointer to the image associated with this
   * decoder. May be called even if this decoder is not associated with an
   * image.
   */
  RasterImage* GetImageMaybeNull() const { return mImage.get(); }

  RawAccessFrameRef GetCurrentFrameRef() {
    return mCurrentFrame ? mCurrentFrame->RawAccessRef() : RawAccessFrameRef();
  }

  /**
   * For use during decoding only. Allows the BlendAnimationFilter to get the
   * current frame we are producing for its animation parameters.
   */
  imgFrame* GetCurrentFrame() { return mCurrentFrame.get(); }

  /**
   * For use during decoding only. Allows the BlendAnimationFilter to get the
   * frame it should be pulling the previous frame data from.
   */
  const RawAccessFrameRef& GetRestoreFrameRef() const { return mRestoreFrame; }

  const gfx::IntRect& GetRestoreDirtyRect() const { return mRestoreDirtyRect; }

  const gfx::IntRect& GetRecycleRect() const { return mRecycleRect; }

  const gfx::IntRect& GetFirstFrameRefreshArea() const {
    return mFirstFrameRefreshArea;
  }

  bool HasFrameToTake() const { return mHasFrameToTake; }
  void ClearHasFrameToTake() {
    MOZ_ASSERT(mHasFrameToTake);
    mHasFrameToTake = false;
  }

  IDecoderFrameRecycler* GetFrameRecycler() const { return mFrameRecycler; }
  void SetFrameRecycler(IDecoderFrameRecycler* aFrameRecycler) {
    mFrameRecycler = aFrameRecycler;
  }

 protected:
  friend class AutoRecordDecoderTelemetry;
  friend class DecoderTestHelper;
  friend class nsBMPDecoder;
  friend class nsICODecoder;
  friend class ReorientSurfaceSink;
  friend class SurfaceSink;

  virtual ~Decoder();

  /*
   * Internal hooks. Decoder implementations may override these and
   * only these methods.
   *
   * BeforeFinishInternal() can be used to detect if decoding is in an
   * incomplete state, e.g. due to file truncation, in which case it should
   * return a failing nsresult.
   */
  virtual nsresult InitInternal();
  virtual LexerResult DoDecode(SourceBufferIterator& aIterator,
                               IResumable* aOnResume) = 0;
  virtual nsresult BeforeFinishInternal();
  virtual nsresult FinishInternal();
  virtual nsresult FinishWithErrorInternal();

  qcms_profile* GetCMSOutputProfile() const;
  qcms_transform* GetCMSsRGBTransform(gfx::SurfaceFormat aFormat) const;

  /**
   * @return the per-image-format telemetry ID for recording this decoder's
   * speed, or Nothing() if we don't record speed telemetry for this kind of
   * decoder.
   */
  virtual Maybe<Telemetry::HistogramID> SpeedHistogram() const {
    return Nothing();
  }

  /*
   * Progress notifications.
   */

  // Called by decoders when they determine the size of the image. Informs
  // the image of its size and sends notifications.
  void PostSize(int32_t aWidth, int32_t aHeight, Orientation = Orientation(),
                Resolution = Resolution());

  // Called by decoders if they determine that the image has transparency.
  //
  // This should be fired as early as possible to allow observers to do things
  // that affect content, so it's necessarily pessimistic - if there's a
  // possibility that the image has transparency, for example because its header
  // specifies that it has an alpha channel, we fire PostHasTransparency
  // immediately. PostFrameStop's aFrameOpacity argument, on the other hand, is
  // only used internally to ImageLib. Because PostFrameStop isn't delivered
  // until the entire frame has been decoded, decoders may take into account the
  // actual contents of the frame and give a more accurate result.
  void PostHasTransparency();

  // Called by decoders if they determine that the image is animated.
  //
  // @param aTimeout The time for which the first frame should be shown before
  //                 we advance to the next frame.
  void PostIsAnimated(FrameTimeout aFirstFrameTimeout);

  // Called by decoders when they end a frame. Informs the image, sends
  // notifications, and does internal book-keeping.
  // Specify whether this frame is opaque as an optimization.
  // For animated images, specify the disposal, blend method and timeout for
  // this frame.
  void PostFrameStop(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY);

  /**
   * Called by the decoders when they have a region to invalidate. We may not
   * actually pass these invalidations on right away.
   *
   * @param aRect The invalidation rect in the coordinate system of the unscaled
   *              image (that is, the image at its intrinsic size).
   * @param aRectAtOutputSize If not Nothing(), the invalidation rect in the
   *                          coordinate system of the scaled image (that is,
   *                          the image at our output size). This must
   *                          be supplied if we're downscaling during decode.
   */
  void PostInvalidation(
      const OrientedIntRect& aRect,
      const Maybe<OrientedIntRect>& aRectAtOutputSize = Nothing());

  // Called by the decoders when they have successfully decoded the image. This
  // may occur as the result of the decoder getting to the appropriate point in
  // the stream, or by us calling FinishInternal().
  //
  // May not be called mid-frame.
  //
  // For animated images, specify the loop count. -1 means loop forever, 0
  // means a single iteration, stopping on the last frame.
  void PostDecodeDone(int32_t aLoopCount = 0);

  /**
   * Allocates a new frame, making it our current frame if successful.
   */
  nsresult AllocateFrame(const gfx::IntSize& aOutputSize,
                         gfx::SurfaceFormat aFormat,
                         const Maybe<AnimationParams>& aAnimParams = Nothing());

 private:
  /// Report that an error was encountered while decoding.
  void PostError();

  /**
   * CompleteDecode() finishes up the decoding process after Decode() determines
   * that we're finished. It records final progress and does all the cleanup
   * that's possible off-main-thread.
   */
  void CompleteDecode();

  /// @return the number of complete frames we have. Does not include the
  /// current frame if it's unfinished.
  uint32_t GetCompleteFrameCount() {
    if (mFrameCount == 0) {
      return 0;
    }

    return mInFrame ? mFrameCount - 1 : mFrameCount;
  }

  RawAccessFrameRef AllocateFrameInternal(
      const gfx::IntSize& aOutputSize, gfx::SurfaceFormat aFormat,
      const Maybe<AnimationParams>& aAnimParams,
      RawAccessFrameRef&& aPreviousFrame);

 protected:
  /// Color management profile from the ICCP chunk in the image.
  qcms_profile* mInProfile;

  /// Color management transform to apply to image data.
  qcms_transform* mTransform;

  uint8_t* mImageData;  // Pointer to image data in BGRA/X
  uint32_t mImageDataLength;

  CMSMode mCMSMode;

 private:
  RefPtr<RasterImage> mImage;
  Maybe<SourceBufferIterator> mIterator;
  IDecoderFrameRecycler* mFrameRecycler;

  // The current frame the decoder is producing.
  RawAccessFrameRef mCurrentFrame;

  // The complete frame to combine with the current partial frame to produce
  // a complete current frame.
  RawAccessFrameRef mRestoreFrame;

  ImageMetadata mImageMetadata;

  OrientedIntRect
      mInvalidRect;  // Tracks new rows as the current frame is decoded.
  gfx::IntRect mRestoreDirtyRect;  // Tracks an invalidation region between the
                                   // restore frame and the previous frame.
  gfx::IntRect mRecycleRect;       // Tracks an invalidation region between the
                                   // recycled frame and the current frame.
  Maybe<OrientedIntSize> mOutputSize;    // The size of our output surface.
  Maybe<OrientedIntSize> mExpectedSize;  // The expected size of the image.
  Progress mProgress;

  uint32_t mFrameCount;      // Number of frames, including anything in-progress
  FrameTimeout mLoopLength;  // Length of a single loop of this image.
  gfx::IntRect
      mFirstFrameRefreshArea;  // The area of the image that needs to
                               // be invalidated when the animation loops.

  // Telemetry data for this decoder.
  TimeDuration mDecodeTime;

  DecoderFlags mDecoderFlags;
  SurfaceFlags mSurfaceFlags;

  bool mInitialized : 1;
  bool mMetadataDecode : 1;
  bool mHaveExplicitOutputSize : 1;
  bool mInFrame : 1;
  bool mFinishedNewFrame : 1;  // True if PostFrameStop() has been called since
                               // the last call to TakeCompleteFrameCount().
  // Has a new frame that AnimationSurfaceProvider can take. Unfortunately this
  // has to be separate from mFinishedNewFrame because the png decoder yields a
  // new frame before calling PostFrameStop().
  bool mHasFrameToTake : 1;
  bool mReachedTerminalState : 1;
  bool mDecodeDone : 1;
  bool mError : 1;
  bool mShouldReportError : 1;
  bool mFinalizeFrames : 1;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_Decoder_h
