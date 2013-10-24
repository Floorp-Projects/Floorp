/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFSourceReaderCallback.h"
#include "WMFUtils.h"

namespace mozilla {

#ifdef PR_LOGGING
static PRLogModuleInfo* gWMFSourceReaderCallbackLog = nullptr;
#define LOG(...) PR_LOG(gWMFSourceReaderCallbackLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

// IUnknown Methods
STDMETHODIMP
WMFSourceReaderCallback::QueryInterface(REFIID aIId, void **aInterface)
{
  LOG("WMFSourceReaderCallback::QueryInterface %s", GetGUIDName(aIId).get());

  if (aIId == IID_IMFSourceReaderCallback) {
    return DoGetInterface(static_cast<WMFSourceReaderCallback*>(this), aInterface);
  }
  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<WMFSourceReaderCallback*>(this), aInterface);
  }

  *aInterface = nullptr;
  return E_NOINTERFACE;
}

NS_IMPL_ADDREF(WMFSourceReaderCallback)
NS_IMPL_RELEASE(WMFSourceReaderCallback)

WMFSourceReaderCallback::WMFSourceReaderCallback()
  : mMonitor("WMFSourceReaderCallback")
  , mResultStatus(S_OK)
  , mStreamFlags(0)
  , mTimestamp(0)
  , mSample(nullptr)
  , mReadFinished(false)
{
#ifdef PR_LOGGING
  if (!gWMFSourceReaderCallbackLog) {
    gWMFSourceReaderCallbackLog = PR_NewLogModule("WMFSourceReaderCallback");
  }
#endif
}

HRESULT
WMFSourceReaderCallback::NotifyReadComplete(HRESULT aReadStatus,
                                            DWORD aStreamIndex,
                                            DWORD aStreamFlags,
                                            LONGLONG aTimestamp,
                                            IMFSample *aSample)
{
  // Note: aSample can be nullptr on success if more data is required!
  ReentrantMonitorAutoEnter mon(mMonitor);

  if (mSample) {
    // The WMFReader should have called Wait() to retrieve the last
    // sample returned by the last ReadSample() call, but if we're
    // aborting the read before Wait() is called the sample ref
    // can be non-null.
    mSample->Release();
    mSample = nullptr;
  }

  if (SUCCEEDED(aReadStatus)) {
    if (aSample) {
      mTimestamp = aTimestamp;
      mSample = aSample;
      mSample->AddRef();
    }
  }

  mResultStatus = aReadStatus;
  mStreamFlags = aStreamFlags;

  // Set the sentinal value and notify the monitor, so that threads waiting
  // in Wait() are awoken.
  mReadFinished = true;
  mon.NotifyAll();

  return S_OK;
}

STDMETHODIMP
WMFSourceReaderCallback::OnReadSample(HRESULT aReadStatus,
                                      DWORD aStreamIndex,
                                      DWORD aStreamFlags,
                                      LONGLONG aTimestamp,
                                      IMFSample *aSample)
{
  LOG("WMFSourceReaderCallback::OnReadSample() hr=0x%x flags=0x%x time=%lld sample=%p",
      aReadStatus, aStreamFlags, aTimestamp, aSample);
  return NotifyReadComplete(aReadStatus,
                            aStreamIndex,
                            aStreamFlags,
                            aTimestamp,
                            aSample);
}

HRESULT
WMFSourceReaderCallback::Cancel()
{
  LOG("WMFSourceReaderCallback::Cancel()");
  return NotifyReadComplete(E_ABORT,
                            0,
                            0,
                            0,
                            nullptr);
}

STDMETHODIMP
WMFSourceReaderCallback::OnEvent(DWORD, IMFMediaEvent *)
{
  return S_OK;
}

STDMETHODIMP
WMFSourceReaderCallback::OnFlush(DWORD)
{
  return S_OK;
}

HRESULT
WMFSourceReaderCallback::Wait(DWORD* aStreamFlags,
                              LONGLONG* aTimeStamp,
                              IMFSample** aSample)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  LOG("WMFSourceReaderCallback::Wait() starting wait");
  while (!mReadFinished) {
    mon.Wait();
  }
  mReadFinished = false;
  LOG("WMFSourceReaderCallback::Wait() done waiting");

  *aStreamFlags = mStreamFlags;
  *aTimeStamp = mTimestamp;
  *aSample = mSample;
  HRESULT hr = mResultStatus;

  mSample = nullptr;
  mTimestamp = 0;
  mStreamFlags = 0;
  mResultStatus = S_OK;

  return hr;
}

} // namespace mozilla
