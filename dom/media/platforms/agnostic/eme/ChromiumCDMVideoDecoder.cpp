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
    const GMPVideoDecoderParams& aParams, CDMProxy* aCDMProxy)
    : mCDMParent(aCDMProxy->AsChromiumCDMProxy()->GetCDMParent()),
      mConfig(aParams.mConfig),
      mCrashHelper(aParams.mCrashHelper),
      mGMPThread(GetGMPThread()),
      mImageContainer(aParams.mImageContainer),
      mKnowsCompositor(aParams.mKnowsCompositor) {}

ChromiumCDMVideoDecoder::~ChromiumCDMVideoDecoder() = default;

static uint32_t ToCDMH264Profile(uint8_t aProfile) {
  switch (aProfile) {
    case 66:
      return cdm::VideoCodecProfile::kH264ProfileBaseline;
    case 77:
      return cdm::VideoCodecProfile::kH264ProfileMain;
    case 88:
      return cdm::VideoCodecProfile::kH264ProfileExtended;
    case 100:
      return cdm::VideoCodecProfile::kH264ProfileHigh;
    case 110:
      return cdm::VideoCodecProfile::kH264ProfileHigh10;
    case 122:
      return cdm::VideoCodecProfile::kH264ProfileHigh422;
    case 144:
      return cdm::VideoCodecProfile::kH264ProfileHigh444Predictive;
  }
  return cdm::VideoCodecProfile::kUnknownVideoCodecProfile;
}

RefPtr<MediaDataDecoder::InitPromise> ChromiumCDMVideoDecoder::Init() {
  if (!mCDMParent) {
    // Must have failed to get the CDMParent from the ChromiumCDMProxy
    // in our constructor; the MediaKeys must have shut down the CDM
    // before we had a chance to start up the decoder.
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  gmp::CDMVideoDecoderConfig config;
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    config.mCodec() = cdm::VideoCodec::kCodecH264;
    config.mProfile() =
        ToCDMH264Profile(mConfig.mExtraData->SafeElementAt(1, 0));
    config.mExtraData() = mConfig.mExtraData->Clone();
    mConvertToAnnexB = true;
  } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
    config.mCodec() = cdm::VideoCodec::kCodecVp8;
    config.mProfile() = cdm::VideoCodecProfile::kProfileNotNeeded;
  } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
    config.mCodec() = cdm::VideoCodec::kCodecVp9;
    config.mProfile() = cdm::VideoCodecProfile::kProfileNotNeeded;
  } else {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  config.mImageWidth() = mConfig.mImage.width;
  config.mImageHeight() = mConfig.mImage.height;
  config.mEncryptionScheme() = cdm::EncryptionScheme::kUnencrypted;
  switch (mConfig.mCrypto.mCryptoScheme) {
    case CryptoScheme::None:
      break;
    case CryptoScheme::Cenc:
      config.mEncryptionScheme() = cdm::EncryptionScheme::kCenc;
      break;
    case CryptoScheme::Cbcs:
      config.mEncryptionScheme() = cdm::EncryptionScheme::kCbcs;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Should not have unrecognized encryption type");
      break;
  }

  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  VideoInfo info = mConfig;
  RefPtr<layers::ImageContainer> imageContainer = mImageContainer;
  RefPtr<layers::KnowsCompositor> knowsCompositor = mKnowsCompositor;
  return InvokeAsync(mGMPThread, __func__,
                     [cdm, config, info, imageContainer, knowsCompositor]() {
                       return cdm->InitializeVideoDecoder(
                           config, info, imageContainer, knowsCompositor);
                     });
}

nsCString ChromiumCDMVideoDecoder::GetDescriptionName() const {
  return "chromium cdm video decoder"_ns;
}

MediaDataDecoder::ConversionRequired ChromiumCDMVideoDecoder::NeedsConversion()
    const {
  return mConvertToAnnexB ? ConversionRequired::kNeedAnnexB
                          : ConversionRequired::kNeedNone;
}

RefPtr<MediaDataDecoder::DecodePromise> ChromiumCDMVideoDecoder::Decode(
    MediaRawData* aSample) {
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mGMPThread, __func__, [cdm, sample]() {
    return cdm->DecryptAndDecodeFrame(sample);
  });
}

RefPtr<MediaDataDecoder::FlushPromise> ChromiumCDMVideoDecoder::Flush() {
  MOZ_ASSERT(mCDMParent);
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  return InvokeAsync(mGMPThread, __func__,
                     [cdm]() { return cdm->FlushVideoDecoder(); });
}

RefPtr<MediaDataDecoder::DecodePromise> ChromiumCDMVideoDecoder::Drain() {
  MOZ_ASSERT(mCDMParent);
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  return InvokeAsync(mGMPThread, __func__, [cdm]() { return cdm->Drain(); });
}

RefPtr<ShutdownPromise> ChromiumCDMVideoDecoder::Shutdown() {
  if (!mCDMParent) {
    // Must have failed to get the CDMParent from the ChromiumCDMProxy
    // in our constructor; the MediaKeys must have shut down the CDM
    // before we had a chance to start up the decoder.
    return ShutdownPromise::CreateAndResolve(true, __func__);
  }
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDMParent;
  return InvokeAsync(mGMPThread, __func__,
                     [cdm]() { return cdm->ShutdownVideoDecoder(); });
}

}  // namespace mozilla
