/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHEntry.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsDocShellEditorData.h"
#include "nsSHEntryShared.h"
#include "nsILayoutHistoryState.h"
#include "nsIContentViewer.h"
#include "nsIStructuredCloneContainer.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "mozilla/net/ReferrerPolicy.h"
#include <algorithm>

namespace dom = mozilla::dom;

static uint32_t gEntryID = 0;

nsSHEntry::nsSHEntry()
  : mShared(new nsSHEntryShared())
  , mLoadReplace(false)
  , mReferrerPolicy(mozilla::net::RP_Default)
  , mLoadType(0)
  , mID(gEntryID++)
  , mScrollPositionX(0)
  , mScrollPositionY(0)
  , mParent(nullptr)
  , mURIWasModified(false)
  , mIsSrcdocEntry(false)
  , mScrollRestorationIsManual(false)
{
}

nsSHEntry::nsSHEntry(const nsSHEntry& aOther)
  : mShared(aOther.mShared)
  , mURI(aOther.mURI)
  , mOriginalURI(aOther.mOriginalURI)
  , mLoadReplace(aOther.mLoadReplace)
  , mReferrerURI(aOther.mReferrerURI)
  , mReferrerPolicy(aOther.mReferrerPolicy)
  , mTitle(aOther.mTitle)
  , mPostData(aOther.mPostData)
  , mLoadType(0)         // XXX why not copy?
  , mID(aOther.mID)
  , mScrollPositionX(0)  // XXX why not copy?
  , mScrollPositionY(0)  // XXX why not copy?
  , mParent(aOther.mParent)
  , mURIWasModified(aOther.mURIWasModified)
  , mStateData(aOther.mStateData)
  , mIsSrcdocEntry(aOther.mIsSrcdocEntry)
  , mScrollRestorationIsManual(false)
  , mSrcdocData(aOther.mSrcdocData)
  , mBaseURI(aOther.mBaseURI)
{
}

static bool
ClearParentPtr(nsISHEntry* aEntry, void* /* aData */)
{
  if (aEntry) {
    aEntry->SetParent(nullptr);
  }
  return true;
}

nsSHEntry::~nsSHEntry()
{
  // Null out the mParent pointers on all our kids.
  mChildren.EnumerateForwards(ClearParentPtr, nullptr);
}

NS_IMPL_ISUPPORTS(nsSHEntry, nsISHContainer, nsISHEntry, nsISHEntryInternal)

