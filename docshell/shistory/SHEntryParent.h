/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SHistoryEntry_h
#define mozilla_dom_SHistoryEntry_h

#include "mozilla/dom/PSHEntryParent.h"
#include "mozilla/dom/MaybeNewPSHEntry.h"
#include "mozilla/WeakPtr.h"
#include "nsSHEntry.h"
#include "nsSHEntryShared.h"

namespace mozilla {
namespace dom {

class LegacySHistory;
class PContentParent;
class SHEntryParent;

/**
 * Implementation of the shared state for session history entries in the parent
 * process.
 */
class SHEntrySharedParent : public SHEntrySharedParentState {
 public:
  SHEntrySharedParent(PContentParent* aContentParent, LegacySHistory* aSHistory,
                      uint64_t aSharedID);

  already_AddRefed<SHEntrySharedParent> Duplicate(uint64_t aNewSharedID) {
    RefPtr<SHEntrySharedParent> shared =
        new SHEntrySharedParent(this, aNewSharedID);
    shared->CopyFrom(this);
    return shared.forget();
  }

  PContentParent* GetContentParent() { return mContentParent.get(); }

 protected:
  SHEntrySharedParent(SHEntrySharedParent* aDuplicate, uint64_t aSharedID)
      : SHEntrySharedParentState(aDuplicate, aSharedID),
        mContentParent(aDuplicate->mContentParent) {}

  void Destroy() override;

 private:
  mozilla::WeakPtr<PContentParent> mContentParent;
};

/**
 * Session history entry implementation based on the legacy implementation that
 * used to live in the child process. Ideally this wouldn't implement nsISHEntry
 * (it should only ever be accessed by SHEntryParent and LegacySHistory).
 * The actor is (re)created as needed, whenever we need to return an entry to
 * the child process. The lifetime is determined by the child side.
 */
class LegacySHEntry final : public nsSHEntry, public CrossProcessSHEntry {
 public:
  LegacySHEntry(PContentParent* aContentParent, LegacySHistory* aSHistory,
                uint64_t aSharedID);
  explicit LegacySHEntry(const LegacySHEntry& aEntry)
      : nsSHEntry(aEntry), mActor(nullptr) {}

  NS_DECL_ISUPPORTS_INHERITED

  MaybeNewPSHEntryParent GetOrCreateActor(PContentParent* aContentParent);

  using nsSHEntry::AbandonBFCacheEntry;
  void AbandonBFCacheEntry(uint64_t aNewSharedID);
  NS_IMETHODIMP GetBfcacheID(uint64_t* aBFCacheID) override;

  uint64_t GetSharedStateID() const { return mShared->GetID(); }

  dom::SHEntrySharedParentState* GetSharedState() const {
    return mShared.get();
  }

  NS_IMETHOD Clone(nsISHEntry** aResult) override;

 private:
  friend class SHEntryParent;
  friend class SHistoryParent;

  ~LegacySHEntry() = default;

  SHEntryParent* CreateActor();

  SHEntryParent* mActor;
};

/**
 * Session history entry actor for the parent process. Forwards to the legacy
 * implementation that used to live in the child process (see LegacySHEntry).
 */
class SHEntryParent final : public PSHEntryParent {
  friend class PSHEntryParent;
  friend class SHistoryParent;
  friend class ContentParent;

 public:
  explicit SHEntryParent(LegacySHEntry* aEntry)
      : PSHEntryParent(), mEntry(aEntry) {}

