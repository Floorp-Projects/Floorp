/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsFocusManager.h"

namespace mozilla {
namespace dom {

BrowsingContextGroup::BrowsingContextGroup() {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->HoldBrowsingContextGroup(this);
  } else {
    ContentParent::HoldBrowsingContextGroup(this);
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

  if (mContexts.IsEmpty()) {
    // There are no browsing context still referencing this group. We can clear
    // all subscribers.
    UnsubscribeAllContentParents();
    if (XRE_IsContentProcess()) {
      ContentChild::GetSingleton()->ReleaseBrowsingContextGroup(this);
    } else {
      ContentParent::ReleaseBrowsingContextGroup(this);
    }
    // We may have been deleted here as the ContentChild/Parent may
    // have held the last references to `this`.
    // Do not access any members at this point.
  }
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

  bool sendFocused = false;
  bool sendActive = false;
  BrowsingContext* focused = nullptr;
  BrowsingContext* active = nullptr;
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    focused = fm->GetFocusedBrowsingContextInChrome();
    active = fm->GetActiveBrowsingContextInChrome();
  }

  nsTArray<BrowsingContext::IPCInitializer> inits(mContexts.Count());
  nsTArray<WindowContext::IPCInitializer> windowInits(mContexts.Count());

  auto addInits = [&](BrowsingContext* aContext) {
    inits.AppendElement(aContext->GetIPCInitializer());
    if (focused == aContext) {
      sendFocused = true;
    }
    if (active == aContext) {
      sendActive = true;
    }
    for (auto& window : aContext->GetWindowContexts()) {
      windowInits.AppendElement(window->GetIPCInitializer());
    }
  };

  // First, perform a pre-order walk of our BrowsingContext objects from our
  // toplevels. This should visit every active BrowsingContext.
  for (auto& context : mToplevels) {
    MOZ_DIAGNOSTIC_ASSERT(!IsContextCached(context),
                          "cached contexts must have a parent");
    context->PreOrderWalk(addInits);
  }

  // Ensure that cached BrowsingContext objects are also visited, by visiting
  // them after mToplevels.
  for (auto iter = mCachedContexts.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->PreOrderWalk(addInits);
  }

  // We should have visited every browsing context.
  MOZ_DIAGNOSTIC_ASSERT(inits.Length() == mContexts.Count(),
                        "Visited the wrong number of contexts!");

  // Send all of our contexts to the target content process.
  Unused << aProcess->SendRegisterBrowsingContextGroup(inits, windowInits);

  if (sendActive || sendFocused) {
    Unused << aProcess->SendSetupFocusedAndActive(
        sendFocused ? focused : nullptr, sendActive ? active : nullptr);
  }
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
  UnsubscribeAllContentParents();
}

void BrowsingContextGroup::UnsubscribeAllContentParents() {
  for (auto iter = mSubscribers.Iter(); !iter.Done(); iter.Next()) {
    nsRefPtrHashKey<ContentParent>* entry = iter.Get();
    entry->GetKey()->OnBrowsingContextGroupUnsubscribe(this);
  }
  mSubscribers.Clear();
}

nsISupports* BrowsingContextGroup::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContextGroup::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContextGroup_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult BrowsingContextGroup::QueuePostMessageEvent(
    already_AddRefed<nsIRunnable>&& aRunnable) {
  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled()) {
    if (!mPostMessageEventQueue) {
      nsCOMPtr<nsISerialEventTarget> target = GetMainThreadSerialEventTarget();
      mPostMessageEventQueue = ThrottledEventQueue::Create(
          target, "PostMessage Queue",
          nsIRunnablePriority::PRIORITY_DEFERRED_TIMERS);
      nsresult rv = mPostMessageEventQueue->SetIsPaused(false);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    // Ensure the queue is enabled
    if (mPostMessageEventQueue->IsPaused()) {
      nsresult rv = mPostMessageEventQueue->SetIsPaused(false);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    if (mPostMessageEventQueue) {
      mPostMessageEventQueue->Dispatch(std::move(aRunnable),
                                       NS_DISPATCH_NORMAL);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

void BrowsingContextGroup::FlushPostMessageEvents() {
  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled()) {
    if (mPostMessageEventQueue) {
      nsresult rv = mPostMessageEventQueue->SetIsPaused(true);
      MOZ_ALWAYS_SUCCEEDS(rv);
      nsCOMPtr<nsIRunnable> event;
      while ((event = mPostMessageEventQueue->GetEvent())) {
        NS_DispatchToMainThread(event.forget());
      }
    }
  }
}

static StaticRefPtr<BrowsingContextGroup> sChromeGroup;

/* static */
BrowsingContextGroup* BrowsingContextGroup::GetChromeGroup() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  if (!sChromeGroup && XRE_IsParentProcess()) {
    sChromeGroup = new BrowsingContextGroup();
    ClearOnShutdown(&sChromeGroup);
  }

  return sChromeGroup;
}

void BrowsingContextGroup::GetDocGroups(nsTArray<DocGroup*>& aDocGroups) {
  nsTHashtable<nsRefPtrHashKey<DocGroup>> docGroups;
  for (auto& toplevel : mToplevels) {
    toplevel->PreOrderWalk([&](BrowsingContext* aContext) {
      if (nsDocShell* docShell = nsDocShell::Cast(aContext->GetDocShell())) {
        if (!docShell->HasContentViewer()) {
          return;
        }
        if (Document* document = docShell->GetDocument()) {
          docGroups.PutEntry(document->GetDocGroup());
        }
      }
    });
  }

  for (auto iter = docGroups.ConstIter(); !iter.Done(); iter.Next()) {
    aDocGroups.AppendElement(iter.Get()->GetKey());
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BrowsingContextGroup, mContexts,
                                      mToplevels, mSubscribers, mCachedContexts)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContextGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContextGroup, Release)

}  // namespace dom
}  // namespace mozilla
