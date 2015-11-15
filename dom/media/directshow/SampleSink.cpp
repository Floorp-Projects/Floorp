/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SampleSink.h"
#include "AudioSinkFilter.h"
#include "AudioSinkInputPin.h"
#include "VideoUtils.h"
#include "mozilla/Logging.h"

using namespace mozilla::media;

namespace mozilla {

extern LogModule* GetDirectShowLog();
#define LOG(...) MOZ_LOG(GetDirectShowLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

SampleSink::SampleSink()
  : mMonitor("SampleSink"),
    mIsFlushing(false),
    mAtEOS(false)
{
  MOZ_COUNT_CTOR(SampleSink);
}

SampleSink::~SampleSink()
{
  MOZ_COUNT_DTOR(SampleSink);
}

void
SampleSink::SetAudioFormat(const WAVEFORMATEX* aInFormat)
{
  NS_ENSURE_TRUE(aInFormat, );
  ReentrantMonitorAutoEnter mon(mMonitor);
  memcpy(&mAudioFormat, aInFormat, sizeof(WAVEFORMATEX));
}

void
SampleSink::GetAudioFormat(WAVEFORMATEX* aOutFormat)
{
  MOZ_ASSERT(aOutFormat);
  ReentrantMonitorAutoEnter mon(mMonitor);
  memcpy(aOutFormat, &mAudioFormat, sizeof(WAVEFORMATEX));
}

HRESULT
SampleSink::Receive(IMediaSample* aSample)
{
  ReentrantMonitorAutoEnter mon(mMonitor);

  while (true) {
    if (mIsFlushing) {
      return S_FALSE;
    }
    if (!mSample) {
      break;
    }
    if (mAtEOS) {
      return E_UNEXPECTED;
    }
    // Wait until the consumer thread consumes the sample.
    mon.Wait();
  }

  if (MOZ_LOG_TEST(GetDirectShowLog(), LogLevel::Debug)) {
    REFERENCE_TIME start = 0, end = 0;
    HRESULT hr = aSample->GetMediaTime(&start, &end);
    LOG("SampleSink::Receive() [%4.2lf-%4.2lf]",
        (double)RefTimeToUsecs(start) / USECS_PER_S,
        (double)RefTimeToUsecs(end) / USECS_PER_S);
  }

  mSample = aSample;
  // Notify the signal, to awaken the consumer thread in WaitForSample()
  // if necessary.
  mon.NotifyAll();
  return S_OK;
}

HRESULT
SampleSink::Extract(RefPtr<IMediaSample>& aOutSample)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  // Loop until we have a sample, or we should abort.
  while (true) {
    if (mIsFlushing) {
      return S_FALSE;
    }
    if (mSample) {
      break;
    }
    if (mAtEOS) {
      // Order is important here, if we have a sample, we should return it
      // before reporting EOS.
      return E_UNEXPECTED;
    }
    // Wait until the producer thread gives us a sample.
    mon.Wait();
  }
  aOutSample = mSample;

  if (MOZ_LOG_TEST(GetDirectShowLog(), LogLevel::Debug)) {
    int64_t start = 0, end = 0;
    mSample->GetMediaTime(&start, &end);
    LOG("SampleSink::Extract() [%4.2lf-%4.2lf]",
        (double)RefTimeToUsecs(start) / USECS_PER_S,
        (double)RefTimeToUsecs(end) / USECS_PER_S);
  }

  mSample = nullptr;
  // Notify the signal, to awaken the producer thread in Receive()
  // if necessary.
  mon.NotifyAll();
  return S_OK;
}

void
SampleSink::Flush()
{
  LOG("SampleSink::Flush()");
  ReentrantMonitorAutoEnter mon(mMonitor);
  mIsFlushing = true;
  mSample = nullptr;
  mon.NotifyAll();
}

void
SampleSink::Reset()
{
  LOG("SampleSink::Reset()");
  ReentrantMonitorAutoEnter mon(mMonitor);
  mIsFlushing = false;
  mAtEOS = false;
}

void
SampleSink::SetEOS()
{
  LOG("SampleSink::SetEOS()");
  ReentrantMonitorAutoEnter mon(mMonitor);
  mAtEOS = true;
  // Notify to unblock any threads waiting for samples in
  // Extract() or Receive(). Now that we're at EOS, no more samples
  // will come!
  mon.NotifyAll();
}

bool
SampleSink::AtEOS()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  return mAtEOS && !mSample;
}

} // namespace mozilla

