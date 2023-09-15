/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3Decoder.h"
#include "MediaContainerType.h"
#include "PDMFactory.h"

namespace mozilla {

/* static */
bool MP3Decoder::IsEnabled() {
  RefPtr<PDMFactory> platform = new PDMFactory();
  return !platform->SupportsMimeType("audio/mpeg"_ns).isEmpty();
}

/* static */
bool MP3Decoder::IsSupportedType(const MediaContainerType& aContainerType) {
  if (aContainerType.Type() == MEDIAMIMETYPE("audio/mp3") ||
      aContainerType.Type() == MEDIAMIMETYPE("audio/mpeg")) {
    return IsEnabled() && (aContainerType.ExtendedType().Codecs().IsEmpty() ||
                           aContainerType.ExtendedType().Codecs() == "mp3");
  }
  return false;
}

/* static */
nsTArray<UniquePtr<TrackInfo>> MP3Decoder::GetTracksInfo(
    const MediaContainerType& aType) {
  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (!IsSupportedType(aType)) {
    return tracks;
  }

  tracks.AppendElement(
      CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          "audio/mpeg"_ns, aType));

  return tracks;
}

}  // namespace mozilla
