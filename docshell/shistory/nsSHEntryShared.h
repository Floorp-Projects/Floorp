/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntryShared_h__
#define nsSHEntryShared_h__

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsExpirationTracker.h"
#include "nsIBFCacheEntry.h"
#include "nsIWeakReferenceUtils.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsStubMutationObserver.h"

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

class nsSHEntry;
class nsISHEntry;
class nsISHistory;
class nsIContentSecurityPolicy;
class nsIDocShellTreeItem;
class nsIDocumentViewer;
class nsILayoutHistoryState;
class nsIPrincipal;
class nsDocShellEditorData;
class nsFrameLoader;
class nsIMutableArray;
class nsSHistory;

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
 * SHEntrySharedState holds shared state both in the child process and in the
 * parent process.
 */
struct SHEntrySharedState {
  SHEntrySharedState() : mId(GenerateId()) {}
  SHEntrySharedState(const SHEntrySharedState& aState) = default;
  SHEntrySharedState(nsIPrincipal* aTriggeringPrincipal,
                     nsIPrincipal* aPrincipalToInherit,
                     nsIPrincipal* aPartitionedPrincipalToInherit,
                     nsIContentSecurityPolicy* aCsp,
                     const nsACString& aContentType)
      : mId(GenerateId()),
        mTriggeringPrincipal(aTriggeringPrincipal),
        mPrincipalToInherit(aPrincipalToInherit),
        mPartitionedPrincipalToInherit(aPartitionedPrincipalToInherit),
        mCsp(aCsp),
        mContentType(aContentType) {}

  // These members aren't copied by SHEntrySharedParentState::CopyFrom() because
  // they're specific to a particular content viewer.
  uint64_t mId = 0;

  // These members are copied by SHEntrySharedParentState::CopyFrom(). If you
  // add a member here, be sure to update the CopyFrom() implementation.
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;
  nsCOMPtr<nsIPrincipal> mPartitionedPrincipalToInherit;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
  nsCString mContentType;
  // Child side updates layout history state when page is being unloaded or
  // moved to bfcache.
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
  uint32_t mCacheKey = 0;
  bool mIsFrameNavigation = false;
  bool mSaveLayoutState = true;

 protected:
  static uint64_t GenerateId();
};

/**
 * SHEntrySharedParentState holds the shared state that can live in the parent
 * process.
 */
class SHEntrySharedParentState : public SHEntrySharedState {
 public:
  friend class SessionHistoryInfo;

  uint64_t GetId() const { return mId; }
  void ChangeId(uint64_t aId);

  void SetFrameLoader(nsFrameLoader* aFrameLoader);

  nsFrameLoader* GetFrameLoader();

  void NotifyListenersDocumentViewerEvicted();

  nsExpirationState* GetExpirationState() { return &mExpirationState; }

  SHEntrySharedParentState();
  SHEntrySharedParentState(nsIPrincipal* aTriggeringPrincipal,
                           nsIPrincipal* aPrincipalToInherit,
                           nsIPrincipal* aPartitionedPrincipalToInherit,
                           nsIContentSecurityPolicy* aCsp,
                           const nsACString& aContentType);

  // This returns the existing SHEntrySharedParentState that was registered for
  // aId, if one exists.
  static SHEntrySharedParentState* Lookup(uint64_t aId);

 protected:
  virtual ~SHEntrySharedParentState();
  NS_INLINE_DECL_VIRTUAL_REFCOUNTING_WITH_DESTROY(SHEntrySharedParentState,
                                                  Destroy())

  virtual void Destroy() { delete this; }

  void CopyFrom(SHEntrySharedParentState* aSource);

  // These members are copied by SHEntrySharedParentState::CopyFrom(). If you
  // add a member here, be sure to update the CopyFrom() implementation.
  nsID mDocShellID{};

  nsIntRect mViewerBounds{0, 0, 0, 0};

  uint32_t mLastTouched = 0;

  // These members aren't copied by SHEntrySharedParentState::CopyFrom() because
  // they're specific to a particular content viewer.
  nsWeakPtr mSHistory;

  RefPtr<nsFrameLoader> mFrameLoader;

  nsExpirationState mExpirationState;

  bool mSticky = true;
  bool mDynamicallyCreated = false;

  // This flag is about necko cache, not bfcache.
  bool mExpired = false;
};

/**
 * SHEntrySharedChildState holds the shared state that needs to live in the
 * process where the document was loaded.
 */
class SHEntrySharedChildState {
 protected:
  void CopyFrom(SHEntrySharedChildState* aSource);

 public:
  // These members are copied by SHEntrySharedChildState::CopyFrom(). If you
  // add a member here, be sure to update the CopyFrom() implementation.
  nsCOMArray<nsIDocShellTreeItem> mChildShells;

  // These members aren't copied by SHEntrySharedChildState::CopyFrom() because
  // they're specific to a particular content viewer.
  nsCOMPtr<nsIDocumentViewer> mDocumentViewer;
  RefPtr<mozilla::dom::Document> mDocument;
  nsCOMPtr<nsISupports> mWindowState;
  // FIXME Move to parent?
  nsCOMPtr<nsIMutableArray> mRefreshURIList;
  UniquePtr<nsDocShellEditorData> mEditorData;
};

}  // namespace dom
}  // namespace mozilla

/**
 * nsSHEntryShared holds the shared state if the session history is not stored
 * in the parent process, or if the load itself happens in the parent process.
 * Note, since nsSHEntryShared inherits both SHEntrySharedParentState and
 * SHEntrySharedChildState and those have some same member variables,
 * the ones from SHEntrySharedParentState should be used.
 */
class nsSHEntryShared final : public nsIBFCacheEntry,
                              public nsStubMutationObserver,
                              public mozilla::dom::SHEntrySharedParentState,
                              public mozilla::dom::SHEntrySharedChildState {
 public:
  static void EnsureHistoryTracker();
  static void Shutdown();

  using SHEntrySharedParentState::SHEntrySharedParentState;

  already_AddRefed<nsSHEntryShared> Duplicate();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIBFCACHEENTRY

  // The nsIMutationObserver bits we actually care about.
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

 private:
  ~nsSHEntryShared();

  friend class nsSHEntry;

  void RemoveFromExpirationTracker();
  void SyncPresentationState();
  void DropPresentationState();

  nsresult SetDocumentViewer(nsIDocumentViewer* aViewer);
};

#endif
