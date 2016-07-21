/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AgnosticDecoderModule.h"
#include "mozilla/Logging.h"
#include "OpusDecoder.h"
#include "VorbisDecoder.h"
#include "VPXDecoder.h"
#include "WAVDecoder.h"
#include "TheoraDecoder.h"

namespace mozilla {

bool
AgnosticDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                        DecoderDoctorDiagnostics* aDiagnostics) const
{
  bool supports =
    VPXDecoder::IsVPX(aMimeType) ||
    OpusDataDecoder::IsOpus(aMimeType) ||
    VorbisDataDecoder::IsVorbis(aMimeType) ||
    WaveDataDecoder::IsWave(aMimeType) ||
    TheoraDecoder::IsTheora(aMimeType);
  MOZ_LOG(sPDMLog, LogLevel::Debug, ("Agnostic decoder %s requested type",
        supports ? "supports" : "rejects"));
  return supports;
}

already_AddRefed<MediaDataDecoder>
AgnosticDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<MediaDataDecoder> m;

  if (VPXDecoder::IsVPX(aParams.mConfig.mMimeType)) {
    m = new VPXDecoder(aParams);
  } else if (TheoraDecoder::IsTheora(aParams.mConfig.mMimeType)) {
    m = new TheoraDecoder(aParams);
  }

  return m.forget();
}

already_AddRefed<MediaDataDecoder>
AgnosticDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<MediaDataDecoder> m;

  const TrackInfo& config = aParams.mConfig;
  if (VorbisDataDecoder::IsVorbis(config.mMimeType)) {
    m = new VorbisDataDecoder(aParams);
  } else if (OpusDataDecoder::IsOpus(config.mMimeType)) {
    m = new OpusDataDecoder(aParams);
  } else if (WaveDataDecoder::IsWave(config.mMimeType)) {
    m = new WaveDataDecoder(aParams);
  }

  return m.forget();
}

} // namespace mozilla
