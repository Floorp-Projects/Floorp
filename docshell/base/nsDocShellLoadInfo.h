/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellLoadInfo_h__
#define nsDocShellLoadInfo_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIDocShellLoadInfo.h"

class nsIInputStream;
class nsISHEntry;
class nsIURI;
class nsIDocShell;

class nsDocShellLoadInfo : public nsIDocShellLoadInfo
{
public:
  nsDocShellLoadInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCSHELLLOADINFO

protected:
  virtual ~nsDocShellLoadInfo();

protected:
  nsCOMPtr<nsIURI> mReferrer;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  bool mResultPrincipalURIIsSome;
  bool mLoadReplace;
  bool mInheritPrincipal;
  bool mPrincipalIsExplicit;
  bool mForceAllowDataURI;
  bool mOriginalFrameSrc;
  bool mSendReferrer;
  nsDocShellInfoReferrerPolicy mReferrerPolicy;
  nsDocShellInfoLoadType mLoadType;
  nsCOMPtr<nsISHEntry> mSHEntry;
  nsString mTarget;
  nsCOMPtr<nsIInputStream> mPostDataStream;
  nsCOMPtr<nsIInputStream> mHeadersStream;
  bool mIsSrcdocLoad;
  nsString mSrcdocData;
  nsCOMPtr<nsIDocShell> mSourceDocShell;
  nsCOMPtr<nsIURI> mBaseURI;
};

#endif /* nsDocShellLoadInfo_h__ */
