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

BrowsingContextGroup::BrowsingContextGroup() {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->HoldBrowsingContextGroup(this);
  }
}

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

  Subscribe(aProcess);

  nsTArray<BrowsingContext::IPCInitializer> inits(mContexts.Count());

  // First, perform a pre-order walk of our BrowsingContext objects from our
  // toplevels. This should visit every active BrowsingContext.
  for (auto& context : mToplevels) {
    MOZ_DIAGNOSTIC_ASSERT(!IsContextCached(context),
                          "cached contexts must have a parent");

    context->PreOrderWalk([&](BrowsingContext* aContext) {
      inits.AppendElement(aContext->GetIPCInitializer());
    });
  }

  // Ensure that cached BrowsingContext objects are also visited, by visiting
  // them after mToplevels.
  for (auto iter = mCachedContexts.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->PreOrderWalk([&](BrowsingContext* aContext) {
      inits.AppendElement(aContext->GetIPCInitializer());
    });
  }

  // We should have visited every browsing context.
  MOZ_DIAGNOSTIC_ASSERT(inits.Length() == mContexts.Count(),
                        "Visited the wrong number of contexts!");

  // Send all of our contexts to the target content process.
  Unused << aProcess->SendRegisterBrowsingContextGroup(inits);
}

bool BrowsingContextGroup::IsContextCached(BrowsingContext* aContext) const {
  MOZ_DIAGNOSTIC_ASSERT(aContext);
  return mCachedContexts.Contains(aContext);
}

void BrowsingContextGroup::CacheContext(BrowsingContext* aContext) {
  mCachedContexts.PutEntry(aContext);
}

void BrowsingContextGroup::CacheContexts(
    const BrowsingContext::Children& aContexts) {
  for (BrowsingContext* child : aContexts) {
    mCachedContexts.PutEntry(child);
  }
}

bool BrowsingContextGroup::EvictCachedContext(BrowsingContext* aContext) {
  return mCachedContexts.EnsureRemoved(aContext);
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
                                      mToplevels, mSubscribers, mCachedContexts)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContextGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContextGroup, Release)

}  // namespace dom
}  // namespace mozilla
