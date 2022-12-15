/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WrapperCachedNonISupportsTestInterface.h"
#include "mozilla/dom/TestFunctionsBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WrapperCachedNonISupportsTestInterface)

JSObject* WrapperCachedNonISupportsTestInterface::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WrapperCachedNonISupportsTestInterface_Binding::Wrap(aCx, this,
                                                              aGivenProto);
}

already_AddRefed<WrapperCachedNonISupportsTestInterface>
WrapperCachedNonISupportsTestInterface::Constructor(
    const GlobalObject& aGlobalObject) {
  RefPtr<WrapperCachedNonISupportsTestInterface> result =
      new WrapperCachedNonISupportsTestInterface();
  return result.forget();
}

}  // namespace mozilla::dom
