/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlacDecoder.h"
#include "MediaContainerType.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

/* static */
bool FlacDecoder::IsEnabled() {
#ifdef MOZ_FFVPX
  return StaticPrefs::media_flac_enabled();
#else
  return false;
#endif
}

/* static */
bool FlacDecoder::IsSupportedType(const MediaContainerType& aContainerType) {
  return IsEnabled() &&
         (aContainerType.Type() == MEDIAMIMETYPE("audio/flac") ||
          aContainerType.Type() == MEDIAMIMETYPE("audio/x-flac") ||
          aContainerType.Type() == MEDIAMIMETYPE("application/x-flac"));
}

/* static */
nsTArray<UniquePtr<TrackInfo>> FlacDecoder::GetTracksInfo(
    const MediaContainerType& aType) {
  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (!IsSupportedType(aType)) {
    return tracks;
  }

  tracks.AppendElement(
      CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          "audio/flac"_ns, aType));

  return tracks;
}

}  // namespace mozilla
