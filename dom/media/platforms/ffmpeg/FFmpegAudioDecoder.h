/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegAACDecoder_h__
#define __FFmpegAACDecoder_h__

#include "FFmpegDataDecoder.h"
#include "FFmpegLibWrapper.h"

namespace mozilla {

template <int V>
class FFmpegAudioDecoder {};

template <>
class FFmpegAudioDecoder<LIBAV_VER>;
DDLoggedTypeNameAndBase(FFmpegAudioDecoder<LIBAV_VER>,
                        FFmpegDataDecoder<LIBAV_VER>);

template <>
class FFmpegAudioDecoder<LIBAV_VER>
    : public FFmpegDataDecoder<LIBAV_VER>,
      public DecoderDoctorLifeLogger<FFmpegAudioDecoder<LIBAV_VER>> {
 public:
  FFmpegAudioDecoder(FFmpegLibWrapper* aLib, const AudioInfo& aConfig);
  virtual ~FFmpegAudioDecoder();

  RefPtr<InitPromise> Init() override;
  void InitCodecContext() MOZ_REQUIRES(sMutex) override;
  static AVCodecID GetCodecId(const nsACString& aMimeType);
  nsCString GetDescriptionName() const override {
#ifdef USING_MOZFFVPX
    return "ffvpx audio decoder"_ns;
#else
    return "ffmpeg audio decoder"_ns;
#endif
  }
  nsCString GetCodecName() const override;

 private:
  MediaResult DoDecode(MediaRawData* aSample, uint8_t* aData, int aSize,
                       bool* aGotFrame, DecodedData& aResults) override;
  // This method is to be called only when decoding mp3, in order to correctly
  // discard padding frames.
  uint64_t Padding() const;
  // This method is to be called only when decoding AAC, in order to correctly
  // discard padding frames, based on the number of frames decoded and the total
  // frame count of the media.
  uint64_t TotalFrames() const;
  // The number of frames of encoder delay, that need to be discarded at the
  // beginning of the stream.
  uint32_t mEncoderDelay = 0;
  // This holds either the encoder padding (when this decoder decodes mp3), or
  // the total frame count of the media (when this decoder decodes AAC).
  // It is best accessed via the `Padding` and `TotalFrames` methods, for
  // clarity.
  uint64_t mEncoderPaddingOrTotalFrames = 0;
};

}  // namespace mozilla

#endif  // __FFmpegAACDecoder_h__
