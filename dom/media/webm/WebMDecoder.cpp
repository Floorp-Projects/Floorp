/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebMDecoder.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#ifdef MOZ_AV1
#include "AOMDecoder.h"
#endif
#include "MediaContainerType.h"
#include "PDMFactory.h"
#include "VideoUtils.h"

namespace mozilla {

/* static */
bool
WebMDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (!Preferences::GetBool("media.webm.enabled")) {
    return false;
  }

  bool isVideo = aContainerType.Type() == MEDIAMIMETYPE("video/webm");
  if (aContainerType.Type() != MEDIAMIMETYPE("audio/webm") && !isVideo) {
    return false;
  }

  const MediaCodecs& codecs = aContainerType.ExtendedType().Codecs();
  if (codecs.IsEmpty()) {
    // WebM guarantees that the only codecs it contained are vp8, vp9, opus or vorbis.
    return true;
  }
  // Verify that all the codecs specified are ones that we expect that
  // we can play.
  for (const auto& codec : codecs.Range()) {
    if (codec.EqualsLiteral("opus") || codec.EqualsLiteral("vorbis")) {
      continue;
    }
    // Note: Only accept VP8/VP9 in a video container type, not in an audio
    // container type.

    if (isVideo) {
      UniquePtr<TrackInfo> trackInfo;
      if (IsVP9CodecString(codec))  {
        trackInfo = CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("video/vp9"), aContainerType);
      } else if (IsVP8CodecString(codec)) {
        trackInfo = CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("video/vp8"), aContainerType);
      }
      // If it is vp8 or vp9, check the bit depth.
      if (trackInfo) {
        uint8_t profile = 0;
        uint8_t level = 0;
        uint8_t bitDepth = 0;
        if (ExtractVPXCodecDetails(codec, profile, level, bitDepth)) {
          trackInfo->GetAsVideoInfo()->mBitDepth = bitDepth;

          // Verify that we have a PDM that supports this bit depth.
          RefPtr<PDMFactory> platform = new PDMFactory();
          if (!platform->Supports(*trackInfo, nullptr)) {
            return false;
          }
        }
        continue;
      }
    }
#ifdef MOZ_AV1
    if (isVideo && StaticPrefs::MediaAv1Enabled() &&
        AOMDecoder::IsSupportedCodec(codec)) {
      continue;
    }
#endif
    // Some unsupported codec.
    return false;
  }
  return true;
}

} // namespace mozilla

