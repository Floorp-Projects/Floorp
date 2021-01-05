/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceMaplikeJSObject.h"
#include "mozilla/dom/TestInterfaceMaplike.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceMaplikeJSObject, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceMaplikeJSObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceMaplikeJSObject)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceMaplikeJSObject)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceMaplikeJSObject::TestInterfaceMaplikeJSObject(
    nsPIDOMWindowInner* aParent)
    : mParent(aParent) {}

// static
already_AddRefed<TestInterfaceMaplikeJSObject>
TestInterfaceMaplikeJSObject::Constructor(const GlobalObject& aGlobal,
                                          ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceMaplikeJSObject> r =
      new TestInterfaceMaplikeJSObject(window);
  return r.forget();
}

JSObject* TestInterfaceMaplikeJSObject::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceMaplikeJSObject_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* TestInterfaceMaplikeJSObject::GetParentObject() const {
  return mParent;
}

void TestInterfaceMaplikeJSObject::SetInternal(JSContext* aCx,
                                               const nsAString& aKey,
                                               JS::Handle<JSObject*> aObject) {
  ErrorResult rv;
  TestInterfaceMaplikeJSObject_Binding::MaplikeHelpers::Set(this, aKey, aObject,
                                                            rv);
}

void TestInterfaceMaplikeJSObject::ClearInternal() {
  ErrorResult rv;
  TestInterfaceMaplikeJSObject_Binding::MaplikeHelpers::Clear(this, rv);
}

bool TestInterfaceMaplikeJSObject::DeleteInternal(const nsAString& aKey) {
  ErrorResult rv;
  return TestInterfaceMaplikeJSObject_Binding::MaplikeHelpers::Delete(this,
                                                                      aKey, rv);
}

bool TestInterfaceMaplikeJSObject::HasInternal(const nsAString& aKey) {
  ErrorResult rv;
  return TestInterfaceMaplikeJSObject_Binding::MaplikeHelpers::Has(this, aKey,
                                                                   rv);
}

void TestInterfaceMaplikeJSObject::GetInternal(
    JSContext* aCx, const nsAString& aKey, JS::MutableHandle<JSObject*> aRetVal,
    ErrorResult& aRv) {
  TestInterfaceMaplikeJSObject_Binding::MaplikeHelpers::Get(this, aCx, aKey,
                                                            aRetVal, aRv);
}
}  // namespace mozilla::dom
