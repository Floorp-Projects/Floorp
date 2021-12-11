/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AOMDecoder_h_)
#  define AOMDecoder_h_

#  include <stdint.h>

#  include "PlatformDecoderModule.h"
#  include "aom/aom_decoder.h"
#  include "mozilla/Span.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(AOMDecoder, MediaDataDecoder);

class AOMDecoder : public MediaDataDecoder,
                   public DecoderDoctorLifeLogger<AOMDecoder> {
 public:
  explicit AOMDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return "av1 libaom video decoder"_ns;
  }

  // Return true if aMimeType is a one of the strings used
  // by our demuxers to identify AV1 streams.
  static bool IsAV1(const nsACString& aMimeType);

  // Return true if a sample is a keyframe.
  static bool IsKeyframe(Span<const uint8_t> aBuffer);

  // Return the frame dimensions for a sample.
  static gfx::IntSize GetFrameSize(Span<const uint8_t> aBuffer);

 private:
  ~AOMDecoder();
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);

  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;

  // AOM decoder state
  aom_codec_ctx_t mCodec;

  const VideoInfo mInfo;
};

}  // namespace mozilla

#endif  // AOMDecoder_h_
