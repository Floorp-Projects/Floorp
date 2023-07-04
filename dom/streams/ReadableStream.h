/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStream_h
#define mozilla_dom_ReadableStream_h

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class Promise;
class ReadableStreamBYOBRequest;
class ReadableStreamDefaultReader;
class ReadableStreamGenericReader;
struct ReadableStreamGetReaderOptions;
struct ReadableStreamIteratorOptions;
struct ReadIntoRequest;
class WritableStream;
struct ReadableWritablePair;
struct StreamPipeOptions;

using ReadableStreamReader =
    ReadableStreamDefaultReaderOrReadableStreamBYOBReader;
using OwningReadableStreamReader =
    OwningReadableStreamDefaultReaderOrReadableStreamBYOBReader;
class NativeUnderlyingSource;
class BodyStreamHolder;
class UniqueMessagePortId;
class MessagePort;

class ReadableStream : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReadableStream)

  friend class WritableStream;

 protected:
  virtual ~ReadableStream();

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // If one extends ReadableStream with another cycle collectable class,
  // calling HoldJSObjects and DropJSObjects should happen using 'this' of
  // that extending class. And in that case Explicit should be passed to the
  // constructor of ReadableStream so that it doesn't make those calls.
  // See also https://bugzilla.mozilla.org/show_bug.cgi?id=1801214.
  enum class HoldDropJSObjectsCaller { Implicit, Explicit };

  explicit ReadableStream(const GlobalObject& aGlobal,
                          HoldDropJSObjectsCaller aHoldDropCaller);
  explicit ReadableStream(nsIGlobalObject* aGlobal,
                          HoldDropJSObjectsCaller aHoldDropCaller);

 public:
  // Abstract algorithms
  MOZ_CAN_RUN_SCRIPT static already_AddRefed<ReadableStream> CreateAbstract(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      UnderlyingSourceAlgorithmsBase* aAlgorithms,
      mozilla::Maybe<double> aHighWaterMark,
      QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT static already_AddRefed<ReadableStream> CreateByteAbstract(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      UnderlyingSourceAlgorithmsBase* aAlgorithms, ErrorResult& aRv);

  // Slot Getter/Setters:
  MOZ_KNOWN_LIVE ReadableStreamController* Controller() { return mController; }
  ReadableStreamDefaultController* DefaultController() {
    MOZ_ASSERT(mController && mController->IsDefault());
    return mController->AsDefault();
  }
  void SetController(ReadableStreamController& aController) {
    MOZ_ASSERT(!mController);
    mController = &aController;
  }

  bool Disturbed() const { return mDisturbed; }
  void SetDisturbed(bool aDisturbed) { mDisturbed = aDisturbed; }

  ReadableStreamGenericReader* GetReader() { return mReader; }
  void SetReader(ReadableStreamGenericReader* aReader);

  ReadableStreamDefaultReader* GetDefaultReader();

  enum class ReaderState { Readable, Closed, Errored };

  ReaderState State() const { return mState; }
  void SetState(const ReaderState& aState) { mState = aState; }

  JS::Value StoredError() const { return mStoredError; }
  void SetStoredError(JS::Handle<JS::Value> aStoredError) {
    mStoredError = aStoredError;
  }

  nsIInputStream* MaybeGetInputStreamIfUnread() {
    MOZ_ASSERT(!Disturbed());
    if (UnderlyingSourceAlgorithmsBase* algorithms =
            Controller()->GetAlgorithms()) {
      return algorithms->MaybeGetInputStreamIfUnread();
    }
    return nullptr;
  }

  // [Transferable]
  // https://html.spec.whatwg.org/multipage/structured-data.html#transfer-steps
  MOZ_CAN_RUN_SCRIPT bool Transfer(JSContext* aCx,
                                   UniqueMessagePortId& aPortId);
  MOZ_CAN_RUN_SCRIPT static already_AddRefed<ReadableStream>
  ReceiveTransferImpl(JSContext* aCx, nsIGlobalObject* aGlobal,
                      MessagePort& aPort);
  // https://html.spec.whatwg.org/multipage/structured-data.html#transfer-receiving-steps
  MOZ_CAN_RUN_SCRIPT static bool ReceiveTransfer(
      JSContext* aCx, nsIGlobalObject* aGlobal, MessagePort& aPort,
      JS::MutableHandle<JSObject*> aReturnObject);

  // Public functions to implement other specs
  // https://streams.spec.whatwg.org/#other-specs-rs

  // https://streams.spec.whatwg.org/#readablestream-set-up
  static already_AddRefed<ReadableStream> CreateNative(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      UnderlyingSourceAlgorithmsWrapper& aAlgorithms,
      mozilla::Maybe<double> aHighWaterMark,
      QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);

  // https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support

 protected:
  // Sets up the ReadableStream with byte reading support. Intended for
  // subclasses.
  void SetUpByteNative(JSContext* aCx,
                       UnderlyingSourceAlgorithmsWrapper& aAlgorithms,
                       mozilla::Maybe<double> aHighWaterMark, ErrorResult& aRv);

 public:
  // Creates and sets up a ReadableStream with byte reading support. Use
  // SetUpByteNative for this purpose in subclasses.
  static already_AddRefed<ReadableStream> CreateByteNative(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      UnderlyingSourceAlgorithmsWrapper& aAlgorithms,
      mozilla::Maybe<double> aHighWaterMark, ErrorResult& aRv);

  // The following algorithms must only be used on ReadableStream instances
  // initialized via the above set up or set up with byte reading support
  // algorithms (not, e.g., on web-developer-created instances):

  // https://streams.spec.whatwg.org/#readablestream-close
  MOZ_CAN_RUN_SCRIPT void CloseNative(JSContext* aCx, ErrorResult& aRv);

  // https://streams.spec.whatwg.org/#readablestream-error
  void ErrorNative(JSContext* aCx, JS::Handle<JS::Value> aError,
                   ErrorResult& aRv);

  // https://streams.spec.whatwg.org/#readablestream-enqueue
  MOZ_CAN_RUN_SCRIPT void EnqueueNative(JSContext* aCx,
                                        JS::Handle<JS::Value> aChunk,
                                        ErrorResult& aRv);

  // https://streams.spec.whatwg.org/#readablestream-current-byob-request-view
  void GetCurrentBYOBRequestView(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aView,
                                 ErrorResult& aRv);

  // The following algorithms can be used on arbitrary ReadableStream instances,
  // including ones that are created by web developers. They can all fail in
  // various operation-specific ways, and these failures should be handled by
  // the calling specification.

  // https://streams.spec.whatwg.org/#readablestream-get-a-reader
  already_AddRefed<mozilla::dom::ReadableStreamDefaultReader> GetReader(
      ErrorResult& aRv);

  // IDL layer functions

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // IDL methods

  // TODO: Use MOZ_CAN_RUN_SCRIPT when IDL constructors can use it (bug 1749042)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<ReadableStream>
  Constructor(const GlobalObject& aGlobal,
              const Optional<JS::Handle<JSObject*>>& aUnderlyingSource,
              const QueuingStrategy& aStrategy, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT static already_AddRefed<ReadableStream> From(
      const GlobalObject& aGlobal, JS::Handle<JS::Value> asyncIterable,
      ErrorResult& aRv);

  bool Locked() const;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Cancel(
      JSContext* cx, JS::Handle<JS::Value> aReason, ErrorResult& aRv);

  void GetReader(const ReadableStreamGetReaderOptions& aOptions,
                 OwningReadableStreamReader& resultReader, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<ReadableStream> PipeThrough(
      const ReadableWritablePair& aTransform, const StreamPipeOptions& aOptions,
      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> PipeTo(
      WritableStream& aDestination, const StreamPipeOptions& aOptions,
      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void Tee(JSContext* aCx,
                              nsTArray<RefPtr<ReadableStream>>& aResult,
                              ErrorResult& aRv);

  struct IteratorData {
    void Traverse(nsCycleCollectionTraversalCallback& cb);
    void Unlink();

    RefPtr<ReadableStreamDefaultReader> mReader;
    bool mPreventCancel;
  };

  using Iterator = AsyncIterableIterator<ReadableStream>;

  void InitAsyncIteratorData(IteratorData& aData, Iterator::IteratorType aType,
                             const ReadableStreamIteratorOptions& aOptions,
                             ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> GetNextIterationResult(
      Iterator* aIterator, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> IteratorReturn(
      JSContext* aCx, Iterator* aIterator, JS::Handle<JS::Value> aValue,
      ErrorResult& aRv);

  // Internal Slots:
 private:
  RefPtr<ReadableStreamController> mController;
  bool mDisturbed = false;
  RefPtr<ReadableStreamGenericReader> mReader;
  ReaderState mState = ReaderState::Readable;
  JS::Heap<JS::Value> mStoredError;

  HoldDropJSObjectsCaller mHoldDropCaller;
};

namespace streams_abstract {

bool IsReadableStreamLocked(ReadableStream* aStream);

double ReadableStreamGetNumReadRequests(ReadableStream* aStream);

void ReadableStreamError(JSContext* aCx, ReadableStream* aStream,
                         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableStreamClose(JSContext* aCx,
                                            ReadableStream* aStream,
                                            ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableStreamFulfillReadRequest(
    JSContext* aCx, ReadableStream* aStream, JS::Handle<JS::Value> aChunk,
    bool done, ErrorResult& aRv);

void ReadableStreamAddReadRequest(ReadableStream* aStream,
                                  ReadRequest* aReadRequest);
void ReadableStreamAddReadIntoRequest(ReadableStream* aStream,
                                      ReadIntoRequest* aReadIntoRequest);

MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> ReadableStreamCancel(
    JSContext* aCx, ReadableStream* aStream, JS::Handle<JS::Value> aError,
    ErrorResult& aRv);

already_AddRefed<ReadableStreamDefaultReader>
AcquireReadableStreamDefaultReader(ReadableStream* aStream, ErrorResult& aRv);

bool ReadableStreamHasBYOBReader(ReadableStream* aStream);
bool ReadableStreamHasDefaultReader(ReadableStream* aStream);

}  // namespace streams_abstract

}  // namespace mozilla::dom

#endif  // mozilla_dom_ReadableStream_h
