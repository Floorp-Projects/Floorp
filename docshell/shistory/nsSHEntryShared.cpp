/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHEntryShared.h"

#include "nsArray.h"
#include "nsContentUtils.h"
#include "nsDocShellEditorData.h"
#include "nsIContentViewer.h"
#include "nsISHistory.h"
#include "mozilla/dom/Document.h"
#include "nsILayoutHistoryState.h"
#include "nsIWebNavigation.h"
#include "nsSHistory.h"
#include "nsThreadUtils.h"
#include "nsFrameLoader.h"
#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"

namespace dom = mozilla::dom;

namespace {
uint64_t gSHEntrySharedID = 0;
nsTHashMap<nsUint64HashKey, mozilla::dom::SHEntrySharedParentState*>*
    sIdToSharedState = nullptr;
}  // namespace

namespace mozilla {
namespace dom {

/* static */
uint64_t SHEntrySharedState::GenerateId() {
  return nsContentUtils::GenerateProcessSpecificId(++gSHEntrySharedID);
}

/* static */
SHEntrySharedParentState* SHEntrySharedParentState::Lookup(uint64_t aId) {
  MOZ_ASSERT(aId != 0);

  return sIdToSharedState ? sIdToSharedState->Get(aId) : nullptr;
}

static void AddSHEntrySharedParentState(
    SHEntrySharedParentState* aSharedState) {
  MOZ_ASSERT(aSharedState->mId != 0);

  if (!sIdToSharedState) {
    sIdToSharedState =
        new nsTHashMap<nsUint64HashKey, SHEntrySharedParentState*>();
  }
  sIdToSharedState->InsertOrUpdate(aSharedState->mId, aSharedState);
}

SHEntrySharedParentState::SHEntrySharedParentState() {
  AddSHEntrySharedParentState(this);
}

SHEntrySharedParentState::SHEntrySharedParentState(
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, const nsACString& aContentType)
    : SHEntrySharedState(aTriggeringPrincipal, aPrincipalToInherit,
                         aPartitionedPrincipalToInherit, aCsp, aContentType) {
  AddSHEntrySharedParentState(this);
}

SHEntrySharedParentState::~SHEntrySharedParentState() {
  MOZ_ASSERT(mId != 0);

  RefPtr<nsFrameLoader> loader = mFrameLoader.forget();
  if (loader) {
    loader->Destroy();
  }

  sIdToSharedState->Remove(mId);
  if (sIdToSharedState->IsEmpty()) {
    delete sIdToSharedState;
    sIdToSharedState = nullptr;
  }
}

void SHEntrySharedParentState::ChangeId(uint64_t aId) {
  MOZ_ASSERT(aId != 0);

  sIdToSharedState->Remove(mId);
  mId = aId;
  sIdToSharedState->InsertOrUpdate(mId, this);
}

void SHEntrySharedParentState::CopyFrom(SHEntrySharedParentState* aEntry) {
  mDocShellID = aEntry->mDocShellID;
  mTriggeringPrincipal = aEntry->mTriggeringPrincipal;
  mPrincipalToInherit = aEntry->mPrincipalToInherit;
  mPartitionedPrincipalToInherit = aEntry->mPartitionedPrincipalToInherit;
  mCsp = aEntry->mCsp;
  mSaveLayoutState = aEntry->mSaveLayoutState;
  mContentType.Assign(aEntry->mContentType);
  mIsFrameNavigation = aEntry->mIsFrameNavigation;
  mSticky = aEntry->mSticky;
  mDynamicallyCreated = aEntry->mDynamicallyCreated;
  mCacheKey = aEntry->mCacheKey;
  mLastTouched = aEntry->mLastTouched;
}

void dom::SHEntrySharedParentState::NotifyListenersContentViewerEvicted() {
  if (nsCOMPtr<nsISHistory> shistory = do_QueryReferent(mSHistory)) {
    RefPtr<nsSHistory> nsshistory = static_cast<nsSHistory*>(shistory.get());
    nsshistory->NotifyListenersContentViewerEvicted(1);
  }
}

void SHEntrySharedChildState::CopyFrom(SHEntrySharedChildState* aEntry) {
  mChildShells.AppendObjects(aEntry->mChildShells);
}

void SHEntrySharedParentState::SetFrameLoader(nsFrameLoader* aFrameLoader) {
  mFrameLoader = aFrameLoader;
}

nsFrameLoader* SHEntrySharedParentState::GetFrameLoader() {
  return mFrameLoader;
}

}  // namespace dom
}  // namespace mozilla

void nsSHEntryShared::Shutdown() {}

