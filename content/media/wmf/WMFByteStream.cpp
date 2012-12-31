/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"

#include <unknwn.h>
#include <ole2.h>

#include "WMFByteStream.h"
#include "WMFUtils.h"
#include "MediaResource.h"
#include "nsISeekableStream.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gWMFByteStreamLog = nullptr;
#define LOG(...) PR_LOG(gWMFByteStreamLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

HRESULT
DoGetInterface(IUnknown* aUnknown, void** aInterface)
{
  if (!aInterface)
    return E_POINTER;
  *aInterface = aUnknown;
  aUnknown->AddRef();
  return S_OK;
}

WMFByteStream::WMFByteStream(MediaResource* aResource)
  : mWorkQueueId(MFASYNC_CALLBACK_QUEUE_UNDEFINED),
    mResource(aResource),
    mReentrantMonitor("WMFByteStream"),
    mOffset(0)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  NS_ASSERTION(mResource, "Must have a valid media resource");

#ifdef PR_LOGGING
  if (!gWMFByteStreamLog) {
    gWMFByteStreamLog = PR_NewLogModule("WMFByteStream");
  }
#endif

  MOZ_COUNT_CTOR(WMFByteStream);
}

WMFByteStream::~WMFByteStream()
{
  MOZ_COUNT_DTOR(WMFByteStream);
}

nsresult
WMFByteStream::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  // Work queue is not yet initialized, try to create.
  HRESULT hr = wmf::MFAllocateWorkQueue(&mWorkQueueId);
  if (FAILED(hr)) {
    NS_WARNING("WMFByteStream Failed to allocate work queue.");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
WMFByteStream::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  if (mWorkQueueId != MFASYNC_CALLBACK_QUEUE_UNDEFINED) {
    HRESULT hr = wmf::MFUnlockWorkQueue(mWorkQueueId);
    if (FAILED(hr)) {
      NS_WARNING("WMFByteStream Failed to unlock work queue.");
      LOG("WMFByteStream unlock work queue hr=%x %d\n", hr, hr);
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

// IUnknown Methods
STDMETHODIMP
WMFByteStream::QueryInterface(REFIID aIId, void **aInterface)
{
  LOG("WMFByteStream::QueryInterface %s", GetGUIDName(aIId).get());

  if (aIId == IID_IMFByteStream) {
    return DoGetInterface(static_cast<IMFByteStream*>(this), aInterface);
  }
  if (aIId == IID_IMFAsyncCallback) {
    return DoGetInterface(static_cast<IMFAsyncCallback*>(this), aInterface);
  }
  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IMFByteStream*>(this), aInterface);
  }

  *aInterface = NULL;
  return E_NOINTERFACE;
}

NS_IMPL_THREADSAFE_ADDREF(WMFByteStream)
NS_IMPL_THREADSAFE_RELEASE(WMFByteStream)

NS_IMPL_THREADSAFE_ADDREF(WMFByteStream::AsyncReadRequestState)
NS_IMPL_THREADSAFE_RELEASE(WMFByteStream::AsyncReadRequestState)

// IUnknown Methods
STDMETHODIMP
WMFByteStream::AsyncReadRequestState::QueryInterface(REFIID aIId, void **aInterface)
{
  LOG("WMFByteStream::AsyncReadRequestState::QueryInterface %s", GetGUIDName(aIId).get());

  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IUnknown*>(this), aInterface);
  }

  *aInterface = NULL;
  return E_NOINTERFACE;
}

// IMFByteStream Methods
STDMETHODIMP
WMFByteStream::BeginRead(BYTE *aBuffer,
                         ULONG aLength,
                         IMFAsyncCallback *aCallback,
                         IUnknown *aCallerState)
{
  NS_ENSURE_TRUE(aBuffer, E_POINTER);
  NS_ENSURE_TRUE(aCallback, E_POINTER);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  LOG("WMFByteStream::BeginRead() mOffset=%lld tell=%lld length=%lu",
      mOffset, mResource->Tell(), aLength);

  // Create an object to store our state.
  RefPtr<IUnknown> requestState = new AsyncReadRequestState(mOffset, aBuffer, aLength);

  // Create an IMFAsyncResult, this is passed back to the caller as a token to
  // retrieve the number of bytes read.
  RefPtr<IMFAsyncResult> callersResult;
  HRESULT hr = wmf::MFCreateAsyncResult(requestState,
                                        aCallback,
                                        aCallerState,
                                        byRef(callersResult));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Queue a work item on our Windows Media Foundation work queue to call
  // this object's Invoke() function which performs the read, and in turn
  // invokes the caller's callback to notify the caller that the read
  // operation is complete. Note: This creates another IMFAsyncResult to
  // wrap callersResult, and it's that wrapping IMFAsyncResult which is
  // passed to Invoke().
  hr = wmf::MFPutWorkItem(mWorkQueueId, this, callersResult);

  return hr;
}

