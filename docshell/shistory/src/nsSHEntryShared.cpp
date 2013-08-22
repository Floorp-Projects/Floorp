/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHEntryShared.h"

#include "nsIDOMDocument.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIDocument.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#include "nsIDocShellTreeItem.h"
#include "nsDocShellEditorData.h"
#include "nsThreadUtils.h"
#include "nsILayoutHistoryState.h"
#include "prprf.h"
#include "mozilla/Attributes.h"

namespace dom = mozilla::dom;

namespace {

uint64_t gSHEntrySharedID = 0;

} // anonymous namespace

// Hardcode this to time out unused content viewers after 30 minutes
// XXX jlebar shouldn't this be a pref?
#define CONTENT_VIEWER_TIMEOUT_SECONDS (30*60)

typedef nsExpirationTracker<nsSHEntryShared, 3> HistoryTrackerBase;
class HistoryTracker MOZ_FINAL : public HistoryTrackerBase {
public:
  // Expire cached contentviewers after 20-30 minutes in the cache.
  HistoryTracker() 
    : HistoryTrackerBase(1000 * CONTENT_VIEWER_TIMEOUT_SECONDS / 2)
  {
  }
  
protected:
  virtual void NotifyExpired(nsSHEntryShared *aObj) {
    RemoveObject(aObj);
    aObj->Expire();
  }
};

static HistoryTracker *gHistoryTracker = nullptr;

void
nsSHEntryShared::Startup()
{
  gHistoryTracker = new HistoryTracker();
}

void
nsSHEntryShared::Shutdown()
{
  delete gHistoryTracker;
  gHistoryTracker = nullptr;
}

nsSHEntryShared::nsSHEntryShared()
  : mDocShellID(0)
  , mIsFrameNavigation(false)
  , mSaveLayoutState(true)
  , mSticky(true)
  , mDynamicallyCreated(false)
  , mLastTouched(0)
  , mID(gSHEntrySharedID++)
  , mExpired(false)
  , mViewerBounds(0, 0, 0, 0)
{
}

nsSHEntryShared::~nsSHEntryShared()
{
  RemoveFromExpirationTracker();

#ifdef DEBUG
  // Check that we're not still on track to expire.  We shouldn't be, because
  // we just removed ourselves!
  nsExpirationTracker<nsSHEntryShared, 3>::Iterator
    iterator(gHistoryTracker);

  nsSHEntryShared *elem;
  while ((elem = iterator.Next()) != nullptr) {
    NS_ASSERTION(elem != this, "Found dead entry still in the tracker!");
  }
#endif

  if (mContentViewer) {
    RemoveFromBFCacheSync();
  }
}

NS_IMPL_ISUPPORTS2(nsSHEntryShared, nsIBFCacheEntry, nsIMutationObserver)

already_AddRefed<nsSHEntryShared>
nsSHEntryShared::Duplicate(nsSHEntryShared *aEntry)
{
  nsRefPtr<nsSHEntryShared> newEntry = new nsSHEntryShared();

  newEntry->mDocShellID = aEntry->mDocShellID;
  newEntry->mChildShells.AppendObjects(aEntry->mChildShells);
  newEntry->mOwner = aEntry->mOwner;
  newEntry->mContentType.Assign(aEntry->mContentType);
  newEntry->mIsFrameNavigation = aEntry->mIsFrameNavigation;
  newEntry->mSaveLayoutState = aEntry->mSaveLayoutState;
  newEntry->mSticky = aEntry->mSticky;
  newEntry->mDynamicallyCreated = aEntry->mDynamicallyCreated;
  newEntry->mCacheKey = aEntry->mCacheKey;
  newEntry->mLastTouched = aEntry->mLastTouched;

  return newEntry.forget();
}

void nsSHEntryShared::RemoveFromExpirationTracker()
{
  if (GetExpirationState()->IsTracked()) {
    gHistoryTracker->RemoveObject(this);
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
  nsRefPtr<nsSHEntryShared> kungFuDeathGrip = this;

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

void
nsSHEntryShared::Expire()
{
  // This entry has timed out. If we still have a content viewer, we need to
  // evict it.
  if (!mContentViewer) {
    return;
  }
  nsCOMPtr<nsISupports> container;
  mContentViewer->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(container);
  if (!treeItem) {
    return;
  }
  // We need to find the root DocShell since only that object has an
  // SHistory and we need the SHistory to evict content viewers
  nsCOMPtr<nsIDocShellTreeItem> root;
  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(root);
  nsCOMPtr<nsISHistory> history;
  webNav->GetSessionHistory(getter_AddRefs(history));
  nsCOMPtr<nsISHistoryInternal> historyInt = do_QueryInterface(history);
  if (!historyInt) {
    return;
  }
  historyInt->EvictExpiredContentViewerForEntry(this);
}

nsresult
nsSHEntryShared::SetContentViewer(nsIContentViewer *aViewer)
{
  NS_PRECONDITION(!aViewer || !mContentViewer,
                  "SHEntryShared already contains viewer");

  if (mContentViewer || !aViewer) {
    DropPresentationState();
  }

  mContentViewer = aViewer;

  if (mContentViewer) {
    gHistoryTracker->AddObject(this);

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
  NS_ASSERTION(mContentViewer && mDocument,
               "we're not in the bfcache!");

  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  DropPresentationState();

  // Warning! The call to DropPresentationState could have dropped the last
  // reference to this object, so don't access members beyond here.

  if (viewer) {
    viewer->Destroy();
  }

  return NS_OK;
}

class DestroyViewerEvent : public nsRunnable
{
public:
  DestroyViewerEvent(nsIContentViewer* aViewer, nsIDocument* aDocument)
    : mViewer(aViewer),
      mDocument(aDocument)
  {}

  NS_IMETHOD Run()
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
  NS_ASSERTION(mContentViewer && mDocument,
               "we're not in the bfcache!");

  // Release the reference to the contentviewer asynchronously so that the
  // document doesn't get nuked mid-mutation.

  nsCOMPtr<nsIRunnable> evt =
    new DestroyViewerEvent(mContentViewer, mDocument);
  nsresult rv = NS_DispatchToCurrentThread(evt);
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
nsSHEntryShared::GetID(uint64_t *aID)
{
  *aID = mID;
  return NS_OK;
}

//*****************************************************************************
//    nsSHEntryShared: nsIMutationObserver
//*****************************************************************************

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
                                     int32_t aModType)
{
}

void
nsSHEntryShared::AttributeChanged(nsIDocument* aDocument,
                                  dom::Element* aElement,
                                  int32_t aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t aModType)
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
nsSHEntryShared::ParentChainChanged(nsIContent *aContent)
{
}
