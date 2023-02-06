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

// https://streams.spec.whatwg.org/#readable-byte-stream-queue-entry
// Important: These list elements need to be traced by the owning structure.
struct ReadableByteStreamQueueEntry
    : LinkedListElement<RefPtr<ReadableByteStreamQueueEntry>> {
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ReadableByteStreamQueueEntry)

  friend class ReadableByteStreamController::cycleCollection;

  ReadableByteStreamQueueEntry(JS::Handle<JSObject*> aBuffer,
                               size_t aByteOffset, size_t aByteLength)
      : LinkedListElement<RefPtr<ReadableByteStreamQueueEntry>>(),
        mBuffer(aBuffer),
        mByteOffset(aByteOffset),
        mByteLength(aByteLength) {}

  JSObject* Buffer() const { return mBuffer; }
  void SetBuffer(JS::Handle<JSObject*> aBuffer) { mBuffer = aBuffer; }

  size_t ByteOffset() const { return mByteOffset; }
  void SetByteOffset(size_t aByteOffset) { mByteOffset = aByteOffset; }

  size_t ByteLength() const { return mByteLength; }
  void SetByteLength(size_t aByteLength) { mByteLength = aByteLength; }

  void ClearBuffer() { mBuffer = nullptr; }

 private:
  // An ArrayBuffer, which will be a transferred version of the one originally
  // supplied by the underlying byte source.
  //
  // This is traced by the list owner (see ReadableByteStreamController's
  // tracing code).
  JS::Heap<JSObject*> mBuffer;

  // A nonnegative integer number giving the byte offset derived from the view
  // originally supplied by the underlying byte source
  size_t mByteOffset = 0;

  // A nonnegative integer number giving the byte length derived from the view
  // originally supplied by the underlying byte source
  size_t mByteLength = 0;

  ~ReadableByteStreamQueueEntry() = default;
};

// Important: These list elments need to be traced by the owning structure.
struct PullIntoDescriptor final
    : LinkedListElement<RefPtr<PullIntoDescriptor>> {
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::PullIntoDescriptor)

  enum Constructor {
    DataView,
#define DEFINE_TYPED_CONSTRUCTOR_ENUM_NAMES(ExternalT, NativeT, Name) Name,
    JS_FOR_EACH_TYPED_ARRAY(DEFINE_TYPED_CONSTRUCTOR_ENUM_NAMES)
#undef DEFINE_TYPED_CONSTRUCTOR_ENUM_NAMES
  };

  static Constructor constructorFromScalar(JS::Scalar::Type type) {
    switch (type) {
#define REMAP_PULL_INTO_DESCRIPTOR_TYPE(ExternalT, NativeT, Name) \
  case JS::Scalar::Name:                                          \
    return Constructor::Name;
      JS_FOR_EACH_TYPED_ARRAY(REMAP_PULL_INTO_DESCRIPTOR_TYPE)
#undef REMAP

      case JS::Scalar::Int64:
      case JS::Scalar::Simd128:
      case JS::Scalar::MaxTypedArrayViewType:
        break;
    }
    MOZ_CRASH("Unexpected Scalar::Type");
  }

  friend class ReadableByteStreamController::cycleCollection;

  PullIntoDescriptor(JS::Handle<JSObject*> aBuffer, uint64_t aBufferByteLength,
                     uint64_t aByteOffset, uint64_t aByteLength,
                     uint64_t aBytesFilled, uint64_t aElementSize,
                     Constructor aViewConstructor, ReaderType aReaderType)
      : LinkedListElement<RefPtr<PullIntoDescriptor>>(),
        mBuffer(aBuffer),
        mBufferByteLength(aBufferByteLength),
        mByteOffset(aByteOffset),
        mByteLength(aByteLength),
        mBytesFilled(aBytesFilled),
        mElementSize(aElementSize),
        mViewConstructor(aViewConstructor),
        mReaderType(aReaderType) {}

  JSObject* Buffer() const { return mBuffer; }
  void SetBuffer(JS::Handle<JSObject*> aBuffer) { mBuffer = aBuffer; }

  uint64_t BufferByteLength() const { return mBufferByteLength; }
  void SetBufferByteLength(const uint64_t aBufferByteLength) {
    mBufferByteLength = aBufferByteLength;
  }

  uint64_t ByteOffset() const { return mByteOffset; }
  void SetByteOffset(const uint64_t aByteOffset) { mByteOffset = aByteOffset; }

  uint64_t ByteLength() const { return mByteLength; }
  void SetByteLength(const uint64_t aByteLength) { mByteLength = aByteLength; }

  uint64_t BytesFilled() const { return mBytesFilled; }
  void SetBytesFilled(const uint64_t aBytesFilled) {
    mBytesFilled = aBytesFilled;
  }

  uint64_t ElementSize() const { return mElementSize; }
  void SetElementSize(const uint64_t aElementSize) {
    mElementSize = aElementSize;
  }

  Constructor ViewConstructor() const { return mViewConstructor; }

  // Note: Named GetReaderType to avoid name conflict with type.
  ReaderType GetReaderType() const { return mReaderType; }
  void SetReaderType(const ReaderType aReaderType) {
    mReaderType = aReaderType;
  }

  void ClearBuffer() { mBuffer = nullptr; }

 private:
  // This is traced by the list owner (see ReadableByteStreamController's
  // tracing code).
  JS::Heap<JSObject*> mBuffer;
  uint64_t mBufferByteLength = 0;
  uint64_t mByteOffset = 0;
  uint64_t mByteLength = 0;
  uint64_t mBytesFilled = 0;
  uint64_t mElementSize = 0;
  Constructor mViewConstructor;
  ReaderType mReaderType;

  ~PullIntoDescriptor() = default;
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
