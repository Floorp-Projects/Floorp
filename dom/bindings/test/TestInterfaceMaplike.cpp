/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceMaplike.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceMaplike, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceMaplike)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceMaplike)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceMaplike)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceMaplike::TestInterfaceMaplike(nsPIDOMWindowInner* aParent)
: mParent(aParent)
{
}

//static
already_AddRefed<TestInterfaceMaplike>
TestInterfaceMaplike::Constructor(const GlobalObject& aGlobal,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceMaplike> r = new TestInterfaceMaplike(window);
  return r.forget();
}

JSObject*
TestInterfaceMaplike::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceMaplike_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
TestInterfaceMaplike::GetParentObject() const
{
  return mParent;
}

void
TestInterfaceMaplike::SetInternal(const nsAString& aKey, int32_t aValue)
{
  ErrorResult rv;
  TestInterfaceMaplike_Binding::MaplikeHelpers::Set(this, aKey, aValue, rv);
}

void
TestInterfaceMaplike::ClearInternal()
{
  ErrorResult rv;
  TestInterfaceMaplike_Binding::MaplikeHelpers::Clear(this, rv);
}

bool
TestInterfaceMaplike::DeleteInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplike_Binding::MaplikeHelpers::Delete(this, aKey, rv);
}

bool
TestInterfaceMaplike::HasInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplike_Binding::MaplikeHelpers::Has(this, aKey, rv);
}

} // namespace dom
} // namespace mozilla
