/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHEntry_h
#define nsSHEntry_h

// Helper Classes
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

// Interfaces needed
#include "nsIInputStream.h"
#include "nsISHEntry.h"
#include "nsISHContainer.h"
#include "nsIURI.h"
#include "nsIHistoryEntry.h"

class nsSHEntryShared;

class nsSHEntry MOZ_FINAL : public nsISHEntry,
                            public nsISHContainer,
                            public nsISHEntryInternal
{
public: 
  nsSHEntry();
  nsSHEntry(const nsSHEntry &other);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIHISTORYENTRY
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
  nsRefPtr<nsSHEntryShared> mShared;

  // See nsSHEntry.idl for comments on these members.
  nsCOMPtr<nsIURI>         mURI;
  nsCOMPtr<nsIURI>         mReferrerURI;
  nsString                 mTitle;
  nsCOMPtr<nsIInputStream> mPostData;
  PRUint32                 mLoadType;
  PRUint32                 mID;
  PRInt32                  mScrollPositionX;
  PRInt32                  mScrollPositionY;
  nsISHEntry*              mParent;
  nsCOMArray<nsISHEntry>   mChildren;
  bool                     mURIWasModified;
  nsCOMPtr<nsIStructuredCloneContainer> mStateData;
};

#endif /* nsSHEntry_h */
