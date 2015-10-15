/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceSetlike.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceSetlike, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceSetlike)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceSetlike)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceSetlike)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceSetlike::TestInterfaceSetlike(JSContext* aCx,
                                           nsPIDOMWindow* aParent)
: mParent(aParent)
{
}

//static
already_AddRefed<TestInterfaceSetlike>
TestInterfaceSetlike::Constructor(const GlobalObject& aGlobal,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<TestInterfaceSetlike> r = new TestInterfaceSetlike(nullptr, window);
  return r.forget();
}

JSObject*
TestInterfaceSetlike::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceSetlikeBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindow*
TestInterfaceSetlike::GetParentObject() const
{
  return mParent;
}

} // namespace dom
} // namespace mozilla
