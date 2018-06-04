/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OggDecoder.h"
#include "MediaContainerType.h"
#include "MediaDecoder.h"
#include "nsMimeTypes.h"
#include "mozilla/StaticPrefs.h"

namespace mozilla {

/* static */
bool
OggDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (!StaticPrefs::MediaOggEnabled()) {
    return false;
  }

  if (aContainerType.Type() != MEDIAMIMETYPE(AUDIO_OGG) &&
      aContainerType.Type() != MEDIAMIMETYPE(VIDEO_OGG) &&
      aContainerType.Type() != MEDIAMIMETYPE("application/ogg")) {
    return false;
  }

  const bool isOggVideo = (aContainerType.Type() != MEDIAMIMETYPE(AUDIO_OGG));

  const MediaCodecs& codecs = aContainerType.ExtendedType().Codecs();
  if (codecs.IsEmpty()) {
    // Ogg guarantees that the only codecs it contained are supported.
    return true;
  }
  // Verify that all the codecs specified are ones that we expect that
  // we can play.
  for (const auto& codec : codecs.Range()) {
    if ((MediaDecoder::IsOpusEnabled() && codec.EqualsLiteral("opus")) ||
        codec.EqualsLiteral("vorbis") ||
        codec.EqualsLiteral("flac")) {
      continue;
    }
    // Note: Only accept Theora in a video container type, not in an audio
    // container type.
    if (isOggVideo && codec.EqualsLiteral("theora")) {
      continue;
    }
    // Some unsupported codec.
    return false;
  }
  return true;
}

} // namespace mozilla
