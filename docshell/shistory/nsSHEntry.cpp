/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHEntry.h"

#include <algorithm>

#include "nsIContentSecurityPolicy.h"
#include "nsDocShellEditorData.h"
#include "nsDocShellLoadTypes.h"
#include "nsIContentViewer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInputStream.h"
#include "nsILayoutHistoryState.h"
#include "nsIStructuredCloneContainer.h"
#include "nsIURI.h"
#include "nsSHEntryShared.h"
#include "nsSHistory.h"

#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Logging.h"
#include "nsIReferrerInfo.h"

extern mozilla::LazyLogModule gPageCacheLog;

namespace dom = mozilla::dom;

static uint32_t gEntryID = 0;

nsSHEntry::nsSHEntry()
    : mShared(new nsSHEntryShared()),
      mLoadType(0),
      mID(gEntryID++),
      mScrollPositionX(0),
      mScrollPositionY(0),
      mParent(nullptr),
      mLoadReplace(false),
      mURIWasModified(false),
      mIsSrcdocEntry(false),
      mScrollRestorationIsManual(false),
      mLoadedInThisProcess(false),
      mPersist(true) {}

nsSHEntry::nsSHEntry(const nsSHEntry& aOther)
    : mShared(aOther.mShared),
      mURI(aOther.mURI),
      mOriginalURI(aOther.mOriginalURI),
      mResultPrincipalURI(aOther.mResultPrincipalURI),
      mReferrerInfo(aOther.mReferrerInfo),
      mTitle(aOther.mTitle),
      mPostData(aOther.mPostData),
      mLoadType(0)  // XXX why not copy?
      ,
      mID(aOther.mID),
      mScrollPositionX(0)  // XXX why not copy?
      ,
      mScrollPositionY(0)  // XXX why not copy?
      ,
      mParent(aOther.mParent),
      mStateData(aOther.mStateData),
      mSrcdocData(aOther.mSrcdocData),
      mBaseURI(aOther.mBaseURI),
      mLoadReplace(aOther.mLoadReplace),
      mURIWasModified(aOther.mURIWasModified),
      mIsSrcdocEntry(aOther.mIsSrcdocEntry),
      mScrollRestorationIsManual(false),
      mLoadedInThisProcess(aOther.mLoadedInThisProcess),
      mPersist(aOther.mPersist) {}

nsSHEntry::~nsSHEntry() {
  // Null out the mParent pointers on all our kids.
  for (nsISHEntry* entry : mChildren) {
    if (entry) {
      entry->SetParent(nullptr);
    }
  }
}

NS_IMPL_ISUPPORTS(nsSHEntry, nsISHEntry)

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
nsSHEntry::SetContentViewer(nsIContentViewer* aViewer) {
  return mShared->SetContentViewer(aViewer);
}

