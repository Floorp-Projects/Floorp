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

namespace mozilla::dom {

class ReadableStreamDefaultTeeSourceAlgorithms;

enum class TeeBranch : bool {
  Branch1,
  Branch2,
};

inline TeeBranch OtherTeeBranch(TeeBranch aBranch) {
  if (aBranch == TeeBranch::Branch1) {
    return TeeBranch::Branch2;
  }
  return TeeBranch::Branch1;
}

// A closure capturing the free variables in the ReadableStreamTee family of
// algorithms.
// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
struct TeeState : public nsISupports {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TeeState)

  static already_AddRefed<TeeState> Create(ReadableStream* aStream,
                                           bool aCloneForBranch2,
                                           ErrorResult& aRv);

  ReadableStream* GetStream() const { return mStream; }
  void SetStream(ReadableStream* aStream) { mStream = aStream; }

  ReadableStreamGenericReader* GetReader() const { return mReader; }
  void SetReader(ReadableStreamGenericReader* aReader) { mReader = aReader; }

  ReadableStreamDefaultReader* GetDefaultReader() const {
    return mReader->AsDefault();
  }

  bool ReadAgain() const { return mReadAgain; }
  void SetReadAgain(bool aReadAgain) { mReadAgain = aReadAgain; }

  // ReadableByteStreamTee uses ReadAgainForBranch{1,2};
  bool ReadAgainForBranch1() const { return ReadAgain(); }
  void SetReadAgainForBranch1(bool aReadAgainForBranch1) {
    SetReadAgain(aReadAgainForBranch1);
  }

  bool ReadAgainForBranch2() const { return mReadAgainForBranch2; }
  void SetReadAgainForBranch2(bool aReadAgainForBranch2) {
    mReadAgainForBranch2 = aReadAgainForBranch2;
  }

  bool Reading() const { return mReading; }
  void SetReading(bool aReading) { mReading = aReading; }

  bool Canceled1() const { return mCanceled1; }
  void SetCanceled1(bool aCanceled1) { mCanceled1 = aCanceled1; }

  bool Canceled2() const { return mCanceled2; }
  void SetCanceled2(bool aCanceled2) { mCanceled2 = aCanceled2; }

  void SetCanceled(TeeBranch aBranch, bool aCanceled) {
    aBranch == TeeBranch::Branch1 ? SetCanceled1(aCanceled)
                                  : SetCanceled2(aCanceled);
  }
  bool Canceled(TeeBranch aBranch) {
    return aBranch == TeeBranch::Branch1 ? Canceled1() : Canceled2();
  }

  JS::Value Reason1() const { return mReason1; }
  void SetReason1(JS::Handle<JS::Value> aReason1) { mReason1 = aReason1; }

  JS::Value Reason2() const { return mReason2; }
  void SetReason2(JS::Handle<JS::Value> aReason2) { mReason2 = aReason2; }

  void SetReason(TeeBranch aBranch, JS::Handle<JS::Value> aReason) {
    aBranch == TeeBranch::Branch1 ? SetReason1(aReason) : SetReason2(aReason);
  }

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

  // Some code is better served by using an enum into various internal slots to
  // avoid duplication: Here we provide alternative accessors for that case.
  ReadableStream* Branch(TeeBranch aBranch) const {
    return aBranch == TeeBranch::Branch1 ? Branch1() : Branch2();
  }

  void SetReadAgainForBranch(TeeBranch aBranch, bool aValue) {
    if (aBranch == TeeBranch::Branch1) {
      SetReadAgainForBranch1(aValue);
      return;
    }
    SetReadAgainForBranch2(aValue);
  }

  MOZ_CAN_RUN_SCRIPT void PullCallback(JSContext* aCx, nsIGlobalObject* aGlobal,
                                       ErrorResult& aRv);

 private:
  TeeState(ReadableStream* aStream, bool aCloneForBranch2);

  // Implicit:
  RefPtr<ReadableStream> mStream;

  // Step 3. (Step Numbering is based on ReadableStreamDefaultTee)
  RefPtr<ReadableStreamGenericReader> mReader;

  // Step 4.
  bool mReading = false;

  // Step 5.
  // (Aliased to readAgainForBranch1, for the purpose of ReadableByteStreamTee)
  bool mReadAgain = false;

  // ReadableByteStreamTee
  bool mReadAgainForBranch2 = false;

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

  virtual ~TeeState() { mozilla::DropJSObjects(this); }
};

}  // namespace mozilla::dom
#endif
