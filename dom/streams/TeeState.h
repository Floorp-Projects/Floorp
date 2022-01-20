/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TeeState_h
#define mozilla_dom_TeeState_h

#include "mozilla/HoldDropJSObjects.h"
#include "nsISupports.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/Promise.h"
#include "nsISupportsBase.h"

namespace mozilla::dom {

class ReadableStreamDefaultTeePullAlgorithm;

// A closure capturing the free variables in the ReadableStreamTee family of
// algorithms.
// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
struct TeeState : public nsISupports {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TeeState)

  static already_AddRefed<TeeState> Create(JSContext* aCx,
                                           ReadableStream* aStream,
                                           bool aCloneForBranch2,
                                           ErrorResult& aRv);

  ReadableStream* GetStream() const { return mStream; }
  void SetStream(ReadableStream* aStream) { mStream = aStream; }

  ReadableStreamDefaultReader* GetReader() const { return mReader; }
  void SetReader(ReadableStreamDefaultReader* aReader) { mReader = aReader; }

  bool ReadAgain() const { return mReadAgain; }
  void SetReadAgain(bool aReadAgain) { mReadAgain = aReadAgain; }

  bool Reading() const { return mReading; }
  void SetReading(bool aReading) { mReading = aReading; }

  bool Canceled1() const { return mCanceled1; }
  void SetCanceled1(bool aCanceled1) { mCanceled1 = aCanceled1; }

  bool Canceled2() const { return mCanceled2; }
  void SetCanceled2(bool aCanceled2) { mCanceled2 = aCanceled2; }

  JS::Value Reason1() const { return mReason1; }
  void SetReason1(JS::HandleValue aReason1) { mReason1 = aReason1; }

  JS::Value Reason2() const { return mReason2; }
  void SetReason2(JS::HandleValue aReason2) { mReason2 = aReason2; }

  ReadableStream* Branch1() const { return mBranch1; }
  void SetBranch1(already_AddRefed<ReadableStream> aBranch1) {
    mBranch1 = aBranch1;
  }

  ReadableStream* Branch2() const { return mBranch2; }
  void SetBranch2(already_AddRefed<ReadableStream> aBranch2) {
    mBranch2 = aBranch2;
  }

  Promise* CancelPromise() const { return mCancelPromise; }
  void SetCancelPromise(Promise* aCancelPromise) {
    mCancelPromise = aCancelPromise;
  }

  bool CloneForBranch2() const { return mCloneForBranch2; }
  void setCloneForBranch2(bool aCloneForBranch2) {
    mCloneForBranch2 = aCloneForBranch2;
  }

  void SetPullAlgorithm(ReadableStreamDefaultTeePullAlgorithm* aPullAlgorithm);
  ReadableStreamDefaultTeePullAlgorithm* PullAlgorithm() {
    return mPullAlgorithm;
  }

 private:
  TeeState(JSContext* aCx, ReadableStream* aStream, bool aCloneForBranch2);

  // Implicit:
  RefPtr<ReadableStream> mStream;

  // Step 3.
  RefPtr<ReadableStreamDefaultReader> mReader;

  // Step 4.
  bool mReading = false;

  // Step 5.
  bool mReadAgain = false;

  // Step 6.
  bool mCanceled1 = false;

  // Step 7.
  bool mCanceled2 = false;

  // Step 8.
  JS::Heap<JS::Value> mReason1;

  // Step 9.
  JS::Heap<JS::Value> mReason2;

  // Step 10.
  RefPtr<ReadableStream> mBranch1;

  // Step 11.
  RefPtr<ReadableStream> mBranch2;

  // Step 12.
  RefPtr<Promise> mCancelPromise;

  // Implicit:
  bool mCloneForBranch2 = false;

  // Used as part of the recursive ChunkSteps call in the read request
  RefPtr<ReadableStreamDefaultTeePullAlgorithm> mPullAlgorithm;

  virtual ~TeeState() { mozilla::DropJSObjects(this); }
};

}  // namespace mozilla::dom
#endif
