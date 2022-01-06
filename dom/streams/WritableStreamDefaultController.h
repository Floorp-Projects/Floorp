/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WritableStreamDefaultController_h
#define mozilla_dom_WritableStreamDefaultController_h

#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/QueueWithSizes.h"
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/UnderlyingSinkCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"
#include "nsTArray.h"
#include "nsISupportsBase.h"

namespace mozilla {
namespace dom {

class AbortSignal;
class WritableStream;
struct UnderlyingSink;

class WritableStreamDefaultController final : public nsISupports,
                                              public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WritableStreamDefaultController)

  explicit WritableStreamDefaultController(nsISupports* aGlobal,
                                           WritableStream& aStream);

 protected:
  ~WritableStreamDefaultController();

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods/properties

  AbortSignal* Signal() { return mSignal; }

  MOZ_CAN_RUN_SCRIPT void Error(JSContext* aCx, JS::Handle<JS::Value> aError,
                                ErrorResult& aRv);

  // [[AbortSteps]]
  MOZ_CAN_RUN_SCRIPT virtual already_AddRefed<Promise> AbortSteps(
      JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv);

  // [[ErrorSteps]]
  virtual void ErrorSteps();

  // Internal Slot Accessors

  QueueWithSizes& Queue() { return mQueue; }

  double QueueTotalSize() const { return mQueueTotalSize; }
  void SetQueueTotalSize(double aQueueTotalSize) {
    mQueueTotalSize = aQueueTotalSize;
  }

  void SetSignal(AbortSignal* aSignal);

  bool Started() const { return mStarted; }
  void SetStarted(bool aStarted) { mStarted = aStarted; }

  double StrategyHWM() const { return mStrategyHWM; }
  void SetStrategyHWM(double aStrategyHWM) { mStrategyHWM = aStrategyHWM; }

  QueuingStrategySize* StrategySizeAlgorithm() const {
    return mStrategySizeAlgorithm;
  }
  void SetStrategySizeAlgorithm(QueuingStrategySize* aStrategySizeAlgorithm) {
    mStrategySizeAlgorithm = aStrategySizeAlgorithm;
  }

  UnderlyingSinkWriteCallbackHelper* GetWriteAlgorithm() {
    return mWriteAlgorithm;
  }
  void SetWriteAlgorithm(UnderlyingSinkWriteCallbackHelper* aWriteAlgorithm) {
    mWriteAlgorithm = aWriteAlgorithm;
  }

  UnderlyingSinkCloseCallbackHelper* GetCloseAlgorithm() {
    return mCloseAlgorithm;
  }
  void SetCloseAlgorithm(UnderlyingSinkCloseCallbackHelper* aCloseAlgorithm) {
    mCloseAlgorithm = aCloseAlgorithm;
  }

  UnderlyingSinkAbortCallbackHelper* GetAbortAlgorithm() {
    return mAbortAlgorithm;
  }
  void SetAbortAlgorithm(UnderlyingSinkAbortCallbackHelper* aAbortAlgorithm) {
    mAbortAlgorithm = aAbortAlgorithm;
  }

  WritableStream* Stream() { return mStream; }

  // WritableStreamDefaultControllerGetBackpressure
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-backpressure
  bool GetBackpressure() const {
    // Step 1. Let desiredSize be !
    // WritableStreamDefaultControllerGetDesiredSize(controller).
    double desiredSize = GetDesiredSize();
    // Step 2. Return true if desiredSize ≤ 0, or false otherwise.
    return desiredSize <= 0;
  }

  // WritableStreamDefaultControllerGetDesiredSize
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-desired-size
  double GetDesiredSize() const { return mStrategyHWM - mQueueTotalSize; }

  // WritableStreamDefaultControllerClearAlgorithms
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-clear-algorithms
  void ClearAlgorithms() {
    // Step 1. Set controller.[[writeAlgorithm]] to undefined.
    mWriteAlgorithm = nullptr;

    // Step 2. Set controller.[[closeAlgorithm]] to undefined.
    mCloseAlgorithm = nullptr;

    // Step 3. Set controller.[[abortAlgorithm]] to undefined.
    mAbortAlgorithm = nullptr;

    // Step 4. Set controller.[[strategySizeAlgorithm]] to undefined.
    mStrategySizeAlgorithm = nullptr;
  }

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Internal Slots
  QueueWithSizes mQueue = {};
  double mQueueTotalSize = 0.0;
  RefPtr<AbortSignal> mSignal;
  bool mStarted = false;
  double mStrategyHWM = 0.0;

  RefPtr<QueuingStrategySize> mStrategySizeAlgorithm;
  RefPtr<UnderlyingSinkWriteCallbackHelper> mWriteAlgorithm;
  RefPtr<UnderlyingSinkCloseCallbackHelper> mCloseAlgorithm;
  RefPtr<UnderlyingSinkAbortCallbackHelper> mAbortAlgorithm;
  RefPtr<WritableStream> mStream;
};

extern void SetUpWritableStreamDefaultController(
    JSContext* aCx, WritableStream* aStream,
    WritableStreamDefaultController* aController,
    UnderlyingSinkStartCallbackHelper* aStartAlgorithm,
    UnderlyingSinkWriteCallbackHelper* aWriteAlgorithm,
    UnderlyingSinkCloseCallbackHelper* aCloseAlgorithm,
    UnderlyingSinkAbortCallbackHelper* aAbortAlgorithm, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);

extern void SetUpWritableStreamDefaultControllerFromUnderlyingSink(
    JSContext* aCx, WritableStream* aStream, JS::HandleObject aUnderlyingSink,
    UnderlyingSink& aUnderlyingSinkDict, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT extern void WritableStreamDefaultControllerClose(
    JSContext* aCx, WritableStreamDefaultController* aController,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT extern void WritableStreamDefaultControllerWrite(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, double chunkSize, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT extern void WritableStreamDefaultControllerError(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aError, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT extern void WritableStreamDefaultControllerErrorIfNeeded(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aError, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT extern double WritableStreamDefaultControllerGetChunkSize(
    JSContext* aCx, WritableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WritableStreamDefaultController_h
