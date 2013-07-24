/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WMFSourceReaderCallback_h_)
#define WMFSourceReaderCallback_h_

#include "WMF.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla {

// A listener which we pass into the IMFSourceReader upon creation which is
// notified when an asynchronous call to IMFSourceReader::ReadSample()
// completes. This allows us to abort ReadSample() operations when the
// WMFByteStream's underlying MediaResource is closed. This ensures that
// the decode threads don't get stuck in a synchronous ReadSample() call
// when the MediaResource is unexpectedly shutdown.
class WMFSourceReaderCallback MOZ_FINAL : public IMFSourceReaderCallback
{
public:
  WMFSourceReaderCallback();

  // IUnknown Methods.
  STDMETHODIMP QueryInterface(REFIID aIId, LPVOID *aInterface);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // IMFSourceReaderCallback methods
  STDMETHODIMP OnReadSample(HRESULT hrStatus,
                            DWORD dwStreamIndex,
                            DWORD dwStreamFlags,
                            LONGLONG llTimestamp,
                            IMFSample *pSample);
  STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
  STDMETHODIMP OnFlush(DWORD);

  // Causes the calling thread to block waiting for the
  // IMFSourceReader::ReadSample() result callback to occur, or for the
  // WMFByteStream to be closed.
  HRESULT Wait(DWORD* aStreamFlags,
               LONGLONG* aTimeStamp,
               IMFSample** aSample);

  // Cancels Wait() calls.
  HRESULT Cancel();

private:

  // Sets state to record the result of a read, and awake threads
  // waiting in Wait().
  HRESULT NotifyReadComplete(HRESULT aReadStatus,
                             DWORD aStreamIndex,
                             DWORD aStreamFlags,
                             LONGLONG aTimestamp,
                             IMFSample *aSample);

  // Synchronizes all member data in this class, and Wait() blocks on
  // and NotifyReadComplete() notifies this monitor.
  ReentrantMonitor mMonitor;

  // Read result data.
  HRESULT mResultStatus;
  DWORD mStreamFlags;
  LONGLONG mTimestamp;
  IMFSample* mSample;

  // Sentinal. Set to true when a read result is returned. Wait() won't exit
  // until this is set to true.
  bool mReadFinished;

  // IUnknown ref counting.
  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

};

} // namespace mozilla

#endif // WMFSourceReaderCallback_h_
