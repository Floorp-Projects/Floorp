/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestingDeprecatedInterface.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestingDeprecatedInterface, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestingDeprecatedInterface)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestingDeprecatedInterface)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestingDeprecatedInterface)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<TestingDeprecatedInterface>
TestingDeprecatedInterface::Constructor(const GlobalObject& aGlobal) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  RefPtr<TestingDeprecatedInterface> obj =
      new TestingDeprecatedInterface(global);
  return obj.forget();
}

TestingDeprecatedInterface::TestingDeprecatedInterface(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {}

TestingDeprecatedInterface::~TestingDeprecatedInterface() = default;

JSObject* TestingDeprecatedInterface::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestingDeprecatedInterface_Binding::Wrap(aCx, this, aGivenProto);
}

void TestingDeprecatedInterface::DeprecatedMethod() const {}

bool TestingDeprecatedInterface::DeprecatedAttribute() const { return true; }

}  // namespace dom
}  // namespace mozilla
