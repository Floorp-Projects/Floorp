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
#include "mp4_demuxer/Adts.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "prlog.h"
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

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif
#define READ_OUTPUT_BUFFER_TIMEOUT_US  3000

using namespace android;
typedef android::MediaCodecProxy MediaCodecProxy;

namespace mozilla {

GonkAudioDecoderManager::GonkAudioDecoderManager(
  MediaTaskQueue* aTaskQueue,
  const AudioInfo& aConfig)
  : GonkDecoderManager(aTaskQueue)
  , mAudioChannels(aConfig.mChannels)
  , mAudioRate(aConfig.mRate)
  , mAudioProfile(aConfig.mProfile)
  , mUseAdts(true)
  , mAudioBuffer(nullptr)
{
  MOZ_COUNT_CTOR(GonkAudioDecoderManager);
  MOZ_ASSERT(mAudioChannels);
  mUserData.AppendElements(aConfig.mCodecSpecificConfig->Elements(),
                           aConfig.mCodecSpecificConfig->Length());
  // Pass through mp3 without applying an ADTS header.
  if (!aConfig.mMimeType.EqualsLiteral("audio/mp4a-latm")) {
      mUseAdts = false;
  }
}

GonkAudioDecoderManager::~GonkAudioDecoderManager()
{
  MOZ_COUNT_DTOR(GonkAudioDecoderManager);
}

android::sp<MediaCodecProxy>
GonkAudioDecoderManager::Init(MediaDataDecoderCallback* aCallback)
{
  if (mLooper != nullptr) {
    return nullptr;
  }
  // Create ALooper
  mLooper = new ALooper;
  mLooper->setName("GonkAudioDecoderManager");
  mLooper->start();

  mDecoder = MediaCodecProxy::CreateByType(mLooper, "audio/mp4a-latm", false, nullptr);
  if (!mDecoder.get()) {
    return nullptr;
  }
  if (!mDecoder->AskMediaCodecAndWait())
  {
    mDecoder = nullptr;
    return nullptr;
  }
  sp<AMessage> format = new AMessage;
  // Fixed values
  GADM_LOG("Init Audio channel no:%d, sample-rate:%d", mAudioChannels, mAudioRate);
  format->setString("mime", "audio/mp4a-latm");
  format->setInt32("channel-count", mAudioChannels);
  format->setInt32("sample-rate", mAudioRate);
  format->setInt32("aac-profile", mAudioProfile);
  format->setInt32("is-adts", true);
  status_t err = mDecoder->configure(format, nullptr, nullptr, 0);
  if (err != OK || !mDecoder->Prepare()) {
    return nullptr;
  }
  status_t rv = mDecoder->Input(mUserData.Elements(), mUserData.Length(), 0,
                                android::MediaCodec::BUFFER_FLAG_CODECCONFIG);

  if (rv == OK) {
    return mDecoder;
  } else {
    GADM_LOG("Failed to input codec specific data!");
    return nullptr;
  }
}

status_t
GonkAudioDecoderManager::SendSampleToOMX(MediaRawData* aSample)
{
  return mDecoder->Input(reinterpret_cast<const uint8_t*>(aSample->mData),
                         aSample->mSize,
                         aSample->mTime,
                         0);
}

bool
GonkAudioDecoderManager::PerformFormatSpecificProcess(MediaRawData* aSample)
{
  if (aSample && mUseAdts) {
    int8_t frequency_index =
        mp4_demuxer::Adts::GetFrequencyIndex(mAudioRate);
    bool rv = mp4_demuxer::Adts::ConvertSample(mAudioChannels,
                                               frequency_index,
                                               mAudioProfile,
                                               aSample);
    if (!rv) {
      GADM_LOG("Failed to apply ADTS header");
      return false;
    }
  }

  return true;
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

nsresult
GonkAudioDecoderManager::Flush()
{
  GonkDecoderManager::Flush();
  status_t err = mDecoder->flush();
  if (err != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}
} // namespace mozilla
