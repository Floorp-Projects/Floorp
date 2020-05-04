/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SHistoryParent.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/SHEntryParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/Logging.h"
#include "nsTHashtable.h"
#include "SHEntryChild.h"

extern mozilla::LazyLogModule gSHistoryLog;

namespace mozilla {
namespace dom {

LegacySHistory::LegacySHistory(SHistoryParent* aSHistoryParent,
                               CanonicalBrowsingContext* aRootBC,
                               const nsID& aDocShellID)
    : nsSHistory(aRootBC, aDocShellID), mSHistoryParent(aSHistoryParent) {
  mIsRemote = true;
  aRootBC->SetSessionHistory(this);
}

static void FillInLoadResult(
    nsresult aRv, const nsTArray<nsSHistory::LoadEntryResult>& aLoadResults,
    LoadSHEntryResult* aResult) {
  if (NS_SUCCEEDED(aRv)) {
    nsTArray<LoadSHEntryData> data;
    data.SetCapacity(aLoadResults.Length());
    for (const nsSHistory::LoadEntryResult& l : aLoadResults) {
      data.AppendElement(
          LoadSHEntryData(static_cast<LegacySHEntry*>(l.mLoadState->SHEntry()),
                          l.mBrowsingContext, l.mLoadState));
    }

    *aResult = data;
  } else {
    *aResult = aRv;
  }
}

SHistoryParent::SHistoryParent(CanonicalBrowsingContext* aContext)
    : mHistory(new LegacySHistory(this, aContext, nsID())) {}

SHistoryParent::~SHistoryParent() { mHistory->mSHistoryParent = nullptr; }

SHEntryParent* SHistoryParent::CreateEntry(PContentParent* aContentParent,
                                           PSHistoryParent* aSHistoryParent,
                                           uint64_t aSharedID) {
  RefPtr<LegacySHEntry> entry = new LegacySHEntry(
      aContentParent, static_cast<SHistoryParent*>(aSHistoryParent)->mHistory,
      aSharedID);
  return entry->CreateActor();
}

void SHistoryParent::ActorDestroy(ActorDestroyReason aWhy) {}

bool SHistoryParent::RecvGetCount(int32_t* aCount) {
  return NS_SUCCEEDED(mHistory->GetCount(aCount));
}

bool SHistoryParent::RecvGetIndex(int32_t* aIndex) {
  return NS_SUCCEEDED(mHistory->GetIndex(aIndex));
}

bool SHistoryParent::RecvSetIndex(int32_t aIndex, nsresult* aResult) {
  *aResult = mHistory->SetIndex(aIndex);
  return true;
}

bool SHistoryParent::RecvGetRequestedIndex(int32_t* aIndex) {
  return NS_SUCCEEDED(mHistory->GetRequestedIndex(aIndex));
}

bool SHistoryParent::RecvInternalSetRequestedIndex(int32_t aIndex) {
  mHistory->InternalSetRequestedIndex(aIndex);
  return true;
}

bool SHistoryParent::RecvGetEntryAtIndex(int32_t aIndex, nsresult* aResult,
                                         RefPtr<CrossProcessSHEntry>* aEntry) {
  nsCOMPtr<nsISHEntry> entry;
  *aResult = mHistory->GetEntryAtIndex(aIndex, getter_AddRefs(entry));
  *aEntry = entry.forget().downcast<LegacySHEntry>();
  return true;
}

bool SHistoryParent::RecvPurgeHistory(int32_t aNumEntries, nsresult* aResult) {
  *aResult = mHistory->PurgeHistory(aNumEntries);
  return true;
}

bool SHistoryParent::RecvReloadCurrentEntry(LoadSHEntryResult* aLoadResult) {
  nsTArray<nsSHistory::LoadEntryResult> loadResults;
  nsresult rv = mHistory->ReloadCurrentEntry(loadResults);
  if (NS_SUCCEEDED(rv)) {
    FillInLoadResult(rv, loadResults, aLoadResult);
  } else {
    *aLoadResult = rv;
  }
  return true;
}

bool SHistoryParent::RecvGotoIndex(int32_t aIndex,
                                   LoadSHEntryResult* aLoadResult) {
  nsTArray<nsSHistory::LoadEntryResult> loadResults;
  nsresult rv = mHistory->GotoIndex(aIndex, loadResults);
  FillInLoadResult(rv, loadResults, aLoadResult);
  return true;
}

bool SHistoryParent::RecvGetIndexOfEntry(PSHEntryParent* aEntry,
                                         int32_t* aIndex) {
  MOZ_ASSERT(Manager() == aEntry->Manager());
  *aIndex =
      mHistory->GetIndexOfEntry(static_cast<SHEntryParent*>(aEntry)->mEntry);
  return true;
}

bool SHistoryParent::RecvAddEntry(PSHEntryParent* aEntry, bool aPersist,
                                  nsresult* aResult, int32_t* aEntriesPurged) {
  MOZ_ASSERT(Manager() == aEntry->Manager());
  *aResult = mHistory->AddEntry(static_cast<SHEntryParent*>(aEntry)->mEntry,
                                aPersist, aEntriesPurged);
  return true;
}

bool SHistoryParent::RecvUpdateIndex() {
  return NS_SUCCEEDED(mHistory->UpdateIndex());
}

bool SHistoryParent::RecvReplaceEntry(int32_t aIndex, PSHEntryParent* aEntry,
                                      nsresult* aResult) {
  MOZ_ASSERT(Manager() == aEntry->Manager());
  *aResult = mHistory->ReplaceEntry(
      aIndex, static_cast<SHEntryParent*>(aEntry)->mEntry);
  return true;
}

bool SHistoryParent::RecvNotifyOnHistoryReload(bool* aOk) {
  return NS_SUCCEEDED(mHistory->NotifyOnHistoryReload(aOk));
}

bool SHistoryParent::RecvEvictOutOfRangeContentViewers(int32_t aIndex) {
  return NS_SUCCEEDED(mHistory->EvictOutOfRangeContentViewers(aIndex));
}

bool SHistoryParent::RecvEvictAllContentViewers() {
  return NS_SUCCEEDED(mHistory->EvictAllContentViewers());
}

bool SHistoryParent::RecvEvictContentViewersOrReplaceEntry(
    PSHEntryParent* aNewSHEntry, bool aReplace) {
  MOZ_ASSERT(Manager() == aNewSHEntry->Manager());
  mHistory->EvictContentViewersOrReplaceEntry(
      aNewSHEntry ? static_cast<SHEntryParent*>(aNewSHEntry)->mEntry.get()
                  : nullptr,
      aReplace);
  return true;
}

bool SHistoryParent::RecvRemoveDynEntries(int32_t aIndex,
                                          PSHEntryParent* aEntry) {
  MOZ_ASSERT(Manager() == aEntry->Manager());
  mHistory->RemoveDynEntries(aIndex,
                             static_cast<SHEntryParent*>(aEntry)->mEntry);
  return true;
}

bool SHistoryParent::RecvEnsureCorrectEntryAtCurrIndex(PSHEntryParent* aEntry) {
  mHistory->EnsureCorrectEntryAtCurrIndex(
      static_cast<SHEntryParent*>(aEntry)->mEntry);
  return true;
}

bool SHistoryParent::RecvRemoveEntries(nsTArray<nsID>&& aIds, int32_t aIndex,
                                       bool* aDidRemove) {
  mHistory->RemoveEntries(aIds, aIndex, aDidRemove);
  return true;
}

bool SHistoryParent::RecvRemoveFrameEntries(PSHEntryParent* aEntry) {
  mHistory->RemoveFrameEntries(
      static_cast<SHEntryParent*>(aEntry)->GetSHEntry());
  return true;
}

bool SHistoryParent::RecvReload(const uint32_t& aReloadFlags,
                                LoadSHEntryResult* aLoadResult) {
  nsTArray<nsSHistory::LoadEntryResult> loadResults;
  nsresult rv = mHistory->Reload(aReloadFlags, loadResults);
  if (NS_SUCCEEDED(rv) && loadResults.IsEmpty()) {
    *aLoadResult = NS_OK;
  } else {
    FillInLoadResult(rv, loadResults, aLoadResult);
  }
  return true;
}

bool SHistoryParent::RecvGetAllEntries(
    nsTArray<RefPtr<CrossProcessSHEntry>>* aEntries) {
  nsTArray<nsCOMPtr<nsISHEntry>>& entries = mHistory->Entries();
  uint32_t length = entries.Length();
  aEntries->AppendElements(length);
  for (uint32_t i = 0; i < length; ++i) {
    aEntries->ElementAt(i) = static_cast<LegacySHEntry*>(entries[i].get());
  }
  return true;
}

bool SHistoryParent::RecvFindEntryForBFCache(
    const uint64_t& aSharedID, const bool& aIncludeCurrentEntry,
    RefPtr<CrossProcessSHEntry>* aEntry, int32_t* aIndex) {
  int32_t currentIndex;
  mHistory->GetIndex(&currentIndex);
  int32_t startSafeIndex, endSafeIndex;
  mHistory->WindowIndices(currentIndex, &startSafeIndex, &endSafeIndex);
  for (int32_t i = startSafeIndex; i <= endSafeIndex; ++i) {
    nsCOMPtr<nsISHEntry> entry;
    nsresult rv = mHistory->GetEntryAtIndex(i, getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, false);

    RefPtr<LegacySHEntry> shEntry = entry.forget().downcast<LegacySHEntry>();
    if (shEntry->GetSharedStateID() == aSharedID) {
      if (!aIncludeCurrentEntry && i == currentIndex) {
        *aEntry = nullptr;
        *aIndex = -1;
      } else {
        *aEntry = std::move(shEntry);
        *aIndex = i;
      }

      return true;
    }
  }
  *aEntry = nullptr;
  *aIndex = -1;
  return true;
}

bool SHistoryParent::RecvEvict(nsTArray<PSHEntryParent*>&& aEntries) {
  for (PSHEntryParent* entry : aEntries) {
    int32_t index =
        mHistory->GetIndexOfEntry(static_cast<SHEntryParent*>(entry)->mEntry);
    if (index != -1) {
      mHistory->RemoveDynEntries(index,
                                 static_cast<SHEntryParent*>(entry)->mEntry);
    }
  }
  return true;
}

bool SHistoryParent::RecvNotifyListenersContentViewerEvicted(
    uint32_t aNumEvicted) {
  mHistory->NotifyListenersContentViewerEvicted(aNumEvicted);
  return true;
}

void LegacySHistory::EvictOutOfRangeWindowContentViewers(int32_t aIndex) {
  if (aIndex < 0) {
    return;
  }
  NS_ENSURE_TRUE_VOID(aIndex < Length());

  // Calculate the range that's safe from eviction.
  int32_t startSafeIndex, endSafeIndex;
  WindowIndices(aIndex, &startSafeIndex, &endSafeIndex);

  // See nsSHistory::EvictOutOfRangeWindowContentViewers for more comments.

  MOZ_LOG(gSHistoryLog, mozilla::LogLevel::Debug,
          ("EvictOutOfRangeWindowContentViewers(index=%d), "
           "Length()=%d. Safe range [%d, %d]",
           aIndex, Length(), startSafeIndex, endSafeIndex));

  // Collect content viewers within safe range so we don't accidentally evict
  // one of them if it appears outside this range.
  nsTHashtable<nsUint64HashKey> safeSharedStateIDs;
  for (int32_t i = startSafeIndex; i <= endSafeIndex; i++) {
    RefPtr<LegacySHEntry> entry =
        static_cast<LegacySHEntry*>(mEntries[i].get());
    MOZ_ASSERT(entry);
    safeSharedStateIDs.PutEntry(entry->GetSharedStateID());
  }

  // Iterate over entries that are not within safe range and save the IDs
  // of shared state and content parents into a hashtable.
  // Format of the hashtable: content parent -> list of shared state IDs
  nsDataHashtable<nsPtrHashKey<PContentParent>, nsTHashtable<nsUint64HashKey>>
      toEvict;
  for (int32_t i = 0; i < Length(); i++) {
    if (i >= startSafeIndex && i <= endSafeIndex) {
      continue;
    }
    RefPtr<LegacySHEntry> entry =
        static_cast<LegacySHEntry*>(mEntries[i].get());
    dom::SHEntrySharedParentState* sharedParentState = entry->GetSharedState();
    uint64_t id = entry->GetSharedStateID();
    PContentParent* parent =
        static_cast<SHEntrySharedParent*>(sharedParentState)
            ->GetContentParent();
    MOZ_ASSERT(parent);
    if (!safeSharedStateIDs.Contains(id)) {
      nsTHashtable<nsUint64HashKey>& ids = toEvict.GetOrInsert(parent);
      ids.PutEntry(id);
    }
  }
  if (toEvict.Count() == 0) {
    return;
  }
  // Remove dynamically created children from entries that will be evicted
  // later. We are iterating over the mEntries (instead of toEvict) to gain
  // access to each nsSHEntry because toEvict only contains content parents and
  // IDs of SHEntrySharedParentState

  // (It's important that the condition checks Length(), rather than a cached
  // copy of Length(), because the length might change between iterations due to
  // RemoveDynEntries call.)
  for (int32_t i = 0; i < Length(); i++) {
    RefPtr<LegacySHEntry> entry =
        static_cast<LegacySHEntry*>(mEntries[i].get());
    MOZ_ASSERT(entry);
    uint64_t id = entry->GetSharedStateID();
    if (!safeSharedStateIDs.Contains(id)) {
      // When dropping bfcache, we have to remove associated dynamic entries as
      // well.
      int32_t index = GetIndexOfEntry(entry);
      if (index != -1) {
        RemoveDynEntries(index, entry);
      }
    }
  }

  // Iterate over the 'toEvict' hashtable to get the IDs of content viewers to
  // evict for each parent
  for (auto iter = toEvict.ConstIter(); !iter.Done(); iter.Next()) {
    auto parent = iter.Key();
    const nsTHashtable<nsUint64HashKey>& ids = iter.Data();

    // Convert ids into an array because we don't have support for passing
    // nsTHashtable over IPC
    AutoTArray<uint64_t, 4> evictArray;
    for (auto iter = ids.ConstIter(); !iter.Done(); iter.Next()) {
      evictArray.AppendElement(iter.Get()->GetKey());
    }

    Unused << parent->SendEvictContentViewers(evictArray);
  }
}

NS_IMETHODIMP
LegacySHistory::CreateEntry(nsISHEntry** aEntry) {
  NS_ENSURE_TRUE(mRootBC, NS_ERROR_FAILURE);

  NS_ADDREF(*aEntry = new LegacySHEntry(
                static_cast<CanonicalBrowsingContext*>(mRootBC)
                    ->GetContentParent(),
                this, SHEntryChildShared::CreateSharedID()));
  return NS_OK;
}

NS_IMETHODIMP
LegacySHistory::ReloadCurrentEntry() {
  return mSHistoryParent->SendReloadCurrentEntryFromChild() ? NS_OK
                                                            : NS_ERROR_FAILURE;
}

bool SHistoryParent::RecvAddToRootSessionHistory(
    bool aCloneChildren, PSHEntryParent* aOSHE,
    const MaybeDiscarded<BrowsingContext>& aBC, PSHEntryParent* aEntry,
    uint32_t aLoadType, bool aShouldPersist,
    Maybe<int32_t>* aPreviousEntryIndex, Maybe<int32_t>* aLoadedEntryIndex,
    nsTArray<SwapEntriesDocshellData>* aEntriesToUpdate,
    int32_t* aEntriesPurged, nsresult* aResult) {
  MOZ_ASSERT(!aBC.IsNull(), "Browsing context cannot be null");
  nsTArray<EntriesAndBrowsingContextData> entriesToSendOverIDL;
  *aResult = mHistory->AddToRootSessionHistory(
      aCloneChildren,
      aOSHE ? static_cast<SHEntryParent*>(aOSHE)->mEntry.get() : nullptr,
      aBC.GetMaybeDiscarded(),
      static_cast<SHEntryParent*>(aEntry)->mEntry.get(), aLoadType,
      aShouldPersist, static_cast<ContentParent*>(Manager())->ChildID(),
      aPreviousEntryIndex, aLoadedEntryIndex, &entriesToSendOverIDL,
      aEntriesPurged);
  SHistoryParent::CreateActorsForSwapEntries(entriesToSendOverIDL,
                                             aEntriesToUpdate, Manager());
  return true;
}

bool SHistoryParent::RecvAddChildSHEntryHelper(
    PSHEntryParent* aCloneRef, PSHEntryParent* aNewEntry,
    const MaybeDiscarded<BrowsingContext>& aBC, bool aCloneChildren,
    nsTArray<SwapEntriesDocshellData>* aEntriesToUpdate,
    int32_t* aEntriesPurged, RefPtr<CrossProcessSHEntry>* aChild,
    nsresult* aResult) {
  MOZ_ASSERT(!aBC.IsNull(), "Browsing context cannot be null");
  nsCOMPtr<nsISHEntry> child;
  nsTArray<EntriesAndBrowsingContextData> entriesToSendOverIPC;
  *aResult = mHistory->AddChildSHEntryHelper(
      static_cast<SHEntryParent*>(aCloneRef)->mEntry.get(),
      aNewEntry ? static_cast<SHEntryParent*>(aNewEntry)->mEntry.get()
                : nullptr,
      aBC.GetMaybeDiscarded(), aCloneChildren,
      static_cast<ContentParent*>(Manager())->ChildID(), &entriesToSendOverIPC,
      aEntriesPurged, getter_AddRefs(child));
  SHistoryParent::CreateActorsForSwapEntries(entriesToSendOverIPC,
                                             aEntriesToUpdate, Manager());
  *aChild = child.forget().downcast<LegacySHEntry>();
  return true;
}

void SHistoryParent::CreateActorsForSwapEntries(
    const nsTArray<EntriesAndBrowsingContextData>& aEntriesToSendOverIPC,
    nsTArray<SwapEntriesDocshellData>* aEntriesToUpdate,
    PContentParent* aContentParent) {
  for (auto& data : aEntriesToSendOverIPC) {
    SwapEntriesDocshellData* toUpdate = aEntriesToUpdate->AppendElement();

    // Old entry
    toUpdate->oldEntry() = static_cast<LegacySHEntry*>(data.oldEntry.get());

    // New entry
    toUpdate->newEntry() = static_cast<LegacySHEntry*>(data.newEntry.get());
    toUpdate->context() = data.context;
  }
}

}  // namespace dom
}  // namespace mozilla
