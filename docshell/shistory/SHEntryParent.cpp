/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SHEntryParent.h"
#include "SHistoryParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/MaybeNewPSHEntry.h"
#include "nsILayoutHistoryState.h"
#include "nsStructuredCloneContainer.h"

namespace mozilla {
namespace dom {

SHEntrySharedParent::SHEntrySharedParent(PContentParent* aContentParent,
                                         LegacySHistory* aSHistory,
                                         uint64_t aSharedID)
    : SHEntrySharedParentState(aSHistory, aSharedID),
      mContentParent(aContentParent) {}

void SHEntrySharedParent::Destroy() {
  if (mContentParent &&
      !static_cast<ContentParent*>(mContentParent.get())->IsDestroyed()) {
    Unused << mContentParent->SendDestroySHEntrySharedState(mID);
  }
  SHEntrySharedParentState::Destroy();
}

LegacySHEntry::LegacySHEntry(PContentParent* aContentParent,
                             LegacySHistory* aSHistory, uint64_t aSharedID)
    : nsSHEntry(new SHEntrySharedParent(aContentParent, aSHistory, aSharedID)),
      mActor(nullptr) {}

NS_IMPL_ISUPPORTS_INHERITED0(LegacySHEntry, nsSHEntry)

SHEntryParent* LegacySHEntry::CreateActor() {
  MOZ_ASSERT(!mActor);
  mActor = new SHEntryParent(this);
  return mActor;
}

MaybeNewPSHEntryParent LegacySHEntry::GetOrCreateActor(
    PContentParent* aContentParent) {
  if (mActor) {
    return AsVariant(static_cast<PSHEntryParent*>(mActor));
  }

  nsCOMPtr<nsISHistory> shistory = do_QueryReferent(mShared->mSHistory);
  return AsVariant(
      NewPSHEntry{aContentParent->OpenPSHEntryEndpoint(CreateActor()),
                  static_cast<LegacySHistory*>(shistory.get())->GetActor(),
                  nullptr, mShared->mID});
}

void LegacySHEntry::AbandonBFCacheEntry(uint64_t aNewSharedID) {
  mShared =
      static_cast<SHEntrySharedParent*>(mShared.get())->Duplicate(aNewSharedID);
}

NS_IMETHODIMP
LegacySHEntry::GetBfcacheID(uint64_t* aBFCacheID) {
  *aBFCacheID = mShared->GetID();
  return NS_OK;
}

NS_IMETHODIMP
LegacySHEntry::Clone(nsISHEntry** aResult) {
  nsCOMPtr<nsISHEntry> entry = new LegacySHEntry(*this);
  entry.forget(aResult);
  return NS_OK;
}

void SHEntryParent::ActorDestroy(ActorDestroyReason aWhy) {
  mEntry->mActor = nullptr;
}

bool SHEntryParent::RecvGetURI(RefPtr<nsIURI>* aURI) {
  *aURI = mEntry->GetURI();
  return true;
}

bool SHEntryParent::RecvSetURI(nsIURI* aURI) {
  DebugOnly<nsresult> rv = mEntry->SetURI(aURI);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetOriginalURI(RefPtr<nsIURI>* aOriginalURI) {
  *aOriginalURI = mEntry->GetOriginalURI();
  return true;
}

bool SHEntryParent::RecvSetOriginalURI(nsIURI* aOriginalURI) {
  DebugOnly<nsresult> rv = mEntry->SetOriginalURI(aOriginalURI);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetResultPrincipalURI(
    RefPtr<nsIURI>* aResultPrincipalURI) {
  *aResultPrincipalURI = mEntry->GetResultPrincipalURI();
  return true;
}

bool SHEntryParent::RecvSetResultPrincipalURI(nsIURI* aResultPrincipalURI) {
  DebugOnly<nsresult> rv = mEntry->SetResultPrincipalURI(aResultPrincipalURI);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetLoadReplace(bool* aLoadReplace) {
  *aLoadReplace = mEntry->GetLoadReplace();
  return true;
}

bool SHEntryParent::RecvSetLoadReplace(const bool& aLoadReplace) {
  DebugOnly<nsresult> rv = mEntry->SetLoadReplace(aLoadReplace);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetTitle(nsString* aTitle) {
  DebugOnly<nsresult> rv = mEntry->GetTitle(*aTitle);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvSetTitle(const nsString& aTitle) {
  DebugOnly<nsresult> rv = mEntry->SetTitle(aTitle);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetIsSubFrame(bool* aIsSubFrame) {
  *aIsSubFrame = mEntry->GetIsSubFrame();
  return true;
}

bool SHEntryParent::RecvSetIsSubFrame(const bool& aIsSubFrame) {
  DebugOnly<nsresult> rv = mEntry->SetIsSubFrame(aIsSubFrame);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetReferrerInfo(
    RefPtr<nsIReferrerInfo>* aReferrerInfo) {
  *aReferrerInfo = mEntry->GetReferrerInfo();
  return true;
}

bool SHEntryParent::RecvSetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  DebugOnly<nsresult> rv = mEntry->SetReferrerInfo(aReferrerInfo);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetSticky(bool* aSticky) {
  *aSticky = mEntry->GetSticky();
  return true;
}

bool SHEntryParent::RecvSetSticky(const bool& aSticky) {
  DebugOnly<nsresult> rv = mEntry->SetSticky(aSticky);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetPostData(RefPtr<nsIInputStream>* aPostData) {
  *aPostData = mEntry->GetPostData();
  return true;
}

bool SHEntryParent::RecvSetPostData(nsIInputStream* aPostData) {
  DebugOnly<nsresult> rv = mEntry->SetPostData(aPostData);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetParent(RefPtr<CrossProcessSHEntry>* aParentEntry) {
  nsCOMPtr<nsISHEntry> parent = mEntry->GetParent();
  *aParentEntry = parent.forget().downcast<LegacySHEntry>();
  return true;
}

bool SHEntryParent::RecvSetParent(PSHEntryParent* aParentEntry) {
  DebugOnly<nsresult> rv = mEntry->SetParent(
      aParentEntry ? static_cast<SHEntryParent*>(aParentEntry)->mEntry.get()
                   : nullptr);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetLoadType(uint32_t* aLoadType) {
  *aLoadType = mEntry->GetLoadType();
  return true;
}

bool SHEntryParent::RecvSetLoadType(const uint32_t& aLoadType) {
  DebugOnly<nsresult> rv = mEntry->SetLoadType(aLoadType);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetID(uint32_t* aID) {
  *aID = mEntry->GetID();
  return true;
}

bool SHEntryParent::RecvSetID(const uint32_t& aID) {
  DebugOnly<nsresult> rv = mEntry->SetID(aID);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetCacheKey(uint32_t* aCacheKey) {
  *aCacheKey = mEntry->GetCacheKey();
  return true;
}

bool SHEntryParent::RecvSetCacheKey(const uint32_t& aCacheKey) {
  DebugOnly<nsresult> rv = mEntry->SetCacheKey(aCacheKey);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetExpirationStatus(bool* aExpirationStatus) {
  *aExpirationStatus = mEntry->GetExpirationStatus();
  return true;
}

bool SHEntryParent::RecvSetExpirationStatus(const bool& aExpirationStatus) {
  DebugOnly<nsresult> rv = mEntry->SetExpirationStatus(aExpirationStatus);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetContentType(nsCString* aContentType) {
  DebugOnly<nsresult> rv = mEntry->GetContentType(*aContentType);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvSetContentType(const nsCString& aContentType) {
  DebugOnly<nsresult> rv = mEntry->SetContentType(aContentType);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetURIWasModified(bool* aURIWasModified) {
  *aURIWasModified = mEntry->GetURIWasModified();
  return true;
}

bool SHEntryParent::RecvSetURIWasModified(const bool& aURIWasModified) {
  DebugOnly<nsresult> rv = mEntry->SetURIWasModified(aURIWasModified);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetTriggeringPrincipal(
    RefPtr<nsIPrincipal>* aTriggeringPrincipal) {
  *aTriggeringPrincipal = mEntry->GetTriggeringPrincipal();
  return true;
}

bool SHEntryParent::RecvSetTriggeringPrincipal(
    nsIPrincipal* aTriggeringPrincipal) {
  DebugOnly<nsresult> rv = mEntry->SetTriggeringPrincipal(aTriggeringPrincipal);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetPrincipalToInherit(
    RefPtr<nsIPrincipal>* aPrincipalToInherit) {
  *aPrincipalToInherit = mEntry->GetPrincipalToInherit();
  return true;
}

bool SHEntryParent::RecvSetPrincipalToInherit(
    nsIPrincipal* aPrincipalToInherit) {
  DebugOnly<nsresult> rv = mEntry->SetPrincipalToInherit(aPrincipalToInherit);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetStoragePrincipalToInherit(
    RefPtr<nsIPrincipal>* aStoragePrincipalToInherit) {
  *aStoragePrincipalToInherit = mEntry->GetStoragePrincipalToInherit();
  return true;
}

bool SHEntryParent::RecvSetStoragePrincipalToInherit(
    nsIPrincipal* aStoragePrincipalToInherit) {
  DebugOnly<nsresult> rv =
      mEntry->SetStoragePrincipalToInherit(aStoragePrincipalToInherit);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetCsp(RefPtr<nsIContentSecurityPolicy>* aCsp) {
  *aCsp = mEntry->GetCsp();
  return true;
}

bool SHEntryParent::RecvSetCsp(nsIContentSecurityPolicy* aCsp) {
  DebugOnly<nsresult> rv = mEntry->SetCsp(aCsp);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetStateData(ClonedMessageData* aData) {
  nsCOMPtr<nsIStructuredCloneContainer> container = mEntry->GetStateData();
  if (container) {
    static_cast<nsStructuredCloneContainer*>(container.get())
        ->BuildClonedMessageDataForParent(
            static_cast<ContentParent*>(ToplevelProtocol()), *aData);
  }
  return true;
}

bool SHEntryParent::RecvSetStateData(ClonedMessageData&& aData) {
  // FIXME Need more data! Should we signal null separately from the
  //       ClonedMessageData?
  if (aData.data().data.Size() == 0) {
    mEntry->SetStateData(nullptr);
  } else {
    RefPtr<nsStructuredCloneContainer> container =
        new nsStructuredCloneContainer();
    container->StealFromClonedMessageDataForParent(aData);
    mEntry->SetStateData(container);
  }
  return true;
}

bool SHEntryParent::RecvGetDocshellID(nsID* aDocshellID) {
  DebugOnly<nsresult> rv = mEntry->GetDocshellID(*aDocshellID);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");

  return true;
}

bool SHEntryParent::RecvSetDocshellID(const nsID& aDocshellID) {
  DebugOnly<nsresult> rv = mEntry->SetDocshellID(aDocshellID);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetIsSrcdocEntry(bool* aIsSrcdocEntry) {
  *aIsSrcdocEntry = mEntry->GetIsSrcdocEntry();
  return true;
}

bool SHEntryParent::RecvGetSrcdocData(nsString* aSrcdocData) {
  DebugOnly<nsresult> rv = mEntry->GetSrcdocData(*aSrcdocData);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvSetSrcdocData(const nsString& aSrcdocData) {
  DebugOnly<nsresult> rv = mEntry->SetSrcdocData(aSrcdocData);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetBaseURI(RefPtr<nsIURI>* aBaseURI) {
  *aBaseURI = mEntry->GetBaseURI();
  return true;
}

bool SHEntryParent::RecvSetBaseURI(nsIURI* aBaseURI) {
  DebugOnly<nsresult> rv = mEntry->SetBaseURI(aBaseURI);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetScrollRestorationIsManual(
    bool* aScrollRestorationIsManual) {
  DebugOnly<nsresult> rv =
      mEntry->GetScrollRestorationIsManual(aScrollRestorationIsManual);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvSetScrollRestorationIsManual(
    const bool& aScrollRestorationIsManual) {
  DebugOnly<nsresult> rv =
      mEntry->SetScrollRestorationIsManual(aScrollRestorationIsManual);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetLoadedInThisProcess(bool* aLoadedInThisProcess) {
  *aLoadedInThisProcess = mEntry->GetLoadedInThisProcess();
  return true;
}

bool SHEntryParent::RecvGetLastTouched(uint32_t* aLastTouched) {
  *aLastTouched = mEntry->GetLastTouched();
  return true;
}

bool SHEntryParent::RecvSetLastTouched(const uint32_t& aLastTouched) {
  DebugOnly<nsresult> rv = mEntry->SetLastTouched(aLastTouched);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetChildCount(int32_t* aChildCount) {
  *aChildCount = mEntry->GetChildCount();
  return true;
}

bool SHEntryParent::RecvGetPersist(bool* aPersist) {
  *aPersist = mEntry->GetPersist();
  return true;
}

bool SHEntryParent::RecvSetPersist(const bool& aPersist) {
  DebugOnly<nsresult> rv = mEntry->SetPersist(aPersist);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetScrollPosition(int32_t* aX, int32_t* aY) {
  DebugOnly<nsresult> rv = mEntry->GetScrollPosition(aX, aY);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvSetScrollPosition(const int32_t& aX,
                                          const int32_t& aY) {
  DebugOnly<nsresult> rv = mEntry->SetScrollPosition(aX, aY);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvGetViewerBounds(nsIntRect* aBounds) {
  mEntry->GetViewerBounds(*aBounds);
  return true;
}

bool SHEntryParent::RecvSetViewerBounds(const nsIntRect& aBounds) {
  mEntry->SetViewerBounds(aBounds);
  return true;
}

bool SHEntryParent::RecvCreate(
    nsIURI* aURI, const nsString& aTitle, nsIInputStream* aInputStream,
    const uint32_t& aCacheKey, const nsCString& aContentType,
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aStoragePrincipalToInherit, nsIContentSecurityPolicy* aCsp,
    const nsID& aDocshellID, const bool& aDynamicCreation, nsIURI* aOriginalURI,
    nsIURI* aResultPrincipalURI, const bool& aLoadReplace,
    nsIReferrerInfo* aReferrerInfo, const nsAString& srcdoc,
    const bool& srcdocEntry, nsIURI* aBaseURI, const bool& aSaveLayoutState,
    const bool& aExpired) {
  DebugOnly<nsresult> rv = mEntry->Create(
      aURI, aTitle, aInputStream, aCacheKey, aContentType, aTriggeringPrincipal,
      aPrincipalToInherit, aStoragePrincipalToInherit, aCsp, aDocshellID,
      aDynamicCreation, aOriginalURI, aResultPrincipalURI, aLoadReplace,
      aReferrerInfo, srcdoc, srcdocEntry, aBaseURI, aSaveLayoutState, aExpired);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvHasDetachedEditor(bool* aHasDetachedEditor) {
  *aHasDetachedEditor = mEntry->HasDetachedEditor();
  return true;
}

bool SHEntryParent::RecvIsDynamicallyAdded(bool* aIsDynamicallyAdded) {
  *aIsDynamicallyAdded = mEntry->IsDynamicallyAdded();
  return true;
}

bool SHEntryParent::RecvHasDynamicallyAddedChild(
    bool* aHasDynamicallyAddedChild) {
  DebugOnly<nsresult> rv =
      mEntry->HasDynamicallyAddedChild(aHasDynamicallyAddedChild);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvDocshellID(nsID* aDocshellID) {
  mEntry->GetDocshellID(*aDocshellID);
  return true;
}

bool SHEntryParent::RecvAdoptBFCacheEntry(PSHEntryParent* aEntry,
                                          nsresult* aResult) {
  *aResult =
      mEntry->AdoptBFCacheEntry(static_cast<SHEntryParent*>(aEntry)->mEntry);
  return true;
}

bool SHEntryParent::RecvAbandonBFCacheEntry(const uint64_t& aNewSharedID) {
  mEntry->AbandonBFCacheEntry(aNewSharedID);
  return true;
}

bool SHEntryParent::RecvSharesDocumentWith(PSHEntryParent* aEntry,
                                           bool* aSharesDocumentWith,
                                           nsresult* aResult) {
  *aResult = mEntry->SharesDocumentWith(
      static_cast<SHEntryParent*>(aEntry)->mEntry, aSharesDocumentWith);
  return true;
}

bool SHEntryParent::RecvSetLoadTypeAsHistory() {
  DebugOnly<nsresult> rv = mEntry->SetLoadTypeAsHistory();
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  return true;
}

bool SHEntryParent::RecvAddChild(PSHEntryParent* aChild, const int32_t& aOffset,
                                 const bool& aUseRemoteSubframes,
                                 nsresult* aResult) {
  *aResult = mEntry->AddChild(
      aChild ? static_cast<SHEntryParent*>(aChild)->mEntry.get() : nullptr,
      aOffset, aUseRemoteSubframes);
  return true;
}

bool SHEntryParent::RecvRemoveChild(PSHEntryParent* aChild, nsresult* aResult) {
  *aResult = mEntry->RemoveChild(static_cast<SHEntryParent*>(aChild)->mEntry);
  return true;
}

bool SHEntryParent::RecvGetChildAt(const int32_t& aIndex,
                                   RefPtr<CrossProcessSHEntry>* aChild) {
  nsCOMPtr<nsISHEntry> child;
  DebugOnly<nsresult> rv = mEntry->GetChildAt(aIndex, getter_AddRefs(child));
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");

  *aChild = child.forget().downcast<LegacySHEntry>();
  return true;
}

bool SHEntryParent::RecvGetChildSHEntryIfHasNoDynamicallyAddedChild(
    const int32_t& aChildOffset, RefPtr<CrossProcessSHEntry>* aChild) {
  nsCOMPtr<nsISHEntry> child;
  mEntry->GetChildSHEntryIfHasNoDynamicallyAddedChild(aChildOffset,
                                                      getter_AddRefs(child));
  *aChild = child.forget().downcast<LegacySHEntry>();
  return true;
}

bool SHEntryParent::RecvReplaceChild(PSHEntryParent* aNewChild,
                                     nsresult* aResult) {
  *aResult =
      mEntry->ReplaceChild(static_cast<SHEntryParent*>(aNewChild)->mEntry);
  return true;
}

bool SHEntryParent::RecvClearEntry(const uint64_t& aNewSharedID) {
  mEntry->ClearEntry();
  mEntry->AbandonBFCacheEntry(aNewSharedID);
  return true;
}

bool SHEntryParent::RecvCreateLoadInfo(
    RefPtr<nsDocShellLoadState>* aLoadState) {
  mEntry->CreateLoadInfo(getter_AddRefs(*aLoadState));
  return true;
}

bool SHEntryParent::RecvUpdateLayoutHistoryState(
    const bool& aScrollPositionOnly, nsTArray<nsCString>&& aKeys,
    nsTArray<PresState>&& aStates) {
  nsCOMPtr<nsILayoutHistoryState> layoutHistoryState;
  // InitLayoutHistoryState creates a new object only if there isn't one
  // already.
  mEntry->InitLayoutHistoryState(getter_AddRefs(layoutHistoryState));
  layoutHistoryState->Reset();

  if (aKeys.Length() != aStates.Length()) {
    NS_WARNING("Bogus data sent from the child process?");
    return true;
  }

  layoutHistoryState->SetScrollPositionOnly(aScrollPositionOnly);

  for (uint32_t i = 0; i < aKeys.Length(); ++i) {
    PresState& state = aStates[i];
    UniquePtr<PresState> newState = MakeUnique<PresState>(state);
    layoutHistoryState->AddState(aKeys[i], std::move(newState));
  }
  return true;
}

bool SHEntryParent::RecvClone(RefPtr<CrossProcessSHEntry>* aResult) {
  nsCOMPtr<nsISHEntry> result;
  DebugOnly<nsresult> rv =
      static_cast<LegacySHEntry*>(mEntry)->Clone(getter_AddRefs(result));
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Didn't expect this to fail.");
  *aResult = result.forget().downcast<LegacySHEntry>();
  return true;
}

bool SHEntryParent::RecvSyncTreesForSubframeNavigation(
    PSHEntryParent* aSHEntry, const MaybeDiscarded<BrowsingContext>& aBC,
    const MaybeDiscarded<BrowsingContext>& aIgnoreBC,
    nsTArray<SwapEntriesDocshellData>* aEntriesToUpdate) {
  nsTArray<EntriesAndBrowsingContextData> entriesToSendOverIPC;
  // aBC or aIgnoreBC can be discarded but we can update them anyway if they are
  // not null
  mEntry->SyncTreesForSubframeNavigation(
      static_cast<ContentParent*>(Manager())->ChildID(),
      aSHEntry ? static_cast<SHEntryParent*>(aSHEntry)->mEntry.get() : nullptr,
      aBC.GetMaybeDiscarded(), aIgnoreBC.GetMaybeDiscarded(),
      &entriesToSendOverIPC);
  SHistoryParent::CreateActorsForSwapEntries(entriesToSendOverIPC,
                                             aEntriesToUpdate, Manager());
  return true;
}

}  // namespace dom
}  // namespace mozilla
