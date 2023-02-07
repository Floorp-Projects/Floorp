/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TeeState.h"

#include "ReadableStreamTee.h"
#include "js/Value.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

using namespace streams_abstract;

NS_IMPL_CYCLE_COLLECTION_WITH_JS_MEMBERS(TeeState,
                                         (mStream, mReader, mBranch1, mBranch2,
                                          mCancelPromise),
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
      Promise::CreateInfallible(teeState->GetStream()->GetParentObject());
  teeState->SetCancelPromise(promise);

  return teeState.forget();
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// Pull Algorithm Steps:
void TeeState::PullCallback(JSContext* aCx, nsIGlobalObject* aGlobal,
                            ErrorResult& aRv) {
  // Step 13.1: If reading is true,
  if (Reading()) {
    // Step 13.1.1: Set readAgain to true.
    SetReadAgain(true);

    // Step 13.1.2: Return a promise resolved with undefined.
    // (The caller will create it if necessary)
    return;
  }

  // Step 13.2: Set reading to true.
  SetReading(true);

  // Step 13.3: Let readRequest be a read request with the following items:
  RefPtr<ReadRequest> readRequest =
      new ReadableStreamDefaultTeeReadRequest(this);

  // Step 13.4: Perform ! ReadableStreamDefaultReaderRead(reader, readRequest).
  RefPtr<ReadableStreamGenericReader> reader = GetReader();
  ReadableStreamDefaultReaderRead(aCx, reader, readRequest, aRv);

  // Step 13.5: Return a promise resolved with undefined.
  // (The caller will create it if necessary)
}

}  // namespace mozilla::dom
