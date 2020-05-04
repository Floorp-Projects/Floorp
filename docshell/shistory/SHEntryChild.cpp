/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SHEntryChild.h"
#include "SHistoryChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/MaybeNewPSHEntry.h"
#include "mozilla/StaticPrefs_docshell.h"
#include "nsDocShell.h"
#include "nsDocShellEditorData.h"
#include "nsDocShellLoadState.h"
#include "nsIContentViewer.h"
#include "nsILayoutHistoryState.h"
#include "nsIMutableArray.h"
#include "nsStructuredCloneContainer.h"

namespace mozilla {
namespace dom {

StaticAutoPtr<nsRefPtrHashtable<nsUint64HashKey, SHEntryChildShared>>
    sSHEntryChildSharedTable;

uint64_t SHEntryChildShared::sNextSharedID = 0;

/* static */
void SHEntryChildShared::Init() {
  sSHEntryChildSharedTable =
      new nsRefPtrHashtable<nsUint64HashKey, SHEntryChildShared>();
  ClearOnShutdown(&sSHEntryChildSharedTable);
}

/* static */
SHEntryChildShared* SHEntryChildShared::GetOrCreate(SHistoryChild* aSHistory,
                                                    uint64_t aSharedID) {
  MOZ_DIAGNOSTIC_ASSERT(
      sSHEntryChildSharedTable,
      "SHEntryChildShared::Init should have created the table.");
  RefPtr<SHEntryChildShared>& shared =
      sSHEntryChildSharedTable->GetOrInsert(aSharedID);
  if (!shared) {
    shared = new SHEntryChildShared(aSHistory, aSharedID);
  }
  return shared;
}

void SHEntryChildShared::Remove(uint64_t aSharedID) {
  if (sSHEntryChildSharedTable) {
    sSHEntryChildSharedTable->Remove(aSharedID);
  }
}

void SHEntryChildShared::EvictContentViewers(
    const nsTArray<uint64_t>& aToEvictSharedStateIDs) {
  MOZ_ASSERT(sSHEntryChildSharedTable,
             "we have content viewers to evict, but the table hasn't been "
             "initialized yet");
  uint32_t numEvictedSoFar = 0;
  for (auto iter = aToEvictSharedStateIDs.begin();
       iter != aToEvictSharedStateIDs.end(); ++iter) {
    RefPtr<SHEntryChildShared> shared = sSHEntryChildSharedTable->Get(*iter);
    if (!shared) {
      // This can happen if we've created an entry in the parent, and have never
      // sent it over IPC to the child.
      continue;
    }
    nsCOMPtr<nsIContentViewer> viewer = shared->mContentViewer;
    if (viewer) {
      numEvictedSoFar++;
      shared->SetContentViewer(nullptr);
      shared->SyncPresentationState();
      viewer->Destroy();
    }
    if (std::next(iter) == aToEvictSharedStateIDs.end() &&
        numEvictedSoFar > 0) {
      // This is the last shared object, so we should notify our
      // listeners about any content viewers that were evicted.
      // It does not matter which shared entry we will use for notifying.
      shared->NotifyListenersContentViewerEvicted(numEvictedSoFar);
    }
  }
}

SHEntryChildShared::SHEntryChildShared(SHistoryChild* aSHistory, uint64_t aID)
    : mID(aID), mSHistory(aSHistory) {}

SHEntryChildShared::~SHEntryChildShared() {
  // The destruction can be caused by either the entry is removed from session
  // history and no one holds the reference, or the whole session history is on
  // destruction. We want to ensure that we invoke
  // shistory->RemoveFromExpirationTracker for the former case.
  RemoveFromExpirationTracker();

  // Calling RemoveDynEntriesForBFCacheEntry on destruction is unnecessary since
  // there couldn't be any SHEntry holding this shared entry, and we noticed
  // that calling RemoveDynEntriesForBFCacheEntry in the middle of
  // nsSHistory::Release can cause a crash, so set mSHistory to null explicitly
  // before RemoveFromBFCacheSync.
  mSHistory = nullptr;
  if (mContentViewer) {
    RemoveFromBFCacheSync();
  }
}

NS_IMPL_ISUPPORTS(SHEntryChildShared, nsIBFCacheEntry, nsIMutationObserver)

already_AddRefed<SHEntryChildShared> SHEntryChildShared::Duplicate() {
  RefPtr<SHEntryChildShared> newEntry = new SHEntryChildShared(
      mSHistory, mozilla::dom::SHEntryChildShared::CreateSharedID());
  newEntry->CopyFrom(this);
  return newEntry.forget();
}

void SHEntryChildShared::RemoveFromExpirationTracker() {
  if (mSHistory && GetExpirationState()->IsTracked()) {
    mSHistory->RemoveFromExpirationTracker(this);
  }
}

void SHEntryChildShared::SyncPresentationState() {
  if (mContentViewer && mWindowState) {
    // If we have a content viewer and a window state, we should be ok.
    return;
  }

  DropPresentationState();
}

void SHEntryChildShared::DropPresentationState() {
  RefPtr<SHEntryChildShared> kungFuDeathGrip = this;

  if (mDocument) {
    mDocument->SetBFCacheEntry(nullptr);
    mDocument->RemoveMutationObserver(this);
    mDocument = nullptr;
  }
  if (mContentViewer) {
    mContentViewer->ClearHistoryEntry();
  }

  RemoveFromExpirationTracker();
  mContentViewer = nullptr;
  // FIXME Bug 1547735
  // mSticky = true;
  mWindowState = nullptr;
  // FIXME Bug 1547735
  // mViewerBounds.SetRect(0, 0, 0, 0);
  mChildShells.Clear();
  mRefreshURIList = nullptr;
  mEditorData = nullptr;
}

void SHEntryChildShared::NotifyListenersContentViewerEvicted(
    uint32_t aNumEvicted) {
  if (StaticPrefs::docshell_shistory_testing_bfevict() && mSHistory) {
    mSHistory->SendNotifyListenersContentViewerEvicted(aNumEvicted);
  }
}

nsresult SHEntryChildShared::SetContentViewer(nsIContentViewer* aViewer) {
  MOZ_ASSERT(!aViewer || !mContentViewer,
             "SHEntryShared already contains viewer");

  if (mContentViewer || !aViewer) {
    DropPresentationState();
  }

  // If we're setting mContentViewer to null, state should already be cleared
  // in the DropPresentationState() call above; If we're setting it to a
  // non-null content viewer, the entry shouldn't have been tracked either.
  MOZ_ASSERT(!GetExpirationState()->IsTracked());
  mContentViewer = aViewer;

  if (mContentViewer) {
    // mSHistory is only set for root entries, but in general bfcache only
    // applies to root entries as well. BFCache for subframe navigation has been
    // disabled since 2005 in bug 304860.
    if (mSHistory) {
      mSHistory->AddToExpirationTracker(this);
    }

    // Store observed document in strong pointer in case it is removed from
    // the contentviewer
    mDocument = mContentViewer->GetDocument();
    if (mDocument) {
      mDocument->SetBFCacheEntry(this);
      mDocument->AddMutationObserver(this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP SHEntryChildShared::RemoveFromBFCacheSync() {
  MOZ_ASSERT(mContentViewer && mDocument, "we're not in the bfcache!");

  // The call to DropPresentationState could drop the last reference, so hold
  // |this| until RemoveDynEntriesForBFCacheEntry finishes.
  RefPtr<SHEntryChildShared> kungFuDeathGrip = this;

  // DropPresentationState would clear mContentViewer.
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  DropPresentationState();

  if (viewer) {
    viewer->Destroy();
  }

  // Now that we've dropped the viewer, we have to clear associated dynamic
  // subframe entries.
  if (mSHistory) {
    mSHistory->RemoveDynEntriesForBFCacheEntry(this);
  }

  return NS_OK;
}

NS_IMETHODIMP SHEntryChildShared::RemoveFromBFCacheAsync() {
  MOZ_ASSERT(mContentViewer && mDocument, "we're not in the bfcache!");

  // Check it again to play safe in release builds.
  if (!mDocument) {
    return NS_ERROR_UNEXPECTED;
  }

  // DropPresentationState would clear mContentViewer & mDocument. Capture and
  // release the references asynchronously so that the document doesn't get
  // nuked mid-mutation.
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  RefPtr<dom::Document> _document = mDocument;
  RefPtr<SHEntryChildShared> self = this;
  nsresult rv = mDocument->Dispatch(
      mozilla::TaskCategory::Other,
      NS_NewRunnableFunction(
          "SHEntryChildShared::RemoveFromBFCacheAsync",
          // _document isn't used in the closure, but just keeps mDocument
          // alive until the closure runs.
          [self, viewer, _document]() {
            if (viewer) {
              viewer->Destroy();
            }

            if (self->mSHistory) {
              self->mSHistory->RemoveDynEntriesForBFCacheEntry(self);
            }
          }));

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch RemoveFromBFCacheAsync runnable.");
  } else {
    // Drop presentation. Only do this if we succeeded in posting the event
    // since otherwise the document could be torn down mid-mutation, causing
    // crashes.
    DropPresentationState();
  }

  return NS_OK;
}

void SHEntryChildShared::CharacterDataChanged(nsIContent* aContent,
                                              const CharacterDataChangeInfo&) {
  RemoveFromBFCacheAsync();
}

void SHEntryChildShared::AttributeChanged(dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsAtom* aAttribute, int32_t aModType,
                                          const nsAttrValue* aOldValue) {
  RemoveFromBFCacheAsync();
}

void SHEntryChildShared::ContentAppended(nsIContent* aFirstNewContent) {
  RemoveFromBFCacheAsync();
}

void SHEntryChildShared::ContentInserted(nsIContent* aChild) {
  RemoveFromBFCacheAsync();
}

void SHEntryChildShared::ContentRemoved(nsIContent* aChild,
                                        nsIContent* aPreviousSibling) {
  RemoveFromBFCacheAsync();
}

NS_IMPL_ADDREF(SHEntryChild)
NS_IMETHODIMP_(MozExternalRefCountType) SHEntryChild::Release() {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  NS_ASSERT_OWNINGTHREAD(SHEntryChild);
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "SHEntryChild");
  if (count == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  if (count == 1 && !mIPCActorDeleted) {
    Send__delete__(this);
  }
  return count;
}
NS_IMPL_QUERY_INTERFACE(SHEntryChild, nsISHEntry)

NS_IMETHODIMP
SHEntryChild::SetScrollPosition(int32_t aX, int32_t aY) {
  return SendSetScrollPosition(aX, aY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetScrollPosition(int32_t* aX, int32_t* aY) {
  return SendGetScrollPosition(aX, aY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetURIWasModified(bool* aURIWasModified) {
  return SendGetURIWasModified(aURIWasModified) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetURIWasModified(bool aURIWasModified) {
  return SendSetURIWasModified(aURIWasModified) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetURI(nsIURI** aURI) {
  RefPtr<nsIURI> uri;
  if (!SendGetURI(&uri)) {
    return NS_ERROR_FAILURE;
  }

  uri.forget(aURI);

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetURI(nsIURI* aURI) {
  return SendSetURI(aURI) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetOriginalURI(nsIURI** aOriginalURI) {
  RefPtr<nsIURI> originalURI;
  if (!SendGetOriginalURI(&originalURI)) {
    return NS_ERROR_FAILURE;
  }

  originalURI.forget(aOriginalURI);

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetOriginalURI(nsIURI* aOriginalURI) {
  return SendSetOriginalURI(aOriginalURI) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetResultPrincipalURI(nsIURI** aResultPrincipalURI) {
  RefPtr<nsIURI> resultPrincipalURI;
  if (!SendGetResultPrincipalURI(&resultPrincipalURI)) {
    return NS_ERROR_FAILURE;
  }

  resultPrincipalURI.forget(aResultPrincipalURI);

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetResultPrincipalURI(nsIURI* aResultPrincipalURI) {
  return SendSetResultPrincipalURI(aResultPrincipalURI) ? NS_OK
                                                        : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetLoadReplace(bool* aLoadReplace) {
  return SendGetLoadReplace(aLoadReplace) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetLoadReplace(bool aLoadReplace) {
  return SendSetLoadReplace(aLoadReplace) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  RefPtr<nsIReferrerInfo> referrerInfo;
  if (SendGetReferrerInfo(&referrerInfo)) {
    referrerInfo.forget(aReferrerInfo);
  }
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  return SendSetReferrerInfo(aReferrerInfo) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetContentViewer(nsIContentViewer* aViewer) {
  return mShared->SetContentViewer(aViewer);
}

NS_IMETHODIMP
SHEntryChild::GetContentViewer(nsIContentViewer** aResult) {
  NS_IF_ADDREF(*aResult = mShared->mContentViewer);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetSticky(bool aSticky) {
  return SendSetSticky(aSticky) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetSticky(bool* aSticky) {
  return SendGetSticky(aSticky) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetTitle(nsAString& aTitle) {
  nsString title;
  if (!SendGetTitle(&title)) {
    return NS_ERROR_FAILURE;
  }

  aTitle = std::move(title);

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetTitle(const nsAString& aTitle) {
  return SendSetTitle(nsString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetPostData(nsIInputStream** aPostData) {
  RefPtr<nsIInputStream> postData;
  if (!SendGetPostData(&postData)) {
    return NS_ERROR_FAILURE;
  }

  postData.forget(aPostData);

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetPostData(nsIInputStream* aPostData) {
  return SendSetPostData(aPostData) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetLayoutHistoryState(nsILayoutHistoryState** aResult) {
  NS_IF_ADDREF(*aResult = mShared->mLayoutHistoryState);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetLayoutHistoryState(nsILayoutHistoryState* aState) {
  mShared->mLayoutHistoryState = aState;
  if (mShared->mLayoutHistoryState) {
    mShared->mLayoutHistoryState->SetScrollPositionOnly(
        !mShared->mSaveLayoutState);
  }
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::InitLayoutHistoryState(nsILayoutHistoryState** aState) {
  nsCOMPtr<nsILayoutHistoryState> historyState;
  if (mShared->mLayoutHistoryState) {
    historyState = mShared->mLayoutHistoryState;
  } else {
    historyState = NS_NewLayoutHistoryState();
    SetLayoutHistoryState(historyState);
  }

  historyState.forget(aState);

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SynchronizeLayoutHistoryState() {
  if (!mShared->mLayoutHistoryState) {
    return NS_OK;
  }

  bool scrollPositionOnly = false;
  nsTArray<nsCString> keys;
  nsTArray<mozilla::PresState> states;
  mShared->mLayoutHistoryState->GetContents(&scrollPositionOnly, keys, states);
  Unused << SendUpdateLayoutHistoryState(scrollPositionOnly, keys, states);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::GetLoadType(uint32_t* aResult) {
  return SendGetLoadType(aResult) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetLoadType(uint32_t aLoadType) {
  return SendSetLoadType(aLoadType) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetID(uint32_t* aResult) {
  return SendGetID(aResult) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetID(uint32_t aID) {
  return SendSetID(aID) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetIsSubFrame(bool* aFlag) {
  return SendGetIsSubFrame(aFlag) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetIsSubFrame(bool aFlag) {
  return SendSetIsSubFrame(aFlag) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetCacheKey(uint32_t* aResult) {
  return SendGetCacheKey(aResult) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetCacheKey(uint32_t aCacheKey) {
  return SendSetCacheKey(aCacheKey) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetSaveLayoutStateFlag(bool* aFlag) {
  *aFlag = mShared->mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetSaveLayoutStateFlag(bool aFlag) {
  mShared->mSaveLayoutState = aFlag;
  if (mShared->mLayoutHistoryState) {
    mShared->mLayoutHistoryState->SetScrollPositionOnly(!aFlag);
  }

  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::GetExpirationStatus(bool* aFlag) {
  return SendGetExpirationStatus(aFlag) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetExpirationStatus(bool aFlag) {
  return SendSetExpirationStatus(aFlag) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetContentType(nsACString& aContentType) {
  nsCString contentType;
  if (!SendGetContentType(&contentType)) {
    return NS_ERROR_FAILURE;
  }
  aContentType = contentType;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetContentType(const nsACString& aContentType) {
  return SendSetContentType(nsCString(aContentType)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::Create(
    nsIURI* aURI, const nsAString& aTitle, nsIInputStream* aInputStream,
    uint32_t aCacheKey, const nsACString& aContentType,
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aStoragePrincipalToInherit, nsIContentSecurityPolicy* aCsp,
    const nsID& aDocShellID, bool aDynamicCreation, nsIURI* aOriginalURI,
    nsIURI* aResultPrincipalURI, bool aLoadReplace,
    nsIReferrerInfo* aReferrerInfo, const nsAString& srcdoc, bool srcdocEntry,
    nsIURI* aBaseURI, bool aSaveLayoutState, bool aExpired) {
  mShared->mLayoutHistoryState = nullptr;

  mShared->mSaveLayoutState = aSaveLayoutState;
  return SendCreate(aURI, nsString(aTitle), aInputStream, aCacheKey,
                    nsCString(aContentType), aTriggeringPrincipal,
                    aPrincipalToInherit, aStoragePrincipalToInherit, aCsp,
                    aDocShellID, aDynamicCreation, aOriginalURI,
                    aResultPrincipalURI, aLoadReplace, aReferrerInfo,
                    nsString(srcdoc), srcdocEntry, aBaseURI, aSaveLayoutState,
                    aExpired)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::Clone(nsISHEntry** aResult) {
  RefPtr<CrossProcessSHEntry> result;
  if (!SendClone(&result)) {
    return NS_ERROR_FAILURE;
  }
  *aResult = result ? do_AddRef(result->ToSHEntryChild()).take() : nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::GetParent(nsISHEntry** aResult) {
  RefPtr<CrossProcessSHEntry> parent;
  if (!SendGetParent(&parent)) {
    return NS_ERROR_FAILURE;
  }

  *aResult = parent ? do_AddRef(parent->ToSHEntryChild()).take() : nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetParent(nsISHEntry* aParent) {
  return SendSetParent(static_cast<SHEntryChild*>(aParent)) ? NS_OK
                                                            : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetWindowState(nsISupports* aState) {
  mShared->mWindowState = aState;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::GetWindowState(nsISupports** aState) {
  NS_IF_ADDREF(*aState = mShared->mWindowState);
  return NS_OK;
}

NS_IMETHODIMP_(void)
SHEntryChild::SetViewerBounds(const nsIntRect& aBounds) {
  Unused << SendSetViewerBounds(aBounds);
}

NS_IMETHODIMP_(void)
SHEntryChild::GetViewerBounds(nsIntRect& aBounds) {
  Unused << SendGetViewerBounds(&aBounds);
}

NS_IMETHODIMP
SHEntryChild::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal) {
  RefPtr<nsIPrincipal> triggeringPrincipal;
  if (!SendGetTriggeringPrincipal(&triggeringPrincipal)) {
    return NS_ERROR_FAILURE;
  }
  triggeringPrincipal.forget(aTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal) {
  return SendSetTriggeringPrincipal(aTriggeringPrincipal) ? NS_OK
                                                          : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetPrincipalToInherit(nsIPrincipal** aPrincipalToInherit) {
  RefPtr<nsIPrincipal> principalToInherit;
  if (!SendGetPrincipalToInherit(&principalToInherit)) {
    return NS_ERROR_FAILURE;
  }
  principalToInherit.forget(aPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit) {
  return SendSetPrincipalToInherit(aPrincipalToInherit) ? NS_OK
                                                        : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetStoragePrincipalToInherit(
    nsIPrincipal** aStoragePrincipalToInherit) {
  RefPtr<nsIPrincipal> storagePrincipalToInherit;
  if (!SendGetStoragePrincipalToInherit(&storagePrincipalToInherit)) {
    return NS_ERROR_FAILURE;
  }
  storagePrincipalToInherit.forget(aStoragePrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetStoragePrincipalToInherit(
    nsIPrincipal* aStoragePrincipalToInherit) {
  return SendSetStoragePrincipalToInherit(aStoragePrincipalToInherit)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetCsp(nsIContentSecurityPolicy** aCsp) {
  RefPtr<nsIContentSecurityPolicy> csp;
  if (!SendGetCsp(&csp)) {
    return NS_ERROR_FAILURE;
  }
  csp.forget(aCsp);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetCsp(nsIContentSecurityPolicy* aCsp) {
  return SendSetCsp(aCsp) ? NS_OK : NS_ERROR_FAILURE;
}

bool SHEntryChild::HasBFCacheEntry(nsIBFCacheEntry* aEntry) {
  return mShared == aEntry;
}

NS_IMETHODIMP
SHEntryChild::AdoptBFCacheEntry(nsISHEntry* aEntry) {
  RefPtr<SHEntryChild> entry = static_cast<SHEntryChild*>(aEntry);
  nsresult rv;
  if (!SendAdoptBFCacheEntry(entry, &rv)) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  mShared = entry->mShared;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SharesDocumentWith(nsISHEntry* aEntry, bool* aOut) {
  nsresult rv;
  // FIXME Maybe check local shared state instead?
  return SendSharesDocumentWith(static_cast<SHEntryChild*>(aEntry), aOut, &rv)
             ? rv
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::AbandonBFCacheEntry() {
  RefPtr<SHEntryChildShared> shared = mShared->Duplicate();
  if (!SendAbandonBFCacheEntry(shared->GetID())) {
    return NS_ERROR_FAILURE;
  }

  shared.swap(mShared);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::GetIsSrcdocEntry(bool* aIsSrcdocEntry) {
  return SendGetIsSrcdocEntry(aIsSrcdocEntry) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetSrcdocData(nsAString& aSrcdocData) {
  nsString srcdocData;
  if (!SendGetSrcdocData(&srcdocData)) {
    return NS_ERROR_FAILURE;
  }
  aSrcdocData = srcdocData;
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetSrcdocData(const nsAString& aSrcdocData) {
  return SendSetSrcdocData(nsString(aSrcdocData)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetBaseURI(nsIURI** aBaseURI) {
  RefPtr<nsIURI> baseURI;
  if (!SendGetBaseURI(&baseURI)) {
    return NS_ERROR_FAILURE;
  }
  baseURI.forget(aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetBaseURI(nsIURI* aBaseURI) {
  return SendSetBaseURI(aBaseURI) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetScrollRestorationIsManual(bool* aIsManual) {
  return SendGetScrollRestorationIsManual(aIsManual) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetScrollRestorationIsManual(bool aIsManual) {
  return SendSetScrollRestorationIsManual(aIsManual) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetLoadedInThisProcess(bool* aLoadedInThisProcess) {
  return SendGetLoadedInThisProcess(aLoadedInThisProcess) ? NS_OK
                                                          : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetChildCount(int32_t* aCount) {
  return SendGetChildCount(aCount) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::AddChild(nsISHEntry* aChild, int32_t aOffset,
                       bool aUseRemoteSubframes) {
  nsresult rv;
  return SendAddChild(static_cast<SHEntryChild*>(aChild), aOffset,
                      aUseRemoteSubframes, &rv)
             ? rv
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::RemoveChild(nsISHEntry* aChild) {
  nsresult rv;
  return aChild && SendRemoveChild(static_cast<SHEntryChild*>(aChild), &rv)
             ? rv
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetChildAt(int32_t aIndex, nsISHEntry** aResult) {
  RefPtr<CrossProcessSHEntry> child;
  if (!SendGetChildAt(aIndex, &child)) {
    return NS_ERROR_FAILURE;
  }

  *aResult = child ? do_AddRef(child->ToSHEntryChild()).take() : nullptr;
  return NS_OK;
}

NS_IMETHODIMP_(void)
SHEntryChild::GetChildSHEntryIfHasNoDynamicallyAddedChild(int32_t aChildOffset,
                                                          nsISHEntry** aChild) {
  RefPtr<CrossProcessSHEntry> child;
  SendGetChildSHEntryIfHasNoDynamicallyAddedChild(aChildOffset, &child);
  *aChild = child ? do_AddRef(child->ToSHEntryChild()).take() : nullptr;
}

NS_IMETHODIMP
SHEntryChild::ReplaceChild(nsISHEntry* aNewEntry) {
  nsresult rv;
  return SendReplaceChild(static_cast<SHEntryChild*>(aNewEntry), &rv)
             ? rv
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
SHEntryChild::ClearEntry() {
  // We want to call AbandonBFCacheEntry in SHEntryParent,
  // hence the need for duplciating the shared entry here
  RefPtr<SHEntryChildShared> shared = mShared->Duplicate();
  SendClearEntry(shared->GetID());
  shared.swap(mShared);
}

NS_IMETHODIMP_(void)
SHEntryChild::AddChildShell(nsIDocShellTreeItem* aShell) {
  mShared->mChildShells.AppendObject(aShell);
}

NS_IMETHODIMP
SHEntryChild::ChildShellAt(int32_t aIndex, nsIDocShellTreeItem** aShell) {
  NS_IF_ADDREF(*aShell = mShared->mChildShells.SafeObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP_(void)
SHEntryChild::ClearChildShells() { mShared->mChildShells.Clear(); }

NS_IMETHODIMP
SHEntryChild::GetRefreshURIList(nsIMutableArray** aList) {
  // FIXME Move to parent.
  NS_IF_ADDREF(*aList = mShared->mRefreshURIList);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetRefreshURIList(nsIMutableArray* aList) {
  // FIXME Move to parent.
  mShared->mRefreshURIList = aList;
  return NS_OK;
}

NS_IMETHODIMP_(void)
SHEntryChild::SyncPresentationState() { mShared->SyncPresentationState(); }

nsDocShellEditorData* SHEntryChild::ForgetEditorData() {
  return mShared->mEditorData.release();
}

void SHEntryChild::SetEditorData(nsDocShellEditorData* aData) {
  NS_ASSERTION(!(aData && mShared->mEditorData),
               "We're going to overwrite an owning ref!");
  if (mShared->mEditorData != aData) {
    mShared->mEditorData = WrapUnique(aData);
  }
}

bool SHEntryChild::HasDetachedEditor() {
  return mShared->mEditorData != nullptr;
}

NS_IMETHODIMP
SHEntryChild::GetStateData(nsIStructuredCloneContainer** aContainer) {
  ClonedMessageData data;
  if (!SendGetStateData(&data)) {
    return NS_ERROR_FAILURE;
  }
  // FIXME Should we signal null separately from the ClonedMessageData?
  if (data.data().data.Size() == 0) {
    *aContainer = nullptr;
  } else {
    RefPtr<nsStructuredCloneContainer> container =
        new nsStructuredCloneContainer();
    container->StealFromClonedMessageDataForParent(data);
    container.forget(aContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetStateData(nsIStructuredCloneContainer* aContainer) {
  // FIXME nsIStructuredCloneContainer is not builtin_class
  ClonedMessageData data;
  if (aContainer) {
    static_cast<nsStructuredCloneContainer*>(aContainer)
        ->BuildClonedMessageDataForChild(ContentChild::GetSingleton(), data);
  }
  return SendSetStateData(data) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(bool)
SHEntryChild::IsDynamicallyAdded() {
  bool isDynamicallyAdded;
  return SendIsDynamicallyAdded(&isDynamicallyAdded) && isDynamicallyAdded;
}

NS_IMETHODIMP
SHEntryChild::HasDynamicallyAddedChild(bool* aAdded) {
  return SendHasDynamicallyAddedChild(aAdded) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetDocshellID(nsID& aID) {
  if (!SendGetDocshellID(&aID)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetDocshellID(const nsID& aID) {
  return SendSetDocshellID(aID) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetLastTouched(uint32_t* aLastTouched) {
  return SendGetLastTouched(aLastTouched) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetLastTouched(uint32_t aLastTouched) {
  return SendSetLastTouched(aLastTouched) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetShistory(nsISHistory** aSHistory) {
  *aSHistory = do_AddRef(mShared->mSHistory).take();
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::SetLoadTypeAsHistory() {
  return SendSetLoadTypeAsHistory() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::GetPersist(bool* aPersist) {
  return SendGetPersist(aPersist) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::SetPersist(bool aPersist) {
  return SendSetPersist(aPersist) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SHEntryChild::CreateLoadInfo(nsDocShellLoadState** aLoadState) {
  *aLoadState = nullptr;
  RefPtr<nsDocShellLoadState> loadState;
  if (!SendCreateLoadInfo(&loadState)) {
    return NS_ERROR_FAILURE;
  }
  loadState.forget(aLoadState);
  return NS_OK;
}

NS_IMETHODIMP
SHEntryChild::GetBfcacheID(uint64_t* aBFCacheID) {
  *aBFCacheID = mShared->GetID();
  return NS_OK;
}

NS_IMETHODIMP_(void)
SHEntryChild::SyncTreesForSubframeNavigation(nsISHEntry* aEntry,
                                             BrowsingContext* aBC,
                                             BrowsingContext* aIgnoreBC) {
  nsTArray<SwapEntriesDocshellData> entriesToUpdate;
  Unused << SendSyncTreesForSubframeNavigation(
      static_cast<SHEntryChild*>(aEntry), aBC, aIgnoreBC, &entriesToUpdate);
  for (auto& data : entriesToUpdate) {
    // data.context() is a MaybeDiscardedBrowsingContext
    // It can't be null, but if it has been discarded we will update
    // the docshell anyway
    MOZ_ASSERT(!data.context().IsNull(), "Browsing context cannot be null");
    nsDocShell* docshell = static_cast<nsDocShell*>(
        data.context().GetMaybeDiscarded()->GetDocShell());
    if (docshell) {
      RefPtr<SHEntryChild> oldEntry = data.oldEntry()->ToSHEntryChild();
      RefPtr<SHEntryChild> newEntry;
      if (data.newEntry()) {
        newEntry = data.newEntry()->ToSHEntryChild();
      }
      docshell->SwapHistoryEntries(oldEntry, newEntry);
    }
  }
}

void SHEntryChild::EvictContentViewer() {
  nsCOMPtr<nsIContentViewer> viewer = GetContentViewer();
  if (viewer) {
    // Drop the presentation state before destroying the viewer, so that
    // document teardown is able to correctly persist the state.
    mShared->NotifyListenersContentViewerEvicted();
    SetContentViewer(nullptr);
    SyncPresentationState();
    viewer->Destroy();
  }
}

}  // namespace dom
}  // namespace mozilla
