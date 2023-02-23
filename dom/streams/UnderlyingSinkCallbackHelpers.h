/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UnderlyingSinkCallbackHelpers_h
#define mozilla_dom_UnderlyingSinkCallbackHelpers_h

#include "mozilla/Maybe.h"
#include "mozilla/Buffer.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnderlyingSinkBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsIAsyncOutputStream.h"

/*
 * See the comment in UnderlyingSourceCallbackHelpers.h!
 *
 * A native implementation of these callbacks is however currently not required.
 */
namespace mozilla::dom {

class WritableStreamDefaultController;

class UnderlyingSinkAlgorithmsBase : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UnderlyingSinkAlgorithmsBase)

  MOZ_CAN_RUN_SCRIPT virtual void StartCallback(
      JSContext* aCx, WritableStreamDefaultController& aController,
      JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) = 0;

  MOZ_CAN_RUN_SCRIPT virtual already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) = 0;

  MOZ_CAN_RUN_SCRIPT virtual already_AddRefed<Promise> CloseCallback(
      JSContext* aCx, ErrorResult& aRv) = 0;

  MOZ_CAN_RUN_SCRIPT virtual already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) = 0;

  // Implement this when you need to release underlying resources immediately
  // from closed/errored(aborted) streams, without waiting for GC.
  virtual void ReleaseObjects() {}

 protected:
  virtual ~UnderlyingSinkAlgorithmsBase() = default;
};

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
class UnderlyingSinkAlgorithms final : public UnderlyingSinkAlgorithmsBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      UnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)

  UnderlyingSinkAlgorithms(nsIGlobalObject* aGlobal,
                           JS::Handle<JSObject*> aUnderlyingSink,
                           UnderlyingSink& aUnderlyingSinkDict)
      : mGlobal(aGlobal), mUnderlyingSink(aUnderlyingSink) {
    // Step 6. (implicit Step 2.)
    if (aUnderlyingSinkDict.mStart.WasPassed()) {
      mStartCallback = aUnderlyingSinkDict.mStart.Value();
    }

    // Step 7. (implicit Step 3.)
    if (aUnderlyingSinkDict.mWrite.WasPassed()) {
      mWriteCallback = aUnderlyingSinkDict.mWrite.Value();
    }

    // Step 8. (implicit Step 4.)
    if (aUnderlyingSinkDict.mClose.WasPassed()) {
      mCloseCallback = aUnderlyingSinkDict.mClose.Value();
    }

    // Step 9. (implicit Step 5.)
    if (aUnderlyingSinkDict.mAbort.WasPassed()) {
      mAbortCallback = aUnderlyingSinkDict.mAbort.Value();
    }

    mozilla::HoldJSObjects(this);
  };

  MOZ_CAN_RUN_SCRIPT void StartCallback(
      JSContext* aCx, WritableStreamDefaultController& aController,
      JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CloseCallback(
      JSContext* aCx, ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

 protected:
  ~UnderlyingSinkAlgorithms() override { mozilla::DropJSObjects(this); }

 private:
  // Virtually const, but are cycle collected
  nsCOMPtr<nsIGlobalObject> mGlobal;
  JS::Heap<JSObject*> mUnderlyingSink;
  MOZ_KNOWN_LIVE RefPtr<UnderlyingSinkStartCallback> mStartCallback;
  MOZ_KNOWN_LIVE RefPtr<UnderlyingSinkWriteCallback> mWriteCallback;
  MOZ_KNOWN_LIVE RefPtr<UnderlyingSinkCloseCallback> mCloseCallback;
  MOZ_KNOWN_LIVE RefPtr<UnderlyingSinkAbortCallback> mAbortCallback;
};

// https://streams.spec.whatwg.org/#writablestream-set-up
// Wrappers defined by the "Set up" methods in the spec.
// (closeAlgorithmWrapper, abortAlgorithmWrapper)
// This helps you just return nullptr when 1) the algorithm is synchronous, or
// 2) an error occurred, as this wrapper will return a resolved or rejected
// promise respectively.
// Note that StartCallback is only for JS consumers to access the
// controller, and thus is no-op here since native consumers can call
// `ErrorNative()` etc. without direct controller access.
class UnderlyingSinkAlgorithmsWrapper : public UnderlyingSinkAlgorithmsBase {
 public:
  void StartCallback(JSContext* aCx,
                     WritableStreamDefaultController& aController,
                     JS::MutableHandle<JS::Value> aRetVal,
                     ErrorResult& aRv) final {
    // Step 1: Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CloseCallback(
      JSContext* aCx, ErrorResult& aRv) final;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) final;

  virtual already_AddRefed<Promise> CloseCallbackImpl(JSContext* aCx,
                                                      ErrorResult& aRv) {
    // (closeAlgorithm is optional, give null by default)
    return nullptr;
  }

  virtual already_AddRefed<Promise> AbortCallbackImpl(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) {
    // (abortAlgorithm is optional, give null by default)
    return nullptr;
  }
};

class WritableStreamToOutput final : public UnderlyingSinkAlgorithmsWrapper,
                                     public nsIOutputStreamCallback {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WritableStreamToOutput,
                                           UnderlyingSinkAlgorithmsBase)

  WritableStreamToOutput(nsIGlobalObject* aParent,
                         nsIAsyncOutputStream* aOutput)
      : mWritten(0), mParent(aParent), mOutput(aOutput) {}

  // Streams algorithms

  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override;

  // No CloseCallbackImpl() since ReleaseObjects() will call Close()

  already_AddRefed<Promise> AbortCallbackImpl(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

  void ReleaseObjects() override;

 private:
  ~WritableStreamToOutput() override = default;

  void ClearData() {
    mData = Nothing();
    mPromise = nullptr;
    mWritten = 0;
  }

  uint32_t mWritten;
  nsCOMPtr<nsIGlobalObject> mParent;
  nsCOMPtr<nsIAsyncOutputStream> mOutput;
  RefPtr<Promise> mPromise;  // Resolved when entirely written
  Maybe<Buffer<uint8_t>> mData;
};

}  // namespace mozilla::dom

#endif
