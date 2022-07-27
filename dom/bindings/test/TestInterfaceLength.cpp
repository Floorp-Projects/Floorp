/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceLength.h"
#include "mozilla/dom/TestFunctionsBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(TestInterfaceLength, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(TestInterfaceLength, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(TestInterfaceLength)

JSObject* TestInterfaceLength::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceLength_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<TestInterfaceLength> TestInterfaceLength::Constructor(
    const GlobalObject& aGlobalObject, const bool aArg) {
  return MakeAndAddRef<TestInterfaceLength>();
}

}  // namespace mozilla::dom
