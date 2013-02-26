/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"

#include <unknwn.h>
#include <ole2.h>

#include "WMFByteStream.h"
#include "WMFSourceReaderCallback.h"
#include "WMFUtils.h"
#include "MediaResource.h"
#include "nsISeekableStream.h"
#include "mozilla/RefPtr.h"
#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"
#include <algorithm>

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gWMFByteStreamLog = nullptr;
#define LOG(...) PR_LOG(gWMFByteStreamLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

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

class ReleaseWMFByteStreamResourcesEvent MOZ_FINAL : public nsRunnable {
public:
  ReleaseWMFByteStreamResourcesEvent(already_AddRefed<MediaResource> aResource)
    : mResource(aResource) {}
  virtual ~ReleaseWMFByteStreamResourcesEvent() {}
  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    // Explicitly release the MediaResource reference. We *must* do this on
    // the main thread, so we must explicitly release it here, we can't rely
    // on the destructor to release it, since if this event runs before its
    // dispatch call returns the destructor may run on the non-main thread.
    mResource = nullptr;
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
  nsRefPtr<MediaResource> mResource;
};

WMFByteStream::WMFByteStream(MediaResource* aResource,
                             WMFSourceReaderCallback* aSourceReaderCallback)
  : mSourceReaderCallback(aSourceReaderCallback),
    mResourceMonitor("WMFByteStream.MediaResource"),
    mResource(aResource),
    mReentrantMonitor("WMFByteStream.Data"),
    mOffset(0),
    mIsShutdown(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  NS_ASSERTION(mResource, "Must have a valid media resource");
  NS_ASSERTION(mSourceReaderCallback, "Must have a source reader callback.");

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
  // The WMFByteStream can be deleted from a thread pool thread, so we
  // dispatch an event to the main thread to deref the thread pool and
  // deref the MediaResource.
  nsCOMPtr<nsIRunnable> event =
    new ReleaseWMFByteStreamResourcesEvent(mResource.forget());
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

  NS_ConvertUTF8toUTF16 contentTypeUTF16(mResource->GetContentType());
  if (!contentTypeUTF16.IsEmpty()) {
    HRESULT hr = wmf::MFCreateAttributes(byRef(mAttributes), 1);
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

    hr = mAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE,
                                contentTypeUTF16.get());
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

    LOG("WMFByteStream has Content-Type=%s", mResource->GetContentType().get());
  }
  return NS_OK;
}

nsresult
WMFByteStream::Shutdown()
{
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mIsShutdown = true;
  }
  mSourceReaderCallback->Cancel();
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
  if (aIId == IID_IMFAttributes) {
    return DoGetInterface(static_cast<IMFAttributes*>(this), aInterface);
  }

  *aInterface = NULL;
  return E_NOINTERFACE;
}

NS_IMPL_THREADSAFE_ADDREF(WMFByteStream)
NS_IMPL_THREADSAFE_RELEASE(WMFByteStream)


// Stores data regarding an async read opreation.
class ReadRequest MOZ_FINAL : public IUnknown {
public:
  ReadRequest(int64_t aOffset, BYTE* aBuffer, ULONG aLength)
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

NS_IMPL_THREADSAFE_ADDREF(ReadRequest)
NS_IMPL_THREADSAFE_RELEASE(ReadRequest)

// IUnknown Methods
STDMETHODIMP
ReadRequest::QueryInterface(REFIID aIId, void **aInterface)
{
  LOG("ReadRequest::QueryInterface %s", GetGUIDName(aIId).get());

  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IUnknown*>(this), aInterface);
  }

  *aInterface = NULL;
  return E_NOINTERFACE;
}

class ProcessReadRequestEvent MOZ_FINAL : public nsRunnable {
public:
  ProcessReadRequestEvent(WMFByteStream* aStream,
                          IMFAsyncResult* aResult,
                          ReadRequest* aRequestState)
    : mStream(aStream),
      mResult(aResult),
      mRequestState(aRequestState) {}

  NS_IMETHOD Run() {
    mStream->ProcessReadRequest(mResult, mRequestState);
    return NS_OK;
  }
private:
  RefPtr<WMFByteStream> mStream;
  RefPtr<IMFAsyncResult> mResult;
  RefPtr<ReadRequest> mRequestState;
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

