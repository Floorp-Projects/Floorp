/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecoderModule.h"
#include "DecoderDoctorDiagnostics.h"
#include "GMPVideoDecoder.h"
#include "GMPUtils.h"
#include "MediaDataDecoderProxy.h"
#include "MediaPrefs.h"
#include "VideoUtils.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/StaticMutex.h"
#include "gmp-video-decode.h"
#include "MP4Decoder.h"
#include "VPXDecoder.h"
#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif

namespace mozilla {

GMPDecoderModule::GMPDecoderModule()
{
}

GMPDecoderModule::~GMPDecoderModule()
{
}

static already_AddRefed<MediaDataDecoderProxy>
CreateDecoderWrapper(MediaDataDecoderCallback* aCallback)
{
  RefPtr<gmp::GeckoMediaPluginService> s(gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
  if (!s) {
    return nullptr;
  }
  RefPtr<AbstractThread> thread(s->GetAbstractGMPThread());
  if (!thread) {
    return nullptr;
  }
  RefPtr<MediaDataDecoderProxy> decoder(new MediaDataDecoderProxy(thread.forget(), aCallback));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
GMPDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  if (!MP4Decoder::IsH264(aParams.mConfig.mMimeType) &&
      !VPXDecoder::IsVP8(aParams.mConfig.mMimeType) &&
      !VPXDecoder::IsVP9(aParams.mConfig.mMimeType)) {
    return nullptr;
  }

  RefPtr<MediaDataDecoderProxy> wrapper = CreateDecoderWrapper(aParams.mCallback);
  auto params = GMPVideoDecoderParams(aParams).WithCallback(wrapper);
  wrapper->SetProxyTarget(new GMPVideoDecoder(params));
  return wrapper.forget();
}

already_AddRefed<MediaDataDecoder>
GMPDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  return nullptr;
}

PlatformDecoderModule::ConversionRequired
GMPDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  // GMPVideoCodecType::kGMPVideoCodecH264 specifies that encoded frames must be in AVCC format.
  if (aConfig.IsVideo() && MP4Decoder::IsH264(aConfig.mMimeType)) {
    return ConversionRequired::kNeedAVCC;
  } else {
    return ConversionRequired::kNeedNone;
  }
}

/* static */
bool
GMPDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                   const Maybe<nsCString>& aGMP)
{
  if (aGMP.isNothing()) {
    return false;
  }

  if (MP4Decoder::IsH264(aMimeType)) {
    return HaveGMPFor(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                      { NS_LITERAL_CSTRING("h264"), aGMP.value()});
  }

  if (VPXDecoder::IsVP9(aMimeType)) {
    return HaveGMPFor(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                      { NS_LITERAL_CSTRING("vp9"), aGMP.value()});
  }

  if (VPXDecoder::IsVP8(aMimeType)) {
    return HaveGMPFor(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                      { NS_LITERAL_CSTRING("vp8"), aGMP.value()});
  }

  return false;
}

bool
GMPDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                   DecoderDoctorDiagnostics* aDiagnostics) const
{
  return false;
}

} // namespace mozilla
