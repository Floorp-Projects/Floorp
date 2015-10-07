/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDecoderModule_h__
#define __FFmpegDecoderModule_h__

#include "PlatformDecoderModule.h"
#include "FFmpegAudioDecoder.h"
#include "FFmpegH264Decoder.h"

namespace mozilla
{

template <int V>
class FFmpegDecoderModule : public PlatformDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule>
  Create()
  {
    uint32_t major, minor;
    GetVersion(major, minor);
    if (major < 54 && !sFFmpegDecoderEnabled) {
      return nullptr;
    }
    nsRefPtr<PlatformDecoderModule> pdm = new FFmpegDecoderModule();

    return pdm.forget();
  }

  static bool
  GetVersion(uint32_t& aMajor, uint32_t& aMinor)
  {
    uint32_t version = avcodec_version();
    aMajor = (version >> 16) & 0xff;
    aMinor = (version >> 8) & 0xff;
    return true;
  }

  FFmpegDecoderModule() {}
  virtual ~FFmpegDecoderModule() {}

  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) override
  {
    nsRefPtr<MediaDataDecoder> decoder =
      new FFmpegH264Decoder<V>(aVideoTaskQueue, aCallback, aConfig,
                               aImageContainer);
    return decoder.forget();
  }

  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) override
  {
    nsRefPtr<MediaDataDecoder> decoder =
      new FFmpegAudioDecoder<V>(aAudioTaskQueue, aCallback, aConfig);
    return decoder.forget();
  }

  bool SupportsMimeType(const nsACString& aMimeType) override
  {
    return FFmpegAudioDecoder<V>::GetCodecId(aMimeType) != AV_CODEC_ID_NONE ||
      FFmpegH264Decoder<V>::GetCodecId(aMimeType) != AV_CODEC_ID_NONE;
  }

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override
  {
    if (aConfig.IsVideo() &&
        (aConfig.mMimeType.EqualsLiteral("video/avc") ||
         aConfig.mMimeType.EqualsLiteral("video/mp4"))) {
      return PlatformDecoderModule::kNeedAVCC;
    } else {
      return kNeedNone;
    }
  }

};

} // namespace mozilla

#endif // __FFmpegDecoderModule_h__
