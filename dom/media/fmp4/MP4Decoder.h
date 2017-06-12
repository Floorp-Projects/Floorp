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

class MediaContainerType;

// Decoder that uses a bundled MP4 demuxer and platform decoders to play MP4.
class MP4Decoder : public MediaDecoder
{
public:
  explicit MP4Decoder(MediaDecoderInit& aInit);

  MediaDecoder* Clone(MediaDecoderInit& aInit) override {
    if (!IsEnabled()) {
      return nullptr;
    }
    return new MP4Decoder(aInit);
  }

  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if aContainerType is an MP4 type that we think we can render
  // with the a platform decoder backend.
  // If provided, codecs are checked for support.
  static bool IsSupportedType(const MediaContainerType& aContainerType,
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

  void GetMozDebugReaderData(nsACString& aString) override;

private:
  RefPtr<MediaFormatReader> mReader;
};

} // namespace mozilla

#endif
