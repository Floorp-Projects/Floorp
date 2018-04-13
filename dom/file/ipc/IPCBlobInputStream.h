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
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class IPCBlobInputStreamChild;

class IPCBlobInputStream final : public nsIAsyncInputStream
                               , public nsIInputStreamCallback
                               , public nsICloneableInputStreamWithRange
                               , public nsIIPCSerializableInputStream
                               , public nsIAsyncFileMetadata
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

  explicit IPCBlobInputStream(IPCBlobInputStreamChild* aActor);

  void
  StreamReady(already_AddRefed<nsIInputStream> aInputStream);

private:
  ~IPCBlobInputStream();

  nsresult
  EnsureAsyncRemoteStream();

  void
  InitWithExistingRange(uint64_t aStart, uint64_t aLength);

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

  nsCOMPtr<nsIInputStream> mRemoteStream;
  nsCOMPtr<nsIAsyncInputStream> mAsyncRemoteStream;

  // These 2 values are set only if mState is ePending.
  // They are protected by mutex.
  nsCOMPtr<nsIInputStreamCallback> mInputStreamCallback;
  nsCOMPtr<nsIEventTarget> mInputStreamCallbackEventTarget;

  // These 2 values are set only if mState is ePending.
  nsCOMPtr<nsIFileMetadataCallback> mFileMetadataCallback;
  nsCOMPtr<nsIEventTarget> mFileMetadataCallbackEventTarget;

  Mutex mMutex;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_IPCBlobInputStream_h
