/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntry_h
#define nsSHEntry_h

// Helper Classes
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

// Interfaces needed
#include "nsISHEntry.h"
#include "nsISHContainer.h"

class nsSHEntryShared;
class nsIInputStream;
class nsIURI;

class nsSHEntry final : public nsISHEntry,
                        public nsISHContainer,
                        public nsISHEntryInternal
{
public:
  nsSHEntry();
  nsSHEntry(const nsSHEntry& aOther);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHENTRY
  NS_DECL_NSISHENTRYINTERNAL
  NS_DECL_NSISHCONTAINER

  void DropPresentationState();

  static nsresult Startup();
  static void Shutdown();

private:
  ~nsSHEntry();

  // We share the state in here with other SHEntries which correspond to the
  // same document.
  RefPtr<nsSHEntryShared> mShared;

  // See nsSHEntry.idl for comments on these members.
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  bool mLoadReplace;
  nsCOMPtr<nsIURI> mReferrerURI;
  uint32_t mReferrerPolicy;
  nsString mTitle;
  nsCOMPtr<nsIInputStream> mPostData;
  uint32_t mLoadType;
  uint32_t mID;
  int32_t mScrollPositionX;
  int32_t mScrollPositionY;
  nsISHEntry* mParent;
  nsCOMArray<nsISHEntry> mChildren;
  bool mURIWasModified;
  nsCOMPtr<nsIStructuredCloneContainer> mStateData;
  bool mIsSrcdocEntry;
  bool mScrollRestorationIsManual;
  nsString mSrcdocData;
  nsCOMPtr<nsIURI> mBaseURI;
};

#endif /* nsSHEntry_h */
