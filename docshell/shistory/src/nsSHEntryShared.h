/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntryShared_h__
#define nsSHEntryShared_h__

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIBFCacheEntry.h"
#include "nsIMutationObserver.h"
#include "nsExpirationTracker.h"
#include "nsRect.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

class nsSHEntry;
class nsISHEntry;
class nsIDocument;
class nsIContentViewer;
class nsIDocShellTreeItem;
class nsILayoutHistoryState;
class nsDocShellEditorData;
class nsISupportsArray;

// A document may have multiple SHEntries, either due to hash navigations or
// calls to history.pushState.  SHEntries corresponding to the same document
// share many members; in particular, they share state related to the
// back/forward cache.
//
// nsSHEntryShared is the vehicle for this sharing.
class nsSHEntryShared final
  : public nsIBFCacheEntry
  , public nsIMutationObserver
{
public:
  static void EnsureHistoryTracker();
  static void Shutdown();

  nsSHEntryShared();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER
  NS_DECL_NSIBFCACHEENTRY

private:
  ~nsSHEntryShared();

  friend class nsSHEntry;

  friend class HistoryTracker;
  friend class nsExpirationTracker<nsSHEntryShared, 3>;
  nsExpirationState *GetExpirationState() { return &mExpirationState; }

  static already_AddRefed<nsSHEntryShared> Duplicate(nsSHEntryShared* aEntry);

  void RemoveFromExpirationTracker();
  void Expire();
  nsresult SyncPresentationState();
  void DropPresentationState();

  nsresult SetContentViewer(nsIContentViewer* aViewer);

  // See nsISHEntry.idl for an explanation of these members.

  // These members are copied by nsSHEntryShared::Duplicate().  If you add a
  // member here, be sure to update the Duplicate() implementation.
  uint64_t mDocShellID;
  nsCOMArray<nsIDocShellTreeItem> mChildShells;
  nsCOMPtr<nsISupports> mOwner;
  nsCString mContentType;
  bool mIsFrameNavigation;
  bool mSaveLayoutState;
  bool mSticky;
  bool mDynamicallyCreated;
  nsCOMPtr<nsISupports> mCacheKey;
  uint32_t mLastTouched;

  // These members aren't copied by nsSHEntryShared::Duplicate() because
  // they're specific to a particular content viewer.
  uint64_t mID;
  nsCOMPtr<nsIContentViewer> mContentViewer;
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
  bool mExpired;
  nsCOMPtr<nsISupports> mWindowState;
  nsIntRect mViewerBounds;
  nsCOMPtr<nsISupportsArray> mRefreshURIList;
  nsExpirationState mExpirationState;
  nsAutoPtr<nsDocShellEditorData> mEditorData;
};

#endif
