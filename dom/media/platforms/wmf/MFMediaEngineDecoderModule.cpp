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

/* static */
bool MFMediaEngineDecoderModule::SupportsConfig(const TrackInfo& aConfig) {
  RefPtr<MFMediaEngineDecoderModule> module = new MFMediaEngineDecoderModule();
  return module->SupportInternal(SupportDecoderParams(aConfig), nullptr) !=
         media::DecodeSupport::Unsupported;
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
  bool supports = false;
  WMFStreamType type = GetStreamTypeFromMimeType(aParams.MimeType());
  if (type != WMFStreamType::Unknown) {
    supports = CanCreateMFTDecoder(type);
  }
  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("MFMediaEngine decoder %s requested type '%s'",
           supports ? "supports" : "rejects", aParams.MimeType().get()));
  return supports ? media::DecodeSupport::SoftwareDecode
                  : media::DecodeSupport::Unsupported;
}

static bool CreateMFTDecoderOnMTA(const WMFStreamType& aType) {
  RefPtr<MFTDecoder> decoder = new MFTDecoder();
  switch (aType) {
    case WMFStreamType::MP3:
      return SUCCEEDED(decoder->Create(CLSID_CMP3DecMediaObject));
    case WMFStreamType::AAC:
      return SUCCEEDED(decoder->Create(CLSID_CMSAACDecMFT));
    // Opus and vorbis are supported via extension.
    // https://www.microsoft.com/en-us/p/web-media-extensions/9n5tdp8vcmhs
    case WMFStreamType::OPUS:
      return SUCCEEDED(decoder->Create(CLSID_MSOpusDecoder));
    case WMFStreamType::VORBIS:
      return SUCCEEDED(decoder->Create(
          MFT_CATEGORY_AUDIO_DECODER, MFAudioFormat_Vorbis, MFAudioFormat_PCM));
    case WMFStreamType::H264:
    case WMFStreamType::VP8:
    case WMFStreamType::VP9:
    case WMFStreamType::AV1:
      // TODO : do a real check later when starting implement video output. For
      // testing purpose, we allow all these formats temporarily.
      return true;
    default:
      return false;
  }
}

bool MFMediaEngineDecoderModule::CanCreateMFTDecoder(
    const WMFStreamType& aType) const {
  // TODO : caching the result to prevent performing on MTA thread everytime.
  bool canCreateDecoder = false;
  mozilla::mscom::EnsureMTA(
      [&]() { canCreateDecoder = CreateMFTDecoderOnMTA(aType); });
  return canCreateDecoder;
}

#undef LOG

}  // namespace mozilla
