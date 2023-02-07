/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamDefaultController_h
#define mozilla_dom_ReadableStreamDefaultController_h

#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "mozilla/dom/QueueWithSizes.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"
#include "nsTArray.h"

namespace mozilla::dom {

class ReadableStream;
class ReadableStreamDefaultReader;
struct UnderlyingSource;
class ReadableStreamGenericReader;

class ReadableStreamDefaultController final : public ReadableStreamController,
                                              public nsWrapperCache {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      ReadableStreamDefaultController, ReadableStreamController)

 public:
  explicit ReadableStreamDefaultController(nsIGlobalObject* aGlobal);

 protected:
  ~ReadableStreamDefaultController() override;

 public:
  bool IsDefault() override { return true; }
  bool IsByte() override { return false; }
  ReadableStreamDefaultController* AsDefault() override { return this; }
  ReadableByteStreamController* AsByte() override { return nullptr; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Nullable<double> GetDesiredSize();

  MOZ_CAN_RUN_SCRIPT void Close(JSContext* aCx, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void Enqueue(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                  ErrorResult& aRv);

  void Error(JSContext* aCx, JS::Handle<JS::Value> aError, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CancelSteps(
      JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) override;
  MOZ_CAN_RUN_SCRIPT void PullSteps(JSContext* aCx, ReadRequest* aReadRequest,
                                    ErrorResult& aRv) override;

  void ReleaseSteps() override;

  // Internal Slot Accessors
  bool CloseRequested() const { return mCloseRequested; }
  void SetCloseRequested(bool aCloseRequested) {
    mCloseRequested = aCloseRequested;
  }

  bool PullAgain() const { return mPullAgain; }
  void SetPullAgain(bool aPullAgain) { mPullAgain = aPullAgain; }

  bool Pulling() const { return mPulling; }
  void SetPulling(bool aPulling) { mPulling = aPulling; }

  QueueWithSizes& Queue() { return mQueue; }

  double QueueTotalSize() const { return mQueueTotalSize; }
  void SetQueueTotalSize(double aQueueTotalSize) {
    mQueueTotalSize = aQueueTotalSize;
  }

  bool Started() const { return mStarted; }
  void SetStarted(bool aStarted) { mStarted = aStarted; }

  double StrategyHWM() const { return mStrategyHWM; }
  void SetStrategyHWM(double aStrategyHWM) { mStrategyHWM = aStrategyHWM; }

  QueuingStrategySize* StrategySizeAlgorithm() const {
    return mStrategySizeAlgorithm;
  }
  void setStrategySizeAlgorithm(QueuingStrategySize* aStrategySizeAlgorithm) {
    mStrategySizeAlgorithm = aStrategySizeAlgorithm;
  }

 private:
  // Internal Slots:
  bool mCloseRequested = false;
  bool mPullAgain = false;
  bool mPulling = false;
  QueueWithSizes mQueue = {};
  double mQueueTotalSize = 0.0;
  bool mStarted = false;
  double mStrategyHWM = false;
  RefPtr<QueuingStrategySize> mStrategySizeAlgorithm;
};

namespace streams_abstract {

MOZ_CAN_RUN_SCRIPT void SetUpReadableStreamDefaultController(
    JSContext* aCx, ReadableStream* aStream,
    ReadableStreamDefaultController* aController,
    UnderlyingSourceAlgorithmsBase* aAlgorithms, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void
SetupReadableStreamDefaultControllerFromUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream,
    JS::Handle<JSObject*> aUnderlyingSource,
    UnderlyingSource& aUnderlyingSourceDict, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableStreamDefaultControllerEnqueue(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableStreamDefaultControllerClose(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void ReadableStreamDefaultReaderRead(
    JSContext* aCx, ReadableStreamGenericReader* reader, ReadRequest* aRequest,
    ErrorResult& aRv);

void ReadableStreamDefaultControllerError(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    JS::Handle<JS::Value> aValue, ErrorResult& aRv);

Nullable<double> ReadableStreamDefaultControllerGetDesiredSize(
    ReadableStreamDefaultController* aController);

enum class CloseOrEnqueue { Close, Enqueue };

bool ReadableStreamDefaultControllerCanCloseOrEnqueueAndThrow(
    ReadableStreamDefaultController* aController,
    CloseOrEnqueue aCloseOrEnqueue, ErrorResult& aRv);

bool ReadableStreamDefaultControllerShouldCallPull(
    ReadableStreamDefaultController* aController);

}  // namespace streams_abstract

}  // namespace mozilla::dom

#endif  // mozilla_dom_ReadableStreamDefaultController_h
