/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BodyStream_h
#define mozilla_dom_BodyStream_h

#include "jsapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsIAsyncInputStream.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsNetCID.h"
#include "nsWeakReference.h"

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

  virtual void NullifyStream() { mReadableStreamBody = nullptr; }

  virtual void MarkAsRead() {}

  void SetReadableStreamBody(ReadableStream* aBody) {
    mReadableStreamBody = aBody;
  }
  ReadableStream* GetReadableStreamBody() { return mReadableStreamBody; }

 protected:
  virtual ~BodyStreamHolder() = default;

  // This is the ReadableStream exposed to content. It's underlying source is a
  // BodyStream object.
  RefPtr<ReadableStream> mReadableStreamBody;

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
                         public nsSupportsWeakReference {
  friend class BodyStreamHolder;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOBSERVER

  // This method creates a JS ReadableStream object and it assigns it to the
  // aStreamHolder calling SetReadableStreamBody().
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static void Create(JSContext* aCx, BodyStreamHolder* aStreamHolder,
                     nsIGlobalObject* aGlobal, nsIInputStream* aInputStream,
                     ErrorResult& aRv);

  void Close();

  static nsresult RetrieveInputStream(BodyStreamHolder* aStream,
                                      nsIInputStream** aInputStream);

 private:
  BodyStream(nsIGlobalObject* aGlobal, BodyStreamHolder* aStreamHolder,
             nsIInputStream* aInputStream);
  ~BodyStream();

 public:
  // Pull Callback
  already_AddRefed<Promise> PullCallback(JSContext* aCx,
                                         ReadableStreamController& aController,
                                         ErrorResult& aRv);

  void CloseInputAndReleaseObjects();

 private:
  // Fills a buffer with bytes from the stream.
  void WriteIntoReadRequestBuffer(JSContext* aCx, ReadableStream* aStream,
                                  JS::Handle<JSObject*> aBuffer,
                                  uint32_t aLength, uint32_t* aByteWritten);

  // This is a script boundary until Bug 1750605 is resolved and allows us
  // to replace this with MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void EnqueueChunkWithSizeIntoStream(
      JSContext* aCx, ReadableStream* aStream, uint64_t aAvailableData,
      ErrorResult& aRv);

  void ErrorPropagation(JSContext* aCx, ReadableStream* aStream,
                        nsresult aError);

  // TODO: convert this to MOZ_CAN_RUN_SCRIPT (bug 1750605)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void CloseAndReleaseObjects(
      JSContext* aCx, ReadableStream* aStream);

  class WorkerShutdown;

  void ReleaseObjects();

  // The closed state should ultimately be managed by the source algorithms
  // class. See also bug 1815997.
  bool IsClosed() { return !mStreamHolder; }

  // Common methods

  // mGlobal is set on creation, and isn't modified off the owning thread.
  // It isn't set to nullptr until ReleaseObjects() runs.
  nsCOMPtr<nsIGlobalObject> mGlobal;
  // Same for mStreamHolder. mStreamHolder being nullptr means the stream is
  // closed.
  RefPtr<BodyStreamHolder> mStreamHolder;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  // Same as mGlobal but set to nullptr on OnInputStreamReady (on the owning
  // thread).
  // This promise is created by PullCallback and resolved when
  // OnInputStreamReady succeeds. No need to try hard to settle it though, see
  // also ReleaseObjects() for the reason.
  RefPtr<Promise> mPullPromise;

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
