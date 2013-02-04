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
#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"

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

// Thread pool listener which ensures that MSCOM is initialized and
// deinitialized on the thread pool thread. We can call back into WMF
// on this thread, so we need MSCOM working.
class ThreadPoolListener MOZ_FINAL : public nsIThreadPoolListener {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER
};

NS_IMPL_THREADSAFE_ISUPPORTS1(ThreadPoolListener, nsIThreadPoolListener)

NS_IMETHODIMP
ThreadPoolListener::OnThreadCreated()
{
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    NS_WARNING("Failed to initialize MSCOM on WMFByteStream thread.");
  }
  return NS_OK;
}

NS_IMETHODIMP
ThreadPoolListener::OnThreadShuttingDown()
{
  CoUninitialize();
  return NS_OK;
}

// Thread pool on which read requests are processed.
// This is created and destroyed on the main thread only.
static nsIThreadPool* sThreadPool = nullptr;

// Counter of the number of WMFByteStreams that are instantiated and that need
// the thread pool. This is read/write on the main thread only.
static int32_t sThreadPoolRefCnt = 0;

class ReleaseThreadPoolEvent MOZ_FINAL : public nsRunnable {
public:
  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    NS_ASSERTION(sThreadPoolRefCnt > 0, "sThreadPoolRefCnt Should be non-negative");
    sThreadPoolRefCnt--;
    if (sThreadPoolRefCnt == 0) {
      NS_ASSERTION(sThreadPool != nullptr, "Should have thread pool ref if sThreadPoolRefCnt==0.");
      // Note: store ref to thread pool, then clear global ref, then
      // Shutdown() using the stored ref. Events can run during the Shutdown()
      // call, so if we release after calling Shutdown(), another event may
      // have incremented the refcnt in the meantime, and have a dangling
      // pointer to the now destroyed threadpool!
      nsCOMPtr<nsIThreadPool> pool = sThreadPool;
      NS_IF_RELEASE(sThreadPool);
      pool->Shutdown();
    }
    return NS_OK;
  }
};

WMFByteStream::WMFByteStream(MediaResource* aResource)
  : mResource(aResource),
    mReentrantMonitor("WMFByteStream"),
    mOffset(0),
    mIsShutdown(false)
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
  // The WMFByteStream can be deleted from a WMF work queue thread, so we
  // dispatch an event to the main thread to deref the thread pool.
  nsCOMPtr<nsIRunnable> event = new ReleaseThreadPoolEvent();
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

nsresult
WMFByteStream::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  if (!sThreadPool) {
    nsresult rv;
    nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    sThreadPool = pool;
    NS_ADDREF(sThreadPool);

    rv = sThreadPool->SetName(NS_LITERAL_CSTRING("WMFByteStream Async Read Pool"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIThreadPoolListener> listener = new ThreadPoolListener();
    rv = sThreadPool->SetListener(listener);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  sThreadPoolRefCnt++;

  // Store a ref to the thread pool, so that we keep the pool alive as long as
  // we're alive.
  mThreadPool = sThreadPool;

  return NS_OK;
}

nsresult
WMFByteStream::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mIsShutdown = true;
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
  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IMFByteStream*>(this), aInterface);
  }

  *aInterface = NULL;
  return E_NOINTERFACE;
}

NS_IMPL_THREADSAFE_ADDREF(WMFByteStream)
NS_IMPL_THREADSAFE_RELEASE(WMFByteStream)


// Stores data regarding an async read opreation.
class AsyncReadRequestState MOZ_FINAL : public IUnknown {
public:
  AsyncReadRequestState(int64_t aOffset, BYTE* aBuffer, ULONG aLength)
    : mOffset(aOffset),
      mBuffer(aBuffer),
      mBufferLength(aLength),
      mBytesRead(0)
  {}

  // IUnknown Methods
  STDMETHODIMP QueryInterface(REFIID aRIID, LPVOID *aOutObject);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  int64_t mOffset;
  BYTE* mBuffer;
  ULONG mBufferLength;
  ULONG mBytesRead;

