/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidDecoderModule.h"
#include "AndroidBridge.h"

#include "MediaCodecDataDecoder.h"
#include "RemoteDataDecoder.h"

#include "MediaInfo.h"
#include "VPXDecoder.h"

#include "MediaPrefs.h"
#include "nsPromiseFlatString.h"
#include "nsIGfxInfo.h"

#include "prlog.h"

#include <jni.h>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sAndroidDecoderModuleLog, \
    mozilla::LogLevel::Debug, ("AndroidDecoderModule(%p)::%s: " arg, \
      this, __func__, ##__VA_ARGS__))

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::java::sdk;
using media::TimeUnit;

namespace mozilla {

mozilla::LazyLogModule sAndroidDecoderModuleLog("AndroidDecoderModule");

static const char*
TranslateMimeType(const nsACString& aMimeType)
{
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8)) {
    return "video/x-vnd.on2.vp8";
  } else if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9)) {
    return "video/x-vnd.on2.vp9";
  }
  return PromiseFlatCString(aMimeType).get();
}

static bool
GetFeatureStatus(int32_t aFeature)
{
  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  int32_t status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  nsCString discardFailureId;
  if (!gfxInfo || NS_FAILED(gfxInfo->GetFeatureStatus(aFeature, discardFailureId, &status))) {
    return false;
  }
  return status == nsIGfxInfo::FEATURE_STATUS_OK;
};

bool
AndroidDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                       DecoderDoctorDiagnostics* aDiagnostics) const
{
  if (!AndroidBridge::Bridge() ||
      AndroidBridge::Bridge()->GetAPIVersion() < 16) {
    return false;
  }

  if (aMimeType.EqualsLiteral("video/mp4") ||
      aMimeType.EqualsLiteral("video/avc")) {
    return true;
  }

  // When checking "audio/x-wav", CreateDecoder can cause a JNI ERROR by
  // Accessing a stale local reference leading to a SIGSEGV crash.
  // To avoid this we check for wav types here.
  if (aMimeType.EqualsLiteral("audio/x-wav") ||
      aMimeType.EqualsLiteral("audio/wave; codecs=1") ||
      aMimeType.EqualsLiteral("audio/wave; codecs=6") ||
      aMimeType.EqualsLiteral("audio/wave; codecs=7") ||
      aMimeType.EqualsLiteral("audio/wave; codecs=65534")) {
    return false;
  }

  if ((VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8) &&
       !GetFeatureStatus(nsIGfxInfo::FEATURE_VP8_HW_DECODE)) ||
      (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9) &&
       !GetFeatureStatus(nsIGfxInfo::FEATURE_VP9_HW_DECODE))) {
    return false;
  }

  return java::HardwareCodecCapabilityUtils::FindDecoderCodecInfoForMimeType(
      nsCString(TranslateMimeType(aMimeType)));
}

already_AddRefed<MediaDataDecoder>
AndroidDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  MediaFormat::LocalRef format;

  const VideoInfo& config = aParams.VideoConfig();
  NS_ENSURE_SUCCESS(MediaFormat::CreateVideoFormat(
      TranslateMimeType(config.mMimeType),
      config.mDisplay.width,
      config.mDisplay.height,
      &format), nullptr);

  RefPtr<MediaDataDecoder> decoder = MediaPrefs::PDMAndroidRemoteCodecEnabled() ?
      RemoteDataDecoder::CreateVideoDecoder(config,
                                            format,
                                            aParams.mCallback,
                                            aParams.mImageContainer) :
      MediaCodecDataDecoder::CreateVideoDecoder(config,
                                                format,
                                                aParams.mCallback,
                                                aParams.mImageContainer);

  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
AndroidDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  const AudioInfo& config = aParams.AudioConfig();
  MOZ_ASSERT(config.mBitDepth == 16, "We only handle 16-bit audio!");

  MediaFormat::LocalRef format;

  LOG("CreateAudioFormat with mimeType=%s, mRate=%d, channels=%d",
      config.mMimeType.Data(), config.mRate, config.mChannels);

  NS_ENSURE_SUCCESS(MediaFormat::CreateAudioFormat(
      config.mMimeType,
      config.mRate,
      config.mChannels,
      &format), nullptr);

  RefPtr<MediaDataDecoder> decoder = MediaPrefs::PDMAndroidRemoteCodecEnabled() ?
      RemoteDataDecoder::CreateAudioDecoder(config, format, aParams.mCallback) :
      MediaCodecDataDecoder::CreateAudioDecoder(config, format, aParams.mCallback);

  return decoder.forget();
}

PlatformDecoderModule::ConversionRequired
AndroidDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo()) {
    return ConversionRequired::kNeedAnnexB;
  }
  return ConversionRequired::kNeedNone;
}

} // mozilla
