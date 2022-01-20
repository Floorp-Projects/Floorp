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
#include "mozilla/dom/ReadRequest.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"
#include "nsTArray.h"
#include "nsISupportsBase.h"

namespace mozilla {
namespace dom {

class ReadableStream;
struct UnderlyingSource;
class UnderlyingSourceCancelCallbackHelper;
class UnderlyingSourcePullCallbackHelper;

class ReadableStreamDefaultController final : public nsISupports,
                                              public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReadableStreamDefaultController)

 public:
  explicit ReadableStreamDefaultController(nsISupports* aGlobal);

 protected:
  ~ReadableStreamDefaultController();

  nsCOMPtr<nsIGlobalObject> mGlobal;

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Nullable<double> GetDesiredSize();

  void Close(JSContext* aCx, ErrorResult& aRv);

  void Enqueue(JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aRv);

  void Error(JSContext* aCx, JS::Handle<JS::Value> aError, ErrorResult& aRv);

  virtual already_AddRefed<Promise> CancelSteps(JSContext* aCx,
                                                JS::Handle<JS::Value> aReason,
                                                ErrorResult& aRv);
  virtual void PullSteps(JSContext* aCx, ReadRequest* aReadRequest,
                         ErrorResult& aRv);

  // Internal Slot Accessors
  UnderlyingSourceCancelCallbackHelper* GetCancelAlgorithm() const {
    return mCancelAlgorithm;
  }
  void SetCancelAlgorithm(
      UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm) {
    mCancelAlgorithm = aCancelAlgorithm;
  }

  bool CloseRequested() const { return mCloseRequested; }
  void SetCloseRequested(bool aCloseRequested) {
    mCloseRequested = aCloseRequested;
  }

  bool PullAgain() const { return mPullAgain; }
  void SetPullAgain(bool aPullAgain) { mPullAgain = aPullAgain; }

  UnderlyingSourcePullCallbackHelper* GetPullAlgorithm() {
    return mPullAlgorithm;
  }
  void SetPullAlgorithm(UnderlyingSourcePullCallbackHelper* aPullAlgorithm) {
    mPullAlgorithm = aPullAlgorithm;
  }

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

  ReadableStream* GetStream() { return mStream; }
  void SetStream(ReadableStream* aStream);

 private:
  // Internal Slots: Public for ease of prototyping because
  // of the reams of static methods that access internal slots.
  RefPtr<UnderlyingSourceCancelCallbackHelper> mCancelAlgorithm;
  bool mCloseRequested = false;
  bool mPullAgain = false;
  RefPtr<UnderlyingSourcePullCallbackHelper> mPullAlgorithm;
  bool mPulling = false;
  QueueWithSizes mQueue = {};
  double mQueueTotalSize = 0.0;
  bool mStarted = false;
  double mStrategyHWM = false;
  RefPtr<QueuingStrategySize> mStrategySizeAlgorithm;
  RefPtr<ReadableStream> mStream;
};

extern void SetUpReadableStreamDefaultController(
    JSContext* aCx, ReadableStream* aStream,
    ReadableStreamDefaultController* aController,
    UnderlyingSourceStartCallbackHelper* aStartAlgorithm,
    UnderlyingSourcePullCallbackHelper* aPullAlgorithm,
    UnderlyingSourceCancelCallbackHelper* aCancelAlgorithm,
    double aHighWaterMark, QueuingStrategySize* aSizeAlgorithm,
    ErrorResult& aRv);

extern void SetupReadableStreamDefaultControllerFromUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream, JS::HandleObject aUnderlyingSource,
    UnderlyingSource& aUnderlyingSourceDict, double aHighWaterMark,
    QueuingStrategySize* aSizeAlgorithm, ErrorResult& aRv);

extern void ReadableStreamDefaultControllerEnqueue(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv);

extern void ReadableStreamDefaultControllerClose(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    ErrorResult& aRv);

extern void ReadableStreamDefaultReaderRead(JSContext* aCx,
                                            ReadableStreamDefaultReader* reader,
                                            ReadRequest* aRequest,
                                            ErrorResult& aRv);

extern void ReadableStreamDefaultControllerError(
    JSContext* aCx, ReadableStreamDefaultController* aController,
    JS::Handle<JS::Value> aValue, ErrorResult& aRv);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReadableStreamDefaultController_h
