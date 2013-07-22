/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WMFByteStream_h_)
#define WMFByteStream_h_

#include "WMF.h"

#include "nsISupportsImpl.h"
#include "nsCOMPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

class nsIThreadPool;

namespace mozilla {

class MediaResource;
class ReadRequest;
class WMFSourceReaderCallback;

// Wraps a MediaResource around an IMFByteStream interface, so that it can
// be used by the IMFSourceReader. Each WMFByteStream creates a WMF Work Queue
// on which blocking I/O is performed. The SourceReader requests reads
// asynchronously using {Begin,End}Read(), and more rarely synchronously
// using Read().
//
// Note: This implementation attempts to be bug-compatible with Windows Media
//       Foundation's implementation of IMFByteStream. The behaviour of WMF's
//       IMFByteStream was determined by creating it and testing the edge cases.
//       For details see the test code at:
//       https://github.com/cpearce/IMFByteStreamBehaviour/
class WMFByteStream MOZ_FINAL : public IMFByteStream
                              , public IMFAttributes
{
public:
  WMFByteStream(MediaResource* aResource, WMFSourceReaderCallback* aCallback);
  ~WMFByteStream();

  nsresult Init();
  nsresult Shutdown();

  // IUnknown Methods.
  STDMETHODIMP QueryInterface(REFIID aIId, LPVOID *aInterface);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // IMFByteStream Methods.
  STDMETHODIMP BeginRead(BYTE *aBuffer,
                         ULONG aLength,
                         IMFAsyncCallback *aCallback,
                         IUnknown *aCallerState);
  STDMETHODIMP BeginWrite(const BYTE *, ULONG ,
                          IMFAsyncCallback *,
                          IUnknown *);
  STDMETHODIMP Close();
  STDMETHODIMP EndRead(IMFAsyncResult* aResult, ULONG *aBytesRead);
  STDMETHODIMP EndWrite(IMFAsyncResult *, ULONG *);
  STDMETHODIMP Flush();
  STDMETHODIMP GetCapabilities(DWORD *aCapabilities);
  STDMETHODIMP GetCurrentPosition(QWORD *aPosition);
  STDMETHODIMP GetLength(QWORD *pqwLength);
  STDMETHODIMP IsEndOfStream(BOOL *aIsEndOfStream);
  STDMETHODIMP Read(BYTE *, ULONG, ULONG *);
  STDMETHODIMP Seek(MFBYTESTREAM_SEEK_ORIGIN aSeekOrigin,
                    LONGLONG aSeekOffset,
                    DWORD aSeekFlags,
                    QWORD *aCurrentPosition);
  STDMETHODIMP SetCurrentPosition(QWORD aPosition);
  STDMETHODIMP SetLength(QWORD);
  STDMETHODIMP Write(const BYTE *, ULONG, ULONG *);

  // IMFAttributes methods
  STDMETHODIMP GetItem(REFGUID guidKey, PROPVARIANT* pValue);
  STDMETHODIMP GetItemType(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType);
  STDMETHODIMP CompareItem(REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult);
  STDMETHODIMP Compare(IMFAttributes* pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL* pbResult);
  STDMETHODIMP GetUINT32(REFGUID guidKey, UINT32* punValue);
  STDMETHODIMP GetUINT64(REFGUID guidKey, UINT64* punValue);
  STDMETHODIMP GetDouble(REFGUID guidKey, double* pfValue);
  STDMETHODIMP GetGUID(REFGUID guidKey, GUID* pguidValue);
  STDMETHODIMP GetStringLength(REFGUID guidKey, UINT32* pcchLength);
  STDMETHODIMP GetString(REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32* pcchLength);
  STDMETHODIMP GetAllocatedString(REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength);
  STDMETHODIMP GetBlobSize(REFGUID guidKey, UINT32* pcbBlobSize);
  STDMETHODIMP GetBlob(REFGUID guidKey, UINT8* pBuf, UINT32 cbBufSize, UINT32* pcbBlobSize);
  STDMETHODIMP GetAllocatedBlob(REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize);
  STDMETHODIMP GetUnknown(REFGUID guidKey, REFIID riid, LPVOID* ppv);
  STDMETHODIMP SetItem(REFGUID guidKey, REFPROPVARIANT Value);
  STDMETHODIMP DeleteItem(REFGUID guidKey);
  STDMETHODIMP DeleteAllItems();
  STDMETHODIMP SetUINT32(REFGUID guidKey, UINT32 unValue);
  STDMETHODIMP SetUINT64(REFGUID guidKey,UINT64 unValue);
  STDMETHODIMP SetDouble(REFGUID guidKey, double fValue);
  STDMETHODIMP SetGUID(REFGUID guidKey, REFGUID guidValue);
  STDMETHODIMP SetString(REFGUID guidKey, LPCWSTR wszValue);
  STDMETHODIMP SetBlob(REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize);
  STDMETHODIMP SetUnknown(REFGUID guidKey, IUnknown* pUnknown);
  STDMETHODIMP LockStore();
  STDMETHODIMP UnlockStore();
  STDMETHODIMP GetCount(UINT32* pcItems);
  STDMETHODIMP GetItemByIndex(UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue);
  STDMETHODIMP CopyAllItems(IMFAttributes* pDest);

  // We perform an async read operation in this callback implementation.
  // Processes an async read request, storing the result in aResult, and
  // notifying the caller when the read operation is complete.
  void ProcessReadRequest(IMFAsyncResult* aResult,
                          ReadRequest* aRequestState);

  // Returns the number of bytes that have been consumed by the users of this
  // class since the last time we called this, and resets the internal counter.
  uint32_t GetAndResetBytesConsumedCount();

private:

  // Locks the MediaResource and performs the read. The other read methods
  // call this function.
  nsresult Read(ReadRequest* aRequestState);

  // Returns true if the current position of the stream is at end of stream.
  bool IsEOS();

  // Reference to the thread pool in which we perform the reads asynchronously.
  // Note this is pool is shared amongst all active WMFByteStreams.
  nsCOMPtr<nsIThreadPool> mThreadPool;

  // Reference to the source reader's callback. We use this reference to
  // notify threads waiting on a ReadSample() callback to stop waiting
  // if the stream is closed, which happens when the media element is
  // shutdown.
  RefPtr<WMFSourceReaderCallback> mSourceReaderCallback;

  // Resource we're wrapping.
  nsRefPtr<MediaResource> mResource;

  // Protects mOffset, which is accessed by the SourceReaders thread(s), and
  // on the work queue thread.
  ReentrantMonitor mReentrantMonitor;

  // Current offset of the logical read cursor. We maintain this separately
  // from the media resource's offset since a partially complete read (in Invoke())
  // would leave the resource's offset at a value unexpected by the caller,
  // since the read hadn't yet completed.
  int64_t mOffset;

  // We implement IMFAttributes by forwarding all calls to an instance of the
  // standard IMFAttributes class, which we store a reference to here.
  RefPtr<IMFAttributes> mAttributes;

  // Number of bytes that have been consumed by callers of the read functions
  // on this object since the last time GetAndResetBytesConsumedCount() was
  // called.
  uint32_t mBytesConsumed;

  // True if the resource has been shutdown, either because the WMFReader is
  // shutting down, or because the underlying MediaResource has closed.
  bool mIsShutdown;

  // IUnknown ref counting.
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

} // namespace mozilla

#endif
