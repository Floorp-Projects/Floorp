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

bool BrowsingContextGroup::Contains(BrowsingContext* aBrowsingContext) {
  return aBrowsingContext->Group() == this;
}

void BrowsingContextGroup::Register(BrowsingContext* aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);
  mContexts.PutEntry(aBrowsingContext);
}

void BrowsingContextGroup::Unregister(BrowsingContext* aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);
  mContexts.RemoveEntry(aBrowsingContext);
}

void BrowsingContextGroup::Subscribe(ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aOriginProcess);
  mSubscribers.PutEntry(aOriginProcess);
  aOriginProcess->OnBrowsingContextGroupSubscribe(this);
}

void BrowsingContextGroup::Unsubscribe(ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aOriginProcess);
  mSubscribers.RemoveEntry(aOriginProcess);
  aOriginProcess->OnBrowsingContextGroupUnsubscribe(this);
}

void BrowsingContextGroup::EnsureSubscribed(ContentParent* aProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aProcess);
  if (mSubscribers.Contains(aProcess)) {
    return;
  }

  MOZ_RELEASE_ASSERT(
      mContexts.Count() == 1,
      "EnsureSubscribed doesn't work on non-fresh BrowsingContextGroups yet!");

  // Subscribe to the BrowsingContext, and send down initial state!
  Subscribe(aProcess);

  // XXX(nika): We can't send down existing BrowsingContextGroups reliably yet
  // due to ordering issues! (Bug ?)
  for (auto iter = mContexts.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<BrowsingContext> bc = iter.Get()->GetKey();
    Unused << aProcess->SendAttachBrowsingContext(
        bc->GetParent(), bc->GetOpener(), BrowsingContextId(bc->Id()),
        bc->Name());
  }
}

BrowsingContextGroup::~BrowsingContextGroup() {
  for (auto iter = mSubscribers.Iter(); !iter.Done(); iter.Next()) {
    nsRefPtrHashKey<ContentParent>* entry = iter.Get();
    entry->GetKey()->OnBrowsingContextGroupUnsubscribe(this);
  }
}

nsISupports* BrowsingContextGroup::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContextGroup::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContextGroup_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BrowsingContextGroup, mContexts,
                                      mToplevels, mSubscribers)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContextGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContextGroup, Release)

}  // namespace dom
}  // namespace mozilla
