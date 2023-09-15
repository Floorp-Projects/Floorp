/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecoderModule.h"

#include "DecoderDoctorDiagnostics.h"
#include "GMPService.h"
#include "GMPUtils.h"
#include "GMPVideoDecoder.h"
#include "MP4Decoder.h"
#include "MediaDataDecoderProxy.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "gmp-video-decode.h"
#include "mozilla/StaticMutex.h"
#include "nsServiceManagerUtils.h"
#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#endif

namespace mozilla {

static already_AddRefed<MediaDataDecoderProxy> CreateDecoderWrapper(
    GMPVideoDecoderParams&& aParams) {
  RefPtr<gmp::GeckoMediaPluginService> s(
      gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
  if (!s) {
    return nullptr;
  }
  nsCOMPtr<nsISerialEventTarget> thread(s->GetGMPThread());
  if (!thread) {
    return nullptr;
  }

  RefPtr<MediaDataDecoderProxy> decoder(new MediaDataDecoderProxy(
      do_AddRef(new GMPVideoDecoder(std::move(aParams))), thread.forget()));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> GMPDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (!MP4Decoder::IsH264(aParams.mConfig.mMimeType) &&
      !VPXDecoder::IsVP8(aParams.mConfig.mMimeType) &&
      !VPXDecoder::IsVP9(aParams.mConfig.mMimeType)) {
    return nullptr;
  }

  return CreateDecoderWrapper(GMPVideoDecoderParams(aParams));
}

already_AddRefed<MediaDataDecoder> GMPDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  return nullptr;
}

/* static */
media::DecodeSupportSet GMPDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, const nsACString& aApi,
    const Maybe<nsCString>& aKeySystem) {
  AutoTArray<nsCString, 2> tags;
  if (MP4Decoder::IsH264(aMimeType)) {
    tags.AppendElement("h264"_ns);
  } else if (VPXDecoder::IsVP9(aMimeType)) {
    tags.AppendElement("vp9"_ns);
  } else if (VPXDecoder::IsVP8(aMimeType)) {
    tags.AppendElement("vp8"_ns);
  } else {
    return media::DecodeSupportSet{};
  }

  // Optional tag for EME GMP plugins.
  if (aKeySystem) {
    tags.AppendElement(*aKeySystem);
  }

  // GMP plugins are always software based.
  return HaveGMPFor(aApi, tags) ? media::DecodeSupport::SoftwareDecode
                                : media::DecodeSupportSet{};
}

media::DecodeSupportSet GMPDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return SupportsMimeType(aMimeType, "decode-video"_ns, Nothing());
}

/* static */
already_AddRefed<PlatformDecoderModule> GMPDecoderModule::Create() {
  return MakeAndAddRef<GMPDecoderModule>();
}

}  // namespace mozilla
