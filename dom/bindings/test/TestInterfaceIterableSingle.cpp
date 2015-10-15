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

TestInterfaceIterableSingle::TestInterfaceIterableSingle(nsPIDOMWindow* aParent)
  : mParent(aParent)
{
  for(int i = 0; i < 3; ++i) {
    mValues.AppendElement(i);
  }
}

//static
already_AddRefed<TestInterfaceIterableSingle>
TestInterfaceIterableSingle::Constructor(const GlobalObject& aGlobal,
                                         ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<TestInterfaceIterableSingle> r = new TestInterfaceIterableSingle(window);
  return r.forget();
}

JSObject*
TestInterfaceIterableSingle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceIterableSingleBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindow*
TestInterfaceIterableSingle::GetParentObject() const
{
  return mParent;
}

size_t
TestInterfaceIterableSingle::GetIterableLength() const
{
  return mValues.Length();
}

uint32_t
TestInterfaceIterableSingle::GetKeyAtIndex(uint32_t index) const
{
  MOZ_ASSERT(index < mValues.Length());
  return mValues.ElementAt(index);
}

uint32_t
TestInterfaceIterableSingle::GetValueAtIndex(uint32_t index) const
{
  return GetKeyAtIndex(index);
}

} // namespace dom
} // namespace mozilla
