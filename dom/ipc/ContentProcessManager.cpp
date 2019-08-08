/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessManager.h"
#include "ContentParent.h"
#include "mozilla/dom/BrowserParent.h"

#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"

#include "nsPrintfCString.h"
#include "nsIScriptSecurityManager.h"

// XXX need another bug to move this to a common header.
#ifdef DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

namespace mozilla {
namespace dom {

/* static */
StaticAutoPtr<ContentProcessManager> ContentProcessManager::sSingleton;

/* static */
ContentProcessManager* ContentProcessManager::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!sSingleton) {
    sSingleton = new ContentProcessManager();
    ClearOnShutdown(&sSingleton);
  }
  return sSingleton;
}

void ContentProcessManager::AddContentProcess(ContentParent* aChildCp) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChildCp);

  auto entry = mContentParentMap.LookupForAdd(aChildCp->ChildID());
  MOZ_ASSERT_IF(entry, entry.Data() == aChildCp);
  entry.OrInsert([&] { return aChildCp; });
}

void ContentProcessManager::RemoveContentProcess(
    const ContentParentId& aChildCpId) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ALWAYS_TRUE(mContentParentMap.Remove(aChildCpId));
}

ContentParent* ContentProcessManager::GetContentProcessById(
    const ContentParentId& aChildCpId) {
  MOZ_ASSERT(NS_IsMainThread());

  ContentParent* contentParent = mContentParentMap.Get(aChildCpId);
  if (NS_WARN_IF(!contentParent)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }
  return contentParent;
}

bool ContentProcessManager::RegisterRemoteFrame(BrowserParent* aChildBp) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChildBp);

  auto entry = mBrowserParentMap.LookupForAdd(aChildBp->GetTabId());
  MOZ_ASSERT_IF(entry, entry.Data() == aChildBp);
  entry.OrInsert([&] { return aChildBp; });
  return !entry;
}

void ContentProcessManager::UnregisterRemoteFrame(const TabId& aChildTabId) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ALWAYS_TRUE(mBrowserParentMap.Remove(aChildTabId));
}

ContentParentId ContentProcessManager::GetTabProcessId(
    const TabId& aChildTabId) {
  MOZ_ASSERT(NS_IsMainThread());

  if (BrowserParent* browserParent = mBrowserParentMap.Get(aChildTabId)) {
    return browserParent->Manager()->ChildID();
  }
  return ContentParentId(0);
}

uint32_t ContentProcessManager::GetBrowserParentCountByProcessId(
    const ContentParentId& aChildCpId) {
  MOZ_ASSERT(NS_IsMainThread());

  ContentParent* contentParent = mContentParentMap.Get(aChildCpId);
  if (NS_WARN_IF(!contentParent)) {
    return 0;
  }
  return contentParent->ManagedPBrowserParent().Count();
}

already_AddRefed<BrowserParent>
ContentProcessManager::GetBrowserParentByProcessAndTabId(
    const ContentParentId& aChildCpId, const TabId& aChildTabId) {
  RefPtr<BrowserParent> browserParent = mBrowserParentMap.Get(aChildTabId);
  if (NS_WARN_IF(!browserParent)) {
    return nullptr;
  }

  if (NS_WARN_IF(browserParent->Manager()->ChildID() != aChildCpId)) {
    return nullptr;
  }

  return browserParent.forget();
}

already_AddRefed<BrowserParent>
ContentProcessManager::GetTopLevelBrowserParentByProcessAndTabId(
    const ContentParentId& aChildCpId, const TabId& aChildTabId) {
  RefPtr<BrowserParent> browserParent =
      GetBrowserParentByProcessAndTabId(aChildCpId, aChildTabId);
  while (browserParent && browserParent->GetBrowserBridgeParent()) {
    browserParent = browserParent->GetBrowserBridgeParent()->Manager();
  }

  return browserParent.forget();
}

}  // namespace dom
}  // namespace mozilla
