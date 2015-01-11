/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_DECODER_H_
#define MOZILLA_IMAGELIB_DECODER_H_

#include "FrameAnimator.h"
#include "RasterImage.h"
#include "mozilla/RefPtr.h"
#include "DecodePool.h"
#include "ImageMetadata.h"
#include "Orientation.h"
#include "mozilla/Telemetry.h"

namespace mozilla {

namespace image {

class Decoder
{
public:

  explicit Decoder(RasterImage* aImage);

  /**
   * Initialize an image decoder. Decoders may not be re-initialized.
   *
   * Notifications Sent: TODO
   */
  void Init();

  /**
   * Writes data to the decoder.
   *
   * @param aBuffer buffer containing the data to be written
   * @param aCount the number of bytes to write
   *
   * Any errors are reported by setting the appropriate state on the decoder.
   *
   * Notifications Sent: TODO
   */
  void Write(const char* aBuffer, uint32_t aCount);

  /**
   * Informs the decoder that all the data has been written.
   *
   * Notifications Sent: TODO
   */
  void Finish(ShutdownReason aReason);

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

  // We're not COM-y, so we don't get refcounts by default
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Decoder)

  /*
   * State.
   */

  // If we're doing a "size decode", we more or less pass through the image
  // data, stopping only to scoop out the image dimensions. A size decode
  // must be enabled by SetSizeDecode() _before_calling Init().
  bool IsSizeDecode() { return mSizeDecode; }
  void SetSizeDecode(bool aSizeDecode)
  {
    NS_ABORT_IF_FALSE(!mInitialized, "Can't set size decode after Init()!");
    mSizeDecode = aSizeDecode;
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

  size_t BytesDecoded() const { return mBytesDecoded; }

  // The amount of time we've spent inside Write() so far for this decoder.
  TimeDuration DecodeTime() const { return mDecodeTime; }

  // The number of times Write() has been called so far for this decoder.
  uint32_t ChunkCount() const { return mChunkCount; }

  // The number of frames we have, including anything in-progress. Thus, this
  // is only 0 if we haven't begun any frames.
  uint32_t GetFrameCount() { return mFrameCount; }

  // The number of complete frames we have (ie, not including anything in-progress).
  uint32_t GetCompleteFrameCount() { return mInFrame ? mFrameCount - 1 : mFrameCount; }

  // Error tracking
  bool HasError() { return HasDataError() || HasDecoderError(); }
  bool HasDataError() { return mDataError; }
  bool HasDecoderError() { return NS_FAILED(mFailCode); }
  nsresult GetDecoderError() { return mFailCode; }
  void PostResizeError() { PostDataError(); }
  bool GetDecodeDone() const {
    return mDecodeDone;
  }

  // flags.  Keep these in sync with imgIContainer.idl.
  // SetDecodeFlags must be called before Init(), otherwise
  // default flags are assumed.
  enum {
    DECODER_NO_PREMULTIPLY_ALPHA = 0x2,     // imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA
    DECODER_NO_COLORSPACE_CONVERSION = 0x4  // imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION
  };

  enum DecodeStyle {
      PROGRESSIVE, // produce intermediate frames representing the partial state of the image
      SEQUENTIAL // decode to final image immediately
  };

  void SetDecodeFlags(uint32_t aFlags) { mDecodeFlags = aFlags; }
  uint32_t GetDecodeFlags() { return mDecodeFlags; }

  nsIntSize GetSize() const { return mImageMetadata.GetSize(); }
  bool HasSize() const { return mImageMetadata.HasSize(); }
  void SetSizeOnImage();

  // Use HistogramCount as an invalid Histogram ID
  virtual Telemetry::ID SpeedHistogram() { return Telemetry::HistogramCount; }

  ImageMetadata& GetImageMetadata() { return mImageMetadata; }

  /**
   * Returns a weak pointer to the image associated with this decoder.
   */
  RasterImage* GetImage() const { MOZ_ASSERT(mImage); return mImage.get(); }

  already_AddRefed<imgFrame> GetCurrentFrame()
  {
    nsRefPtr<imgFrame> frame = mCurrentFrame.get();
    return frame.forget();
  }

  RawAccessFrameRef GetCurrentFrameRef()
  {
    return mCurrentFrame ? mCurrentFrame->RawAccessRef()
                         : RawAccessFrameRef();
  }

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

  // Called by the decoders when they have a region to invalidate. We may not
  // actually pass these invalidations on right away.
  void PostInvalidation(nsIntRect& aRect);

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
   * Allocates a new frame, making it our new current frame if successful.
   *
   * The @aFrameNum parameter only exists as a sanity check; it's illegal to
   * create a new frame anywhere but immediately after the existing frames.
   *
   * If a non-paletted frame is desired, pass 0 for aPaletteDepth.
   */
  nsresult AllocateFrame(uint32_t aFrameNum,
                         const nsIntRect& aFrameRect,
                         gfx::SurfaceFormat aFormat,
                         uint8_t aPaletteDepth = 0);

  RawAccessFrameRef AllocateFrameInternal(uint32_t aFrameNum,
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
  RawAccessFrameRef mCurrentFrame;
  ImageMetadata mImageMetadata;
  nsIntRect mInvalidRect; // Tracks an invalidation region in the current frame.
  Progress mProgress;

  uint8_t* mImageData;       // Pointer to image data in either Cairo or 8bit format
  uint32_t mImageDataLength;
  uint32_t* mColormap;       // Current colormap to be used in Cairo format
  uint32_t mColormapSize;

  // Telemetry data for this decoder.
  TimeDuration mDecodeTime;
  uint32_t mChunkCount;

  uint32_t mDecodeFlags;
  size_t mBytesDecoded;
  bool mSendPartialInvalidations;
  bool mDecodeDone;
  bool mDataError;

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

#endif // MOZILLA_IMAGELIB_DECODER_H_
