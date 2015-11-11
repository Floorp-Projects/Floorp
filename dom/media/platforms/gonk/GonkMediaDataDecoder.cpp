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
#define INPUT_TIMEOUT_US 0LL // Don't wait for buffer if none is available.
#define MIN_QUEUED_SAMPLES 2

#ifdef DEBUG
#include <utils/AndroidThreads.h>
#endif

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

#ifdef DEBUG
  sp<AMessage> findThreadId(new AMessage(kNotifyFindLooperId, id()));
  findThreadId->post();
#endif

  return mDecodeLooper->start() == OK && mTaskLooper->start() == OK;
}

nsresult
GonkDecoderManager::Input(MediaRawData* aSample)
{
  RefPtr<MediaRawData> sample;

  if (aSample) {
    sample = aSample;
  } else {
    // It means EOS with empty sample.
    sample = new MediaRawData();
  }
  {
    MutexAutoLock lock(mMutex);
    mQueuedSamples.AppendElement(sample);
  }

  sp<AMessage> input = new AMessage(kNotifyProcessInput, id());
  if (!aSample) {
    input->setInt32("input-eos", 1);
  }
  input->post();
  return NS_OK;
}

int32_t
GonkDecoderManager::ProcessQueuedSamples()
{
  MOZ_ASSERT(OnTaskLooper());

  MutexAutoLock lock(mMutex);
  status_t rv;
  while (mQueuedSamples.Length()) {
    RefPtr<MediaRawData> data = mQueuedSamples.ElementAt(0);
    rv = mDecoder->Input(reinterpret_cast<const uint8_t*>(data->Data()),
                         data->Size(),
                         data->mTime,
                         0,
                         INPUT_TIMEOUT_US);
    if (rv == OK) {
      mQueuedSamples.RemoveElementAt(0);
      mWaitOutput.AppendElement(WaitOutputInfo(data->mOffset, data->mTime,
                                               /* eos */ data->Data() == nullptr));
    } else if (rv == -EAGAIN || rv == -ETIMEDOUT) {
      // In most cases, EAGAIN or ETIMEOUT are safe because OMX can't fill
      // buffer on time.
      break;
    } else {
      return rv;
    }
  }
  return mQueuedSamples.Length();
}

