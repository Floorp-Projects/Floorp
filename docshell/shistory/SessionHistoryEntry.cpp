/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionHistoryEntry.h"
#include "nsDocShellLoadState.h"
#include "nsILayoutHistoryState.h"
#include "nsSHEntryShared.h"
#include "nsStructuredCloneContainer.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

static uint64_t gNextHistoryEntryId = 0;

SessionHistoryInfo::SessionHistoryInfo(nsDocShellLoadState* aLoadState,
                                       nsIChannel* aChannel)
    : mURI(aLoadState->URI()),
      mOriginalURI(aLoadState->OriginalURI()),
      mResultPrincipalURI(aLoadState->ResultPrincipalURI()),
      mReferrerInfo(aLoadState->GetReferrerInfo()),
      mPostData(aLoadState->PostDataStream()),
      mLoadType(aLoadState->LoadType()),
      mScrollPositionX(0),
      mScrollPositionY(0),
      mSrcdocData(aLoadState->SrcdocData()),
      mBaseURI(aLoadState->BaseURI()),
      mId(++gNextHistoryEntryId),
      mLoadReplace(aLoadState->LoadReplace()),
      mURIWasModified(false),
      /* FIXME Should this be aLoadState->IsSrcdocLoad()? */
      mIsSrcdocEntry(!aLoadState->SrcdocData().IsEmpty()),
      mScrollRestorationIsManual(false) {
  bool isNoStore = false;
  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel)) {
    Unused << httpChannel->IsNoStoreResponse(&isNoStore);
    mPersist = !isNoStore;
  }
}

static uint32_t gEntryID;
nsDataHashtable<nsUint64HashKey, SessionHistoryEntry*>*
    SessionHistoryEntry::sInfoIdToEntry = nullptr;

SessionHistoryEntry* SessionHistoryEntry::GetByInfoId(uint64_t aId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!sInfoIdToEntry) {
    return nullptr;
  }

  return sInfoIdToEntry->Get(aId);
}

SessionHistoryEntry::SessionHistoryEntry(nsDocShellLoadState* aLoadState,
                                         nsIChannel* aChannel)
    : mInfo(new SessionHistoryInfo(aLoadState, aChannel)),
      mSharedInfo(new SHEntrySharedParentState()),
      mID(++gEntryID) {
  mSharedInfo->mTriggeringPrincipal = aLoadState->TriggeringPrincipal();
  mSharedInfo->mPrincipalToInherit = aLoadState->PrincipalToInherit();
  mSharedInfo->mPartitionedPrincipalToInherit =
      aLoadState->PartitionedPrincipalToInherit();
  mSharedInfo->mCsp = aLoadState->Csp();
  // FIXME Set remaining shared fields!

  if (!sInfoIdToEntry) {
    sInfoIdToEntry =
        new nsDataHashtable<nsUint64HashKey, SessionHistoryEntry*>();
  }
  sInfoIdToEntry->Put(Info().Id(), this);
}

SessionHistoryEntry::~SessionHistoryEntry() {
  sInfoIdToEntry->Remove(Info().Id());
  if (sInfoIdToEntry->IsEmpty()) {
    delete sInfoIdToEntry;
    sInfoIdToEntry = nullptr;
  }
}

NS_IMPL_ISUPPORTS(SessionHistoryEntry, nsISHEntry)

