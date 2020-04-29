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

  static intptr_t read_source(uint8_t* aDestBuf, uintptr_t aDestBufSize,
                              void* aUserData);

  Mp4parseAvifParser* mParser;
  Maybe<aom_codec_ctx_t> mCodecContext;

  Vector<uint8_t> mBufferedData;

  /// Pointer to the next place to read from mBufferedData
  const uint8_t* mReadCursor = nullptr;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_decoders_nsAVIFDecoder_h
