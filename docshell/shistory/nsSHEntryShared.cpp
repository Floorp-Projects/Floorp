/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHEntryShared.h"

#include "nsArray.h"
#include "nsDocShellEditorData.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsILayoutHistoryState.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIWebNavigation.h"
#include "nsThreadUtils.h"

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"

namespace dom = mozilla::dom;

namespace {

uint64_t gSHEntrySharedID = 0;

} // namespace

void
nsSHEntryShared::Shutdown()
{
}

nsSHEntryShared::nsSHEntryShared()
  : mDocShellID({0})
  , mCacheKey(0)
  , mLastTouched(0)
  , mID(gSHEntrySharedID++)
  , mViewerBounds(0, 0, 0, 0)
  , mIsFrameNavigation(false)
  , mSaveLayoutState(true)
  , mSticky(true)
  , mDynamicallyCreated(false)
  , mExpired(false)
{
}

nsSHEntryShared::~nsSHEntryShared()
{
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

NS_IMPL_ISUPPORTS(nsSHEntryShared, nsIBFCacheEntry, nsIMutationObserver)

already_AddRefed<nsSHEntryShared>
nsSHEntryShared::Duplicate(nsSHEntryShared* aEntry)
{
  RefPtr<nsSHEntryShared> newEntry = new nsSHEntryShared();

  newEntry->mDocShellID = aEntry->mDocShellID;
  newEntry->mChildShells.AppendObjects(aEntry->mChildShells);
  newEntry->mTriggeringPrincipal = aEntry->mTriggeringPrincipal;
  newEntry->mPrincipalToInherit = aEntry->mPrincipalToInherit;
  newEntry->mContentType.Assign(aEntry->mContentType);
  newEntry->mIsFrameNavigation = aEntry->mIsFrameNavigation;
  newEntry->mSaveLayoutState = aEntry->mSaveLayoutState;
  newEntry->mSticky = aEntry->mSticky;
  newEntry->mDynamicallyCreated = aEntry->mDynamicallyCreated;
  newEntry->mCacheKey = aEntry->mCacheKey;
  newEntry->mLastTouched = aEntry->mLastTouched;

  return newEntry.forget();
}

void
nsSHEntryShared::RemoveFromExpirationTracker()
{
  nsCOMPtr<nsISHistoryInternal> shistory = do_QueryReferent(mSHistory);
  if (shistory && GetExpirationState()->IsTracked()) {
    shistory->RemoveFromExpirationTracker(this);
  }
}

nsresult
nsSHEntryShared::SyncPresentationState()
{
  if (mContentViewer && mWindowState) {
    // If we have a content viewer and a window state, we should be ok.
    return NS_OK;
  }

  DropPresentationState();

  return NS_OK;
}

void
nsSHEntryShared::DropPresentationState()
{
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

nsresult
nsSHEntryShared::SetContentViewer(nsIContentViewer* aViewer)
{
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
    if (nsCOMPtr<nsISHistoryInternal> shistory = do_QueryReferent(mSHistory)) {
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

nsresult
nsSHEntryShared::RemoveFromBFCacheSync()
{
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
  nsCOMPtr<nsISHistoryInternal> shistory = do_QueryReferent(mSHistory);
  if (shistory) {
    shistory->RemoveDynEntriesForBFCacheEntry(this);
  }

  return NS_OK;
}

nsresult
nsSHEntryShared::RemoveFromBFCacheAsync()
{
  MOZ_ASSERT(mContentViewer && mDocument, "we're not in the bfcache!");

  // Check it again to play safe in release builds.
  if (!mDocument) {
    return NS_ERROR_UNEXPECTED;
  }

  // DropPresentationState would clear mContentViewer & mDocument. Capture and
  // release the references asynchronously so that the document doesn't get
  // nuked mid-mutation.
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  nsCOMPtr<nsIDocument> document = mDocument;
  RefPtr<nsSHEntryShared> self = this;
  nsresult rv = mDocument->Dispatch(mozilla::TaskCategory::Other,
    NS_NewRunnableFunction("nsSHEntryShared::RemoveFromBFCacheAsync",
    [self, viewer, document]() {
      if (viewer) {
        viewer->Destroy();
      }

      nsCOMPtr<nsISHistoryInternal> shistory = do_QueryReferent(self->mSHistory);
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

nsresult
nsSHEntryShared::GetID(uint64_t* aID)
{
  *aID = mID;
  return NS_OK;
}

void
nsSHEntryShared::CharacterDataChanged(nsIContent* aContent,
                                      const CharacterDataChangeInfo&)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::AttributeChanged(dom::Element* aElement,
                                  int32_t aNameSpaceID,
                                  nsAtom* aAttribute,
                                  int32_t aModType,
                                  const nsAttrValue* aOldValue)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ContentAppended(nsIContent* aFirstNewContent)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ContentInserted(nsIContent* aChild)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ContentRemoved(nsIContent* aChild,
                                nsIContent* aPreviousSibling)
{
  RemoveFromBFCacheAsync();
}