// IMFAsyncCallback::Invoke implementation. This is called back on the work
// queue thread, and performs the actual read.
STDMETHODIMP
WMFByteStream::Invoke(IMFAsyncResult* aResult)
{
  // Note: We assume that the WMF Work Queue that calls this function does
  // so synchronously, i.e. this function call returns before any other
  // work queue item runs. This is important, as if we run multiple instances
  // of this function at once we'll interleave seeks and reads on the
  // media resoure.

  // Extract the caller's IMFAsyncResult object from the wrapping aResult object.
  RefPtr<IMFAsyncResult> callerResult;
  RefPtr<IUnknown> unknown;
  HRESULT hr = aResult->GetState(byRef(unknown));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  hr = unknown->QueryInterface(static_cast<IMFAsyncResult**>(byRef(callerResult)));
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  // Get the object that holds our state information for the asynchronous call.
  hr = callerResult->GetObject(byRef(unknown));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && unknown, hr);
  AsyncReadRequestState* requestState =
    static_cast<AsyncReadRequestState*>(unknown.get());

  // Ensure the read head is at the correct offset in the resource. It may not
  // be if the SourceReader seeked.
  if (mResource->Tell() != requestState->mOffset) {
    nsresult rv = mResource->Seek(nsISeekableStream::NS_SEEK_SET,
                                  requestState->mOffset);
    if (NS_FAILED(rv)) {
      // Let SourceReader know the read failed.
      callerResult->SetStatus(E_FAIL);
      wmf::MFInvokeCallback(callerResult);
      LOG("WMFByteStream::Invoke() seek to read offset failed, aborting read");
      return S_OK;
    }
  }
  NS_ASSERTION(mResource->Tell() == requestState->mOffset, "State mismatch!");

  // Read in a loop to ensure we fill the buffer, when possible.
  ULONG totalBytesRead = 0;
  nsresult rv = NS_OK;
  while (totalBytesRead < requestState->mBufferLength) {
    BYTE* buffer = requestState->mBuffer + totalBytesRead;
    ULONG bytesRead = 0;
    ULONG length = requestState->mBufferLength - totalBytesRead;
    rv = mResource->Read(reinterpret_cast<char*>(buffer),
                         length,
                         reinterpret_cast<uint32_t*>(&bytesRead));
    totalBytesRead += bytesRead;
    if (NS_FAILED(rv) || bytesRead == 0) {
      break;
    }
  }

  // Record the number of bytes read, so the caller can retrieve
  // it later.
  requestState->mBytesRead = NS_SUCCEEDED(rv) ? totalBytesRead : 0;
  callerResult->SetStatus(S_OK);

  LOG("WMFByteStream::Invoke() read %d at %lld finished rv=%x",
       requestState->mBytesRead, requestState->mOffset, rv);

  // Let caller know read is complete.
  wmf::MFInvokeCallback(callerResult);

  return S_OK;
}