  if (mIsShutdown || mOffset < 0) {
    return E_INVALIDARG;
  }

  // Create an object to store our state.
  RefPtr<ReadRequest> requestState = new ReadRequest(mOffset, aBuffer, aLength);

  // Create an IMFAsyncResult, this is passed back to the caller as a token to
  // retrieve the number of bytes read.
  RefPtr<IMFAsyncResult> callersResult;
  HRESULT hr = wmf::MFCreateAsyncResult(requestState,
                                        aCallback,
                                        aCallerState,
                                        byRef(callersResult));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Dispatch an event to perform the read in the thread pool.
  nsCOMPtr<nsIRunnable> r = new ProcessReadRequestEvent(this,
                                                        callersResult,
                                                        requestState);
  nsresult rv = mThreadPool->Dispatch(r, NS_DISPATCH_NORMAL);

  if (mResource->GetLength() > -1) {
    mOffset = std::min<int64_t>(mOffset + aLength, mResource->GetLength());
  } else {
    mOffset += aLength;
  }

  return NS_SUCCEEDED(rv) ? S_OK : E_FAIL;
}

nsresult
WMFByteStream::Read(ReadRequest* aRequestState)
{
  ReentrantMonitorAutoEnter mon(mResourceMonitor);

  // Ensure the read head is at the correct offset in the resource. It may not
  // be if the SourceReader seeked.
  if (mResource->Tell() != aRequestState->mOffset) {
    nsresult rv = mResource->Seek(nsISeekableStream::NS_SEEK_SET,
                                  aRequestState->mOffset);
    NS_ENSURE_SUCCESS(rv, rv);
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
    NS_ENSURE_SUCCESS(rv, rv);
    totalBytesRead += bytesRead;
    if (bytesRead == 0) {
      break;
    }
  }
  aRequestState->mBytesRead = totalBytesRead;
  return NS_OK;
}

