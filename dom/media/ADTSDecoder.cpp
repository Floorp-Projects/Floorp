/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ADTSDecoder.h"
#include "MediaContainerType.h"
#include "PDMFactory.h"

namespace mozilla {

/* static */
bool ADTSDecoder::IsEnabled() {
  RefPtr<PDMFactory> platform = new PDMFactory();
  return !platform->SupportsMimeType("audio/mp4a-latm"_ns).isEmpty();
}

/* static */
bool ADTSDecoder::IsSupportedType(const MediaContainerType& aContainerType) {
  if (aContainerType.Type() == MEDIAMIMETYPE("audio/aac") ||
      aContainerType.Type() == MEDIAMIMETYPE("audio/aacp") ||
      aContainerType.Type() == MEDIAMIMETYPE("audio/x-aac")) {
    return IsEnabled() && (aContainerType.ExtendedType().Codecs().IsEmpty() ||
                           aContainerType.ExtendedType().Codecs() == "aac");
  }

  return false;
}

/* static */
nsTArray<UniquePtr<TrackInfo>> ADTSDecoder::GetTracksInfo(
    const MediaContainerType& aType) {
  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (!IsSupportedType(aType)) {
    return tracks;
  }

  tracks.AppendElement(
      CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          "audio/mp4a-latm"_ns, aType));

  return tracks;
}

}  // namespace mozilla
