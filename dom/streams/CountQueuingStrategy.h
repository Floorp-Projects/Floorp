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
#include "mozilla/dom/BaseQueuingStrategy.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

class CountQueuingStrategy final : public BaseQueuingStrategy,
                                   public nsWrapperCache {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CountQueuingStrategy,
                                                         BaseQueuingStrategy)

 public:
  explicit CountQueuingStrategy(nsISupports* aGlobal, double aHighWaterMark)
      : BaseQueuingStrategy(aGlobal, aHighWaterMark) {}

 protected:
  ~CountQueuingStrategy() override = default;

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<CountQueuingStrategy> Constructor(
      const GlobalObject& aGlobal, const QueuingStrategyInit& aInit);

  already_AddRefed<Function> GetSize(ErrorResult& aRv);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CountQueuingStrategy_h
