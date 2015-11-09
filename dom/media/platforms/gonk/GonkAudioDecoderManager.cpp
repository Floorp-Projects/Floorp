/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaCodecProxy.h"
#include <OMX_IVCommon.h>
#include <gui/Surface.h>
#include <ICrypto.h>
#include "GonkAudioDecoderManager.h"
#include "MediaDecoderReader.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include "stagefright/MediaBuffer.h"
#include "stagefright/MetaData.h"
#include "stagefright/MediaErrors.h"
#include <stagefright/foundation/AMessage.h>
#include <stagefright/foundation/ALooper.h>
#include "media/openmax/OMX_Audio.h"
#include "MediaData.h"
#include "MediaInfo.h"

#define CODECCONFIG_TIMEOUT_US 10000LL
#define READ_OUTPUT_BUFFER_TIMEOUT_US  0LL

#include <android/log.h>
#define GADM_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "GonkAudioDecoderManager", __VA_ARGS__)

extern PRLogModuleInfo* GetPDMLog();
#define LOG(...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

using namespace android;
typedef android::MediaCodecProxy MediaCodecProxy;

namespace mozilla {

GonkAudioDecoderManager::GonkAudioDecoderManager(const AudioInfo& aConfig)
  : mAudioChannels(aConfig.mChannels)
  , mAudioRate(aConfig.mRate)
  , mAudioProfile(aConfig.mProfile)
  , mAudioCompactor(mAudioQueue)
{
  MOZ_COUNT_CTOR(GonkAudioDecoderManager);
  MOZ_ASSERT(mAudioChannels);
  mCodecSpecificData = aConfig.mCodecSpecificConfig;
  mMimeType = aConfig.mMimeType;
}

GonkAudioDecoderManager::~GonkAudioDecoderManager()
{
  MOZ_COUNT_DTOR(GonkAudioDecoderManager);
}

RefPtr<MediaDataDecoder::InitPromise>
GonkAudioDecoderManager::Init()
{
  if (InitMediaCodecProxy()) {
    return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
  } else {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }
}

bool
GonkAudioDecoderManager::InitMediaCodecProxy()
{
  status_t rv = OK;
  if (!InitLoopers(MediaData::AUDIO_DATA)) {
    return false;
  }

  mDecoder = MediaCodecProxy::CreateByType(mDecodeLooper, mMimeType.get(), false, nullptr);
  if (!mDecoder.get()) {
    return false;
  }
  if (!mDecoder->AllocateAudioMediaCodec())
  {
    mDecoder = nullptr;
    return false;
  }
  sp<AMessage> format = new AMessage;
  // Fixed values
  GADM_LOG("Configure audio mime type:%s, chan no:%d, sample-rate:%d, profile:%d",
           mMimeType.get(), mAudioChannels, mAudioRate, mAudioProfile);
  format->setString("mime", mMimeType.get());
  format->setInt32("channel-count", mAudioChannels);
  format->setInt32("sample-rate", mAudioRate);
  format->setInt32("aac-profile", mAudioProfile);
  status_t err = mDecoder->configure(format, nullptr, nullptr, 0);
  if (err != OK || !mDecoder->Prepare()) {
    return false;
  }

  if (mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    rv = mDecoder->Input(mCodecSpecificData->Elements(), mCodecSpecificData->Length(), 0,
                         android::MediaCodec::BUFFER_FLAG_CODECCONFIG,
                         CODECCONFIG_TIMEOUT_US);
  }

  if (rv == OK) {
    return true;
  } else {
    GADM_LOG("Failed to input codec specific data!");
    return false;
  }
}

nsresult
GonkAudioDecoderManager::CreateAudioData(MediaBuffer* aBuffer, int64_t aStreamOffset)
{
  if (!(aBuffer != nullptr && aBuffer->data() != nullptr)) {
    GADM_LOG("Audio Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  int64_t timeUs;
  if (!aBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aBuffer->range_length() == 0) {
    // Some decoders may return spurious empty buffers that we just want to ignore
    // quoted from Android's AwesomePlayer.cpp
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mLastTime > timeUs) {
    GADM_LOG("Output decoded sample time is revert. time=%lld", timeUs);
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_AVAILABLE;
  }
  mLastTime = timeUs;

  const uint8_t *data = static_cast<const uint8_t*>(aBuffer->data());
  size_t dataOffset = aBuffer->range_offset();
  size_t size = aBuffer->range_length();

  uint32_t frames = size / (2 * mAudioChannels);

  CheckedInt64 duration = FramesToUsecs(frames, mAudioRate);
  if (!duration.isValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  typedef AudioCompactor::NativeCopy OmxCopy;
  mAudioCompactor.Push(aStreamOffset,
                       timeUs,
                       mAudioRate,
                       frames,
                       mAudioChannels,
                       OmxCopy(data+dataOffset,
                               size,
                               mAudioChannels));
  return NS_OK;
}

nsresult
GonkAudioDecoderManager::Output(int64_t aStreamOffset,
                                RefPtr<MediaData>& aOutData)
{
  aOutData = nullptr;
  if (mAudioQueue.GetSize() > 0) {
    aOutData = mAudioQueue.PopFront();
    return mAudioQueue.AtEndOfStream() ? NS_ERROR_ABORT : NS_OK;
  }

  status_t err;
  MediaBuffer* audioBuffer = nullptr;
  err = mDecoder->Output(&audioBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);
  AutoReleaseMediaBuffer a(audioBuffer, mDecoder.get());

  switch (err) {
    case OK:
    {
      nsresult rv = CreateAudioData(audioBuffer, aStreamOffset);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case android::INFO_FORMAT_CHANGED:
    {
      // If the format changed, update our cached info.
      GADM_LOG("Decoder format changed");
      sp<AMessage> audioCodecFormat;

      if (mDecoder->getOutputFormat(&audioCodecFormat) != OK ||
        audioCodecFormat == nullptr) {
        return NS_ERROR_UNEXPECTED;
      }

      int32_t codec_channel_count = 0;
      int32_t codec_sample_rate = 0;

      if (!audioCodecFormat->findInt32("channel-count", &codec_channel_count) ||
        !audioCodecFormat->findInt32("sample-rate", &codec_sample_rate)) {
        return NS_ERROR_UNEXPECTED;
      }

      // Update AudioInfo
      mAudioChannels = codec_channel_count;
      mAudioRate = codec_sample_rate;

      return Output(aStreamOffset, aOutData);
    }
    case android::INFO_OUTPUT_BUFFERS_CHANGED:
    {
      GADM_LOG("Info Output Buffers Changed");
      if (mDecoder->UpdateOutputBuffers()) {
        return Output(aStreamOffset, aOutData);
      }
      return NS_ERROR_FAILURE;
    }
    case -EAGAIN:
    {
      return NS_ERROR_NOT_AVAILABLE;
    }
    case android::ERROR_END_OF_STREAM:
    {
      GADM_LOG("Got EOS frame!");
      nsresult rv = CreateAudioData(audioBuffer, aStreamOffset);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_ABORT);
      MOZ_ASSERT(mAudioQueue.GetSize() > 0);
      mAudioQueue.Finish();
      break;
    }
    case -ETIMEDOUT:
    {
      GADM_LOG("Timeout. can try again next time");
      return NS_ERROR_UNEXPECTED;
    }
    default:
    {
      GADM_LOG("Decoder failed, err=%d", err);
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mAudioQueue.GetSize() > 0) {
    aOutData = mAudioQueue.PopFront();
    // Return NS_ERROR_ABORT at the last sample.
    return mAudioQueue.AtEndOfStream() ? NS_ERROR_ABORT : NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

void
GonkAudioDecoderManager::ProcessFlush()
{
  GADM_LOG("FLUSH<<<");
  mAudioQueue.Reset();
  GADM_LOG(">>>FLUSH");
  GonkDecoderManager::ProcessFlush();
}

} // namespace mozilla
