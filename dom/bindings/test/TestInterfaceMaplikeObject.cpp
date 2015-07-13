/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceMaplikeObject.h"
#include "mozilla/dom/TestInterfaceMaplike.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeBinding.h"
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

TestInterfaceMaplikeObject::TestInterfaceMaplikeObject(nsPIDOMWindow* aParent)
: mParent(aParent)
{
}

//static
already_AddRefed<TestInterfaceMaplikeObject>
TestInterfaceMaplikeObject::Constructor(const GlobalObject& aGlobal,
                                        ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<TestInterfaceMaplikeObject> r =
    new TestInterfaceMaplikeObject(window);
  return r.forget();
}

JSObject*
TestInterfaceMaplikeObject::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceMaplikeObjectBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindow*
TestInterfaceMaplikeObject::GetParentObject() const
{
  return mParent;
}

void
TestInterfaceMaplikeObject::SetInternal(const nsAString& aKey)
{
  nsRefPtr<TestInterfaceMaplike> p(new TestInterfaceMaplike(mParent));
  ErrorResult rv;
  TestInterfaceMaplikeObjectBinding::MaplikeHelpers::Set(this, aKey, *p, rv);
}

void
TestInterfaceMaplikeObject::ClearInternal()
{
  ErrorResult rv;
  TestInterfaceMaplikeObjectBinding::MaplikeHelpers::Clear(this, rv);
}

bool
TestInterfaceMaplikeObject::DeleteInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplikeObjectBinding::MaplikeHelpers::Delete(this, aKey, rv);
}

bool
TestInterfaceMaplikeObject::HasInternal(const nsAString& aKey)
{
  ErrorResult rv;
  return TestInterfaceMaplikeObjectBinding::MaplikeHelpers::Has(this, aKey, rv);
}

} // namespace dom
} // namespace mozilla
