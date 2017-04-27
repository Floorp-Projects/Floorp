/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessManager.h"
#include "ContentParent.h"
#include "mozilla/dom/TabParent.h"

#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"

#include "nsPrintfCString.h"
#include "nsIScriptSecurityManager.h"

// XXX need another bug to move this to a common header.
#ifdef DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

namespace mozilla {
namespace dom {

/* static */
StaticAutoPtr<ContentProcessManager>
ContentProcessManager::sSingleton;

/* static */ ContentProcessManager*
ContentProcessManager::GetSingleton()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!sSingleton) {
    sSingleton = new ContentProcessManager();
    ClearOnShutdown(&sSingleton);
  }
  return sSingleton;
}

void
ContentProcessManager::AddContentProcess(ContentParent* aChildCp,
                                         const ContentParentId& aParentCpId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChildCp);

  ContentProcessInfo info;
  info.mCp = aChildCp;
  info.mParentCpId = aParentCpId;
  mContentParentMap[aChildCp->ChildID()] = info;
}

void
ContentProcessManager::RemoveContentProcess(const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mContentParentMap.find(aChildCpId) != mContentParentMap.end());

  mContentParentMap.erase(aChildCpId);
  for (auto iter = mContentParentMap.begin();
       iter != mContentParentMap.end();
       ++iter) {
    if (!iter->second.mChildrenCpId.empty()) {
      iter->second.mChildrenCpId.erase(aChildCpId);
    }
  }
}

bool
ContentProcessManager::AddGrandchildProcess(const ContentParentId& aParentCpId,
                                            const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aParentCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING("Parent process should be already in map!");
    return false;
  }
  iter->second.mChildrenCpId.insert(aChildCpId);
  return true;
}

bool
ContentProcessManager::GetParentProcessId(const ContentParentId& aChildCpId,
                                          /*out*/ ContentParentId* aParentCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }
  *aParentCpId = iter->second.mParentCpId;
  return true;
}

ContentParent*
ContentProcessManager::GetContentProcessById(const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }
  return iter->second.mCp;
}

nsTArray<ContentParentId>
ContentProcessManager::GetAllChildProcessById(const ContentParentId& aParentCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<ContentParentId> cpIdArray;
  auto iter = mContentParentMap.find(aParentCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return Move(cpIdArray);
  }

  for (auto cpIter = iter->second.mChildrenCpId.begin();
       cpIter != iter->second.mChildrenCpId.end();
       ++cpIter) {
    cpIdArray.AppendElement(*cpIter);
  }

  return Move(cpIdArray);
}

bool
ContentProcessManager::RegisterRemoteFrame(const TabId& aTabId,
                                           const TabId& aOpenerTabId,
                                           const IPCTabContext& aContext,
                                           const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  struct RemoteFrameInfo info;

  // If it's a PopupIPCTabContext, it's the case that a TabChild want to
  // open a new tab. aOpenerTabId has to be it's parent frame's opener id.
  if (aContext.type() == IPCTabContext::TPopupIPCTabContext) {
    auto remoteFrameIter = iter->second.mRemoteFrames.find(aOpenerTabId);
    if (remoteFrameIter == iter->second.mRemoteFrames.end()) {
      ASSERT_UNLESS_FUZZING("Failed to find parent frame's opener id.");
      return false;
    }

    info.mOpenerTabId = remoteFrameIter->second.mOpenerTabId;
    info.mContext = remoteFrameIter->second.mContext;
  }
  else {
    MaybeInvalidTabContext tc(aContext);
    if (!tc.IsValid()) {
      NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                               "the child process. (%s)",
                               tc.GetInvalidReason()).get());
      return false;
    }
    info.mOpenerTabId = aOpenerTabId;
    info.mContext = tc.GetTabContext();
  }

  iter->second.mRemoteFrames[aTabId] = info;
  return true;
}

void
ContentProcessManager::UnregisterRemoteFrame(const ContentParentId& aChildCpId,
                                             const TabId& aChildTabId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return;
  }

  auto remoteFrameIter = iter->second.mRemoteFrames.find(aChildTabId);
  if (remoteFrameIter != iter->second.mRemoteFrames.end()) {
    iter->second.mRemoteFrames.erase(aChildTabId);
  }
}

