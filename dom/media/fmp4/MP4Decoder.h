/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MP4Decoder_h_)
#define MP4Decoder_h_

#include "MediaDecoder.h"
#include "MediaFormatReader.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/layers/KnowsCompositor.h"

namespace mozilla {

class MediaContentType;

// Decoder that uses a bundled MP4 demuxer and platform decoders to play MP4.
class MP4Decoder : public MediaDecoder
{
public:
  explicit MP4Decoder(MediaDecoderOwner* aOwner);

  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override {
    if (!IsEnabled()) {
      return nullptr;
    }
    return new MP4Decoder(aOwner);
  }

  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if aType is a type that we think we can render with the
  // a MP4 platform decoder backend.
  static bool CanHandleMediaType(const MediaContentType& aType,
                                 DecoderDoctorDiagnostics* aDiagnostics);

  // Return true if aMimeType is a one of the strings used by our demuxers to
  // identify H264. Does not parse general content type strings, i.e. white
  // space matters.
  static bool IsH264(const nsACString& aMimeType);

  // Return true if aMimeType is a one of the strings used by our demuxers to
  // identify AAC. Does not parse general content type strings, i.e. white
  // space matters.
  static bool IsAAC(const nsACString& aMimeType);

  // Returns true if the MP4 backend is preffed on.
  static bool IsEnabled();

  static already_AddRefed<dom::Promise>
  IsVideoAccelerated(layers::KnowsCompositor* aKnowsCompositor, nsIGlobalObject* aParent);

  void GetMozDebugReaderData(nsAString& aString) override;

private:
  RefPtr<MediaFormatReader> mReader;
};

} // namespace mozilla

#endif
