/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AgnosticDecoderModule.h"
#include "OpusDecoder.h"
#include "VorbisDecoder.h"
#include "VPXDecoder.h"

namespace mozilla {

bool
AgnosticDecoderModule::SupportsMimeType(const nsACString& aMimeType)
{
  return VPXDecoder::IsVPX(aMimeType) ||
    OpusDataDecoder::IsOpus(aMimeType) ||
    VorbisDataDecoder::IsVorbis(aMimeType);
}

already_AddRefed<MediaDataDecoder>
AgnosticDecoderModule::CreateVideoDecoder(const VideoInfo& aConfig,
                                          layers::LayersBackend aLayersBackend,
                                          layers::ImageContainer* aImageContainer,
                                          FlushableTaskQueue* aVideoTaskQueue,
                                          MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> m;

  if (VPXDecoder::IsVPX(aConfig.mMimeType)) {
    m = new VPXDecoder(*aConfig.GetAsVideoInfo(),
                       aImageContainer,
                       aVideoTaskQueue,
                       aCallback);
  }

  return m.forget();
}

already_AddRefed<MediaDataDecoder>
AgnosticDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                          FlushableTaskQueue* aAudioTaskQueue,
                                          MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> m;

  if (VorbisDataDecoder::IsVorbis(aConfig.mMimeType)) {
    m = new VorbisDataDecoder(*aConfig.GetAsAudioInfo(),
                              aAudioTaskQueue,
                              aCallback);
  } else if (OpusDataDecoder::IsOpus(aConfig.mMimeType)) {
    m = new OpusDataDecoder(*aConfig.GetAsAudioInfo(),
                            aAudioTaskQueue,
                            aCallback);
  }

  return m.forget();
}

} // namespace mozilla
