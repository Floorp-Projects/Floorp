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
#include "DecodePool.h"
#include "DecoderFlags.h"
#include "Downscaler.h"
#include "ImageMetadata.h"
#include "Orientation.h"
#include "SourceBuffer.h"
#include "StreamingLexer.h"
#include "SurfaceFlags.h"

namespace mozilla {

namespace Telemetry {
  enum ID : uint32_t;
} // namespace Telemetry

namespace image {

class Decoder
{
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
   *   - the decoder is yielding until it gets more data (Yield::NEED_MORE_DATA), or
   *   - the decoder is yielding to allow the caller to access intermediate
   *     output (Yield::OUTPUT_AVAILABLE).
   */
  LexerResult Decode(IResumable* aOnResume = nullptr);

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
  nsIntRect TakeInvalidRect()
  {
    nsIntRect invalidRect = mInvalidRect;
    mInvalidRect.SetEmpty();
    return invalidRect;
  }

  /**
   * Gets the progress changes accumulated by the decoder so far, and clears
   * them. This means that each call to TakeProgress() returns only the changes
   * accumulated since the last call to TakeProgress().
   */
  Progress TakeProgress()
  {
    Progress progress = mProgress;
    mProgress = NoProgress;
    return progress;
  }

  /**
   * Returns true if there's any progress to report.
   */
  bool HasProgress() const
  {
    return mProgress != NoProgress || !mInvalidRect.IsEmpty() || mFinishedNewFrame;
  }

  /*
   * State.
   */

  /**
   * If we're doing a metadata decode, we only decode the image's headers, which
   * is enough to determine the image's intrinsic size. A metadata decode is
   * enabled by calling SetMetadataDecode() *before* calling Init().
   */
  void SetMetadataDecode(bool aMetadataDecode)
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mMetadataDecode = aMetadataDecode;
  }
  bool IsMetadataDecode() const { return mMetadataDecode; }

  /**
   * If this decoder supports downscale-during-decode, sets the target size that
   * this image should be decoded to.
   *
   * If the provided size is unacceptable, an error is returned.
   *
   * Returning NS_OK from this method is a promise that the decoder will decode
   * the image to the requested target size unless it encounters an error.
   *
   * This must be called before Init() is called.
   */
  nsresult SetTargetSize(const gfx::IntSize& aSize);

  /**
   * If this decoder supports downscale-during-decode and is configured to
   * downscale, returns the target size that the output size will be decoded to.
   * Otherwise, returns Nothing().
   */
  Maybe<gfx::IntSize> GetTargetSize();

  /**
   * Set the requested sample size for this decoder. Used to implement the
   * -moz-sample-size media fragment.
   *
   *  XXX(seth): Support for -moz-sample-size will be removed in bug 1120056.
   */
  virtual void SetSampleSize(int aSampleSize) { }

  /**
   * Set an iterator to the SourceBuffer which will feed data to this decoder.
   * This must always be called before calling Init(). (And only before Init().)
   *
   * XXX(seth): We should eliminate this method and pass a SourceBufferIterator
   * to the various decoder constructors instead.
   */
  void SetIterator(SourceBufferIterator&& aIterator)
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mIterator.emplace(Move(aIterator));
  }

  /**
   * Should this decoder send partial invalidations?
   */
  bool ShouldSendPartialInvalidations() const
  {
    return !(mDecoderFlags & DecoderFlags::IS_REDECODE);
  }

  /**
   * Should we stop decoding after the first frame?
   */
  bool IsFirstFrameDecode() const
  {
    return bool(mDecoderFlags & DecoderFlags::FIRST_FRAME_ONLY);
  }

  size_t BytesDecoded() const
  {
    MOZ_ASSERT(mIterator);
    return mIterator->ByteCount();
  }

  // The amount of time we've spent inside DoDecode() so far for this decoder.
  TimeDuration DecodeTime() const { return mDecodeTime; }

