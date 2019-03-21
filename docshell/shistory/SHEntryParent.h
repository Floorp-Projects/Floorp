/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SHistoryEntry_h
#define mozilla_dom_SHistoryEntry_h

#include "mozilla/dom/PSHEntryParent.h"
#include "mozilla/WeakPtr.h"
#include "nsSHEntry.h"
#include "nsSHEntryShared.h"

namespace mozilla {
namespace dom {

class PContentParent;
class SHEntryParent;

/**
 * Implementation of the shared state for session history entries in the parent
 * process.
 */
class SHEntrySharedParent : public SHEntrySharedParentState {
 public:
  SHEntrySharedParent(PContentParent* aContentParent, uint64_t aSharedID);

  void Destroy() override;

  PContentParent* GetContentParent() { return mContentParent.get(); }

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
class LegacySHEntry final : public nsSHEntry {
 public:
  LegacySHEntry(PContentParent* aParent, uint64_t aSharedID)
      : nsSHEntry(new SHEntrySharedParent(aParent, aSharedID)),
        mActor(nullptr) {}
  explicit LegacySHEntry(const LegacySHEntry& aEntry)
      : nsSHEntry(aEntry), mActor(nullptr) {}

  void GetOrCreateActor(PContentParent* aContentParent,
                        MaybeNewPSHEntry& aEntry);

  using nsSHEntry::AbandonBFCacheEntry;
  void AbandonBFCacheEntry(uint64_t aNewSharedID);

  uint64_t GetSharedStateID() const { return mShared->GetID(); }

 private:
  friend class SHEntryParent;
  friend class ContentParent;

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

  static void GetOrCreate(PContentParent* aManager, nsISHEntry* aSHEntry,
                          MaybeNewPSHEntry& aResult);

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
  bool RecvGetParent(MaybeNewPSHEntry* aParentEntry);
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
                  nsIContentSecurityPolicy* aCsp, const nsID& aDocshellID,
                  const bool& aDynamicCreation);
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
  bool RecvGetChildAt(const int32_t& aIndex, MaybeNewPSHEntry* aChild);
  bool RecvReplaceChild(PSHEntryParent* aNewChild, nsresult* aResult);

  void GetOrCreate(nsISHEntry* aSHEntry, MaybeNewPSHEntry* aResult) {
    GetOrCreate(Manager(), aSHEntry, *aResult);
  }

  RefPtr<LegacySHEntry> mEntry;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_SHEntryParent_h */
