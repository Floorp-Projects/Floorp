/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SHistoryParent.h"
#include "mozilla/dom/SHEntryParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentProcessManager.h"

namespace mozilla {
namespace dom {

static void FillInLoadResult(PContentParent* aManager, nsresult aRv,
                             const nsSHistory::LoadEntryResult& aLoadResult,
                             LoadSHEntryResult* aResult) {
  if (NS_SUCCEEDED(aRv)) {
    MaybeNewPSHEntry entry;
    static_cast<LegacySHEntry*>(aLoadResult.mLoadState->SHEntry())
        ->GetOrCreateActor(aManager, entry);
    *aResult = LoadSHEntryData(std::move(entry), aLoadResult.mBrowsingContext,
                               aLoadResult.mLoadState);
  } else {
    *aResult = aRv;
  }
}

SHistoryParent::SHistoryParent(CanonicalBrowsingContext* aContext)
    : mContext(aContext), mHistory(new LegacySHistory(aContext, nsID())) {}

SHistoryParent::~SHistoryParent() {}

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
                                         MaybeNewPSHEntry* aEntry) {
  nsCOMPtr<nsISHEntry> entry;
  *aResult = mHistory->GetEntryAtIndex(aIndex, getter_AddRefs(entry));
  SHEntryParent::GetOrCreate(Manager(), entry, *aEntry);
  return true;
}

bool SHistoryParent::RecvPurgeHistory(int32_t aNumEntries, nsresult* aResult) {
  *aResult = mHistory->PurgeHistory(aNumEntries);
  return true;
}

bool SHistoryParent::RecvReloadCurrentEntry(LoadSHEntryResult* aLoadResult) {
  nsSHistory::LoadEntryResult loadResult;
  nsresult rv = mHistory->ReloadCurrentEntry(loadResult);
  if (NS_SUCCEEDED(rv)) {
    MaybeNewPSHEntry entry;
    SHEntryParent::GetOrCreate(Manager(), loadResult.mLoadState->SHEntry(),
                               entry);
    *aLoadResult = LoadSHEntryData(
        std::move(entry), loadResult.mBrowsingContext, loadResult.mLoadState);
  } else {
    *aLoadResult = rv;
  }
  return true;
}

bool SHistoryParent::RecvGotoIndex(int32_t aIndex,
                                   LoadSHEntryResult* aLoadResult) {
  nsSHistory::LoadEntryResult loadResult;
  nsresult rv = mHistory->GotoIndex(aIndex, loadResult);
  FillInLoadResult(Manager(), rv, loadResult, aLoadResult);
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
  // FIXME Implement this!
  return true;
}

bool SHistoryParent::RecvEvictAllContentViewers() {
  return NS_SUCCEEDED(mHistory->EvictAllContentViewers());
}

bool SHistoryParent::RecvRemoveDynEntries(int32_t aIndex,
                                          PSHEntryParent* aEntry) {
  MOZ_ASSERT(Manager() == aEntry->Manager());
  mHistory->RemoveDynEntries(aIndex,
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
  nsSHistory::LoadEntryResult loadResult;
  nsresult rv = mHistory->Reload(aReloadFlags, loadResult);
  FillInLoadResult(Manager(), rv, loadResult, aLoadResult);
  return true;
}

bool SHistoryParent::RecvGetAllEntries(nsTArray<MaybeNewPSHEntry>* aEntries) {
  nsTArray<nsCOMPtr<nsISHEntry>>& entries = mHistory->Entries();
  uint32_t length = entries.Length();
  aEntries->AppendElements(length);
  for (uint32_t i = 0; i < length; ++i) {
    SHEntryParent::GetOrCreate(Manager(), entries[i].get(),
                               aEntries->ElementAt(i));
  }
  return true;
}

bool SHistoryParent::RecvFindEntryForBFCache(const uint64_t& aSharedID,
                                             const bool& aIncludeCurrentEntry,
                                             MaybeNewPSHEntry* aEntry,
                                             int32_t* aIndex) {
  int32_t currentIndex;
  mHistory->GetIndex(&currentIndex);
  int32_t startSafeIndex, endSafeIndex;
  mHistory->WindowIndices(currentIndex, &startSafeIndex, &endSafeIndex);
  for (int32_t i = startSafeIndex; i <= endSafeIndex; ++i) {
    nsCOMPtr<nsISHEntry> entry;
    nsresult rv = mHistory->GetEntryAtIndex(i, getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, false);

    if (static_cast<LegacySHEntry*>(entry.get())->GetSharedStateID() ==
        aSharedID) {
      if (!aIncludeCurrentEntry && i == currentIndex) {
        *aEntry = (PSHEntryParent*)nullptr;
      } else {
        SHEntryParent::GetOrCreate(Manager(), entry, *aEntry);
      }

      return true;
    }
  }
  *aEntry = (PSHEntryParent*)nullptr;
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

}  // namespace dom
}  // namespace mozilla
