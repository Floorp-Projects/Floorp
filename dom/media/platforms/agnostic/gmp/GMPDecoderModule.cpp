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
    const nsACString& aMimeType, const Maybe<nsCString>& aGMP) {
  bool isSupported = false;
  if (aGMP.isNothing()) {
    return media::DecodeSupport::Unsupported;
  }

  nsCString api = nsLiteralCString(CHROMIUM_CDM_API);

  if (MP4Decoder::IsH264(aMimeType)) {
    isSupported = HaveGMPFor(api, {"h264"_ns, aGMP.value()});
  } else if (VPXDecoder::IsVP9(aMimeType)) {
    isSupported = HaveGMPFor(api, {"vp9"_ns, aGMP.value()});
  } else if (VPXDecoder::IsVP8(aMimeType)) {
    isSupported = HaveGMPFor(api, {"vp8"_ns, aGMP.value()});
  }

  // TODO: Note that we do not yet distinguish between SW/HW decode support.
  //       Will be done in bug 1754239.
  if (isSupported) {
    return media::DecodeSupport::SoftwareDecode;
  }
  return media::DecodeSupport::Unsupported;
}

media::DecodeSupportSet GMPDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return media::DecodeSupport::Unsupported;
}

/* static */
already_AddRefed<PlatformDecoderModule> GMPDecoderModule::Create() {
  return MakeAndAddRef<GMPDecoderModule>();
}

}  // namespace mozilla
