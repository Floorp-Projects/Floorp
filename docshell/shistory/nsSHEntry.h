/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntry_h
#define nsSHEntry_h

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsISHEntry.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

class nsSHEntryShared;
class nsIInputStream;
class nsIURI;
class nsIReferrerInfo;

class nsSHEntry : public nsISHEntry, public nsSupportsWeakReference {
 public:
  nsSHEntry();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHENTRY

  virtual void EvictDocumentViewer();

  static nsresult Startup();
  static void Shutdown();

  nsSHEntryShared* GetState() { return mShared; }

 protected:
  explicit nsSHEntry(const nsSHEntry& aOther);
  virtual ~nsSHEntry();

  // We share the state in here with other SHEntries which correspond to the
  // same document.
  RefPtr<nsSHEntryShared> mShared;

  // See nsSHEntry.idl for comments on these members.
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  nsCOMPtr<nsIURI> mUnstrippedURI;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  nsString mTitle;
  nsString mName;
  nsCOMPtr<nsIInputStream> mPostData;
  uint32_t mLoadType;
  uint32_t mID;
  int32_t mScrollPositionX;
  int32_t mScrollPositionY;
  nsWeakPtr mParent;
  nsCOMArray<nsISHEntry> mChildren;
  nsCOMPtr<nsIStructuredCloneContainer> mStateData;
  nsString mSrcdocData;
  nsCOMPtr<nsIURI> mBaseURI;
  bool mLoadReplace;
  bool mURIWasModified;
  bool mIsSrcdocEntry;
  bool mScrollRestorationIsManual;
  bool mLoadedInThisProcess;
  bool mPersist;
  bool mHasUserInteraction;
  bool mHasUserActivation;
};

#endif /* nsSHEntry_h */
