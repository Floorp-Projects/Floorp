/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsFocusManager.h"

namespace mozilla {
namespace dom {

static StaticRefPtr<BrowsingContextGroup> sChromeGroup;

static StaticAutoPtr<
    nsDataHashtable<nsUint64HashKey, RefPtr<BrowsingContextGroup>>>
    sBrowsingContextGroups;

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::GetOrCreate(
    uint64_t aId) {
  if (!sBrowsingContextGroups) {
    sBrowsingContextGroups =
        new nsDataHashtable<nsUint64HashKey, RefPtr<BrowsingContextGroup>>();
    ClearOnShutdown(&sBrowsingContextGroups);
  }

  auto entry = sBrowsingContextGroups->LookupForAdd(aId);
  RefPtr<BrowsingContextGroup> group =
      entry.OrInsert([&] { return do_AddRef(new BrowsingContextGroup(aId)); });
  return group.forget();
}

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::Create() {
  return GetOrCreate(nsContentUtils::GenerateBrowsingContextId());
}

BrowsingContextGroup::BrowsingContextGroup(uint64_t aId) : mId(aId) {
  mTimerEventQueue = ThrottledEventQueue::Create(
      GetMainThreadSerialEventTarget(), "BrowsingContextGroup timer queue");

  mWorkerEventQueue = ThrottledEventQueue::Create(
      GetMainThreadSerialEventTarget(), "BrowsingContextGroup worker queue");
}

bool BrowsingContextGroup::Contains(BrowsingContext* aBrowsingContext) {
  return aBrowsingContext->Group() == this;
}

void BrowsingContextGroup::Register(BrowsingContext* aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);
  MOZ_DIAGNOSTIC_ASSERT(this == sChromeGroup ? aBrowsingContext->IsChrome()
                                             : aBrowsingContext->IsContent(),
                        "Only chrome BCs may exist in the chrome group, and "
                        "only content BCs may exist in other groups");
  mContexts.PutEntry(aBrowsingContext);
}

void BrowsingContextGroup::Unregister(BrowsingContext* aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);
  mContexts.RemoveEntry(aBrowsingContext);

  if (mContexts.IsEmpty()) {
    // There are no browsing context still referencing this group. We can clear
    // all subscribers.
    UnsubscribeAllContentParents();

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

  // If this origin process still embeds any non-discarded BrowsingContexts in
  // this BrowsingContextGroup, make sure to discard them, as this process is
  // going away.
  nsTArray<RefPtr<BrowsingContext>> toDiscard;
  for (auto& context : mContexts) {
    if (context.GetKey()->Canonical()->IsEmbeddedInProcess(
            aOriginProcess->ChildID())) {
      toDiscard.AppendElement(context.GetKey());
    }
  }
  for (auto& context : toDiscard) {
    context->Detach(/* aFromIPC */ true);
  }
}

static void CollectContextInitializers(
    Span<RefPtr<BrowsingContext>> aContexts,
    nsTArray<SyncedContextInitializer>& aInits) {
  // The order that we record these initializers is important, as it will keep
  // the order that children are attached to their parent in the newly connected
  // content process consistent.
  for (auto& context : aContexts) {
    aInits.AppendElement(context->GetIPCInitializer());
    for (auto& window : context->GetWindowContexts()) {
      aInits.AppendElement(window->GetIPCInitializer());
      CollectContextInitializers(window->Children(), aInits);
    }
  }
}

void BrowsingContextGroup::EnsureSubscribed(ContentParent* aProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aProcess);
  if (mSubscribers.Contains(aProcess)) {
    return;
  }

  Subscribe(aProcess);

  // FIXME: This won't send non-discarded children of discarded BCs, but those
  // BCs will be in the process of being destroyed anyway.
  // FIXME: Prevent that situation from occuring.
  nsTArray<SyncedContextInitializer> inits(mContexts.Count() * 2);
  CollectContextInitializers(mToplevels, inits);

  // Send all of our contexts to the target content process.
  Unused << aProcess->SendRegisterBrowsingContextGroup(Id(), inits);

  // If the focused or active BrowsingContexts belong in this group, tell the
  // newly subscribed process.
  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    BrowsingContext* focused = fm->GetFocusedBrowsingContextInChrome();
    if (focused && focused->Group() != this) {
      focused = nullptr;
    }
    BrowsingContext* active = fm->GetActiveBrowsingContextInChrome();
    if (active && active->Group() != this) {
      active = nullptr;
    }

    if (focused || active) {
      Unused << aProcess->SendSetupFocusedAndActive(focused, active);
    }
  }
}

BrowsingContextGroup::~BrowsingContextGroup() {
  UnsubscribeAllContentParents();
}

void BrowsingContextGroup::UnsubscribeAllContentParents() {
  if (sBrowsingContextGroups) {
    sBrowsingContextGroups->Remove(Id());
  }

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

/* static */
BrowsingContextGroup* BrowsingContextGroup::GetChromeGroup() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  if (!sChromeGroup && XRE_IsParentProcess()) {
    sChromeGroup = BrowsingContextGroup::Create();
    ClearOnShutdown(&sChromeGroup);
  }

  return sChromeGroup;
}

void BrowsingContextGroup::GetDocGroups(nsTArray<DocGroup*>& aDocGroups) {
  MOZ_ASSERT(NS_IsMainThread());
  for (auto iter = mDocGroups.ConstIter(); !iter.Done(); iter.Next()) {
    aDocGroups.AppendElement(iter.Data());
  }
}

already_AddRefed<DocGroup> BrowsingContextGroup::AddDocument(
    const nsACString& aKey, Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<DocGroup>& docGroup = mDocGroups.GetOrInsert(aKey);
  if (!docGroup) {
    docGroup = DocGroup::Create(this, aKey);
  }

  docGroup->AddDocument(aDocument);
  return do_AddRef(docGroup);
}

void BrowsingContextGroup::RemoveDocument(const nsACString& aKey,
                                          Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<DocGroup> docGroup = aDocument->GetDocGroup();
  // Removing the last document in DocGroup might decrement the
  // DocGroup BrowsingContextGroup's refcount to 0.
  RefPtr<BrowsingContextGroup> kungFuDeathGrip(this);
  docGroup->RemoveDocument(aDocument);

  if (docGroup->IsEmpty()) {
    mDocGroups.Remove(aKey);
  }
}

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::Select(
    WindowContext* aParent, BrowsingContext* aOpener) {
  if (aParent) {
    return do_AddRef(aParent->Group());
  }
  if (aOpener) {
    return do_AddRef(aOpener->Group());
  }
  return Create();
}

void BrowsingContextGroup::GetAllGroups(
    nsTArray<RefPtr<BrowsingContextGroup>>& aGroups) {
  aGroups.Clear();
  if (!sBrowsingContextGroups) {
    return;
  }

  aGroups.SetCapacity(sBrowsingContextGroups->Count());
  for (auto& group : *sBrowsingContextGroups) {
    aGroups.AppendElement(group.GetData());
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BrowsingContextGroup, mContexts,
                                      mToplevels, mSubscribers,
                                      mTimerEventQueue, mWorkerEventQueue)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContextGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContextGroup, Release)

}  // namespace dom
}  // namespace mozilla
