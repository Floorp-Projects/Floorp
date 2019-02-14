/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

StaticAutoPtr<nsTArray<RefPtr<BrowsingContextGroup>>>
    BrowsingContextGroup::sAllGroups;

/* static */ void BrowsingContextGroup::Init() {
  if (!sAllGroups) {
    sAllGroups = new nsTArray<RefPtr<BrowsingContextGroup>>();
    ClearOnShutdown(&sAllGroups);
  }
}

bool BrowsingContextGroup::Contains(BrowsingContext* aBrowsingContext) {
  return aBrowsingContext->Group() == this;
}

void BrowsingContextGroup::Register(BrowsingContext* aBrowsingContext) {
  mContexts.PutEntry(aBrowsingContext);
}

void BrowsingContextGroup::Unregister(BrowsingContext* aBrowsingContext) {
  mContexts.RemoveEntry(aBrowsingContext);
}

BrowsingContextGroup::BrowsingContextGroup() {
  sAllGroups->AppendElement(this);
}

BrowsingContextGroup::~BrowsingContextGroup() {
  if (sAllGroups) {
    sAllGroups->RemoveElement(this);
  }
}

nsISupports* BrowsingContextGroup::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContextGroup::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContextGroup_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowsingContextGroup)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowsingContextGroup)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContexts)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mToplevels)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContextGroup)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContexts)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mToplevels)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BrowsingContextGroup)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContextGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContextGroup, Release)

}  // namespace dom
}  // namespace mozilla
