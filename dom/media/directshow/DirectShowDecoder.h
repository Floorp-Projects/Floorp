/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(DirectShowDecoder_h_)
#define DirectShowDecoder_h_

#include "MediaDecoder.h"

namespace mozilla {

class MediaCodecs;
class MediaContentType;

// Decoder that uses DirectShow to playback MP3 files only.
class DirectShowDecoder : public MediaDecoder
{
public:

  explicit DirectShowDecoder(MediaDecoderOwner* aOwner);
  virtual ~DirectShowDecoder();

  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override {
    if (!IsEnabled()) {
      return nullptr;
    }
    return new DirectShowDecoder(aOwner);
  }

  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if aType is a MIME type that we render with the
  // DirectShow backend. If aCodecList is non null,
  // it is filled with a (static const) null-terminated list of strings
  // denoting the codecs we'll playback. Note that playback is strictly
  // limited to MP3 only.
  static bool GetSupportedCodecs(const MediaContentType& aType,
                                 MediaCodecs* aOutCodecs);

  // Returns true if the DirectShow backend is preffed on.
  static bool IsEnabled();
};

} // namespace mozilla

#endif
