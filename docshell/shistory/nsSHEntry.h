/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntry_h
#define nsSHEntry_h

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsISHEntry.h"
#include "nsString.h"

#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class SHEntrySharedChildState;
class SHEntrySharedParentState;

}  // namespace dom
}  // namespace mozilla

class nsSHEntryShared;
class nsIInputStream;
class nsIURI;
class nsIReferrerInfo;

// Structure for passing around entries via XPCOM from methods such as
// nsSHistory::CloneAndReplaceChild, nsSHistory::SetChildHistoryEntry and
// nsSHEntry::SyncTreesForSubframeNavigation, that need to swap entries in
// docshell, to corresponding Recv methods so that we don't have to create
// actors until we are about to return from the parent process
struct EntriesAndBrowsingContextData {
  nsCOMPtr<nsISHEntry> oldEntry;
  nsCOMPtr<nsISHEntry> newEntry;
  mozilla::dom::BrowsingContext* context;
};

class nsSHEntry : public nsISHEntry {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISHENTRY

  virtual void EvictContentViewer();

  static nsresult Startup();
  static void Shutdown();
  void SyncTreesForSubframeNavigation(
      uint64_t aOtherPid, nsISHEntry* aEntry,
      mozilla::dom::BrowsingContext* aTopBC,
      mozilla::dom::BrowsingContext* aIgnoreBC,
      nsTArray<EntriesAndBrowsingContextData>* aEntriesToUpdate);

 protected:
  explicit nsSHEntry(mozilla::dom::SHEntrySharedParentState* aState);
  explicit nsSHEntry(const nsSHEntry& aOther);
  virtual ~nsSHEntry();

  // We share the state in here with other SHEntries which correspond to the
  // same document.
  RefPtr<mozilla::dom::SHEntrySharedParentState> mShared;

  // See nsSHEntry.idl for comments on these members.
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  nsString mTitle;
  nsCOMPtr<nsIInputStream> mPostData;
  uint32_t mLoadType;
  uint32_t mID;
  int32_t mScrollPositionX;
  int32_t mScrollPositionY;
  nsISHEntry* mParent;
  nsCOMArray<nsISHEntry> mChildren;
  nsCOMPtr<nsIStructuredCloneContainer> mStateData;
  nsString mSrcdocData;
  nsCOMPtr<nsIURI> mBaseURI;
  bool mLoadReplace;
  bool mURIWasModified;
  bool mIsSrcdocEntry;
  bool mScrollRestorationIsManual;
  bool mLoadedInThisProcess;
  bool mPersist;
};

/**
 * Session history entry class used for implementing session history for
 * docshells in the parent process (a different solution would be to use the
 * IPC actors for that too, with both parent and child actor created in the
 * parent process).
 */
class nsLegacySHEntry final : public nsSHEntry {
 public:
  explicit nsLegacySHEntry(nsISHistory* aHistory, uint64_t aID);
  explicit nsLegacySHEntry(const nsLegacySHEntry& aOther) : nsSHEntry(aOther) {}

  NS_IMETHOD GetContentViewer(nsIContentViewer** aResult) override;
  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer) override;
  NS_IMETHOD GetWindowState(nsISupports** aState) override;
  NS_IMETHOD SetWindowState(nsISupports* aState) override;
  using nsISHEntry::GetRefreshURIList;
  NS_IMETHOD GetRefreshURIList(nsIMutableArray** aRefreshURIList) override;
  NS_IMETHOD SetRefreshURIList(nsIMutableArray* aRefreshURIList) override;
  using nsISHEntry::GetSaveLayoutStateFlag;
  NS_IMETHOD GetSaveLayoutStateFlag(bool* aSaveLayoutStateFlag) override;
  NS_IMETHOD SetSaveLayoutStateFlag(bool aSaveLayoutStateFlag) override;
  NS_IMETHOD_(void) AddChildShell(nsIDocShellTreeItem* aShell) override;
  NS_IMETHOD ChildShellAt(int32_t aIndex,
                          nsIDocShellTreeItem** aShell) override;
  NS_IMETHOD_(void) ClearChildShells() override;
  NS_IMETHOD_(void) SyncPresentationState() override;
  NS_IMETHOD Create(
      nsIURI* aURI, const nsAString& aTitle, nsIInputStream* aInputStream,
      uint32_t aCacheKey, const nsACString& aContentType,
      nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
      nsIPrincipal* aStoragePrincipalToInherit, nsIContentSecurityPolicy* aCsp,
      const nsID& aDocshellID, bool aDynamicCreation, nsIURI* aOriginalURI,
      nsIURI* aResultPrincipalURI, bool aLoadReplace,
      nsIReferrerInfo* aReferrerInfo, const nsAString& aSrcdocData,
      bool aSrcdocEntry, nsIURI* aBaseURI, bool aSaveLayoutState,
      bool aExpired) override;
  NS_IMETHOD Clone(nsISHEntry** aResult) override;
  NS_IMETHOD_(nsDocShellEditorData*) ForgetEditorData(void) override;
  NS_IMETHOD_(void) SetEditorData(nsDocShellEditorData* aData) override;
  NS_IMETHOD_(bool) HasDetachedEditor() override;
  NS_IMETHOD_(bool) HasBFCacheEntry(nsIBFCacheEntry* aEntry) override;
  NS_IMETHOD AbandonBFCacheEntry() override;
  NS_IMETHOD_(void) ClearEntry() override;
  NS_IMETHOD GetBfcacheID(uint64_t* aBFCacheID) override;

 private:
  nsSHEntryShared* GetState();
};

#endif /* nsSHEntry_h */
