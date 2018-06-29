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

class nsIInputStream;
class nsISHEntry;
class nsIURI;
class nsIDocShell;

class nsDocShellLoadInfo
{
public:
  typedef uint32_t nsDocShellInfoLoadType;
  typedef uint32_t nsDocShellInfoReferrerPolicy;
  /* these are load type enums... */
  const static uint32_t loadNormal = 0;                     // Normal Load
  const static uint32_t loadNormalReplace = 1;              // Normal Load but replaces current history slot
  const static uint32_t loadHistory = 2;                    // Load from history
  const static uint32_t loadReloadNormal = 3;               // Reload
  const static uint32_t loadReloadBypassCache = 4;
  const static uint32_t loadReloadBypassProxy = 5;
  const static uint32_t loadReloadBypassProxyAndCache = 6;
  const static uint32_t loadLink = 7;
  const static uint32_t loadRefresh = 8;
  const static uint32_t loadReloadCharsetChange = 9;
  const static uint32_t loadBypassHistory = 10;
  const static uint32_t loadStopContent = 11;
  const static uint32_t loadStopContentAndReplace = 12;
  const static uint32_t loadNormalExternal = 13;
  const static uint32_t loadNormalBypassCache = 14;
  const static uint32_t loadNormalBypassProxy = 15;
  const static uint32_t loadNormalBypassProxyAndCache = 16;
  const static uint32_t loadPushState = 17;                 // history.pushState or replaceState
  const static uint32_t loadReplaceBypassCache = 18;
  const static uint32_t loadReloadMixedContent = 19;
  const static uint32_t loadNormalAllowMixedContent = 20;
  const static uint32_t loadReloadCharsetChangeBypassCache = 21;
  const static uint32_t loadReloadCharsetChangeBypassProxyAndCache = 22;


  NS_INLINE_DECL_REFCOUNTING(nsDocShellLoadInfo);

  nsDocShellLoadInfo();

  NS_IMETHOD GetReferrer(nsIURI** aReferrer);

  NS_IMETHOD SetReferrer(nsIURI* aReferrer);

  NS_IMETHOD GetOriginalURI(nsIURI** aOriginalURI);

  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI);

  NS_IMETHOD GetResultPrincipalURI(nsIURI** aResultPrincipalURI);

  NS_IMETHOD
  SetResultPrincipalURI(nsIURI* aResultPrincipalURI);

  NS_IMETHOD
  GetResultPrincipalURIIsSome(bool* aIsSome);

  NS_IMETHOD
  SetResultPrincipalURIIsSome(bool aIsSome);

  NS_IMETHOD
  GetLoadReplace(bool* aLoadReplace);

  NS_IMETHOD
  SetLoadReplace(bool aLoadReplace);

  NS_IMETHOD
  GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal);

  NS_IMETHOD
  SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal);

  NS_IMETHOD
  GetInheritPrincipal(bool* aInheritPrincipal);

  NS_IMETHOD
  SetInheritPrincipal(bool aInheritPrincipal);

  NS_IMETHOD
  GetPrincipalIsExplicit(bool* aPrincipalIsExplicit);

  NS_IMETHOD
  SetPrincipalIsExplicit(bool aPrincipalIsExplicit);

  NS_IMETHOD
  GetForceAllowDataURI(bool* aForceAllowDataURI);

  NS_IMETHOD
  SetForceAllowDataURI(bool aForceAllowDataURI);

  NS_IMETHOD GetOriginalFrameSrc(bool* aOriginalFrameSrc);

  NS_IMETHOD SetOriginalFrameSrc(bool aOriginalFrameSrc);

  NS_IMETHOD GetLoadType(nsDocShellInfoLoadType* aLoadType);

  NS_IMETHOD SetLoadType(nsDocShellInfoLoadType aLoadType);

  NS_IMETHOD GetSHEntry(nsISHEntry** aSHEntry);

  NS_IMETHOD SetSHEntry(nsISHEntry* aSHEntry);

  NS_IMETHOD GetTarget(char16_t** aTarget);

  NS_IMETHOD SetTarget(const char16_t* aTarget);

  NS_IMETHOD GetPostDataStream(nsIInputStream** aResult);

  NS_IMETHOD SetPostDataStream(nsIInputStream* aStream);

  NS_IMETHOD GetHeadersStream(nsIInputStream** aHeadersStream);

  NS_IMETHOD SetHeadersStream(nsIInputStream* aHeadersStream);

  NS_IMETHOD GetSendReferrer(bool* aSendReferrer);

  NS_IMETHOD SetSendReferrer(bool aSendReferrer);

  NS_IMETHOD GetReferrerPolicy(nsDocShellInfoReferrerPolicy* aReferrerPolicy);

  NS_IMETHOD SetReferrerPolicy(nsDocShellInfoReferrerPolicy aReferrerPolicy);

  NS_IMETHOD GetIsSrcdocLoad(bool* aIsSrcdocLoad);

  NS_IMETHOD GetSrcdocData(nsAString& aSrcdocData);

  NS_IMETHOD SetSrcdocData(const nsAString& aSrcdocData);

  NS_IMETHOD GetSourceDocShell(nsIDocShell** aSourceDocShell);

  NS_IMETHOD SetSourceDocShell(nsIDocShell* aSourceDocShell);

  NS_IMETHOD GetBaseURI(nsIURI** aBaseURI);

  NS_IMETHOD SetBaseURI(nsIURI* aBaseURI);

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

namespace mozilla {
void
GetMaybeResultPrincipalURI(nsDocShellLoadInfo* aLoadInfo, Maybe<nsCOMPtr<nsIURI>>& aRPURI);
void
SetMaybeResultPrincipalURI(nsDocShellLoadInfo* aLoadInfo, Maybe<nsCOMPtr<nsIURI>> const& aRPURI);
}

#endif /* nsDocShellLoadInfo_h__ */
