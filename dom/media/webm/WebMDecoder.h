/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WebMDecoder_h_)
#define WebMDecoder_h_

#include "MediaDecoder.h"

namespace mozilla {

class WebMDecoder : public MediaDecoder
{
public:
  explicit WebMDecoder(MediaDecoderOwner* aOwner) : MediaDecoder(aOwner) {}
  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override {
    if (!IsWebMEnabled()) {
      return nullptr;
    }
    return new WebMDecoder(aOwner);
  }
  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if the WebM backend is preffed on.
  static bool IsEnabled();

  // Returns true if aMIMEType is a type that we think we can render with the
  // a WebM platform decoder backend. If aCodecs is non emtpy, it is filled
  // with a comma-delimited list of codecs to check support for. Notes in
  // out params whether the codecs string contains Opus/Vorbis or VP8/VP9.
  static bool CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                                 const nsAString& aCodecs);
};

} // namespace mozilla

#endif
