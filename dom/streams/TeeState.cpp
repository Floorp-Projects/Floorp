/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TeeState.h"

#include "js/Value.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamTee.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

using ::ImplCycleCollectionUnlink;

NS_IMPL_CYCLE_COLLECTION_WITH_JS_MEMBERS(TeeState,
                                         (mStream, mReader, mBranch1, mBranch2,
                                          mCancelPromise, mPullAlgorithm),
                                         (mReason1, mReason2))
NS_IMPL_CYCLE_COLLECTING_ADDREF(TeeState)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TeeState)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TeeState)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TeeState::TeeState(ReadableStream* aStream, bool aCloneForBranch2)
    : mStream(aStream),
      mReason1(JS::NullValue()),
      mReason2(JS::NullValue()),
      mCloneForBranch2(aCloneForBranch2) {
  mozilla::HoldJSObjects(this);
  MOZ_RELEASE_ASSERT(!aCloneForBranch2,
                     "cloneForBranch2 path is not implemented.");
}

already_AddRefed<TeeState> TeeState::Create(ReadableStream* aStream,
                                            bool aCloneForBranch2,
                                            ErrorResult& aRv) {
  RefPtr<TeeState> teeState = new TeeState(aStream, aCloneForBranch2);

  RefPtr<ReadableStreamDefaultReader> reader =
      AcquireReadableStreamDefaultReader(teeState->GetStream(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  teeState->SetReader(reader);

  RefPtr<Promise> promise =
      Promise::Create(teeState->GetStream()->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  teeState->SetCancelPromise(promise);

  return teeState.forget();
}

void TeeState::SetPullAlgorithm(
    ReadableStreamDefaultTeePullAlgorithm* aPullAlgorithm) {
  mPullAlgorithm = aPullAlgorithm;
}
}  // namespace mozilla::dom
