/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GroupedSHistory.h"
#include "TabParent.h"
#include "PartialSHistory.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(GroupedSHistory, mPartialHistories)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GroupedSHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GroupedSHistory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GroupedSHistory)
  NS_INTERFACE_MAP_ENTRY(nsIGroupedSHistory)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGroupedSHistory)
NS_INTERFACE_MAP_END

GroupedSHistory::GroupedSHistory()
  : mCount(0),
    mIndexOfActivePartialHistory(-1)
{
}

NS_IMETHODIMP
GroupedSHistory::GetCount(uint32_t* aResult)
{
  MOZ_ASSERT(aResult);
  *aResult = mCount;
  return NS_OK;
}

NS_IMETHODIMP
GroupedSHistory::AppendPartialSessionHistory(nsIPartialSHistory* aPartialHistory)
{
  if (!aPartialHistory) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIPartialSHistory> partialHistory(aPartialHistory);
  if (!partialHistory || mPartialHistories.Contains(partialHistory)) {
    return NS_ERROR_FAILURE;
  }

  // Remove all items after active one and deactive it, unless it's the first
  // call and no active partial history has been set yet.
  if (mIndexOfActivePartialHistory >= 0) {
    PurgePartialHistories(mIndexOfActivePartialHistory);
    nsCOMPtr<nsIPartialSHistory> prevPartialHistory =
      mPartialHistories[mIndexOfActivePartialHistory];
    if (NS_WARN_IF(!prevPartialHistory)) {
      // Cycle collected?
      return NS_ERROR_UNEXPECTED;
    }
    prevPartialHistory->OnDeactive();
  }

  // Attach the partial history.
  uint32_t offset = mCount;
  mCount += partialHistory->GetCount();
  mPartialHistories.AppendElement(partialHistory);
  partialHistory->OnAttachGroupedSessionHistory(offset);
  mIndexOfActivePartialHistory = mPartialHistories.Count() - 1;

  return NS_OK;
}

NS_IMETHODIMP
GroupedSHistory::OnPartialSessionHistoryChange(
  nsIPartialSHistory* aPartialSessionHistory)
{
  if (!aPartialSessionHistory) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIPartialSHistory> partialHistory(aPartialSessionHistory);
  int32_t index = mPartialHistories.IndexOf(partialHistory);
  if (NS_WARN_IF(index != mIndexOfActivePartialHistory) ||
      NS_WARN_IF(index < 0)) {
    // Non-active or not attached partialHistory
    return NS_ERROR_UNEXPECTED;
  }

  PurgePartialHistories(index);

  // Update global count.
  uint32_t count = partialHistory->GetCount();
  uint32_t offset = partialHistory->GetGlobalIndexOffset();
  mCount = count + offset;

  return NS_OK;
}

NS_IMETHODIMP
GroupedSHistory::GotoIndex(uint32_t aGlobalIndex,
                           nsIFrameLoader** aTargetLoaderToSwap)
{
  nsCOMPtr<nsIPartialSHistory> currentPartialHistory =
    mPartialHistories[mIndexOfActivePartialHistory];
  if (!currentPartialHistory) {
    // Cycle collected?
    return NS_ERROR_UNEXPECTED;
  }

  for (uint32_t i = 0; i < mPartialHistories.Length(); i++) {
    nsCOMPtr<nsIPartialSHistory> partialHistory = mPartialHistories[i];
    if (NS_WARN_IF(!partialHistory)) {
      // Cycle collected?
      return NS_ERROR_UNEXPECTED;
    }

    // Examine index range.
    uint32_t offset = partialHistory->GetGlobalIndexOffset();
    uint32_t count = partialHistory->GetCount();
    if (offset <= aGlobalIndex && (offset + count) > aGlobalIndex) {
      uint32_t targetIndex = aGlobalIndex - offset;
      partialHistory->GetOwnerFrameLoader(aTargetLoaderToSwap);
      if ((size_t)mIndexOfActivePartialHistory == i) {
        return NS_OK;
      }
      mIndexOfActivePartialHistory = i;
      if (NS_FAILED(currentPartialHistory->OnDeactive()) ||
          NS_FAILED(partialHistory->OnActive(mCount, targetIndex))) {
        return NS_ERROR_FAILURE;
      }
      return NS_OK;
    }
  }

  // Index not found.
  NS_WARNING("Out of index request!");
  return NS_ERROR_FAILURE;
}

void
GroupedSHistory::PurgePartialHistories(uint32_t aLastPartialIndexToKeep)
{
  uint32_t lastIndex = mPartialHistories.Length() - 1;
  if (aLastPartialIndexToKeep >= lastIndex) {
    // Nothing to remove.
    return;
  }

  // Close tabs.
  for (uint32_t i = lastIndex; i > aLastPartialIndexToKeep; i--) {
    nsCOMPtr<nsIPartialSHistory> partialHistory = mPartialHistories[i];
    if (!partialHistory) {
      // Cycle collected?
      return;
    }

    nsCOMPtr<nsIFrameLoader> loader;
    partialHistory->GetOwnerFrameLoader(getter_AddRefs(loader));
    loader->RequestFrameLoaderClose();
  }

  // Remove references.
  mPartialHistories.RemoveElementsAt(aLastPartialIndexToKeep + 1,
                                     lastIndex - aLastPartialIndexToKeep);
}

/* static */ bool
GroupedSHistory::GroupedHistoryEnabled() {
  return Preferences::GetBool("browser.groupedhistory.enabled", false);
}

NS_IMETHODIMP
GroupedSHistory::CloseInactiveFrameLoaderOwners()
{
  MOZ_ASSERT(mIndexOfActivePartialHistory >= 0);
  for (uint32_t i = 0; i < mPartialHistories.Length(); ++i) {
    if (i != static_cast<uint32_t>(mIndexOfActivePartialHistory)) {
      nsCOMPtr<nsIFrameLoader> loader;
      mPartialHistories[i]->GetOwnerFrameLoader(getter_AddRefs(loader));
      loader->RequestFrameLoaderClose();
    }
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
