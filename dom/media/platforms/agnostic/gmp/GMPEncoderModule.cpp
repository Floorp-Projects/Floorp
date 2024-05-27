/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPEncoderModule.h"

#include "GMPService.h"
#include "GMPUtils.h"
#include "GMPVideoEncoder.h"
#include "MediaDataEncoderProxy.h"
#include "MP4Decoder.h"

namespace mozilla {

already_AddRefed<MediaDataEncoder> GMPEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  if (!Supports(aConfig)) {
    return nullptr;
  }

  RefPtr<gmp::GeckoMediaPluginService> s(
      gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
  if (NS_WARN_IF(!s)) {
    return nullptr;
  }

  nsCOMPtr<nsISerialEventTarget> thread(s->GetGMPThread());
  if (NS_WARN_IF(!thread)) {
    return nullptr;
  }

  RefPtr<MediaDataEncoder> encoder(new GMPVideoEncoder(aConfig));
  return do_AddRef(
      new MediaDataEncoderProxy(encoder.forget(), thread.forget()));
}

bool GMPEncoderModule::Supports(const EncoderConfig& aConfig) const {
  if (!CanLikelyEncode(aConfig)) {
    return false;
  }
  if (aConfig.mScalabilityMode != ScalabilityMode::None) {
    return false;
  }
  if (aConfig.mHardwarePreference == HardwarePreference::RequireHardware) {
    return false;
  }
  if (aConfig.mCodecSpecific && aConfig.mCodecSpecific->is<H264Specific>() &&
      aConfig.mCodecSpecific->as<H264Specific>().mFormat !=
          H264BitStreamFormat::ANNEXB) {
    return false;
  }
  return SupportsCodec(aConfig.mCodec);
}

bool GMPEncoderModule::SupportsCodec(CodecType aCodecType) const {
  if (aCodecType != CodecType::H264) {
    return false;
  }
  return HaveGMPFor("encode-video"_ns, {"h264"_ns});
}

}  // namespace mozilla
