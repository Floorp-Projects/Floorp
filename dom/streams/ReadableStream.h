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
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#ifndef MOZ_DOM_STREAMS
#  error "Shouldn't be compiling with this header without MOZ_DOM_STREAMS set"
#endif

namespace mozilla {
namespace dom {

class Promise;
class ReadableStreamGenericReader;
class ReadableStreamDefaultReader;
class ReadableStreamGenericReader;
struct ReadableStreamGetReaderOptions;
struct ReadIntoRequest;

using ReadableStreamReader =
    ReadableStreamDefaultReaderOrReadableStreamBYOBReader;
using OwningReadableStreamReader =
    OwningReadableStreamDefaultReaderOrReadableStreamBYOBReader;
class NativeUnderlyingSource;
class BodyStreamHolder;

class ReadableStream final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReadableStream)

 protected:
  ~ReadableStream();

  nsCOMPtr<nsIGlobalObject> mGlobal;

 public:
  explicit ReadableStream(const GlobalObject& aGlobal);
  explicit ReadableStream(nsIGlobalObject* aGlobal);

  enum class ReaderState { Readable, Closed, Errored };

  // Slot Getter/Setters:
 public:
  ReadableStreamController* Controller() { return mController; }
  ReadableStreamDefaultController* DefaultController() {
    MOZ_ASSERT(mController && mController->IsDefault());
    return mController->AsDefault();
  }
  void SetController(ReadableStreamController* aController) {
    mController = aController;
  }

  bool Disturbed() const { return mDisturbed; }
  void SetDisturbed(bool aDisturbed) { mDisturbed = aDisturbed; }

  ReadableStreamGenericReader* GetReader() { return mReader; }
  void SetReader(ReadableStreamGenericReader* aReader);

  ReadableStreamDefaultReader* GetDefaultReader();

  ReaderState State() const { return mState; }
  void SetState(const ReaderState& aState) { mState = aState; }

  JS::Value StoredError() const { return mStoredError; }
  void SetStoredError(JS::HandleValue aStoredError) {
    mStoredError = aStoredError;
  }

  UnderlyingSourceErrorCallbackHelper* GetErrorAlgorithm() const {
    return mErrorAlgorithm;
  }
  void SetErrorAlgorithm(UnderlyingSourceErrorCallbackHelper* aErrorAlgorithm) {
    mErrorAlgorithm = aErrorAlgorithm;
  }

  void SetNativeUnderlyingSource(BodyStreamHolder* aUnderlyingSource);
  BodyStreamHolder* GetNativeUnderlyingSource() {
    return mNativeUnderlyingSource;
  }
  bool HasNativeUnderlyingSource() { return mNativeUnderlyingSource; }

  void ReleaseObjects();

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  MOZ_CAN_RUN_SCRIPT static already_AddRefed<ReadableStream> Create(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      BodyStreamHolder* aUnderlyingSource, ErrorResult& aRv);

  // IDL Methods
  // TODO: Use MOZ_CAN_RUN_SCRIPT when IDL constructors can use it (bug 1749042)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<ReadableStream>
  Constructor(const GlobalObject& aGlobal,
              const Optional<JS::Handle<JSObject*>>& aUnderlyingSource,
              const QueuingStrategy& aStrategy, ErrorResult& aRv);

  bool Locked() const;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Cancel(
      JSContext* cx, JS::Handle<JS::Value> aReason, ErrorResult& aRv);

  void GetReader(JSContext* aCx, const ReadableStreamGetReaderOptions& aOptions,
                 OwningReadableStreamReader& resultReader, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void Tee(JSContext* aCx,
                              nsTArray<RefPtr<ReadableStream>>& aResult,
                              ErrorResult& aRv);

  // Internal Slots:
 private:
  RefPtr<ReadableStreamController> mController;
  bool mDisturbed = false;
  RefPtr<ReadableStreamGenericReader> mReader;
  ReaderState mState = ReaderState::Readable;
  JS::Heap<JS::Value> mStoredError;

  // Optional Callback for erroring a stream.
  RefPtr<UnderlyingSourceErrorCallbackHelper> mErrorAlgorithm;

  // Optional strong reference to an Underlying Source; This
  // exists because NativeUnderlyingSource callbacks don't hold
  // a strong reference to the underlying source: So we need
  // something else to hold onto that. As well, some of the integration
  // desires the ability to extract the underlying source from the
  // ReadableStream.
  //
  // While theoretically this ought to be some base class type to support
  // multiple native underlying source types, I'm not sure what base class
  // makes any sense for BodyStream, and given there's only body streams
  // as the underlying source right now, I'm going to punt that problem to
  // the future where we need to provide other native underlying sources
  // (i.e. perhaps WebTransport.)
  RefPtr<BodyStreamHolder> mNativeUnderlyingSource;
};

extern bool IsReadableStreamLocked(ReadableStream* aStream);

extern double ReadableStreamGetNumReadRequests(ReadableStream* aStream);

extern void ReadableStreamError(JSContext* aCx, ReadableStream* aStream,
                                JS::Handle<JS::Value> aValue, ErrorResult& aRv);

extern void ReadableStreamClose(JSContext* aCx, ReadableStream* aStream,
                                ErrorResult& aRv);

extern void ReadableStreamFulfillReadRequest(JSContext* aCx,
                                             ReadableStream* aStream,
                                             JS::Handle<JS::Value> aChunk,
                                             bool done, ErrorResult& aRv);

extern void ReadableStreamAddReadRequest(ReadableStream* aStream,
                                         ReadRequest* aReadRequest);
extern void ReadableStreamAddReadIntoRequest(ReadableStream* aStream,
                                             ReadIntoRequest* aReadIntoRequest);

MOZ_CAN_RUN_SCRIPT extern already_AddRefed<Promise> ReadableStreamCancel(
    JSContext* aCx, ReadableStream* aStream, JS::Handle<JS::Value> aError,
    ErrorResult& aRv);

extern already_AddRefed<ReadableStreamDefaultReader>
AcquireReadableStreamDefaultReader(JSContext* aCx, ReadableStream* aStream,
                                   ErrorResult& aRv);

extern bool ReadableStreamHasBYOBReader(ReadableStream* aStream);
extern bool ReadableStreamHasDefaultReader(ReadableStream* aStream);

MOZ_CAN_RUN_SCRIPT extern already_AddRefed<ReadableStream>
CreateReadableByteStream(JSContext* aCx, nsIGlobalObject* aGlobal,
                         UnderlyingSourceStartCallbackHelper* aStartAlgorithm,
                         UnderlyingSourcePullCallbackHelper* aPullAlgorithm,
                         UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm,
                         ErrorResult& aRv);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReadableStream_h
