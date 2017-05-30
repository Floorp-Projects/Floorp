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

namespace mozilla {

void
GetMaybeResultPrincipalURI(nsIDocShellLoadInfo* aLoadInfo, Maybe<nsCOMPtr<nsIURI>>& aRPURI)
{
  if (!aLoadInfo) {
    return;
  }

  nsresult rv;

  bool isSome;
  rv = aLoadInfo->GetResultPrincipalURIIsSome(&isSome);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  aRPURI.reset();

  if (!isSome) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  rv = aLoadInfo->GetResultPrincipalURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  aRPURI.emplace(Move(uri));
}

void
SetMaybeResultPrincipalURI(nsIDocShellLoadInfo* aLoadInfo, Maybe<nsCOMPtr<nsIURI>> const& aRPURI)
{
  if (!aLoadInfo) {
    return;
  }

  nsresult rv;

  rv = aLoadInfo->SetResultPrincipalURI(aRPURI.refOr(nullptr));
  Unused << NS_WARN_IF(NS_FAILED(rv));

  rv = aLoadInfo->SetResultPrincipalURIIsSome(aRPURI.isSome());
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

} // mozilla

nsDocShellLoadInfo::nsDocShellLoadInfo()
  : mResultPrincipalURIIsSome(false)
  , mLoadReplace(false)
  , mInheritPrincipal(false)
  , mPrincipalIsExplicit(false)
  , mSendReferrer(true)
  , mReferrerPolicy(mozilla::net::RP_Unset)
  , mLoadType(nsIDocShellLoadInfo::loadNormal)
  , mIsSrcdocLoad(false)
{
}

nsDocShellLoadInfo::~nsDocShellLoadInfo()
{
}

NS_IMPL_ADDREF(nsDocShellLoadInfo)
NS_IMPL_RELEASE(nsDocShellLoadInfo)

NS_INTERFACE_MAP_BEGIN(nsDocShellLoadInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellLoadInfo)
  NS_INTERFACE_MAP_ENTRY(nsIDocShellLoadInfo)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDocShellLoadInfo::GetReferrer(nsIURI** aReferrer)
{
  NS_ENSURE_ARG_POINTER(aReferrer);

  *aReferrer = mReferrer;
  NS_IF_ADDREF(*aReferrer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetReferrer(nsIURI* aReferrer)
{
  mReferrer = aReferrer;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetOriginalURI(nsIURI** aOriginalURI)
{
  NS_ENSURE_ARG_POINTER(aOriginalURI);

  *aOriginalURI = mOriginalURI;
  NS_IF_ADDREF(*aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetOriginalURI(nsIURI* aOriginalURI)
{
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetResultPrincipalURI(nsIURI** aResultPrincipalURI)
{
  NS_ENSURE_ARG_POINTER(aResultPrincipalURI);

  *aResultPrincipalURI = mResultPrincipalURI;
  NS_IF_ADDREF(*aResultPrincipalURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetResultPrincipalURI(nsIURI* aResultPrincipalURI)
{
  mResultPrincipalURI = aResultPrincipalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetResultPrincipalURIIsSome(bool* aIsSome)
{
  *aIsSome = mResultPrincipalURIIsSome;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetResultPrincipalURIIsSome(bool aIsSome)
{
  mResultPrincipalURIIsSome = aIsSome;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetLoadReplace(bool* aLoadReplace)
{
  *aLoadReplace = mLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetLoadReplace(bool aLoadReplace)
{
  mLoadReplace = aLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal)
{
  NS_ENSURE_ARG_POINTER(aTriggeringPrincipal);
  NS_IF_ADDREF(*aTriggeringPrincipal = mTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal)
{
  mTriggeringPrincipal = aTriggeringPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetInheritPrincipal(bool* aInheritPrincipal)
{
  NS_ENSURE_ARG_POINTER(aInheritPrincipal);
  *aInheritPrincipal = mInheritPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetInheritPrincipal(bool aInheritPrincipal)
{
  mInheritPrincipal = aInheritPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetPrincipalIsExplicit(bool* aPrincipalIsExplicit)
{
  *aPrincipalIsExplicit = mPrincipalIsExplicit;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetPrincipalIsExplicit(bool aPrincipalIsExplicit)
{
  mPrincipalIsExplicit = aPrincipalIsExplicit;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetLoadType(nsDocShellInfoLoadType* aLoadType)
{
  NS_ENSURE_ARG_POINTER(aLoadType);

  *aLoadType = mLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetLoadType(nsDocShellInfoLoadType aLoadType)
{
  mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetSHEntry(nsISHEntry** aSHEntry)
{
  NS_ENSURE_ARG_POINTER(aSHEntry);

  *aSHEntry = mSHEntry;
  NS_IF_ADDREF(*aSHEntry);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetSHEntry(nsISHEntry* aSHEntry)
{
  mSHEntry = aSHEntry;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetTarget(char16_t** aTarget)
{
  NS_ENSURE_ARG_POINTER(aTarget);

  *aTarget = ToNewUnicode(mTarget);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetTarget(const char16_t* aTarget)
{
  mTarget.Assign(aTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetPostDataStream(nsIInputStream** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mPostDataStream;

  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetPostDataStream(nsIInputStream* aStream)
{
  mPostDataStream = aStream;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetHeadersStream(nsIInputStream** aHeadersStream)
{
  NS_ENSURE_ARG_POINTER(aHeadersStream);
  *aHeadersStream = mHeadersStream;
  NS_IF_ADDREF(*aHeadersStream);
  return NS_OK;
}
NS_IMETHODIMP
nsDocShellLoadInfo::SetHeadersStream(nsIInputStream* aHeadersStream)
{
  mHeadersStream = aHeadersStream;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetSendReferrer(bool* aSendReferrer)
{
  NS_ENSURE_ARG_POINTER(aSendReferrer);

  *aSendReferrer = mSendReferrer;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetSendReferrer(bool aSendReferrer)
{
  mSendReferrer = aSendReferrer;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetReferrerPolicy(
    nsDocShellInfoReferrerPolicy* aReferrerPolicy)
{
  *aReferrerPolicy = mReferrerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetReferrerPolicy(
    nsDocShellInfoReferrerPolicy aReferrerPolicy)
{
  mReferrerPolicy = aReferrerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetIsSrcdocLoad(bool* aIsSrcdocLoad)
{
  *aIsSrcdocLoad = mIsSrcdocLoad;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetSrcdocData(nsAString& aSrcdocData)
{
  aSrcdocData = mSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetSrcdocData(const nsAString& aSrcdocData)
{
  mSrcdocData = aSrcdocData;
  mIsSrcdocLoad = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetSourceDocShell(nsIDocShell** aSourceDocShell)
{
  MOZ_ASSERT(aSourceDocShell);
  nsCOMPtr<nsIDocShell> result = mSourceDocShell;
  result.forget(aSourceDocShell);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetSourceDocShell(nsIDocShell* aSourceDocShell)
{
  mSourceDocShell = aSourceDocShell;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::GetBaseURI(nsIURI** aBaseURI)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);

  *aBaseURI = mBaseURI;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShellLoadInfo::SetBaseURI(nsIURI* aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}
