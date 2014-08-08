/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MP4Decoder_h_)
#define MP4Decoder_h_

#include "MediaDecoder.h"

namespace mozilla {

// Decoder that uses a bundled MP4 demuxer and platform decoders to play MP4.
class MP4Decoder : public MediaDecoder
{
public:

  virtual MediaDecoder* Clone() {
    if (!IsEnabled()) {
      return nullptr;
    }
    return new MP4Decoder();
  }

  virtual MediaDecoderStateMachine* CreateStateMachine();

#ifdef MOZ_EME
  virtual nsresult SetCDMProxy(CDMProxy* aProxy) MOZ_OVERRIDE;
#endif

  // Returns true if aType is a MIME type that we can render with the
  // a MP4 platform decoder backend. If aCodecList is non null,
  // it is filled with a (static const) null-terminated list of strings
  // denoting the codecs we'll playback.
  static bool GetSupportedCodecs(const nsACString& aType,
                                 char const *const ** aCodecList);

  // Returns true if the MP4 backend is preffed on, and we're running on a
  // platform that is likely to have decoders for the contained formats.
  static bool IsEnabled();
};

} // namespace mozilla

#endif
