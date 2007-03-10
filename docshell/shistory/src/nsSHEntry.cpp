/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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


static PRUint32 gEntryID = 0;

//*****************************************************************************
//***    nsSHEntry: Object Management
//*****************************************************************************

nsSHEntry::nsSHEntry() 
  : mLoadType(0)
  , mID(gEntryID++)
  , mPageIdentifier(mID)
  , mScrollPositionX(0)
  , mScrollPositionY(0)
  , mIsFrameNavigation(PR_FALSE)
  , mSaveLayoutState(PR_TRUE)
  , mExpired(PR_FALSE)
  , mSticky(PR_TRUE)
  , mParent(nsnull)
  , mViewerBounds(0, 0, 0, 0)
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
  , mScrollPositionX(0)  // XXX why not copy?
  , mScrollPositionY(0)  // XXX why not copy?
  , mIsFrameNavigation(other.mIsFrameNavigation)
  , mSaveLayoutState(other.mSaveLayoutState)
  , mExpired(other.mExpired)
  , mSticky(PR_TRUE)
  // XXX why not copy mContentType?
  , mCacheKey(other.mCacheKey)
  , mParent(other.mParent)
  , mViewerBounds(0, 0, 0, 0)
  , mOwner(other.mOwner)
{
}

PR_STATIC_CALLBACK(PRBool)
ClearParentPtr(nsISHEntry* aEntry, void* /* aData */)
{
  if (aEntry) {
    aEntry->SetParent(nsnull);
  }
  return PR_TRUE;
}

nsSHEntry::~nsSHEntry()
{
  // Since we never really remove kids from SHEntrys, we need to null
  // out the mParent pointers on all our kids.
  mChildren.EnumerateForwards(ClearParentPtr, nsnull);
  mChildren.Clear();

  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  DropPresentationState();
  if (viewer) {
    viewer->Destroy();
  }
}

//*****************************************************************************
//    nsSHEntry: nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS4(nsSHEntry, nsISHContainer, nsISHEntry, nsIHistoryEntry,
                   nsIMutationObserver)

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

  if (!aViewer) {
    DropPresentationState();
  }

  mContentViewer = aViewer;

  if (mContentViewer) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
    // Store observed document in strong pointer in case it is removed from
    // the contentviewer
    mDocument = do_QueryInterface(domDoc);
    if (mDocument) {
      mDocument->SetShellsHidden(PR_TRUE);
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
  NS_ENSURE_STATE(mSaveLayoutState || !aState);
  mLayoutHistoryState = aState;
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

  // if we are not allowed to hold layout history state, then make sure
  // that we are not holding it!
  if (!mSaveLayoutState)
    mLayoutHistoryState = nsnull;

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
                  nsISupports* aOwner)
{
  mURI = aURI;
  mTitle = aTitle;
  mPostData = aInputStream;
  mCacheKey = aCacheKey;
  mContentType = aContentType;
  mOwner = aOwner;
    
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
nsSHEntry::SetViewerBounds(const nsRect &aBounds)
{
  mViewerBounds = aBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetViewerBounds(nsRect &aBounds)
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
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);

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

  if (aOffset < mChildren.Count()) {
    nsISHEntry* oldChild = mChildren.ObjectAt(aOffset);
    if (oldChild && oldChild != aChild) {
      NS_ERROR("Adding child where we already have a child?  "
               "This will likely misbehave");
      oldChild->SetParent(nsnull);
    }
  }
  
  // This implicitly extends the array to include aOffset
  mChildren.ReplaceObjectAt(aChild, aOffset);

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry * aChild)
{
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);
  PRBool childRemoved = mChildren.RemoveObject(aChild);
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
    mDocument->SetShellsHidden(PR_FALSE);
    mDocument->RemoveMutationObserver(this);
    mDocument = nsnull;
  }
  if (mContentViewer)
    mContentViewer->ClearHistoryEntry();

  mContentViewer = nsnull;
  mSticky = PR_TRUE;
  mWindowState = nsnull;
  mViewerBounds.SetRect(0, 0, 0, 0);
  mChildShells.Clear();
  mRefreshURIList = nsnull;
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
nsSHEntry::CharacterDataChanged(nsIDocument* aDocument,
                                nsIContent* aContent,
                                CharacterDataChangeInfo* aInfo)
{
  DocumentMutated();
}

void
nsSHEntry::AttributeChanged(nsIDocument* aDocument,
                            nsIContent* aContent,
                            PRInt32 aNameSpaceID,
                            nsIAtom* aAttribute,
                            PRInt32 aModType)
{
  DocumentMutated();
}

void
nsSHEntry::ContentAppended(nsIDocument* aDocument,
                        nsIContent* aContainer,
                        PRInt32 aNewIndexInContainer)
{
  DocumentMutated();
}

void
nsSHEntry::ContentInserted(nsIDocument* aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  DocumentMutated();
}

void
nsSHEntry::ContentRemoved(nsIDocument* aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32 aIndexInContainer)
{
  DocumentMutated();
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
nsSHEntry::DocumentMutated()
{
  NS_ASSERTION(mContentViewer && mDocument,
               "we shouldn't still be observing the doc");

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
