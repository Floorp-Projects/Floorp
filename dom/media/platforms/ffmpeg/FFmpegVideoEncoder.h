/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_

#include "PlatformEncoderModule.h"
#include "FFmpegLibWrapper.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
AVCodecID GetFFmpegEncoderCodecId(const nsACString& aMimeType);

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(const nsACString& aMimeType);

template <int V>
class FFmpegVideoEncoder {};

// TODO: Bug 1860925: FFmpegDataEncoder
template <>
class FFmpegVideoEncoder<LIBAV_VER> final : public MediaDataEncoder {
 public:
  explicit FFmpegVideoEncoder(const FFmpegLibWrapper* aLib) : mLib(aLib) {
    MOZ_ASSERT(mLib);
  };

  /* MediaDataEncoder Methods */
  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(Rate aBitsPerSec) override;
  nsCString GetDescriptionName() const override;

 private:
  ~FFmpegVideoEncoder() = default;

  // This refers to a static FFmpegLibWrapper, so raw pointer is adequate.
  const FFmpegLibWrapper* mLib;  // set in constructor
};

}  // namespace mozilla

#endif /* DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_ */
