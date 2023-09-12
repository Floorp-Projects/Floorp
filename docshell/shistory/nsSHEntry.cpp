/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHEntry.h"

#include <algorithm>

#include "nsDocShell.h"
#include "nsDocShellEditorData.h"
#include "nsDocShellLoadState.h"
#include "nsDocShellLoadTypes.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContentViewer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInputStream.h"
#include "nsILayoutHistoryState.h"
#include "nsIMutableArray.h"
#include "nsIStructuredCloneContainer.h"
#include "nsIURI.h"
#include "nsSHEntryShared.h"
#include "nsSHistory.h"

#include "mozilla/Logging.h"
#include "nsIReferrerInfo.h"

extern mozilla::LazyLogModule gPageCacheLog;

static uint32_t gEntryID = 0;

nsSHEntry::nsSHEntry()
    : mShared(new nsSHEntryShared()),
      mLoadType(0),
      mID(++gEntryID),  // SessionStore has special handling for 0 values.
      mScrollPositionX(0),
      mScrollPositionY(0),
      mLoadReplace(false),
      mURIWasModified(false),
      mIsSrcdocEntry(false),
      mScrollRestorationIsManual(false),
      mLoadedInThisProcess(false),
      mPersist(true),
      mHasUserInteraction(false),
      mHasUserActivation(false) {}

nsSHEntry::nsSHEntry(const nsSHEntry& aOther)
    : mShared(aOther.mShared),
      mURI(aOther.mURI),
      mOriginalURI(aOther.mOriginalURI),
      mResultPrincipalURI(aOther.mResultPrincipalURI),
      mUnstrippedURI(aOther.mUnstrippedURI),
      mReferrerInfo(aOther.mReferrerInfo),
      mTitle(aOther.mTitle),
      mPostData(aOther.mPostData),
      mLoadType(0),  // XXX why not copy?
      mID(aOther.mID),
      mScrollPositionX(0),  // XXX why not copy?
      mScrollPositionY(0),  // XXX why not copy?
      mParent(aOther.mParent),
      mStateData(aOther.mStateData),
      mSrcdocData(aOther.mSrcdocData),
      mBaseURI(aOther.mBaseURI),
      mLoadReplace(aOther.mLoadReplace),
      mURIWasModified(aOther.mURIWasModified),
      mIsSrcdocEntry(aOther.mIsSrcdocEntry),
      mScrollRestorationIsManual(false),
      mLoadedInThisProcess(aOther.mLoadedInThisProcess),
      mPersist(aOther.mPersist),
      mHasUserInteraction(false),
      mHasUserActivation(aOther.mHasUserActivation) {}

nsSHEntry::~nsSHEntry() {
  // Null out the mParent pointers on all our kids.
  for (nsISHEntry* entry : mChildren) {
    if (entry) {
      entry->SetParent(nullptr);
    }
  }
}

NS_IMPL_ISUPPORTS(nsSHEntry, nsISHEntry, nsISupportsWeakReference)

