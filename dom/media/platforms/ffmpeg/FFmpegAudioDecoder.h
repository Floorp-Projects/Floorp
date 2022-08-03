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
    return "ffmpeg audio decoder"_ns;
  }

 private:
  MediaResult DoDecode(MediaRawData* aSample, uint8_t* aData, int aSize,
                       bool* aGotFrame, DecodedData& aResults) override;
  uint32_t mEncoderDelay = 0;
  uint32_t mEncoderPadding = 0;
};

}  // namespace mozilla

#endif  // __FFmpegAACDecoder_h__
