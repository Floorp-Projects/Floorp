/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG_bryner
#define DEBUG_PAGE_CACHE
#endif

// Local Includes
#include "nsSHEntry.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsIWebNavigation.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsDocShellEditorData.h"
#include "nsIDocShell.h"

namespace dom = mozilla::dom;

// Hardcode this to time out unused content viewers after 30 minutes
#define CONTENT_VIEWER_TIMEOUT_SECONDS 30*60

typedef nsExpirationTracker<nsSHEntry,3> HistoryTrackerBase;
class HistoryTracker : public HistoryTrackerBase {
public:
  // Expire cached contentviewers after 20-30 minutes in the cache.
  HistoryTracker() : HistoryTrackerBase((CONTENT_VIEWER_TIMEOUT_SECONDS/2)*1000) {}
  
protected:
  virtual void NotifyExpired(nsSHEntry* aObj) {
    RemoveObject(aObj);
    aObj->Expire();
  }
};

static HistoryTracker *gHistoryTracker = nsnull;
static PRUint32 gEntryID = 0;
static PRUint64 gEntryDocIdentifier = 0;

nsresult nsSHEntry::Startup()
{
  gHistoryTracker = new HistoryTracker();
  return gHistoryTracker ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void nsSHEntry::Shutdown()
{
  delete gHistoryTracker;
  gHistoryTracker = nsnull;
}

static void StopTrackingEntry(nsSHEntry *aEntry)
{
  if (aEntry->GetExpirationState()->IsTracked()) {
    gHistoryTracker->RemoveObject(aEntry);
  }
}

//*****************************************************************************
//***    nsSHEntry: Object Management
//*****************************************************************************


nsSHEntry::nsSHEntry() 
  : mLoadType(0)
  , mID(gEntryID++)
  , mPageIdentifier(mID)
  , mDocIdentifier(gEntryDocIdentifier++)
  , mScrollPositionX(0)
  , mScrollPositionY(0)
  , mIsFrameNavigation(PR_FALSE)
  , mSaveLayoutState(PR_TRUE)
  , mExpired(PR_FALSE)
  , mSticky(PR_TRUE)
  , mDynamicallyCreated(PR_FALSE)
  , mParent(nsnull)
  , mViewerBounds(0, 0, 0, 0)
  , mDocShellID(0)
  , mLastTouched(0)
{
}

nsSHEntry::nsSHEntry(const nsSHEntry &other)
  : mURI(other.mURI)
  , mReferrerURI(other.mReferrerURI)
  // XXX why not copy mDocument?
  , mTitle(other.mTitle)
  , mPostData(other.mPostData)
  , mLayoutHistoryState(other.mLayoutHistoryState)
  , mLoadType(0)         // XXX why not copy?
  , mID(other.mID)
  , mPageIdentifier(other.mPageIdentifier)
  , mDocIdentifier(other.mDocIdentifier)
  , mScrollPositionX(0)  // XXX why not copy?
  , mScrollPositionY(0)  // XXX why not copy?
  , mIsFrameNavigation(other.mIsFrameNavigation)
  , mSaveLayoutState(other.mSaveLayoutState)
  , mExpired(other.mExpired)
  , mSticky(PR_TRUE)
  , mDynamicallyCreated(other.mDynamicallyCreated)
  // XXX why not copy mContentType?
  , mCacheKey(other.mCacheKey)
  , mParent(other.mParent)
  , mViewerBounds(0, 0, 0, 0)
  , mOwner(other.mOwner)
  , mStateData(other.mStateData)
  , mDocShellID(other.mDocShellID)
{
}

static PRBool
ClearParentPtr(nsISHEntry* aEntry, void* /* aData */)
{
  if (aEntry) {
    aEntry->SetParent(nsnull);
  }
  return PR_TRUE;
}

nsSHEntry::~nsSHEntry()
{
  StopTrackingEntry(this);

  // Since we never really remove kids from SHEntrys, we need to null
  // out the mParent pointers on all our kids.
  mChildren.EnumerateForwards(ClearParentPtr, nsnull);
  mChildren.Clear();

  if (mContentViewer) {
    // RemoveFromBFCacheSync is virtual, so call the nsSHEntry version
    // explicitly
    nsSHEntry::RemoveFromBFCacheSync();
  }

  mEditorData = nsnull;

#ifdef DEBUG
  // This is not happening as far as I can tell from breakpad as of early November 2007
  nsExpirationTracker<nsSHEntry,3>::Iterator iterator(gHistoryTracker);
  nsSHEntry* elem;
  while ((elem = iterator.Next()) != nsnull) {
    NS_ASSERTION(elem != this, "Found dead entry still in the tracker!");
  }
#endif
}

//*****************************************************************************
//    nsSHEntry: nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS5(nsSHEntry, nsISHContainer, nsISHEntry, nsIHistoryEntry,
                   nsIMutationObserver, nsISHEntryInternal)

