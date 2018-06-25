/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceIterableDoubleUnion.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceIterableDoubleUnion, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceIterableDoubleUnion)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceIterableDoubleUnion)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceIterableDoubleUnion)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceIterableDoubleUnion::TestInterfaceIterableDoubleUnion(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{
  OwningStringOrLong a;
  a.SetAsLong() = 1;
  mValues.AppendElement(std::pair<nsString, OwningStringOrLong>(NS_LITERAL_STRING("long"),
                                                                a));
  a.SetAsString() = NS_LITERAL_STRING("a");
  mValues.AppendElement(std::pair<nsString, OwningStringOrLong>(NS_LITERAL_STRING("string"),
                                                                a));
}

//static
already_AddRefed<TestInterfaceIterableDoubleUnion>
TestInterfaceIterableDoubleUnion::Constructor(const GlobalObject& aGlobal,
                                              ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceIterableDoubleUnion> r = new TestInterfaceIterableDoubleUnion(window);
  return r.forget();
}

JSObject*
TestInterfaceIterableDoubleUnion::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceIterableDoubleUnion_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
TestInterfaceIterableDoubleUnion::GetParentObject() const
{
  return mParent;
}

size_t
TestInterfaceIterableDoubleUnion::GetIterableLength()
{
  return mValues.Length();
}

nsAString&
TestInterfaceIterableDoubleUnion::GetKeyAtIndex(uint32_t aIndex)
{
  MOZ_ASSERT(aIndex < mValues.Length());
  return mValues.ElementAt(aIndex).first;
}

OwningStringOrLong&
TestInterfaceIterableDoubleUnion::GetValueAtIndex(uint32_t aIndex)
{
  MOZ_ASSERT(aIndex < mValues.Length());
  return mValues.ElementAt(aIndex).second;
}

} // namespace dom
} // namespace mozilla
