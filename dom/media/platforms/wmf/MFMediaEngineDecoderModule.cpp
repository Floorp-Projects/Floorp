/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineDecoderModule.h"

#include "VideoUtils.h"
#include "mozilla/MFMediaEngineParent.h"
#include "mozilla/MFMediaEngineUtils.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

#define LOG(msg, ...) \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

/* static */
void MFMediaEngineDecoderModule::Init() {
  // TODO : Init any thing that media engine would need. Implement this when we
  // start implementing media engine in following patches.
}

/* static */
already_AddRefed<PlatformDecoderModule> MFMediaEngineDecoderModule::Create() {
  RefPtr<MFMediaEngineDecoderModule> module = new MFMediaEngineDecoderModule();
  return module.forget();
}

already_AddRefed<MediaDataDecoder>
MFMediaEngineDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (!aParams.mMediaEngineId ||
      !StaticPrefs::media_wmf_media_engine_enabled()) {
    return nullptr;
  }
  RefPtr<MFMediaEngineParent> mediaEngine =
      MFMediaEngineParent::GetMediaEngineById(*aParams.mMediaEngineId);
  if (!mediaEngine) {
    LOG("Can't find media engine %" PRIu64 " for video decoder",
        *aParams.mMediaEngineId);
    return nullptr;
  }
  LOG("MFMediaEngineDecoderModule, CreateVideoDecoder");
  RefPtr<MediaDataDecoder> decoder = mediaEngine->GetMediaEngineStream(
      TrackInfo::TrackType::kVideoTrack, aParams);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
MFMediaEngineDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  if (!aParams.mMediaEngineId ||
      !StaticPrefs::media_wmf_media_engine_enabled()) {
    return nullptr;
  }
  RefPtr<MFMediaEngineParent> mediaEngine =
      MFMediaEngineParent::GetMediaEngineById(*aParams.mMediaEngineId);
  if (!mediaEngine) {
    LOG("Can't find media engine %" PRIu64 " for audio decoder",
        *aParams.mMediaEngineId);
    return nullptr;
  }
  LOG("MFMediaEngineDecoderModule, CreateAudioDecoder");
  RefPtr<MediaDataDecoder> decoder = mediaEngine->GetMediaEngineStream(
      TrackInfo::TrackType::kAudioTrack, aParams);
  return decoder.forget();
}

media::DecodeSupportSet MFMediaEngineDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return media::DecodeSupport::Unsupported;
  }
  return SupportInternal(SupportDecoderParams(*trackInfo), aDiagnostics);
}

media::DecodeSupportSet MFMediaEngineDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  if (!aParams.mMediaEngineId) {
    return media::DecodeSupport::Unsupported;
  }
  return SupportInternal(aParams, aDiagnostics);
}

media::DecodeSupportSet MFMediaEngineDecoderModule::SupportInternal(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  if (!StaticPrefs::media_wmf_media_engine_enabled()) {
    return media::DecodeSupport::Unsupported;
  }
  // TODO : ask media engine or simply ask WMFDecoderModule? I guess the
  // internal implementation of MFMediaEngine should be using MFT, so they
  // should be the same in term of support types. Implement this when we
  // start implementing media engine in following patches.
  return media::DecodeSupport::SoftwareDecode;
}

#undef LOG

}  // namespace mozilla
