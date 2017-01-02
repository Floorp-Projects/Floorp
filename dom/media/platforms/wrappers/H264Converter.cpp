/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include "H264Converter.h"
#include "ImageContainer.h"
#include "MediaInfo.h"
#include "MediaPrefs.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/H264.h"

namespace mozilla
{

H264Converter::H264Converter(PlatformDecoderModule* aPDM,
                             const CreateDecoderParams& aParams)
  : mPDM(aPDM)
  , mCurrentConfig(aParams.VideoConfig())
  , mKnowsCompositor(aParams.mKnowsCompositor)
  , mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mCallback(aParams.mCallback)
  , mDecoder(nullptr)
  , mGMPCrashHelper(aParams.mCrashHelper)
  , mNeedAVCC(aPDM->DecoderNeedsConversion(aParams.mConfig)
      == PlatformDecoderModule::ConversionRequired::kNeedAVCC)
  , mLastError(NS_OK)
{
  CreateDecoder(aParams.mDiagnostics);
}

H264Converter::~H264Converter()
{
}

RefPtr<MediaDataDecoder::InitPromise>
H264Converter::Init()
{
  if (mDecoder) {
    return mDecoder->Init();
  }

  // We haven't been able to initialize a decoder due to a missing SPS/PPS.
  return MediaDataDecoder::InitPromise::CreateAndResolve(
           TrackType::kVideoTrack, __func__);
}

void
H264Converter::Input(MediaRawData* aSample)
{
  if (!mp4_demuxer::AnnexB::ConvertSampleToAVCC(aSample)) {
    // We need AVCC content to be able to later parse the SPS.
    // This is a no-op if the data is already AVCC.
    mCallback->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY,
                                 RESULT_DETAIL("ConvertSampleToAVCC")));
    return;
  }

  if (mInitPromiseRequest.Exists()) {
    if (mNeedKeyframe) {
      if (!aSample->mKeyframe) {
        // Frames dropped, we need a new one.
        mCallback->InputExhausted();
        return;
      }
      mNeedKeyframe = false;
    }
    mMediaRawSamples.AppendElement(aSample);
    return;
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
      mCallback->InputExhausted();
      return;
    }
  } else {
    rv = CheckForSPSChange(aSample);
    if (rv == NS_ERROR_NOT_INITIALIZED) {
      // The decoder is pending initialization.
      mCallback->InputExhausted();
      return;
    }
  }
  if (NS_FAILED(rv)) {
    mCallback->Error(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Unable to create H264 decoder")));
    return;
  }

  if (mNeedKeyframe && !aSample->mKeyframe) {
    mCallback->InputExhausted();
    return;
  }

  if (!mNeedAVCC &&
      !mp4_demuxer::AnnexB::ConvertSampleToAnnexB(aSample, mNeedKeyframe)) {
    mCallback->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY,
                                 RESULT_DETAIL("ConvertSampleToAnnexB")));
    return;
  }

  mNeedKeyframe = false;

  aSample->mExtraData = mCurrentConfig.mExtraData;

  mDecoder->Input(aSample);
}

void
H264Converter::Flush()
{
  mNeedKeyframe = true;
  if (mDecoder) {
    mDecoder->Flush();
  }
}

void
H264Converter::Drain()
{
  mNeedKeyframe = true;
  if (mDecoder) {
    mDecoder->Drain();
    return;
  }
  mCallback->DrainComplete();
}

void
H264Converter::Shutdown()
{
  if (mDecoder) {
    mDecoder->Shutdown();
    mInitPromiseRequest.DisconnectIfExists();
    mDecoder = nullptr;
  }
}

bool
H264Converter::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  if (mDecoder) {
    return mDecoder->IsHardwareAccelerated(aFailureReason);
  }
  return MediaDataDecoder::IsHardwareAccelerated(aFailureReason);
}

void
H264Converter::SetSeekThreshold(const media::TimeUnit& aTime)
{
  if (mDecoder) {
    mDecoder->SetSeekThreshold(aTime);
  } else {
    MediaDataDecoder::SetSeekThreshold(aTime);
  }
}

