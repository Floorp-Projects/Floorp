/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WritableStream_h
#define mozilla_dom_WritableStream_h

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/WritableStreamDefaultController.h"
#include "mozilla/dom/WritableStreamDefaultWriter.h"

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#ifndef MOZ_DOM_STREAMS
#  error "Shouldn't be compiling with this header without MOZ_DOM_STREAMS set"
#endif

namespace mozilla::dom {

class Promise;
class WritableStreamDefaultController;
class WritableStreamDefaultWriter;

class WritableStream final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WritableStream)

 protected:
  ~WritableStream();

 public:
  explicit WritableStream(const GlobalObject& aGlobal);
  explicit WritableStream(nsIGlobalObject* aGlobal);

  enum class WriterState { Writable, Closed, Erroring, Errored };

  // Slot Getter/Setters:
 public:
  bool Backpressure() const { return mBackpressure; }
  void SetBackpressure(bool aBackpressure) { mBackpressure = aBackpressure; }

  Promise* GetCloseRequest() { return mCloseRequest; }
  void SetCloseRequest(Promise* aRequest) { mCloseRequest = aRequest; }

  WritableStreamDefaultController* Controller() { return mController; }
  void SetController(WritableStreamDefaultController* aController) {
    MOZ_ASSERT(aController);
    mController = aController;
  }

  Promise* GetInFlightWriteRequest() const { return mInFlightWriteRequest; }

  Promise* GetPendingAbortRequestPromise() const {
    return mPendingAbortRequestPromise;
  }

  void SetPendingAbortRequest(Promise* aPromise, JS::Handle<JS::Value> aReason,
                              bool aWasAlreadyErroring) {
    mPendingAbortRequestPromise = aPromise;
    mPendingAbortRequestReason = aReason;
    mPendingAbortRequestWasAlreadyErroring = aWasAlreadyErroring;
  }

  WritableStreamDefaultWriter* GetWriter() const { return mWriter; }
  void SetWriter(WritableStreamDefaultWriter* aWriter) { mWriter = aWriter; }

  WriterState State() const { return mState; }
  void SetState(const WriterState& aState) { mState = aState; }

  JS::Value StoredError() const { return mStoredError; }
  void SetStoredError(JS::HandleValue aStoredError) {
    mStoredError = aStoredError;
  }

  void AppendWriteRequest(RefPtr<Promise>& aRequest) {
    mWriteRequests.AppendElement(aRequest);
  }

  // WritableStreamCloseQueuedOrInFlight
  bool CloseQueuedOrInFlight() const {
    return mCloseRequest || mInFlightCloseRequest;
  }

  // WritableStreamDealWithRejection
  void DealWithRejection(JSContext* aCx, JS::Handle<JS::Value> aError,
                         ErrorResult& aRv);

  // WritableStreamFinishErroring
  void FinishErroring(JSContext* aCx, ErrorResult& aRv);

  // WritableStreamFinishInFlightClose
  void FinishInFlightClose();

  // WritableStreamFinishInFlightCloseWithError
  void FinishInFlightCloseWithError(JSContext* aCx,
                                    JS::Handle<JS::Value> aError,
                                    ErrorResult& aRv);

  // WritableStreamFinishInFlightWrite
  void FinishInFlightWrite();

  // WritableStreamFinishInFlightWriteWithError
  void FinishInFlightWriteWithError(JSContext* aCX,
                                    JS::Handle<JS::Value> aError,
                                    ErrorResult& aR);

  // WritableStreamHasOperationMarkedInFlight
  bool HasOperationMarkedInFlight() const {
    return mInFlightWriteRequest || mInFlightCloseRequest;
  }

  // WritableStreamMarkCloseRequestInFlight
  void MarkCloseRequestInFlight();

  // WritableStreamMarkFirstWriteRequestInFlight
  void MarkFirstWriteRequestInFlight();

  // WritableStreamRejectCloseAndClosedPromiseIfNeeded
  void RejectCloseAndClosedPromiseIfNeeded();

  // WritableStreamStartErroring
  void StartErroring(JSContext* aCx, JS::Handle<JS::Value> aReason,
                     ErrorResult& aRv);

  // WritableStreamUpdateBackpressure
  void UpdateBackpressure(bool aBackpressure, ErrorResult& aRv);

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // IDL Methods
  static already_AddRefed<WritableStream> Constructor(
      const GlobalObject& aGlobal,
      const Optional<JS::Handle<JSObject*>>& aUnderlyingSink,
      const QueuingStrategy& aStrategy, ErrorResult& aRv);

  bool Locked() const { return !!mWriter; }

  already_AddRefed<Promise> Abort(JSContext* cx, JS::Handle<JS::Value> aReason,
                                  ErrorResult& aRv);

  already_AddRefed<Promise> Close(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<WritableStreamDefaultWriter> GetWriter(ErrorResult& aRv);

  // Internal Slots:
 private:
  bool mBackpressure = false;
  RefPtr<Promise> mCloseRequest;
  RefPtr<WritableStreamDefaultController> mController;
  RefPtr<Promise> mInFlightWriteRequest;
  RefPtr<Promise> mInFlightCloseRequest;

  // We inline all members of [[pendingAbortRequest]] in this class.
  // The absence (i.e. undefined) of the [[pendingAbortRequest]]
  // is indicated by mPendingAbortRequestPromise = nullptr.
  RefPtr<Promise> mPendingAbortRequestPromise;
  JS::Heap<JS::Value> mPendingAbortRequestReason;
  bool mPendingAbortRequestWasAlreadyErroring = false;

  WriterState mState = WriterState::Writable;
  JS::Heap<JS::Value> mStoredError;
  RefPtr<WritableStreamDefaultWriter> mWriter;
  nsTArray<RefPtr<Promise>> mWriteRequests;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

inline bool IsWritableStreamLocked(WritableStream* aStream) {
  return aStream->Locked();
}

extern already_AddRefed<Promise> WritableStreamAbort(
    JSContext* aCx, WritableStream* aStream, JS::Handle<JS::Value> aReason,
    ErrorResult& aRv);

extern already_AddRefed<Promise> WritableStreamClose(JSContext* aCx,
                                                     WritableStream* aStream,
                                                     ErrorResult& aRv);

extern already_AddRefed<Promise> WritableStreamAddWriteRequest(
    WritableStream* aStream, ErrorResult& aRv);

}  // namespace mozilla::dom

#endif  // mozilla_dom_WritableStream_h
