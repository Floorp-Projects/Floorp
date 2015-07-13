/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_Decoder_h
#define mozilla_image_Decoder_h

#include "FrameAnimator.h"
#include "RasterImage.h"
#include "mozilla/RefPtr.h"
#include "DecodePool.h"
#include "ImageMetadata.h"
#include "Orientation.h"
#include "SourceBuffer.h"

namespace mozilla {

namespace Telemetry {
  enum ID : uint32_t;
}

namespace image {

class Decoder : public IResumable
{
public:

  explicit Decoder(RasterImage* aImage);

  /**
   * Initialize an image decoder. Decoders may not be re-initialized.
   */
  void Init();

  /**
   * Decodes, reading all data currently available in the SourceBuffer. If more
   * data is needed, Decode() automatically ensures that it will be called again
   * on a DecodePool thread when the data becomes available.
   *
   * Any errors are reported by setting the appropriate state on the decoder.
   */
  nsresult Decode();

  /**
   * Cleans up the decoder's state and notifies our image about success or
   * failure. May only be called on the main thread.
   */
  void Finish();

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
    return mProgress != NoProgress || !mInvalidRect.IsEmpty();
  }

  // We're not COM-y, so we don't get refcounts by default
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Decoder, override)

  // Implement IResumable.
  virtual void Resume() override;

  /*
   * State.
   */

  // If we're doing a "size decode", we more or less pass through the image
  // data, stopping only to scoop out the image dimensions. A size decode
  // must be enabled by SetSizeDecode() _before_calling Init().
  bool IsSizeDecode() { return mSizeDecode; }
  void SetSizeDecode(bool aSizeDecode)
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mSizeDecode = aSizeDecode;
  }

  /**
   * If this decoder supports downscale-during-decode, sets the target size that
   * this image should be decoded to.
   *
   * If this decoder *doesn't* support downscale-during-decode, returns
   * NS_ERROR_NOT_AVAILABLE. If the provided size is unacceptable, returns
   * another error.
   *
   * Returning NS_OK from this method is a promise that the decoder will decode
   * the image to the requested target size unless it encounters an error.
   *
   * This must be called before Init() is called.
   */
  virtual nsresult SetTargetSize(const nsIntSize& aSize)
  {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /**
   * Set whether should send partial invalidations.
   *
   * If @aSend is true, we'll send partial invalidations when decoding the first
   * frame of the image, so image notifications observers will be able to
   * gradually draw in the image as it downloads.
   *
   * If @aSend is false (the default), we'll only send an invalidation when we
   * complete the first frame.
   *
   * This must be called before Init() is called.
   */
  void SetSendPartialInvalidations(bool aSend)
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mSendPartialInvalidations = aSend;
  }

  /**
   * Set an iterator to the SourceBuffer which will feed data to this decoder.
   *
   * This should be called for almost all decoders; the exceptions are the
   * contained decoders of an nsICODecoder, which will be fed manually via Write
   * instead.
   *
   * This must be called before Init() is called.
   */
  void SetIterator(SourceBufferIterator&& aIterator)
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mIterator.emplace(Move(aIterator));
  }

  /**
   * Set whether this decoder is associated with a transient image. The decoder
   * may choose to avoid certain optimizations that don't pay off for
   * short-lived images in this case.
   */
  void SetImageIsTransient(bool aIsTransient)
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mImageIsTransient = aIsTransient;
  }

  /**
   * Set whether the image is locked for the lifetime of this decoder. We lock
   * the image during our initial decode to ensure that we don't evict any
   * surfaces before we realize that the image is animated.
   */
  void SetImageIsLocked()
  {
    MOZ_ASSERT(!mInitialized, "Shouldn't be initialized yet");
    mImageIsLocked = true;
  }

  bool ImageIsLocked() const { return mImageIsLocked; }

  size_t BytesDecoded() const { return mBytesDecoded; }

  // The amount of time we've spent inside Write() so far for this decoder.
  TimeDuration DecodeTime() const { return mDecodeTime; }

  // The number of times Write() has been called so far for this decoder.
  uint32_t ChunkCount() const { return mChunkCount; }

  // The number of frames we have, including anything in-progress. Thus, this
  // is only 0 if we haven't begun any frames.
  uint32_t GetFrameCount() { return mFrameCount; }

  // The number of complete frames we have (ie, not including anything
  // in-progress).
  uint32_t GetCompleteFrameCount() {
    return mInFrame ? mFrameCount - 1 : mFrameCount;
  }

  // Error tracking
  bool HasError() const { return HasDataError() || HasDecoderError(); }
  bool HasDataError() const { return mDataError; }
  bool HasDecoderError() const { return NS_FAILED(mFailCode); }
  nsresult GetDecoderError() const { return mFailCode; }
  void PostResizeError() { PostDataError(); }

  bool GetDecodeDone() const
  {
    return mDecodeDone || (mSizeDecode && HasSize()) || HasError() || mDataDone;
  }

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

  void SetFlags(uint32_t aFlags) { mFlags = aFlags; }
  uint32_t GetFlags() const { return mFlags; }
  uint32_t GetDecodeFlags() const { return DecodeFlags(mFlags); }

  bool HasSize() const { return mImageMetadata.HasSize(); }
  void SetSizeOnImage();

  nsIntSize GetSize() const
  {
    MOZ_ASSERT(HasSize());
    return mImageMetadata.GetSize();
  }

  virtual Telemetry::ID SpeedHistogram();

  ImageMetadata& GetImageMetadata() { return mImageMetadata; }

  /**
   * Returns a weak pointer to the image associated with this decoder.
   */
  RasterImage* GetImage() const { MOZ_ASSERT(mImage); return mImage.get(); }

  // XXX(seth): This should be removed once we can optimize imgFrame objects
  // off-main-thread. It only exists to support the code in Finish() for
  // nsICODecoder.
  RawAccessFrameRef GetCurrentFrameRef()
  {
    return mCurrentFrame ? mCurrentFrame->RawAccessRef()
                         : RawAccessFrameRef();
  }

  /**
   * Writes data to the decoder. Only public for the benefit of nsICODecoder;
   * other callers should use Decode().
   *
   * @param aBuffer buffer containing the data to be written
   * @param aCount the number of bytes to write
   *
   * Any errors are reported by setting the appropriate state on the decoder.
   */
  void Write(const char* aBuffer, uint32_t aCount);