bool
ContentProcessManager::GetTabContextByProcessAndTabId(const ContentParentId& aChildCpId,
                                                      const TabId& aChildTabId,
                                                      /*out*/ TabContext* aTabContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTabContext);

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  auto remoteFrameIter = iter->second.mRemoteFrames.find(aChildTabId);
  if (NS_WARN_IF(remoteFrameIter == iter->second.mRemoteFrames.end())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  *aTabContext = remoteFrameIter->second.mContext;

  return true;
}

nsTArray<TabContext>
ContentProcessManager::GetTabContextByContentProcess(const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<TabContext> tabContextArray;
  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return Move(tabContextArray);
  }

  for (auto remoteFrameIter = iter->second.mRemoteFrames.begin();
       remoteFrameIter != iter->second.mRemoteFrames.end();
       ++remoteFrameIter) {
    tabContextArray.AppendElement(remoteFrameIter->second.mContext);
  }

  return Move(tabContextArray);
}

bool
ContentProcessManager::GetRemoteFrameOpenerTabId(const ContentParentId& aChildCpId,
                                                 const TabId& aChildTabId,
                                                 /*out*/TabId* aOpenerTabId)
{
  MOZ_ASSERT(NS_IsMainThread());
  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  auto remoteFrameIter = iter->second.mRemoteFrames.find(aChildTabId);
  if (NS_WARN_IF(remoteFrameIter == iter->second.mRemoteFrames.end())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  *aOpenerTabId = remoteFrameIter->second.mOpenerTabId;

  return true;
}

already_AddRefed<TabParent>
ContentProcessManager::GetTabParentByProcessAndTabId(const ContentParentId& aChildCpId,
                                                     const TabId& aChildTabId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  const ManagedContainer<PBrowserParent>& browsers = iter->second.mCp->ManagedPBrowserParent();
  for (auto iter = browsers.ConstIter(); !iter.Done(); iter.Next()) {
    RefPtr<TabParent> tab = TabParent::GetFrom(iter.Get()->GetKey());
    if (tab->GetTabId() == aChildTabId) {
      return tab.forget();
    }
  }

  return nullptr;
}

already_AddRefed<TabParent>
ContentProcessManager::GetTopLevelTabParentByProcessAndTabId(const ContentParentId& aChildCpId,
                                                             const TabId& aChildTabId)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Used to keep the current ContentParentId and the current TabId
  // in the iteration(do-while loop below)
  ContentParentId currentCpId;
  TabId currentTabId;

  // To get the ContentParentId and the TabParentId on upper level
  ContentParentId parentCpId = aChildCpId;
  TabId openerTabId = aChildTabId;

  // Stop this loop when the upper ContentParentId of
  // the current ContentParentId is chrome(ContentParentId = 0).
  do {
    // Update the current ContentParentId and TabId in iteration
    currentCpId = parentCpId;
    currentTabId = openerTabId;

    // Get the ContentParentId and TabId on upper level
    if (!GetParentProcessId(currentCpId, &parentCpId) ||
        !GetRemoteFrameOpenerTabId(currentCpId, currentTabId, &openerTabId)) {
      return nullptr;
    }
  } while (parentCpId);

  // Get the top level TabParent by the current ContentParentId and TabId
  return GetTabParentByProcessAndTabId(currentCpId, currentTabId);
}

nsTArray<TabId>
ContentProcessManager::GetTabParentsByProcessId(const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<TabId> tabIdList;
  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    ASSERT_UNLESS_FUZZING();
    return Move(tabIdList);
  }

  for (auto remoteFrameIter = iter->second.mRemoteFrames.begin();
      remoteFrameIter != iter->second.mRemoteFrames.end();
      ++remoteFrameIter) {
    tabIdList.AppendElement(remoteFrameIter->first);
  }

  return Move(tabIdList);
}

uint32_t
ContentProcessManager::GetTabParentCountByProcessId(const ContentParentId& aChildCpId)
{
  MOZ_ASSERT(NS_IsMainThread());

  auto iter = mContentParentMap.find(aChildCpId);
  if (NS_WARN_IF(iter == mContentParentMap.end())) {
    return 0;
  }

  return iter->second.mRemoteFrames.size();
}

} // namespace dom
} // namespace mozilla
