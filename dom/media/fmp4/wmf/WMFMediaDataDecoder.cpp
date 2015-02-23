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
{
  MOZ_COUNT_CTOR(WMFMediaDataDecoder);
}

WMFMediaDataDecoder::~WMFMediaDataDecoder()
{
  MOZ_COUNT_DTOR(WMFMediaDataDecoder);
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
  DebugOnly<nsresult> rv = mTaskQueue->FlushAndDispatch(
    NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessShutdown));
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    NS_WARNING("WMFMediaDataDecoder::Shutdown() dispatch of task failed!");
  }
#endif
  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessShutdown()
{
  mMFTManager->Shutdown();
  mMFTManager = nullptr;
  mDecoder = nullptr;
}

void
WMFMediaDataDecoder::ProcessReleaseDecoder()
{
  mMFTManager->Shutdown();
  mDecoder = nullptr;
}

// Inserts data into the decoder's pipeline.
nsresult
WMFMediaDataDecoder::Input(mp4_demuxer::MP4Sample* aSample)
{
  mTaskQueue->Dispatch(
    NS_NewRunnableMethodWithArg<nsAutoPtr<mp4_demuxer::MP4Sample>>(
      this,
      &WMFMediaDataDecoder::ProcessDecode,
      nsAutoPtr<mp4_demuxer::MP4Sample>(aSample)));
  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessDecode(mp4_demuxer::MP4Sample* aSample)
{
  HRESULT hr = mMFTManager->Input(aSample);
  if (FAILED(hr)) {
    NS_WARNING("MFTManager rejected sample");
    mCallback->Error();
    return;
  }

  mLastStreamOffset = aSample->byte_offset;

  ProcessOutput();
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
    mCallback->Error();
  }
}

nsresult
WMFMediaDataDecoder::Flush()
{
  // Flush the input task queue. This cancels all pending Decode() calls.
  // Note this blocks until the task queue finishes its current job, if
  // it's executing at all. Note the MP4Reader ignores all output while
  // flushing.
  mTaskQueue->Flush();

  // Order the MFT to flush; drop all internal data.
  NS_ENSURE_TRUE(mDecoder, NS_ERROR_FAILURE);
  HRESULT hr = mDecoder->Flush();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessDrain()
{
  // Order the decoder to drain...
  if (FAILED(mDecoder->SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0))) {
    NS_WARNING("Failed to send DRAIN command to MFT");
  }
  // Then extract all available output.
  ProcessOutput();
  mCallback->DrainComplete();
}

nsresult
WMFMediaDataDecoder::Drain()
{
  mTaskQueue->Dispatch(NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessDrain));
  return NS_OK;
}

void
WMFMediaDataDecoder::AllocateMediaResources()
{
  mDecoder = mMFTManager->Init();
}

void
WMFMediaDataDecoder::ReleaseMediaResources()
{
  DebugOnly<nsresult> rv = mTaskQueue->FlushAndDispatch(
    NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessReleaseDecoder));
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    NS_WARNING("WMFMediaDataDecoder::ReleaseMediaResources() dispatch of task failed!");
  }
#endif
}

void
WMFMediaDataDecoder::ReleaseDecoder()
{
  ReleaseMediaResources();
}

bool
WMFMediaDataDecoder::IsHardwareAccelerated() const {
  return mMFTManager && mMFTManager->IsHardwareAccelerated();
}

} // namespace mozilla
