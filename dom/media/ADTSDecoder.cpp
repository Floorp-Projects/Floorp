/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ADTSDecoder.h"
#include "ADTSDemuxer.h"
#include "MediaContentType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "PDMFactory.h"

namespace mozilla {

MediaDecoder*
ADTSDecoder::Clone(MediaDecoderOwner* aOwner)
{
  if (!IsEnabled())
    return nullptr;

  return new ADTSDecoder(aOwner);
}

MediaDecoderStateMachine*
ADTSDecoder::CreateStateMachine()
{
  RefPtr<MediaDecoderReader> reader =
      new MediaFormatReader(this, new ADTSDemuxer(GetResource()));
  return new MediaDecoderStateMachine(this, reader);
}

/* static */ bool
ADTSDecoder::IsEnabled()
{
  RefPtr<PDMFactory> platform = new PDMFactory();
  return platform->SupportsMimeType(NS_LITERAL_CSTRING("audio/mp4a-latm"),
                                    /* DecoderDoctorDiagnostics* */ nullptr);
}

/* static */ bool
ADTSDecoder::IsSupportedType(const MediaContentType& aContentType)
{
  if (aContentType.Type() == MEDIAMIMETYPE("audio/aac")
      || aContentType.Type() == MEDIAMIMETYPE("audio/aacp")
      || aContentType.Type() == MEDIAMIMETYPE("audio/x-aac")) {
    return
      IsEnabled()
      && (aContentType.ExtendedType().Codecs().IsEmpty()
          || aContentType.ExtendedType().Codecs().AsString().EqualsASCII("aac"));
  }

  return false;
}

} // namespace mozilla
