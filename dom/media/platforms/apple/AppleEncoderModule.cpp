/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleEncoderModule.h"

#include "AppleVTEncoder.h"
#include "VideoUtils.h"
#include "AppleUtils.h"

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
  if (!CanLikelyEncode(aConfig)) {
    return false;
  }
  // Only two temporal layers supported, and only from 11.3 and more recent
  if (aConfig.mScalabilityMode == ScalabilityMode::L1T3 ||
      (aConfig.mScalabilityMode != ScalabilityMode::None && !OSSupportsSVC())) {
    return false;
  }
  return aConfig.mCodec == CodecType::H264;
}

already_AddRefed<MediaDataEncoder> AppleEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  RefPtr<MediaDataEncoder> encoder(new AppleVTEncoder(aConfig, aTaskQueue));
  return encoder.forget();
}

#undef LOGE
#undef LOGD

}  // namespace mozilla
