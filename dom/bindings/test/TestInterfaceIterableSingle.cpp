/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceIterableSingle.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceIterableSingle, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceIterableSingle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceIterableSingle)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceIterableSingle)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceIterableSingle::TestInterfaceIterableSingle(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{
  for (int i = 0; i < 3; ++i) {
    mValues.AppendElement(i);
  }
}

//static
already_AddRefed<TestInterfaceIterableSingle>
TestInterfaceIterableSingle::Constructor(const GlobalObject& aGlobal,
                                         ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceIterableSingle> r = new TestInterfaceIterableSingle(window);
  return r.forget();
}

JSObject*
TestInterfaceIterableSingle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceIterableSingle_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
TestInterfaceIterableSingle::GetParentObject() const
{
  return mParent;
}

uint32_t
TestInterfaceIterableSingle::Length() const
{
  return mValues.Length();
}

int32_t
TestInterfaceIterableSingle::IndexedGetter(uint32_t aIndex, bool& aFound) const
{
  if (aIndex >= mValues.Length()) {
    aFound = false;
    return 0;
  }

  aFound = true;
  return mValues[aIndex];
}

} // namespace dom
} // namespace mozilla
