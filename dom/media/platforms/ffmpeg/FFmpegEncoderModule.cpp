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
bool FFmpegEncoderModule<V>::SupportsMimeType(
    const nsACString& aMimeType) const {
  return GetFFmpegEncoderCodecId<V>(aMimeType) != AV_CODEC_ID_NONE;
}

template <int V>
already_AddRefed<MediaDataEncoder> FFmpegEncoderModule<V>::CreateVideoEncoder(
    const CreateEncoderParams& aParams, const bool aHardwareNotAllowed) const {
  AVCodecID codecId = GetFFmpegEncoderCodecId<V>(aParams.mConfig.mMimeType);
  if (codecId == AV_CODEC_ID_NONE) {
    FFMPEGV_LOG("No ffmpeg encoder for %s", aParams.mConfig.mMimeType.get());
    return nullptr;
  }

  RefPtr<MediaDataEncoder> encoder;
  switch (CreateEncoderParams::CodecTypeForMime(aParams.mConfig.mMimeType)) {
    case MediaDataEncoder::CodecType::VP8:
      encoder = new FFmpegVideoEncoder<V, MediaDataEncoder::VP8Config>(
          mLib, codecId, aParams.mTaskQueue, aParams.ToVP8Config());
      break;
    case MediaDataEncoder::CodecType::VP9:
      encoder = new FFmpegVideoEncoder<V, MediaDataEncoder::VP9Config>(
          mLib, codecId, aParams.mTaskQueue, aParams.ToVP9Config());
      break;
    default:
      FFMPEGV_LOG("No ffmpeg encoder for %s", aParams.mConfig.mMimeType.get());
      return nullptr;
  }
  FFMPEGV_LOG("ffmpeg %s encoder: %s has been created",
              aParams.mConfig.mMimeType.get(),
              encoder->GetDescriptionName().get());
  return encoder.forget();
}

template class FFmpegEncoderModule<LIBAV_VER>;

}  // namespace mozilla
