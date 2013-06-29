/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellLoadInfo_h__
#define nsDocShellLoadInfo_h__


// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIDocShellLoadInfo.h"
#include "nsIURI.h"
#include "nsIInputStream.h"
#include "nsISHEntry.h"

class nsDocShellLoadInfo : public nsIDocShellLoadInfo
{
public:
  nsDocShellLoadInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCSHELLLOADINFO

protected:
  virtual ~nsDocShellLoadInfo();

protected:
  nsCOMPtr<nsIURI>                 mReferrer;
  nsCOMPtr<nsISupports>            mOwner;
  bool                             mInheritOwner;
  bool                             mOwnerIsExplicit;
  bool                             mSendReferrer;
  nsDocShellInfoLoadType           mLoadType;
  nsCOMPtr<nsISHEntry>             mSHEntry;
  nsString                         mTarget;
  nsCOMPtr<nsIInputStream>         mPostDataStream;
  nsCOMPtr<nsIInputStream>         mHeadersStream;
  bool                             mIsSrcdocLoad;
  nsString                         mSrcdocData;
};

#endif /* nsDocShellLoadInfo_h__ */
