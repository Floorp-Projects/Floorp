/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamTee_h
#define mozilla_dom_ReadableStreamTee_h

#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {
struct TeeState;
enum class TeeBranch : bool;
class ReadableStream;

// Implementation of the Pull algorithm steps for ReadableStreamDefaultTee,
// from
// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// Step 12.
class ReadableStreamDefaultTeeSourceAlgorithms final
    : public UnderlyingSourceAlgorithmsBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      ReadableStreamDefaultTeeSourceAlgorithms, UnderlyingSourceAlgorithmsBase)

  ReadableStreamDefaultTeeSourceAlgorithms(TeeState* aTeeState,
                                           TeeBranch aBranch)
      : mTeeState(aTeeState), mBranch(aBranch) {}

  MOZ_CAN_RUN_SCRIPT void StartCallback(JSContext* aCx,
                                        ReadableStreamController& aController,
                                        JS::MutableHandle<JS::Value> aRetVal,
                                        ErrorResult& aRv) override {
    aRetVal.setUndefined();
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

 protected:
  ~ReadableStreamDefaultTeeSourceAlgorithms() override = default;

 private:
  // Virtually const, but is cycle collected
  MOZ_KNOWN_LIVE RefPtr<TeeState> mTeeState;
  TeeBranch mBranch;
};

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// Step 12.3
struct ReadableStreamDefaultTeeReadRequest final : public ReadRequest {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ReadableStreamDefaultTeeReadRequest,
                                           ReadRequest)

  RefPtr<TeeState> mTeeState;

  explicit ReadableStreamDefaultTeeReadRequest(TeeState* aTeeState)
      : mTeeState(aTeeState) {}

  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT void CloseSteps(JSContext* aCx, ErrorResult& aRv) override;

  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> aError,
                  ErrorResult& aRv) override;

 protected:
  ~ReadableStreamDefaultTeeReadRequest() override = default;
};

namespace streams_abstract {
MOZ_CAN_RUN_SCRIPT void ReadableByteStreamTee(
    JSContext* aCx, ReadableStream* aStream,
    nsTArray<RefPtr<ReadableStream>>& aResult, ErrorResult& aRv);
}

}  // namespace mozilla::dom
#endif
