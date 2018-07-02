/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellLoadInfo.h"
#include "nsISHEntry.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Unused.h"
#include "mozilla/Maybe.h"

namespace mozilla {


} // mozilla

nsDocShellLoadInfo::nsDocShellLoadInfo()
  : mResultPrincipalURIIsSome(false)
  , mLoadReplace(false)
  , mInheritPrincipal(false)
  , mPrincipalIsExplicit(false)
  , mForceAllowDataURI(false)
  , mOriginalFrameSrc(false)
  , mSendReferrer(true)
  , mReferrerPolicy(mozilla::net::RP_Unset)
  , mLoadType(LOAD_NORMAL)
  , mIsSrcdocLoad(false)
{
}

nsDocShellLoadInfo::~nsDocShellLoadInfo()
{
}

nsIURI*
nsDocShellLoadInfo::Referrer() const
{
  return mReferrer;
}

void
nsDocShellLoadInfo::SetReferrer(nsIURI* aReferrer)
{
  mReferrer = aReferrer;
}

nsIURI*
nsDocShellLoadInfo::OriginalURI() const
{
  return mOriginalURI;
}

void
nsDocShellLoadInfo::SetOriginalURI(nsIURI* aOriginalURI)
{
  mOriginalURI = aOriginalURI;
}

nsIURI*
nsDocShellLoadInfo::ResultPrincipalURI() const
{
  return mResultPrincipalURI;
}

void
nsDocShellLoadInfo::SetResultPrincipalURI(nsIURI* aResultPrincipalURI)
{
  mResultPrincipalURI = aResultPrincipalURI;
}

bool
nsDocShellLoadInfo::ResultPrincipalURIIsSome() const
{
  return mResultPrincipalURIIsSome;
}

void
nsDocShellLoadInfo::SetResultPrincipalURIIsSome(bool aIsSome)
{
  mResultPrincipalURIIsSome = aIsSome;
}

bool
nsDocShellLoadInfo::LoadReplace() const
{
  return mLoadReplace;
}

void
nsDocShellLoadInfo::SetLoadReplace(bool aLoadReplace)
{
  mLoadReplace = aLoadReplace;
}

nsIPrincipal*
nsDocShellLoadInfo::TriggeringPrincipal() const
{
  return mTriggeringPrincipal;
}

void
nsDocShellLoadInfo::SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal)
{
  mTriggeringPrincipal = aTriggeringPrincipal;
}

bool
nsDocShellLoadInfo::InheritPrincipal() const
{
  return mInheritPrincipal;
}

void
nsDocShellLoadInfo::SetInheritPrincipal(bool aInheritPrincipal)
{
  mInheritPrincipal = aInheritPrincipal;
}

bool
nsDocShellLoadInfo::PrincipalIsExplicit() const
{
  return mPrincipalIsExplicit;
}

void
nsDocShellLoadInfo::SetPrincipalIsExplicit(bool aPrincipalIsExplicit)
{
  mPrincipalIsExplicit = aPrincipalIsExplicit;
}

bool
nsDocShellLoadInfo::ForceAllowDataURI() const
{
  return mForceAllowDataURI;
}

void
nsDocShellLoadInfo::SetForceAllowDataURI(bool aForceAllowDataURI)
{
  mForceAllowDataURI = aForceAllowDataURI;
}

bool
nsDocShellLoadInfo::OriginalFrameSrc() const
{
  return mOriginalFrameSrc;
}

void
nsDocShellLoadInfo::SetOriginalFrameSrc(bool aOriginalFrameSrc)
{
  mOriginalFrameSrc = aOriginalFrameSrc;
}

uint32_t
nsDocShellLoadInfo::LoadType() const
{
  return mLoadType;
}

void
nsDocShellLoadInfo::SetLoadType(uint32_t aLoadType)
{
  mLoadType = aLoadType;
}

nsISHEntry*
nsDocShellLoadInfo::SHEntry() const
{
  return mSHEntry;
}

void
nsDocShellLoadInfo::SetSHEntry(nsISHEntry* aSHEntry)
{
  mSHEntry = aSHEntry;
}

void
nsDocShellLoadInfo::GetTarget(nsAString& aTarget) const
{
  aTarget = mTarget;
}

void
nsDocShellLoadInfo::SetTarget(const nsAString& aTarget)
{
  mTarget = aTarget;
}

nsIInputStream*
nsDocShellLoadInfo::PostDataStream() const
{
  return mPostDataStream;
}

void
nsDocShellLoadInfo::SetPostDataStream(nsIInputStream* aStream)
{
  mPostDataStream = aStream;
}

nsIInputStream*
nsDocShellLoadInfo::HeadersStream() const
{
  return mHeadersStream;
}

void
nsDocShellLoadInfo::SetHeadersStream(nsIInputStream* aHeadersStream)
{
  mHeadersStream = aHeadersStream;
}

bool
nsDocShellLoadInfo::SendReferrer() const
{
  return mSendReferrer;
}

void
nsDocShellLoadInfo::SetSendReferrer(bool aSendReferrer)
{
  mSendReferrer = aSendReferrer;
}

uint32_t
nsDocShellLoadInfo::ReferrerPolicy() const
{
  return mReferrerPolicy;
}

void
nsDocShellLoadInfo::SetReferrerPolicy(mozilla::net::ReferrerPolicy aReferrerPolicy)
{
  mReferrerPolicy = aReferrerPolicy;
}

bool
nsDocShellLoadInfo::IsSrcdocLoad() const
{
  return mIsSrcdocLoad;
}

void
nsDocShellLoadInfo::GetSrcdocData(nsAString& aSrcdocData) const
{
  aSrcdocData = mSrcdocData;
}

void
nsDocShellLoadInfo::SetSrcdocData(const nsAString& aSrcdocData)
{
  mSrcdocData = aSrcdocData;
  mIsSrcdocLoad = true;
}

nsIDocShell*
nsDocShellLoadInfo::SourceDocShell() const
{
  return mSourceDocShell;
}

void
nsDocShellLoadInfo::SetSourceDocShell(nsIDocShell* aSourceDocShell)
{
  mSourceDocShell = aSourceDocShell;
}

nsIURI*
nsDocShellLoadInfo::BaseURI() const
{
  return mBaseURI;
}

void
nsDocShellLoadInfo::SetBaseURI(nsIURI* aBaseURI)
{
  mBaseURI = aBaseURI;
}

void
nsDocShellLoadInfo::GetMaybeResultPrincipalURI(mozilla::Maybe<nsCOMPtr<nsIURI>>& aRPURI) const
{
  bool isSome = ResultPrincipalURIIsSome();
  aRPURI.reset();

  if (!isSome) {
    return;
  }

  nsCOMPtr<nsIURI> uri = ResultPrincipalURI();
  aRPURI.emplace(std::move(uri));
}

void
nsDocShellLoadInfo::SetMaybeResultPrincipalURI(mozilla::Maybe<nsCOMPtr<nsIURI>> const& aRPURI)
{
  SetResultPrincipalURI(aRPURI.refOr(nullptr));
  SetResultPrincipalURIIsSome(aRPURI.isSome());
}
