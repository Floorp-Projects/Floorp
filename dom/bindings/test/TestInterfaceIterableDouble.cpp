/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceIterableDouble.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceIterableDouble, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceIterableDouble)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceIterableDouble)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceIterableDouble)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceIterableDouble::TestInterfaceIterableDouble(
    nsPIDOMWindowInner* aParent)
    : mParent(aParent) {
  mValues.AppendElement(std::pair<nsString, nsString>(u"a"_ns, u"b"_ns));
  mValues.AppendElement(std::pair<nsString, nsString>(u"c"_ns, u"d"_ns));
  mValues.AppendElement(std::pair<nsString, nsString>(u"e"_ns, u"f"_ns));
}

// static
already_AddRefed<TestInterfaceIterableDouble>
TestInterfaceIterableDouble::Constructor(const GlobalObject& aGlobal,
                                         ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceIterableDouble> r =
      new TestInterfaceIterableDouble(window);
  return r.forget();
}

JSObject* TestInterfaceIterableDouble::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceIterableDouble_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* TestInterfaceIterableDouble::GetParentObject() const {
  return mParent;
}

size_t TestInterfaceIterableDouble::GetIterableLength() {
  return mValues.Length();
}

nsAString& TestInterfaceIterableDouble::GetKeyAtIndex(uint32_t aIndex) {
  MOZ_ASSERT(aIndex < mValues.Length());
  return mValues.ElementAt(aIndex).first;
}

nsAString& TestInterfaceIterableDouble::GetValueAtIndex(uint32_t aIndex) {
  MOZ_ASSERT(aIndex < mValues.Length());
  return mValues.ElementAt(aIndex).second;
}

}  // namespace mozilla::dom
