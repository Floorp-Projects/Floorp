/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStreamTee.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
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
  RefPtr<ReadableStreamDefaultReader> reader(mTeeState->GetReader());
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
        RefPtr<ReadableStreamDefaultController> controller(
            mTeeState->Branch1()->Controller());
        ReadableStreamDefaultControllerEnqueue(cx, controller, chunk1, rv);
        (void)NS_WARN_IF(rv.Failed());
      }

      // Step 5.
      if (!mTeeState->Canceled2()) {
        IgnoredErrorResult rv;
        RefPtr<ReadableStreamDefaultController> controller(
            mTeeState->Branch2()->Controller());
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
        aCx, mTeeState->Branch1()->Controller(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Step 3.
  if (!mTeeState->Canceled2()) {
    ReadableStreamDefaultControllerClose(
        aCx, mTeeState->Branch2()->Controller(), aRv);
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

}  // namespace mozilla::dom