STDMETHODIMP
WMFByteStream::BeginWrite(const BYTE *, ULONG ,
                          IMFAsyncCallback *,
                          IUnknown *)
{
  LOG("WMFByteStream::BeginWrite()");
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Close()
{
  LOG("WMFByteStream::Close()");
  return S_OK;
}

STDMETHODIMP
WMFByteStream::EndRead(IMFAsyncResult* aResult, ULONG *aBytesRead)
{
  NS_ENSURE_TRUE(aResult, E_POINTER);
  NS_ENSURE_TRUE(aBytesRead, E_POINTER);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // Extract our state object.
  RefPtr<IUnknown> unknown;
  HRESULT hr = aResult->GetObject(byRef(unknown));
  if (FAILED(hr) || !unknown) {
    return E_INVALIDARG;
  }
  AsyncReadRequestState* requestState =
    static_cast<AsyncReadRequestState*>(unknown.get());

  // Important: Only advance the read cursor if the caller hasn't seeked
  // since it called BeginRead(). If it has seeked, we still must report
  // the number of bytes read (in *aBytesRead), but we don't advance the
  // read cursor; reads performed after the seek will do that. The SourceReader
  // caller seems to keep track of which async read requested which range
  // to be read, and does call SetCurrentPosition() before it calls EndRead().
  if (mOffset == requestState->mOffset) {
    mOffset += requestState->mBytesRead;
  }

  // Report result.
  *aBytesRead = requestState->mBytesRead;

  LOG("WMFByteStream::EndRead() offset=%lld *aBytesRead=%u mOffset=%lld hr=0x%x eof=%d",
      requestState->mOffset, *aBytesRead, mOffset, hr, (mOffset == mResource->GetLength()));

  return S_OK;
}

STDMETHODIMP
WMFByteStream::EndWrite(IMFAsyncResult *, ULONG *)
{
  LOG("WMFByteStream::EndWrite()");
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Flush()
{
  LOG("WMFByteStream::Flush()");
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetCapabilities(DWORD *aCapabilities)
{
  LOG("WMFByteStream::GetCapabilities()");
  NS_ENSURE_TRUE(aCapabilities, E_POINTER);
  *aCapabilities = MFBYTESTREAM_IS_READABLE |
                   MFBYTESTREAM_IS_SEEKABLE;
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetCurrentPosition(QWORD *aPosition)
{
  NS_ENSURE_TRUE(aPosition, E_POINTER);
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  *aPosition = mOffset;
  LOG("WMFByteStream::GetCurrentPosition() %lld", mOffset);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetLength(QWORD *aLength)
{
  NS_ENSURE_TRUE(aLength, E_POINTER);
  int64_t length = mResource->GetLength();
  *aLength = length;
  LOG("WMFByteStream::GetLength() %lld", length);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::IsEndOfStream(BOOL *aEndOfStream)
{
  NS_ENSURE_TRUE(aEndOfStream, E_POINTER);
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  *aEndOfStream = (mOffset == mResource->GetLength());
  LOG("WMFByteStream::IsEndOfStream() %d", *aEndOfStream);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::Read(BYTE*, ULONG, ULONG*)
{
  LOG("WMFByteStream::Read()");
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN aSeekOrigin,
                    LONGLONG aSeekOffset,
                    DWORD aSeekFlags,
                    QWORD *aCurrentPosition)
{
  LOG("WMFByteStream::Seek(%d, %lld)", aSeekOrigin, aSeekOffset);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (aSeekOrigin == msoBegin) {
    mOffset = aSeekOffset;
  } else {
    mOffset += aSeekOffset;
  }

  if (aCurrentPosition) {
    *aCurrentPosition = mOffset;
  }

  return S_OK;
}

STDMETHODIMP
WMFByteStream::SetCurrentPosition(QWORD aPosition)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  LOG("WMFByteStream::SetCurrentPosition(%lld)",
      aPosition);

  // Note: WMF calls SetCurrentPosition() sometimes after calling BeginRead()
  // but before the read has finished, and thus before it's called EndRead().
  // See comment in EndRead() for more details.

  int64_t length = mResource->GetLength();
  if (length >= 0 && aPosition > uint64_t(length)) {
    // Despite the fact that the MSDN IMFByteStream::SetCurrentPosition()
    // documentation says that we should return E_INVALIDARG when the seek
    // position is after the length, if we do that IMFSourceReader::ReadSample()
    // fails in some situations. So we just clamp the seek target to
    // the EOS, and things seem to just work...
    LOG("WMFByteStream::SetCurrentPosition(%lld) clamping position to eos (%lld)", aPosition, length);
    aPosition = length;
  }
  mOffset = aPosition;

  return S_OK;
}

STDMETHODIMP
WMFByteStream::SetLength(QWORD)
{
  LOG("WMFByteStream::SetLength()");
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Write(const BYTE *, ULONG, ULONG *)
{
  LOG("WMFByteStream::Write()");
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::GetParameters(DWORD*, DWORD*)
{
  LOG("WMFByteStream::GetParameters()");
  return E_NOTIMPL;
}

} // namespace mozilla
