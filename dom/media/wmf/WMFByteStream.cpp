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
#include "nsComponentManagerUtils.h"
#include "mozilla/DebugOnly.h"
#include "SharedThreadPool.h"
#include <algorithm>
#include <cassert>

namespace mozilla {

PRLogModuleInfo* gWMFByteStreamLog = nullptr;
#define WMF_BS_LOG(...) MOZ_LOG(gWMFByteStreamLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

WMFByteStream::WMFByteStream(MediaResource* aResource,
                             WMFSourceReaderCallback* aSourceReaderCallback)
  : mSourceReaderCallback(aSourceReaderCallback),
    mResource(aResource),
    mReentrantMonitor("WMFByteStream.Data"),
    mOffset(0),
    mIsShutdown(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
  NS_ASSERTION(mSourceReaderCallback, "Must have a source reader callback.");

  if (!gWMFByteStreamLog) {
    gWMFByteStreamLog = PR_NewLogModule("WMFByteStream");
  }
  WMF_BS_LOG("[%p] WMFByteStream CTOR", this);
  MOZ_COUNT_CTOR(WMFByteStream);
}

WMFByteStream::~WMFByteStream()
{
  MOZ_COUNT_DTOR(WMFByteStream);
  WMF_BS_LOG("[%p] WMFByteStream DTOR", this);
}

nsresult
WMFByteStream::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  mThreadPool = SharedThreadPool::Get(NS_LITERAL_CSTRING("WMFByteStream IO"), 4);
  NS_ENSURE_TRUE(mThreadPool, NS_ERROR_FAILURE);

  NS_ConvertUTF8toUTF16 contentTypeUTF16(mResource->GetContentType());
  if (!contentTypeUTF16.IsEmpty()) {
    HRESULT hr = wmf::MFCreateAttributes(byRef(mAttributes), 1);
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

    hr = mAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE,
                                contentTypeUTF16.get());
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

    WMF_BS_LOG("[%p] WMFByteStream has Content-Type=%s", this, mResource->GetContentType().get());
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
  WMF_BS_LOG("[%p] WMFByteStream::QueryInterface %s", this, GetGUIDName(aIId).get());

  if (aIId == IID_IMFByteStream) {
    return DoGetInterface(static_cast<IMFByteStream*>(this), aInterface);
  }
  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IMFByteStream*>(this), aInterface);
  }
  if (aIId == IID_IMFAttributes) {
    return DoGetInterface(static_cast<IMFAttributes*>(this), aInterface);
  }

  *aInterface = nullptr;
  return E_NOINTERFACE;
}

NS_IMPL_ADDREF(WMFByteStream)
NS_IMPL_RELEASE(WMFByteStream)


