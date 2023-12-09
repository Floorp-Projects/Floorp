/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleEncoderModule.h"

#include "AppleVTEncoder.h"
#include "VideoUtils.h"

namespace mozilla {

extern LazyLogModule sPEMLog;
#define LOGE(fmt, ...)                       \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Error, \
          ("[AppleEncoderModule] %s: " fmt, __func__, ##__VA_ARGS__))
#define LOGD(fmt, ...)                       \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Debug, \
          ("[AppleEncoderModule] %s: " fmt, __func__, ##__VA_ARGS__))

bool AppleEncoderModule::SupportsCodec(CodecType aCodec) const {
  return aCodec == CodecType::H264;
}

bool AppleEncoderModule::Supports(const EncoderConfig& aConfig) const {
  if (aConfig.mCodec == CodecType::H264) {
    if (!aConfig.mCodecSpecific ||
        !aConfig.mCodecSpecific->is<H264Specific>()) {
      LOGE(
          "Asking for support codec for h264 without h264 specific config, "
          "error.");
      return false;
    }
    H264Specific specific = aConfig.mCodecSpecific->as<H264Specific>();
    int width = aConfig.mSize.width;
    int height = aConfig.mSize.height;
    if (width % 2 || !width) {
      LOGE("Invalid width of %d for h264", width);
      return false;
    }
    if (height % 2 || !height) {
      LOGE("Invalid height of %d for h264", height);
      return false;
    }
    if (specific.mProfile != H264_PROFILE_BASE &&
        specific.mProfile != H264_PROFILE_MAIN) {
      LOGE("Invalid profile of %d for h264", specific.mProfile);
      return false;
    }
    if (width > 4096 || height > 4096) {
      LOGE("Invalid dimensions of %dx%d for h264 encoding", width, height);
      return false;
    }
    return true;
  }
  return false;
}

already_AddRefed<MediaDataEncoder> AppleEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  RefPtr<MediaDataEncoder> encoder(new AppleVTEncoder(aConfig, aTaskQueue));
  return encoder.forget();
}

#undef LOGE
#undef LOGD

}  // namespace mozilla
