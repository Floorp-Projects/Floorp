/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceMaplike.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeBinding.h"
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

TestInterfaceMaplike::TestInterfaceMaplike(nsPIDOMWindow* aParent)
: mParent(aParent)
{
}

//static
already_AddRefed<TestInterfaceMaplike>
TestInterfaceMaplike::Constructor(const GlobalObject& aGlobal,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<TestInterfaceMaplike> r = new TestInterfaceMaplike(window);
  return r.forget();
}

JSObject*
TestInterfaceMaplike::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceMaplikeBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindow*
TestInterfaceMaplike::GetParentObject() const
{
  return mParent;
}

void
TestInterfaceMaplike::SetInternal(const nsAString& aKey, int32_t aValue)
{
  ErrorResult rv;
  TestInterfaceMaplikeBinding::MaplikeHelpers::Set(this, aKey, aValue, rv);
}

void
TestInterfaceMaplike::ClearInternal()
{
  ErrorResult rv;
  TestInterfaceMaplikeBinding::MaplikeHelpers::Clear(this, rv);
}

bool
TestInterfaceMaplike::DeleteInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplikeBinding::MaplikeHelpers::Delete(this, aKey, rv);
}

bool
TestInterfaceMaplike::HasInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplikeBinding::MaplikeHelpers::Has(this, aKey, rv);
}

} // namespace dom
} // namespace mozilla
