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

NS_IMPL_CYCLE_COLLECTION_CLASS(TeeState)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TeeState)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TeeState)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TeeState)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TeeState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBranch1)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBranch2)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCancelPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPullAlgorithm)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TeeState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBranch1)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBranch2)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCancelPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPullAlgorithm)
  tmp->mReason1.setNull();
  tmp->mReason2.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TeeState)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReason1)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReason2)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

TeeState::TeeState(JSContext* aCx, ReadableStream* aStream,
                   bool aCloneForBranch2)
    : mStream(aStream),
      mReason1(JS::NullValue()),
      mReason2(JS::NullValue()),
      mCloneForBranch2(aCloneForBranch2) {
  mozilla::HoldJSObjects(this);
  MOZ_RELEASE_ASSERT(!aCloneForBranch2,
                     "cloneForBranch2 path is not implemented.");
}

already_AddRefed<TeeState> TeeState::Create(JSContext* aCx,
                                            ReadableStream* aStream,
                                            bool aCloneForBranch2,
                                            ErrorResult& aRv) {
  RefPtr<TeeState> teeState = new TeeState(aCx, aStream, aCloneForBranch2);

  RefPtr<ReadableStreamDefaultReader> reader =
      AcquireReadableStreamDefaultReader(aCx, teeState->GetStream(), aRv);
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
