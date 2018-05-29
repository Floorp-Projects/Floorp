/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_IPCBlobInputStream_h
#define mozilla_dom_ipc_IPCBlobInputStream_h

#include "mozilla/Mutex.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIFileStreams.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIInputStreamLength.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class IPCBlobInputStreamChild;

#define IPCBLOBINPUTSTREAM_IID \
  { 0xbcfa38fc, 0x8b7f, 0x4d79, \
    { 0xbe, 0x3a, 0x1e, 0x7b, 0xbe, 0x52, 0x38, 0xcd } }

class nsIIPCBlobInputStream : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(IPCBLOBINPUTSTREAM_IID)

  virtual nsIInputStream*
  GetInternalStream() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIPCBlobInputStream,
                              IPCBLOBINPUTSTREAM_IID)

class IPCBlobInputStream final : public nsIAsyncInputStream
                               , public nsIInputStreamCallback
                               , public nsICloneableInputStreamWithRange
                               , public nsIIPCSerializableInputStream
                               , public nsIAsyncFileMetadata
                               , public nsIInputStreamLength
                               , public nsIAsyncInputStreamLength
                               , public nsIIPCBlobInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAMWITHRANGE
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSIFILEMETADATA
  NS_DECL_NSIASYNCFILEMETADATA
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH

  explicit IPCBlobInputStream(IPCBlobInputStreamChild* aActor);

  void
  StreamReady(already_AddRefed<nsIInputStream> aInputStream);

  void
  LengthReady(int64_t aLength);

  // nsIIPCBlobInputStream
  nsIInputStream*
  GetInternalStream() const override
  {
    if (mRemoteStream) {
     return mRemoteStream;
    }

    if (mAsyncRemoteStream) {
      return mAsyncRemoteStream;
    }

    return nullptr;
  }

private:
  ~IPCBlobInputStream();

  nsresult
  EnsureAsyncRemoteStream(const MutexAutoLock& aProofOfLock);

  void
  InitWithExistingRange(uint64_t aStart, uint64_t aLength,
                        const MutexAutoLock& aProofOfLock);

  RefPtr<IPCBlobInputStreamChild> mActor;

  // This is the list of possible states.
  enum {
    // The initial state. Only ::Available() can be used without receiving an
    // error. The available size is known by the actor.
    eInit,

    // AsyncWait() has been called for the first time. SendStreamNeeded() has
    // been called and we are waiting for the 'real' inputStream.
    ePending,

    // When the child receives the stream from the parent, we move to this
    // state. The received stream is stored in mRemoteStream. From now on, any
    // method call will be forwared to mRemoteStream.
    eRunning,

    // If Close() or CloseWithStatus() is called, we move to this state.
    // mRemoveStream is released and any method will return
    // NS_BASE_STREAM_CLOSED.
    eClosed,
  } mState;

  uint64_t mStart;
  uint64_t mLength;

  // Set to true if the stream is used via Read/ReadSegments or Close.
  bool mConsumed;

  nsCOMPtr<nsIInputStream> mRemoteStream;
  nsCOMPtr<nsIAsyncInputStream> mAsyncRemoteStream;

  // These 2 values are set only if mState is ePending.
  nsCOMPtr<nsIInputStreamCallback> mInputStreamCallback;
  nsCOMPtr<nsIEventTarget> mInputStreamCallbackEventTarget;

  // These 2 values are set only if mState is ePending.
  nsCOMPtr<nsIFileMetadataCallback> mFileMetadataCallback;
  nsCOMPtr<nsIEventTarget> mFileMetadataCallbackEventTarget;

  // These 2 values are set only when nsIAsyncInputStreamLength::asyncWait() is
  // called.
  nsCOMPtr<nsIInputStreamLengthCallback> mLengthCallback;
  nsCOMPtr<nsIEventTarget> mLengthCallbackEventTarget;

  // Any member of this class is protected by mutex because touched on
  // multiple threads.
  Mutex mMutex;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_IPCBlobInputStream_h