nsresult
GonkDecoderManager::Flush()
{
  if (mDecoder == nullptr) {
    GMDD_LOG("Decoder is not initialized");
    return NS_ERROR_UNEXPECTED;
  }

  if (!mInitPromise.IsEmpty()) {
    return NS_OK;
  }

  {
    MutexAutoLock lock(mMutex);
    mQueuedSamples.Clear();
  }

  MonitorAutoLock lock(mFlushMonitor);
  mIsFlushing = true;
  sp<AMessage> flush = new AMessage(kNotifyProcessFlush, id());
  flush->post();
  while (mIsFlushing) {
    lock.Wait();
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

size_t
GonkDecoderManager::NumQueuedSamples()
{
  MutexAutoLock lock(mMutex);
  return mQueuedSamples.Length();
}

void
GonkDecoderManager::ProcessInput(bool aEndOfStream)
{
  MOZ_ASSERT(OnTaskLooper());

  status_t rv = ProcessQueuedSamples();
  if (rv >= 0) {
    if (!aEndOfStream && rv <= MIN_QUEUED_SAMPLES) {
      mDecodeCallback->InputExhausted();
    }

    if (mToDo.get() == nullptr) {
      mToDo = new AMessage(kNotifyDecoderActivity, id());
      if (aEndOfStream) {
        mToDo->setInt32("input-eos", 1);
      }
      mDecoder->requestActivityNotification(mToDo);
    } else if (aEndOfStream) {
      mToDo->setInt32("input-eos", 1);
    }
  } else {
    GMDD_LOG("input processed: error#%d", rv);
    mDecodeCallback->Error();
  }
}

void
GonkDecoderManager::ProcessFlush()
{
  MOZ_ASSERT(OnTaskLooper());

  mLastTime = INT64_MIN;
  MonitorAutoLock lock(mFlushMonitor);
  mWaitOutput.Clear();
  if (mDecoder->flush() != OK) {
    GMDD_LOG("flush error");
    mDecodeCallback->Error();
  }
  mIsFlushing = false;
  lock.NotifyAll();
}

// Use output timestamp to determine which output buffer is already returned
// and remove corresponding info, except for EOS, from the waiting list.
// This method handles the cases that audio decoder sends multiple output
// buffers for one input.
void
GonkDecoderManager::UpdateWaitingList(int64_t aForgetUpTo)
{
  MOZ_ASSERT(OnTaskLooper());

  size_t i;
  for (i = 0; i < mWaitOutput.Length(); i++) {
    const auto& item = mWaitOutput.ElementAt(i);
    if (item.mEOS || item.mTimestamp > aForgetUpTo) {
      break;
    }
  }
  if (i > 0) {
    mWaitOutput.RemoveElementsAt(0, i);
  }
}

void
GonkDecoderManager::ProcessToDo(bool aEndOfStream)
{
  MOZ_ASSERT(OnTaskLooper());

  MOZ_ASSERT(mToDo.get() != nullptr);
  mToDo.clear();

  if (NumQueuedSamples() > 0 && ProcessQueuedSamples() < 0) {
    mDecodeCallback->Error();
    return;
  }

  while (mWaitOutput.Length() > 0) {
    RefPtr<MediaData> output;
    WaitOutputInfo wait = mWaitOutput.ElementAt(0);
    nsresult rv = Output(wait.mOffset, output);
    if (rv == NS_OK) {
      MOZ_ASSERT(output);
      mDecodeCallback->Output(output);
      UpdateWaitingList(output->mTime);
    } else if (rv == NS_ERROR_ABORT) {
      // EOS
      MOZ_ASSERT(mQueuedSamples.IsEmpty());
      if (output) {
        mDecodeCallback->Output(output);
        UpdateWaitingList(output->mTime);
      }
      MOZ_ASSERT(mWaitOutput.Length() == 1);
      mWaitOutput.RemoveElementAt(0);
      mDecodeCallback->DrainComplete();
      return;
    } else if (rv == NS_ERROR_NOT_AVAILABLE) {
      break;
    } else {
      mDecodeCallback->Error();
      return;
    }
  }

  if (!aEndOfStream && NumQueuedSamples() <= MIN_QUEUED_SAMPLES) {
    mDecodeCallback->InputExhausted();
    // No need to shedule todo task this time because InputExhausted() will
    // cause Input() to be invoked and do it for us.
    return;
  }

  if (NumQueuedSamples() || mWaitOutput.Length() > 0) {
    mToDo = new AMessage(kNotifyDecoderActivity, id());
    if (aEndOfStream) {
      mToDo->setInt32("input-eos", 1);
    }
    mDecoder->requestActivityNotification(mToDo);
  }
}

void
GonkDecoderManager::onMessageReceived(const sp<AMessage> &aMessage)
{
  switch (aMessage->what()) {
    case kNotifyProcessInput:
    {
      int32_t eos = 0;
      ProcessInput(aMessage->findInt32("input-eos", &eos) && eos);
      break;
    }
    case kNotifyProcessFlush:
    {
      ProcessFlush();
      break;
    }
    case kNotifyDecoderActivity:
    {
      int32_t eos = 0;
      ProcessToDo(aMessage->findInt32("input-eos", &eos) && eos);
      break;
    }
#ifdef DEBUG
    case kNotifyFindLooperId:
    {
      mTaskLooperId = androidGetThreadId();
      MOZ_ASSERT(mTaskLooperId);
      break;
    }
#endif
    default:
      {
        TRESPASS();
        break;
      }
  }
}

#ifdef DEBUG
bool
GonkDecoderManager::OnTaskLooper()
{
  return androidGetThreadId() == mTaskLooperId;
}
#endif

GonkMediaDataDecoder::GonkMediaDataDecoder(GonkDecoderManager* aManager,
                                           FlushableTaskQueue* aTaskQueue,
                                           MediaDataDecoderCallback* aCallback)
  : mManager(aManager)
{
  MOZ_COUNT_CTOR(GonkMediaDataDecoder);
  mManager->SetDecodeCallback(aCallback);
}

GonkMediaDataDecoder::~GonkMediaDataDecoder()
{
  MOZ_COUNT_DTOR(GonkMediaDataDecoder);
}

RefPtr<MediaDataDecoder::InitPromise>
GonkMediaDataDecoder::Init()
{
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
  mManager->Input(aSample);
  return NS_OK;
}

nsresult
GonkMediaDataDecoder::Flush()
{
  return mManager->Flush();
}

nsresult
GonkMediaDataDecoder::Drain()
{
  mManager->Input(nullptr);
  return NS_OK;
}

} // namespace mozilla
