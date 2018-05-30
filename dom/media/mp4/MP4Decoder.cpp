/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Decoder.h"
#include "MediaContainerType.h"
#include "MP4Demuxer.h"
#include "nsMimeTypes.h"
#include "VideoUtils.h"
#include "PDMFactory.h"
#include "mozilla/StaticPrefs.h"

namespace mozilla {

static bool
IsWhitelistedH264Codec(const nsAString& aCodec)
{
  int16_t profile = 0, level = 0;

  if (!ExtractH264CodecDetails(aCodec, profile, level)) {
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
  return level >= H264_LEVEL_1 &&
         level <= H264_LEVEL_5_2 &&
         (profile == H264_PROFILE_BASE ||
          profile == H264_PROFILE_MAIN ||
          profile == H264_PROFILE_EXTENDED ||
          profile == H264_PROFILE_HIGH);
}

/* static */
bool
MP4Decoder::IsSupportedTypeWithoutDiagnostics(
  const MediaContainerType& aContainerType)
{
  return IsSupportedType(aContainerType, nullptr);
}

/* static */
bool
MP4Decoder::IsSupportedType(const MediaContainerType& aType,
                            DecoderDoctorDiagnostics* aDiagnostics)
{
  if (!IsEnabled()) {
    return false;
  }

  // Whitelist MP4 types, so they explicitly match what we encounter on
  // the web, as opposed to what we use internally (i.e. what our demuxers
  // etc output).
  const bool isAudio = aType.Type() == MEDIAMIMETYPE("audio/mp4") ||
                       aType.Type() == MEDIAMIMETYPE("audio/x-m4a");
  const bool isVideo = aType.Type() == MEDIAMIMETYPE("video/mp4") ||
                       aType.Type() == MEDIAMIMETYPE("video/quicktime") ||
                       aType.Type() == MEDIAMIMETYPE("video/x-m4v");

  if (!isAudio && !isVideo) {
    return false;
  }

  nsTArray<UniquePtr<TrackInfo>> trackInfos;
  if (aType.ExtendedType().Codecs().IsEmpty()) {
    // No codecs specified. Assume H.264
    if (isAudio) {
      trackInfos.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("audio/mp4a-latm"), aType));
    } else {
      MOZ_ASSERT(isVideo);
      trackInfos.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("video/avc"), aType));
    }
  } else {
    // Verify that all the codecs specified are ones that we expect that
    // we can play.
    for (const auto& codec : aType.ExtendedType().Codecs().Range()) {
      if (IsAACCodecString(codec)) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/mp4a-latm"), aType));
        continue;
      }
      if (codec.EqualsLiteral("mp3")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/mpeg"), aType));
        continue;
      }
      if (codec.EqualsLiteral("opus")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/opus"), aType));
        continue;
      }
      if (codec.EqualsLiteral("flac")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/flac"), aType));
        continue;
      }
      if (IsVP9CodecString(codec)) {
        auto trackInfo =
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("video/vp9"), aType);
        uint8_t profile = 0;
        uint8_t level = 0;
        uint8_t bitDepth = 0;
        if (ExtractVPXCodecDetails(codec, profile, level, bitDepth)) {
          trackInfo->GetAsVideoInfo()->mBitDepth = bitDepth;
        }
        trackInfos.AppendElement(std::move(trackInfo));
        continue;
      }
      // Note: Only accept H.264 in a video content type, not in an audio
      // content type.
      if (IsWhitelistedH264Codec(codec) && isVideo) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("video/avc"), aType));
        continue;
      }
      // Some unsupported codec.
      return false;
    }
  }

  // Verify that we have a PDM that supports the whitelisted types.
  RefPtr<PDMFactory> platform = new PDMFactory();
  for (const auto& trackInfo : trackInfos) {
    if (!trackInfo || !platform->Supports(*trackInfo, aDiagnostics)) {
      return false;
    }
  }

  return true;
}

/* static */
bool
MP4Decoder::IsH264(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("video/mp4") ||
         aMimeType.EqualsLiteral("video/avc");
}

/* static */
bool
MP4Decoder::IsAAC(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/mp4a-latm");
}

/* static */
bool
MP4Decoder::IsEnabled()
{
  return StaticPrefs::mediaMp4Enabled();
}

} // namespace mozilla
