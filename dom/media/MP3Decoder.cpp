
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3Decoder.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MP3Demuxer.h"
#include "PDMFactory.h"

namespace mozilla {

MediaDecoder*
MP3Decoder::Clone(MediaDecoderOwner* aOwner) {
  if (!IsEnabled()) {
    return nullptr;
  }
  return new MP3Decoder(aOwner);
}

MediaDecoderStateMachine*
MP3Decoder::CreateStateMachine() {
  RefPtr<MediaDecoderReader> reader =
      new MediaFormatReader(this, new mp3::MP3Demuxer(GetResource()));
  return new MediaDecoderStateMachine(this, reader);
}

/* static */
bool
MP3Decoder::IsEnabled() {
  PDMFactory::Init();
  RefPtr<PDMFactory> platform = new PDMFactory();
  return platform->SupportsMimeType(NS_LITERAL_CSTRING("audio/mpeg"),
                                    /* DecoderDoctorDiagnostics* */ nullptr);
}

/* static */
bool MP3Decoder::CanHandleMediaType(const nsACString& aType,
                                    const nsAString& aCodecs)
{
  if (aType.EqualsASCII("audio/mp3") || aType.EqualsASCII("audio/mpeg")) {
    return IsEnabled() &&
      (aCodecs.IsEmpty() || aCodecs.EqualsASCII("mp3"));
  }
  return false;
}

} // namespace mozilla
