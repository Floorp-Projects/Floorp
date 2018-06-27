/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceMaplikeObject.h"
#include "mozilla/dom/TestInterfaceMaplike.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceMaplikeObject, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceMaplikeObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceMaplikeObject)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceMaplikeObject)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceMaplikeObject::TestInterfaceMaplikeObject(nsPIDOMWindowInner* aParent)
: mParent(aParent)
{
}

//static
already_AddRefed<TestInterfaceMaplikeObject>
TestInterfaceMaplikeObject::Constructor(const GlobalObject& aGlobal,
                                        ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceMaplikeObject> r =
    new TestInterfaceMaplikeObject(window);
  return r.forget();
}

JSObject*
TestInterfaceMaplikeObject::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceMaplikeObject_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
TestInterfaceMaplikeObject::GetParentObject() const
{
  return mParent;
}

void
TestInterfaceMaplikeObject::SetInternal(const nsAString& aKey)
{
  RefPtr<TestInterfaceMaplike> p(new TestInterfaceMaplike(mParent));
  ErrorResult rv;
  TestInterfaceMaplikeObject_Binding::MaplikeHelpers::Set(this, aKey, *p, rv);
}

void
TestInterfaceMaplikeObject::ClearInternal()
{
  ErrorResult rv;
  TestInterfaceMaplikeObject_Binding::MaplikeHelpers::Clear(this, rv);
}

bool
TestInterfaceMaplikeObject::DeleteInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplikeObject_Binding::MaplikeHelpers::Delete(this, aKey, rv);
}

bool
TestInterfaceMaplikeObject::HasInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplikeObject_Binding::MaplikeHelpers::Has(this, aKey, rv);
}

} // namespace dom
} // namespace mozilla
