/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableByteStreamController_h
#define mozilla_dom_ReadableByteStreamController_h

#include <cstddef>
#include "UnderlyingSourceCallbackHelpers.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/QueueWithSizes.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/ReadableStreamBYOBRequest.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/TypedArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"
#include "nsTArray.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

// https://streams.spec.whatwg.org/#pull-into-descriptor-reader-type
// Indicates what type of readable stream reader initiated this request,
// or None if the initiating reader was released.
enum ReaderType { Default, BYOB, None };

struct PullIntoDescriptor;
struct ReadableByteStreamQueueEntry;
struct ReadIntoRequest;

class ReadableByteStreamController final : public ReadableStreamController,
                                           public nsWrapperCache {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      ReadableByteStreamController, ReadableStreamController)

 public:
  explicit ReadableByteStreamController(nsIGlobalObject* aGlobal);

 protected:
  ~ReadableByteStreamController() override;

 public:
  bool IsDefault() override { return false; }
  bool IsByte() override { return true; }
  ReadableStreamDefaultController* AsDefault() override { return nullptr; }
  ReadableByteStreamController* AsByte() override { return this; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<ReadableStreamBYOBRequest> GetByobRequest(JSContext* aCx,
                                                             ErrorResult& aRv);

  Nullable<double> GetDesiredSize() const;

  MOZ_CAN_RUN_SCRIPT void Close(JSContext* aCx, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void Enqueue(JSContext* aCx, const ArrayBufferView& aChunk,
                                  ErrorResult& aRv);

  void Error(JSContext* aCx, JS::Handle<JS::Value> aErrorValue,
             ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CancelSteps(
      JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) override;
  MOZ_CAN_RUN_SCRIPT void PullSteps(JSContext* aCx, ReadRequest* aReadRequest,
                                    ErrorResult& aRv) override;
  void ReleaseSteps() override;

  // Internal Slot Accessors
  Maybe<uint64_t> AutoAllocateChunkSize() { return mAutoAllocateChunkSize; }
  void SetAutoAllocateChunkSize(Maybe<uint64_t>& aSize) {
    mAutoAllocateChunkSize = aSize;
  }

  ReadableStreamBYOBRequest* GetByobRequest() const { return mByobRequest; }
  void SetByobRequest(ReadableStreamBYOBRequest* aByobRequest) {
    mByobRequest = aByobRequest;
  }

  LinkedList<RefPtr<PullIntoDescriptor>>& PendingPullIntos() {
    return mPendingPullIntos;
  }
  void ClearPendingPullIntos();

  double QueueTotalSize() const { return mQueueTotalSize; }
  void SetQueueTotalSize(double aQueueTotalSize) {
    mQueueTotalSize = aQueueTotalSize;
  }
  void AddToQueueTotalSize(double aLength) { mQueueTotalSize += aLength; }

  double StrategyHWM() const { return mStrategyHWM; }
  void SetStrategyHWM(double aStrategyHWM) { mStrategyHWM = aStrategyHWM; }

  bool CloseRequested() const { return mCloseRequested; }
  void SetCloseRequested(bool aCloseRequested) {
    mCloseRequested = aCloseRequested;
  }

  LinkedList<RefPtr<ReadableByteStreamQueueEntry>>& Queue() { return mQueue; }
  void ClearQueue();

  bool Started() const { return mStarted; }
  void SetStarted(bool aStarted) { mStarted = aStarted; }

  bool Pulling() const { return mPulling; }
  void SetPulling(bool aPulling) { mPulling = aPulling; }

  bool PullAgain() const { return mPullAgain; }
  void SetPullAgain(bool aPullAgain) { mPullAgain = aPullAgain; }

 private:
  // A boolean flag indicating whether the stream has been closed by its
  // underlying byte source, but still has chunks in its internal queue that
  // have not yet been read
  bool mCloseRequested = false;

  // A boolean flag set to true if the stream’s mechanisms requested a call
  // to the underlying byte source's pull algorithm to pull more data, but the
  // pull could not yet be done since a previous call is still executing
  bool mPullAgain = false;

  // A boolean flag indicating whether the underlying byte source has finished
  // starting
  bool mStarted = false;

  // A boolean flag set to true while the underlying byte source's pull
  // algorithm is executing and the returned promise has not yet fulfilled,
  // used to prevent reentrant calls
  bool mPulling = false;

  // A positive integer, when the automatic buffer allocation feature is
  // enabled. In that case, this value specifies the size of buffer to allocate.
  // It is undefined otherwise.
  Maybe<uint64_t> mAutoAllocateChunkSize;

  // A ReadableStreamBYOBRequest instance representing the current BYOB pull
  // request, or null if there are no pending requests
  RefPtr<ReadableStreamBYOBRequest> mByobRequest;

  // A list of pull-into descriptors
  LinkedList<RefPtr<PullIntoDescriptor>> mPendingPullIntos;

  // A list of readable byte stream queue entries representing the stream’s
  // internal queue of chunks
  //
  // This is theoretically supposed to be a QueueWithSizes, but it is
  // mostly not actually manipulated or used like QueueWithSizes, so instead we
  // use a LinkedList.
  LinkedList<RefPtr<ReadableByteStreamQueueEntry>> mQueue;

  // The total size, in bytes, of all the chunks stored in [[queue]] (see § 8.1
  // Queue-with-sizes)
  double mQueueTotalSize = 0.0;

  // A number supplied to the constructor as part of the stream’s queuing
  // strategy, indicating the point at which the stream will apply backpressure
  // to its underlying byte source
  double mStrategyHWM = 0.0;
};

namespace streams_abstract {

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerRespond(
    JSContext* aCx, ReadableByteStreamController* aController,
    uint64_t aBytesWritten, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerRespondInternal(
    JSContext* aCx, ReadableByteStreamController* aController,
    uint64_t aBytesWritten, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerRespondWithNewView(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aView, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerPullInto(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aView, ReadIntoRequest* aReadIntoRequest,
    ErrorResult& aRv);

void ReadableByteStreamControllerError(
    ReadableByteStreamController* aController, JS::Handle<JS::Value> aValue,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerEnqueue(
    JSContext* aCx, ReadableByteStreamController* aController,
    JS::Handle<JSObject*> aChunk, ErrorResult& aRv);

already_AddRefed<ReadableStreamBYOBRequest>
ReadableByteStreamControllerGetBYOBRequest(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerClose(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void SetUpReadableByteStreamController(
    JSContext* aCx, ReadableStream* aStream,
    ReadableByteStreamController* aController,
    UnderlyingSourceAlgorithmsBase* aAlgorithms, double aHighWaterMark,
    Maybe<uint64_t> aAutoAllocateChunkSize, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamControllerCallPullIfNeeded(
    JSContext* aCx, ReadableByteStreamController* aController,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void SetUpReadableByteStreamControllerFromUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream,
    JS::Handle<JSObject*> aUnderlyingSource,
    UnderlyingSource& aUnderlyingSourceDict, double aHighWaterMark,
    ErrorResult& aRv);

}  // namespace streams_abstract

}  // namespace mozilla::dom

#endif
