/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSinkInputPin.h"
#include "AudioSinkFilter.h"
#include "SampleSink.h"
#include "mozilla/Logging.h"

#include <wmsdkidl.h>

using namespace mozilla::media;

namespace mozilla {

PRLogModuleInfo* GetDirectShowLog();
#define LOG(...) PR_LOG(GetDirectShowLog(), PR_LOG_DEBUG, (__VA_ARGS__))

AudioSinkInputPin::AudioSinkInputPin(wchar_t* aObjectName,
                                     AudioSinkFilter* aFilter,
                                     mozilla::CriticalSection* aLock,
                                     HRESULT* aOutResult)
  : BaseInputPin(aObjectName, aFilter, aLock, aOutResult, aObjectName),
    mSegmentStartTime(0)
{
  MOZ_COUNT_CTOR(AudioSinkInputPin);
  mSampleSink = new SampleSink();
}

AudioSinkInputPin::~AudioSinkInputPin()
{
  MOZ_COUNT_DTOR(AudioSinkInputPin);
}

HRESULT
AudioSinkInputPin::GetMediaType(int aPosition, MediaType* aOutMediaType)
{
  NS_ENSURE_TRUE(aPosition >= 0, E_INVALIDARG);
  NS_ENSURE_TRUE(aOutMediaType, E_POINTER);

  if (aPosition > 0) {
    return S_FALSE;
  }

  // Note: We set output as PCM, as IEEE_FLOAT only works when using the
  // MP3 decoder as an MFT, and we can't do that while using DirectShow.
  aOutMediaType->SetType(&MEDIATYPE_Audio);
  aOutMediaType->SetSubtype(&MEDIASUBTYPE_PCM);
  aOutMediaType->SetType(&FORMAT_WaveFormatEx);
  aOutMediaType->SetTemporalCompression(FALSE);

  return S_OK;
}

HRESULT
AudioSinkInputPin::CheckMediaType(const MediaType* aMediaType)
{
  if (!aMediaType) {
    return E_INVALIDARG;
  }

  GUID majorType = *aMediaType->Type();
  if (majorType != MEDIATYPE_Audio && majorType != WMMEDIATYPE_Audio) {
    return E_INVALIDARG;
  }

  if (*aMediaType->Subtype() != MEDIASUBTYPE_PCM) {
    return E_INVALIDARG;
  }

  if (*aMediaType->FormatType() != FORMAT_WaveFormatEx) {
    return E_INVALIDARG;
  }

  // We accept the media type, stash its layout format!
  WAVEFORMATEX* wfx = (WAVEFORMATEX*)(aMediaType->pbFormat);
  GetSampleSink()->SetAudioFormat(wfx);

  return S_OK;
}

AudioSinkFilter*
AudioSinkInputPin::GetAudioSinkFilter()
{
  return reinterpret_cast<AudioSinkFilter*>(mFilter);
}

SampleSink*
AudioSinkInputPin::GetSampleSink()
{
  return mSampleSink;
}

HRESULT
AudioSinkInputPin::SetAbsoluteMediaTime(IMediaSample* aSample)
{
  HRESULT hr;
  REFERENCE_TIME start = 0, end = 0;
  hr = aSample->GetTime(&start, &end);
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);
  {
    CriticalSectionAutoEnter lock(*mLock);
    start += mSegmentStartTime;
    end += mSegmentStartTime;
  }
  hr = aSample->SetMediaTime(&start, &end);
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);
  return S_OK;
}

HRESULT
AudioSinkInputPin::Receive(IMediaSample* aSample )
{
  HRESULT hr;
  NS_ENSURE_TRUE(aSample, E_POINTER);

  hr = BaseInputPin::Receive(aSample);
  if (SUCCEEDED(hr) && hr != S_FALSE) { // S_FALSE == flushing
    // Set the timestamp of the sample after being adjusted for
    // seeking/segments in the "media time" attribute. When we seek,
    // DirectShow starts a new "segment", and starts labeling samples
    // from time=0 again, so we need to correct for this to get the
    // actual timestamps after seeking.
    hr = SetAbsoluteMediaTime(aSample);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    hr = GetSampleSink()->Receive(aSample);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }
  return S_OK;
}

TemporaryRef<IMediaSeeking>
AudioSinkInputPin::GetConnectedPinSeeking()
{
  RefPtr<IPin> peer = GetConnected();
  if (!peer)
    return nullptr;
  RefPtr<IMediaSeeking> seeking;
  peer->QueryInterface(static_cast<IMediaSeeking**>(byRef(seeking)));
  return seeking;
}

HRESULT
AudioSinkInputPin::BeginFlush()
{
  HRESULT hr = media::BaseInputPin::BeginFlush();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  GetSampleSink()->Flush();

  return S_OK;
}

HRESULT
AudioSinkInputPin::EndFlush()
{
  HRESULT hr = media::BaseInputPin::EndFlush();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Reset the EOS flag, so that if we're called after a seek we still work.
  GetSampleSink()->Reset();

  return S_OK;
}

HRESULT
AudioSinkInputPin::EndOfStream(void)
{
  HRESULT hr = media::BaseInputPin::EndOfStream();
  if (FAILED(hr) || hr == S_FALSE) {
    // Pin is stil flushing.
    return hr;
  }
  GetSampleSink()->SetEOS();

  return S_OK;
}


HRESULT
AudioSinkInputPin::NewSegment(REFERENCE_TIME tStart,
                              REFERENCE_TIME tStop,
                              double dRate)
{
  CriticalSectionAutoEnter lock(*mLock);
  // Record the start time of the new segment, so that we can store the
  // correct absolute timestamp in the "media time" each incoming sample.
  mSegmentStartTime = tStart;
  return S_OK;
}

} // namespace mozilla

