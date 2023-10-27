/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <array>

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "MediaCodecsSupport.h"
#include "MP4Decoder.h"
#include "OpusDecoder.h"
#include "PlatformDecoderModule.h"
#include "TheoraDecoder.h"
#include "VPXDecoder.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsTHashMap.h"

using MediaCodecsSupport = mozilla::media::MediaCodecsSupport;

namespace mozilla::media {

static StaticAutoPtr<MCSInfo> sInstance;
static StaticMutex sInitMutex;
static StaticMutex sUpdateMutex;

#define CODEC_SUPPORT_LOG(msg, ...) \
  MOZ_LOG(sPDMLog, LogLevel::Debug, ("MediaCodecsSupport, " msg, ##__VA_ARGS__))

void MCSInfo::AddSupport(const MediaCodecsSupported& aSupport) {
  StaticMutexAutoLock lock(sUpdateMutex);
  MCSInfo* instance = GetInstance();
  if (!instance) {
    CODEC_SUPPORT_LOG("Can't add codec support without a MCSInfo instance!");
    return;
  }
  instance->mSupport += aSupport;
}

MediaCodecsSupported MCSInfo::GetSupport() {
  StaticMutexAutoLock lock(sUpdateMutex);
  MCSInfo* instance = GetInstance();
  if (!instance) {
    CODEC_SUPPORT_LOG("Can't get codec support without a MCSInfo instance!");
    return MediaCodecsSupported{};
  }
  return instance->mSupport;
}

void MCSInfo::ResetSupport() {
  StaticMutexAutoLock lock(sUpdateMutex);
  MCSInfo* instance = GetInstance();
  if (!instance) {
    CODEC_SUPPORT_LOG("Can't reset codec support without a MCSInfo instance!");
    return;
  }
  instance->mSupport.clear();
}

DecodeSupportSet MCSInfo::GetDecodeSupportSet(
    const MediaCodec& aCodec, const MediaCodecsSupported& aSupported) {
  DecodeSupportSet support;
  const auto supportInfo = GetCodecDefinition(aCodec);
  if (aSupported.contains(supportInfo.swDecodeSupport)) {
    support += DecodeSupport::SoftwareDecode;
  }
  if (aSupported.contains(supportInfo.hwDecodeSupport)) {
    support += DecodeSupport::HardwareDecode;
  }
  return support;
}

MediaCodecsSupported MCSInfo::GetDecodeMediaCodecsSupported(
    const MediaCodec& aCodec, const DecodeSupportSet& aSupportSet) {
  MediaCodecsSupported support;
  const auto supportInfo = GetCodecDefinition(aCodec);
  if (aSupportSet.contains(DecodeSupport::SoftwareDecode)) {
    support += supportInfo.swDecodeSupport;
  }
  if (aSupportSet.contains(DecodeSupport::HardwareDecode)) {
    support += supportInfo.hwDecodeSupport;
  }
  if (aSupportSet.contains(DecodeSupport::UnsureDueToLackOfExtension)) {
    support += supportInfo.lackOfHWExtenstion;
  }
  return support;
}

bool MCSInfo::SupportsSoftwareDecode(
    const MediaCodecsSupported& aSupportedCodecs, const MediaCodec& aCodec) {
  return (
      aSupportedCodecs.contains(GetCodecDefinition(aCodec).swDecodeSupport));
}

bool MCSInfo::SupportsHardwareDecode(
    const MediaCodecsSupported& aSupportedCodecs, const MediaCodec& aCodec) {
  return (
      aSupportedCodecs.contains(GetCodecDefinition(aCodec).hwDecodeSupport));
}

void MCSInfo::GetMediaCodecsSupportedString(
    nsCString& aSupportString, const MediaCodecsSupported& aSupportedCodecs) {
  CodecDefinition supportInfo;
  aSupportString = ""_ns;
  MCSInfo* instance = GetInstance();
  if (!instance) {
    CODEC_SUPPORT_LOG("Can't get codec support string w/o a MCSInfo instance!");
    return;
  }
  for (const auto& it : GetAllCodecDefinitions()) {
    if (it.codec == MediaCodec::SENTINEL) {
      break;
    }
    if (!instance->mHashTableCodec->Get(it.codec, &supportInfo)) {
      CODEC_SUPPORT_LOG("Can't find codec for MediaCodecsSupported enum: %d",
                        static_cast<int>(it.codec));
      continue;
    }
    aSupportString.Append(supportInfo.commonName);
    bool foundSupport = false;
    if (aSupportedCodecs.contains(it.swDecodeSupport)) {
      aSupportString.Append(" SW"_ns);
      foundSupport = true;
    }
    if (aSupportedCodecs.contains(it.hwDecodeSupport)) {
      aSupportString.Append(" HW"_ns);
      foundSupport = true;
    }
    if (aSupportedCodecs.contains(it.lackOfHWExtenstion)) {
      aSupportString.Append(" LACK_OF_EXTENSION"_ns);
      foundSupport = true;
    }
    if (!foundSupport) {
      aSupportString.Append(" NONE"_ns);
    }
    aSupportString.Append("\n"_ns);
  }
  // Remove any trailing newline characters
  if (!aSupportString.IsEmpty()) {
    aSupportString.Truncate(aSupportString.Length() - 1);
  }
}

MCSInfo* MCSInfo::GetInstance() {
  StaticMutexAutoLock lock(sInitMutex);
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    CODEC_SUPPORT_LOG("In XPCOM shutdown - not returning MCSInfo instance!");
    return nullptr;
  }
  if (!sInstance) {
    sInstance = new MCSInfo();
  }
  return sInstance.get();
}

MCSInfo::MCSInfo() {
  // Initialize hash tables
  mHashTableMCS.reset(new nsTHashMap<MediaCodecsSupport, CodecDefinition>());
  mHashTableCodec.reset(new nsTHashMap<MediaCodec, CodecDefinition>());

  for (const auto& it : GetAllCodecDefinitions()) {
    // Insert MediaCodecsSupport values as keys
    mHashTableMCS->InsertOrUpdate(it.swDecodeSupport, it);
    mHashTableMCS->InsertOrUpdate(it.hwDecodeSupport, it);
    // Insert codec enum values as keys
    mHashTableCodec->InsertOrUpdate(it.codec, it);
  }

  GetMainThreadSerialEventTarget()->Dispatch(
      NS_NewRunnableFunction("MCSInfo::MCSInfo", [&] {
        // Ensure hash tables freed on shutdown
        RunOnShutdown(
            [&] {
              mHashTableMCS.reset();
              mHashTableString.reset();
              mHashTableCodec.reset();
              sInstance = nullptr;
            },
            ShutdownPhase::XPCOMShutdown);
      }));
}

CodecDefinition MCSInfo::GetCodecDefinition(const MediaCodec& aCodec) {
  CodecDefinition info;
  MCSInfo* instance = GetInstance();
  if (!instance) {
    CODEC_SUPPORT_LOG("Can't get codec definition without a MCSInfo instance!");
  } else if (!instance->mHashTableCodec->Get(aCodec, &info)) {
    CODEC_SUPPORT_LOG("Could not find codec definition for codec enum: %d!",
                      static_cast<int>(aCodec));
  }
  return info;
}

MediaCodecsSupport MCSInfo::GetMediaCodecsSupportEnum(
    const MediaCodec& aCodec, const DecodeSupportSet& aSupport) {
  if (aSupport.isEmpty()) {
    return MediaCodecsSupport{};
  }
  const CodecDefinition cd = GetCodecDefinition(aCodec);
  if (aSupport.contains(DecodeSupport::SoftwareDecode)) {
    return cd.swDecodeSupport;
  }
  if (aSupport.contains(DecodeSupport::HardwareDecode)) {
    return cd.hwDecodeSupport;
  }
  return MediaCodecsSupport::SENTINEL;
}

MediaCodecSet MCSInfo::GetMediaCodecSetFromMimeTypes(
    const nsTArray<nsCString>& aCodecStrings) {
  MediaCodecSet support;
  for (const auto& ms : aCodecStrings) {
    const MediaCodec codec = MCSInfo::GetMediaCodecFromMimeType(ms);
    if (codec == MediaCodec::SENTINEL) {
      continue;
    }
    MOZ_ASSERT(codec < MediaCodec::SENTINEL);
    support += codec;
  }
  return support;
}

MediaCodec MCSInfo::GetMediaCodecFromMimeType(const nsACString& aMimeType) {
  // Video codecs
  if (MP4Decoder::IsH264(aMimeType)) {
    return MediaCodec::H264;
  }
  if (VPXDecoder::IsVP8(aMimeType)) {
    return MediaCodec::VP8;
  }
  if (VPXDecoder::IsVP9(aMimeType)) {
    return MediaCodec::VP9;
  }
  if (TheoraDecoder::IsTheora(aMimeType)) {
    return MediaCodec::Theora;
  }
  if (MP4Decoder::IsHEVC(aMimeType)) {
    return MediaCodec::HEVC;
  }
#ifdef MOZ_AV1
  if (AOMDecoder::IsAV1(aMimeType)) {
    return MediaCodec::AV1;
  }
  if (aMimeType.EqualsLiteral("video/av01")) {
    return MediaCodec::AV1;
  }
#endif
  // TODO: Should this be Android only?
#ifdef ANDROID
  if (aMimeType.EqualsLiteral("video/x-vnd.on2.vp8")) {
    return MediaCodec::VP8;
  }
  if (aMimeType.EqualsLiteral("video/x-vnd.on2.vp9")) {
    return MediaCodec::VP9;
  }
#endif
  // Audio codecs
  if (MP4Decoder::IsAAC(aMimeType)) {
    return MediaCodec::AAC;
  }
  if (VorbisDataDecoder::IsVorbis(aMimeType)) {
    return MediaCodec::Vorbis;
  }
  if (aMimeType.EqualsLiteral("audio/flac")) {
    return MediaCodec::FLAC;
  }
  if (WaveDataDecoder::IsWave(aMimeType)) {
    return MediaCodec::Wave;
  }
  if (OpusDataDecoder::IsOpus(aMimeType)) {
    return MediaCodec::Opus;
  }
  if (aMimeType.EqualsLiteral("audio/mpeg")) {
    return MediaCodec::MP3;
  }

  CODEC_SUPPORT_LOG("No specific codec enum for MIME type string: %s",
                    nsCString(aMimeType).get());
  return MediaCodec::SENTINEL;
}

std::array<CodecDefinition, 13> MCSInfo::GetAllCodecDefinitions() {
  static constexpr std::array<CodecDefinition, 13> codecDefinitions = {
      {{MediaCodec::H264, "H264", "video/avc",
        MediaCodecsSupport::H264SoftwareDecode,
        MediaCodecsSupport::H264HardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::VP9, "VP9", "video/vp9",
        MediaCodecsSupport::VP9SoftwareDecode,
        MediaCodecsSupport::VP9HardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::VP8, "VP8", "video/vp8",
        MediaCodecsSupport::VP8SoftwareDecode,
        MediaCodecsSupport::VP8HardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::AV1, "AV1", "video/av1",
        MediaCodecsSupport::AV1SoftwareDecode,
        MediaCodecsSupport::AV1HardwareDecode,
        MediaCodecsSupport::AV1LackOfExtension},
       {MediaCodec::HEVC, "HEVC", "video/hevc",
        MediaCodecsSupport::HEVCSoftwareDecode,
        MediaCodecsSupport::HEVCHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::Theora, "Theora", "video/theora",
        MediaCodecsSupport::TheoraSoftwareDecode,
        MediaCodecsSupport::TheoraHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::AAC, "AAC", "audio/mp4a-latm",
        MediaCodecsSupport::AACSoftwareDecode,
        MediaCodecsSupport::AACHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::MP3, "MP3", "audio/mpeg",
        MediaCodecsSupport::MP3SoftwareDecode,
        MediaCodecsSupport::MP3HardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::Opus, "Opus", "audio/opus",
        MediaCodecsSupport::OpusSoftwareDecode,
        MediaCodecsSupport::OpusHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::Vorbis, "Vorbis", "audio/vorbis",
        MediaCodecsSupport::VorbisSoftwareDecode,
        MediaCodecsSupport::VorbisHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::FLAC, "FLAC", "audio/flac",
        MediaCodecsSupport::FLACSoftwareDecode,
        MediaCodecsSupport::FLACHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::Wave, "Wave", "audio/x-wav",
        MediaCodecsSupport::WaveSoftwareDecode,
        MediaCodecsSupport::WaveHardwareDecode, MediaCodecsSupport::SENTINEL},
       {MediaCodec::SENTINEL, "Undefined codec name",
        "Undefined MIME type string", MediaCodecsSupport::SENTINEL,
        MediaCodecsSupport::SENTINEL}}};
  return codecDefinitions;
}
}  // namespace mozilla::media

#undef CODEC_SUPPORT_LOG
