/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidEncoderModule.h"

#include "AndroidDataEncoder.h"

#include "mozilla/Logging.h"
#include "mozilla/java/HardwareCodecCapabilityUtilsWrappers.h"

namespace mozilla {
extern LazyLogModule sPEMLog;
#define AND_PEM_LOG(arg, ...)            \
  MOZ_LOG(                               \
      sPEMLog, mozilla::LogLevel::Debug, \
      ("AndroidEncoderModule(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

bool AndroidEncoderModule::SupportsCodec(CodecType aCodec) const {
  return (aCodec == CodecType::H264 &&
          java::HardwareCodecCapabilityUtils::HasHWH264(true /* encoder */)) ||
         (aCodec == CodecType::VP8 &&
          java::HardwareCodecCapabilityUtils::HasHWVP8(true /* encoder */)) ||
         (aCodec == CodecType::VP9 &&
          java::HardwareCodecCapabilityUtils::HasHWVP9(true /* encoder */));
}

bool AndroidEncoderModule::Supports(const EncoderConfig& aConfig) const {
  if (!CanLikelyEncode(aConfig)) {
    return false;
  }
  return SupportsCodec(aConfig.mCodec);
}

already_AddRefed<MediaDataEncoder> AndroidEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  if (!Supports(aConfig)) {
    AND_PEM_LOG("Unsupported codec type: %s",
                GetCodecTypeString(aConfig.mCodec));
    return nullptr;
  }
  return MakeRefPtr<AndroidDataEncoder>(aConfig, aTaskQueue).forget();
}

}  // namespace mozilla

#undef AND_PEM_LOG
