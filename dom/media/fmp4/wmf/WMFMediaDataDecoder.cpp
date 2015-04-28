/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataDecoder.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "nsTArray.h"

#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif


namespace mozilla {

WMFMediaDataDecoder::WMFMediaDataDecoder(MFTManager* aMFTManager,
                                         FlushableMediaTaskQueue* aTaskQueue,
                                         MediaDataDecoderCallback* aCallback)
  : mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mMFTManager(aMFTManager)
  , mMonitor("WMFMediaDataDecoder")
  , mIsDecodeTaskDispatched(false)
  , mIsFlushing(false)
{
}

WMFMediaDataDecoder::~WMFMediaDataDecoder()
{
}

nsresult
WMFMediaDataDecoder::Init()
{
  mDecoder = mMFTManager->Init();
  NS_ENSURE_TRUE(mDecoder, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
WMFMediaDataDecoder::Shutdown()
{
  if (mTaskQueue) {
    mTaskQueue->Dispatch(
      NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessShutdown));
  } else {
    ProcessShutdown();
  }
#ifdef DEBUG
  {
    MonitorAutoLock mon(mMonitor);
    // The MP4Reader should have flushed before calling Shutdown().
    MOZ_ASSERT(!mIsDecodeTaskDispatched);
  }
#endif
  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessShutdown()
{
  if (mMFTManager) {
    mMFTManager->Shutdown();
    mMFTManager = nullptr;
  }
  mDecoder = nullptr;
}

void
WMFMediaDataDecoder::EnsureDecodeTaskDispatched()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mIsDecodeTaskDispatched) {
    mTaskQueue->Dispatch(
      NS_NewRunnableMethod(this,
      &WMFMediaDataDecoder::Decode));
    mIsDecodeTaskDispatched = true;
  }
}

// Inserts data into the decoder's pipeline.
nsresult
WMFMediaDataDecoder::Input(MediaRawData* aSample)
{
  MonitorAutoLock mon(mMonitor);
  mInput.push(aSample);
  EnsureDecodeTaskDispatched();
  return NS_OK;
}

void
WMFMediaDataDecoder::Decode()
{
  while (true) {
    nsRefPtr<MediaRawData> input;
    {
      MonitorAutoLock mon(mMonitor);
      MOZ_ASSERT(mIsDecodeTaskDispatched);
      if (mInput.empty()) {
        if (mIsFlushing) {
          if (mDecoder) {
            mDecoder->Flush();
          }
          mIsFlushing = false;
        }
        mIsDecodeTaskDispatched = false;
        mon.NotifyAll();
        return;
      }
      input = mInput.front();
      mInput.pop();
    }

    HRESULT hr = mMFTManager->Input(input);
    if (FAILED(hr)) {
      NS_WARNING("MFTManager rejected sample");
      {
        MonitorAutoLock mon(mMonitor);
        PurgeInputQueue();
      }
      mCallback->Error();
      return;
    }

    mLastStreamOffset = input->mOffset;

    ProcessOutput();
  }
}

void
WMFMediaDataDecoder::ProcessOutput()
{
  nsRefPtr<MediaData> output;
  HRESULT hr = S_OK;
  while (SUCCEEDED(hr = mMFTManager->Output(mLastStreamOffset, output)) &&
         output) {
    mCallback->Output(output);
  }
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    if (mTaskQueue->IsEmpty()) {
      mCallback->InputExhausted();
    }
  } else if (FAILED(hr)) {
    NS_WARNING("WMFMediaDataDecoder failed to output data");
    {
      MonitorAutoLock mon(mMonitor);
      PurgeInputQueue();
    }
    mCallback->Error();
  }
}

void
WMFMediaDataDecoder::PurgeInputQueue()
{
  mMonitor.AssertCurrentThreadOwns();
  while (!mInput.empty()) {
    mInput.pop();
  }
}

nsresult
WMFMediaDataDecoder::Flush()
{
  MonitorAutoLock mon(mMonitor);
  PurgeInputQueue();
  mIsFlushing = true;
  EnsureDecodeTaskDispatched();
  while (mIsDecodeTaskDispatched || mIsFlushing) {
    mon.Wait();
  }
  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessDrain()
{
  if (mDecoder) {
    // Order the decoder to drain...
    if (FAILED(mDecoder->SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0))) {
      NS_WARNING("Failed to send DRAIN command to MFT");
    }
    // Then extract all available output.
    ProcessOutput();
  }
  mCallback->DrainComplete();
}

nsresult
WMFMediaDataDecoder::Drain()
{
  mTaskQueue->Dispatch(NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessDrain));
  return NS_OK;
}

bool
WMFMediaDataDecoder::IsHardwareAccelerated() const {
  return mMFTManager && mMFTManager->IsHardwareAccelerated();
}

} // namespace mozilla
