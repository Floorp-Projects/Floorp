/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsAVIFDecoder_h
#define mozilla_image_decoders_nsAVIFDecoder_h

#include "Decoder.h"
#include "mp4parse.h"
#include "SurfacePipe.h"

#include "aom/aom_decoder.h"
#include "dav1d/dav1d.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace image {
class RasterImage;

class nsAVIFDecoder final : public Decoder {
 public:
  virtual ~nsAVIFDecoder();

  DecoderType GetType() const override { return DecoderType::AVIF; }

 protected:
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;

 private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsAVIFDecoder(RasterImage* aImage);

  static intptr_t ReadSource(uint8_t* aDestBuf, uintptr_t aDestBufSize,
                             void* aUserData);

  static void FreeDav1dData(const uint8_t* buf, void* cookie);

  typedef int Dav1dResult;
  Dav1dResult DecodeWithDav1d(const Mp4parseByteData& aPrimaryItem,
                              layers::PlanarYCbCrData& aDecodedData);

  enum class NonAOMCodecError { NoFrame, SizeOverflow };
  typedef Variant<aom_codec_err_t, NonAOMCodecError> AOMResult;
  AOMResult DecodeWithAOM(const Mp4parseByteData& aPrimaryItem,
                          layers::PlanarYCbCrData& aDecodedData);

  enum class NonDecoderResult {
    NeedMoreData,
    MetadataOk,
    ParseError,
    NoPrimaryItem,
    SizeOverflow,
    OutOfMemory,
    PipeInitError,
    WriteBufferError
  };
  using DecodeResult = Variant<NonDecoderResult, Dav1dResult, AOMResult>;
  DecodeResult Decode(SourceBufferIterator& aIterator, IResumable* aOnResume);

  bool IsDecodeSuccess(const DecodeResult& aResult);

  void RecordDecodeResultTelemetry(const DecodeResult& aResult);

  Mp4parseAvifParser* mParser;
  Maybe<Variant<aom_codec_ctx_t, Dav1dContext*>> mCodecContext;

  Maybe<Dav1dPicture> mDav1dPicture;

  Vector<uint8_t> mBufferedData;

  /// Pointer to the next place to read from mBufferedData
  const uint8_t* mReadCursor = nullptr;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_decoders_nsAVIFDecoder_h
