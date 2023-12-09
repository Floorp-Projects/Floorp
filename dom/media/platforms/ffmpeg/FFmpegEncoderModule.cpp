/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegEncoderModule.h"

#include "FFmpegLog.h"
#include "FFmpegVideoEncoder.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
bool FFmpegEncoderModule<V>::Supports(const EncoderConfig& aConfig) const {
  if (aConfig.mCodec == CodecType::H264) {
    if (!aConfig.mCodecSpecific ||
        !aConfig.mCodecSpecific->is<H264Specific>()) {
      return false;
    }
    H264Specific specific = aConfig.mCodecSpecific->as<H264Specific>();
    int width = aConfig.mSize.width;
    int height = aConfig.mSize.height;
    if (width % 2 || !width) {
      return false;
    }
    if (height % 2 || !height) {
      return false;
    }
    if (specific.mProfile != H264_PROFILE_BASE &&
        specific.mProfile != H264_PROFILE_MAIN) {
      return false;
    }
    if (width > 4096 || height > 4096) {
      return false;
    }
  }
  // TODO: add more checks for VP8/VP9/AV1 here , similar to the checks above.
  return SupportsCodec(aConfig.mCodec) != AV_CODEC_ID_NONE;
}

template <int V>
bool FFmpegEncoderModule<V>::SupportsCodec(CodecType aCodec) const {
  return GetFFmpegEncoderCodecId<V>(aCodec) != AV_CODEC_ID_NONE;
}

template <int V>
already_AddRefed<MediaDataEncoder> FFmpegEncoderModule<V>::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  AVCodecID codecId = GetFFmpegEncoderCodecId<V>(aConfig.mCodec);
  if (codecId == AV_CODEC_ID_NONE) {
    FFMPEGV_LOG("No ffmpeg encoder for %d", static_cast<int>(aConfig.mCodec));
    return nullptr;
  }

  RefPtr<MediaDataEncoder> encoder =
      new FFmpegVideoEncoder<V>(mLib, codecId, aTaskQueue, aConfig);
  FFMPEGV_LOG("ffmpeg %d encoder: %s has been created",
              static_cast<int>(aConfig.mCodec),
              encoder->GetDescriptionName().get());
  return encoder.forget();
}

template class FFmpegEncoderModule<LIBAV_VER>;

}  // namespace mozilla
