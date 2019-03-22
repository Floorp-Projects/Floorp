/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SHistoryParent_h
#define mozilla_dom_SHistoryParent_h

#include "mozilla/dom/PSHistoryParent.h"
#include "mozilla/RefPtr.h"
#include "nsSHistory.h"

namespace mozilla {
namespace dom {

class CanonicalBrowsingContext;
class SHistoryParent;

/**
 * Session history implementation based on the legacy implementation that used
 * to live in the child process. Ideally this wouldn't implement nsISHistory
 * (it should only ever be accessed by SHistoryParent).
 */
class LegacySHistory final : public nsSHistory {
 private:
  virtual ~LegacySHistory() {}

 public:
  LegacySHistory(mozilla::dom::BrowsingContext* aRootBC,
                 const nsID& aDocShellID)
      : nsSHistory(aRootBC, aDocShellID) {
    mIsRemote = true;
  }
};

/**
 * Session history actor for the parent process. Forwards to the legacy
 * implementation that used to live in the child process (see LegacySHistory).
 */
class SHistoryParent final : public PSHistoryParent {
  friend class PSHistoryParent;
  friend class SHEntryParent;

 public:
  explicit SHistoryParent(CanonicalBrowsingContext* aContext);
  virtual ~SHistoryParent();

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  bool RecvGetCount(int32_t* aCount);
  bool RecvGetIndex(int32_t* aIndex);
  bool RecvSetIndex(int32_t aIndex, nsresult* aResult);
  bool RecvGetRequestedIndex(int32_t* aIndex);
  bool RecvInternalSetRequestedIndex(int32_t aIndex);
  bool RecvGetEntryAtIndex(int32_t aIndex, nsresult* aResult,
                           MaybeNewPSHEntry* aEntry);
  bool RecvPurgeHistory(int32_t aNumEntries, nsresult* aResult);
  bool RecvReloadCurrentEntry(LoadSHEntryResult* aLoadResult);
  bool RecvGotoIndex(int32_t aIndex, LoadSHEntryResult* aLoadResult);
  bool RecvGetIndexOfEntry(PSHEntryParent* aEntry, int32_t* aIndex);
  bool RecvAddEntry(PSHEntryParent* aEntry, bool aPersist, nsresult* aResult,
                    int32_t* aEntriesPurged);
  bool RecvUpdateIndex();
  bool RecvReplaceEntry(int32_t aIndex, PSHEntryParent* aEntry,
                        nsresult* aResult);
  bool RecvNotifyOnHistoryReload(bool* aOk);
  bool RecvEvictOutOfRangeContentViewers(int32_t aIndex);
  bool RecvEvictAllContentViewers();
  bool RecvRemoveDynEntries(int32_t aIndex, PSHEntryParent* aEntry);
  bool RecvRemoveEntries(nsTArray<nsID>&& ids, int32_t aIndex,
                         bool* aDidRemove);
  bool RecvRemoveFrameEntries(PSHEntryParent* aEntry);
  bool RecvReload(const uint32_t& aReloadFlags, LoadSHEntryResult* aLoadResult);
  bool RecvGetAllEntries(nsTArray<MaybeNewPSHEntry>* aEntries);
  bool RecvFindEntryForBFCache(const uint64_t& aSharedID,
                               const bool& aIncludeCurrentEntry,
                               MaybeNewPSHEntry* aEntry, int32_t* aIndex);
  bool RecvEvict(nsTArray<PSHEntryParent*>&& aEntries);

  RefPtr<CanonicalBrowsingContext> mContext;
  RefPtr<LegacySHistory> mHistory;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_SHistoryParent_h */
