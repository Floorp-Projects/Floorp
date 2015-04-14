/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "H264Converter.h"
#include "ImageContainer.h"
#include "MediaTaskQueue.h"
#include "MediaInfo.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/H264.h"

namespace mozilla
{

H264Converter::H264Converter(PlatformDecoderModule* aPDM,
                             const VideoInfo& aConfig,
                             layers::LayersBackend aLayersBackend,
                             layers::ImageContainer* aImageContainer,
                             FlushableMediaTaskQueue* aVideoTaskQueue,
                             MediaDataDecoderCallback* aCallback)
  : mPDM(aPDM)
  , mCurrentConfig(aConfig)
  , mLayersBackend(aLayersBackend)
  , mImageContainer(aImageContainer)
  , mVideoTaskQueue(aVideoTaskQueue)
  , mCallback(aCallback)
  , mDecoder(nullptr)
  , mNeedAVCC(aPDM->DecoderNeedsConversion(aConfig) == PlatformDecoderModule::kNeedAVCC)
  , mLastError(NS_OK)
{
  CreateDecoder();
}

H264Converter::~H264Converter()
{
}

nsresult
H264Converter::Init()
{
  if (mDecoder) {
    return mDecoder->Init();
  }
  return mLastError;
}

nsresult
H264Converter::Input(MediaRawData* aSample)
{
  if (!mNeedAVCC) {
    if (!mp4_demuxer::AnnexB::ConvertSampleToAnnexB(aSample)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    if (!mp4_demuxer::AnnexB::ConvertSampleToAVCC(aSample)) {
      return NS_ERROR_FAILURE;
    }
  }
  nsresult rv;
  if (!mDecoder) {
    // It is not possible to create an AVCC H264 decoder without SPS.
    // As such, creation will fail if the extra_data just extracted doesn't
    // contain a SPS.
    rv = CreateDecoderAndInit(aSample);
    if (rv == NS_ERROR_NOT_INITIALIZED) {
      // We are missing the required SPS to create the decoder.
      // Ignore for the time being, the MediaRawData will be dropped.
      return NS_OK;
    }
  } else {
    rv = CheckForSPSChange(aSample);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  aSample->mExtraData = mCurrentConfig.mExtraData;

  return mDecoder->Input(aSample);
}

nsresult
H264Converter::Flush()
{
  if (mDecoder) {
    return mDecoder->Flush();
  }
  return mLastError;
}

nsresult
H264Converter::Drain()
{
  if (mDecoder) {
    return mDecoder->Drain();
  }
  return mLastError;
}

nsresult
H264Converter::Shutdown()
{
  if (mDecoder) {
    nsresult rv = mDecoder->Shutdown();
    mDecoder = nullptr;
    return rv;
  }
  return NS_OK;
}

bool
H264Converter::IsWaitingMediaResources()
{
  if (mDecoder) {
    return mDecoder->IsWaitingMediaResources();
  }
  return MediaDataDecoder::IsWaitingMediaResources();
}

bool
H264Converter::IsHardwareAccelerated() const
{
  if (mDecoder) {
    return mDecoder->IsHardwareAccelerated();
  }
  return MediaDataDecoder::IsHardwareAccelerated();
}

nsresult
H264Converter::CreateDecoder()
{
  if (mNeedAVCC && !mp4_demuxer::AnnexB::HasSPS(mCurrentConfig.mExtraData)) {
    // nothing found yet, will try again later
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(mCurrentConfig.mExtraData);

  mDecoder = mPDM->CreateVideoDecoder(mCurrentConfig,
                                      mLayersBackend,
                                      mImageContainer,
                                      mVideoTaskQueue,
                                      mCallback);
  if (!mDecoder) {
    mLastError = NS_ERROR_FAILURE;
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
H264Converter::CreateDecoderAndInit(MediaRawData* aSample)
{
  nsRefPtr<DataBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(extra_data);

  nsresult rv = CreateDecoder();
  NS_ENSURE_SUCCESS(rv, rv);
  return Init();
}

nsresult
H264Converter::CheckForSPSChange(MediaRawData* aSample)
{
  nsRefPtr<DataBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data) ||
      mp4_demuxer::AnnexB::CompareExtraData(extra_data,
                                            mCurrentConfig.mExtraData)) {
        return NS_OK;
      }
  if (!mNeedAVCC) {
    UpdateConfigFromExtraData(extra_data);
    mDecoder->ConfigurationChanged(mCurrentConfig);
    return NS_OK;
  }
  // The SPS has changed, signal to flush the current decoder and create a
  // new one.
  mDecoder->Flush();
  Shutdown();
  return CreateDecoderAndInit(aSample);
}

void
H264Converter::UpdateConfigFromExtraData(DataBuffer* aExtraData)
{
  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(aExtraData, spsdata) &&
      spsdata.pic_width > 0 && spsdata.pic_height > 0) {
    mp4_demuxer::H264::EnsureSPSIsSane(spsdata);
    mCurrentConfig.mImage.width = spsdata.pic_width;
    mCurrentConfig.mImage.height = spsdata.pic_height;
    mCurrentConfig.mDisplay.width = spsdata.display_width;
    mCurrentConfig.mDisplay.height = spsdata.display_height;
  }
  mCurrentConfig.mExtraData = aExtraData;
}

/* static */
bool
H264Converter::IsH264(const TrackInfo& aConfig)
{
  return aConfig.mMimeType.EqualsLiteral("video/avc") ||
    aConfig.mMimeType.EqualsLiteral("video/mp4");
}

} // namespace mozilla
