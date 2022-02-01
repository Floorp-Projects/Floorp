/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamTee_h
#define mozilla_dom_ReadableStreamTee_h

#include "mozilla/dom/TeeState.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {
// Implementation of the Pull algorithm steps for ReadableStreamDefaultTee,
// from
// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// Step 12.
class ReadableStreamDefaultTeePullAlgorithm final
    : public UnderlyingSourcePullCallbackHelper {
  RefPtr<TeeState> mTeeState;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      ReadableStreamDefaultTeePullAlgorithm, UnderlyingSourcePullCallbackHelper)

  explicit ReadableStreamDefaultTeePullAlgorithm(TeeState* aTeeState)
      : mTeeState(aTeeState) {}

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> PullCallback(JSContext* aCx,
                                         nsIGlobalObject* aGlobal,
                                         ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override {
    nsCOMPtr<nsIGlobalObject> global(aController.GetParentObject());
    return PullCallback(aCx, global, aRv);
  }

 protected:
  ~ReadableStreamDefaultTeePullAlgorithm() = default;
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

  virtual void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                          ErrorResult& aRv) override;

  virtual void CloseSteps(JSContext* aCx, ErrorResult& aRv) override;

  virtual void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> aError,
                          ErrorResult& aRv) override;

 protected:
  virtual ~ReadableStreamDefaultTeeReadRequest() = default;
};

MOZ_CAN_RUN_SCRIPT void ReadableByteStreamTee(
    JSContext* aCx, ReadableStream* aStream,
    nsTArray<RefPtr<ReadableStream>>& aResult, ErrorResult& aRv);

}  // namespace mozilla::dom
#endif