  // The number of chunks this decoder's input was divided into.
  uint32_t ChunkCount() const
  {
    MOZ_ASSERT(mIterator);
    return mIterator->ChunkCount();
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

  /// Did we finish decoding enough that calling Decode() again would be useless?
  bool GetDecodeDone() const
  {
    return mReachedTerminalState || mDecodeDone ||
           (mMetadataDecode && HasSize()) || HasError();
  }

  /// Are we in the middle of a frame right now? Used for assertions only.
  bool InFrame() const { return mInFrame; }

  /// Should we store surfaces created by this decoder in the SurfaceCache?
  bool ShouldUseSurfaceCache() const { return bool(mImage); }

  /**
   * Returns true if this decoder was aborted.
   *
   * This may happen due to a low-memory condition, or because another decoder
   * was racing with this one to decode the same frames with the same flags and
   * this decoder lost the race. Either way, this is not a permanent situation
   * and does not constitute an error, so we don't report any errors when this
   * happens.
   */
  bool WasAborted() const { return mDecodeAborted; }

  enum DecodeStyle {
      PROGRESSIVE, // produce intermediate frames representing the partial
                   // state of the image
      SEQUENTIAL   // decode to final image immediately
  };

  /**
   * Get or set the DecoderFlags that influence the behavior of this decoder.
   */
  void SetDecoderFlags(DecoderFlags aDecoderFlags)
  {
    MOZ_ASSERT(!mInitialized);
    mDecoderFlags = aDecoderFlags;
  }
  DecoderFlags GetDecoderFlags() const { return mDecoderFlags; }

  /**
   * Get or set the SurfaceFlags that select the kind of output this decoder
   * will produce.
   */
  void SetSurfaceFlags(SurfaceFlags aSurfaceFlags)
  {
    MOZ_ASSERT(!mInitialized);
    mSurfaceFlags = aSurfaceFlags;
  }
  SurfaceFlags GetSurfaceFlags() const { return mSurfaceFlags; }

  bool HasSize() const { return mImageMetadata.HasSize(); }

  nsIntSize GetSize() const
  {
    MOZ_ASSERT(HasSize());
    return mImageMetadata.GetSize();
  }

  virtual Telemetry::ID SpeedHistogram();

  ImageMetadata& GetImageMetadata() { return mImageMetadata; }

  /**
   * @return a weak pointer to the image associated with this decoder. Illegal
   * to call if this decoder is not associated with an image.
   */
  RasterImage* GetImage() const { MOZ_ASSERT(mImage); return mImage.get(); }

  /**
   * @return a possibly-null weak pointer to the image associated with this
   * decoder. May be called even if this decoder is not associated with an
   * image.
   */
  RasterImage* GetImageMaybeNull() const { return mImage.get(); }

  RawAccessFrameRef GetCurrentFrameRef()
  {
    return mCurrentFrame ? mCurrentFrame->RawAccessRef()
                         : RawAccessFrameRef();
  }


protected:
  friend class AutoRecordDecoderTelemetry;
  friend class nsICODecoder;
  friend class PalettedSurfaceSink;
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

  /*
   * Progress notifications.
   */

  // Called by decoders when they determine the size of the image. Informs
  // the image of its size and sends notifications.
  void PostSize(int32_t aWidth,
                int32_t aHeight,
                Orientation aOrientation = Orientation());

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
  void PostFrameStop(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY,
                     DisposalMethod aDisposalMethod = DisposalMethod::KEEP,
                     FrameTimeout aTimeout = FrameTimeout::Forever(),
                     BlendMethod aBlendMethod = BlendMethod::OVER,
                     const Maybe<nsIntRect>& aBlendRect = Nothing());

  /**
   * Called by the decoders when they have a region to invalidate. We may not
   * actually pass these invalidations on right away.
   *
   * @param aRect The invalidation rect in the coordinate system of the unscaled
   *              image (that is, the image at its intrinsic size).
   * @param aRectAtTargetSize If not Nothing(), the invalidation rect in the
   *                          coordinate system of the scaled image (that is,
   *                          the image at our target decoding size). This must
   *                          be supplied if we're downscaling during decode.
   */
  void PostInvalidation(const nsIntRect& aRect,
                        const Maybe<nsIntRect>& aRectAtTargetSize = Nothing());

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
   *
   * The @aFrameNum parameter only exists as a sanity check; it's illegal to
   * create a new frame anywhere but immediately after the existing frames.
   *
   * If a non-paletted frame is desired, pass 0 for aPaletteDepth.
   */
  nsresult AllocateFrame(uint32_t aFrameNum,
                         const nsIntSize& aTargetSize,
                         const nsIntRect& aFrameRect,
                         gfx::SurfaceFormat aFormat,
                         uint8_t aPaletteDepth = 0);

  /// Helper method for decoders which only have 'basic' frame allocation needs.
  nsresult AllocateBasicFrame() {
    nsIntSize size = GetSize();
    return AllocateFrame(0, size, nsIntRect(nsIntPoint(), size),
                         gfx::SurfaceFormat::B8G8R8A8);
  }

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
  uint32_t GetCompleteFrameCount()
  {
    if (mFrameCount == 0) {
      return 0;
    }

    return mInFrame ? mFrameCount - 1 : mFrameCount;
  }

  RawAccessFrameRef AllocateFrameInternal(uint32_t aFrameNum,
                                          const nsIntSize& aTargetSize,
                                          const nsIntRect& aFrameRect,
                                          gfx::SurfaceFormat aFormat,
                                          uint8_t aPaletteDepth,
                                          imgFrame* aPreviousFrame);

protected:
  Maybe<Downscaler> mDownscaler;

  uint8_t* mImageData;  // Pointer to image data in either Cairo or 8bit format
  uint32_t mImageDataLength;
  uint32_t* mColormap;  // Current colormap to be used in Cairo format
  uint32_t mColormapSize;

private:
  RefPtr<RasterImage> mImage;
  Maybe<SourceBufferIterator> mIterator;
  RawAccessFrameRef mCurrentFrame;
  ImageMetadata mImageMetadata;
  nsIntRect mInvalidRect; // Tracks an invalidation region in the current frame.
  Progress mProgress;

  uint32_t mFrameCount; // Number of frames, including anything in-progress
  FrameTimeout mLoopLength;  // Length of a single loop of this image.
  gfx::IntRect mFirstFrameRefreshArea;  // The area of the image that needs to
                                        // be invalidated when the animation loops.

  // Telemetry data for this decoder.
  TimeDuration mDecodeTime;

  DecoderFlags mDecoderFlags;
  SurfaceFlags mSurfaceFlags;

  bool mInitialized : 1;
  bool mMetadataDecode : 1;
  bool mInFrame : 1;
  bool mFinishedNewFrame : 1;  // True if PostFrameStop() has been called since
                               // the last call to TakeCompleteFrameCount().
  bool mReachedTerminalState : 1;
  bool mDecodeDone : 1;
  bool mError : 1;
  bool mDecodeAborted : 1;
  bool mShouldReportError : 1;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_Decoder_h
