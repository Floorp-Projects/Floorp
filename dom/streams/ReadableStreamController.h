/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamController_h
#define mozilla_dom_ReadableStreamController_h

#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "UnderlyingSourceCallbackHelpers.h"

namespace mozilla::dom {
struct ReadRequest;
class ReadableStream;
class ReadableStreamDefaultController;
class ReadableByteStreamController;

class ReadableStreamController : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ReadableStreamController)

  ReadableStreamController(nsIGlobalObject* aGlobal);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  virtual bool IsDefault() = 0;
  virtual bool IsByte() = 0;
  virtual ReadableStreamDefaultController* AsDefault() = 0;
  virtual ReadableByteStreamController* AsByte() = 0;

  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<Promise> CancelSteps(JSContext* aCx,
                                                JS::Handle<JS::Value> aReason,
                                                ErrorResult& aRv) = 0;
  MOZ_CAN_RUN_SCRIPT
  virtual void PullSteps(JSContext* aCx, ReadRequest* aReadRequest,
                         ErrorResult& aRv) = 0;

  // No JS implementable UnderlyingSource callback exists for this.
  virtual void ReleaseSteps() = 0;

  UnderlyingSourceAlgorithmsBase* GetAlgorithms() const { return mAlgorithms; }
  void SetAlgorithms(UnderlyingSourceAlgorithmsBase& aAlgorithms) {
    mAlgorithms = &aAlgorithms;
  }
  void ClearAlgorithms() {
    MOZ_ASSERT(mAlgorithms);
    mAlgorithms->ReleaseObjects();
    mAlgorithms = nullptr;
  }

  ReadableStream* Stream() const { return mStream; }
  void SetStream(ReadableStream* aStream);

 protected:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  // The algorithms for the underlying source
  RefPtr<UnderlyingSourceAlgorithmsBase> mAlgorithms;

  RefPtr<ReadableStream> mStream;

  virtual ~ReadableStreamController() = default;
};

}  // namespace mozilla::dom

#endif
