/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WebMDecoder_h_)
#define WebMDecoder_h_

#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla {

class MediaContainerType;
class TrackInfo;

class WebMDecoder
{
public:

  // Returns true if aContainerType is a WebM type that we think we can render
  // with an enabled platform decoder backend.
  // If provided, codecs are checked for support.
  static bool IsSupportedType(const MediaContainerType& aContainerType);

  static nsTArray<UniquePtr<TrackInfo>> GetTracksInfo(
    const MediaContainerType& aType);
private:
  static nsTArray<UniquePtr<TrackInfo>> GetTracksInfo(
    const MediaContainerType& aType,
    MediaResult& aError);
};

} // namespace mozilla

#endif
