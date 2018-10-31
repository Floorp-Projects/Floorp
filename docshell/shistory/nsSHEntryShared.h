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
class nsIDocument;
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
// nsSHEntryShared is the vehicle for this sharing.
class nsSHEntryShared final
  : public nsIBFCacheEntry
  , public nsStubMutationObserver
{
public:
  static void EnsureHistoryTracker();
  static void Shutdown();

  nsSHEntryShared();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIBFCACHEENTRY

  // The nsIMutationObserver bits we actually care about.
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  nsExpirationState *GetExpirationState() { return &mExpirationState; }

private:
  ~nsSHEntryShared();

  friend class nsSHEntry;

  static already_AddRefed<nsSHEntryShared> Duplicate(nsSHEntryShared* aEntry);

  void RemoveFromExpirationTracker();
  void SyncPresentationState();
  void DropPresentationState();

  nsresult SetContentViewer(nsIContentViewer* aViewer);

  // See nsISHEntry.idl for an explanation of these members.

  // These members are copied by nsSHEntryShared::Duplicate().  If you add a
  // member here, be sure to update the Duplicate() implementation.
  nsID mDocShellID;
  nsCOMArray<nsIDocShellTreeItem> mChildShells;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;
  nsCString mContentType;

  uint32_t mCacheKey;
  uint32_t mLastTouched;

  // These members aren't copied by nsSHEntryShared::Duplicate() because
  // they're specific to a particular content viewer.
  uint64_t mID;
  nsCOMPtr<nsIContentViewer> mContentViewer;
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
  nsCOMPtr<nsISupports> mWindowState;
  nsIntRect mViewerBounds;
  nsCOMPtr<nsIMutableArray> mRefreshURIList;
  nsExpirationState mExpirationState;
  nsAutoPtr<nsDocShellEditorData> mEditorData;
  nsWeakPtr mSHistory;

  bool mIsFrameNavigation;
  bool mSaveLayoutState;
  bool mSticky;
  bool mDynamicallyCreated;

  // This flag is about necko cache, not bfcache.
  bool mExpired;
};

#endif
