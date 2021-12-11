/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AgnosticDecoderModule.h"

#include "OpusDecoder.h"
#include "TheoraDecoder.h"
#include "VPXDecoder.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"
#include "VideoUtils.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#  include "DAV1DDecoder.h"
#endif

namespace mozilla {

enum class DecoderType {
#ifdef MOZ_AV1
  AV1,
#endif
  Opus,
  Theora,
  Vorbis,
  VPX,
  Wave,
};

static bool IsAvailableInDefault(DecoderType type) {
  switch (type) {
#ifdef MOZ_AV1
    case DecoderType::AV1:
      return StaticPrefs::media_av1_enabled();
#endif
    case DecoderType::Opus:
    case DecoderType::Theora:
    case DecoderType::Vorbis:
    case DecoderType::VPX:
    case DecoderType::Wave:
      return true;
    default:
      return false;
  }
}

static bool IsAvailableInRdd(DecoderType type) {
  switch (type) {
#ifdef MOZ_AV1
    case DecoderType::AV1:
      return StaticPrefs::media_av1_enabled();
#endif
    case DecoderType::Opus:
      return StaticPrefs::media_rdd_opus_enabled();
    case DecoderType::Theora:
      return StaticPrefs::media_rdd_theora_enabled();
    case DecoderType::Vorbis:
#if defined(__MINGW32__)
      // If this is a MinGW build we need to force AgnosticDecoderModule to
      // handle the decision to support Vorbis decoding (instead of
      // RDD/RemoteDecoderModule) because of Bug 1597408 (Vorbis decoding on
      // RDD causing sandboxing failure on MinGW-clang).  Typically this
      // would be dealt with using defines in StaticPrefList.yaml, but we
      // must handle it here because of Bug 1598426 (the __MINGW32__ define
      // isn't supported in StaticPrefList.yaml).
      return false;
#else
      return StaticPrefs::media_rdd_vorbis_enabled();
#endif
    case DecoderType::VPX:
      return StaticPrefs::media_rdd_vpx_enabled();
    case DecoderType::Wave:
      return StaticPrefs::media_rdd_wav_enabled();
    default:
      return false;
  }
}

// Checks if decoder is available in the current process
static bool IsAvailable(DecoderType type) {
  return XRE_IsRDDProcess() ? IsAvailableInRdd(type)
                            : IsAvailableInDefault(type);
}

bool AgnosticDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return false;
  }
  return Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
}

bool AgnosticDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  const auto& trackInfo = aParams.mConfig;
  const nsACString& mimeType = trackInfo.mMimeType;

  bool supports =
#ifdef MOZ_AV1
      // We remove support for decoding AV1 here if RDD is enabled so that
      // decoding on the content process doesn't accidentally happen in case
      // something goes wrong with launching the RDD process.
      (AOMDecoder::IsAV1(mimeType) && IsAvailable(DecoderType::AV1)) ||
#endif
      (VPXDecoder::IsVPX(mimeType) && IsAvailable(DecoderType::VPX)) ||
      (TheoraDecoder::IsTheora(mimeType) && IsAvailable(DecoderType::Theora)) ||
      (VorbisDataDecoder::IsVorbis(mimeType) &&
       IsAvailable(DecoderType::Vorbis)) ||
      (WaveDataDecoder::IsWave(mimeType) && IsAvailable(DecoderType::Wave)) ||
      (OpusDataDecoder::IsOpus(mimeType) && IsAvailable(DecoderType::Opus));
  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Agnostic decoder %s requested type",
           supports ? "supports" : "rejects"));
  return supports;
}

already_AddRefed<MediaDataDecoder> AgnosticDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (!Supports(SupportDecoderParams(aParams), nullptr /* diagnostic */)) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> m;

  if (VPXDecoder::IsVPX(aParams.mConfig.mMimeType)) {
    m = new VPXDecoder(aParams);
  }
#ifdef MOZ_AV1
  // We remove support for decoding AV1 here if RDD is enabled so that
  // decoding on the content process doesn't accidentally happen in case
  // something goes wrong with launching the RDD process.
  if (StaticPrefs::media_av1_enabled() &&
      (!StaticPrefs::media_rdd_process_enabled() || XRE_IsRDDProcess()) &&
      AOMDecoder::IsAV1(aParams.mConfig.mMimeType)) {
    if (StaticPrefs::media_av1_use_dav1d()) {
      m = new DAV1DDecoder(aParams);
    } else {
      m = new AOMDecoder(aParams);
    }
  }
#endif
  else if (TheoraDecoder::IsTheora(aParams.mConfig.mMimeType)) {
    m = new TheoraDecoder(aParams);
  }

  return m.forget();
}

already_AddRefed<MediaDataDecoder> AgnosticDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  if (!Supports(SupportDecoderParams(aParams), nullptr /* diagnostic */)) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> m;

  const TrackInfo& config = aParams.mConfig;
  if (VorbisDataDecoder::IsVorbis(config.mMimeType)) {
    m = new VorbisDataDecoder(aParams);
  } else if (OpusDataDecoder::IsOpus(config.mMimeType)) {
    m = new OpusDataDecoder(aParams);
  } else if (WaveDataDecoder::IsWave(config.mMimeType)) {
    m = new WaveDataDecoder(aParams);
  }

  return m.forget();
}

/* static */
already_AddRefed<PlatformDecoderModule> AgnosticDecoderModule::Create() {
  RefPtr<PlatformDecoderModule> pdm = new AgnosticDecoderModule();
  return pdm.forget();
}

}  // namespace mozilla
