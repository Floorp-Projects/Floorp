/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AgnosticDecoderModule.h"
#include "OpusDecoder.h"
#include "TheoraDecoder.h"
#include "VPXDecoder.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#  include "DAV1DDecoder.h"
#endif

namespace mozilla {

bool AgnosticDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  bool supports =
      VPXDecoder::IsVPX(aMimeType) || OpusDataDecoder::IsOpus(aMimeType) ||
      WaveDataDecoder::IsWave(aMimeType) || TheoraDecoder::IsTheora(aMimeType);
  if (!StaticPrefs::MediaRddVorbisEnabled() ||
      !StaticPrefs::MediaRddProcessEnabled() ||
      !BrowserTabsRemoteAutostart()) {
    supports |= VorbisDataDecoder::IsVorbis(aMimeType);
  }
#ifdef MOZ_AV1
  // We remove support for decoding AV1 here if RDD is enabled so that
  // decoding on the content process doesn't accidentally happen in case
  // something goes wrong with launching the RDD process.
  if (StaticPrefs::MediaAv1Enabled() &&
      !StaticPrefs::MediaRddProcessEnabled()) {
    supports |= AOMDecoder::IsAV1(aMimeType);
  }
#endif
  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Agnostic decoder %s requested type",
           supports ? "supports" : "rejects"));
  return supports;
}

already_AddRefed<MediaDataDecoder> AgnosticDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> m;

  if (VPXDecoder::IsVPX(aParams.mConfig.mMimeType)) {
    m = new VPXDecoder(aParams);
  }
#ifdef MOZ_AV1
  // see comment above about AV1 and the RDD process
  else if (AOMDecoder::IsAV1(aParams.mConfig.mMimeType) &&
           !StaticPrefs::MediaRddProcessEnabled() &&
           StaticPrefs::MediaAv1Enabled()) {
    if (StaticPrefs::MediaAv1UseDav1d()) {
      m = new DAV1DDecoder(aParams);
    } else {
      m = new AOMDecoder(aParams);
    }
  }
#endif
  else if (TheoraDecoder::IsTheora(aParams.mConfig.mMimeType)) {
    m = new TheoraDecoder(aParams);
  }

  return m.forget();
}

already_AddRefed<MediaDataDecoder> AgnosticDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
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

}  // namespace mozilla
