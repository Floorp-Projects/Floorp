/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDecoderModule_h__
#define __FFmpegDecoderModule_h__

#include "PlatformDecoderModule.h"
#include "FFmpegLibWrapper.h"
#include "FFmpegAudioDecoder.h"
#include "FFmpegVideoDecoder.h"

namespace mozilla
{

template <int V>
class FFmpegDecoderModule : public PlatformDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule>
  Create(FFmpegLibWrapper* aLib)
  {
    RefPtr<PlatformDecoderModule> pdm = new FFmpegDecoderModule(aLib);

    return pdm.forget();
  }

  explicit FFmpegDecoderModule(FFmpegLibWrapper* aLib) : mLib(aLib) {}
  virtual ~FFmpegDecoderModule() {}

  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     TaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override
  {
    RefPtr<MediaDataDecoder> decoder =
      new FFmpegVideoDecoder<V>(mLib, aTaskQueue, aCallback, aConfig,
                                aImageContainer);
    return decoder.forget();
  }

  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     TaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override
  {
#ifdef USING_MOZFFVPX
    return nullptr;
#else
    RefPtr<MediaDataDecoder> decoder =
      new FFmpegAudioDecoder<V>(mLib, aTaskQueue, aCallback, aConfig);
    return decoder.forget();
#endif
  }

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override
  {
    AVCodecID videoCodec = FFmpegVideoDecoder<V>::GetCodecId(aMimeType);
#ifdef USING_MOZFFVPX
    if (videoCodec == AV_CODEC_ID_NONE) {
        return false;
    }
    return !!FFmpegDataDecoder<V>::FindAVCodec(mLib, videoCodec);
#else
    AVCodecID audioCodec = FFmpegAudioDecoder<V>::GetCodecId(aMimeType);
    if (audioCodec == AV_CODEC_ID_NONE && videoCodec == AV_CODEC_ID_NONE) {
      return false;
    }
    AVCodecID codec = audioCodec != AV_CODEC_ID_NONE ? audioCodec : videoCodec;
    return !!FFmpegDataDecoder<V>::FindAVCodec(mLib, codec);
#endif
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

private:
  FFmpegLibWrapper* mLib;
};

} // namespace mozilla

#endif // __FFmpegDecoderModule_h__
