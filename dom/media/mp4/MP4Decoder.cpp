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

static bool IsTypeValid(const MediaContainerType& aType)
{
  // Whitelist MP4 types, so they explicitly match what we encounter on
  // the web, as opposed to what we use internally (i.e. what our demuxers
  // etc output).
  return aType.Type() == MEDIAMIMETYPE("audio/mp4") ||
         aType.Type() == MEDIAMIMETYPE("audio/x-m4a") ||
         aType.Type() == MEDIAMIMETYPE("video/mp4") ||
         aType.Type() == MEDIAMIMETYPE("video/quicktime") ||
         aType.Type() == MEDIAMIMETYPE("video/x-m4v");
}

/* statis */ nsTArray<UniquePtr<TrackInfo>>
MP4Decoder::GetTracksInfo(const MediaContainerType& aType, MediaResult& aError)
{
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
          NS_LITERAL_CSTRING("audio/mp4a-latm"), aType));
      continue;
    }
    if (codec.EqualsLiteral("mp3")) {
      tracks.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("audio/mpeg"), aType));
      continue;
    }
    if (codec.EqualsLiteral("opus") || codec.EqualsLiteral("flac")) {
      tracks.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("audio/") + NS_ConvertUTF16toUTF8(codec), aType));
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
      tracks.AppendElement(std::move(trackInfo));
      continue;
    }
    if (isVideo && IsWhitelistedH264Codec(codec)) {
      tracks.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("video/avc"), aType));
      continue;
    }
    // Unknown codec
    aError =
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Unknown codec:%s",
                                NS_ConvertUTF16toUTF8(codec).get()));
  }
  return tracks;
}

/* static */
bool
MP4Decoder::IsSupportedType(const MediaContainerType& aType,
                            DecoderDoctorDiagnostics* aDiagnostics)
{
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
          NS_LITERAL_CSTRING("audio/mp4a-latm"), aType));
    } else {
      tracks.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("video/avc"), aType));
    }
  }

  // Verify that we have a PDM that supports the whitelisted types.
  RefPtr<PDMFactory> platform = new PDMFactory();
  for (const auto& track : tracks) {
    if (!track || !platform->Supports(*track, aDiagnostics)) {
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

/* static */ nsTArray<UniquePtr<TrackInfo>>
MP4Decoder::GetTracksInfo(const MediaContainerType& aType)
{
  MediaResult rv = NS_OK;
  return GetTracksInfo(aType, rv);
}

} // namespace mozilla