nsSHEntryShared::~nsSHEntryShared() {
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

NS_IMPL_QUERY_INTERFACE(nsSHEntryShared, nsIBFCacheEntry, nsIMutationObserver)
NS_IMPL_ADDREF_INHERITED(nsSHEntryShared, dom::SHEntrySharedParentState)
NS_IMPL_RELEASE_INHERITED(nsSHEntryShared, dom::SHEntrySharedParentState)

already_AddRefed<nsSHEntryShared> nsSHEntryShared::Duplicate() {
  RefPtr<nsSHEntryShared> newEntry = new nsSHEntryShared();

  newEntry->dom::SHEntrySharedParentState::CopyFrom(this);
  newEntry->dom::SHEntrySharedChildState::CopyFrom(this);

  return newEntry.forget();
}

void nsSHEntryShared::RemoveFromExpirationTracker() {
  nsCOMPtr<nsISHistory> shistory = do_QueryReferent(mSHistory);
  if (shistory && GetExpirationState()->IsTracked()) {
    shistory->RemoveFromExpirationTracker(this);
  }
}

void nsSHEntryShared::SyncPresentationState() {
  if (mContentViewer && mWindowState) {
    // If we have a content viewer and a window state, we should be ok.
    return;
  }

  DropPresentationState();
}

void nsSHEntryShared::DropPresentationState() {
  RefPtr<nsSHEntryShared> kungFuDeathGrip = this;

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
  mSticky = true;
  mWindowState = nullptr;
  mViewerBounds.SetRect(0, 0, 0, 0);
  mChildShells.Clear();
  mRefreshURIList = nullptr;
  mEditorData = nullptr;
}

nsresult nsSHEntryShared::SetContentViewer(nsIContentViewer* aViewer) {
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
    if (nsCOMPtr<nsISHistory> shistory = do_QueryReferent(mSHistory)) {
      shistory->AddToExpirationTracker(this);
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

nsresult nsSHEntryShared::RemoveFromBFCacheSync() {
  MOZ_ASSERT(mContentViewer && mDocument, "we're not in the bfcache!");

  // The call to DropPresentationState could drop the last reference, so hold
  // |this| until RemoveDynEntriesForBFCacheEntry finishes.
  RefPtr<nsSHEntryShared> kungFuDeathGrip = this;

  // DropPresentationState would clear mContentViewer.
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  DropPresentationState();

  if (viewer) {
    viewer->Destroy();
  }

  // Now that we've dropped the viewer, we have to clear associated dynamic
  // subframe entries.
  nsCOMPtr<nsISHistory> shistory = do_QueryReferent(mSHistory);
  if (shistory) {
    shistory->RemoveDynEntriesForBFCacheEntry(this);
  }

  return NS_OK;
}

nsresult nsSHEntryShared::RemoveFromBFCacheAsync() {
  MOZ_ASSERT(mContentViewer && mDocument, "we're not in the bfcache!");

  // Check it again to play safe in release builds.
  if (!mDocument) {
    return NS_ERROR_UNEXPECTED;
  }

  // DropPresentationState would clear mContentViewer & mDocument. Capture and
  // release the references asynchronously so that the document doesn't get
  // nuked mid-mutation.
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  RefPtr<dom::Document> document = mDocument;
  RefPtr<nsSHEntryShared> self = this;
  nsresult rv = mDocument->Dispatch(
      mozilla::TaskCategory::Other,
      NS_NewRunnableFunction(
          "nsSHEntryShared::RemoveFromBFCacheAsync",
          [self, viewer, document]() {
            if (viewer) {
              viewer->Destroy();
            }

            nsCOMPtr<nsISHistory> shistory = do_QueryReferent(self->mSHistory);
            if (shistory) {
              shistory->RemoveDynEntriesForBFCacheEntry(self);
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

void nsSHEntryShared::CharacterDataChanged(nsIContent* aContent,
                                           const CharacterDataChangeInfo&) {
  RemoveFromBFCacheAsync();
}

void nsSHEntryShared::AttributeChanged(dom::Element* aElement,
                                       int32_t aNameSpaceID, nsAtom* aAttribute,
                                       int32_t aModType,
                                       const nsAttrValue* aOldValue) {
  RemoveFromBFCacheAsync();
}

void nsSHEntryShared::ContentAppended(nsIContent* aFirstNewContent) {
  RemoveFromBFCacheAsync();
}

void nsSHEntryShared::ContentInserted(nsIContent* aChild) {
  RemoveFromBFCacheAsync();
}

void nsSHEntryShared::ContentRemoved(nsIContent* aChild,
                                     nsIContent* aPreviousSibling) {
  RemoveFromBFCacheAsync();
}
