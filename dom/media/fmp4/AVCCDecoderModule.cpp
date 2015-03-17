/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AVCCDecoderModule.h"
#include "ImageContainer.h"
#include "MediaTaskQueue.h"
#include "mp4_demuxer/DecoderData.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/H264.h"

namespace mozilla
{

class AVCCMediaDataDecoder : public MediaDataDecoder {
public:

  AVCCMediaDataDecoder(PlatformDecoderModule* aPDM,
                       const mp4_demuxer::VideoDecoderConfig& aConfig,
                       layers::LayersBackend aLayersBackend,
                       layers::ImageContainer* aImageContainer,
                       FlushableMediaTaskQueue* aVideoTaskQueue,
                       MediaDataDecoderCallback* aCallback);

  virtual ~AVCCMediaDataDecoder();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;
  virtual bool IsWaitingMediaResources() MOZ_OVERRIDE;
  virtual bool IsDormantNeeded() MOZ_OVERRIDE;
  virtual void AllocateMediaResources() MOZ_OVERRIDE;
  virtual void ReleaseMediaResources() MOZ_OVERRIDE;
  virtual bool IsHardwareAccelerated() const MOZ_OVERRIDE;

private:
  // Will create the required MediaDataDecoder if we have a AVC SPS.
  // Returns NS_ERROR_FAILURE if error is permanent and can't be recovered and
  // will set mError accordingly.
  nsresult CreateDecoder();
  nsresult CreateDecoderAndInit(mp4_demuxer::MP4Sample* aSample);
  nsresult CheckForSPSChange(mp4_demuxer::MP4Sample* aSample);
  void UpdateConfigFromExtraData(mp4_demuxer::ByteBuffer* aExtraData);