NS_IMETHODIMP
nsSHEntry::SetScrollPosition(int32_t aX, int32_t aY) {
  mScrollPositionX = aX;
  mScrollPositionY = aY;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetScrollPosition(int32_t* aX, int32_t* aY) {
  *aX = mScrollPositionX;
  *aY = mScrollPositionY;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetURIWasModified(bool* aOut) {
  *aOut = mURIWasModified;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetURIWasModified(bool aIn) {
  mURIWasModified = aIn;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetURI(nsIURI** aURI) {
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetURI(nsIURI* aURI) {
  mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetOriginalURI(nsIURI** aOriginalURI) {
  *aOriginalURI = mOriginalURI;
  NS_IF_ADDREF(*aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetOriginalURI(nsIURI* aOriginalURI) {
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetResultPrincipalURI(nsIURI** aResultPrincipalURI) {
  *aResultPrincipalURI = mResultPrincipalURI;
  NS_IF_ADDREF(*aResultPrincipalURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetResultPrincipalURI(nsIURI* aResultPrincipalURI) {
  mResultPrincipalURI = aResultPrincipalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetUnstrippedURI(nsIURI** aUnstrippedURI) {
  *aUnstrippedURI = mUnstrippedURI;
  NS_IF_ADDREF(*aUnstrippedURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetUnstrippedURI(nsIURI* aUnstrippedURI) {
  mUnstrippedURI = aUnstrippedURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLoadReplace(bool* aLoadReplace) {
  *aLoadReplace = mLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLoadReplace(bool aLoadReplace) {
  mLoadReplace = aLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  *aReferrerInfo = mReferrerInfo;
  NS_IF_ADDREF(*aReferrerInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  mReferrerInfo = aReferrerInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSticky(bool aSticky) {
  mShared->mSticky = aSticky;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSticky(bool* aSticky) {
  *aSticky = mShared->mSticky;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetTitle(nsAString& aTitle) {
  // Check for empty title...
  if (mTitle.IsEmpty() && mURI) {
    // Default title is the URL.
    nsAutoCString spec;
    if (NS_SUCCEEDED(mURI->GetSpec(spec))) {
      AppendUTF8toUTF16(spec, mTitle);
    }
  }

  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetTitle(const nsAString& aTitle) {
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetName(const nsAString& aName) {
  mName = aName;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPostData(nsIInputStream** aResult) {
  *aResult = mPostData;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPostData(nsIInputStream* aPostData) {
  mPostData = aPostData;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetHasPostData(bool* aResult) {
  *aResult = !!mPostData;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLayoutHistoryState(nsILayoutHistoryState** aResult) {
  *aResult = mShared->mLayoutHistoryState;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLayoutHistoryState(nsILayoutHistoryState* aState) {
  mShared->mLayoutHistoryState = aState;
  if (mShared->mLayoutHistoryState) {
    mShared->mLayoutHistoryState->SetScrollPositionOnly(
        !mShared->mSaveLayoutState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::InitLayoutHistoryState(nsILayoutHistoryState** aState) {
  if (!mShared->mLayoutHistoryState) {
    nsCOMPtr<nsILayoutHistoryState> historyState;
    historyState = NS_NewLayoutHistoryState();
    SetLayoutHistoryState(historyState);
  }

  nsCOMPtr<nsILayoutHistoryState> state = GetLayoutHistoryState();
  state.forget(aState);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLoadType(uint32_t* aResult) {
  *aResult = mLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLoadType(uint32_t aLoadType) {
  mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetID(uint32_t* aResult) {
  *aResult = mID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetID(uint32_t aID) {
  mID = aID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetIsSubFrame(bool* aFlag) {
  *aFlag = mShared->mIsFrameNavigation;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetIsSubFrame(bool aFlag) {
  mShared->mIsFrameNavigation = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetHasUserInteraction(bool* aFlag) {
  // The back button and menulist deal with root/top-level
  // session history entries, thus we annotate only the root entry.
  if (!mParent) {
    *aFlag = mHasUserInteraction;
  } else {
    nsCOMPtr<nsISHEntry> root = nsSHistory::GetRootSHEntry(this);
    root->GetHasUserInteraction(aFlag);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetHasUserInteraction(bool aFlag) {
  // The back button and menulist deal with root/top-level
  // session history entries, thus we annotate only the root entry.
  if (!mParent) {
    mHasUserInteraction = aFlag;
  } else {
    nsCOMPtr<nsISHEntry> root = nsSHistory::GetRootSHEntry(this);
    root->SetHasUserInteraction(aFlag);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetHasUserActivation(bool* aFlag) {
  *aFlag = mHasUserActivation;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetHasUserActivation(bool aFlag) {
  mHasUserActivation = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetCacheKey(uint32_t* aResult) {
  *aResult = mShared->mCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetCacheKey(uint32_t aCacheKey) {
  mShared->mCacheKey = aCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetContentType(nsACString& aContentType) {
  aContentType = mShared->mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetContentType(const nsACString& aContentType) {
  mShared->mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Create(
    nsIURI* aURI, const nsAString& aTitle, nsIInputStream* aInputStream,
    uint32_t aCacheKey, const nsACString& aContentType,
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, const nsID& aDocShellID,
    bool aDynamicCreation, nsIURI* aOriginalURI, nsIURI* aResultPrincipalURI,
    nsIURI* aUnstrippedURI, bool aLoadReplace, nsIReferrerInfo* aReferrerInfo,
    const nsAString& aSrcdocData, bool aSrcdocEntry, nsIURI* aBaseURI,
    bool aSaveLayoutState, bool aExpired, bool aUserActivation) {
  MOZ_ASSERT(
      aTriggeringPrincipal,
      "need a valid triggeringPrincipal to create a session history entry");

  mURI = aURI;
  mTitle = aTitle;
  mPostData = aInputStream;

  // Set the LoadType by default to loadHistory during creation
  mLoadType = LOAD_HISTORY;

  mShared->mCacheKey = aCacheKey;
  mShared->mContentType = aContentType;
  mShared->mTriggeringPrincipal = aTriggeringPrincipal;
  mShared->mPrincipalToInherit = aPrincipalToInherit;
  mShared->mPartitionedPrincipalToInherit = aPartitionedPrincipalToInherit;
  mShared->mCsp = aCsp;
  mShared->mDocShellID = aDocShellID;
  mShared->mDynamicallyCreated = aDynamicCreation;

  // By default all entries are set false for subframe flag.
  // nsDocShell::CloneAndReplace() which creates entries for
  // all subframe navigations, sets the flag to true.
  mShared->mIsFrameNavigation = false;

  mHasUserInteraction = false;

  mShared->mExpired = aExpired;

  mIsSrcdocEntry = aSrcdocEntry;
  mSrcdocData = aSrcdocData;

  mBaseURI = aBaseURI;

  mLoadedInThisProcess = true;

  mOriginalURI = aOriginalURI;
  mResultPrincipalURI = aResultPrincipalURI;
  mUnstrippedURI = aUnstrippedURI;
  mLoadReplace = aLoadReplace;
  mReferrerInfo = aReferrerInfo;

  mHasUserActivation = aUserActivation;

  mShared->mLayoutHistoryState = nullptr;

  mShared->mSaveLayoutState = aSaveLayoutState;

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetParent(nsISHEntry** aResult) {
  nsCOMPtr<nsISHEntry> parent = do_QueryReferent(mParent);
  parent.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetParent(nsISHEntry* aParent) {
  mParent = do_GetWeakReference(aParent);
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHEntry::SetViewerBounds(const nsIntRect& aBounds) {
  mShared->mViewerBounds = aBounds;
}

NS_IMETHODIMP_(void)
nsSHEntry::GetViewerBounds(nsIntRect& aBounds) {
  aBounds = mShared->mViewerBounds;
}

NS_IMETHODIMP
nsSHEntry::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal) {
  NS_IF_ADDREF(*aTriggeringPrincipal = mShared->mTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal) {
  mShared->mTriggeringPrincipal = aTriggeringPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPrincipalToInherit(nsIPrincipal** aPrincipalToInherit) {
  NS_IF_ADDREF(*aPrincipalToInherit = mShared->mPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit) {
  mShared->mPrincipalToInherit = aPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPartitionedPrincipalToInherit(
    nsIPrincipal** aPartitionedPrincipalToInherit) {
  NS_IF_ADDREF(*aPartitionedPrincipalToInherit =
                   mShared->mPartitionedPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPartitionedPrincipalToInherit(
    nsIPrincipal* aPartitionedPrincipalToInherit) {
  mShared->mPartitionedPrincipalToInherit = aPartitionedPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetCsp(nsIContentSecurityPolicy** aCsp) {
  NS_IF_ADDREF(*aCsp = mShared->mCsp);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetCsp(nsIContentSecurityPolicy* aCsp) {
  mShared->mCsp = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AdoptBFCacheEntry(nsISHEntry* aEntry) {
  nsSHEntryShared* shared = static_cast<nsSHEntry*>(aEntry)->mShared;
  NS_ENSURE_STATE(shared);

  mShared = shared;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SharesDocumentWith(nsISHEntry* aEntry, bool* aOut) {
  NS_ENSURE_ARG_POINTER(aOut);

  *aOut = mShared == static_cast<nsSHEntry*>(aEntry)->mShared;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetIsSrcdocEntry(bool* aIsSrcdocEntry) {
  *aIsSrcdocEntry = mIsSrcdocEntry;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSrcdocData(nsAString& aSrcdocData) {
  aSrcdocData = mSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSrcdocData(const nsAString& aSrcdocData) {
  mSrcdocData = aSrcdocData;
  mIsSrcdocEntry = true;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetBaseURI(nsIURI** aBaseURI) {
  *aBaseURI = mBaseURI;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetBaseURI(nsIURI* aBaseURI) {
  mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetScrollRestorationIsManual(bool* aIsManual) {
  *aIsManual = mScrollRestorationIsManual;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetScrollRestorationIsManual(bool aIsManual) {
  mScrollRestorationIsManual = aIsManual;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLoadedInThisProcess(bool* aLoadedInThisProcess) {
  *aLoadedInThisProcess = mLoadedInThisProcess;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetChildCount(int32_t* aCount) {
  *aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChild(nsISHEntry* aChild, int32_t aOffset,
                    bool aUseRemoteSubframes) {
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

  bool newChildIsDyn = aChild ? aChild->IsDynamicallyAdded() : false;

  // If the new child is dynamically added, try to add it to aOffset, but if
  // there are non-dynamically added children, the child must be after those.
  if (newChildIsDyn) {
    int32_t lastNonDyn = aOffset - 1;
    for (int32_t i = aOffset; i < mChildren.Count(); ++i) {
      nsISHEntry* entry = mChildren[i];
      if (entry) {
        if (entry->IsDynamicallyAdded()) {
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
          if (entry->IsDynamicallyAdded()) {
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
        // Under Fission, this can happen when a network-created iframe starts
        // out in-process, moves out-of-process, and then switches back. At that
        // point, we'll create a new network-created DocShell at the same index
        // where we already have an entry for the original network-created
        // DocShell.
        //
        // This should ideally stop being an issue once the Fission-aware
        // session history rewrite is complete.
        NS_ASSERTION(
            aUseRemoteSubframes,
            "Adding a child where we already have a child? This may misbehave");
        oldChild->SetParent(nullptr);
      }
    }

    mChildren.ReplaceObjectAt(aChild, aOffset);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry* aChild) {
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);
  bool childRemoved = false;
  if (aChild->IsDynamicallyAdded()) {
    childRemoved = mChildren.RemoveObject(aChild);
  } else {
    int32_t index = mChildren.IndexOfObject(aChild);
    if (index >= 0) {
      // Other alive non-dynamic child docshells still keep mChildOffset,
      // so we don't want to change the indices here.
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
nsSHEntry::GetChildAt(int32_t aIndex, nsISHEntry** aResult) {
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

NS_IMETHODIMP_(void)
nsSHEntry::GetChildSHEntryIfHasNoDynamicallyAddedChild(int32_t aChildOffset,
                                                       nsISHEntry** aChild) {
  *aChild = nullptr;

  bool dynamicallyAddedChild = false;
  HasDynamicallyAddedChild(&dynamicallyAddedChild);
  if (dynamicallyAddedChild) {
    return;
  }

  // If the user did a shift-reload on this frameset page,
  // we don't want to load the subframes from history.
  if (IsForceReloadType(mLoadType) || mLoadType == LOAD_REFRESH) {
    return;
  }

  /* Before looking for the subframe's url, check
   * the expiration status of the parent. If the parent
   * has expired from cache, then subframes will not be
   * loaded from history in certain situations.
   * If the user pressed reload and the parent frame has expired
   *  from cache, we do not want to load the child frame from history.
   */
  if (mShared->mExpired && (mLoadType == LOAD_RELOAD_NORMAL)) {
    // The parent has expired. Return null.
    *aChild = nullptr;
    return;
  }
  // Get the child subframe from session history.
  GetChildAt(aChildOffset, aChild);
  if (*aChild) {
    // Set the parent's Load Type on the child
    (*aChild)->SetLoadType(mLoadType);
  }
}

NS_IMETHODIMP
nsSHEntry::ReplaceChild(nsISHEntry* aNewEntry) {
  NS_ENSURE_STATE(aNewEntry);

  nsID docshellID;
  aNewEntry->GetDocshellID(docshellID);

  for (int32_t i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i]) {
      nsID childDocshellID;
      nsresult rv = mChildren[i]->GetDocshellID(childDocshellID);
      NS_ENSURE_SUCCESS(rv, rv);
      if (docshellID == childDocshellID) {
        mChildren[i]->SetParent(nullptr);
        mChildren.ReplaceObjectAt(aNewEntry, i);
        return aNewEntry->SetParent(this);
      }
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void) nsSHEntry::ClearEntry() {
  int32_t childCount = GetChildCount();
  // Remove all children of this entry
  for (int32_t i = childCount - 1; i >= 0; i--) {
    nsCOMPtr<nsISHEntry> child;
    GetChildAt(i, getter_AddRefs(child));
    RemoveChild(child);
  }
  AbandonBFCacheEntry();
}

NS_IMETHODIMP
nsSHEntry::GetStateData(nsIStructuredCloneContainer** aContainer) {
  NS_IF_ADDREF(*aContainer = mStateData);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetStateData(nsIStructuredCloneContainer* aContainer) {
  mStateData = aContainer;
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsSHEntry::IsDynamicallyAdded() { return mShared->mDynamicallyCreated; }

NS_IMETHODIMP
nsSHEntry::HasDynamicallyAddedChild(bool* aAdded) {
  *aAdded = false;
  for (int32_t i = 0; i < mChildren.Count(); ++i) {
    nsISHEntry* entry = mChildren[i];
    if (entry) {
      *aAdded = entry->IsDynamicallyAdded();
      if (*aAdded) {
        break;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetDocshellID(nsID& aID) {
  aID = mShared->mDocShellID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetDocshellID(const nsID& aID) {
  mShared->mDocShellID = aID;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetLastTouched(uint32_t* aLastTouched) {
  *aLastTouched = mShared->mLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLastTouched(uint32_t aLastTouched) {
  mShared->mLastTouched = aLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetShistory(nsISHistory** aSHistory) {
  nsCOMPtr<nsISHistory> shistory(do_QueryReferent(mShared->mSHistory));
  shistory.forget(aSHistory);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetShistory(nsISHistory* aSHistory) {
  nsWeakPtr shistory = do_GetWeakReference(aSHistory);
  // mSHistory can not be changed once it's set
  MOZ_ASSERT(!mShared->mSHistory || (mShared->mSHistory == shistory));
  mShared->mSHistory = shistory;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetLoadTypeAsHistory() {
  // Set the LoadType by default to loadHistory during creation
  mLoadType = LOAD_HISTORY;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetPersist(bool* aPersist) {
  *aPersist = mPersist;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetPersist(bool aPersist) {
  mPersist = aPersist;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::CreateLoadInfo(nsDocShellLoadState** aLoadState) {
  nsCOMPtr<nsIURI> uri = GetURI();
  RefPtr<nsDocShellLoadState> loadState(new nsDocShellLoadState(uri));

  nsCOMPtr<nsIURI> originalURI = GetOriginalURI();
  loadState->SetOriginalURI(originalURI);

  mozilla::Maybe<nsCOMPtr<nsIURI>> emplacedResultPrincipalURI;
  nsCOMPtr<nsIURI> resultPrincipalURI = GetResultPrincipalURI();
  emplacedResultPrincipalURI.emplace(std::move(resultPrincipalURI));
  loadState->SetMaybeResultPrincipalURI(emplacedResultPrincipalURI);

  nsCOMPtr<nsIURI> unstrippedURI = GetUnstrippedURI();
  loadState->SetUnstrippedURI(unstrippedURI);

  loadState->SetLoadReplace(GetLoadReplace());
  nsCOMPtr<nsIInputStream> postData = GetPostData();
  loadState->SetPostDataStream(postData);

  nsAutoCString contentType;
  GetContentType(contentType);
  loadState->SetTypeHint(contentType);

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = GetTriggeringPrincipal();
  loadState->SetTriggeringPrincipal(triggeringPrincipal);
  nsCOMPtr<nsIPrincipal> principalToInherit = GetPrincipalToInherit();
  loadState->SetPrincipalToInherit(principalToInherit);
  nsCOMPtr<nsIPrincipal> partitionedPrincipalToInherit =
      GetPartitionedPrincipalToInherit();
  loadState->SetPartitionedPrincipalToInherit(partitionedPrincipalToInherit);
  nsCOMPtr<nsIContentSecurityPolicy> csp = GetCsp();
  loadState->SetCsp(csp);
  nsCOMPtr<nsIReferrerInfo> referrerInfo = GetReferrerInfo();
  loadState->SetReferrerInfo(referrerInfo);

  // Do not inherit principal from document (security-critical!);
  uint32_t flags = nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_NONE;

  // Passing nullptr as aSourceDocShell gives the same behaviour as before
  // aSourceDocShell was introduced. According to spec we should be passing
  // the source browsing context that was used when the history entry was
  // first created. bug 947716 has been created to address this issue.
  nsAutoString srcdoc;
  nsCOMPtr<nsIURI> baseURI;
  if (GetIsSrcdocEntry()) {
    GetSrcdocData(srcdoc);
    baseURI = GetBaseURI();
    flags |= nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  } else {
    srcdoc = VoidString();
  }
  loadState->SetSrcdocData(srcdoc);
  loadState->SetBaseURI(baseURI);
  loadState->SetInternalLoadFlags(flags);

  loadState->SetFirstParty(true);

  loadState->SetHasValidUserGestureActivation(GetHasUserActivation());

  loadState->SetSHEntry(this);

  // When we create a load state from the history entry we already know if
  // https-first was able to upgrade the request from http to https. There is no
  // point in re-retrying to upgrade.
  loadState->SetIsExemptFromHTTPSFirstMode(true);

  loadState.forget(aLoadState);
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHEntry::SyncTreesForSubframeNavigation(
    nsISHEntry* aEntry, mozilla::dom::BrowsingContext* aTopBC,
    mozilla::dom::BrowsingContext* aIgnoreBC) {
  // XXX Keep this in sync with
  // SessionHistoryEntry::SyncTreesForSubframeNavigation
  //
  // We need to sync up the browsing context and session history trees for
  // subframe navigation.  If the load was in a subframe, we forward up to
  // the top browsing context, which will then recursively sync up all browsing
  // contexts to their corresponding entries in the new session history tree. If
  // we don't do this, then we can cache a content viewer on the wrong cloned
  // entry, and subsequently restore it at the wrong time.
  nsCOMPtr<nsISHEntry> newRootEntry = nsSHistory::GetRootSHEntry(aEntry);
  if (newRootEntry) {
    // newRootEntry is now the new root entry.
    // Find the old root entry as well.

    // Need a strong ref. on |oldRootEntry| so it isn't destroyed when
    // SetChildHistoryEntry() does SwapHistoryEntries() (bug 304639).
    nsCOMPtr<nsISHEntry> oldRootEntry = nsSHistory::GetRootSHEntry(this);

    if (oldRootEntry) {
      nsSHistory::SwapEntriesData data = {aIgnoreBC, newRootEntry, nullptr};
      nsSHistory::SetChildHistoryEntry(oldRootEntry, aTopBC, 0, &data);
    }
  }
}

void nsSHEntry::EvictContentViewer() {
  nsCOMPtr<nsIContentViewer> viewer = GetContentViewer();
  if (viewer) {
    mShared->NotifyListenersContentViewerEvicted();
    // Drop the presentation state before destroying the viewer, so that
    // document teardown is able to correctly persist the state.
    SetContentViewer(nullptr);
    SyncPresentationState();
    viewer->Destroy();
  }
}

NS_IMETHODIMP
nsSHEntry::SetContentViewer(nsIContentViewer* aViewer) {
  return GetState()->SetContentViewer(aViewer);
}

NS_IMETHODIMP
nsSHEntry::GetContentViewer(nsIContentViewer** aResult) {
  *aResult = GetState()->mContentViewer;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetIsInBFCache(bool* aResult) {
  *aResult = !!GetState()->mContentViewer;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Clone(nsISHEntry** aResult) {
  nsCOMPtr<nsISHEntry> entry = new nsSHEntry(*this);
  entry.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetSaveLayoutStateFlag(bool* aFlag) {
  *aFlag = mShared->mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSaveLayoutStateFlag(bool aFlag) {
  mShared->mSaveLayoutState = aFlag;
  if (mShared->mLayoutHistoryState) {
    mShared->mLayoutHistoryState->SetScrollPositionOnly(!aFlag);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetWindowState(nsISupports* aState) {
  GetState()->mWindowState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetWindowState(nsISupports** aState) {
  NS_IF_ADDREF(*aState = GetState()->mWindowState);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetRefreshURIList(nsIMutableArray** aList) {
  NS_IF_ADDREF(*aList = GetState()->mRefreshURIList);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetRefreshURIList(nsIMutableArray* aList) {
  GetState()->mRefreshURIList = aList;
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHEntry::AddChildShell(nsIDocShellTreeItem* aShell) {
  MOZ_ASSERT(aShell, "Null child shell added to history entry");
  GetState()->mChildShells.AppendObject(aShell);
}

NS_IMETHODIMP
nsSHEntry::ChildShellAt(int32_t aIndex, nsIDocShellTreeItem** aShell) {
  NS_IF_ADDREF(*aShell = GetState()->mChildShells.SafeObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHEntry::ClearChildShells() { GetState()->mChildShells.Clear(); }

NS_IMETHODIMP_(void)
nsSHEntry::SyncPresentationState() { GetState()->SyncPresentationState(); }

nsDocShellEditorData* nsSHEntry::ForgetEditorData() {
  // XXX jlebar Check how this is used.
  return GetState()->mEditorData.release();
}

void nsSHEntry::SetEditorData(nsDocShellEditorData* aData) {
  NS_ASSERTION(!(aData && GetState()->mEditorData),
               "We're going to overwrite an owning ref!");
  if (GetState()->mEditorData != aData) {
    GetState()->mEditorData = mozilla::WrapUnique(aData);
  }
}

bool nsSHEntry::HasDetachedEditor() {
  return GetState()->mEditorData != nullptr;
}

bool nsSHEntry::HasBFCacheEntry(
    mozilla::dom::SHEntrySharedParentState* aEntry) {
  return GetState() == aEntry;
}

NS_IMETHODIMP
nsSHEntry::AbandonBFCacheEntry() {
  mShared = GetState()->Duplicate();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetBfcacheID(uint64_t* aBFCacheID) {
  *aBFCacheID = mShared->GetId();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetWireframe(JSContext* aCx, JS::MutableHandle<JS::Value> aOut) {
  aOut.set(JS::NullValue());
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetWireframe(JSContext* aCx, JS::Handle<JS::Value> aArg) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
