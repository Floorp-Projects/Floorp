/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GonkMediaDataDecoder.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "MediaCodecProxy.h"

#include <stagefright/foundation/ADebug.h>

#include "mozilla/Logging.h"
#include <android/log.h>
#define GMDD_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "GonkMediaDataDecoder", __VA_ARGS__)

extern PRLogModuleInfo* GetPDMLog();
#define LOG(...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

using namespace android;

namespace mozilla {

bool
GonkDecoderManager::InitLoopers(MediaData::Type aType)
{
  MOZ_ASSERT(mDecodeLooper.get() == nullptr && mTaskLooper.get() == nullptr);
  MOZ_ASSERT(aType == MediaData::VIDEO_DATA || aType == MediaData::AUDIO_DATA);

  const char* suffix = (aType == MediaData::VIDEO_DATA ? "video" : "audio");
  mDecodeLooper = new ALooper;
  android::AString name("MediaCodecProxy/");
  name.append(suffix);
  mDecodeLooper->setName(name.c_str());

  mTaskLooper = new ALooper;
  name.setTo("GonkDecoderManager/");
  name.append(suffix);
  mTaskLooper->setName(name.c_str());
  mTaskLooper->registerHandler(this);

  return mDecodeLooper->start() == OK && mTaskLooper->start() == OK;
}

nsresult
GonkDecoderManager::Input(MediaRawData* aSample)
{
  MutexAutoLock lock(mMutex);
  nsRefPtr<MediaRawData> sample;

  if (!aSample) {
    // It means EOS with empty sample.
    sample = new MediaRawData();
  } else {
    sample = aSample;
  }

  mQueuedSamples.AppendElement(sample);

  status_t rv;
  while (mQueuedSamples.Length()) {
    nsRefPtr<MediaRawData> data = mQueuedSamples.ElementAt(0);
    {
      MutexAutoUnlock unlock(mMutex);
      rv = mDecoder->Input(reinterpret_cast<const uint8_t*>(data->Data()),
                           data->Size(),
                           data->mTime,
                           0);
    }
    if (rv == OK) {
      mQueuedSamples.RemoveElementAt(0);
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
GonkDecoderManager::Flush()
{
  if (mDecoder == nullptr) {
    GMDD_LOG("Decoder is not initialized");
    return NS_ERROR_UNEXPECTED;
  }
  {
    MutexAutoLock lock(mMutex);
    mQueuedSamples.Clear();
  }

  mLastTime = 0;

  if (mDecoder->flush() != OK) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
GonkDecoderManager::Shutdown()
{
  if (mDecoder.get()) {
    mDecoder->stop();
    mDecoder->ReleaseMediaResources();
    mDecoder = nullptr;
  }

  mInitPromise.RejectIfExists(DecoderFailureReason::CANCELED, __func__);

  return NS_OK;
}

bool
GonkDecoderManager::HasQueuedSample()
{
  MutexAutoLock lock(mMutex);
  return mQueuedSamples.Length();
}

void
GonkDecoderManager::onMessageReceived(const sp<AMessage> &aMessage)
{
  switch (aMessage->what()) {
    default:
      {
        TRESPASS();
        break;
      }
  }
}

GonkMediaDataDecoder::GonkMediaDataDecoder(GonkDecoderManager* aManager,
                                           FlushableTaskQueue* aTaskQueue,
                                           MediaDataDecoderCallback* aCallback)
  : mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mManager(aManager)
  , mSignaledEOS(false)
  , mDrainComplete(false)
{
  MOZ_COUNT_CTOR(GonkMediaDataDecoder);
  mManager->SetDecodeCallback(aCallback);
}

GonkMediaDataDecoder::~GonkMediaDataDecoder()
{
  MOZ_COUNT_DTOR(GonkMediaDataDecoder);
}

nsRefPtr<MediaDataDecoder::InitPromise>
GonkMediaDataDecoder::Init()
{
  mDrainComplete = false;

  return mManager->Init();
}

nsresult
GonkMediaDataDecoder::Shutdown()
{
  nsresult rv = mManager->Shutdown();

  // Because codec allocated runnable and init promise is at reader TaskQueue,
  // so manager needs to be destroyed at reader TaskQueue to prevent racing.
  mManager = nullptr;
  return rv;
}

// Inserts data into the decoder's pipeline.
nsresult
GonkMediaDataDecoder::Input(MediaRawData* aSample)
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
      this,
      &GonkMediaDataDecoder::ProcessDecode,
      nsRefPtr<MediaRawData>(aSample)));
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

void
GonkMediaDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  nsresult rv = mManager->Input(aSample);
  if (rv != NS_OK) {
    NS_WARNING("GonkMediaDataDecoder failed to input data");
    GMDD_LOG("Failed to input data err: %d",int(rv));
    mCallback->Error();
    return;
  }
  if (aSample) {
    mLastStreamOffset = aSample->mOffset;
  }
  ProcessOutput();
}

void
GonkMediaDataDecoder::ProcessOutput()
{
  nsRefPtr<MediaData> output;
  nsresult rv = NS_ERROR_ABORT;

  while (!mDrainComplete) {
    // There are samples in queue, try to send them into decoder when EOS.
    if (mSignaledEOS && mManager->HasQueuedSample()) {
      GMDD_LOG("ProcessOutput: drain all input samples");
      rv = mManager->Input(nullptr);
    }
    rv = mManager->Output(mLastStreamOffset, output);
    if (rv == NS_OK) {
      mCallback->Output(output);
      continue;
    } else if (rv == NS_ERROR_NOT_AVAILABLE && mSignaledEOS) {
      // Try to get more frames before getting EOS frame
      continue;
    }
    else {
      break;
    }
  }

  if (rv == NS_ERROR_NOT_AVAILABLE && !mSignaledEOS) {
    mCallback->InputExhausted();
    return;
  }
  if (rv != NS_OK) {
    NS_WARNING("GonkMediaDataDecoder failed to output data");
    GMDD_LOG("Failed to output data");
    // GonkDecoderManangers report NS_ERROR_ABORT when EOS is reached.
    if (rv == NS_ERROR_ABORT) {
      if (output) {
        mCallback->Output(output);
      }
      mCallback->DrainComplete();
      MOZ_ASSERT_IF(mSignaledEOS, !mManager->HasQueuedSample());
      mSignaledEOS = false;
      mDrainComplete = true;
      return;
    }
    GMDD_LOG("Callback error!");
    mCallback->Error();
  }
}

nsresult
GonkMediaDataDecoder::Flush()
{
  // Flush the input task queue. This cancels all pending Decode() calls.
  // Note this blocks until the task queue finishes its current job, if
  // it's executing at all. Note the MP4Reader ignores all output while
  // flushing.
  mTaskQueue->Flush();
  mDrainComplete = false;
  return mManager->Flush();
}

void
GonkMediaDataDecoder::ProcessDrain()
{
  // Notify decoder input EOS by sending a null data.
  ProcessDecode(nullptr);
  mSignaledEOS = true;
  ProcessOutput();
}

nsresult
GonkMediaDataDecoder::Drain()
{
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &GonkMediaDataDecoder::ProcessDrain);
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

} // namespace mozilla