NS_IMETHODIMP
SessionHistoryEntry::GetURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> uri = mInfo->mURI;
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetURI(nsIURI* aURI) {
  mInfo->mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetOriginalURI(nsIURI** aOriginalURI) {
  nsCOMPtr<nsIURI> originalURI = mInfo->mOriginalURI;
  originalURI.forget(aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetOriginalURI(nsIURI* aOriginalURI) {
  mInfo->mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetResultPrincipalURI(nsIURI** aResultPrincipalURI) {
  nsCOMPtr<nsIURI> resultPrincipalURI = mInfo->mResultPrincipalURI;
  resultPrincipalURI.forget(aResultPrincipalURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetResultPrincipalURI(nsIURI* aResultPrincipalURI) {
  mInfo->mResultPrincipalURI = aResultPrincipalURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLoadReplace(bool* aLoadReplace) {
  *aLoadReplace = mInfo->mLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLoadReplace(bool aLoadReplace) {
  mInfo->mLoadReplace = aLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetTitle(nsAString& aTitle) {
  aTitle = mInfo->mTitle;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetTitle(const nsAString& aTitle) {
  mInfo->mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetIsSubFrame(bool* aIsSubFrame) {
  *aIsSubFrame = mSharedInfo->mIsFrameNavigation;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetIsSubFrame(bool aIsSubFrame) {
  mSharedInfo->mIsFrameNavigation = aIsSubFrame;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetHasUserInteraction(bool* aFlag) {
  NS_WARNING("Not implemented in the parent process!");
  *aFlag = true;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetHasUserInteraction(bool aFlag) {
  NS_WARNING("Not implemented in the parent process!");
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  nsCOMPtr<nsIReferrerInfo> referrerInfo = mInfo->mReferrerInfo;
  referrerInfo.forget(aReferrerInfo);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  mInfo->mReferrerInfo = aReferrerInfo;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetContentViewer(nsIContentViewer** aContentViewer) {
  NS_WARNING("This lives in the child process");
  *aContentViewer = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetContentViewer(nsIContentViewer* aContentViewer) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::GetSticky(bool* aSticky) {
  *aSticky = mSharedInfo->mSticky;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetSticky(bool aSticky) {
  mSharedInfo->mSticky = aSticky;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetWindowState(nsISupports** aWindowState) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::SetWindowState(nsISupports* aWindowState) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::GetRefreshURIList(nsIMutableArray** aRefreshURIList) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::SetRefreshURIList(nsIMutableArray* aRefreshURIList) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPostData(nsIInputStream** aPostData) {
  nsCOMPtr<nsIInputStream> postData = mInfo->mPostData;
  postData.forget(aPostData);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPostData(nsIInputStream* aPostData) {
  mInfo->mPostData = aPostData;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLayoutHistoryState(
    nsILayoutHistoryState** aLayoutHistoryState) {
  nsCOMPtr<nsILayoutHistoryState> layoutHistoryState =
      mSharedInfo->mLayoutHistoryState;
  layoutHistoryState.forget(aLayoutHistoryState);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLayoutHistoryState(
    nsILayoutHistoryState* aLayoutHistoryState) {
  mSharedInfo->mLayoutHistoryState = aLayoutHistoryState;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetParent(nsISHEntry** aParent) {
  nsCOMPtr<nsISHEntry> parent = mParent;
  parent.forget(aParent);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetParent(nsISHEntry* aParent) {
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLoadType(uint32_t* aLoadType) {
  *aLoadType = mInfo->mLoadType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLoadType(uint32_t aLoadType) {
  mInfo->mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetID(uint32_t* aID) {
  *aID = mID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetID(uint32_t aID) {
  mID = aID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetCacheKey(uint32_t* aCacheKey) {
  *aCacheKey = mSharedInfo->mCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetCacheKey(uint32_t aCacheKey) {
  mSharedInfo->mCacheKey = aCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetSaveLayoutStateFlag(bool* aSaveLayoutStateFlag) {
  *aSaveLayoutStateFlag = mSharedInfo->mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetSaveLayoutStateFlag(bool aSaveLayoutStateFlag) {
  mSharedInfo->mSaveLayoutState = aSaveLayoutStateFlag;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetContentType(nsACString& aContentType) {
  aContentType = mSharedInfo->mContentType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetContentType(const nsACString& aContentType) {
  mSharedInfo->mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetURIWasModified(bool* aURIWasModified) {
  *aURIWasModified = mInfo->mURIWasModified;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetURIWasModified(bool aURIWasModified) {
  mInfo->mURIWasModified = aURIWasModified;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetTriggeringPrincipal(
    nsIPrincipal** aTriggeringPrincipal) {
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
      mSharedInfo->mTriggeringPrincipal;
  triggeringPrincipal.forget(aTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetTriggeringPrincipal(
    nsIPrincipal* aTriggeringPrincipal) {
  mSharedInfo->mTriggeringPrincipal = aTriggeringPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPrincipalToInherit(nsIPrincipal** aPrincipalToInherit) {
  nsCOMPtr<nsIPrincipal> principalToInherit = mSharedInfo->mPrincipalToInherit;
  principalToInherit.forget(aPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit) {
  mSharedInfo->mPrincipalToInherit = aPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPartitionedPrincipalToInherit(
    nsIPrincipal** aPartitionedPrincipalToInherit) {
  nsCOMPtr<nsIPrincipal> partitionedPrincipalToInherit =
      mSharedInfo->mPartitionedPrincipalToInherit;
  partitionedPrincipalToInherit.forget(aPartitionedPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPartitionedPrincipalToInherit(
    nsIPrincipal* aPartitionedPrincipalToInherit) {
  mSharedInfo->mPartitionedPrincipalToInherit = aPartitionedPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetCsp(nsIContentSecurityPolicy** aCsp) {
  nsCOMPtr<nsIContentSecurityPolicy> csp = mSharedInfo->mCsp;
  csp.forget(aCsp);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetCsp(nsIContentSecurityPolicy* aCsp) {
  mSharedInfo->mCsp = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetStateData(nsIStructuredCloneContainer** aStateData) {
  RefPtr<nsStructuredCloneContainer> stateData = mInfo->mStateData;
  stateData.forget(aStateData);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetStateData(nsIStructuredCloneContainer* aStateData) {
  mInfo->mStateData = static_cast<nsStructuredCloneContainer*>(aStateData);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetDocshellID(nsID& aDocshellID) {
  aDocshellID = mSharedInfo->mDocShellID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetDocshellID(const nsID& aDocshellID) {
  mSharedInfo->mDocShellID = aDocshellID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetIsSrcdocEntry(bool* aIsSrcdocEntry) {
  *aIsSrcdocEntry = mInfo->mIsSrcdocEntry;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetSrcdocData(nsAString& aSrcdocData) {
  aSrcdocData = mInfo->mSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetSrcdocData(const nsAString& aSrcdocData) {
  mInfo->mSrcdocData = aSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetBaseURI(nsIURI** aBaseURI) {
  nsCOMPtr<nsIURI> baseURI = mInfo->mBaseURI;
  baseURI.forget(aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetBaseURI(nsIURI* aBaseURI) {
  mInfo->mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetScrollRestorationIsManual(
    bool* aScrollRestorationIsManual) {
  *aScrollRestorationIsManual = mInfo->mScrollRestorationIsManual;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetScrollRestorationIsManual(
    bool aScrollRestorationIsManual) {
  mInfo->mScrollRestorationIsManual = aScrollRestorationIsManual;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLoadedInThisProcess(bool* aLoadedInThisProcess) {
  // FIXME
  //*aLoadedInThisProcess = mInfo->mLoadedInThisProcess;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetShistory(nsISHistory** aShistory) {
  nsCOMPtr<nsISHistory> sHistory = do_QueryReferent(mSharedInfo->mSHistory);
  sHistory.forget(aShistory);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetShistory(nsISHistory* aShistory) {
  nsWeakPtr shistory = do_GetWeakReference(aShistory);
  // mSHistory can not be changed once it's set
  MOZ_ASSERT(!mSharedInfo->mSHistory || (mSharedInfo->mSHistory == shistory));
  mSharedInfo->mSHistory = shistory;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLastTouched(uint32_t* aLastTouched) {
  *aLastTouched = mSharedInfo->mLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLastTouched(uint32_t aLastTouched) {
  mSharedInfo->mLastTouched = aLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetChildCount(int32_t* aChildCount) {
  *aChildCount = mChildren.Length();
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPersist(bool* aPersist) {
  *aPersist = mInfo->mPersist;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPersist(bool aPersist) {
  mInfo->mPersist = aPersist;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetScrollPosition(int32_t* aX, int32_t* aY) {
  *aX = mInfo->mScrollPositionX;
  *aY = mInfo->mScrollPositionY;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetScrollPosition(int32_t aX, int32_t aY) {
  mInfo->mScrollPositionX = aX;
  mInfo->mScrollPositionY = aY;
  return NS_OK;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::GetViewerBounds(nsIntRect& bounds) {
  bounds = mSharedInfo->mViewerBounds;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SetViewerBounds(const nsIntRect& bounds) {
  mSharedInfo->mViewerBounds = bounds;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::AddChildShell(nsIDocShellTreeItem* shell) {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP
SessionHistoryEntry::ChildShellAt(int32_t index,
                                  nsIDocShellTreeItem** _retval) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::ClearChildShells() {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SyncPresentationState() {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP
SessionHistoryEntry::InitLayoutHistoryState(
    nsILayoutHistoryState** aLayoutHistoryState) {
  if (!mSharedInfo->mLayoutHistoryState) {
    nsCOMPtr<nsILayoutHistoryState> historyState;
    historyState = NS_NewLayoutHistoryState();
    SetLayoutHistoryState(historyState);
  }

  return GetLayoutHistoryState(aLayoutHistoryState);
}

NS_IMETHODIMP
SessionHistoryEntry::Create(
    nsIURI* aURI, const nsAString& aTitle, nsIInputStream* aInputStream,
    uint32_t aCacheKey, const nsACString& aContentType,
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, const nsID& aDocshellID,
    bool aDynamicCreation, nsIURI* aOriginalURI, nsIURI* aResultPrincipalURI,
    bool aLoadReplace, nsIReferrerInfo* aReferrerInfo, const nsAString& aSrcdoc,
    bool aSrcdocEntry, nsIURI* aBaseURI, bool aSaveLayoutState, bool aExpired) {
  MOZ_CRASH("Might need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SessionHistoryEntry::Clone(nsISHEntry** _retval) {
  MOZ_CRASH("Might need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(nsDocShellEditorData*)
SessionHistoryEntry::ForgetEditorData() {
  MOZ_CRASH("This lives in the child process");
  return nullptr;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SetEditorData(nsDocShellEditorData* aData) {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP_(bool)
SessionHistoryEntry::HasDetachedEditor() {
  MOZ_CRASH("This lives in the child process");
  return false;
}

NS_IMETHODIMP_(bool)
SessionHistoryEntry::IsDynamicallyAdded() {
  return mSharedInfo->mDynamicallyCreated;
}

NS_IMETHODIMP
SessionHistoryEntry::HasDynamicallyAddedChild(bool* aHasDynamicallyAddedChild) {
  for (const auto& child : mChildren) {
    if (child->IsDynamicallyAdded()) {
      *aHasDynamicallyAddedChild = true;
      return NS_OK;
    }
  }
  *aHasDynamicallyAddedChild = false;
  return NS_OK;
}

NS_IMETHODIMP_(bool)
SessionHistoryEntry::HasBFCacheEntry(nsIBFCacheEntry* aEntry) {
  MOZ_CRASH("This lives in the child process");
  return false;
}

NS_IMETHODIMP
SessionHistoryEntry::AdoptBFCacheEntry(nsISHEntry* aEntry) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::AbandonBFCacheEntry() {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::SharesDocumentWith(nsISHEntry* aEntry,
                                        bool* aSharesDocumentWith) {
  MOZ_CRASH("Might need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLoadTypeAsHistory() {
  mInfo->mLoadType = LOAD_HISTORY;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::AddChild(nsISHEntry* aChild, int32_t aOffset,
                              bool aUseRemoteSubframes) {
  MOZ_CRASH("Need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SessionHistoryEntry::RemoveChild(nsISHEntry* aChild) {
  MOZ_CRASH("Need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SessionHistoryEntry::GetChildAt(int32_t aIndex, nsISHEntry** aChild) {
  nsCOMPtr<nsISHEntry> child = mChildren.SafeElementAt(aIndex);
  child.forget(aChild);
  return NS_OK;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::GetChildSHEntryIfHasNoDynamicallyAddedChild(
    int32_t aChildOffset, nsISHEntry** aChild) {
  MOZ_CRASH("Need to implement this");
}

NS_IMETHODIMP
SessionHistoryEntry::ReplaceChild(nsISHEntry* aNewChild) {
  MOZ_CRASH("Need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::ClearEntry() {
  int32_t childCount = GetChildCount();
  // Remove all children of this entry
  for (int32_t i = childCount; i > 0; --i) {
    nsCOMPtr<nsISHEntry> child;
    GetChildAt(i - 1, getter_AddRefs(child));
    RemoveChild(child);
  }
}

NS_IMETHODIMP
SessionHistoryEntry::CreateLoadInfo(nsDocShellLoadState** aLoadState) {
  nsCOMPtr<nsIURI> uri = GetURI();
  RefPtr<nsDocShellLoadState> loadState(new nsDocShellLoadState(mInfo->mURI));

  loadState->SetOriginalURI(mInfo->mOriginalURI);
  loadState->SetMaybeResultPrincipalURI(Some(mInfo->mResultPrincipalURI));
  loadState->SetLoadReplace(mInfo->mLoadReplace);
  loadState->SetPostDataStream(mInfo->mPostData);
  loadState->SetReferrerInfo(mInfo->mReferrerInfo);

  loadState->SetTypeHint(mSharedInfo->mContentType);
  loadState->SetTriggeringPrincipal(mSharedInfo->mTriggeringPrincipal);
  loadState->SetPrincipalToInherit(mSharedInfo->mPrincipalToInherit);
  loadState->SetPartitionedPrincipalToInherit(
      mSharedInfo->mPartitionedPrincipalToInherit);
  loadState->SetCsp(mSharedInfo->mCsp);

  // Do not inherit principal from document (security-critical!);
  uint32_t flags = nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_NONE;

  // Passing nullptr as aSourceDocShell gives the same behaviour as before
  // aSourceDocShell was introduced. According to spec we should be passing
  // the source browsing context that was used when the history entry was
  // first created. bug 947716 has been created to address this issue.
  nsAutoString srcdoc;
  nsCOMPtr<nsIURI> baseURI;
  if (mInfo->mIsSrcdocEntry) {
    srcdoc = mInfo->mSrcdocData;
    baseURI = mInfo->mBaseURI;
    flags |= nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  } else {
    srcdoc = VoidString();
  }
  loadState->SetSrcdocData(srcdoc);
  loadState->SetBaseURI(baseURI);
  loadState->SetLoadFlags(flags);

  loadState->SetFirstParty(true);
  loadState->SetSHEntry(this);

  loadState.forget(aLoadState);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetBfcacheID(uint64_t* aBfcacheID) {
  *aBfcacheID = mSharedInfo->mID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SynchronizeLayoutHistoryState() {
  // No-op on purpose. See nsISHEntry.idl
  return NS_OK;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SyncTreesForSubframeNavigation(
    nsISHEntry* aEntry, mozilla::dom::BrowsingContext* aTopBC,
    mozilla::dom::BrowsingContext* aIgnoreBC) {
  MOZ_CRASH("Need to implement this");
}

}  // namespace dom

namespace ipc {

void IPDLParamTraits<dom::SessionHistoryInfo>::Write(
    IPC::Message* aMsg, IProtocol* aActor,
    const dom::SessionHistoryInfo& aParam) {
  dom::ClonedMessageData stateData;
  if (aParam.mStateData) {
    JSStructuredCloneData& data = aParam.mStateData->Data();
    auto iter = data.Start();
    bool success;
    stateData.data().data = data.Borrow(iter, data.Size(), &success);
    if (NS_WARN_IF(!success)) {
      return;
    }
    MOZ_ASSERT(aParam.mStateData->PortIdentifiers().IsEmpty() &&
               aParam.mStateData->BlobImpls().IsEmpty() &&
               aParam.mStateData->InputStreams().IsEmpty());
  }

  WriteIPDLParam(aMsg, aActor, aParam.mURI);
  WriteIPDLParam(aMsg, aActor, aParam.mOriginalURI);
  WriteIPDLParam(aMsg, aActor, aParam.mResultPrincipalURI);
  WriteIPDLParam(aMsg, aActor, aParam.mReferrerInfo);
  WriteIPDLParam(aMsg, aActor, aParam.mTitle);
  WriteIPDLParam(aMsg, aActor, aParam.mPostData);
  WriteIPDLParam(aMsg, aActor, aParam.mLoadType);
  WriteIPDLParam(aMsg, aActor, aParam.mScrollPositionX);
  WriteIPDLParam(aMsg, aActor, aParam.mScrollPositionY);
  WriteIPDLParam(aMsg, aActor, stateData);
  WriteIPDLParam(aMsg, aActor, aParam.mSrcdocData);
  WriteIPDLParam(aMsg, aActor, aParam.mBaseURI);
  WriteIPDLParam(aMsg, aActor, aParam.mId);
  WriteIPDLParam(aMsg, aActor, aParam.mLoadReplace);
  WriteIPDLParam(aMsg, aActor, aParam.mURIWasModified);
  WriteIPDLParam(aMsg, aActor, aParam.mIsSrcdocEntry);
  WriteIPDLParam(aMsg, aActor, aParam.mScrollRestorationIsManual);
  WriteIPDLParam(aMsg, aActor, aParam.mPersist);
}

bool IPDLParamTraits<dom::SessionHistoryInfo>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    dom::SessionHistoryInfo* aResult) {
  dom::ClonedMessageData stateData;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mOriginalURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResultPrincipalURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mReferrerInfo) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mTitle) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mPostData) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLoadType) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollPositionX) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollPositionY) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &stateData) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSrcdocData) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mBaseURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mId) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLoadReplace) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mURIWasModified) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mIsSrcdocEntry) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &aResult->mScrollRestorationIsManual) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mPersist)) {
    aActor->FatalError("Error reading fields for SessionHistoryInfo");
    return false;
  }
  aResult->mStateData = new nsStructuredCloneContainer();
  if (aActor->GetSide() == ChildSide) {
    UnpackClonedMessageDataForChild(stateData, *aResult->mStateData);
  } else {
    UnpackClonedMessageDataForParent(stateData, *aResult->mStateData);
  }
  return true;
}

}  // namespace ipc

}  // namespace mozilla
