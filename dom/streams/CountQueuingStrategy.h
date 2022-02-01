/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CountQueuingStrategy_h
#define mozilla_dom_CountQueuingStrategy_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

class Function;

}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace dom {

class CountQueuingStrategy final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CountQueuingStrategy)

 public:
  explicit CountQueuingStrategy(nsISupports* aGlobal, double aHighWaterMark)
      : mGlobal(do_QueryInterface(aGlobal)), mHighWaterMark(aHighWaterMark) {}

 protected:
  ~CountQueuingStrategy() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;

 public:
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<CountQueuingStrategy> Constructor(
      const GlobalObject& aGlobal, const QueuingStrategyInit& aInit);

  double HighWaterMark() const { return mHighWaterMark; }

  already_AddRefed<Function> GetSize(ErrorResult& aRv);

 private:
  double mHighWaterMark = 0.0;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CountQueuingStrategy_h
