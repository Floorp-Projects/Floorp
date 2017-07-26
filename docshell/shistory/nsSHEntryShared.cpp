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
#include "nsIDOMDocument.h"
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
  RemoveFromExpirationTracker();
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
  NS_PRECONDITION(!aViewer || !mContentViewer,
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
    nsCOMPtr<nsISHistoryInternal> shistory = do_QueryReferent(mSHistory);
    if (shistory) {
      shistory->AddToExpirationTracker(this);
    }

    nsCOMPtr<nsIDOMDocument> domDoc;
    mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
    // Store observed document in strong pointer in case it is removed from
    // the contentviewer
    mDocument = do_QueryInterface(domDoc);
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

  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  DropPresentationState();

  // Warning! The call to DropPresentationState could have dropped the last
  // reference to this object, so don't access members beyond here.

  if (viewer) {
    viewer->Destroy();
  }

  return NS_OK;
}

class DestroyViewerEvent : public mozilla::Runnable
{
public:
  DestroyViewerEvent(nsIContentViewer* aViewer, nsIDocument* aDocument)
    : mozilla::Runnable("DestroyViewerEvent")
    , mViewer(aViewer)
    , mDocument(aDocument)
  {
  }

  NS_IMETHOD Run() override
  {
    if (mViewer) {
      mViewer->Destroy();
    }
    return NS_OK;
  }

  nsCOMPtr<nsIContentViewer> mViewer;
  nsCOMPtr<nsIDocument> mDocument;
};

nsresult
nsSHEntryShared::RemoveFromBFCacheAsync()
{
  MOZ_ASSERT(mContentViewer && mDocument, "we're not in the bfcache!");

  // Release the reference to the contentviewer asynchronously so that the
  // document doesn't get nuked mid-mutation.

  if (!mDocument) {
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIRunnable> evt = new DestroyViewerEvent(mContentViewer, mDocument);
  nsresult rv = mDocument->Dispatch(mozilla::TaskCategory::Other, evt.forget());
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch DestroyViewerEvent");
  } else {
    // Drop presentation. Only do this if we succeeded in posting the event
    // since otherwise the document could be torn down mid-mutation, causing
    // crashes.
    DropPresentationState();
  }

  // Careful! The call to DropPresentationState could have dropped the last
  // reference to this nsSHEntryShared, so don't access members beyond here.

  return NS_OK;
}

nsresult
nsSHEntryShared::GetID(uint64_t* aID)
{
  *aID = mID;
  return NS_OK;
}

void
nsSHEntryShared::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_NOTREACHED("Document destroyed while we're holding a strong ref to it");
}

void
nsSHEntryShared::CharacterDataWillChange(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
}

void
nsSHEntryShared::CharacterDataChanged(nsIDocument* aDocument,
                                      nsIContent* aContent,
                                      CharacterDataChangeInfo* aInfo)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::AttributeWillChange(nsIDocument* aDocument,
                                     dom::Element* aContent,
                                     int32_t aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t aModType,
                                     const nsAttrValue* aNewValue)
{
}

void
nsSHEntryShared::NativeAnonymousChildListChange(nsIDocument* aDocument,
                                                nsIContent* aContent,
                                                bool aIsRemove)
{
}

void
nsSHEntryShared::AttributeChanged(nsIDocument* aDocument,
                                  dom::Element* aElement,
                                  int32_t aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t aModType,
                                  const nsAttrValue* aOldValue)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ContentAppended(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aFirstNewContent,
                                 int32_t /* unused */)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ContentInserted(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 int32_t /* unused */)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ContentRemoved(nsIDocument* aDocument,
                                nsIContent* aContainer,
                                nsIContent* aChild,
                                int32_t aIndexInContainer,
                                nsIContent* aPreviousSibling)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntryShared::ParentChainChanged(nsIContent* aContent)
{
}