  // IUnknown ref counting.
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

NS_IMPL_THREADSAFE_ADDREF(AsyncReadRequestState)
NS_IMPL_THREADSAFE_RELEASE(AsyncReadRequestState)

// IUnknown Methods
STDMETHODIMP
AsyncReadRequestState::QueryInterface(REFIID aIId, void **aInterface)
{
  LOG("WMFByteStream::AsyncReadRequestState::QueryInterface %s", GetGUIDName(aIId).get());

  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IUnknown*>(this), aInterface);
  }

  *aInterface = NULL;
  return E_NOINTERFACE;
}

class PerformReadEvent MOZ_FINAL : public nsRunnable {
public:
  PerformReadEvent(WMFByteStream* aStream,
                   IMFAsyncResult* aResult,
                   AsyncReadRequestState* aRequestState)
    : mStream(aStream),
      mResult(aResult),
      mRequestState(aRequestState) {}

  NS_IMETHOD Run() {
    mStream->PerformRead(mResult, mRequestState);
    return NS_OK;
  }
private:
  RefPtr<WMFByteStream> mStream;
  RefPtr<IMFAsyncResult> mResult;
  RefPtr<AsyncReadRequestState> mRequestState;
};

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
  LOG("WMFByteStream::BeginRead() mOffset=%lld tell=%lld length=%lu mIsShutdown=%d",
      mOffset, mResource->Tell(), aLength, mIsShutdown);

  if (mIsShutdown) {
    return E_FAIL;
  }

  // Create an object to store our state.
  RefPtr<AsyncReadRequestState> requestState = new AsyncReadRequestState(mOffset, aBuffer, aLength);

  // Create an IMFAsyncResult, this is passed back to the caller as a token to
  // retrieve the number of bytes read.
  RefPtr<IMFAsyncResult> callersResult;
  HRESULT hr = wmf::MFCreateAsyncResult(requestState,
                                        aCallback,
                                        aCallerState,
                                        byRef(callersResult));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Dispatch an event to perform the read in the thread pool.
  nsCOMPtr<nsIRunnable> r = new PerformReadEvent(this, callersResult, requestState);
  nsresult rv = mThreadPool->Dispatch(r, NS_DISPATCH_NORMAL);

  return NS_SUCCEEDED(rv) ? S_OK : E_FAIL;
}

// Note: This is called on one of the thread pool's threads.
void
WMFByteStream::PerformRead(IMFAsyncResult* aResult, AsyncReadRequestState* aRequestState)
{
  // Ensure the read head is at the correct offset in the resource. It may not
  // be if the SourceReader seeked.
  if (mResource->Tell() != aRequestState->mOffset) {
    nsresult rv = mResource->Seek(nsISeekableStream::NS_SEEK_SET,
                                  aRequestState->mOffset);
    if (NS_FAILED(rv)) {
      // Let SourceReader know the read failed.
      aResult->SetStatus(E_FAIL);
      wmf::MFInvokeCallback(aResult);
      LOG("WMFByteStream::Invoke() seek to read offset failed, aborting read");
      return;
    }
  }
  NS_ASSERTION(mResource->Tell() == aRequestState->mOffset, "State mismatch!");

  // Read in a loop to ensure we fill the buffer, when possible.
  ULONG totalBytesRead = 0;
  nsresult rv = NS_OK;
  while (totalBytesRead < aRequestState->mBufferLength) {
    BYTE* buffer = aRequestState->mBuffer + totalBytesRead;
    ULONG bytesRead = 0;
    ULONG length = aRequestState->mBufferLength - totalBytesRead;
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
  aRequestState->mBytesRead = NS_SUCCEEDED(rv) ? totalBytesRead : 0;
  aResult->SetStatus(S_OK);

  LOG("WMFByteStream::Invoke() read %d at %lld finished rv=%x",
       aRequestState->mBytesRead, aRequestState->mOffset, rv);

  // Let caller know read is complete.
  DebugOnly<HRESULT> hr = wmf::MFInvokeCallback(aResult);
  NS_ASSERTION(SUCCEEDED(hr), "Failed to invoke callback!");
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

  if (mIsShutdown) {
    return E_FAIL;
  }

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

  if (mIsShutdown) {
    return E_FAIL;
  }

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

} // namespace mozilla
