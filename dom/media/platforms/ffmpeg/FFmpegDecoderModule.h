/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDecoderModule_h__
#define __FFmpegDecoderModule_h__

#include "FFmpegAudioDecoder.h"
#include "FFmpegLibWrapper.h"
#include "FFmpegVideoDecoder.h"
#include "PlatformDecoderModule.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

template <int V>
class FFmpegDecoderModule : public PlatformDecoderModule {
 public:
  static already_AddRefed<PlatformDecoderModule> Create(
      FFmpegLibWrapper* aLib) {
    RefPtr<PlatformDecoderModule> pdm = new FFmpegDecoderModule(aLib);

    return pdm.forget();
  }

  explicit FFmpegDecoderModule(FFmpegLibWrapper* aLib) : mLib(aLib) {}
  virtual ~FFmpegDecoderModule() = default;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override {
    if (!Supports(SupportDecoderParams(aParams), nullptr)) {
      return nullptr;
    }
    RefPtr<MediaDataDecoder> decoder = new FFmpegVideoDecoder<V>(
        mLib, aParams.VideoConfig(), aParams.mKnowsCompositor,
        aParams.mImageContainer,
        aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency),
        aParams.mOptions.contains(
            CreateDecoderParams::Option::HardwareDecoderNotAllowed));
    return decoder.forget();
  }

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override {
    if (!Supports(SupportDecoderParams(aParams), nullptr)) {
      return nullptr;
    }
    RefPtr<MediaDataDecoder> decoder =
        new FFmpegAudioDecoder<V>(mLib, aParams.AudioConfig());
    return decoder.forget();
  }

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override {
    UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
    if (!trackInfo) {
      return false;
    }
    return Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
  }

  bool Supports(const SupportDecoderParams& aParams,
                DecoderDoctorDiagnostics* aDiagnostics) const override {
    const auto& trackInfo = aParams.mConfig;
    const nsACString& mimeType = trackInfo.mMimeType;

    // Temporary - forces use of VPXDecoder when alpha is present.
    // Bug 1263836 will handle alpha scenario once implemented. It will shift
    // the check for alpha to PDMFactory but not itself remove the need for a
    // check.
    if (VPXDecoder::IsVPX(mimeType) && trackInfo.GetAsVideoInfo()->HasAlpha()) {
      return false;
    }

    AVCodecID videoCodec = FFmpegVideoDecoder<V>::GetCodecId(mimeType);
    AVCodecID audioCodec = FFmpegAudioDecoder<V>::GetCodecId(mimeType);
    if (audioCodec == AV_CODEC_ID_NONE && videoCodec == AV_CODEC_ID_NONE) {
      return false;
    }
    AVCodecID codec = audioCodec != AV_CODEC_ID_NONE ? audioCodec : videoCodec;
    return !!FFmpegDataDecoder<V>::FindAVCodec(mLib, codec);
  }

 protected:
  bool SupportsColorDepth(
      gfx::ColorDepth aColorDepth,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
#if defined(MOZ_WIDGET_ANDROID)
    return aColorDepth == gfx::ColorDepth::COLOR_8;
#endif
    return true;
  }

 private:
  FFmpegLibWrapper* mLib;
};

}  // namespace mozilla

#endif  // __FFmpegDecoderModule_h__
