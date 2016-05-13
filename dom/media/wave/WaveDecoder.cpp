/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaveDemuxer.h"
#include "mozilla/Preferences.h"
#include "MediaDecoderStateMachine.h"
#include "WaveReader.h"
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
  if (Preferences::GetBool("media.wave.decoder.enabled", false)) {
    RefPtr<MediaDecoderReader> reader =
        new MediaFormatReader(this, new WAVDemuxer(GetResource()));
    return new MediaDecoderStateMachine(this, reader);
  } else {
    return new MediaDecoderStateMachine(this, new WaveReader(this));
  }
}

/* static */
bool
WaveDecoder::IsEnabled()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!Preferences::GetBool("media.wave.decoder.enabled", false)) {
    return false;
  }
  RefPtr<PDMFactory> platform = new PDMFactory();
  return platform->SupportsMimeType(NS_LITERAL_CSTRING("audio/x-wav"),
                                    /* DecoderDoctorDiagnostics* */ nullptr);
}

/* static */
bool
WaveDecoder::CanHandleMediaType(const nsACString& aType,
                               const nsAString& aCodecs)
{
  if (aType.EqualsASCII("audio/wave") || aType.EqualsASCII("audio/x-wav") ||
      aType.EqualsASCII("audio/wav")  || aType.EqualsASCII("audio/x-pn-wav")) {
    return IsEnabled() && (aCodecs.IsEmpty() ||
                           aCodecs.EqualsASCII("1") ||
                           aCodecs.EqualsASCII("6") ||
                           aCodecs.EqualsASCII("7"));
  }

  return false;
}

} // namespace mozilla
