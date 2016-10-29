/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PartialSHistory.h"

#include "nsIWebNavigation.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(PartialSHistory, mOwnerFrameLoader)
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
PartialSHistory::OnAttachGroupedSessionHistory(uint32_t aOffset)
{
  mGlobalIndexOffset = aOffset;

  // If we have direct reference to nsISHistory, simply pass through.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    // nsISHistory uses int32_t
    if (aOffset > INT32_MAX) {
      return NS_ERROR_FAILURE;
    }
    return shistory->OnAttachGroupedSessionHistory(aOffset);
  }

  // Otherwise notify through TabParent.
  RefPtr<TabParent> tabParent(GetTabParent());
  if (!tabParent) {
    // We have neither shistory nor tabParent?
    NS_WARNING("Unable to get shitory nor tabParent!");
    return NS_ERROR_UNEXPECTED;
  }
  Unused << tabParent->SendNotifyAttachGroupedSessionHistory(aOffset);

  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::OnSessionHistoryChange(uint32_t aCount)
{
  mCount = aCount;
  return OnLengthChange(aCount);
}

NS_IMETHODIMP
PartialSHistory::OnActive(uint32_t aGlobalLength, uint32_t aTargetLocalIndex)
{
  // In-process case.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    // nsISHistory uses int32_t
    if (aGlobalLength > INT32_MAX || aTargetLocalIndex > INT32_MAX) {
      return NS_ERROR_FAILURE;
    }
    return shistory->OnPartialSessionHistoryActive(aGlobalLength,
                                                   aTargetLocalIndex);
  }

  // Cross-process case.
  RefPtr<TabParent> tabParent(GetTabParent());
  if (!tabParent) {
    // We have neither shistory nor tabParent?
    NS_WARNING("Unable to get shitory nor tabParent!");
    return NS_ERROR_UNEXPECTED;
  }
  Unused << tabParent->SendNotifyPartialSessionHistoryActive(aGlobalLength,
                                                             aTargetLocalIndex);

  return NS_OK;
}

NS_IMETHODIMP
PartialSHistory::OnDeactive()
{
  // In-process case.
  nsCOMPtr<nsISHistory> shistory(GetSessionHistory());
  if (shistory) {
    if (NS_FAILED(shistory->OnPartialSessionHistoryDeactive())) {
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
  Unused << tabParent->SendNotifyPartialSessionHistoryDeactive();
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
  return mOwnerFrameLoader->RequestGroupedHistoryNavigation(aIndex);
}

/*******************************************************************************
 * nsISHistoryListener
 ******************************************************************************/

NS_IMETHODIMP
PartialSHistory::OnLengthChange(int32_t aCount)
{
  if (!mOwnerFrameLoader) {
    // Cycle collected?
    return NS_ERROR_UNEXPECTED;
  }

  if (aCount < 0) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGroupedSHistory> groupedHistory;
  mOwnerFrameLoader->GetGroupedSessionHistory(getter_AddRefs(groupedHistory));
  if (!groupedHistory) {
    // Maybe we're not the active partial history, but in this case we shouldn't
    // receive any update from session history object either.
    return NS_ERROR_FAILURE;
  }

  groupedHistory->OnPartialSessionHistoryChange(this);
  return NS_OK;
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
