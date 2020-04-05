/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SHistoryChild.h"
#include "SHEntryChild.h"
#include "nsISHistoryListener.h"

namespace mozilla {
namespace dom {
class SwapEntriesDocshellData;
}
}  // namespace mozilla
#define CONTENT_VIEWER_TIMEOUT_SECONDS \
  "browser.sessionhistory.contentViewerTimeout"

// Default this to time out unused content viewers after 30 minutes
#define CONTENT_VIEWER_TIMEOUT_SECONDS_DEFAULT (30 * 60)

namespace mozilla {
namespace dom {

void SHistoryChild::HistoryTracker::NotifyExpired(SHEntryChildShared* aObj) {
  RemoveObject(aObj);
  mSHistory->EvictExpiredContentViewerForEntry(aObj);
}

SHistoryChild::SHistoryChild(BrowsingContext* aRootBC)
    : mRootDocShell(static_cast<nsDocShell*>(aRootBC->GetDocShell())),
      mIPCActorDeleted(false) {
  // Bind mHistoryTracker's event target to the tabGroup for aRootBC.
  // Maybe move this to ChildSHistory?
  nsCOMPtr<nsPIDOMWindowOuter> win = aRootBC->GetDOMWindow();
  if (win) {
    // Seamonkey moves shistory between <xul:browser>s when restoring a tab.
    // Let's try not to break our friend too badly...
    if (mHistoryTracker) {
      NS_WARNING(
          "Change the root docshell of a shistory is unsafe and "
          "potentially problematic.");
      mHistoryTracker->AgeAllGenerations();
    }

    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(win);

    mHistoryTracker = mozilla::MakeUnique<SHistoryChild::HistoryTracker>(
        this,
        mozilla::Preferences::GetUint(CONTENT_VIEWER_TIMEOUT_SECONDS,
                                      CONTENT_VIEWER_TIMEOUT_SECONDS_DEFAULT),
        global->EventTargetFor(mozilla::TaskCategory::Other));
  }
}

NS_IMPL_ADDREF(SHistoryChild)
NS_IMETHODIMP_(MozExternalRefCountType) SHistoryChild::Release() {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  NS_ASSERT_OWNINGTHREAD(SHEntryChild);
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "SHistoryChild");
  if (count == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  if (count == 1 && !mIPCActorDeleted) {
    Unused << Send__delete__(this);
  }
  return count;
}
NS_IMPL_QUERY_INTERFACE(SHistoryChild, nsISHistory, nsISupportsWeakReference)

NS_IMETHODIMP
SHistoryChild::GetCount(int32_t* aCount) {
  return SendGetCount(aCount) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHistoryChild::GetIndex(int32_t* aIndex) {
  return SendGetIndex(aIndex) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHistoryChild::SetIndex(int32_t aIndex) {
  nsresult rv;
  return SendSetIndex(aIndex, &rv) ? rv : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHistoryChild::GetRequestedIndex(int32_t* aRequestedIndex) {
  return SendGetRequestedIndex(aRequestedIndex) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
SHistoryChild::InternalSetRequestedIndex(int32_t aRequestedIndex) {
  SendInternalSetRequestedIndex(aRequestedIndex);
}

NS_IMETHODIMP
SHistoryChild::GetEntryAtIndex(int32_t aIndex, nsISHEntry** aResult) {
  nsresult rv;
  RefPtr<CrossProcessSHEntry> entry;
  if (!SendGetEntryAtIndex(aIndex, &rv, &entry)) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = entry ? do_AddRef(entry->ToSHEntryChild()).take() : nullptr;

  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::PurgeHistory(int32_t aNumEntries) {
  nsresult rv;
  if (!SendPurgeHistory(aNumEntries, &rv)) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRootDocShell) {
    mRootDocShell->HistoryPurged(aNumEntries);
  }

  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::AddSHistoryListener(nsISHistoryListener* aListener) {
  NS_ENSURE_ARG_POINTER(aListener);

  // Check if the listener supports Weak Reference. This is a must.
  // This listener functionality is used by embedders and we want to
  // have the right ownership with who ever listens to SHistory
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_FAILURE;
  }

  mListeners.AppendElementUnlessExists(listener);
  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::RemoveSHistoryListener(nsISHistoryListener* aListener) {
  // Make sure the listener that wants to be removed is the
  // one we have in store.
  nsWeakPtr listener = do_GetWeakReference(aListener);
  mListeners.RemoveElement(listener);
  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::ReloadCurrentEntry() {
  LoadSHEntryResult loadResult;
  if (!SendReloadCurrentEntry(&loadResult)) {
    return NS_ERROR_FAILURE;
  }

  if (loadResult.type() == LoadSHEntryResult::Tnsresult) {
    return loadResult;
  }

  return LoadURI(loadResult);
}

NS_IMETHODIMP
SHistoryChild::GotoIndex(int32_t aIndex) {
  LoadSHEntryResult loadResult;
  if (!SendGotoIndex(aIndex, &loadResult)) {
    return NS_ERROR_FAILURE;
  }

  if (loadResult.type() == LoadSHEntryResult::Tnsresult) {
    return loadResult;
  }

  return LoadURI(loadResult);
}

NS_IMETHODIMP_(int32_t)
SHistoryChild::GetIndexOfEntry(nsISHEntry* aEntry) {
  int32_t index;
  if (!SendGetIndexOfEntry(static_cast<SHEntryChild*>(aEntry), &index)) {
    return 0;
  }
  return index;
}

NS_IMETHODIMP
SHistoryChild::AddEntry(nsISHEntry* aEntry, bool aPersist) {
  NS_ENSURE_ARG(aEntry);

  nsresult rv;
  int32_t entriesPurged;
  if (!SendAddEntry(static_cast<SHEntryChild*>(aEntry), aPersist, &rv,
                    &entriesPurged)) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRootDocShell) {
    aEntry->SetDocshellID(mRootDocShell->HistoryID());

    if (entriesPurged > 0) {
      mRootDocShell->HistoryPurged(entriesPurged);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(void)
SHistoryChild::ClearRootBrowsingContext() { mRootDocShell = nullptr; }

NS_IMETHODIMP
SHistoryChild::UpdateIndex(void) {
  return SendUpdateIndex() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHistoryChild::ReplaceEntry(int32_t aIndex, nsISHEntry* aReplaceEntry) {
  nsresult rv;
  if (!SendReplaceEntry(aIndex, static_cast<SHEntryChild*>(aReplaceEntry),
                        &rv)) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::NotifyOnHistoryReload(bool* _retval) {
  return SendNotifyOnHistoryReload(_retval) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHistoryChild::EvictOutOfRangeContentViewers(int32_t aIndex) {
  // FIXME Need to get out of range entries and entries that are safe (to
  //       compare content viewers so we don't evict live content viewers).
  return SendEvictOutOfRangeContentViewers(aIndex) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHistoryChild::EvictExpiredContentViewerForEntry(nsIBFCacheEntry* aBFEntry) {
  SHEntryChildShared* shared = static_cast<SHEntryChildShared*>(aBFEntry);

  RefPtr<CrossProcessSHEntry> entry;
  int32_t index;
  if (!SendFindEntryForBFCache(shared->GetID(), false, &entry, &index)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SHEntryChild> shEntry;
  if (entry && (shEntry = entry->ToSHEntryChild())) {
    shEntry->EvictContentViewer();
    SendEvict(nsTArray<PSHEntryChild*>({shEntry.get()}));
  }

  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::EvictAllContentViewers(void) {
  nsTArray<RefPtr<CrossProcessSHEntry>> entries;
  if (!SendGetAllEntries(&entries)) {
    return NS_ERROR_FAILURE;
  }

  // Keep a strong reference to all the entries, we're going to send the array
  // back to the parent!
  nsTArray<RefPtr<SHEntryChild>> shEntries(entries.Length());
  for (RefPtr<CrossProcessSHEntry>& entry : entries) {
    RefPtr<SHEntryChild> shEntry = entry->ToSHEntryChild();
    shEntry->EvictContentViewer();
    shEntries.AppendElement(shEntry.forget());
  }

  nsTArray<PSHEntryChild*> pshEntries;
  pshEntries.AppendElements(shEntries);
  SendEvict(pshEntries);
  return NS_OK;
}

NS_IMETHODIMP_(void)
SHistoryChild::EvictContentViewersOrReplaceEntry(nsISHEntry* aNewSHEntry,
                                                 bool aReplace) {
  SendEvictContentViewersOrReplaceEntry(static_cast<SHEntryChild*>(aNewSHEntry),
                                        aReplace);
}

NS_IMETHODIMP_(void)
SHistoryChild::AddToExpirationTracker(nsIBFCacheEntry* aBFEntry) {
  RefPtr<SHEntryChildShared> entry = static_cast<SHEntryChildShared*>(aBFEntry);
  if (mHistoryTracker && entry) {
    mHistoryTracker->AddObject(entry);
  }
}

NS_IMETHODIMP_(void)
SHistoryChild::RemoveFromExpirationTracker(nsIBFCacheEntry* aBFEntry) {
  RefPtr<SHEntryChildShared> entry = static_cast<SHEntryChildShared*>(aBFEntry);
  MOZ_ASSERT(mHistoryTracker && !mHistoryTracker->IsEmpty());
  if (mHistoryTracker && entry) {
    mHistoryTracker->RemoveObject(entry);
  }
}

NS_IMETHODIMP_(void)
SHistoryChild::RemoveDynEntries(int32_t aIndex, nsISHEntry* aEntry) {
  SendRemoveDynEntries(aIndex, static_cast<SHEntryChild*>(aEntry));
}

NS_IMETHODIMP_(void)
SHistoryChild::EnsureCorrectEntryAtCurrIndex(nsISHEntry* aEntry) {
  SendEnsureCorrectEntryAtCurrIndex(static_cast<SHEntryChild*>(aEntry));
}

NS_IMETHODIMP_(void)
SHistoryChild::RemoveDynEntriesForBFCacheEntry(nsIBFCacheEntry* aBFEntry) {
  RefPtr<CrossProcessSHEntry> entry;
  int32_t index;
  if (!SendFindEntryForBFCache(
          static_cast<SHEntryChildShared*>(aBFEntry)->GetID(), true, &entry,
          &index)) {
    return;
  }

  RefPtr<SHEntryChild> shEntry;
  if (entry && (shEntry = entry->ToSHEntryChild())) {
    RemoveDynEntries(index, shEntry);
  }
}

NS_IMETHODIMP_(void)
SHistoryChild::RemoveEntries(nsTArray<nsID>& aIDs, int32_t aStartIndex) {
  bool didRemove = false;
  if (SendRemoveEntries(aIDs, aStartIndex, &didRemove) && didRemove &&
      mRootDocShell) {
    mRootDocShell->DispatchLocationChangeEvent();
  }
}

NS_IMETHODIMP_(void)
SHistoryChild::RemoveFrameEntries(nsISHEntry* aEntry) {
  SendRemoveFrameEntries(static_cast<SHEntryChild*>(aEntry));
}

NS_IMETHODIMP
SHistoryChild::Reload(uint32_t aReloadFlags) {
  LoadSHEntryResult loadResult;
  if (!SendReload(aReloadFlags, &loadResult)) {
    return NS_ERROR_FAILURE;
  }

  if (loadResult.type() == LoadSHEntryResult::Tnsresult) {
    return loadResult;
  }

  return LoadURI(loadResult);
}

NS_IMETHODIMP
SHistoryChild::CreateEntry(nsISHEntry** aEntry) {
  uint64_t sharedID = SHEntryChildShared::CreateSharedID();
  RefPtr<SHEntryChild> entry = static_cast<SHEntryChild*>(
      Manager()->SendPSHEntryConstructor(this, sharedID));
  if (!entry) {
    return NS_ERROR_FAILURE;
  }
  entry.forget(aEntry);
  return NS_OK;
}

nsresult SHistoryChild::LoadURI(nsTArray<LoadSHEntryData>& aLoadData) {
  for (LoadSHEntryData& l : aLoadData) {
    if (l.browsingContext().IsNullOrDiscarded()) {
      continue;
    }

    nsCOMPtr<nsIDocShell> docShell = l.browsingContext().get()->GetDocShell();
    if (!docShell) {
      continue;
    }

    RefPtr<SHEntryChild> entry;
    if (l.shEntry()) {
      entry = l.shEntry()->ToSHEntryChild();
    }

    // FIXME Should this be sent through IPC?
    l.loadState()->SetSHEntry(entry);
    docShell->LoadURI(l.loadState(), false);
  }
  return NS_OK;
}

NS_IMETHODIMP
SHistoryChild::AddToRootSessionHistory(bool aCloneChildren, nsISHEntry* aOSHE,
                                       BrowsingContext* aBC, nsISHEntry* aEntry,
                                       uint32_t aLoadType, bool aShouldPersist,
                                       Maybe<int32_t>* aPreviousEntryIndex,
                                       Maybe<int32_t>* aLoadedEntryIndex) {
  nsresult rv;
  int32_t entriesPurged;
  nsTArray<SwapEntriesDocshellData> entriesToUpdate;
  if (!SendAddToRootSessionHistory(
          aCloneChildren, static_cast<SHEntryChild*>(aOSHE), aBC,
          static_cast<SHEntryChild*>(aEntry), aLoadType, aShouldPersist,
          aPreviousEntryIndex, aLoadedEntryIndex, &entriesToUpdate,
          &entriesPurged, &rv)) {
    return NS_ERROR_FAILURE;
  }
  for (auto& data : entriesToUpdate) {
    MOZ_ASSERT(!data.context().IsNull(), "Browsing context cannot be null");
    nsDocShell* docshell = static_cast<nsDocShell*>(
        data.context().GetMaybeDiscarded()->GetDocShell());
    if (docshell) {
      docshell->SwapHistoryEntries(data.oldEntry()->ToSHEntryChild(),
                                   data.newEntry()->ToSHEntryChild());
    }
  }
  if (NS_SUCCEEDED(rv) && mRootDocShell && entriesPurged > 0) {
    mRootDocShell->HistoryPurged(entriesPurged);
  }
  return rv;
}

NS_IMETHODIMP
SHistoryChild::AddChildSHEntryHelper(nsISHEntry* aCloneRef,
                                     nsISHEntry* aNewEntry,
                                     BrowsingContext* aBC,
                                     bool aCloneChildren) {
  nsresult rv;
  RefPtr<CrossProcessSHEntry> child;
  int32_t entriesPurged;
  nsTArray<SwapEntriesDocshellData> entriesToUpdate;
  if (!SendAddChildSHEntryHelper(static_cast<SHEntryChild*>(aCloneRef),
                                 static_cast<SHEntryChild*>(aNewEntry), aBC,
                                 aCloneChildren, &entriesToUpdate,
                                 &entriesPurged, &child, &rv)) {
    return NS_ERROR_FAILURE;
  }
  for (auto& data : entriesToUpdate) {
    MOZ_ASSERT(!data.context().IsNull(), "Browsing context cannot be null");
    nsDocShell* docshell = static_cast<nsDocShell*>(
        data.context().GetMaybeDiscarded()->GetDocShell());
    if (docshell) {
      docshell->SwapHistoryEntries(data.oldEntry()->ToSHEntryChild(),
                                   data.newEntry()->ToSHEntryChild());
    }
  }
  if (!child) {
    return rv;
  }
  if (NS_SUCCEEDED(rv) && mRootDocShell && entriesPurged > 0) {
    mRootDocShell->HistoryPurged(entriesPurged);
  }

  return rv;
}

}  // namespace dom
}  // namespace mozilla
