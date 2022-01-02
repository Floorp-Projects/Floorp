/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsWebPDecoder_h
#define mozilla_image_decoders_nsWebPDecoder_h

#include "Decoder.h"
#include "webp/demux.h"
#include "StreamingLexer.h"
#include "SurfacePipe.h"

namespace mozilla {
namespace image {
class RasterImage;

class nsWebPDecoder final : public Decoder {
 public:
  virtual ~nsWebPDecoder();

  DecoderType GetType() const override { return DecoderType::WEBP; }

 protected:
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  Maybe<Telemetry::HistogramID> SpeedHistogram() const override;

 private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsWebPDecoder(RasterImage* aImage);

  void ApplyColorProfile(const char* aProfile, size_t aLength);

  LexerResult UpdateBuffer(SourceBufferIterator& aIterator,
                           SourceBufferIterator::State aState);
  LexerResult ReadData();
  LexerResult ReadHeader(WebPDemuxer* aDemuxer, bool aIsComplete);
  LexerResult ReadPayload(WebPDemuxer* aDemuxer, bool aIsComplete);

  nsresult CreateFrame(const OrientedIntRect& aFrameRect);
  void EndFrame();

  LexerResult ReadSingle(const uint8_t* aData, size_t aLength,
                         const OrientedIntRect& aFrameRect);

  LexerResult ReadMultiple(WebPDemuxer* aDemuxer, bool aIsComplete);

  /// The SurfacePipe used to write to the output surface.
  SurfacePipe mPipe;

  /// The buffer used to accumulate data until the complete WebP header is
  /// received, if and only if the iterator is discontiguous.
  Vector<uint8_t> mBufferedData;

  /// The libwebp output buffer descriptor pointing to the decoded data.
  WebPDecBuffer mBuffer;

  /// The libwebp incremental decoder descriptor, wraps mBuffer.
  WebPIDecoder* mDecoder;

  /// Blend method for the current frame.
  BlendMethod mBlend;

  /// Disposal method for the current frame.
  DisposalMethod mDisposal;

  /// Frame timeout for the current frame;
  FrameTimeout mTimeout;

  /// Surface format for the current frame.
  gfx::SurfaceFormat mFormat;

  /// Frame rect for the current frame.
  OrientedIntRect mFrameRect;

  /// The last row of decoded pixels written to mPipe.
  int mLastRow;

  /// Number of decoded frames.
  uint32_t mCurrentFrame;

  /// Pointer to the start of the contiguous encoded image data.
  const uint8_t* mData;

  /// Length of data pointed to by mData.
  size_t mLength;

  /// True if the iterator has reached its end.
  bool mIteratorComplete;

  /// True if this decoding pass requires a WebPDemuxer.
  bool mNeedDemuxer;

  /// True if we have setup the color profile for the image.
  bool mGotColorProfile;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_decoders_nsWebPDecoder_h
