/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CountQueuingStrategy.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CountQueuingStrategy, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CountQueuingStrategy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CountQueuingStrategy)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CountQueuingStrategy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<CountQueuingStrategy> CountQueuingStrategy::Constructor(
    const GlobalObject& aGlobal, const QueuingStrategyInit& aInit) {
  RefPtr<CountQueuingStrategy> strategy =
      new CountQueuingStrategy(aGlobal.GetAsSupports(), aInit.mHighWaterMark);
  return strategy.forget();
}

nsIGlobalObject* CountQueuingStrategy::GetParentObject() const {
  return mGlobal;
}

JSObject* CountQueuingStrategy::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return CountQueuingStrategy_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
