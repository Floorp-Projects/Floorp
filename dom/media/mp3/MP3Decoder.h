/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MP3Decoder_h_
#define MP3Decoder_h_

#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla {

class MediaContainerType;
class TrackInfo;

class MP3Decoder
{
public:
  // Returns true if the MP3 backend is preffed on, and we're running on a
  // platform that is likely to have decoders for the format.
  static bool IsEnabled();
  static bool IsSupportedType(const MediaContainerType& aContainerType);
  static nsTArray<UniquePtr<TrackInfo>> GetTracksInfo(
    const MediaContainerType& aType);
};

} // namespace mozilla

#endif
