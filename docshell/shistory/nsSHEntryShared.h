/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntryShared_h__
#define nsSHEntryShared_h__

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsExpirationTracker.h"
#include "nsIBFCacheEntry.h"
#include "nsIWeakReferenceUtils.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsStubMutationObserver.h"

#include "mozilla/Attributes.h"

class nsSHEntry;
class nsISHEntry;
class nsIContentViewer;
class nsIDocShellTreeItem;
class nsILayoutHistoryState;
class nsDocShellEditorData;
class nsIMutableArray;

// A document may have multiple SHEntries, either due to hash navigations or
// calls to history.pushState.  SHEntries corresponding to the same document
// share many members; in particular, they share state related to the
// back/forward cache.
//
// The classes defined here are the vehicle for this sharing.
//
// Some of the state can only be stored in the process where we did the actual
// load, because that's where the objects live (eg. the content viewer).

namespace mozilla {
namespace dom {
class Document;

/**
 * SHEntrySharedParentState holds the shared state that can live in the parent
 * process.
 */
class SHEntrySharedParentState {
 public:
  explicit SHEntrySharedParentState(uint64_t aID);

  uint64_t GetID() const { return mID; }

  void NotifyListenersContentViewerEvicted();

 protected:
  friend class nsSHEntry;

  virtual ~SHEntrySharedParentState();
  NS_INLINE_DECL_VIRTUAL_REFCOUNTING_WITH_DESTROY(SHEntrySharedParentState,
                                                  Destroy())

  virtual void Destroy() { delete this; }

  void CopyFrom(SHEntrySharedParentState* aSource);

  // These members are copied by SHEntrySharedParentState::CopyFrom(). If you
  // add a member here, be sure to update the CopyFrom() implementation.
  nsID mDocShellID;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;
  nsCOMPtr<nsIPrincipal> mStoragePrincipalToInherit;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
  nsCString mContentType;

  nsIntRect mViewerBounds;

  uint32_t mCacheKey;
  uint32_t mLastTouched;

  // These members aren't copied by SHEntrySharedParentState::CopyFrom() because
  // they're specific to a particular content viewer.
  uint64_t mID;
  nsWeakPtr mSHistory;

  bool mIsFrameNavigation;
  bool mSticky;
  bool mDynamicallyCreated;

  // This flag is about necko cache, not bfcache.
  bool mExpired;
};

/**
 * SHEntrySharedChildState holds the shared state that needs to live in the
 * process where the document was loaded.
 */
class SHEntrySharedChildState {
 protected:
  SHEntrySharedChildState();

  void CopyFrom(SHEntrySharedChildState* aSource);

 public:
  // These members are copied by SHEntrySharedChildState::CopyFrom(). If you
  // add a member here, be sure to update the CopyFrom() implementation.
  nsCOMArray<nsIDocShellTreeItem> mChildShells;

  // These members aren't copied by SHEntrySharedChildState::CopyFrom() because
  // they're specific to a particular content viewer.
  nsCOMPtr<nsIContentViewer> mContentViewer;
  RefPtr<mozilla::dom::Document> mDocument;
  // FIXME Move to parent?
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
  nsCOMPtr<nsISupports> mWindowState;
  // FIXME Move to parent?
  nsCOMPtr<nsIMutableArray> mRefreshURIList;
  nsExpirationState mExpirationState;
  nsAutoPtr<nsDocShellEditorData> mEditorData;

  // FIXME Move to parent?
  bool mSaveLayoutState;
};

}  // namespace dom
}  // namespace mozilla

/**
 * nsSHEntryShared holds the shared state if the session history is not stored
 * in the parent process, or if the load itself happens in the parent process.
 */
class nsSHEntryShared final : public nsIBFCacheEntry,
                              public nsStubMutationObserver,
                              public mozilla::dom::SHEntrySharedParentState,
                              public mozilla::dom::SHEntrySharedChildState {
 public:
  static void EnsureHistoryTracker();
  static void Shutdown();

  explicit nsSHEntryShared(uint64_t aID);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIBFCACHEENTRY

  // The nsIMutationObserver bits we actually care about.
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  nsExpirationState* GetExpirationState() { return &mExpirationState; }

  static already_AddRefed<nsSHEntryShared> Duplicate(nsSHEntryShared* aEntry,
                                                     uint64_t aNewSharedID);

 private:
  ~nsSHEntryShared();

  friend class nsLegacySHEntry;

  void RemoveFromExpirationTracker();
  void SyncPresentationState();
  void DropPresentationState();

  nsresult SetContentViewer(nsIContentViewer* aViewer);
};

#endif
