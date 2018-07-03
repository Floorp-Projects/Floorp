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

  enum LoadType: uint32_t {
    loadNormal = 0,                     // Normal Load
    loadNormalReplace = 1,              // Normal Load but replaces current history slot
    loadHistory = 2,                    // Load from history
    loadReloadNormal = 3,               // Reload
    loadReloadBypassCache = 4,
    loadReloadBypassProxy = 5,
    loadReloadBypassProxyAndCache = 6,
    loadLink = 7,
    loadRefresh = 8,
    loadReloadCharsetChange = 9,
    loadBypassHistory = 10,
    loadStopContent = 11,
    loadStopContentAndReplace = 12,
    loadNormalExternal = 13,
    loadNormalBypassCache = 14,
    loadNormalBypassProxy = 15,
    loadNormalBypassProxyAndCache = 16,
    loadPushState = 17,                 // history.pushState or replaceState
    loadReplaceBypassCache = 18,
    loadReloadMixedContent = 19,
    loadNormalAllowMixedContent = 20,
    loadReloadCharsetChangeBypassCache = 21,
    loadReloadCharsetChangeBypassProxyAndCache = 22
  };

  NS_INLINE_DECL_REFCOUNTING(nsDocShellLoadInfo);

  nsDocShellLoadInfo();

  nsIURI* Referrer() const;

  void SetReferrer(nsIURI* aReferrer);

  nsIURI* OriginalURI() const;

  void SetOriginalURI(nsIURI* aOriginalURI);

  nsIURI* ResultPrincipalURI() const;

  void SetResultPrincipalURI(nsIURI* aResultPrincipalURI);

  bool ResultPrincipalURIIsSome() const;

  void SetResultPrincipalURIIsSome(bool aIsSome);

  bool LoadReplace() const;

  void SetLoadReplace(bool aLoadReplace);

  nsIPrincipal* TriggeringPrincipal() const;

  void SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal);

  bool InheritPrincipal() const;

  void SetInheritPrincipal(bool aInheritPrincipal);

  bool PrincipalIsExplicit() const;

  void SetPrincipalIsExplicit(bool aPrincipalIsExplicit);

  bool ForceAllowDataURI() const;

  void SetForceAllowDataURI(bool aForceAllowDataURI);

  bool OriginalFrameSrc() const;

  void SetOriginalFrameSrc(bool aOriginalFrameSrc);

  nsDocShellInfoLoadType LoadType() const;

  void SetLoadType(nsDocShellInfoLoadType aLoadType);

  nsISHEntry* SHEntry() const;

  void SetSHEntry(nsISHEntry* aSHEntry);

  void GetTarget(nsAString& aTarget) const;

  void SetTarget(const nsAString& aTarget);

  nsIInputStream* PostDataStream() const;

  void SetPostDataStream(nsIInputStream* aStream);

  nsIInputStream* HeadersStream() const;

  void SetHeadersStream(nsIInputStream* aHeadersStream);

  bool SendReferrer() const;

  void SetSendReferrer(bool aSendReferrer);

  uint32_t ReferrerPolicy() const;

  void SetReferrerPolicy(mozilla::net::ReferrerPolicy aReferrerPolicy);

  bool IsSrcdocLoad() const;

  void GetSrcdocData(nsAString& aSrcdocData) const;

  void SetSrcdocData(const nsAString& aSrcdocData);

  nsIDocShell* SourceDocShell() const;

  void SetSourceDocShell(nsIDocShell* aSourceDocShell);

  nsIURI* BaseURI() const;

  void SetBaseURI(nsIURI* aBaseURI);

  void
  GetMaybeResultPrincipalURI(mozilla::Maybe<nsCOMPtr<nsIURI>>& aRPURI) const;

  void
  SetMaybeResultPrincipalURI(mozilla::Maybe<nsCOMPtr<nsIURI>> const& aRPURI);

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
  mozilla::net::ReferrerPolicy mReferrerPolicy;
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
