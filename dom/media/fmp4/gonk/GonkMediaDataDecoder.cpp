/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mp4_demuxer/mp4_demuxer.h"
#include "GonkMediaDataDecoder.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "MediaCodecProxy.h"
#include "MediaData.h"

#include "prlog.h"
#include <android/log.h>
#define GMDD_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "GonkMediaDataDecoder", __VA_ARGS__)

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

using namespace android;

namespace mozilla {

GonkDecoderManager::GonkDecoderManager(MediaTaskQueue* aTaskQueue)
  : mMonitor("GonkDecoderManager")
{
}

nsresult
GonkDecoderManager::Input(MediaRawData* aSample)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  nsRefPtr<MediaRawData> sample;

  if (!aSample) {
    // It means EOS with empty sample.
    sample = new MediaRawData();
  } else {
    sample = aSample;
    if (!PerformFormatSpecificProcess(sample)) {
      return NS_ERROR_FAILURE;
    }
  }

  mQueueSample.AppendElement(sample);

  status_t rv;
  while (mQueueSample.Length()) {
    nsRefPtr<MediaRawData> data = mQueueSample.ElementAt(0);
    {
      ReentrantMonitorAutoExit mon_exit(mMonitor);
      rv = SendSampleToOMX(data);
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
GonkDecoderManager::Flush()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  mQueueSample.Clear();
  return NS_OK;
}

GonkMediaDataDecoder::GonkMediaDataDecoder(GonkDecoderManager* aManager,
                                           FlushableMediaTaskQueue* aTaskQueue,
                                           MediaDataDecoderCallback* aCallback)
  : mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mManager(aManager)
  , mSignaledEOS(false)
  , mDrainComplete(false)
{
  MOZ_COUNT_CTOR(GonkMediaDataDecoder);
}

GonkMediaDataDecoder::~GonkMediaDataDecoder()
{
  MOZ_COUNT_DTOR(GonkMediaDataDecoder);
}

nsresult
GonkMediaDataDecoder::Init()
{
  sp<MediaCodecProxy> decoder;
  decoder = mManager->Init(mCallback);
  mDecoder = decoder;
  mDrainComplete = false;

  return NS_OK;
}

nsresult
GonkMediaDataDecoder::Shutdown()
{
  if (!mDecoder.get()) {
    return NS_OK;
  }

  mDecoder->stop();
  mDecoder->ReleaseMediaResources();
  mDecoder = nullptr;
  return NS_OK;
}

// Inserts data into the decoder's pipeline.
nsresult
GonkMediaDataDecoder::Input(MediaRawData* aSample)
{
  mTaskQueue->Dispatch(
    NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
      this,
      &GonkMediaDataDecoder::ProcessDecode,
      nsRefPtr<MediaRawData>(aSample)));
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
  mTaskQueue->Dispatch(NS_NewRunnableMethod(this, &GonkMediaDataDecoder::ProcessDrain));
  return NS_OK;
}

} // namespace mozilla