//*****************************************************************************
//    nsSHEntry: nsISHEntry
//*****************************************************************************

NS_IMETHODIMP nsSHEntry::SetScrollPosition(PRInt32 x, PRInt32 y)
{
  mScrollPositionX = x;
  mScrollPositionY = y;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetScrollPosition(PRInt32 *x, PRInt32 *y)
{
  *x = mScrollPositionX;
  *y = mScrollPositionY;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetURI(nsIURI** aURI)
{
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetURI(nsIURI* aURI)
{
  mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetReferrerURI(nsIURI **aReferrerURI)
{
  *aReferrerURI = mReferrerURI;
  NS_IF_ADDREF(*aReferrerURI);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetReferrerURI(nsIURI *aReferrerURI)
{
  mReferrerURI = aReferrerURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetContentViewer(nsIContentViewer *aViewer)
{
  NS_PRECONDITION(!aViewer || !mContentViewer, "SHEntry already contains viewer");

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

NS_IMETHODIMP
nsSHEntry::GetContentViewer(nsIContentViewer **aResult)
{
  *aResult = mContentViewer;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetAnyContentViewer(nsISHEntry **aOwnerEntry,
                               nsIContentViewer **aResult)
{
  // Find a content viewer in the root node or any of its children,
  // assuming that there is only one content viewer total in any one
  // nsSHEntry tree
  GetContentViewer(aResult);
  if (*aResult) {
#ifdef DEBUG_PAGE_CACHE 
    printf("Found content viewer\n");
#endif
    *aOwnerEntry = this;
    NS_ADDREF(*aOwnerEntry);
    return NS_OK;
  }
  // The root SHEntry doesn't have a ContentViewer, so check child nodes
  for (PRInt32 i = 0; i < mChildren.Count(); i++) {
    nsISHEntry* child = mChildren[i];
    if (child) {
#ifdef DEBUG_PAGE_CACHE
      printf("Evaluating SHEntry child %d\n", i);
#endif
      child->GetAnyContentViewer(aOwnerEntry, aResult);
      if (*aResult) {
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSticky(PRBool aSticky)
{
  mSticky = aSticky;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSticky(PRBool *aSticky)
{
  *aSticky = mSticky;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetTitle(PRUnichar** aTitle)
{
  // Check for empty title...
  if (mTitle.IsEmpty() && mURI) {
    // Default title is the URL.
    nsCAutoString spec;
    if (NS_SUCCEEDED(mURI->GetSpec(spec)))
      AppendUTF8toUTF16(spec, mTitle);
  }

  *aTitle = ToNewUnicode(mTitle);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetTitle(const nsAString &aTitle)
{
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetPostData(nsIInputStream** aResult)
{
  *aResult = mPostData;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetPostData(nsIInputStream* aPostData)
{
  mPostData = aPostData;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetLayoutHistoryState(nsILayoutHistoryState** aResult)
{
  *aResult = mLayoutHistoryState;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetLayoutHistoryState(nsILayoutHistoryState* aState)
{
  mLayoutHistoryState = aState;
  if (mLayoutHistoryState)
    mLayoutHistoryState->SetScrollPositionOnly(!mSaveLayoutState);

  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetLoadType(PRUint32 * aResult)
{
  *aResult = mLoadType;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetLoadType(PRUint32  aLoadType)
{
  mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetID(PRUint32 * aResult)
{
  *aResult = mID;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetID(PRUint32  aID)
{
  mID = aID;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetPageIdentifier(PRUint32 * aResult)
{
  *aResult = mPageIdentifier;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetPageIdentifier(PRUint32 aPageIdentifier)
{
  mPageIdentifier = aPageIdentifier;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetDocIdentifier(PRUint64 * aResult)
{
  *aResult = mDocIdentifier;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetDocIdentifier(PRUint64 aDocIdentifier)
{
  // This ensures that after a session restore, gEntryDocIdentifier is greater
  // than all SHEntries' docIdentifiers, which ensures that we'll never repeat
  // a doc identifier.
  if (aDocIdentifier >= gEntryDocIdentifier)
    gEntryDocIdentifier = aDocIdentifier + 1;

  mDocIdentifier = aDocIdentifier;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetUniqueDocIdentifier()
{
  mDocIdentifier = gEntryDocIdentifier++;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetIsSubFrame(PRBool * aFlag)
{
  *aFlag = mIsFrameNavigation;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetIsSubFrame(PRBool  aFlag)
{
  mIsFrameNavigation = aFlag;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetCacheKey(nsISupports** aResult)
{
  *aResult = mCacheKey;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetCacheKey(nsISupports* aCacheKey)
{
  mCacheKey = aCacheKey;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetSaveLayoutStateFlag(PRBool * aFlag)
{
  *aFlag = mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetSaveLayoutStateFlag(PRBool  aFlag)
{
  mSaveLayoutState = aFlag;
  if (mLayoutHistoryState)
    mLayoutHistoryState->SetScrollPositionOnly(!aFlag);

  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetExpirationStatus(PRBool * aFlag)
{
  *aFlag = mExpired;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetExpirationStatus(PRBool  aFlag)
{
  mExpired = aFlag;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetContentType(nsACString& aContentType)
{
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetContentType(const nsACString& aContentType)
{
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Create(nsIURI * aURI, const nsAString &aTitle,
                  nsIInputStream * aInputStream,
                  nsILayoutHistoryState * aLayoutHistoryState,
                  nsISupports * aCacheKey, const nsACString& aContentType,
                  nsISupports* aOwner,
                  PRUint64 aDocShellID, PRBool aDynamicCreation)
{
  mURI = aURI;
  mTitle = aTitle;
  mPostData = aInputStream;
  mCacheKey = aCacheKey;
  mContentType = aContentType;
  mOwner = aOwner;
  mDocShellID = aDocShellID;
  mDynamicallyCreated = aDynamicCreation;

  // Set the LoadType by default to loadHistory during creation
  mLoadType = (PRUint32) nsIDocShellLoadInfo::loadHistory;

  // By default all entries are set false for subframe flag. 
  // nsDocShell::CloneAndReplace() which creates entries for
  // all subframe navigations, sets the flag to true.
  mIsFrameNavigation = PR_FALSE;

  // By default we save LayoutHistoryState
  mSaveLayoutState = PR_TRUE;
  mLayoutHistoryState = aLayoutHistoryState;

  //By default the page is not expired
  mExpired = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Clone(nsISHEntry ** aResult)
{
  *aResult = new nsSHEntry(*this);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetParent(nsISHEntry ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mParent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetParent(nsISHEntry * aParent)
{
  /* parent not Addrefed on purpose to avoid cyclic reference
   * Null parent is OK
   *
   * XXX this method should not be scriptable if this is the case!!
   */
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetWindowState(nsISupports *aState)
{
  mWindowState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetWindowState(nsISupports **aState)
{
  NS_IF_ADDREF(*aState = mWindowState);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetViewerBounds(const nsIntRect &aBounds)
{
  mViewerBounds = aBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetViewerBounds(nsIntRect &aBounds)
{
  aBounds = mViewerBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetOwner(nsISupports **aOwner)
{
  NS_IF_ADDREF(*aOwner = mOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetOwner(nsISupports *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

//*****************************************************************************
//    nsSHEntry: nsISHContainer
//*****************************************************************************

NS_IMETHODIMP 
nsSHEntry::GetChildCount(PRInt32 * aCount)
{
  *aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChild(nsISHEntry * aChild, PRInt32 aOffset)
{
  if (aChild) {
    NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);
  }

  if (aOffset < 0) {
    mChildren.AppendObject(aChild);
    return NS_OK;
  }

  //
  // Bug 52670: Ensure children are added in order.
  //
  //  Later frames in the child list may load faster and get appended
  //  before earlier frames, causing session history to be scrambled.
  //  By growing the list here, they are added to the right position.
  //
  //  Assert that aOffset will not be so high as to grow us a lot.
  //
  NS_ASSERTION(aOffset < (mChildren.Count()+1023), "Large frames array!\n");

  PRBool newChildIsDyn = PR_FALSE;
  if (aChild) {
    aChild->IsDynamicallyAdded(&newChildIsDyn);
  }

  // If the new child is dynamically added, try to add it to aOffset, but if
  // there are non-dynamically added children, the child must be after those.
  if (newChildIsDyn) {
    PRInt32 lastNonDyn = aOffset - 1;
    for (PRInt32 i = aOffset; i < mChildren.Count(); ++i) {
      nsISHEntry* entry = mChildren[i];
      if (entry) {
        PRBool dyn = PR_FALSE;
        entry->IsDynamicallyAdded(&dyn);
        if (dyn) {
          break;
        } else {
          lastNonDyn = i;
        }
      }
    }
    // InsertObjectAt allows only appending one object.
    // If aOffset is larger than Count(), we must first manually
    // set the capacity.
    if (aOffset > mChildren.Count()) {
      mChildren.SetCount(aOffset);
    }
    if (!mChildren.InsertObjectAt(aChild, lastNonDyn + 1)) {
      NS_WARNING("Adding a child failed!");
      aChild->SetParent(nsnull);
      return NS_ERROR_FAILURE;
    }
  } else {
    // If the new child isn't dynamically added, it should be set to aOffset.
    // If there are dynamically added children before that, those must be
    // moved to be after aOffset.
    if (mChildren.Count() > 0) {
      PRInt32 start = PR_MIN(mChildren.Count() - 1, aOffset);
      PRInt32 dynEntryIndex = -1;
      nsISHEntry* dynEntry = nsnull;
      for (PRInt32 i = start; i >= 0; --i) {
        nsISHEntry* entry = mChildren[i];
        if (entry) {
          PRBool dyn = PR_FALSE;
          entry->IsDynamicallyAdded(&dyn);
          if (dyn) {
            dynEntryIndex = i;
            dynEntry = entry;
          } else {
            break;
          }
        }
      }
  
      if (dynEntry) {
        nsCOMArray<nsISHEntry> tmp;
        tmp.SetCount(aOffset - dynEntryIndex + 1);
        mChildren.InsertObjectsAt(tmp, dynEntryIndex);
        NS_ASSERTION(mChildren[aOffset + 1] == dynEntry, "Whaat?");
      }
    }
    

    // Make sure there isn't anything at aOffset.
    if (aOffset < mChildren.Count()) {
      nsISHEntry* oldChild = mChildren[aOffset];
      if (oldChild && oldChild != aChild) {
        NS_ERROR("Adding a child where we already have a child? This may misbehave");
        oldChild->SetParent(nsnull);
      }
    }

    if (!mChildren.ReplaceObjectAt(aChild, aOffset)) {
      NS_WARNING("Adding a child failed!");
      aChild->SetParent(nsnull);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry * aChild)
{
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);
  PRBool childRemoved = PR_FALSE;
  PRBool dynamic = PR_FALSE;
  aChild->IsDynamicallyAdded(&dynamic);
  if (dynamic) {
    childRemoved = mChildren.RemoveObject(aChild);
  } else {
    PRInt32 index = mChildren.IndexOfObject(aChild);
    if (index >= 0) {
      childRemoved = mChildren.ReplaceObjectAt(nsnull, index);
    }
  }
  if (childRemoved)
    aChild->SetParent(nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetChildAt(PRInt32 aIndex, nsISHEntry ** aResult)
{
  if (aIndex >= 0 && aIndex < mChildren.Count()) {
    *aResult = mChildren[aIndex];
    // yes, mChildren can have holes in it.  AddChild's offset parameter makes
    // that possible.
    NS_IF_ADDREF(*aResult);
  } else {
    *aResult = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChildShell(nsIDocShellTreeItem *aShell)
{
  NS_ASSERTION(aShell, "Null child shell added to history entry");
  mChildShells.AppendObject(aShell);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::ChildShellAt(PRInt32 aIndex, nsIDocShellTreeItem **aShell)
{
  NS_IF_ADDREF(*aShell = mChildShells.SafeObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::ClearChildShells()
{
  mChildShells.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetRefreshURIList(nsISupportsArray **aList)
{
  NS_IF_ADDREF(*aList = mRefreshURIList);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetRefreshURIList(nsISupportsArray *aList)
{
  mRefreshURIList = aList;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SyncPresentationState()
{
  if (mContentViewer && mWindowState) {
    // If we have a content viewer and a window state, we should be ok.
    return NS_OK;
  }

  DropPresentationState();

  return NS_OK;
}

void
nsSHEntry::DropPresentationState()
{
  nsRefPtr<nsSHEntry> kungFuDeathGrip = this;

  if (mDocument) {
    mDocument->SetBFCacheEntry(nsnull);
    mDocument->RemoveMutationObserver(this);
    mDocument = nsnull;
  }
  if (mContentViewer)
    mContentViewer->ClearHistoryEntry();

  StopTrackingEntry(this);
  mContentViewer = nsnull;
  mSticky = PR_TRUE;
  mWindowState = nsnull;
  mViewerBounds.SetRect(0, 0, 0, 0);
  mChildShells.Clear();
  mRefreshURIList = nsnull;
  mEditorData = nsnull;
}

void
nsSHEntry::Expire()
{
  // This entry has timed out. If we still have a content viewer, we need to
  // get it evicted.
  if (!mContentViewer)
    return;
  nsCOMPtr<nsISupports> container;
  mContentViewer->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(container);
  if (!treeItem)
    return;
  // We need to find the root DocShell since only that object has an
  // SHistory and we need the SHistory to evict content viewers
  nsCOMPtr<nsIDocShellTreeItem> root;
  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(root);
  nsCOMPtr<nsISHistory> history;
  webNav->GetSessionHistory(getter_AddRefs(history));
  nsCOMPtr<nsISHistoryInternal> historyInt = do_QueryInterface(history);
  if (!historyInt)
    return;
  historyInt->EvictExpiredContentViewerForEntry(this);
}

//*****************************************************************************
//    nsSHEntry: nsIMutationObserver
//*****************************************************************************

void
nsSHEntry::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_NOTREACHED("Document destroyed while we're holding a strong ref to it");
}

void
nsSHEntry::CharacterDataWillChange(nsIDocument* aDocument,
                                   nsIContent* aContent,
                                   CharacterDataChangeInfo* aInfo)
{
}

void
nsSHEntry::CharacterDataChanged(nsIDocument* aDocument,
                                nsIContent* aContent,
                                CharacterDataChangeInfo* aInfo)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntry::AttributeWillChange(nsIDocument* aDocument,
                               dom::Element* aContent,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType)
{
}

void
nsSHEntry::AttributeChanged(nsIDocument* aDocument,
                            dom::Element* aElement,
                            PRInt32 aNameSpaceID,
                            nsIAtom* aAttribute,
                            PRInt32 aModType)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntry::ContentAppended(nsIDocument* aDocument,
                           nsIContent* aContainer,
                           nsIContent* aFirstNewContent,
                           PRInt32 /* unused */)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntry::ContentInserted(nsIDocument* aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 /* unused */)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntry::ContentRemoved(nsIDocument* aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32 aIndexInContainer,
                          nsIContent* aPreviousSibling)
{
  RemoveFromBFCacheAsync();
}

void
nsSHEntry::ParentChainChanged(nsIContent *aContent)
{
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
    if (mViewer)
      mViewer->Destroy();
    return NS_OK;
  }

  nsCOMPtr<nsIContentViewer> mViewer;
  nsCOMPtr<nsIDocument> mDocument;
};

void
nsSHEntry::RemoveFromBFCacheSync()
{
  NS_ASSERTION(mContentViewer && mDocument,
               "we're not in the bfcache!");

  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  DropPresentationState();

  // Warning! The call to DropPresentationState could have dropped the last
  // reference to this nsSHEntry, so no accessing members beyond here.

  if (viewer) {
    viewer->Destroy();
  }
}

void
nsSHEntry::RemoveFromBFCacheAsync()
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
  }
  else {
    // Drop presentation. Also ensures that we don't post more then one
    // PLEvent. Only do this if we succeeded in posting the event since
    // otherwise the document could be torn down mid mutation causing crashes.
    DropPresentationState();
  }
  // Warning! The call to DropPresentationState could have dropped the last
  // reference to this nsSHEntry, so no accessing members beyond here.
}

nsDocShellEditorData*
nsSHEntry::ForgetEditorData()
{
  return mEditorData.forget();
}

void
nsSHEntry::SetEditorData(nsDocShellEditorData* aData)
{
  NS_ASSERTION(!(aData && mEditorData),
               "We're going to overwrite an owning ref!");
  if (mEditorData != aData)
    mEditorData = aData;
}

PRBool
nsSHEntry::HasDetachedEditor()
{
  return mEditorData != nsnull;
}

NS_IMETHODIMP
nsSHEntry::GetStateData(nsAString &aStateData)
{
  aStateData.Assign(mStateData);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetStateData(const nsAString &aDataStr)
{
  mStateData.Assign(aDataStr);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::IsDynamicallyAdded(PRBool* aAdded)
{
  *aAdded = mDynamicallyCreated;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::HasDynamicallyAddedChild(PRBool* aAdded)
{
  *aAdded = PR_FALSE;
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    nsISHEntry* entry = mChildren[i];
    if (entry) {
      entry->IsDynamicallyAdded(aAdded);
      if (*aAdded) {
        break;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetDocshellID(PRUint64* aID)
{
  *aID = mDocShellID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetDocshellID(PRUint64 aID)
{
  mDocShellID = aID;
  return NS_OK;
}


NS_IMETHODIMP
nsSHEntry::GetLastTouched(PRUint32 *aLastTouched)
{
  *aLastTouched = mLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLastTouched(PRUint32 aLastTouched)
{
  mLastTouched = aLastTouched;
  return NS_OK;
}