  nsRefPtr<PlatformDecoderModule> mPDM;
  mp4_demuxer::VideoDecoderConfig mCurrentConfig;
  layers::LayersBackend mLayersBackend;
  nsRefPtr<layers::ImageContainer> mImageContainer;
  nsRefPtr<FlushableMediaTaskQueue> mVideoTaskQueue;
  MediaDataDecoderCallback* mCallback;
  nsRefPtr<MediaDataDecoder> mDecoder;
  nsresult mLastError;
};

AVCCMediaDataDecoder::AVCCMediaDataDecoder(PlatformDecoderModule* aPDM,
                                           const mp4_demuxer::VideoDecoderConfig& aConfig,
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
  , mLastError(NS_OK)
{
  CreateDecoder();
}

AVCCMediaDataDecoder::~AVCCMediaDataDecoder()
{
}

nsresult
AVCCMediaDataDecoder::Init()
{
  if (mDecoder) {
    return mDecoder->Init();
  }
  return mLastError;
}

nsresult
AVCCMediaDataDecoder::Input(mp4_demuxer::MP4Sample* aSample)
{
  if (!mp4_demuxer::AnnexB::ConvertSampleToAVCC(aSample)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv;
  if (!mDecoder) {
    // It is not possible to create an AVCC H264 decoder without SPS.
    // As such, creation will fail if the extra_data just extracted doesn't
    // contain a SPS.
    rv = CreateDecoderAndInit(aSample);
    if (rv == NS_ERROR_NOT_INITIALIZED) {
      // We are missing the required SPS to create the decoder.
      // Ignore for the time being, the MP4Sample will be dropped.
      return NS_OK;
    }
  } else {
    rv = CheckForSPSChange(aSample);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  aSample->extra_data = mCurrentConfig.extra_data;

  return mDecoder->Input(aSample);
}

nsresult
AVCCMediaDataDecoder::Flush()
{
  if (mDecoder) {
    return mDecoder->Flush();
  }
  return mLastError;
}

nsresult
AVCCMediaDataDecoder::Drain()
{
  if (mDecoder) {
    return mDecoder->Drain();
  }
  return mLastError;
}

nsresult
AVCCMediaDataDecoder::Shutdown()
{
  if (mDecoder) {
    nsresult rv = mDecoder->Shutdown();
    mDecoder = nullptr;
    return rv;
  }
  return NS_OK;
}

bool
AVCCMediaDataDecoder::IsWaitingMediaResources()
{
  if (mDecoder) {
    return mDecoder->IsWaitingMediaResources();
  }
  return MediaDataDecoder::IsWaitingMediaResources();
}

bool
AVCCMediaDataDecoder::IsDormantNeeded()
{
  return true;
}

void
AVCCMediaDataDecoder::AllocateMediaResources()
{
  // Nothing to do, decoder will be allocated on the fly when required.
}

void
AVCCMediaDataDecoder::ReleaseMediaResources()
{
  Shutdown();
}

nsresult
AVCCMediaDataDecoder::CreateDecoder()
{
  if (!mp4_demuxer::AnnexB::HasSPS(mCurrentConfig.extra_data)) {
    // nothing found yet, will try again later
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(mCurrentConfig.extra_data);

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
AVCCMediaDataDecoder::CreateDecoderAndInit(mp4_demuxer::MP4Sample* aSample)
{
  nsRefPtr<mp4_demuxer::ByteBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(extra_data);

  nsresult rv = CreateDecoder();
  NS_ENSURE_SUCCESS(rv, rv);
  return Init();
}

bool
AVCCMediaDataDecoder::IsHardwareAccelerated() const
{
  if (mDecoder) {
    return mDecoder->IsHardwareAccelerated();
  }
  return MediaDataDecoder::IsHardwareAccelerated();
}

nsresult
AVCCMediaDataDecoder::CheckForSPSChange(mp4_demuxer::MP4Sample* aSample)
{
  nsRefPtr<mp4_demuxer::ByteBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data) ||
      mp4_demuxer::AnnexB::CompareExtraData(extra_data,
                                            mCurrentConfig.extra_data)) {
    return NS_OK;
  }
  // The SPS has changed, signal to flush the current decoder and create a
  // new one.
  mDecoder->Flush();
  ReleaseMediaResources();
  return CreateDecoderAndInit(aSample);
}

void
AVCCMediaDataDecoder::UpdateConfigFromExtraData(mp4_demuxer::ByteBuffer* aExtraData)
{
  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(aExtraData, spsdata) &&
      spsdata.pic_width > 0 && spsdata.pic_height > 0) {
    mCurrentConfig.image_width = spsdata.pic_width;
    mCurrentConfig.image_height = spsdata.pic_height;
    mCurrentConfig.display_width = spsdata.display_width;
    mCurrentConfig.display_height = spsdata.display_height;
  }
  mCurrentConfig.extra_data = aExtraData;
}

// AVCCDecoderModule

AVCCDecoderModule::AVCCDecoderModule(PlatformDecoderModule* aPDM)
: mPDM(aPDM)
{
  MOZ_ASSERT(aPDM);
}

AVCCDecoderModule::~AVCCDecoderModule()
{
}

nsresult
AVCCDecoderModule::Startup()
{
  return mPDM->Startup();
}

nsresult
AVCCDecoderModule::Shutdown()
{
  return mPDM->Shutdown();
}

already_AddRefed<MediaDataDecoder>
AVCCDecoderModule::CreateVideoDecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                      layers::LayersBackend aLayersBackend,
                                      layers::ImageContainer* aImageContainer,
                                      FlushableMediaTaskQueue* aVideoTaskQueue,
                                      MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder;

  if ((strcmp(aConfig.mime_type, "video/avc") &&
       strcmp(aConfig.mime_type, "video/mp4")) ||
      !mPDM->DecoderNeedsAVCC(aConfig)) {
    // There is no need for an AVCC wrapper for non-AVC content.
    decoder = mPDM->CreateVideoDecoder(aConfig,
                                       aLayersBackend,
                                       aImageContainer,
                                       aVideoTaskQueue,
                                       aCallback);
  } else {
    decoder = new AVCCMediaDataDecoder(mPDM,
                                       aConfig,
                                       aLayersBackend,
                                       aImageContainer,
                                       aVideoTaskQueue,
                                       aCallback);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
AVCCDecoderModule::CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                                      FlushableMediaTaskQueue* aAudioTaskQueue,
                                      MediaDataDecoderCallback* aCallback)
{
  return mPDM->CreateAudioDecoder(aConfig,
                                  aAudioTaskQueue,
                                  aCallback);
}

bool
AVCCDecoderModule::SupportsAudioMimeType(const char* aMimeType)
{
  return mPDM->SupportsAudioMimeType(aMimeType);
}

bool
AVCCDecoderModule::SupportsVideoMimeType(const char* aMimeType)
{
  return mPDM->SupportsVideoMimeType(aMimeType);
}

} // namespace mozilla
