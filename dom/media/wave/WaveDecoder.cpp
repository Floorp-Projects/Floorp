/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveDecoder.h"
#include "MediaContainerType.h"
#include "MediaDecoder.h"

namespace mozilla {

/* static */ bool
WaveDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (!MediaDecoder::IsWaveEnabled()) {
    return false;
  }
  if (aContainerType.Type() == MEDIAMIMETYPE("audio/wave") ||
      aContainerType.Type() == MEDIAMIMETYPE("audio/x-wav") ||
      aContainerType.Type() == MEDIAMIMETYPE("audio/wav") ||
      aContainerType.Type() == MEDIAMIMETYPE("audio/x-pn-wav")) {
    return (aContainerType.ExtendedType().Codecs().IsEmpty() ||
            aContainerType.ExtendedType().Codecs() == "1" ||
            aContainerType.ExtendedType().Codecs() == "6" ||
            aContainerType.ExtendedType().Codecs() == "7");
  }

  return false;
}

/* static */ nsTArray<UniquePtr<TrackInfo>>
WaveDecoder::GetTracksInfo(const MediaContainerType& aType)
{
  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (!IsSupportedType(aType)) {
    return tracks;
  }

  const MediaCodecs& codecs = aType.ExtendedType().Codecs();
  if (codecs.IsEmpty()) {
    tracks.AppendElement(
      CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
        NS_LITERAL_CSTRING("audio/x-wav"), aType));
    return tracks;
  }

  for (const auto& codec : codecs.Range()) {
    tracks.AppendElement(
      CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
        NS_LITERAL_CSTRING("audio/wave; codecs=") + NS_ConvertUTF16toUTF8(codec),
        aType));
  }
  return tracks;
}

} // namespace mozilla
