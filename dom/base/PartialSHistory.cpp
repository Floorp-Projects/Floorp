/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PartialSHistory.h"

#include "nsIWebNavigation.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(PartialSHistory, mOwnerFrameLoader, mGroupedSHistory)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PartialSHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PartialSHistory)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PartialSHistory)
  NS_INTERFACE_MAP_ENTRY(nsIPartialSHistory)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPartialSHistory)
  NS_INTERFACE_MAP_ENTRY(nsISHistoryListener)
  NS_INTERFACE_MAP_ENTRY(nsIPartialSHistoryListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

PartialSHistory::PartialSHistory(nsIFrameLoader* aOwnerFrameLoader)
  : mCount(0),
    mGlobalIndexOffset(0),
    mActive(nsIPartialSHistory::STATE_ACTIVE),
    mOwnerFrameLoader(aOwnerFrameLoader)
{
  MOZ_ASSERT(aOwnerFrameLoader);
}

already_AddRefed<nsISHistory>
PartialSHistory::GetSessionHistory()
{
  if (!mOwnerFrameLoader) {
    // Cycle collected?
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell;
  mOwnerFrameLoader->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) {
    return nullptr;
  }

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
  nsCOMPtr<nsISHistory> shistory;
  webNav->GetSessionHistory(getter_AddRefs(shistory));
  return shistory.forget();
}

already_AddRefed<TabParent>
PartialSHistory::GetTabParent()
{
  if (!mOwnerFrameLoader) {
    // Cycle collected?
    return nullptr;
  }

  nsCOMPtr<nsITabParent> tabParent;
  mOwnerFrameLoader->GetTabParent(getter_AddRefs(tabParent));
  return RefPtr<TabParent>(static_cast<TabParent*>(tabParent.get())).forget();
}

NS_IMETHODIMP
PartialSHistory::GetCount(uint32_t* aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }

  // If we have direct reference to nsISHistory, simply pass through.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    int32_t count;
    nsresult rv = shistory->GetCount(&count);
    if (NS_FAILED(rv) || count < 0) {
      *aResult = 0;
      return NS_ERROR_FAILURE;
    }
    *aResult = count;
    return NS_OK;
  }

  // Otherwise use the cached value.
  *aResult = mCount;
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::GetGlobalIndex(int32_t* aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsISHistory> shistory = GetSessionHistory();
  if (shistory) {
    int32_t idx;
    nsresult rv = shistory->GetIndex(&idx);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = idx + GetGlobalIndexOffset();
    return NS_OK;
  }

  *aResult = mIndex + GetGlobalIndexOffset();
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::GetGlobalIndexOffset(uint32_t* aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }

  // If we have direct reference to nsISHistory, simply pass through.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    int32_t offset;
    nsresult rv = shistory->GetGlobalIndexOffset(&offset);
    if (NS_FAILED(rv) || offset < 0) {
      *aResult = 0;
      return NS_ERROR_FAILURE;
    }
    *aResult = offset;
    return NS_OK;
  }

  // Otherwise use the cached value.
  *aResult = mGlobalIndexOffset;
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::GetOwnerFrameLoader(nsIFrameLoader** aResult)
{
  nsCOMPtr<nsIFrameLoader> loader(mOwnerFrameLoader);
  loader.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::OnAttachGroupedSHistory(nsIGroupedSHistory* aGroup, uint32_t aOffset)
{
  MOZ_ASSERT(!mGroupedSHistory, "Only may join a single GroupedSHistory");

  mActive = nsIPartialSHistory::STATE_ACTIVE;
  mGlobalIndexOffset = aOffset;
  mGroupedSHistory = aGroup;

  // If we have direct reference to nsISHistory, simply pass through.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    // nsISHistory uses int32_t
    if (aOffset > INT32_MAX) {
      return NS_ERROR_FAILURE;
    }
    return shistory->OnAttachGroupedSHistory(aOffset);
  }

  // Otherwise notify through TabParent.
  RefPtr<TabParent> tabParent(GetTabParent());
  if (!tabParent) {
    // We have neither shistory nor tabParent?
    NS_WARNING("Unable to get shitory nor tabParent!");
    return NS_ERROR_UNEXPECTED;
  }
  Unused << tabParent->SendNotifyAttachGroupedSHistory(aOffset);

  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::HandleSHistoryUpdate(uint32_t aCount, uint32_t aIndex, bool aTruncate)
{
  // Update our local cache of mCount and mIndex
  mCount = aCount;
  mIndex = aIndex;
  return SHistoryDidUpdate(aTruncate);
}

nsresult
PartialSHistory::SHistoryDidUpdate(bool aTruncate /* = false */)
{
  if (!mOwnerFrameLoader) {
    // Cycle collected?
    return NS_ERROR_UNEXPECTED;
  }

  if (!mGroupedSHistory) {
    // It's OK if we don't have a grouped history, that just means that we
    // aren't in a grouped shistory, so we don't need to do anything.
    return NS_OK;
  }

  mGroupedSHistory->HandleSHistoryUpdate(this, aTruncate);
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::OnActive(uint32_t aGlobalLength, uint32_t aTargetLocalIndex)
{
  MOZ_ASSERT(mGroupedSHistory);

  mActive = nsIPartialSHistory::STATE_ACTIVE;

  // In-process case.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    // nsISHistory uses int32_t
    if (aGlobalLength > INT32_MAX || aTargetLocalIndex > INT32_MAX) {
      return NS_ERROR_FAILURE;
    }
    return shistory->OnPartialSHistoryActive(aGlobalLength, aTargetLocalIndex);
  }

  // Cross-process case.
  RefPtr<TabParent> tabParent(GetTabParent());
  if (!tabParent) {
    // We have neither shistory nor tabParent?
    NS_WARNING("Unable to get shitory nor tabParent!");
    return NS_ERROR_UNEXPECTED;
  }
  Unused << tabParent->SendNotifyPartialSHistoryActive(aGlobalLength,
                                                       aTargetLocalIndex);

  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::OnDeactive()
{
  MOZ_ASSERT(mGroupedSHistory);

  mActive = nsIPartialSHistory::STATE_INACTIVE;

  // In-process case.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    if (NS_FAILED(shistory->OnPartialSHistoryDeactive())) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  // Cross-process case.
  RefPtr<TabParent> tabParent(GetTabParent());
  if (!tabParent) {
    // We have neither shistory nor tabParent?
    NS_WARNING("Unable to get shitory nor tabParent!");
    return NS_ERROR_UNEXPECTED;
  }
  Unused << tabParent->SendNotifyPartialSHistoryDeactive();
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::GetActiveState(int32_t* aActive)
{
  *aActive = mActive;
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::SetActiveState(int32_t aActive)
{
  mActive = aActive;
  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::GetGroupedSHistory(nsIGroupedSHistory** aGrouped)
{
  nsCOMPtr<nsIGroupedSHistory> shistory = mGroupedSHistory;
  shistory.forget(aGrouped);
  return NS_OK;
}

/*******************************************************************************
 * nsIPartialSHistoryListener
 ******************************************************************************/

NS_IMETHODIMP
PartialSHistory::OnRequestCrossBrowserNavigation(uint32_t aIndex)
{
  if (!mOwnerFrameLoader) {
    // Cycle collected?
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISupports> promise;
  return mOwnerFrameLoader->RequestGroupedHistoryNavigation(aIndex, getter_AddRefs(promise));
}

/*******************************************************************************
 * nsISHistoryListener
 ******************************************************************************/

NS_IMETHODIMP
PartialSHistory::OnLengthChanged(int32_t aCount)
{
  return SHistoryDidUpdate(/* aTruncate = */ true);
}

NS_IMETHODIMP
PartialSHistory::OnIndexChanged(int32_t aIndex)
{
  return SHistoryDidUpdate(/* aTruncate = */ false);
}

NS_IMETHODIMP
PartialSHistory::OnHistoryNewEntry(nsIURI *aNewURI, int32_t aOldIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartialSHistory::OnHistoryGoBack(nsIURI *aBackURI, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartialSHistory::OnHistoryGoForward(nsIURI *aForwardURI, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartialSHistory::OnHistoryReload(nsIURI *aReloadURI, uint32_t aReloadFlags, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartialSHistory::OnHistoryGotoIndex(int32_t aIndex, nsIURI *aGotoURI, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartialSHistory::OnHistoryPurge(int32_t aNumEntries, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartialSHistory::OnHistoryReplaceEntry(int32_t aIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace dom
} // namespace mozilla