  LegacySHEntry* GetSHEntry() { return mEntry; }

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  bool RecvGetURI(RefPtr<nsIURI>* aURI);
  bool RecvSetURI(nsIURI* aURI);
  bool RecvGetOriginalURI(RefPtr<nsIURI>* aOriginalURI);
  bool RecvSetOriginalURI(nsIURI* aOriginalURI);
  bool RecvGetResultPrincipalURI(RefPtr<nsIURI>* aResultPrincipalURI);
  bool RecvSetResultPrincipalURI(nsIURI* aResultPrincipalURI);
  bool RecvGetLoadReplace(bool* aLoadReplace);
  bool RecvSetLoadReplace(const bool& aLoadReplace);
  bool RecvGetTitle(nsString* aTitle);
  bool RecvSetTitle(const nsString& aTitle);
  bool RecvGetIsSubFrame(bool* aIsSubFrame);
  bool RecvSetIsSubFrame(const bool& aIsSubFrame);
  bool RecvGetReferrerInfo(RefPtr<nsIReferrerInfo>* aReferrerInfo);
  bool RecvSetReferrerInfo(nsIReferrerInfo* aReferrerInfo);
  bool RecvGetSticky(bool* aSticky);
  bool RecvSetSticky(const bool& aSticky);
  bool RecvGetPostData(RefPtr<nsIInputStream>* aPostData);
  bool RecvSetPostData(nsIInputStream* aPostData);
  bool RecvGetParent(RefPtr<CrossProcessSHEntry>* aParentEntry);
  bool RecvSetParent(PSHEntryParent* aParentEntry);
  bool RecvGetLoadType(uint32_t* aLoadType);
  bool RecvSetLoadType(const uint32_t& aLoadType);
  bool RecvGetID(uint32_t* aID);
  bool RecvSetID(const uint32_t& aID);
  bool RecvGetCacheKey(uint32_t* aCacheKey);
  bool RecvSetCacheKey(const uint32_t& aCacheKey);
  bool RecvGetExpirationStatus(bool* aExpirationStatus);
  bool RecvSetExpirationStatus(const bool& aExpirationStatus);
  bool RecvGetContentType(nsCString* aContentType);
  bool RecvSetContentType(const nsCString& aContentType);
  bool RecvGetURIWasModified(bool* aURIWasModified);
  bool RecvSetURIWasModified(const bool& aURIWasModified);
  bool RecvGetTriggeringPrincipal(RefPtr<nsIPrincipal>* aTriggeringPrincipal);
  bool RecvSetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal);
  bool RecvGetPrincipalToInherit(RefPtr<nsIPrincipal>* aPrincipalToInherit);
  bool RecvSetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit);
  bool RecvGetStoragePrincipalToInherit(
      RefPtr<nsIPrincipal>* aStoragePrincipalToInherit);
  bool RecvSetStoragePrincipalToInherit(
      nsIPrincipal* aStoragePrincipalToInherit);
  bool RecvGetCsp(RefPtr<nsIContentSecurityPolicy>* aCsp);
  bool RecvSetCsp(nsIContentSecurityPolicy* aCsp);
  bool RecvGetStateData(ClonedMessageData* aData);
  bool RecvSetStateData(ClonedMessageData&& aData);
  bool RecvGetDocshellID(nsID* aDocshellID);
  bool RecvSetDocshellID(const nsID& aDocshellID);
  bool RecvGetIsSrcdocEntry(bool* aIsSrcdocEntry);
  bool RecvGetSrcdocData(nsString* aSrcdocData);
  bool RecvSetSrcdocData(const nsString& aSrcdocData);
  bool RecvGetBaseURI(RefPtr<nsIURI>* aBaseURI);
  bool RecvSetBaseURI(nsIURI* aBaseURI);
  bool RecvGetScrollRestorationIsManual(bool* aScrollRestorationIsManual);
  bool RecvSetScrollRestorationIsManual(const bool& aScrollRestorationIsManual);
  bool RecvGetLoadedInThisProcess(bool* aLoadedInThisProcess);
  bool RecvGetLastTouched(uint32_t* aLastTouched);
  bool RecvSetLastTouched(const uint32_t& aLastTouched);
  bool RecvGetChildCount(int32_t* aChildCount);
  bool RecvGetPersist(bool* aPersist);
  bool RecvSetPersist(const bool& aPersist);
  bool RecvGetScrollPosition(int32_t* aX, int32_t* aY);
  bool RecvSetScrollPosition(const int32_t& aX, const int32_t& aY);
  bool RecvGetViewerBounds(nsIntRect* aBounds);
  bool RecvSetViewerBounds(const nsIntRect& aBounds);
  bool RecvCreate(nsIURI* aURI, const nsString& aTitle,
                  nsIInputStream* aInputStream, const uint32_t& aCacheKey,
                  const nsCString& aContentType,
                  nsIPrincipal* aTriggeringPrincipal,
                  nsIPrincipal* aPrincipalToInherit,
                  nsIPrincipal* aStoragePrincipalToInherit,
                  nsIContentSecurityPolicy* aCsp, const nsID& aDocshellID,
                  const bool& aDynamicCreation, nsIURI* aOriginalURI,
                  nsIURI* aResultPrincipalURI, const bool& aLoadReplace,
                  nsIReferrerInfo* aReferrerInfo, const nsAString& srcdoc,
                  const bool& srcdocEntry, nsIURI* aBaseURI,
                  const bool& aSaveLayoutState, const bool& aExpired);
  bool RecvClone(PSHEntryParent** aCloneEntry);
  bool RecvHasDetachedEditor(bool* aHasDetachedEditor);
  bool RecvIsDynamicallyAdded(bool* aIsDynamicallyAdded);
  bool RecvHasDynamicallyAddedChild(bool* aHasDynamicallyAddedChild);
  bool RecvDocshellID(nsID* aDocshellID);
  bool RecvAdoptBFCacheEntry(PSHEntryParent* aEntry, nsresult* aResult);
  bool RecvAbandonBFCacheEntry(const uint64_t& aNewSharedID);
  bool RecvSharesDocumentWith(PSHEntryParent* aEntry, bool* aSharesDocumentWith,
                              nsresult* aResult);
  bool RecvSetLoadTypeAsHistory();
  bool RecvAddChild(PSHEntryParent* aChild, const int32_t& aOffset,
                    const bool& aUseRemoteSubframes, nsresult* aResult);
  bool RecvRemoveChild(PSHEntryParent* aChild, nsresult* aResult);
  bool RecvGetChildAt(const int32_t& aIndex,
                      RefPtr<CrossProcessSHEntry>* aChild);
  bool RecvGetChildSHEntryIfHasNoDynamicallyAddedChild(
      const int32_t& aChildOffset, RefPtr<CrossProcessSHEntry>* aChild);
  bool RecvReplaceChild(PSHEntryParent* aNewChild, nsresult* aResult);
  bool RecvClearEntry(const uint64_t& aNewSharedID);

  bool RecvCreateLoadInfo(RefPtr<nsDocShellLoadState>* aLoadState);
  bool RecvClone(RefPtr<CrossProcessSHEntry>* aResult);

  bool RecvSyncTreesForSubframeNavigation(
      PSHEntryParent* aSHEntry, const MaybeDiscarded<BrowsingContext>& aBC,
      const MaybeDiscarded<BrowsingContext>& aIgnoreBC,
      nsTArray<SwapEntriesDocshellData>* aEntriesToUpdate);

  bool RecvUpdateLayoutHistoryState(const bool& aScrollPositionOnly,
                                    nsTArray<nsCString>&& aKeys,
                                    nsTArray<PresState>&& aStates);

  RefPtr<LegacySHEntry> mEntry;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_SHEntryParent_h */
