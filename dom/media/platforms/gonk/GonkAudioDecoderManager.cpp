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

#include <android/log.h>
#define GADM_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "GonkAudioDecoderManager", __VA_ARGS__)

extern PRLogModuleInfo* GetPDMLog();
#define LOG(...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#define READ_OUTPUT_BUFFER_TIMEOUT_US  3000

using namespace android;
typedef android::MediaCodecProxy MediaCodecProxy;

namespace mozilla {

GonkAudioDecoderManager::GonkAudioDecoderManager(const AudioInfo& aConfig)
  : mLastDecodedTime(0)
  , mAudioChannels(aConfig.mChannels)
  , mAudioRate(aConfig.mRate)
  , mAudioProfile(aConfig.mProfile)
  , mAudioBuffer(nullptr)
  , mMonitor("GonkAudioDecoderManager")
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

nsRefPtr<MediaDataDecoder::InitPromise>
GonkAudioDecoderManager::Init(MediaDataDecoderCallback* aCallback)
{
  if (InitMediaCodecProxy(aCallback)) {
    return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
  } else {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }
}

bool
GonkAudioDecoderManager::InitMediaCodecProxy(MediaDataDecoderCallback* aCallback)
{
  status_t rv = OK;
  if (mLooper != nullptr) {
    return false;
  }
  // Create ALooper
  mLooper = new ALooper;
  mLooper->setName("GonkAudioDecoderManager");
  mLooper->start();

  mDecoder = MediaCodecProxy::CreateByType(mLooper, mMimeType.get(), false, nullptr);
  if (!mDecoder.get()) {
    return false;
  }
  if (!mDecoder->AskMediaCodecAndWait())
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
                         android::MediaCodec::BUFFER_FLAG_CODECCONFIG);
  }

  if (rv == OK) {
    return true;
  } else {
    GADM_LOG("Failed to input codec specific data!");
    return false;
  }
}

bool
GonkAudioDecoderManager::HasQueuedSample()
{
    MonitorAutoLock mon(mMonitor);
    return mQueueSample.Length();
}

nsresult
GonkAudioDecoderManager::Input(MediaRawData* aSample)
{
  MonitorAutoLock mon(mMonitor);
  nsRefPtr<MediaRawData> sample;

  if (aSample) {
    sample = aSample;
  } else {
    // It means EOS with empty sample.
    sample = new MediaRawData();
  }

  mQueueSample.AppendElement(sample);

  status_t rv;
  while (mQueueSample.Length()) {
    nsRefPtr<MediaRawData> data = mQueueSample.ElementAt(0);
    {
      MonitorAutoUnlock mon_exit(mMonitor);
      rv = mDecoder->Input(reinterpret_cast<const uint8_t*>(data->Data()),
                           data->Size(),
                           data->mTime,
                           0);
    }
    if (rv == OK) {
      mQueueSample.RemoveElementAt(0);
    } else if (rv == -EAGAIN || rv == -ETIMEDOUT) {
      // In most cases, EAGAIN or ETIMEOUT are safe because OMX can't fill
      // buffer on time.
      return NS_OK;
    } else {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

nsresult
GonkAudioDecoderManager::CreateAudioData(int64_t aStreamOffset, AudioData **v) {
  if (!(mAudioBuffer != nullptr && mAudioBuffer->data() != nullptr)) {
    GADM_LOG("Audio Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  int64_t timeUs;
  if (!mAudioBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mAudioBuffer->range_length() == 0) {
    // Some decoders may return spurious empty buffers that we just want to ignore
    // quoted from Android's AwesomePlayer.cpp
    ReleaseAudioBuffer();
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mLastDecodedTime > timeUs) {
    ReleaseAudioBuffer();
    GADM_LOG("Output decoded sample time is revert. time=%lld", timeUs);
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_AVAILABLE;
  }
  mLastDecodedTime = timeUs;

  const uint8_t *data = static_cast<const uint8_t*>(mAudioBuffer->data());
  size_t dataOffset = mAudioBuffer->range_offset();
  size_t size = mAudioBuffer->range_length();

  nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[size/2]);
  memcpy(buffer.get(), data+dataOffset, size);
  uint32_t frames = size / (2 * mAudioChannels);

  CheckedInt64 duration = FramesToUsecs(frames, mAudioRate);
  if (!duration.isValid()) {
    return NS_ERROR_UNEXPECTED;
  }
  nsRefPtr<AudioData> audioData = new AudioData(aStreamOffset,
                                                timeUs,
                                                duration.value(),
                                                frames,
                                                buffer.forget(),
                                                mAudioChannels,
                                                mAudioRate);
  ReleaseAudioBuffer();
  audioData.forget(v);
  return NS_OK;
}

nsresult
GonkAudioDecoderManager::Flush()
{
  {
    MonitorAutoLock mon(mMonitor);
    mQueueSample.Clear();
  }

  mLastDecodedTime = 0;

  if (mDecoder->flush() != OK) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
GonkAudioDecoderManager::Output(int64_t aStreamOffset,
                                nsRefPtr<MediaData>& aOutData)
{
  aOutData = nullptr;
  status_t err;
  err = mDecoder->Output(&mAudioBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);

  switch (err) {
    case OK:
    {
      nsRefPtr<AudioData> data;
      nsresult rv = CreateAudioData(aStreamOffset, getter_AddRefs(data));
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // Decoder outputs an empty video buffer, try again
        return NS_ERROR_NOT_AVAILABLE;
      } else if (rv != NS_OK || data == nullptr) {
        return NS_ERROR_UNEXPECTED;
      }
      aOutData = data;
      return NS_OK;
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
      nsRefPtr<AudioData> data;
      nsresult rv = CreateAudioData(aStreamOffset, getter_AddRefs(data));
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // For EOS, no need to do any thing.
        return NS_ERROR_ABORT;
      } else if (rv != NS_OK || data == nullptr) {
        GADM_LOG("Failed to create audio data!");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData = data;
      return NS_ERROR_ABORT;
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

  return NS_OK;
}

void GonkAudioDecoderManager::ReleaseAudioBuffer() {
  if (mAudioBuffer) {
    mDecoder->ReleaseMediaBuffer(mAudioBuffer);
    mAudioBuffer = nullptr;
  }
}

} // namespace mozilla