NS_IMETHODIMP
nsSHEntry::SetScrollPosition(int32_t aX, int32_t aY)
{
  mScrollPositionX = aX;
  mScrollPositionY = aY;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetScrollPosition(int32_t* aX, int32_t* aY)
{
  *aX = mScrollPositionX;
  *aY = mScrollPositionY;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetURIWasModified(bool* aOut)
{
  *aOut = mURIWasModified;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetURIWasModified(bool aIn)
{
  mURIWasModified = aIn;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetURI(nsIURI** aURI)
{
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetURI(nsIURI* aURI)
{
  mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetOriginalURI(nsIURI** aOriginalURI)
{
  *aOriginalURI = mOriginalURI;
  NS_IF_ADDREF(*aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetOriginalURI(nsIURI* aOriginalURI)
{
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLoadReplace(bool* aLoadReplace)
{
  *aLoadReplace = mLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLoadReplace(bool aLoadReplace)
{
  mLoadReplace = aLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetReferrerURI(nsIURI** aReferrerURI)
{
  *aReferrerURI = mReferrerURI;
  NS_IF_ADDREF(*aReferrerURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetReferrerURI(nsIURI* aReferrerURI)
{
  mReferrerURI = aReferrerURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetReferrerPolicy(uint32_t* aReferrerPolicy)
{
  *aReferrerPolicy = mReferrerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetReferrerPolicy(uint32_t aReferrerPolicy)
{
  mReferrerPolicy = aReferrerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetContentViewer(nsIContentViewer* aViewer)
{
  return mShared->SetContentViewer(aViewer);
}

NS_IMETHODIMP
nsSHEntry::GetContentViewer(nsIContentViewer** aResult)
{
  *aResult = mShared->mContentViewer;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetAnyContentViewer(nsISHEntry** aOwnerEntry,
                               nsIContentViewer** aResult)
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
  for (int32_t i = 0; i < mChildren.Count(); i++) {
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
nsSHEntry::SetSticky(bool aSticky)
{
  mShared->mSticky = aSticky;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSticky(bool* aSticky)
{
  *aSticky = mShared->mSticky;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetTitle(char16_t** aTitle)
{
  // Check for empty title...
  if (mTitle.IsEmpty() && mURI) {
    // Default title is the URL.
    nsAutoCString spec;
    if (NS_SUCCEEDED(mURI->GetSpec(spec))) {
      AppendUTF8toUTF16(spec, mTitle);
    }
  }

  *aTitle = ToNewUnicode(mTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetTitle(const nsAString& aTitle)
{
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPostData(nsIInputStream** aResult)
{
  *aResult = mPostData;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPostData(nsIInputStream* aPostData)
{
  mPostData = aPostData;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLayoutHistoryState(nsILayoutHistoryState** aResult)
{
  *aResult = mShared->mLayoutHistoryState;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLayoutHistoryState(nsILayoutHistoryState* aState)
{
  mShared->mLayoutHistoryState = aState;
  if (mShared->mLayoutHistoryState) {
    mShared->mLayoutHistoryState->SetScrollPositionOnly(
      !mShared->mSaveLayoutState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLoadType(uint32_t* aResult)
{
  *aResult = mLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLoadType(uint32_t aLoadType)
{
  mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetID(uint32_t* aResult)
{
  *aResult = mID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetID(uint32_t aID)
{
  mID = aID;
  return NS_OK;
}

nsSHEntryShared*
nsSHEntry::GetSharedState()
{
  return mShared;
}

NS_IMETHODIMP
nsSHEntry::GetIsSubFrame(bool* aFlag)
{
  *aFlag = mShared->mIsFrameNavigation;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetIsSubFrame(bool aFlag)
{
  mShared->mIsFrameNavigation = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetCacheKey(nsISupports** aResult)
{
  *aResult = mShared->mCacheKey;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetCacheKey(nsISupports* aCacheKey)
{
  mShared->mCacheKey = aCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSaveLayoutStateFlag(bool* aFlag)
{
  *aFlag = mShared->mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSaveLayoutStateFlag(bool aFlag)
{
  mShared->mSaveLayoutState = aFlag;
  if (mShared->mLayoutHistoryState) {
    mShared->mLayoutHistoryState->SetScrollPositionOnly(!aFlag);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetExpirationStatus(bool* aFlag)
{
  *aFlag = mShared->mExpired;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetExpirationStatus(bool aFlag)
{
  mShared->mExpired = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetContentType(nsACString& aContentType)
{
  aContentType = mShared->mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetContentType(const nsACString& aContentType)
{
  mShared->mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Create(nsIURI* aURI, const nsAString& aTitle,
                  nsIInputStream* aInputStream,
                  nsILayoutHistoryState* aLayoutHistoryState,
                  nsISupports* aCacheKey, const nsACString& aContentType,
                  nsIPrincipal* aTriggeringPrincipal,
                  nsIPrincipal* aPrincipalToInherit,
                  const nsID& aDocShellID,
                  bool aDynamicCreation)
{
  mURI = aURI;
  mTitle = aTitle;
  mPostData = aInputStream;

  // Set the LoadType by default to loadHistory during creation
  mLoadType = (uint32_t)nsIDocShellLoadInfo::loadHistory;

  mShared->mCacheKey = aCacheKey;
  mShared->mContentType = aContentType;
  mShared->mTriggeringPrincipal = aTriggeringPrincipal;
  mShared->mPrincipalToInherit = aPrincipalToInherit;
  mShared->mDocShellID = aDocShellID;
  mShared->mDynamicallyCreated = aDynamicCreation;

  // By default all entries are set false for subframe flag.
  // nsDocShell::CloneAndReplace() which creates entries for
  // all subframe navigations, sets the flag to true.
  mShared->mIsFrameNavigation = false;

  // By default we save LayoutHistoryState
  mShared->mSaveLayoutState = true;
  mShared->mLayoutHistoryState = aLayoutHistoryState;

  // By default the page is not expired
  mShared->mExpired = false;

  mIsSrcdocEntry = false;
  mSrcdocData = NullString();

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Clone(nsISHEntry** aResult)
{
  *aResult = new nsSHEntry(*this);
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetParent(nsISHEntry** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mParent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetParent(nsISHEntry* aParent)
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
nsSHEntry::SetWindowState(nsISupports* aState)
{
  mShared->mWindowState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetWindowState(nsISupports** aState)
{
  NS_IF_ADDREF(*aState = mShared->mWindowState);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetViewerBounds(const nsIntRect& aBounds)
{
  mShared->mViewerBounds = aBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetViewerBounds(nsIntRect& aBounds)
{
  aBounds = mShared->mViewerBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal)
{
  NS_IF_ADDREF(*aTriggeringPrincipal = mShared->mTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal)
{
  mShared->mTriggeringPrincipal = aTriggeringPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPrincipalToInherit(nsIPrincipal** aPrincipalToInherit)
{
  NS_IF_ADDREF(*aPrincipalToInherit = mShared->mPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit)
{
  mShared->mPrincipalToInherit = aPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetBFCacheEntry(nsIBFCacheEntry** aEntry)
{
  NS_ENSURE_ARG_POINTER(aEntry);
  NS_IF_ADDREF(*aEntry = mShared);
  return NS_OK;
}

bool
nsSHEntry::HasBFCacheEntry(nsIBFCacheEntry* aEntry)
{
  return static_cast<nsIBFCacheEntry*>(mShared) == aEntry;
}

NS_IMETHODIMP
nsSHEntry::AdoptBFCacheEntry(nsISHEntry* aEntry)
{
  nsCOMPtr<nsISHEntryInternal> shEntry = do_QueryInterface(aEntry);
  NS_ENSURE_STATE(shEntry);

  nsSHEntryShared* shared = shEntry->GetSharedState();
  NS_ENSURE_STATE(shared);

  mShared = shared;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SharesDocumentWith(nsISHEntry* aEntry, bool* aOut)
{
  NS_ENSURE_ARG_POINTER(aOut);

  nsCOMPtr<nsISHEntryInternal> internal = do_QueryInterface(aEntry);
  NS_ENSURE_STATE(internal);

  *aOut = mShared == internal->GetSharedState();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AbandonBFCacheEntry()
{
  mShared = nsSHEntryShared::Duplicate(mShared);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetIsSrcdocEntry(bool* aIsSrcdocEntry)
{
  *aIsSrcdocEntry = mIsSrcdocEntry;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSrcdocData(nsAString& aSrcdocData)
{
  aSrcdocData = mSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSrcdocData(const nsAString& aSrcdocData)
{
  mSrcdocData = aSrcdocData;
  mIsSrcdocEntry = true;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetBaseURI(nsIURI** aBaseURI)
{
  *aBaseURI = mBaseURI;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetBaseURI(nsIURI* aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetScrollRestorationIsManual(bool* aIsManual)
{
  *aIsManual = mScrollRestorationIsManual;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetScrollRestorationIsManual(bool aIsManual)
{
  mScrollRestorationIsManual = aIsManual;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetChildCount(int32_t* aCount)
{
  *aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChild(nsISHEntry* aChild, int32_t aOffset)
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
  NS_ASSERTION(aOffset < (mChildren.Count() + 1023), "Large frames array!\n");

  bool newChildIsDyn = false;
  if (aChild) {
    aChild->IsDynamicallyAdded(&newChildIsDyn);
  }

  // If the new child is dynamically added, try to add it to aOffset, but if
  // there are non-dynamically added children, the child must be after those.
  if (newChildIsDyn) {
    int32_t lastNonDyn = aOffset - 1;
    for (int32_t i = aOffset; i < mChildren.Count(); ++i) {
      nsISHEntry* entry = mChildren[i];
      if (entry) {
        bool dyn = false;
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
      aChild->SetParent(nullptr);
      return NS_ERROR_FAILURE;
    }
  } else {
    // If the new child isn't dynamically added, it should be set to aOffset.
    // If there are dynamically added children before that, those must be
    // moved to be after aOffset.
    if (mChildren.Count() > 0) {
      int32_t start = std::min(mChildren.Count() - 1, aOffset);
      int32_t dynEntryIndex = -1;
      nsISHEntry* dynEntry = nullptr;
      for (int32_t i = start; i >= 0; --i) {
        nsISHEntry* entry = mChildren[i];
        if (entry) {
          bool dyn = false;
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
        oldChild->SetParent(nullptr);
      }
    }

    mChildren.ReplaceObjectAt(aChild, aOffset);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry* aChild)
{
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);
  bool childRemoved = false;
  bool dynamic = false;
  aChild->IsDynamicallyAdded(&dynamic);
  if (dynamic) {
    childRemoved = mChildren.RemoveObject(aChild);
  } else {
    int32_t index = mChildren.IndexOfObject(aChild);
    if (index >= 0) {
      mChildren.ReplaceObjectAt(nullptr, index);
      childRemoved = true;
    }
  }
  if (childRemoved) {
    aChild->SetParent(nullptr);

    // reduce the child count, i.e. remove empty children at the end
    for (int32_t i = mChildren.Count() - 1; i >= 0 && !mChildren[i]; --i) {
      if (!mChildren.RemoveObjectAt(i)) {
        break;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetChildAt(int32_t aIndex, nsISHEntry** aResult)
{
  if (aIndex >= 0 && aIndex < mChildren.Count()) {
    *aResult = mChildren[aIndex];
    // yes, mChildren can have holes in it.  AddChild's offset parameter makes
    // that possible.
    NS_IF_ADDREF(*aResult);
  } else {
    *aResult = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::ReplaceChild(nsISHEntry* aNewEntry)
{
  NS_ENSURE_STATE(aNewEntry);

  nsID docshellID = aNewEntry->DocshellID();

  for (int32_t i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i] && docshellID == mChildren[i]->DocshellID()) {
      mChildren[i]->SetParent(nullptr);
      mChildren.ReplaceObjectAt(aNewEntry, i);
      return aNewEntry->SetParent(this);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSHEntry::AddChildShell(nsIDocShellTreeItem* aShell)
{
  NS_ASSERTION(aShell, "Null child shell added to history entry");
  mShared->mChildShells.AppendObject(aShell);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::ChildShellAt(int32_t aIndex, nsIDocShellTreeItem** aShell)
{
  NS_IF_ADDREF(*aShell = mShared->mChildShells.SafeObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::ClearChildShells()
{
  mShared->mChildShells.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetRefreshURIList(nsIMutableArray** aList)
{
  NS_IF_ADDREF(*aList = mShared->mRefreshURIList);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetRefreshURIList(nsIMutableArray* aList)
{
  mShared->mRefreshURIList = aList;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SyncPresentationState()
{
  return mShared->SyncPresentationState();
}

void
nsSHEntry::RemoveFromBFCacheSync()
{
  mShared->RemoveFromBFCacheSync();
}

void
nsSHEntry::RemoveFromBFCacheAsync()
{
  mShared->RemoveFromBFCacheAsync();
}

nsDocShellEditorData*
nsSHEntry::ForgetEditorData()
{
  // XXX jlebar Check how this is used.
  return mShared->mEditorData.forget();
}

void
nsSHEntry::SetEditorData(nsDocShellEditorData* aData)
{
  NS_ASSERTION(!(aData && mShared->mEditorData),
               "We're going to overwrite an owning ref!");
  if (mShared->mEditorData != aData) {
    mShared->mEditorData = aData;
  }
}

bool
nsSHEntry::HasDetachedEditor()
{
  return mShared->mEditorData != nullptr;
}

NS_IMETHODIMP
nsSHEntry::GetStateData(nsIStructuredCloneContainer** aContainer)
{
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_IF_ADDREF(*aContainer = mStateData);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetStateData(nsIStructuredCloneContainer* aContainer)
{
  mStateData = aContainer;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::IsDynamicallyAdded(bool* aAdded)
{
  *aAdded = mShared->mDynamicallyCreated;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::HasDynamicallyAddedChild(bool* aAdded)
{
  *aAdded = false;
  for (int32_t i = 0; i < mChildren.Count(); ++i) {
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
nsSHEntry::GetDocshellID(nsID** aID)
{
  *aID = static_cast<nsID*>(nsMemory::Clone(&mShared->mDocShellID, sizeof(nsID)));
  return NS_OK;
}

const nsID
nsSHEntry::DocshellID()
{
  return mShared->mDocShellID;
}

NS_IMETHODIMP
nsSHEntry::SetDocshellID(const nsID* aID)
{
  mShared->mDocShellID = *aID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLastTouched(uint32_t* aLastTouched)
{
  *aLastTouched = mShared->mLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLastTouched(uint32_t aLastTouched)
{
  mShared->mLastTouched = aLastTouched;
  return NS_OK;
}
