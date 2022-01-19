/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReadableStream.h"
#include "js/Exception.h"
#include "js/TypeDecls.h"
#include "js/experimental/TypedData.h"
#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ReadIntoRequest.h"
#include "mozilla/dom/ReadableStreamBYOBReader.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamGenericReader.h"
#include "mozilla/dom/ReadableStreamTee.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/CycleCollectedJSContext.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableStreamDefaultTeePullAlgorithm)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    ReadableStreamDefaultTeePullAlgorithm, UnderlyingSourcePullCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTeeState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    ReadableStreamDefaultTeePullAlgorithm, UnderlyingSourcePullCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTeeState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(ReadableStreamDefaultTeePullAlgorithm,
                         UnderlyingSourcePullCallbackHelper)
NS_IMPL_RELEASE_INHERITED(ReadableStreamDefaultTeePullAlgorithm,
                          UnderlyingSourcePullCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamDefaultTeePullAlgorithm)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourcePullCallbackHelper)

already_AddRefed<Promise> ReadableStreamDefaultTeePullAlgorithm::PullCallback(
    JSContext* aCx, nsIGlobalObject* aGlobal, ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
  // Pull Algorithm Steps:

  // Step 13.1:
  if (mTeeState->Reading()) {
    // Step 13.1.1
    mTeeState->SetReadAgain(true);

    // Step 13.1.2
    return Promise::CreateResolvedWithUndefined(aGlobal, aRv);
  }

  // Step 13.2:
  mTeeState->SetReading(true);

  // Step 13.3:
  RefPtr<ReadRequest> readRequest =
      new ReadableStreamDefaultTeeReadRequest(mTeeState);

  // Step 13.4:
  RefPtr<ReadableStreamGenericReader> reader(mTeeState->GetReader());
  ReadableStreamDefaultReaderRead(aCx, reader, readRequest, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 13.5
  return Promise::CreateResolvedWithUndefined(aGlobal, aRv);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableStreamDefaultTeeReadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    ReadableStreamDefaultTeeReadRequest, ReadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTeeState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    ReadableStreamDefaultTeeReadRequest, ReadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTeeState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(ReadableStreamDefaultTeeReadRequest, ReadRequest)
NS_IMPL_RELEASE_INHERITED(ReadableStreamDefaultTeeReadRequest, ReadRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamDefaultTeeReadRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadRequest)

void ReadableStreamDefaultTeeReadRequest::ChunkSteps(
    JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1.
  class ReadableStreamDefaultTeeReadRequestChunkSteps
      : public MicroTaskRunnable {
    RefPtr<TeeState> mTeeState;
    JS::PersistentRooted<JS::Value> mChunk;

   public:
    ReadableStreamDefaultTeeReadRequestChunkSteps(JSContext* aCx,
                                                  TeeState* aTeeState,
                                                  JS::Handle<JS::Value> aChunk)
        : mTeeState(aTeeState), mChunk(aCx, aChunk) {}

    MOZ_CAN_RUN_SCRIPT
    void Run(AutoSlowOperation& aAso) override {
      AutoJSAPI jsapi;
      if (NS_WARN_IF(!jsapi.Init(mTeeState->GetStream()->GetParentObject()))) {
        return;
      }
      JSContext* cx = jsapi.cx();
      // Step Numbering below is relative to Chunk steps Microtask:
      //
      // Step 1.
      mTeeState->SetReadAgain(false);

      // Step 2.
      JS::RootedValue chunk1(cx, mChunk);
      JS::RootedValue chunk2(cx, mChunk);

      // Step 3. Skipped until we implement cloneForBranch2 path.
      MOZ_RELEASE_ASSERT(!mTeeState->CloneForBranch2());

      // Step 4.
      if (!mTeeState->Canceled1()) {
        IgnoredErrorResult rv;
        // Since we controlled the creation of the two stream branches, we know
        // they both have default controllers.
        RefPtr<ReadableStreamDefaultController> controller(
            mTeeState->Branch1()->DefaultController());
        ReadableStreamDefaultControllerEnqueue(cx, controller, chunk1, rv);
        (void)NS_WARN_IF(rv.Failed());
      }

      // Step 5.
      if (!mTeeState->Canceled2()) {
        IgnoredErrorResult rv;
        RefPtr<ReadableStreamDefaultController> controller(
            mTeeState->Branch2()->DefaultController());
        ReadableStreamDefaultControllerEnqueue(cx, controller, chunk2, rv);
        (void)NS_WARN_IF(rv.Failed());
      }

      // Step 6.
      mTeeState->SetReading(false);

      // Step 7. If |readAgain| is true, perform |pullAlgorithm|.
      if (mTeeState->ReadAgain()) {
        RefPtr<ReadableStreamDefaultTeePullAlgorithm> pullAlgorithm(
            mTeeState->PullAlgorithm());
        IgnoredErrorResult rv;
        nsCOMPtr<nsIGlobalObject> global(
            mTeeState->GetStream()->GetParentObject());
        // MG:XXX: A future refactoring could rewrite pull callbacks innards to
        // not produce this promise, which is currently always a promise
        // resolved with undefined.
        RefPtr<Promise> ignoredPromise =
            pullAlgorithm->PullCallback(cx, global, rv);
        (void)NS_WARN_IF(rv.Failed());
      }
    }

    bool Suppressed() override {
      nsIGlobalObject* global = mTeeState->GetStream()->GetParentObject();
      return global && global->IsInSyncOperation();
    }
  };

  RefPtr<ReadableStreamDefaultTeeReadRequestChunkSteps> task =
      MakeRefPtr<ReadableStreamDefaultTeeReadRequestChunkSteps>(aCx, mTeeState,
                                                                aChunk);
  CycleCollectedJSContext::Get()->DispatchToMicroTask(task.forget());
}

void ReadableStreamDefaultTeeReadRequest::CloseSteps(JSContext* aCx,
                                                     ErrorResult& aRv) {
  // Step Numbering below is relative to 'close steps' of
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
  //
  // Step 1.
  mTeeState->SetReading(false);

  // Step 2.
  if (!mTeeState->Canceled1()) {
    ReadableStreamDefaultControllerClose(
        aCx, mTeeState->Branch1()->DefaultController(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 3.
  if (!mTeeState->Canceled2()) {
    ReadableStreamDefaultControllerClose(
        aCx, mTeeState->Branch2()->DefaultController(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 4.
  if (!mTeeState->Canceled1() || !mTeeState->Canceled2()) {
    mTeeState->CancelPromise()->MaybeResolveWithUndefined();
  }
}

void ReadableStreamDefaultTeeReadRequest::ErrorSteps(
    JSContext* aCx, JS::Handle<JS::Value> aError, ErrorResult& aRv) {
  mTeeState->SetReading(false);
}

MOZ_CAN_RUN_SCRIPT void PullWithDefaultReader(JSContext* aCx,
                                              TeeState* aTeeState,
                                              ErrorResult& aRv);
MOZ_CAN_RUN_SCRIPT void PullWithBYOBReader(JSContext* aCx, TeeState* aTeeState,
                                           JS::HandleObject aView,
                                           bool aForBranch2, ErrorResult& aRv);

// Algorithm described in
// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee, Steps
// 17 and Steps 18, genericized across branch numbers:
//
// Note: As specified this algorithm always returns a promise resolved with
// undefined, however as some places immediately discard said promise, we
// provide this version which doesn't return a promise.
//
// NativeByteStreamTeePullAlgorithm, which implements
// UnderlyingSourcePullCallbackHelper is the version which provies the return
// promise.
MOZ_CAN_RUN_SCRIPT void ByteStreamTeePullAlgorithm(JSContext* aCx, size_t index,
                                                   TeeState* aTeeState,
                                                   ErrorResult& aRv) {
  MOZ_ASSERT(index == 1 || index == 2);

  // Step {17,18}.1: If reading is true,
  if (aTeeState->Reading()) {
    // Step {17,18}.1.1: Set readAgainForBranch{1,2} to true.
    aTeeState->SetReadAgainForBranch(index, true);

    // Step {17,18}.1.1: Return a promise resolved with undefined.
    return;
  }

  // Step {17,18}.2: Set reading to true.
  aTeeState->SetReading(true);

  // Step {17,18}.3: Let byobRequest be
  // !ReadableByteStreamControllerGetBYOBRequest(branch{1,2}.[[controller]]).
  RefPtr<ReadableStreamBYOBRequest> byobRequest =
      ReadableByteStreamControllerGetBYOBRequest(
          aCx, aTeeState->Branch(index)->Controller()->AsByte(), aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step {17,18}.4: If byobRequest is null, perform pullWithDefaultReader.
  if (!byobRequest) {
    PullWithDefaultReader(aCx, aTeeState, aRv);
  } else {
    // Step {17,18}.5: Otherwise, perform pullWithBYOBReader, given
    // byobRequest.[[view]] and false.
    JS::RootedObject view(aCx, byobRequest->View());
    PullWithBYOBReader(aCx, aTeeState, view, false, aRv);
  }

  // Step {17,18}.6: Return a promise resolved with undefined.
  return;
}

class NativeByteStreamTeePullAlgorithm final
    : public UnderlyingSourcePullCallbackHelper {
  // Virtually const, but is cycle collected
  RefPtr<TeeState> mTeeState;
  size_t mBranchIndex;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NativeByteStreamTeePullAlgorithm,
                                           UnderlyingSourcePullCallbackHelper)

  explicit NativeByteStreamTeePullAlgorithm(TeeState* aTeeState,
                                            size_t aBranchIndex)
      : mTeeState(aTeeState), mBranchIndex(aBranchIndex) {
    MOZ_ASSERT(aBranchIndex == 1 || aBranchIndex == 2);
  }

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override {
    RefPtr<Promise> returnPromise = Promise::CreateResolvedWithUndefined(
        mTeeState->GetStream()->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    ByteStreamTeePullAlgorithm(aCx, mBranchIndex, MOZ_KnownLive(mTeeState),
                               aRv);

    return returnPromise.forget();
  }

 protected:
  ~NativeByteStreamTeePullAlgorithm() = default;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(NativeByteStreamTeePullAlgorithm)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    NativeByteStreamTeePullAlgorithm, UnderlyingSourcePullCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTeeState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    NativeByteStreamTeePullAlgorithm, UnderlyingSourcePullCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTeeState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(NativeByteStreamTeePullAlgorithm,
                         UnderlyingSourcePullCallbackHelper)
NS_IMPL_RELEASE_INHERITED(NativeByteStreamTeePullAlgorithm,
                          UnderlyingSourcePullCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NativeByteStreamTeePullAlgorithm)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourcePullCallbackHelper)

struct PullWithDefaultReaderReadRequest final : public ReadRequest {
  RefPtr<TeeState> mTeeState;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PullWithDefaultReaderReadRequest,
                                           ReadRequest)

  explicit PullWithDefaultReaderReadRequest(TeeState* aTeeState)
      : mTeeState(aTeeState) {}

  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
    // Step 15.2.1
    class PullWithDefaultReaderChunkStepMicrotask : public MicroTaskRunnable {
      RefPtr<TeeState> mTeeState;
      JS::PersistentRooted<JSObject*> mChunk;

     public:
      PullWithDefaultReaderChunkStepMicrotask(JSContext* aCx,
                                              TeeState* aTeeState,
                                              JS::Handle<JSObject*> aChunk)
          : mTeeState(aTeeState), mChunk(aCx, aChunk) {}

      MOZ_CAN_RUN_SCRIPT
      void Run(AutoSlowOperation& aAso) override {
        // Step Numbering in this function is relative to the Queue a microtask
        // of the Chunk steps of 15.2.1 of
        // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
        AutoJSAPI jsapi;
        if (NS_WARN_IF(
                !jsapi.Init(mTeeState->GetStream()->GetParentObject()))) {
          return;
        }
        JSContext* cx = jsapi.cx();

        // Step 1. Set readAgainForBranch1 to false.
        mTeeState->SetReadAgainForBranch1(false);

        // Step 2. Set readAgainForBranch2 to false.
        mTeeState->SetReadAgainForBranch2(false);

        // Step 3. Let chunk1 and chunk2 be chunk.
        JS::Rooted<JSObject*> chunk1(cx, mChunk);
        JS::Rooted<JSObject*> chunk2(cx, mChunk);

        // Step 4. If canceled1 is false and canceled2 is false,
        ErrorResult rv;
        if (!mTeeState->Canceled1() && !mTeeState->Canceled2()) {
          // Step 4.1. Let cloneResult be CloneAsUint8Array(chunk).
          JS::Rooted<JSObject*> cloneResult(cx, CloneAsUint8Array(cx, mChunk));

          // Step 4.2. If cloneResult is an abrupt completion,
          if (!cloneResult) {
            // Step 4.2.1 Perform
            // !ReadableByteStreamControllerError(branch1.[[controller]],
            // cloneResult.[[Value]]).
            JS::Rooted<JS::Value> exceptionValue(cx);
            if (!JS_GetPendingException(cx, &exceptionValue)) {
              // Uncatchable exception, simply return.
              return;
            }
            JS_ClearPendingException(cx);

            ErrorResult rv;
            ReadableByteStreamControllerError(
                mTeeState->Branch1()->Controller()->AsByte(), exceptionValue,
                rv);
            if (rv.MaybeSetPendingException(
                    cx, "Error during ReadableByteStreamControllerError")) {
              return;
            }

            // Step 4.2.2. Perform !
            // ReadableByteStreamControllerError(branch2.[[controller]],
            // cloneResult.[[Value]]).
            ReadableByteStreamControllerError(
                mTeeState->Branch2()->Controller()->AsByte(), exceptionValue,
                rv);
            if (rv.MaybeSetPendingException(
                    cx, "Error during ReadableByteStreamControllerError")) {
              return;
            }

            // Step 4.2.3. Resolve cancelPromise with !
            // ReadableStreamCancel(stream, cloneResult.[[Value]]).
            RefPtr<ReadableStream> stream(mTeeState->GetStream());
            RefPtr<Promise> promise =
                ReadableStreamCancel(cx, stream, exceptionValue, rv);
            if (rv.MaybeSetPendingException(
                    cx, "Error during ReadableByteStreamControllerError")) {
              return;
            }
            mTeeState->CancelPromise()->MaybeResolve(promise);

            // Step 4.2.4. Return.
            return;
          }

          // Step 4.3. Otherwise, set chunk2 to cloneResult.[[Value]].
          chunk2 = cloneResult;
        }

        // Step 5. If canceled1 is false,
        // perform ! ReadableByteStreamControllerEnqueue(branch1.[[controller]],
        // chunk1).
        if (!mTeeState->Canceled1()) {
          ErrorResult rv;
          RefPtr<ReadableByteStreamController> controller(
              mTeeState->Branch1()->Controller()->AsByte());
          ReadableByteStreamControllerEnqueue(cx, controller, chunk1, rv);
          if (rv.MaybeSetPendingException(
                  cx, "Error during ReadableByteStreamControllerEnqueue")) {
            return;
          }
        }

        // Step 6. If canceled2 is false,
        // perform ! ReadableByteStreamControllerEnqueue(branch2.[[controller]],
        // chunk2).
        if (!mTeeState->Canceled2()) {
          ErrorResult rv;
          RefPtr<ReadableByteStreamController> controller(
              mTeeState->Branch2()->Controller()->AsByte());
          ReadableByteStreamControllerEnqueue(cx, controller, chunk2, rv);
          if (rv.MaybeSetPendingException(
                  cx, "Error during ReadableByteStreamControllerEnqueue")) {
            return;
          }
        }

        // Step 7. Set reading to false.
        mTeeState->SetReading(false);

        // Step 8. If readAgainForBranch1 is true, perform pull1Algorithm.
        if (mTeeState->ReadAgainForBranch1()) {
          ByteStreamTeePullAlgorithm(cx, 1, MOZ_KnownLive(mTeeState), rv);
        } else if (mTeeState->ReadAgainForBranch2()) {
          // Step 9. Otherwise, if readAgainForBranch2 is true, perform
          // pull2Algorithm.
          ByteStreamTeePullAlgorithm(cx, 2, MOZ_KnownLive(mTeeState), rv);
        }
      }

      bool Suppressed() override {
        nsIGlobalObject* global = mTeeState->GetStream()->GetParentObject();
        return global && global->IsInSyncOperation();
      }
    };

    MOZ_ASSERT(aChunk.isObjectOrNull());
    MOZ_ASSERT(aChunk.toObjectOrNull() != nullptr);
    JS::RootedObject chunk(aCx, &aChunk.toObject());
    RefPtr<PullWithDefaultReaderChunkStepMicrotask> task =
        MakeRefPtr<PullWithDefaultReaderChunkStepMicrotask>(aCx, mTeeState,
                                                            chunk);
    CycleCollectedJSContext::Get()->DispatchToMicroTask(task.forget());
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void CloseSteps(JSContext* aCx,
                                              ErrorResult& aRv) override {
    // Step numbering below is relative to Step 15.2. 'close steps' of
    // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee

    // Step 1. Set reading to false.
    mTeeState->SetReading(false);

    // Step 2. If canceled1 is false, perform !
    // ReadableByteStreamControllerClose(branch1.[[controller]]).
    RefPtr<ReadableByteStreamController> branch1Controller =
        mTeeState->Branch1()->Controller()->AsByte();
    if (!mTeeState->Canceled1()) {
      ReadableByteStreamControllerClose(aCx, branch1Controller, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 3. If canceled2 is false, perform !
    // ReadableByteStreamControllerClose(branch2.[[controller]]).
    RefPtr<ReadableByteStreamController> branch2Controller =
        mTeeState->Branch2()->Controller()->AsByte();
    if (!mTeeState->Canceled2()) {
      ReadableByteStreamControllerClose(aCx, branch2Controller, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 4. If branch1.[[controller]].[[pendingPullIntos]] is not empty,
    // perform ! ReadableByteStreamControllerRespond(branch1.[[controller]], 0).
    if (!branch1Controller->PendingPullIntos().isEmpty()) {
      ReadableByteStreamControllerRespond(aCx, branch1Controller, 0, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 5. If branch2.[[controller]].[[pendingPullIntos]] is not empty,
    // perform ! ReadableByteStreamControllerRespond(branch2.[[controller]], 0).
    if (!branch2Controller->PendingPullIntos().isEmpty()) {
      ReadableByteStreamControllerRespond(aCx, branch2Controller, 0, aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 6. If canceled1 is false or canceled2 is false, resolve
    // cancelPromise with undefined.
    if (!mTeeState->Canceled1() || !mTeeState->Canceled2()) {
      mTeeState->CancelPromise()->MaybeResolveWithUndefined();
    }
  }

  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> aError,
                  ErrorResult& aRv) override {
    mTeeState->SetReading(false);
  }

 protected:
  virtual ~PullWithDefaultReaderReadRequest() = default;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(PullWithDefaultReaderReadRequest,
                                   ReadRequest, mTeeState)
NS_IMPL_ADDREF_INHERITED(PullWithDefaultReaderReadRequest, ReadRequest)
NS_IMPL_RELEASE_INHERITED(PullWithDefaultReaderReadRequest, ReadRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PullWithDefaultReaderReadRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadRequest)

void ForwardReaderError(TeeState* aTeeState,
                        ReadableStreamGenericReader* aThisReader);

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee:
// Step 15.
void PullWithDefaultReader(JSContext* aCx, TeeState* aTeeState,
                           ErrorResult& aRv) {
  RefPtr<ReadableStreamGenericReader> reader = aTeeState->GetReader();

  // Step 15.1. If reader implements ReadableStreamBYOBReader,
  if (reader->IsBYOB()) {
    // Step 15.1.1. Assert: reader.[[readIntoRequests]] is empty.
    MOZ_ASSERT(reader->AsBYOB()->ReadIntoRequests().length() == 0);

    // Step 15.1.2. Perform ! ReadableStreamBYOBReaderRelease(reader).
    // TODO: Fix this when we have
    // ReadableStreamBYOBReaderErrorReadIntoRequests.
    ReadableStreamReaderGenericRelease(reader, aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 15.1.3. Set reader to ! AcquireReadableStreamDefaultReader(stream).
    reader =
        AcquireReadableStreamDefaultReader(aCx, aTeeState->GetStream(), aRv);
    if (aRv.Failed()) {
      return;
    }
    aTeeState->SetReader(reader);

    // Step 16.1.4. Perform forwardReaderError, given reader.
    ForwardReaderError(aTeeState, reader);
  }

  // Step 15.2
  RefPtr<ReadRequest> readRequest =
      new PullWithDefaultReaderReadRequest(aTeeState);

  // Step 15.3
  ReadableStreamDefaultReaderRead(aCx, reader, readRequest, aRv);
}

class PullWithBYOBReader_ReadIntoRequest final : public ReadIntoRequest {
  RefPtr<TeeState> mTeeState;
  bool mForBranch2;
  virtual ~PullWithBYOBReader_ReadIntoRequest() = default;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PullWithBYOBReader_ReadIntoRequest,
                                           ReadIntoRequest)

  explicit PullWithBYOBReader_ReadIntoRequest(TeeState* aTeeState,
                                              bool aForBranch2)
      : mTeeState(aTeeState), mForBranch2(aForBranch2) {}

  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
    // Step 16.4 chunk steps, Step 1.
    class PullWithBYOBReaderChunkMicrotask : public MicroTaskRunnable {
      RefPtr<TeeState> mTeeState;
      JS::PersistentRooted<JSObject*> mChunk;
      bool mForBranch2 = false;

     public:
      PullWithBYOBReaderChunkMicrotask(JSContext* aCx, TeeState* aTeeState,
                                       JS::Handle<JSObject*> aChunk,
                                       bool aForBranch2)
          : mTeeState(aTeeState),
            mChunk(aCx, aChunk),
            mForBranch2(aForBranch2) {}

      MOZ_CAN_RUN_SCRIPT
      void Run(AutoSlowOperation& aAso) override {
        AutoJSAPI jsapi;
        if (NS_WARN_IF(
                !jsapi.Init(mTeeState->GetStream()->GetParentObject()))) {
          return;
        }
        JSContext* cx = jsapi.cx();
        ErrorResult rv;
        // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
        //
        // Step Numbering below is relative to Chunk steps Microtask at
        // Step 16.4 chunk steps, Step 1.

        // Step 1.
        mTeeState->SetReadAgainForBranch1(false);

        // Step 2.
        mTeeState->SetReadAgainForBranch2(false);

        // Step 3.
        bool byobCanceled =
            mForBranch2 ? mTeeState->Canceled2() : mTeeState->Canceled1();

        // Step 4.
        bool otherCanceled =
            !mForBranch2 ? mTeeState->Canceled2() : mTeeState->Canceled1();

        // Rather than store byobBranch / otherBranch, we re-derive the pointers
        // below, as borrowed from steps 16.2/16.3
        ReadableStream* byobBranch =
            mForBranch2 ? mTeeState->Branch2() : mTeeState->Branch1();
        ReadableStream* otherBranch =
            !mForBranch2 ? mTeeState->Branch2() : mTeeState->Branch1();

        // Step 5.
        if (!otherCanceled) {
          // Step 5.1 (using the name clonedChunk because we don't want to name
          // the completion record explicitly)
          JS::RootedObject clonedChunk(cx, CloneAsUint8Array(cx, mChunk));

          // Step 5.2. If cloneResult is an abrupt completion,
          if (!clonedChunk) {
            JS::RootedValue exception(cx);
            if (!JS_GetPendingException(cx, &exception)) {
              // Uncatchable exception. Return with pending
              // exception still on context.
              return;
            }

            // It's not expliclitly stated, but I assume the intention here is
            // that we perform a normal completion here, so we clear the
            // exception.
            JS_ClearPendingException(cx);

            // Step 5.2.1

            ReadableByteStreamControllerError(
                byobBranch->Controller()->AsByte(), exception, rv);
            if (rv.MaybeSetPendingException(cx)) {
              return;
            }

            // Step 5.2.2.
            ReadableByteStreamControllerError(
                otherBranch->Controller()->AsByte(), exception, rv);
            if (rv.MaybeSetPendingException(cx)) {
              return;
            }

            // Step 5.2.3.
            RefPtr<ReadableStream> stream = mTeeState->GetStream();
            RefPtr<Promise> cancelPromise =
                ReadableStreamCancel(cx, stream, exception, rv);
            if (rv.MaybeSetPendingException(cx)) {
              return;
            }
            mTeeState->CancelPromise()->MaybeResolve(cancelPromise);

            // Step 5.2.4.
            return;
          }

          // Step 5.3 (implicitly handled above by name selection)
          // Step 5.4.
          if (!byobCanceled) {
            RefPtr<ReadableByteStreamController> controller(
                byobBranch->Controller()->AsByte());
            ReadableByteStreamControllerRespondWithNewView(cx, controller,
                                                           mChunk, rv);
            if (rv.MaybeSetPendingException(cx)) {
              return;
            }
          }

          // Step 5.4.
          RefPtr<ReadableByteStreamController> otherController =
              otherBranch->Controller()->AsByte();
          ReadableByteStreamControllerEnqueue(cx, otherController, clonedChunk,
                                              rv);
          if (rv.MaybeSetPendingException(cx)) {
            return;
          }
          // Step 6.
        } else if (!byobCanceled) {
          RefPtr<ReadableByteStreamController> byobController =
              byobBranch->Controller()->AsByte();
          ReadableByteStreamControllerRespondWithNewView(cx, byobController,
                                                         mChunk, rv);
          if (rv.MaybeSetPendingException(cx)) {
            return;
          }
        }

        // Step 7.
        mTeeState->SetReading(false);

        // Step 8.
        if (mTeeState->ReadAgainForBranch1()) {
          ByteStreamTeePullAlgorithm(cx, 1, MOZ_KnownLive(mTeeState), rv);
          if (rv.MaybeSetPendingException(cx)) {
            return;
          }
        } else if (mTeeState->ReadAgainForBranch2()) {
          ByteStreamTeePullAlgorithm(cx, 2, MOZ_KnownLive(mTeeState), rv);
          if (rv.MaybeSetPendingException(cx)) {
            return;
          }
        }
      }

      bool Suppressed() override {
        nsIGlobalObject* global = mTeeState->GetStream()->GetParentObject();
        return global && global->IsInSyncOperation();
      }
    };

    MOZ_ASSERT(aChunk.isObjectOrNull());
    MOZ_ASSERT(aChunk.toObjectOrNull());
    JS::RootedObject chunk(aCx, aChunk.toObjectOrNull());
    RefPtr<PullWithBYOBReaderChunkMicrotask> task =
        MakeRefPtr<PullWithBYOBReaderChunkMicrotask>(aCx, mTeeState, chunk,
                                                     mForBranch2);
    CycleCollectedJSContext::Get()->DispatchToMicroTask(task.forget());
  }

  MOZ_CAN_RUN_SCRIPT
  void CloseSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override {
    // Step 1.
    mTeeState->SetReading(false);

    // Step 2.
    bool byobCanceled =
        mForBranch2 ? mTeeState->Canceled2() : mTeeState->Canceled1();

    // Step 3.
    bool otherCanceled =
        !mForBranch2 ? mTeeState->Canceled2() : mTeeState->Canceled1();

    // Rather than store byobBranch / otherBranch, we re-derive the pointers
    // below, as borrowed from steps 16.2/16.3
    ReadableStream* byobBranch =
        mForBranch2 ? mTeeState->Branch2() : mTeeState->Branch1();
    ReadableStream* otherBranch =
        !mForBranch2 ? mTeeState->Branch2() : mTeeState->Branch1();

    // Step 4.
    if (!byobCanceled) {
      ReadableByteStreamControllerClose(aCx, byobBranch->Controller()->AsByte(),
                                        aRv);
      if (aRv.Failed()) {
        return;
      }
    }
    // Step 5.
    if (!otherCanceled) {
      ReadableByteStreamControllerClose(
          aCx, otherBranch->Controller()->AsByte(), aRv);
      if (aRv.Failed()) {
        return;
      }
    }

    // Step 6.
    if (!aChunk.isUndefined()) {
      MOZ_ASSERT(aChunk.isObject());
      MOZ_ASSERT(aChunk.toObjectOrNull());

      JS::RootedObject chunkObject(aCx, &aChunk.toObject());
      MOZ_ASSERT(JS_IsArrayBufferViewObject(chunkObject));
      // Step 6.1.
      MOZ_ASSERT(JS_GetArrayBufferViewByteLength(chunkObject) == 0);

      // Step 6.2.
      if (!byobCanceled) {
        RefPtr<ReadableByteStreamController> byobController(
            byobBranch->Controller()->AsByte());
        ReadableByteStreamControllerRespondWithNewView(aCx, byobController,
                                                       chunkObject, aRv);
        if (aRv.Failed()) {
          return;
        }
      }

      // Step 6.3
      if (!otherCanceled &&
          !otherBranch->Controller()->AsByte()->PendingPullIntos().isEmpty()) {
        RefPtr<ReadableByteStreamController> otherController(
            otherBranch->Controller()->AsByte());
        ReadableByteStreamControllerRespond(aCx, otherController, 0, aRv);
        if (aRv.Failed()) {
          return;
        }
      }
    }

    // Step 7.
    if (!byobCanceled || !otherCanceled) {
      mTeeState->CancelPromise()->MaybeResolveWithUndefined();
    }
  }

  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                  ErrorResult& errorResult) override {
    // Step 1.
    mTeeState->SetReading(false);
  }
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(PullWithBYOBReader_ReadIntoRequest,
                                   ReadIntoRequest, mTeeState)
NS_IMPL_ADDREF_INHERITED(PullWithBYOBReader_ReadIntoRequest, ReadIntoRequest)
NS_IMPL_RELEASE_INHERITED(PullWithBYOBReader_ReadIntoRequest, ReadIntoRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PullWithBYOBReader_ReadIntoRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadIntoRequest)

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
// Step 16.
void PullWithBYOBReader(JSContext* aCx, TeeState* aTeeState,
                        JS::HandleObject aView, bool aForBranch2,
                        ErrorResult& aRv) {
  // Step 16.1
  if (aTeeState->GetReader()->IsDefault()) {
    // Step 16.1.1
    MOZ_ASSERT(aTeeState->GetDefaultReader()->ReadRequests().isEmpty());

    // Step 16.1.2. Perform !ReadableStreamReaderGenericRelease(reader).
    ReadableStreamReaderGenericRelease(aTeeState->GetReader(), aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 16.1.3. Set reader to !AcquireReadableStreamBYOBReader(stream).
    RefPtr<ReadableStreamBYOBReader> reader =
        AcquireReadableStreamBYOBReader(aCx, aTeeState->GetStream(), aRv);
    if (aRv.Failed()) {
      return;
    }
    aTeeState->SetReader(reader);

    // Step 16.1.4. Perform forwardReaderError, given reader.
    ForwardReaderError(aTeeState, reader);
  }

  // Step 16.2. Unused in this function, moved to consumers.
  // Step 16.3. Unused in this function, moved to consumers.

  // Step 16.4.
  RefPtr<ReadIntoRequest> readIntoRequest =
      new PullWithBYOBReader_ReadIntoRequest(aTeeState, aForBranch2);

  // Step 16.5.
  RefPtr<ReadableStreamBYOBReader> byobReader =
      aTeeState->GetReader()->AsBYOB();
  ReadableStreamBYOBReaderRead(aCx, byobReader, aView, readIntoRequest, aRv);
}

class ForwardReaderErrorPromiseHandler final : public PromiseNativeHandler {
  ~ForwardReaderErrorPromiseHandler() = default;
  RefPtr<TeeState> mTeeState;
  RefPtr<ReadableStreamGenericReader> mReader;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ForwardReaderErrorPromiseHandler)

  ForwardReaderErrorPromiseHandler(TeeState* aTeeState,
                                   ReadableStreamGenericReader* aReader)
      : mTeeState(aTeeState), mReader(aReader) {}

  MOZ_CAN_RUN_SCRIPT
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
  }

  MOZ_CAN_RUN_SCRIPT
  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    // Step 14.1.1
    if (mTeeState->GetReader() != mReader) {
      return;
    }

    ErrorResult rv;
    // Step 14.1.2: Perform
    // !ReadableByteStreamControllerError(branch1.[[controller]], r).
    MOZ_ASSERT(mTeeState->Branch1()->Controller()->IsByte());
    ReadableByteStreamControllerError(
        mTeeState->Branch1()->Controller()->AsByte(), aValue, rv);
    if (rv.MaybeSetPendingException(
            aCx, "ReadableByteStreamTee: Error during forwardReaderError")) {
      return;
    }

    // Step 14.1.3: Perform
    // !ReadableByteStreamControllerError(branch2.[[controller]], r).
    MOZ_ASSERT(mTeeState->Branch2()->Controller()->IsByte());
    ReadableByteStreamControllerError(
        mTeeState->Branch2()->Controller()->AsByte(), aValue, rv);
    if (rv.MaybeSetPendingException(
            aCx, "ReadableByteStreamTee: Error during forwardReaderError")) {
      return;
    }

    // Step 14.1.4: If canceled1 is false or canceled2 is false, resolve
    // cancelPromise with undefined.
    if (!mTeeState->Canceled1() || !mTeeState->Canceled2()) {
      mTeeState->CancelPromise()->MaybeResolveWithUndefined();
    }
  }
};

NS_IMPL_CYCLE_COLLECTION(ForwardReaderErrorPromiseHandler, mTeeState, mReader)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ForwardReaderErrorPromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ForwardReaderErrorPromiseHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ForwardReaderErrorPromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// See https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
// Step 14.
void ForwardReaderError(TeeState* aTeeState,
                        ReadableStreamGenericReader* aThisReader) {
  aThisReader->ClosedPromise()->AppendNativeHandler(
      new ForwardReaderErrorPromiseHandler(aTeeState, aThisReader));
}

class ReadableByteStreamTeeCancelAlgorithm final
    : public UnderlyingSourceCancelCallbackHelper {
  RefPtr<TeeState> mTeeState;
  size_t mStreamIndex;

  size_t otherStream() {
    if (mStreamIndex == 1) {
      return 2;
    }
    MOZ_ASSERT(mStreamIndex == 2);
    return 1;
  }

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ReadableByteStreamTeeCancelAlgorithm,
                                           UnderlyingSourceCancelCallbackHelper)

  explicit ReadableByteStreamTeeCancelAlgorithm(TeeState* aTeeState,
                                                size_t aStreamIndex)
      : mTeeState(aTeeState), mStreamIndex(aStreamIndex) {}

  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
  // Steps 19 and 20 both use this class.
  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 1.
    mTeeState->SetCanceled(mStreamIndex, true);

    // Step 2.
    mTeeState->SetReason(mStreamIndex, aReason.Value());

    // Step 3.
    if (mTeeState->Canceled(otherStream())) {
      // Step 3.1
      JS::RootedObject compositeReason(aCx, JS::NewArrayObject(aCx, 2));
      if (!compositeReason) {
        aRv.StealExceptionFromJSContext(aCx);
        return nullptr;
      }

      JS::RootedValue reason1(aCx, mTeeState->Reason1());
      if (!JS_SetElement(aCx, compositeReason, 0, reason1)) {
        aRv.StealExceptionFromJSContext(aCx);
        return nullptr;
      }

      JS::RootedValue reason2(aCx, mTeeState->Reason2());
      if (!JS_SetElement(aCx, compositeReason, 1, reason2)) {
        aRv.StealExceptionFromJSContext(aCx);
        return nullptr;
      }

      // Step 3.2
      JS::RootedValue compositeReasonValue(aCx,
                                           JS::ObjectValue(*compositeReason));
      RefPtr<ReadableStream> stream(mTeeState->GetStream());
      RefPtr<Promise> cancelResult =
          ReadableStreamCancel(aCx, stream, compositeReasonValue, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }

      // Step 3.3
      mTeeState->CancelPromise()->MaybeResolve(cancelResult);
    }

    // Step 4.
    return do_AddRef(mTeeState->CancelPromise());
  }

 protected:
  ~ReadableByteStreamTeeCancelAlgorithm() = default;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadableByteStreamTeeCancelAlgorithm)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    ReadableByteStreamTeeCancelAlgorithm, UnderlyingSourceCancelCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTeeState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    ReadableByteStreamTeeCancelAlgorithm, UnderlyingSourceCancelCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTeeState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(ReadableByteStreamTeeCancelAlgorithm,
                         UnderlyingSourceCancelCallbackHelper)
NS_IMPL_RELEASE_INHERITED(ReadableByteStreamTeeCancelAlgorithm,
                          UnderlyingSourceCancelCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableByteStreamTeeCancelAlgorithm)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceCancelCallbackHelper)

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
void ReadableByteStreamTee(JSContext* aCx, ReadableStream* aStream,
                           nsTArray<RefPtr<ReadableStream>>& aResult,
                           ErrorResult& aRv) {
  // Step 1. Implicit
  // Step 2.
  MOZ_ASSERT(aStream->Controller()->IsByte());

  // Step 3-13 captured as part of TeeState allocation
  RefPtr<TeeState> teeState = TeeState::Create(aCx, aStream, false, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 14: See ForwardReaderError
  // Step 15. See PullWithDefaultReader
  // Step 16. See PullWithBYOBReader
  // Step 17,18. See {Native,}ByteStreamTeePullAlgorithm
  // Step 19,20. See ReadableByteStreamTeeCancelAlgorithm
  // Step 21. Elided because consumers know how to handle nullptr correctly.
  // Step 22.
  nsCOMPtr<nsIGlobalObject> global = aStream->GetParentObject();
  RefPtr<UnderlyingSourcePullCallbackHelper> pull1Algorithm =
      new NativeByteStreamTeePullAlgorithm(teeState, 1);
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancel1Algorithm =
      new ReadableByteStreamTeeCancelAlgorithm(teeState, 1);
  teeState->SetBranch1(CreateReadableByteStream(
      aCx, global, nullptr, pull1Algorithm, cancel1Algorithm, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 23.
  RefPtr<UnderlyingSourcePullCallbackHelper> pull2Algorithm =
      new NativeByteStreamTeePullAlgorithm(teeState, 2);
  RefPtr<UnderlyingSourceCancelCallbackHelper> cancel2Algorithm =
      new ReadableByteStreamTeeCancelAlgorithm(teeState, 2);
  teeState->SetBranch2(CreateReadableByteStream(
      aCx, global, nullptr, pull2Algorithm, cancel2Algorithm, aRv));
  if (aRv.Failed()) {
    return;
  }

  // Step 24.
  ForwardReaderError(teeState, teeState->GetReader());

  // Step 25.
  aResult.AppendElement(teeState->Branch1());
  aResult.AppendElement(teeState->Branch2());
}
}  // namespace mozilla::dom
