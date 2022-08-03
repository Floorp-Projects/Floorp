/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BodyStream_h
#define mozilla_dom_BodyStream_h

#include "jsapi.h"
#include "js/Stream.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsIAsyncInputStream.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsNetCID.h"
#include "nsWeakReference.h"
#include "mozilla/Mutex.h"

class nsIGlobalObject;

class nsIInputStream;

namespace mozilla {
class ErrorResult;

namespace dom {

class BodyStream;
class StrongWorkerRef;
class ReadableStream;
class ReadableStreamController;

class BodyStreamUnderlyingSourceAlgorithms;

class BodyStreamHolder : public nsISupports {
  friend class BodyStream;
  friend class BodyStreamUnderlyingSourceAlgorithms;
  friend class BodyStreamUnderlyingSourceErrorCallbackHelper;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(BodyStreamHolder)

  BodyStreamHolder();

  virtual void NullifyStream() = 0;

  virtual void MarkAsRead() = 0;

  virtual void SetReadableStreamBody(ReadableStream* aBody) = 0;
  virtual ReadableStream* GetReadableStreamBody() = 0;

 protected:
  virtual ~BodyStreamHolder() = default;

 private:
  void StoreBodyStream(BodyStream* aBodyStream);
  already_AddRefed<BodyStream> TakeBodyStream() {
    MOZ_ASSERT_IF(mStreamCreated, mBodyStream);
    return mBodyStream.forget();
  }
  BodyStream* GetBodyStream() { return mBodyStream; }

  RefPtr<BodyStream> mBodyStream;

#ifdef DEBUG
  bool mStreamCreated = false;
#endif
};

class BodyStream final : public nsIInputStreamCallback,
                         public nsIObserver,
                         public nsSupportsWeakReference,
                         public SingleWriterLockOwner {
  friend class BodyStreamHolder;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOBSERVER

  // This method creates a JS ReadableStream object and it assigns it to the
  // aStreamHolder calling SetReadableStreamBody().
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static void Create(JSContext* aCx, BodyStreamHolder* aStreamHolder,
                     nsIGlobalObject* aGlobal, nsIInputStream* aInputStream,
                     ErrorResult& aRv);

  void Close();

  bool OnWritingThread() const override {
#ifdef MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED
    return _mOwningThread.IsCurrentThread();
#else
    return true;
#endif
  }

  static nsresult RetrieveInputStream(BodyStreamHolder* aStream,
                                      nsIInputStream** aInputStream);

 private:
  BodyStream(nsIGlobalObject* aGlobal, BodyStreamHolder* aStreamHolder,
             nsIInputStream* aInputStream);
  ~BodyStream() = default;

#ifdef DEBUG
  void AssertIsOnOwningThread() const;
#else
  void AssertIsOnOwningThread() const {}
#endif

 public:
  // Cancel Callback
  already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv);

  // Pull Callback
  already_AddRefed<Promise> PullCallback(JSContext* aCx,
                                         ReadableStreamController& aController,
                                         ErrorResult& aRv);

  void ErrorCallback();

 private:
  // Fills a buffer with bytes from the stream.
  void WriteIntoReadRequestBuffer(JSContext* aCx, ReadableStream* aStream,
                                  JS::Handle<JSObject*> aBuffer,
                                  uint32_t aLength, uint32_t* aByteWritten);

  // This is a script boundary until Bug 1750605 is resolved and allows us
  // to replace this with MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void EnqueueChunkWithSizeIntoStream(
      JSContext* aCx, ReadableStream* aStream, uint64_t bytes,
      ErrorResult& aRv);

  void ErrorPropagation(JSContext* aCx,
                        const MutexSingleWriterAutoLock& aProofOfLock,
                        ReadableStream* aStream, nsresult aRv)
      MOZ_REQUIRES(mMutex);

  // TODO: convert this to MOZ_CAN_RUN_SCRIPT (bug 1750605)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void CloseAndReleaseObjects(
      JSContext* aCx, const MutexSingleWriterAutoLock& aProofOfLock,
      ReadableStream* aStream) MOZ_REQUIRES(mMutex);

  class WorkerShutdown;

  void ReleaseObjects(const MutexSingleWriterAutoLock& aProofOfLock)
      MOZ_REQUIRES(mMutex);

  void ReleaseObjects();

  // Common methods

  enum State {
    // This is the beginning state before any reading operation.
    eInitializing,

    // RequestDataCallback has not been called yet. We haven't started to read
    // data from the stream yet.
    eWaiting,

    // We are reading data in a separate I/O thread.
    eReading,

    // We are ready to write something in the JS Buffer.
    eWriting,

    // After a writing, we want to check if the stream is closed. After the
    // check, we go back to eWaiting. If a reading request happens in the
    // meantime, we move to eReading state.
    eChecking,

    // Operation completed.
    eClosed,
  };

  // We need a mutex because JS engine can release BodyStream on a non-owning
  // thread. We must be sure that the releasing of resources doesn't trigger
  // race conditions.
  MutexSingleWriter mMutex;

  // Protected by mutex.
  State mState MOZ_GUARDED_BY(mMutex);  // all writes are from the owning thread

  // mGlobal is set on creation, and isn't modified off the owning thread.
  // It isn't set to nullptr until ReleaseObjects() runs.
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<BodyStreamHolder> mStreamHolder MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  // This is the original inputStream received during the CTOR. It will be
  // converted into an nsIAsyncInputStream and stored into mInputStream at the
  // first use.
  nsCOMPtr<nsIInputStream> mOriginalInputStream;
  nsCOMPtr<nsIAsyncInputStream> mInputStream;

  RefPtr<StrongWorkerRef> mWorkerRef;
  RefPtr<StrongWorkerRef> mAsyncWaitWorkerRef;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BodyStream_h
