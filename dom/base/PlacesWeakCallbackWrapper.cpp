/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PlacesWeakCallbackWrapper.h"

#include "mozilla/dom/ContentProcessMessageManager.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(PlacesWeakCallbackWrapper,
                                               mParent, mCallback)

PlacesWeakCallbackWrapper::PlacesWeakCallbackWrapper(
    nsISupports* aParent, PlacesEventCallback& aCallback)
    : mParent(do_GetWeakReference(aParent)), mCallback(&aCallback) {}

already_AddRefed<PlacesWeakCallbackWrapper>
PlacesWeakCallbackWrapper::Constructor(const GlobalObject& aGlobal,
                                       PlacesEventCallback& aCallback) {
  nsCOMPtr<nsISupports> parent = aGlobal.GetAsSupports();
  RefPtr<PlacesWeakCallbackWrapper> wrapper =
      new PlacesWeakCallbackWrapper(parent, aCallback);
  return wrapper.forget();
}

PlacesWeakCallbackWrapper::~PlacesWeakCallbackWrapper() = default;

nsISupports* PlacesWeakCallbackWrapper::GetParentObject() const {
  nsCOMPtr<nsISupports> parent = do_QueryReferent(mParent);
  return parent;
}

JSObject* PlacesWeakCallbackWrapper::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PlacesWeakCallbackWrapper_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
