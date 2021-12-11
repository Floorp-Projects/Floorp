/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsJXLDecoder_h
#define mozilla_image_decoders_nsJXLDecoder_h

#include "Decoder.h"
#include "mp4parse.h"
#include "SurfacePipe.h"

#include "jxl/decode_cxx.h"
#include "jxl/thread_parallel_runner_cxx.h"

#include "mozilla/Telemetry.h"

namespace mozilla::image {
class RasterImage;

class nsJXLDecoder final : public Decoder {
 public:
  virtual ~nsJXLDecoder();

  DecoderType GetType() const override { return DecoderType::JXL; }

 protected:
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;

 private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsJXLDecoder(RasterImage* aImage);

  size_t PreferredThreadCount();

  enum class State { JXL_DATA, FINISHED_JXL_DATA };

  LexerTransition<State> ReadJXLData(const char* aData, size_t aLength);
  LexerTransition<State> FinishedJXLData();

  StreamingLexer<State> mLexer;
  JxlDecoderPtr mDecoder;
  JxlThreadParallelRunnerPtr mParallelRunner;
  Vector<uint8_t> mBuffer;
  Vector<uint8_t> mOutBuffer;
  JxlBasicInfo mInfo{};
};

}  // namespace mozilla::image

#endif  // mozilla_image_decoders_nsJXLDecoder_h
