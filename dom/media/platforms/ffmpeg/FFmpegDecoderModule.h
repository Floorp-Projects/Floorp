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
#include "mozilla/StaticPrefs.h"

namespace mozilla {

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

  explicit FFmpegDecoderModule(FFmpegLibWrapper* aLib) : mLib(aLib) { }
  virtual ~FFmpegDecoderModule() { }

  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) override
  {
    // Temporary - forces use of VPXDecoder when alpha is present.
    // Bug 1263836 will handle alpha scenario once implemented. It will shift
    // the check for alpha to PDMFactory but not itself remove the need for a
    // check.
    if (aParams.VideoConfig().HasAlpha()) {
      return nullptr;
    }
    if (aParams.mOptions.contains(
          CreateDecoderParams::Option::LowLatency) &&
        !StaticPrefs::MediaFfmpegLowLatencyEnabled()) {
      return nullptr;
    }
    RefPtr<MediaDataDecoder> decoder = new FFmpegVideoDecoder<V>(
      mLib,
      aParams.mTaskQueue,
      aParams.VideoConfig(),
      aParams.mKnowsCompositor,
      aParams.mImageContainer,
      aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency));
    return decoder.forget();
  }

  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) override
  {
    RefPtr<MediaDataDecoder> decoder =
      new FFmpegAudioDecoder<V>(mLib,
                                aParams.mTaskQueue,
                                aParams.AudioConfig());
    return decoder.forget();
  }

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override
  {
    AVCodecID videoCodec = FFmpegVideoDecoder<V>::GetCodecId(aMimeType);
    AVCodecID audioCodec = FFmpegAudioDecoder<V>::GetCodecId(aMimeType);
    if (audioCodec == AV_CODEC_ID_NONE && videoCodec == AV_CODEC_ID_NONE) {
      return false;
    }
    AVCodecID codec = audioCodec != AV_CODEC_ID_NONE ? audioCodec : videoCodec;
    return !!FFmpegDataDecoder<V>::FindAVCodec(mLib, codec);
  }

protected:
  bool SupportsBitDepth(const uint8_t aBitDepth,
                        DecoderDoctorDiagnostics* aDiagnostics) const override
  {
    // We don't support bitDepth > 8 when compositor backend is D3D11.
    // But we don't have KnowsCompositor or any object
    // that we can ask for the layersbackend type.
    // We should remove this restriction until
    // we solve the D3D11 compositor backend issue.
#if defined(XP_LINUX) || defined(XP_MACOSX)
    return true;
#endif
    return aBitDepth == 8;
  }

private:
  FFmpegLibWrapper* mLib;
};

} // namespace mozilla

#endif // __FFmpegDecoderModule_h__
