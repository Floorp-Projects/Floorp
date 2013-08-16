/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SampleSink.h"
#include "AudioSinkFilter.h"
#include "AudioSinkInputPin.h"
#include "VideoUtils.h"
#include "prlog.h"


#include <initguid.h>
#include <wmsdkidl.h>

#define DELETE_RESET(p) { delete (p) ; (p) = NULL ;}

DEFINE_GUID(CLSID_MozAudioSinkFilter, 0x1872d8c8, 0xea8d, 0x4c34, 0xae, 0x96, 0x69, 0xde,
            0xf1, 0x33, 0x7b, 0x33);

using namespace mozilla::media;

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* GetDirectShowLog();
#define LOG(...) PR_LOG(GetDirectShowLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

AudioSinkFilter::AudioSinkFilter(const wchar_t* aObjectName, HRESULT* aOutResult)
  : BaseFilter(aObjectName, CLSID_MozAudioSinkFilter),
    mFilterCritSec("AudioSinkFilter::mFilterCritSec")
{
  (*aOutResult) = S_OK;
  mInputPin = new AudioSinkInputPin(L"AudioSinkInputPin",
                                    this,
                                    &mFilterCritSec,
                                    aOutResult);
}

AudioSinkFilter::~AudioSinkFilter()
{
}

int
AudioSinkFilter::GetPinCount()
{
  return 1;
}

BasePin*
AudioSinkFilter::GetPin(int aIndex)
{
  CriticalSectionAutoEnter lockFilter(mFilterCritSec);
  return (aIndex == 0) ? static_cast<BasePin*>(mInputPin) : nullptr;
}

HRESULT
AudioSinkFilter::Pause()
{
  CriticalSectionAutoEnter lockFilter(mFilterCritSec);
  if (mState == State_Stopped) {
    //  Change the state, THEN activate the input pin.
    mState = State_Paused;
    if (mInputPin && mInputPin->IsConnected()) {
      mInputPin->Active();
    }
  } else if (mState == State_Running) {
    mState = State_Paused;
  }
  return S_OK;
}

HRESULT
AudioSinkFilter::Stop()
{
  CriticalSectionAutoEnter lockFilter(mFilterCritSec);
  mState = State_Stopped;
  if (mInputPin) {
    mInputPin->Inactive();
  }

  GetSampleSink()->Flush();

  return S_OK;
}

HRESULT
AudioSinkFilter::Run(REFERENCE_TIME tStart)
{
  LOG("AudioSinkFilter::Run(%lld) [%4.2lf]",
      RefTimeToUsecs(tStart),
      double(RefTimeToUsecs(tStart)) / USECS_PER_S);
  return media::BaseFilter::Run(tStart);
}

HRESULT
AudioSinkFilter::GetClassID( OUT CLSID * pCLSID )
{
  (* pCLSID) = CLSID_MozAudioSinkFilter;
  return S_OK;
}

HRESULT
AudioSinkFilter::QueryInterface(REFIID aIId, void **aInterface)
{
  if (aIId == IID_IMediaSeeking) {
    *aInterface = static_cast<IMediaSeeking*>(this);
    AddRef();
    return S_OK;
  }
  return mozilla::media::BaseFilter::QueryInterface(aIId, aInterface);
}

ULONG
AudioSinkFilter::AddRef()
{
  return ::InterlockedIncrement(&mRefCnt);
}

ULONG
AudioSinkFilter::Release()
{
  unsigned long newRefCnt = ::InterlockedDecrement(&mRefCnt);
  if (!newRefCnt) {
    delete this;
  }
  return newRefCnt;
}

SampleSink*
AudioSinkFilter::GetSampleSink()
{
  return mInputPin->GetSampleSink();
}


// IMediaSeeking implementation.
//
// Calls to IMediaSeeking are forwarded to the output pin that the
// AudioSinkInputPin is connected to, i.e. upstream towards the parser and
// source filters, which actually implement seeking.
#define ENSURE_CONNECTED_PIN_SEEKING \
  if (!mInputPin) { \
    return E_NOTIMPL; \
  } \
  RefPtr<IMediaSeeking> pinSeeking = mInputPin->GetConnectedPinSeeking(); \
  if (!pinSeeking) { \
    return E_NOTIMPL; \
  }

HRESULT
AudioSinkFilter::GetCapabilities(DWORD* aCapabilities)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetCapabilities(aCapabilities);
}

HRESULT
AudioSinkFilter::CheckCapabilities(DWORD* aCapabilities)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->CheckCapabilities(aCapabilities);
}

HRESULT
AudioSinkFilter::IsFormatSupported(const GUID* aFormat)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->IsFormatSupported(aFormat);
}

HRESULT
AudioSinkFilter::QueryPreferredFormat(GUID* aFormat)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->QueryPreferredFormat(aFormat);
}

HRESULT
AudioSinkFilter::GetTimeFormat(GUID* aFormat)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetTimeFormat(aFormat);
}

HRESULT
AudioSinkFilter::IsUsingTimeFormat(const GUID* aFormat)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->IsUsingTimeFormat(aFormat);
}

HRESULT
AudioSinkFilter::SetTimeFormat(const GUID* aFormat)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->SetTimeFormat(aFormat);
}

HRESULT
AudioSinkFilter::GetDuration(LONGLONG* aDuration)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetDuration(aDuration);
}

HRESULT
AudioSinkFilter::GetStopPosition(LONGLONG* aStop)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetStopPosition(aStop);
}

HRESULT
AudioSinkFilter::GetCurrentPosition(LONGLONG* aCurrent)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetCurrentPosition(aCurrent);
}

HRESULT
AudioSinkFilter::ConvertTimeFormat(LONGLONG* aTarget,
                                   const GUID* aTargetFormat,
                                   LONGLONG aSource,
                                   const GUID* aSourceFormat)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->ConvertTimeFormat(aTarget,
                                       aTargetFormat,
                                       aSource,
                                       aSourceFormat);
}

HRESULT
AudioSinkFilter::SetPositions(LONGLONG* aCurrent,
                              DWORD aCurrentFlags,
                              LONGLONG* aStop,
                              DWORD aStopFlags)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->SetPositions(aCurrent,
                                  aCurrentFlags,
                                  aStop,
                                  aStopFlags);
}

HRESULT
AudioSinkFilter::GetPositions(LONGLONG* aCurrent,
                              LONGLONG* aStop)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetPositions(aCurrent, aStop);
}

HRESULT
AudioSinkFilter::GetAvailable(LONGLONG* aEarliest,
                              LONGLONG* aLatest)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetAvailable(aEarliest, aLatest);
}

HRESULT
AudioSinkFilter::SetRate(double aRate)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->SetRate(aRate);
}

HRESULT
AudioSinkFilter::GetRate(double* aRate)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetRate(aRate);
}

HRESULT
AudioSinkFilter::GetPreroll(LONGLONG* aPreroll)
{
  ENSURE_CONNECTED_PIN_SEEKING
  return pinSeeking->GetPreroll(aPreroll);
}

} // namespace mozilla

