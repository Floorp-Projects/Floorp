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
#ifdef MOZ_DOM_STREAMS
#  include "mozilla/dom/BindingDeclarations.h"
#endif
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
class WeakWorkerRef;
class ReadableStream;
#ifdef MOZ_DOM_STREAMS
class ReadableStreamController;
#endif

class BodyStreamUnderlyingSourcePullCallbackHelper;
class BodyStreamUnderlyingSourceCancelCallbackHelper;
class BodyStreamUnderlyingSourceErrorCallbackHelper;

class BodyStreamHolder : public nsISupports {
  friend class BodyStream;
  friend class BodyStreamUnderlyingSourcePullCallbackHelper;
  friend class BodyStreamUnderlyingSourceCancelCallbackHelper;
  friend class BodyStreamUnderlyingSourceErrorCallbackHelper;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(BodyStreamHolder)

  BodyStreamHolder();

  virtual void NullifyStream() = 0;

  virtual void MarkAsRead() = 0;

#ifndef MOZ_DOM_STREAMS
  virtual void SetReadableStreamBody(JSObject* aBody) = 0;
  virtual JSObject* GetReadableStreamBody() = 0;
#else
  virtual void SetReadableStreamBody(ReadableStream* aBody) = 0;
  virtual ReadableStream* GetReadableStreamBody() = 0;
#endif

 protected:
  virtual ~BodyStreamHolder() = default;

 private:
  void StoreBodyStream(BodyStream* aBodyStream);
#ifdef MOZ_DOM_STREAMS
  already_AddRefed<BodyStream> TakeBodyStream() {
    MOZ_ASSERT_IF(mStreamCreated, mBodyStream);
    return mBodyStream.forget();
  }
#else
  void ForgetBodyStream();
#endif
  BodyStream* GetBodyStream() { return mBodyStream; }

#ifdef MOZ_DOM_STREAMS
  RefPtr<BodyStream> mBodyStream;
#else
  // Raw pointer because BodyStream keeps BodyStreamHolder alive and it
  // nullifies this stream before being released.
  BodyStream* mBodyStream;
#endif

#ifdef DEBUG
  bool mStreamCreated = false;
#endif
};

class BodyStream final : public nsIInputStreamCallback,
                         public nsIObserver,
                         public nsSupportsWeakReference
#ifndef MOZ_DOM_STREAMS
    ,
                         private JS::ReadableStreamUnderlyingSource
#endif
{
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

#ifdef MOZ_DOM_STREAMS
  static nsresult RetrieveInputStream(BodyStreamHolder* aStream,
                                      nsIInputStream** aInputStream);
#else
  static nsresult RetrieveInputStream(
      JS::ReadableStreamUnderlyingSource* aUnderlyingReadableStreamSource,
      nsIInputStream** aInputStream);
#endif

 private:
  BodyStream(nsIGlobalObject* aGlobal, BodyStreamHolder* aStreamHolder,
             nsIInputStream* aInputStream);
  ~BodyStream() = default;

#ifdef DEBUG
  void AssertIsOnOwningThread();
#else
  void AssertIsOnOwningThread() {}
#endif

#ifdef MOZ_DOM_STREAMS
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
                                  void* aBuffer, size_t aLength,
                                  size_t* aByteWritten);

  // This is a script boundary until Bug 1750605 is resolved and allows us
  // to replace this with MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void EnqueueChunkWithSizeIntoStream(
      JSContext* aCx, ReadableStream* aStream, uint64_t bytes,
      ErrorResult& aRv);

  void ErrorPropagation(JSContext* aCx, const MutexAutoLock& aProofOfLock,
                        ReadableStream* aStream, nsresult aRv);

  void CloseAndReleaseObjects(JSContext* aCx, const MutexAutoLock& aProofOfLock,
                              ReadableStream* aStream);
#else
  void requestData(JSContext* aCx, JS::HandleObject aStream,
                   size_t aDesiredSize) override;

  void writeIntoReadRequestBuffer(JSContext* aCx, JS::HandleObject aStream,
                                  void* aBuffer, size_t aLength,
                                  size_t* aBytesWritten) override;

  JS::Value cancel(JSContext* aCx, JS::HandleObject aStream,
                   JS::HandleValue aReason) override;

  void onClosed(JSContext* aCx, JS::HandleObject aStream) override;

  void onErrored(JSContext* aCx, JS::HandleObject aStream,
                 JS::HandleValue aReason) override;

  void finalize() override;

  void ErrorPropagation(JSContext* aCx, const MutexAutoLock& aProofOfLock,
                        JS::HandleObject aStream, nsresult aRv);

  void CloseAndReleaseObjects(JSContext* aCx, const MutexAutoLock& aProofOfLock,
                              JS::HandleObject aStream);
#endif

  class WorkerShutdown;

  void ReleaseObjects(const MutexAutoLock& aProofOfLock);

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
  Mutex mMutex;

  // Protected by mutex.
  State mState;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<BodyStreamHolder> mStreamHolder;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  // This is the original inputStream received during the CTOR. It will be
  // converted into an nsIAsyncInputStream and stored into mInputStream at the
  // first use.
  nsCOMPtr<nsIInputStream> mOriginalInputStream;
  nsCOMPtr<nsIAsyncInputStream> mInputStream;

  RefPtr<WeakWorkerRef> mWorkerRef;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BodyStream_h
