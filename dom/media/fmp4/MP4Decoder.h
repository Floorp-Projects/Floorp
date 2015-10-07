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
  MP4Decoder();

  virtual MediaDecoder* Clone() override {
    if (!IsEnabled()) {
      return nullptr;
    }
    return new MP4Decoder();
  }

  virtual MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if aMIMEType is a type that we think we can render with the
  // a MP4 platform decoder backend. If aCodecs is non emtpy, it is filled
  // with a comma-delimited list of codecs to check support for. Notes in
  // out params wether the codecs string contains AAC or H.264.
  static bool CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                                 const nsAString& aCodecs);

  static bool CanHandleMediaType(const nsAString& aMIMEType);

  // Returns true if the MP4 backend is preffed on.
  static bool IsEnabled();

  static bool IsVideoAccelerated(layers::LayersBackend aBackend, nsACString& aReason);
  static bool CanCreateAACDecoder();
  static bool CanCreateH264Decoder();
};

} // namespace mozilla

#endif