// Stores data regarding an async read opreation.
class ReadRequest final : public IUnknown {
  ~ReadRequest() {}

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
  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

NS_IMPL_ADDREF(ReadRequest)
NS_IMPL_RELEASE(ReadRequest)

// IUnknown Methods
STDMETHODIMP
ReadRequest::QueryInterface(REFIID aIId, void **aInterface)
{
  WMF_BS_LOG("ReadRequest::QueryInterface %s", GetGUIDName(aIId).get());

  if (aIId == IID_IUnknown) {
    return DoGetInterface(static_cast<IUnknown*>(this), aInterface);
  }

  *aInterface = nullptr;
  return E_NOINTERFACE;
}

class ProcessReadRequestEvent final : public nsRunnable {
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
  WMF_BS_LOG("[%p] WMFByteStream::BeginRead() mOffset=%lld tell=%lld length=%lu mIsShutdown=%d",
             this, mOffset, mResource->Tell(), aLength, mIsShutdown);

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
  // Read in a loop to ensure we fill the buffer, when possible.
  ULONG totalBytesRead = 0;
  nsresult rv = NS_OK;
  while (totalBytesRead < aRequestState->mBufferLength) {
    BYTE* buffer = aRequestState->mBuffer + totalBytesRead;
    ULONG bytesRead = 0;
    ULONG length = aRequestState->mBufferLength - totalBytesRead;
    rv = mResource->ReadAt(aRequestState->mOffset + totalBytesRead,
                           reinterpret_cast<char*>(buffer),
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
    WMF_BS_LOG("[%p] WMFByteStream::ProcessReadRequest() read offset greater than length, soft-failing read", this);
    return;
  }

  nsresult rv = Read(aRequestState);
  if (NS_FAILED(rv)) {
    Shutdown();
    aResult->SetStatus(E_ABORT);
  } else {
    aResult->SetStatus(S_OK);
  }

  WMF_BS_LOG("[%p] WMFByteStream::ProcessReadRequest() read %d at %lld finished rv=%x",
             this, aRequestState->mBytesRead, aRequestState->mOffset, rv);

  // Let caller know read is complete.
  DebugOnly<HRESULT> hr = wmf::MFInvokeCallback(aResult);
  NS_ASSERTION(SUCCEEDED(hr), "Failed to invoke callback!");
}

STDMETHODIMP
WMFByteStream::BeginWrite(const BYTE *, ULONG ,
                          IMFAsyncCallback *,
                          IUnknown *)
{
  WMF_BS_LOG("[%p] WMFByteStream::BeginWrite()", this);
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Close()
{
  WMF_BS_LOG("[%p] WMFByteStream::Close()", this);
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

  WMF_BS_LOG("[%p] WMFByteStream::EndRead() offset=%lld *aBytesRead=%u mOffset=%lld status=0x%x hr=0x%x eof=%d",
             this, requestState->mOffset, *aBytesRead, mOffset, aResult->GetStatus(), hr, IsEOS());

  return aResult->GetStatus();
}

STDMETHODIMP
WMFByteStream::EndWrite(IMFAsyncResult *, ULONG *)
{
  WMF_BS_LOG("[%p] WMFByteStream::EndWrite()", this);
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Flush()
{
  WMF_BS_LOG("[%p] WMFByteStream::Flush()", this);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetCapabilities(DWORD *aCapabilities)
{
  WMF_BS_LOG("[%p] WMFByteStream::GetCapabilities()", this);
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
  WMF_BS_LOG("[%p] WMFByteStream::GetCurrentPosition() %lld", this, mOffset);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::GetLength(QWORD *aLength)
{
  NS_ENSURE_TRUE(aLength, E_POINTER);
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  *aLength = mResource->GetLength();
  WMF_BS_LOG("[%p] WMFByteStream::GetLength() %lld", this, *aLength);
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
  WMF_BS_LOG("[%p] WMFByteStream::IsEndOfStream() %d", this, *aEndOfStream);
  return S_OK;
}

STDMETHODIMP
WMFByteStream::Read(BYTE* aBuffer, ULONG aBufferLength, ULONG* aOutBytesRead)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  nsRefPtr<ReadRequest> request = new ReadRequest(mOffset, aBuffer, aBufferLength);
  if (NS_FAILED(Read(request))) {
    WMF_BS_LOG("[%p] WMFByteStream::Read() offset=%lld failed!", this, mOffset);
    return E_FAIL;
  }
  if (aOutBytesRead) {
    *aOutBytesRead = request->mBytesRead;
  }
  WMF_BS_LOG("[%p] WMFByteStream::Read() offset=%lld length=%u bytesRead=%u",
             this, mOffset, aBufferLength, request->mBytesRead);
  mOffset += request->mBytesRead;
  return S_OK;
}

STDMETHODIMP
WMFByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN aSeekOrigin,
                    LONGLONG aSeekOffset,
                    DWORD aSeekFlags,
                    QWORD *aCurrentPosition)
{
  WMF_BS_LOG("[%p] WMFByteStream::Seek(%d, %lld)", this, aSeekOrigin, aSeekOffset);

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
  WMF_BS_LOG("[%p] WMFByteStream::SetCurrentPosition(%lld)",
             this, aPosition);

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
  WMF_BS_LOG("[%p] WMFByteStream::SetLength()", this);
  return E_NOTIMPL;
}

STDMETHODIMP
WMFByteStream::Write(const BYTE *, ULONG, ULONG *)
{
  WMF_BS_LOG("[%p] WMFByteStream::Write()", this);
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
