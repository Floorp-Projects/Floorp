
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3Decoder.h"

#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MP3Demuxer.h"
#include "PDMFactory.h"

namespace mozilla {

ChannelMediaDecoder*
MP3Decoder::Clone(MediaDecoderInit& aInit)
{
  if (!IsEnabled()) {
    return nullptr;
  }
  return new MP3Decoder(aInit);
}

MediaDecoderStateMachine*
MP3Decoder::CreateStateMachine() {
  mReader = new MediaFormatReader(this, new MP3Demuxer(mResource));
  return new MediaDecoderStateMachine(this, mReader);
}

/* static */
bool
MP3Decoder::IsEnabled() {
  RefPtr<PDMFactory> platform = new PDMFactory();
  return platform->SupportsMimeType(NS_LITERAL_CSTRING("audio/mpeg"),
                                    /* DecoderDoctorDiagnostics* */ nullptr);
}

/* static */
bool MP3Decoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (aContainerType.Type() == MEDIAMIMETYPE("audio/mp3")
      || aContainerType.Type() == MEDIAMIMETYPE("audio/mpeg")) {
    return
      IsEnabled()
      && (aContainerType.ExtendedType().Codecs().IsEmpty()
          || aContainerType.ExtendedType().Codecs() == "mp3");
  }
  return false;
}

} // namespace mozilla