nsresult
H264Converter::CreateDecoder(DecoderDoctorDiagnostics* aDiagnostics)
{
  if (!mp4_demuxer::AnnexB::HasSPS(mCurrentConfig.mExtraData)) {
    // nothing found yet, will try again later
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(mCurrentConfig.mExtraData);

  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(mCurrentConfig.mExtraData, spsdata)) {
    // Do some format check here.
    // WMF H.264 Video Decoder and Apple ATDecoder do not support YUV444 format.
    if (spsdata.profile_idc == 244 /* Hi444PP */ ||
        spsdata.chroma_format_idc == PDMFactory::kYUV444) {
      mLastError = NS_ERROR_FAILURE;
      if (aDiagnostics) {
        aDiagnostics->SetVideoNotSupported();
      }
      return NS_ERROR_FAILURE;
    }
  } else {
    // SPS was invalid.
    mLastError = NS_ERROR_FAILURE;
    return NS_ERROR_FAILURE;
  }

  mDecoder = mPDM->CreateVideoDecoder({
    mCurrentConfig,
    mTaskQueue,
    mCallback,
    aDiagnostics,
    mImageContainer,
    mKnowsCompositor,
    mGMPCrashHelper
  });

  if (!mDecoder) {
    mLastError = NS_ERROR_FAILURE;
    return NS_ERROR_FAILURE;
  }

  mNeedKeyframe = true;

  return NS_OK;
}

nsresult
H264Converter::CreateDecoderAndInit(MediaRawData* aSample)
{
  RefPtr<MediaByteBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(extra_data);

  nsresult rv = CreateDecoder(/* DecoderDoctorDiagnostics* */ nullptr);

  if (NS_SUCCEEDED(rv)) {
    // Queue the incoming sample.
    mMediaRawSamples.AppendElement(aSample);

    mInitPromiseRequest.Begin(mDecoder->Init()
      ->Then(AbstractThread::GetCurrent()->AsTaskQueue(), __func__, this,
             &H264Converter::OnDecoderInitDone,
             &H264Converter::OnDecoderInitFailed));
    return NS_ERROR_NOT_INITIALIZED;
  }
  return rv;
}

void
H264Converter::OnDecoderInitDone(const TrackType aTrackType)
{
  mInitPromiseRequest.Complete();
  bool gotInput = false;
  for (uint32_t i = 0 ; i < mMediaRawSamples.Length(); i++) {
    const RefPtr<MediaRawData>& sample = mMediaRawSamples[i];
    if (mNeedKeyframe) {
      if (!sample->mKeyframe) {
        continue;
      }
      mNeedKeyframe = false;
    }
    if (!mNeedAVCC &&
        !mp4_demuxer::AnnexB::ConvertSampleToAnnexB(sample, mNeedKeyframe)) {
      mCallback->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY,
                                   RESULT_DETAIL("ConvertSampleToAnnexB")));
      mMediaRawSamples.Clear();
      return;
    }
    mDecoder->Input(sample);
  }
  if (!gotInput) {
    mCallback->InputExhausted();
  }
  mMediaRawSamples.Clear();
}

void
H264Converter::OnDecoderInitFailed(MediaResult aError)
{
  mInitPromiseRequest.Complete();
  mCallback->Error(
    MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                RESULT_DETAIL("Unable to initialize H264 decoder")));
}

nsresult
H264Converter::CheckForSPSChange(MediaRawData* aSample)
{
  RefPtr<MediaByteBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data) ||
      mp4_demuxer::AnnexB::CompareExtraData(extra_data,
                                            mCurrentConfig.mExtraData)) {
        return NS_OK;
      }

  if (MediaPrefs::MediaDecoderCheckRecycling() &&
      mDecoder->SupportDecoderRecycling()) {
    // Do not recreate the decoder, reuse it.
    UpdateConfigFromExtraData(extra_data);
    mNeedKeyframe = true;
    return NS_OK;
  }
  // The SPS has changed, signal to flush the current decoder and create a
  // new one.
  mDecoder->Flush();
  Shutdown();
  return CreateDecoderAndInit(aSample);
}

void
H264Converter::UpdateConfigFromExtraData(MediaByteBuffer* aExtraData)
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

} // namespace mozilla
