/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidEncoderModule.h"

#include "AndroidDataEncoder.h"
#include "MP4Decoder.h"
#include "VPXDecoder.h"

#include "mozilla/Logging.h"

namespace mozilla {
extern LazyLogModule sPEMLog;
#define AND_PEM_LOG(arg, ...)            \
  MOZ_LOG(                               \
      sPEMLog, mozilla::LogLevel::Debug, \
      ("AndroidEncoderModule(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

bool AndroidEncoderModule::SupportsMimeType(const nsACString& aMimeType) const {
  return (MP4Decoder::IsH264(aMimeType) &&
          java::HardwareCodecCapabilityUtils::HasHWH264(true /* encoder */)) ||
         (VPXDecoder::IsVP8(aMimeType) &&
          java::HardwareCodecCapabilityUtils::HasHWVP8(true /* encoder */)) ||
         (VPXDecoder::IsVP9(aMimeType) &&
          java::HardwareCodecCapabilityUtils::HasHWVP9(true /* encoder */));
}

already_AddRefed<MediaDataEncoder> AndroidEncoderModule::CreateVideoEncoder(
    const CreateEncoderParams& aParams) const {
  RefPtr<MediaDataEncoder> encoder;
  switch (CreateEncoderParams::CodecTypeForMime(aParams.mConfig.mMimeType)) {
    case MediaDataEncoder::CodecType::H264:
      return MakeRefPtr<AndroidDataEncoder<MediaDataEncoder::H264Config>>(
                 aParams.ToH264Config(), aParams.mTaskQueue)
          .forget();
    case MediaDataEncoder::CodecType::VP8:
      return MakeRefPtr<AndroidDataEncoder<MediaDataEncoder::VP8Config>>(
                 aParams.ToVP8Config(), aParams.mTaskQueue)
          .forget();
    case MediaDataEncoder::CodecType::VP9:
      return MakeRefPtr<AndroidDataEncoder<MediaDataEncoder::VP9Config>>(
                 aParams.ToVP9Config(), aParams.mTaskQueue)
          .forget();
    default:
      AND_PEM_LOG("Unsupported MIME type:%s", aParams.mConfig.mMimeType.get());
      return nullptr;
  }
}

}  // namespace mozilla

#undef AND_PEM_LOG
