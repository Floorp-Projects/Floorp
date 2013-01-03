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

namespace mozilla {

class MediaResource;

// Wraps a MediaResource around an IMFByteStream interface, so that it can
// be used by the IMFSourceReader. Each WMFByteStream creates a WMF Work Queue
// on which blocking I/O is performed. The SourceReader requests reads
// asynchronously using {Begin,End}Read(). The synchronous I/O methods aren't
// used by the SourceReader, so they're not implemented on this class.
class WMFByteStream MOZ_FINAL : public IMFByteStream,
                                public IMFAsyncCallback
{
public:
  WMFByteStream(MediaResource* aResource);
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

  // IMFAsyncCallback methods.
  // We perform an async read operation in this callback implementation.
  STDMETHODIMP GetParameters(DWORD*, DWORD*);
  STDMETHODIMP Invoke(IMFAsyncResult* aResult);

private:

  // Id of the work queue upon which we perfrom reads. This
  // objects's Invoke() function is called on the work queue's thread,
  // and it's there that we perform blocking IO. This has value
  // MFASYNC_CALLBACK_QUEUE_UNDEFINED if the work queue hasn't been
  // created yet.
  DWORD mWorkQueueId;

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

  // Resource we're wrapping. Note this object's methods are threadsafe,
  // and we only call read/seek on the work queue's thread.
  MediaResource* mResource;

  // Protects mOffset, which is accessed by the SourceReaders thread(s), and
  // on the work queue thread.
  ReentrantMonitor mReentrantMonitor;

  // Current offset of the logical read cursor. We maintain this separately
  // from the media resource's offset since a partially complete read (in Invoke())
  // would leave the resource's offset at a value unexpected by the caller,
  // since the read hadn't yet completed.
  int64_t mOffset;

  // IUnknown ref counting.
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

} // namespace mozilla

#endif
