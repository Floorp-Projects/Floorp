/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/WindowContext.h"
#include "nsDOMMutationObserver.h"
#include "nsIDirectTaskDispatcher.h"
#include "nsIXULRuntime.h"
#include "nsProxyRelease.h"
#include "nsThread.h"
#if defined(XP_WIN)
#  include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#  include <unistd.h>  // for getpid()
#endif                 // defined(XP_WIN)

namespace mozilla::dom {

AutoTArray<RefPtr<DocGroup>, 2>* DocGroup::sPendingDocGroups = nullptr;

NS_IMPL_CYCLE_COLLECTION_CLASS(DocGroup)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DocGroup)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSignalSlotList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContextGroup)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DocGroup)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSignalSlotList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContextGroup)

  // If we still have any documents in this array, they were just unlinked, so
  // clear out our weak pointers to them.
  tmp->mDocuments.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

/* static */
already_AddRefed<DocGroup> DocGroup::Create(
    BrowsingContextGroup* aBrowsingContextGroup, const nsACString& aKey) {
  return do_AddRef(new DocGroup(aBrowsingContextGroup, aKey));
}

/* static */
nsresult DocGroup::GetKey(nsIPrincipal* aPrincipal, bool aCrossOriginIsolated,
                          nsACString& aKey) {
  // Use GetBaseDomain() to handle things like file URIs, IP address URIs,
  // etc. correctly.
  nsresult rv = aCrossOriginIsolated ? aPrincipal->GetOrigin(aKey)
                                     : aPrincipal->GetSiteOrigin(aKey);
  if (NS_FAILED(rv)) {
    aKey.Truncate();
  }

  return rv;
}

void DocGroup::SetExecutionManager(JSExecutionManager* aManager) {
  mExecutionManager = aManager;
}

mozilla::dom::CustomElementReactionsStack*
DocGroup::CustomElementReactionsStack() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mReactionsStack) {
    mReactionsStack = new mozilla::dom::CustomElementReactionsStack();
  }

  return mReactionsStack;
}

void DocGroup::AddDocument(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDocuments.Contains(aDocument));
  MOZ_ASSERT(mBrowsingContextGroup);
  // If the document is loaded as data it may not have a container, in which
  // case it can be difficult to determine the BrowsingContextGroup it's
  // associated with. XSLT can also add the document to the DocGroup before it
  // gets a container in some cases, in which case this will be asserted
  // elsewhere.
  MOZ_ASSERT_IF(
      aDocument->GetBrowsingContext(),
      aDocument->GetBrowsingContext()->Group() == mBrowsingContextGroup);
  mDocuments.AppendElement(aDocument);
}

void DocGroup::RemoveDocument(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);

  if (mDocuments.IsEmpty()) {
    mBrowsingContextGroup = nullptr;
  }
}

DocGroup::DocGroup(BrowsingContextGroup* aBrowsingContextGroup,
                   const nsACString& aKey)
    : mKey(aKey),
      mBrowsingContextGroup(aBrowsingContextGroup),
      mAgentClusterId(nsID::GenerateUUID()) {
  // This method does not add itself to
  // mBrowsingContextGroup->mDocGroups as the caller does it for us.
  MOZ_ASSERT(NS_IsMainThread());
  if (StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
    mArena = new mozilla::dom::DOMArena();
  }
}

DocGroup::~DocGroup() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mDocuments.IsEmpty());
}

void DocGroup::SignalSlotChange(HTMLSlotElement& aSlot) {
  MOZ_ASSERT(!mSignalSlotList.Contains(&aSlot));
  mSignalSlotList.AppendElement(&aSlot);

  if (!sPendingDocGroups) {
    // Queue a mutation observer compound microtask.
    nsDOMMutationObserver::QueueMutationObserverMicroTask();
    sPendingDocGroups = new AutoTArray<RefPtr<DocGroup>, 2>;
  }

  sPendingDocGroups->AppendElement(this);
}

nsTArray<RefPtr<HTMLSlotElement>> DocGroup::MoveSignalSlotList() {
  for (const RefPtr<HTMLSlotElement>& slot : mSignalSlotList) {
    slot->RemovedFromSignalSlotList();
  }
  return std::move(mSignalSlotList);
}

bool DocGroup::IsActive() const {
  for (Document* doc : mDocuments) {
    if (doc->IsCurrentActiveDocument()) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla::dom
