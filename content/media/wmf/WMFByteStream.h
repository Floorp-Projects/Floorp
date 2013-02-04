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

class nsIThreadPool;

namespace mozilla {

class MediaResource;
class AsyncReadRequestState;

// Wraps a MediaResource around an IMFByteStream interface, so that it can
// be used by the IMFSourceReader. Each WMFByteStream creates a WMF Work Queue
// on which blocking I/O is performed. The SourceReader requests reads
// asynchronously using {Begin,End}Read(). The synchronous I/O methods aren't
// used by the SourceReader, so they're not implemented on this class.
class WMFByteStream MOZ_FINAL : public IMFByteStream
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

  // We perform an async read operation in this callback implementation.
  void PerformRead(IMFAsyncResult* aResult, AsyncReadRequestState* aRequestState);

private:

  // Reference to the thread pool in which we perform the reads asynchronously.
  // Note this is pool is shared amongst all active WMFByteStreams.
  nsCOMPtr<nsIThreadPool> mThreadPool;

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

  // True if the resource has been shutdown, either because the WMFReader is
  // shutting down, or because the underlying MediaResource has closed.
  bool mIsShutdown;

  // IUnknown ref counting.
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

} // namespace mozilla

#endif
