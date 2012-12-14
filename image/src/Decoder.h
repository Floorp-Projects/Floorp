/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_DECODER_H_
#define MOZILLA_IMAGELIB_DECODER_H_

#include "RasterImage.h"

#include "imgIDecoderObserver.h"

namespace mozilla {
namespace image {

class Decoder
{
public:

  Decoder(RasterImage& aImage, imgIDecoderObserver* aObserver);
  virtual ~Decoder();

  /**
   * Initialize an image decoder. Decoders may not be re-initialized.
   *
   * @param aContainer The image container to decode to.
   * @param aObserver The observer for decode notification events.
   *
   * Notifications Sent: TODO
   */
  void Init();


  /**
   * Initializes a decoder whose aImage and aObserver is already being used by a
   * parent decoder. Decoders may not be re-initialized.
   *
   * @param aContainer The image container to decode to.
   * @param aObserver The observer for decode notification events.
   *
   * Notifications Sent: TODO
   */
  void InitSharedDecoder();

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
  void Finish(RasterImage::eShutdownIntent aShutdownIntent);

  /**
   * Informs the shared decoder that all the data has been written.
   * Should only be used if InitSharedDecoder was useed
   *
   * Notifications Sent: TODO
   */
  void FinishSharedDecoder();

  /**
   * Tells the decoder to flush any pending invalidations. This informs the image
   * frame of its decoded region, and sends the appropriate OnDataAvailable call
   * to consumers.
   *
   * This can be called any time when we're midway through decoding a frame,
   * and must be called after finishing a frame (before starting a new one).
   */
  void FlushInvalidations();

  // We're not COM-y, so we don't get refcounts by default
  NS_INLINE_DECL_REFCOUNTING(Decoder)

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

  // Use HistogramCount as an invalid Histogram ID
  virtual Telemetry::ID SpeedHistogram() { return Telemetry::HistogramCount; }

protected:

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
  void PostSize(int32_t aWidth, int32_t aHeight);

  // Called by decoders when they begin/end a frame. Informs the image, sends
  // notifications, and does internal book-keeping.
  void PostFrameStart();
  void PostFrameStop();

  // Called by the decoders when they have a region to invalidate. We may not
  // actually pass these invalidations on right away.
  void PostInvalidation(nsIntRect& aRect);

  // Called by the decoders when they have successfully decoded the image. This
  // may occur as the result of the decoder getting to the appropriate point in
  // the stream, or by us calling FinishInternal().
  //
  // May not be called mid-frame.
  void PostDecodeDone();

  // Data errors are the fault of the source data, decoder errors are our fault
  void PostDataError();
  void PostDecoderError(nsresult aFailCode);

  /*
   * Member variables.
   *
   */
  RasterImage &mImage;
  nsCOMPtr<imgIDecoderObserver> mObserver;

  uint32_t mDecodeFlags;
  bool mDecodeDone;
  bool mDataError;

private:
  uint32_t mFrameCount; // Number of frames, including anything in-progress

  nsIntRect mInvalidRect; // Tracks an invalidation region in the current frame.

  nsresult mFailCode;

  bool mInitialized;
  bool mSizeDecode;
  bool mInFrame;
  bool mIsAnimated;
  bool mFirstWrite;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_DECODER_H_
