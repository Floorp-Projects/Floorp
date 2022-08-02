/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStream_h
#define mozilla_RemoteLazyInputStream_h

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Mutex.h"
#include "mozIRemoteLazyInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIFileStreams.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIInputStreamLength.h"
#include "nsCOMPtr.h"

namespace mozilla {

class RemoteLazyInputStreamChild;

class RemoteLazyInputStream final : public nsIAsyncInputStream,
                                    public nsIInputStreamCallback,
                                    public nsICloneableInputStreamWithRange,
                                    public nsIIPCSerializableInputStream,
                                    public nsIAsyncFileMetadata,
                                    public nsIInputStreamLength,
                                    public nsIAsyncInputStreamLength,
                                    public mozIRemoteLazyInputStream {
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

  // Create a new lazy RemoteLazyInputStream, and move the provided aInputStream
  // into storage as referenced by it. May only be called in processes with
  // RemoteLazyInputStreamStorage.
  static already_AddRefed<RemoteLazyInputStream> WrapStream(
      nsIInputStream* aInputStream);

  // mozIRemoteLazyInputStream
  NS_IMETHOD TakeInternalStream(nsIInputStream** aStream) override;
  NS_IMETHOD GetInternalStreamID(nsID& aID) override;

 private:
  friend struct IPC::ParamTraits<mozilla::RemoteLazyInputStream*>;

  // Constructor for an already-closed RemoteLazyInputStream.
  RemoteLazyInputStream() = default;

  explicit RemoteLazyInputStream(RemoteLazyInputStreamChild* aActor,
                                 uint64_t aStart = 0,
                                 uint64_t aLength = UINT64_MAX);

  explicit RemoteLazyInputStream(nsIInputStream* aStream);

  ~RemoteLazyInputStream();

  void StreamNeeded() MOZ_REQUIRES(mMutex);

  // Upon receiving the stream from our actor, we will not wrap it into an async
  // stream until needed. This allows callers to get access to the underlying
  // potentially-sync stream using `TakeInternalStream` before reading.
  nsresult EnsureAsyncRemoteStream() MOZ_REQUIRES(mMutex);

  // Note that data has been read from our input stream, and disconnect from our
  // remote actor.
  void MarkConsumed();

  void IPCWrite(IPC::MessageWriter* aWriter);
  static already_AddRefed<RemoteLazyInputStream> IPCRead(
      IPC::MessageReader* aReader);

  // Helper method to generate a description of a stream for use in loggging.
  nsCString Describe() MOZ_REQUIRES(mMutex);

  // Start and length of the slice to apply on this RemoteLazyInputStream when
  // fetching the underlying stream with `SendStreamNeeded`.
  const uint64_t mStart = 0;
  const uint64_t mLength = UINT64_MAX;

  // Any non-const member of this class is protected by mutex because it is
  // touched on multiple threads.
  Mutex mMutex{"RemoteLazyInputStream::mMutex"};

  // This is the list of possible states.
  enum {
    // The initial state. Only ::Available() can be used without receiving an
    // error. The available size is known by the actor.
    eInit,

    // AsyncWait() has been called for the first time. SendStreamNeeded() has
    // been called and we are waiting for the 'real' inputStream.
    ePending,

    // When the child receives the stream from the parent, we move to this
    // state. The received stream is stored in mInnerStream. From now on, any
    // method call will be forwared to mInnerStream or mAsyncInnerStream.
    eRunning,

    // If Close() or CloseWithStatus() is called, we move to this state.
    // mInnerStream is released and any method will return
    // NS_BASE_STREAM_CLOSED.
    eClosed,
  } mState MOZ_GUARDED_BY(mMutex) = eClosed;

  // The actor which will be used to provide the underlying stream or length
  // information when needed, as well as to efficiently allow transferring the
  // stream over IPC.
  //
  // The connection to our actor will be cleared once the stream has been closed
  // or has started reading, at which point this stream will be serialized and
  // cloned as-if it was the underlying stream.
  RefPtr<RemoteLazyInputStreamChild> mActor MOZ_GUARDED_BY(mMutex);

  nsCOMPtr<nsIInputStream> mInnerStream MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIAsyncInputStream> mAsyncInnerStream MOZ_GUARDED_BY(mMutex);

  // These 2 values are set only if mState is ePending or eRunning.
  // RefPtr is used instead of nsCOMPtr to avoid invoking QueryInterface when
  // assigning in debug builds, as `mInputStreamCallback` may not be threadsafe.
  RefPtr<nsIInputStreamCallback> mInputStreamCallback MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIEventTarget> mInputStreamCallbackEventTarget
      MOZ_GUARDED_BY(mMutex);
  uint32_t mInputStreamCallbackFlags MOZ_GUARDED_BY(mMutex) = 0;
  uint32_t mInputStreamCallbackRequestedCount MOZ_GUARDED_BY(mMutex) = 0;

  // These 2 values are set only if mState is ePending.
  nsCOMPtr<nsIFileMetadataCallback> mFileMetadataCallback
      MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIEventTarget> mFileMetadataCallbackEventTarget
      MOZ_GUARDED_BY(mMutex);
};

}  // namespace mozilla

template <>
struct IPC::ParamTraits<mozilla::RemoteLazyInputStream*> {
  static void Write(IPC::MessageWriter* aWriter,
                    mozilla::RemoteLazyInputStream* aParam);
  static bool Read(IPC::MessageReader* aReader,
                   RefPtr<mozilla::RemoteLazyInputStream>* aResult);
};

#endif  // mozilla_RemoteLazyInputStream_h
