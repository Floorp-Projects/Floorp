/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#include "MediaInfo.h"
#include "OpusDecoder.h"
#include "RemoteDataDecoder.h"
#include "TheoraDecoder.h"
#include "VPXDecoder.h"
#include "VorbisDecoder.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/java/HardwareCodecCapabilityUtilsWrappers.h"
#include "nsIGfxInfo.h"
#include "nsPromiseFlatString.h"
#include "prlog.h"

#undef LOG
#define LOG(arg, ...)                                     \
  MOZ_LOG(                                                \
      sAndroidDecoderModuleLog, mozilla::LogLevel::Debug, \
      ("AndroidDecoderModule(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define SLOG(arg, ...)                                        \
  MOZ_LOG(sAndroidDecoderModuleLog, mozilla::LogLevel::Debug, \
          ("%s: " arg, __func__, ##__VA_ARGS__))

using namespace mozilla;
using media::TimeUnit;

namespace mozilla {

mozilla::LazyLogModule sAndroidDecoderModuleLog("AndroidDecoderModule");

const nsCString TranslateMimeType(const nsACString& aMimeType) {
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8)) {
    static constexpr auto vp8 = "video/x-vnd.on2.vp8"_ns;
    return vp8;
  } else if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9)) {
    static constexpr auto vp9 = "video/x-vnd.on2.vp9"_ns;
    return vp9;
  }
  return nsCString(aMimeType);
}

static bool GetFeatureStatus(int32_t aFeature) {
  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
  int32_t status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  nsCString discardFailureId;
  if (!gfxInfo || NS_FAILED(gfxInfo->GetFeatureStatus(
                      aFeature, discardFailureId, &status))) {
    return false;
  }
  return status == nsIGfxInfo::FEATURE_STATUS_OK;
};

AndroidDecoderModule::AndroidDecoderModule(CDMProxy* aProxy) {
  mProxy = static_cast<MediaDrmCDMProxy*>(aProxy);
}

StaticAutoPtr<nsTArray<nsCString>> AndroidDecoderModule::sSupportedMimeTypes;

bool AndroidDecoderModule::SupportsMimeType(const nsACString& aMimeType) {
  if (jni::GetAPIVersion() < 16) {
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
      aMimeType.EqualsLiteral("audio/wave; codecs=3") ||
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

  // Prefer the gecko decoder for opus and vorbis; stagefright crashes
  // on content demuxed from mp4.
  // Not all android devices support FLAC even when they say they do.
  if (OpusDataDecoder::IsOpus(aMimeType) ||
      VorbisDataDecoder::IsVorbis(aMimeType) ||
      aMimeType.EqualsLiteral("audio/flac")) {
    SLOG("Rejecting audio of type %s", aMimeType.Data());
    return false;
  }

  // Prefer the gecko decoder for Theora.
  // Not all android devices support Theora even when they say they do.
  if (TheoraDecoder::IsTheora(aMimeType)) {
    SLOG("Rejecting video of type %s", aMimeType.Data());
    return false;
  }

  if (aMimeType.EqualsLiteral("audio/mpeg") &&
      StaticPrefs::media_ffvpx_mp3_enabled()) {
    // Prefer the ffvpx mp3 software decoder if available.
    return false;
  }

  if (sSupportedMimeTypes) {
    return sSupportedMimeTypes->Contains(TranslateMimeType(aMimeType));
  }

  return false;
}

nsTArray<nsCString> AndroidDecoderModule::GetSupportedMimeTypes() {
  mozilla::jni::ObjectArray::LocalRef supportedTypes = mozilla::java::
      HardwareCodecCapabilityUtils::GetDecoderSupportedMimeTypes();

  nsTArray<nsCString> st = nsTArray<nsCString>();
  for (size_t i = 0; i < supportedTypes->Length(); i++) {
    st.AppendElement(
        jni::String::LocalRef(supportedTypes->GetElement(i))->ToCString());
  }

  return st;
}

void AndroidDecoderModule::SetSupportedMimeTypes(
    nsTArray<nsCString>&& aSupportedTypes) {
  if (!sSupportedMimeTypes) {
    sSupportedMimeTypes = new nsTArray<nsCString>(std::move(aSupportedTypes));
    ClearOnShutdown(&sSupportedMimeTypes);
  }
}

bool AndroidDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return AndroidDecoderModule::SupportsMimeType(aMimeType);
}

already_AddRefed<MediaDataDecoder> AndroidDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  // Temporary - forces use of VPXDecoder when alpha is present.
  // Bug 1263836 will handle alpha scenario once implemented. It will shift
  // the check for alpha to PDMFactory but not itself remove the need for a
  // check.
  if (aParams.VideoConfig().HasAlpha()) {
    return nullptr;
  }

  nsString drmStubId;
  if (mProxy) {
    drmStubId = mProxy->GetMediaDrmStubId();
  }

  RefPtr<MediaDataDecoder> decoder =
      RemoteDataDecoder::CreateVideoDecoder(aParams, drmStubId, mProxy);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> AndroidDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  const AudioInfo& config = aParams.AudioConfig();
  if (config.mBitDepth != 16) {
    // We only handle 16-bit audio.
    return nullptr;
  }

  LOG("CreateAudioFormat with mimeType=%s, mRate=%d, channels=%d",
      config.mMimeType.Data(), config.mRate, config.mChannels);

  nsString drmStubId;
  if (mProxy) {
    drmStubId = mProxy->GetMediaDrmStubId();
  }
  RefPtr<MediaDataDecoder> decoder =
      RemoteDataDecoder::CreateAudioDecoder(aParams, drmStubId, mProxy);
  return decoder.forget();
}

/* static */
already_AddRefed<PlatformDecoderModule> AndroidDecoderModule::Create(
    CDMProxy* aProxy) {
  return MakeAndAddRef<AndroidDecoderModule>(aProxy);
}

}  // namespace mozilla