NS_IMETHODIMP
nsSHEntry::GetContentViewer(nsIContentViewer** aResult) {
  *aResult = mShared->mContentViewer;
  NS_IF_ADDREF(*aResult);
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

nsSHEntryShared* nsSHEntry::GetSharedState() { return mShared; }

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
nsSHEntry::GetExpirationStatus(bool* aFlag) {
  *aFlag = mShared->mExpired;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetExpirationStatus(bool aFlag) {
  mShared->mExpired = aFlag;
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
nsSHEntry::Create(nsIURI* aURI, const nsAString& aTitle,
                  nsIInputStream* aInputStream,
                  nsILayoutHistoryState* aLayoutHistoryState,
                  uint32_t aCacheKey, const nsACString& aContentType,
                  nsIPrincipal* aTriggeringPrincipal,
                  nsIPrincipal* aPrincipalToInherit,
                  nsIContentSecurityPolicy* aCsp, const nsID& aDocShellID,
                  bool aDynamicCreation) {
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
  mShared->mCsp = aCsp;
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
  mSrcdocData = VoidString();

  mLoadedInThisProcess = true;

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Clone(nsISHEntry** aResult) {
  *aResult = new nsSHEntry(*this);
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetParent(nsISHEntry** aResult) {
  *aResult = mParent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetParent(nsISHEntry* aParent) {
  /* parent not Addrefed on purpose to avoid cyclic reference
   * Null parent is OK
   *
   * XXX this method should not be scriptable if this is the case!!
   */
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetWindowState(nsISupports* aState) {
  mShared->mWindowState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetWindowState(nsISupports** aState) {
  NS_IF_ADDREF(*aState = mShared->mWindowState);
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
nsSHEntry::GetBFCacheEntry(nsIBFCacheEntry** aEntry) {
  NS_IF_ADDREF(*aEntry = mShared);
  return NS_OK;
}

bool nsSHEntry::HasBFCacheEntry(nsIBFCacheEntry* aEntry) {
  return static_cast<nsIBFCacheEntry*>(mShared) == aEntry;
}

NS_IMETHODIMP
nsSHEntry::AdoptBFCacheEntry(nsISHEntry* aEntry) {
  nsSHEntryShared* shared = aEntry->GetSharedState();
  NS_ENSURE_STATE(shared);

  mShared = shared;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SharesDocumentWith(nsISHEntry* aEntry, bool* aOut) {
  NS_ENSURE_ARG_POINTER(aOut);

  *aOut = mShared == aEntry->GetSharedState();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AbandonBFCacheEntry() {
  mShared = nsSHEntryShared::Duplicate(mShared);
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
nsSHEntry::AddChild(nsISHEntry* aChild, int32_t aOffset) {
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
        NS_ERROR(
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

NS_IMETHODIMP
nsSHEntry::ReplaceChild(nsISHEntry* aNewEntry) {
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

NS_IMETHODIMP_(void)
nsSHEntry::AddChildShell(nsIDocShellTreeItem* aShell) {
  MOZ_ASSERT(aShell, "Null child shell added to history entry");
  mShared->mChildShells.AppendObject(aShell);
}

NS_IMETHODIMP
nsSHEntry::ChildShellAt(int32_t aIndex, nsIDocShellTreeItem** aShell) {
  NS_IF_ADDREF(*aShell = mShared->mChildShells.SafeObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHEntry::ClearChildShells() { mShared->mChildShells.Clear(); }

NS_IMETHODIMP
nsSHEntry::GetRefreshURIList(nsIMutableArray** aList) {
  NS_IF_ADDREF(*aList = mShared->mRefreshURIList);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetRefreshURIList(nsIMutableArray* aList) {
  mShared->mRefreshURIList = aList;
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHEntry::SyncPresentationState() { mShared->SyncPresentationState(); }

nsDocShellEditorData* nsSHEntry::ForgetEditorData() {
  // XXX jlebar Check how this is used.
  return mShared->mEditorData.forget();
}

void nsSHEntry::SetEditorData(nsDocShellEditorData* aData) {
  NS_ASSERTION(!(aData && mShared->mEditorData),
               "We're going to overwrite an owning ref!");
  if (mShared->mEditorData != aData) {
    mShared->mEditorData = aData;
  }
}

bool nsSHEntry::HasDetachedEditor() { return mShared->mEditorData != nullptr; }

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
nsSHEntry::GetDocshellID(nsID** aID) {
  *aID = mShared->mDocShellID.Clone();
  return NS_OK;
}

const nsID nsSHEntry::DocshellID() { return mShared->mDocShellID; }

NS_IMETHODIMP
nsSHEntry::SetDocshellID(const nsID* aID) {
  mShared->mDocShellID = *aID;
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
nsSHEntry::GetSHistory(nsISHistory** aSHistory) {
  nsCOMPtr<nsISHistory> shistory(do_QueryReferent(mShared->mSHistory));
  shistory.forget(aSHistory);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::SetSHistory(nsISHistory* aSHistory) {
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
