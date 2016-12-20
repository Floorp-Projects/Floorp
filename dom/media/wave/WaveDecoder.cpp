/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveDemuxer.h"
#include "MediaContentType.h"
#include "MediaDecoderStateMachine.h"
#include "WaveDecoder.h"
#include "MediaFormatReader.h"
#include "PDMFactory.h"

namespace mozilla {

MediaDecoder*
WaveDecoder::Clone(MediaDecoderOwner* aOwner)
{
  return new WaveDecoder(aOwner);
}

MediaDecoderStateMachine*
WaveDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(
    this, new MediaFormatReader(this, new WAVDemuxer(GetResource())));
}

/* static */ bool
WaveDecoder::IsSupportedType(const MediaContentType& aContentType)
{
  if (!IsWaveEnabled()) {
    return false;
  }
  if (aContentType.Type() == MEDIAMIMETYPE("audio/wave")
      || aContentType.Type() == MEDIAMIMETYPE("audio/x-wav")
      || aContentType.Type() == MEDIAMIMETYPE("audio/wav")
      || aContentType.Type() == MEDIAMIMETYPE("audio/x-pn-wav")) {
    return (aContentType.ExtendedType().Codecs().IsEmpty()
            || aContentType.ExtendedType().Codecs().AsString().EqualsASCII("1")
            || aContentType.ExtendedType().Codecs().AsString().EqualsASCII("6")
            || aContentType.ExtendedType().Codecs().AsString().EqualsASCII("7"));
  }

  return false;
}

} // namespace mozilla