protected:
  friend class nsICODecoder;

  virtual ~Decoder();

  /*
   * Internal hooks. Decoder implementations may override these and
   * only these methods.
   */
  virtual void InitInternal();
  virtual void WriteInternal(const char* aBuffer, uint32_t aCount);
  virtual void FinishInternal();
  virtual void FinishWithErrorInternal();

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

  // Called by decoders when they begin a frame. Informs the image, sends
  // notifications, and does internal book-keeping.
  void PostFrameStart();

  // Called by decoders when they end a frame. Informs the image, sends
  // notifications, and does internal book-keeping.
  // Specify whether this frame is opaque as an optimization.
  // For animated images, specify the disposal, blend method and timeout for
  // this frame.
  void PostFrameStop(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY,
                     DisposalMethod aDisposalMethod = DisposalMethod::KEEP,
                     int32_t aTimeout = 0,
                     BlendMethod aBlendMethod = BlendMethod::OVER);

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

  // Data errors are the fault of the source data, decoder errors are our fault
  void PostDataError();
  void PostDecoderError(nsresult aFailCode);

  /**
   * CompleteDecode() finishes up the decoding process after Decode() determines
   * that we're finished. It records final progress and does all the cleanup
   * that's possible off-main-thread.
   */
  void CompleteDecode();

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

  RawAccessFrameRef AllocateFrameInternal(uint32_t aFrameNum,
                                          const nsIntSize& aTargetSize,
                                          const nsIntRect& aFrameRect,
                                          uint32_t aDecodeFlags,
                                          gfx::SurfaceFormat aFormat,
                                          uint8_t aPaletteDepth,
                                          imgFrame* aPreviousFrame);

  /*
   * Member variables.
   *
   */
  nsRefPtr<RasterImage> mImage;
  Maybe<SourceBufferIterator> mIterator;
  RawAccessFrameRef mCurrentFrame;
  ImageMetadata mImageMetadata;
  nsIntRect mInvalidRect; // Tracks an invalidation region in the current frame.
  Progress mProgress;

  uint8_t* mImageData;  // Pointer to image data in either Cairo or 8bit format
  uint32_t mImageDataLength;
  uint32_t* mColormap;  // Current colormap to be used in Cairo format
  uint32_t mColormapSize;

  // Telemetry data for this decoder.
  TimeDuration mDecodeTime;
  uint32_t mChunkCount;

  uint32_t mFlags;
  size_t mBytesDecoded;
  bool mSendPartialInvalidations;
  bool mDataDone;
  bool mDecodeDone;
  bool mDataError;
  bool mDecodeAborted;
  bool mShouldReportError;
  bool mImageIsTransient;
  bool mImageIsLocked;

private:
  uint32_t mFrameCount; // Number of frames, including anything in-progress

  nsresult mFailCode;

  bool mInitialized;
  bool mSizeDecode;
  bool mInFrame;
  bool mIsAnimated;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_Decoder_h
