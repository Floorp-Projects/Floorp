/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchStreamReader_h
#define mozilla_dom_FetchStreamReader_h

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FetchBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsIAsyncOutputStream.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

class ReadableStream;
class ReadableStreamDefaultReader;
class StrongWorkerRef;

class FetchStreamReader final : public nsIOutputStreamCallback {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FetchStreamReader)
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  // This creates a nsIInputStream able to retrieve data from the ReadableStream
  // object. The reading starts when StartConsuming() is called.
  static nsresult Create(JSContext* aCx, nsIGlobalObject* aGlobal,
                         FetchStreamReader** aStreamReader,
                         nsIInputStream** aInputStream);

  MOZ_CAN_RUN_SCRIPT
  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT
  void CloseSteps(JSContext* aCx, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT
  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> aError,
                  ErrorResult& aRv);

  // Idempotently close the output stream and null out all state. If aCx is
  // provided, the reader will also be canceled.  aStatus must be a DOM error
  // as understood by DOMException because it will be provided as the
  // cancellation reason.
  //
  // This is a script boundary minimize annotation changes required while
  // we figure out how to handle some more tricky annotation cases (for
  // example, the destructor of this class. Tracking under Bug 1750656)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void CloseAndRelease(JSContext* aCx, nsresult aStatus);

  void StartConsuming(JSContext* aCx, ReadableStream* aStream,
                      ErrorResult& aRv);

 private:
  explicit FetchStreamReader(nsIGlobalObject* aGlobal);
  ~FetchStreamReader();

  nsresult MaybeGrabStrongWorkerRef(JSContext* aCx);

  nsresult WriteBuffer();

  // Attempt to copy data from mBuffer into mPipeOut. Returns `true` if data was
  // written, and AsyncWait callbacks or FetchReadRequest calls have been set up
  // to write more data in the future, and `false` otherwise.
  MOZ_CAN_RUN_SCRIPT
  bool Process(JSContext* aCx);

  void ReportErrorToConsole(JSContext* aCx, JS::Handle<JS::Value> aValue);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  nsCOMPtr<nsIAsyncOutputStream> mPipeOut;

  RefPtr<StrongWorkerRef> mWorkerRef;
  // This is an additional refcount we add to `mWorkerRef` when we have a
  // pending callback from mPipeOut.AsyncWait() which is guaranteed to fire when
  // either we can write to the pipe or the stream has been closed.  Because
  // this callback must run on our owning worker thread, we must ensure that the
  // worker thread lives long enough to process the runnable (and potentially
  // release the last reference to this non-thread-safe object on this thread).
  //
  // By holding an additional refcount we can avoid creating a mini state
  // machine around mWorkerRef which hopefully improves clarity.
  RefPtr<StrongWorkerRef> mAsyncWaitWorkerRef;

  RefPtr<ReadableStreamDefaultReader> mReader;

  nsTArray<uint8_t> mBuffer;
  uint32_t mBufferRemaining = 0;
  uint32_t mBufferOffset = 0;

  bool mHasOutstandingReadRequest = false;
  bool mStreamClosed = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FetchStreamReader_h