// Note: This is called on one of the thread pool's threads.
void
WMFByteStream::ProcessReadRequest(IMFAsyncResult* aResult,
                                  ReadRequest* aRequestState)
{
  if (mResource->GetLength() > -1 &&
      aRequestState->mOffset > mResource->GetLength()) {
    aResult->SetStatus(S_OK);
    wmf::MFInvokeCallback(aResult);
    LOG("WMFByteStream::Invoke() read offset greater than length, soft-failing read");
    return;
  }

  nsresult rv = Read(aRequestState);
  if (NS_FAILED(rv)) {
    Shutdown();
    aResult->SetStatus(E_ABORT);
  } else {
    aResult->SetStatus(S_OK);
  }

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
  ReadRequest* requestState =
    static_cast<ReadRequest*>(unknown.get());

  // Report result.
  *aBytesRead = requestState->mBytesRead;

  LOG("WMFByteStream::EndRead() offset=%lld *aBytesRead=%u mOffset=%lld status=0x%x hr=0x%x eof=%d",
      requestState->mOffset, *aBytesRead, mOffset, aResult->GetStatus(), hr, IsEOS());

  return aResult->GetStatus();
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
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  bool seekable = mResource->IsTransportSeekable();
  bool cached = mResource->IsDataCachedToEndOfResource(0);
  *aCapabilities = MFBYTESTREAM_IS_READABLE |
                   MFBYTESTREAM_IS_SEEKABLE |
                   (!cached ? MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED : 0) |
                   (!seekable ? MFBYTESTREAM_HAS_SLOW_SEEK : 0);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetCurrentPosition(QWORD *aPosition)
{
  NS_ENSURE_TRUE(aPosition, E_POINTER);
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  // Note: Returning the length of stream as position when read
  // cursor is < 0 seems to be the behaviour expected by WMF, but
  // also note it doesn't seem to expect that the position is an
  // unsigned value since if you seek to > length and read WMF
  // expects the read to succeed after reading 0 bytes, but if you
  // seek to < 0 and read, the read is expected to fails... So
  // go figure...
  *aPosition = mOffset < 0 ? mResource->GetLength() : mOffset;
  LOG("WMFByteStream::GetCurrentPosition() %lld", mOffset);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetLength(QWORD *aLength)
{
  NS_ENSURE_TRUE(aLength, E_POINTER);
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  *aLength = mResource->GetLength();
  LOG("WMFByteStream::GetLength() %lld", *aLength);
  return S_OK;
}

bool
WMFByteStream::IsEOS()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  return mResource->GetLength() > -1 &&
         (mOffset < 0 ||
          mOffset >= mResource->GetLength());
}

STDMETHODIMP
WMFByteStream::IsEndOfStream(BOOL *aEndOfStream)
{
  NS_ENSURE_TRUE(aEndOfStream, E_POINTER);
  *aEndOfStream = IsEOS();
  LOG("WMFByteStream::IsEndOfStream() %d", *aEndOfStream);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::Read(BYTE* aBuffer, ULONG aBufferLength, ULONG* aOutBytesRead)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  ReadRequest request(mOffset, aBuffer, aBufferLength);
  if (NS_FAILED(Read(&request))) {
    LOG("WMFByteStream::Read() offset=%lld failed!", mOffset);
    return E_FAIL;
  }
  if (aOutBytesRead) {
    *aOutBytesRead = request.mBytesRead;
  }
  LOG("WMFByteStream::Read() offset=%lld length=%u bytesRead=%u",
      mOffset, aBufferLength, request.mBytesRead);
  mOffset += request.mBytesRead;
  return S_OK;
}

STDMETHODIMP
WMFByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN aSeekOrigin,
                    LONGLONG aSeekOffset,
                    DWORD aSeekFlags,
                    QWORD *aCurrentPosition)
{
  LOG("WMFByteStream::Seek(%d, %lld)", aSeekOrigin, aSeekOffset);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  int64_t offset = mOffset;
  if (aSeekOrigin == msoBegin) {
    offset = aSeekOffset;
  } else {
    offset += aSeekOffset;
  }
  int64_t length = mResource->GetLength();
  if (length > -1) {
    mOffset = std::min<int64_t>(offset, length);
  } else {
    mOffset = offset;
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

  int64_t length = mResource->GetLength();
  if (length > -1) {
    mOffset = std::min<int64_t>(aPosition, length);
  } else {
    mOffset = aPosition;
  }

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

// IMFAttributes methods
STDMETHODIMP
WMFByteStream::GetItem(REFGUID guidKey, PROPVARIANT* pValue)
{
    MOZ_ASSERT(mAttributes);
    return mAttributes->GetItem(guidKey, pValue);
}

STDMETHODIMP
WMFByteStream::GetItemType(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType)
{
    assert(mAttributes);
    return mAttributes->GetItemType(guidKey, pType);
}

STDMETHODIMP
WMFByteStream::CompareItem(REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult)
{
    assert(mAttributes);
    return mAttributes->CompareItem(guidKey, Value, pbResult);
}

STDMETHODIMP
WMFByteStream::Compare(IMFAttributes* pTheirs,
                       MF_ATTRIBUTES_MATCH_TYPE MatchType,
                       BOOL* pbResult)
{
    assert(mAttributes);
    return mAttributes->Compare(pTheirs, MatchType, pbResult);
}

STDMETHODIMP
WMFByteStream::GetUINT32(REFGUID guidKey, UINT32* punValue)
{
    assert(mAttributes);
    return mAttributes->GetUINT32(guidKey, punValue);
}

STDMETHODIMP
WMFByteStream::GetUINT64(REFGUID guidKey, UINT64* punValue)
{
    assert(mAttributes);
    return mAttributes->GetUINT64(guidKey, punValue);
}

STDMETHODIMP
WMFByteStream::GetDouble(REFGUID guidKey, double* pfValue)
{
    assert(mAttributes);
    return mAttributes->GetDouble(guidKey, pfValue);
}

STDMETHODIMP
WMFByteStream::GetGUID(REFGUID guidKey, GUID* pguidValue)
{
    assert(mAttributes);
    return mAttributes->GetGUID(guidKey, pguidValue);
}

STDMETHODIMP
WMFByteStream::GetStringLength(REFGUID guidKey, UINT32* pcchLength)
{
    assert(mAttributes);
    return mAttributes->GetStringLength(guidKey, pcchLength);
}

STDMETHODIMP
WMFByteStream::GetString(REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32* pcchLength)
{
    assert(mAttributes);
    return mAttributes->GetString(guidKey, pwszValue, cchBufSize, pcchLength);
}

STDMETHODIMP
WMFByteStream::GetAllocatedString(REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength)
{
    assert(mAttributes);
    return mAttributes->GetAllocatedString(guidKey, ppwszValue, pcchLength);
}

STDMETHODIMP
WMFByteStream::GetBlobSize(REFGUID guidKey, UINT32* pcbBlobSize)
{
    assert(mAttributes);
    return mAttributes->GetBlobSize(guidKey, pcbBlobSize);
}

STDMETHODIMP
WMFByteStream::GetBlob(REFGUID guidKey, UINT8* pBuf, UINT32 cbBufSize, UINT32* pcbBlobSize)
{
    assert(mAttributes);
    return mAttributes->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize);
}

STDMETHODIMP
WMFByteStream::GetAllocatedBlob(REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize)
{
    assert(mAttributes);
    return mAttributes->GetAllocatedBlob(guidKey, ppBuf, pcbSize);
}

STDMETHODIMP
WMFByteStream::GetUnknown(REFGUID guidKey, REFIID riid, LPVOID* ppv)
{
    assert(mAttributes);
    return mAttributes->GetUnknown(guidKey, riid, ppv);
}

STDMETHODIMP
WMFByteStream::SetItem(REFGUID guidKey, REFPROPVARIANT Value)
{
    assert(mAttributes);
    return mAttributes->SetItem(guidKey, Value);
}

STDMETHODIMP
WMFByteStream::DeleteItem(REFGUID guidKey)
{
    assert(mAttributes);
    return mAttributes->DeleteItem(guidKey);
}

STDMETHODIMP
WMFByteStream::DeleteAllItems()
{
    assert(mAttributes);
    return mAttributes->DeleteAllItems();
}

STDMETHODIMP
WMFByteStream::SetUINT32(REFGUID guidKey, UINT32 unValue)
{
    assert(mAttributes);
    return mAttributes->SetUINT32(guidKey, unValue);
}

STDMETHODIMP
WMFByteStream::SetUINT64(REFGUID guidKey,UINT64 unValue)
{
    assert(mAttributes);
    return mAttributes->SetUINT64(guidKey, unValue);
}

STDMETHODIMP
WMFByteStream::SetDouble(REFGUID guidKey, double fValue)
{
    assert(mAttributes);
    return mAttributes->SetDouble(guidKey, fValue);
}

STDMETHODIMP
WMFByteStream::SetGUID(REFGUID guidKey, REFGUID guidValue)
{
    assert(mAttributes);
    return mAttributes->SetGUID(guidKey, guidValue);
}

STDMETHODIMP
WMFByteStream::SetString(REFGUID guidKey, LPCWSTR wszValue)
{
    assert(mAttributes);
    return mAttributes->SetString(guidKey, wszValue);
}

STDMETHODIMP
WMFByteStream::SetBlob(REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize)
{
    assert(mAttributes);
    return mAttributes->SetBlob(guidKey, pBuf, cbBufSize);
}

STDMETHODIMP
WMFByteStream::SetUnknown(REFGUID guidKey, IUnknown* pUnknown)
{
    assert(mAttributes);
    return mAttributes->SetUnknown(guidKey, pUnknown);
}

STDMETHODIMP
WMFByteStream::LockStore()
{
    assert(mAttributes);
    return mAttributes->LockStore();
}

STDMETHODIMP
WMFByteStream::UnlockStore()
{
    assert(mAttributes);
    return mAttributes->UnlockStore();
}

STDMETHODIMP
WMFByteStream::GetCount(UINT32* pcItems)
{
    assert(mAttributes);
    return mAttributes->GetCount(pcItems);
}

STDMETHODIMP
WMFByteStream::GetItemByIndex(UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue)
{
    assert(mAttributes);
    return mAttributes->GetItemByIndex(unIndex, pguidKey, pValue);
}

STDMETHODIMP
WMFByteStream::CopyAllItems(IMFAttributes* pDest)
{
    assert(mAttributes);
    return mAttributes->CopyAllItems(pDest);
}

} // namespace mozilla
