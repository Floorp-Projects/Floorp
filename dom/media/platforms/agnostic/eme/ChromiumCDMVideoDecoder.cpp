/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMVideoDecoder.h"
#include "ChromiumCDMProxy.h"
#include "content_decryption_module.h"
#include "GMPService.h"
#include "GMPVideoDecoder.h"
#include "MP4Decoder.h"
#include "VPXDecoder.h"

namespace mozilla {

ChromiumCDMVideoDecoder::ChromiumCDMVideoDecoder(
  const GMPVideoDecoderParams& aParams,
  CDMProxy* aCDMProxy)
  : mCDMParent(aCDMProxy->AsChromiumCDMProxy()->GetCDMParent())
  , mConfig(aParams.mConfig)
  , mCrashHelper(aParams.mCrashHelper)
  , mGMPThread(GetGMPAbstractThread())
  , mImageContainer(aParams.mImageContainer)
{
}

ChromiumCDMVideoDecoder::~ChromiumCDMVideoDecoder()
{
}

static uint32_t
ToCDMH264Profile(uint8_t aProfile)
{
  switch (aProfile) {
    case 66:
      return cdm::VideoDecoderConfig::kH264ProfileBaseline;
    case 77:
      return cdm::VideoDecoderConfig::kH264ProfileMain;
    case 88:
      return cdm::VideoDecoderConfig::kH264ProfileExtended;
    case 100:
      return cdm::VideoDecoderConfig::kH264ProfileHigh;
    case 110:
      return cdm::VideoDecoderConfig::kH264ProfileHigh10;
    case 122:
      return cdm::VideoDecoderConfig::kH264ProfileHigh422;
    case 144:
      return cdm::VideoDecoderConfig::kH264ProfileHigh444Predictive;
  }
  return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
}

RefPtr<MediaDataDecoder::InitPromise>
ChromiumCDMVideoDecoder::Init()
{
  if (!mCDMParent) {
    // Must have failed to get the CDMParent from the ChromiumCDMProxy
    // in our constructor; the MediaKeys must have shut down the CDM
    // before we had a chance to start up the decoder.
    return InitPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  gmp::CDMVideoDecoderConfig config;
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    config.mCodec() = cdm::VideoDecoderConfig::kCodecH264;
    config.mProfile() =
      ToCDMH264Profile(mConfig.mExtraData->SafeElementAt(1, 0));
    config.mExtraData() = *mConfig.mExtraData;
    mConvertToAnnexB = true;
  } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
    config.mCodec() = cdm::VideoDecoderConfig::kCodecVp8;
    config.mProfile() = cdm::VideoDecoderConfig::kProfileNotNeeded;
  } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
    config.mCodec() = cdm::VideoDecoderConfig::kCodecVp9;
    config.mProfile() = cdm::VideoDecoderConfig::kProfileNotNeeded;
  } else {
    return MediaDataDecoder::InitPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  config.mImageWidth() = mConfig.mImage.width;
  config.mImageHeight() = mConfig.mImage.height;

  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  VideoInfo info = mConfig;
  RefPtr<layers::ImageContainer> imageContainer = mImageContainer;
  return InvokeAsync(
    mGMPThread, __func__, [cdm, config, info, imageContainer]() {
      return cdm->InitializeVideoDecoder(config, info, imageContainer);
    });
}

const char*
ChromiumCDMVideoDecoder::GetDescriptionName() const
{
  return "Chromium CDM video decoder";
}

MediaDataDecoder::ConversionRequired
ChromiumCDMVideoDecoder::NeedsConversion() const
{
  return mConvertToAnnexB ? ConversionRequired::kNeedAnnexB
                          : ConversionRequired::kNeedNone;
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMVideoDecoder::Decode(MediaRawData* aSample)
{
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mGMPThread, __func__, [cdm, sample]() {
    return cdm->DecryptAndDecodeFrame(sample);
  });
}

RefPtr<MediaDataDecoder::FlushPromise>
ChromiumCDMVideoDecoder::Flush()
{
  return FlushPromise::CreateAndReject(
    MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, RESULT_DETAIL("Unimplemented")),
    __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMVideoDecoder::Drain()
{
  return DecodePromise::CreateAndReject(
    MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, RESULT_DETAIL("Unimplemented")),
    __func__);
}

RefPtr<ShutdownPromise>
ChromiumCDMVideoDecoder::Shutdown()
{
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

} // namespace mozilla
