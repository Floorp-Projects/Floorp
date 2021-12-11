/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Decoder.h"
#include "H264.h"
#include "MP4Demuxer.h"
#include "MediaContainerType.h"
#include "PDMFactory.h"
#include "PlatformDecoderModule.h"
#include "VideoUtils.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/Tools.h"
#include "nsMimeTypes.h"

namespace mozilla {

static bool IsWhitelistedH264Codec(const nsAString& aCodec) {
  uint8_t profile = 0, constraint = 0, level = 0;

  if (!ExtractH264CodecDetails(aCodec, profile, constraint, level)) {
    return false;
  }

  // Just assume what we can play on all platforms the codecs/formats that
  // WMF can play, since we don't have documentation about what other
  // platforms can play... According to the WMF documentation:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815%28v=vs.85%29.aspx
  // "The Media Foundation H.264 video decoder is a Media Foundation Transform
  // that supports decoding of Baseline, Main, and High profiles, up to level
  // 5.1.". We extend the limit to level 5.2, relying on the decoder to handle
  // any potential errors, the level limit being rather arbitrary.
  // We also report that we can play Extended profile, as there are
  // bitstreams that are Extended compliant that are also Baseline compliant.
  return level >= H264_LEVEL_1 && level <= H264_LEVEL_5_2 &&
         (profile == H264_PROFILE_BASE || profile == H264_PROFILE_MAIN ||
          profile == H264_PROFILE_EXTENDED || profile == H264_PROFILE_HIGH);
}

static bool IsTypeValid(const MediaContainerType& aType) {
  // Whitelist MP4 types, so they explicitly match what we encounter on
  // the web, as opposed to what we use internally (i.e. what our demuxers
  // etc output).
  return aType.Type() == MEDIAMIMETYPE("audio/mp4") ||
         aType.Type() == MEDIAMIMETYPE("audio/x-m4a") ||
         aType.Type() == MEDIAMIMETYPE("video/mp4") ||
         aType.Type() == MEDIAMIMETYPE("video/quicktime") ||
         aType.Type() == MEDIAMIMETYPE("video/x-m4v");
}

/* statis */
nsTArray<UniquePtr<TrackInfo>> MP4Decoder::GetTracksInfo(
    const MediaContainerType& aType, MediaResult& aError) {
  nsTArray<UniquePtr<TrackInfo>> tracks;

  if (!IsTypeValid(aType)) {
    aError = MediaResult(
        NS_ERROR_DOM_MEDIA_FATAL_ERR,
        RESULT_DETAIL("Invalid type:%s", aType.Type().AsString().get()));
    return tracks;
  }

  aError = NS_OK;

  const MediaCodecs& codecs = aType.ExtendedType().Codecs();
  if (codecs.IsEmpty()) {
    return tracks;
  }

  const bool isVideo = aType.Type() == MEDIAMIMETYPE("video/mp4") ||
                       aType.Type() == MEDIAMIMETYPE("video/quicktime") ||
                       aType.Type() == MEDIAMIMETYPE("video/x-m4v");

  for (const auto& codec : codecs.Range()) {
    if (IsAACCodecString(codec)) {
      tracks.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "audio/mp4a-latm"_ns, aType));
      continue;
    }
    if (codec.EqualsLiteral("mp3")) {
      tracks.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "audio/mpeg"_ns, aType));
      continue;
    }
    if (codec.EqualsLiteral("opus") || codec.EqualsLiteral("flac")) {
      tracks.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "audio/"_ns + NS_ConvertUTF16toUTF8(codec), aType));
      continue;
    }
    if (IsVP9CodecString(codec)) {
      auto trackInfo =
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "video/vp9"_ns, aType);
      uint8_t profile = 0;
      uint8_t level = 0;
      uint8_t bitDepth = 0;
      if (ExtractVPXCodecDetails(codec, profile, level, bitDepth)) {
        trackInfo->GetAsVideoInfo()->mColorDepth =
            gfx::ColorDepthForBitDepth(bitDepth);
      }
      tracks.AppendElement(std::move(trackInfo));
      continue;
    }
#ifdef MOZ_AV1
    if (StaticPrefs::media_av1_enabled() && IsAV1CodecString(codec)) {
      tracks.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "video/av1"_ns, aType));
      continue;
    }
#endif
    if (isVideo && IsWhitelistedH264Codec(codec)) {
      auto trackInfo =
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "video/avc"_ns, aType);
      uint8_t profile = 0, constraint = 0, level = 0;
      MOZ_ALWAYS_TRUE(
          ExtractH264CodecDetails(codec, profile, constraint, level));
      uint32_t width = aType.ExtendedType().GetWidth().refOr(1280);
      uint32_t height = aType.ExtendedType().GetHeight().refOr(720);
      trackInfo->GetAsVideoInfo()->mExtraData =
          H264::CreateExtraData(profile, constraint, level, {width, height});
      tracks.AppendElement(std::move(trackInfo));
      continue;
    }
    // Unknown codec
    aError = MediaResult(
        NS_ERROR_DOM_MEDIA_FATAL_ERR,
        RESULT_DETAIL("Unknown codec:%s", NS_ConvertUTF16toUTF8(codec).get()));
  }
  return tracks;
}

/* static */
bool MP4Decoder::IsSupportedType(const MediaContainerType& aType,
                                 DecoderDoctorDiagnostics* aDiagnostics) {
  if (!IsEnabled()) {
    return false;
  }

  MediaResult rv = NS_OK;
  auto tracks = GetTracksInfo(aType, rv);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (tracks.IsEmpty()) {
    // No codecs specified. Assume H.264 or AAC
    if (aType.Type() == MEDIAMIMETYPE("audio/mp4") ||
        aType.Type() == MEDIAMIMETYPE("audio/x-m4a")) {
      tracks.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "audio/mp4a-latm"_ns, aType));
    } else {
      tracks.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
              "video/avc"_ns, aType));
    }
  }

  // Verify that we have a PDM that supports the whitelisted types.
  RefPtr<PDMFactory> platform = new PDMFactory();
  for (const auto& track : tracks) {
    if (!track ||
        !platform->Supports(SupportDecoderParams(*track), aDiagnostics)) {
      return false;
    }
  }

  return true;
}

/* static */
bool MP4Decoder::IsH264(const nsACString& aMimeType) {
  return aMimeType.EqualsLiteral("video/mp4") ||
         aMimeType.EqualsLiteral("video/avc");
}

/* static */
bool MP4Decoder::IsAAC(const nsACString& aMimeType) {
  return aMimeType.EqualsLiteral("audio/mp4a-latm");
}

/* static */
bool MP4Decoder::IsEnabled() { return StaticPrefs::media_mp4_enabled(); }

/* static */
nsTArray<UniquePtr<TrackInfo>> MP4Decoder::GetTracksInfo(
    const MediaContainerType& aType) {
  MediaResult rv = NS_OK;
  return GetTracksInfo(aType, rv);
}

}  // namespace mozilla
