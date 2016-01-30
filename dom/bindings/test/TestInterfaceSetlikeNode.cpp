/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceSetlikeNode.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceSetlikeNode, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceSetlikeNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceSetlikeNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceSetlikeNode)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceSetlikeNode::TestInterfaceSetlikeNode(JSContext* aCx,
                                                   nsPIDOMWindowInner* aParent)
: mParent(aParent)
{
}

//static
already_AddRefed<TestInterfaceSetlikeNode>
TestInterfaceSetlikeNode::Constructor(const GlobalObject& aGlobal,
                                      ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceSetlikeNode> r = new TestInterfaceSetlikeNode(nullptr, window);
  return r.forget();
}

JSObject*
TestInterfaceSetlikeNode::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return TestInterfaceSetlikeNodeBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
TestInterfaceSetlikeNode::GetParentObject() const
{
  return mParent;
}

} // namespace dom
} // namespace mozilla
