/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LoadInfo.h"

#include "mozilla/Assertions.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"

namespace mozilla {

LoadInfo::LoadInfo(nsIPrincipal* aLoadingPrincipal,
                   nsIPrincipal* aTriggeringPrincipal,
                   nsINode* aLoadingContext,
                   nsSecurityFlags aSecurityFlags,
                   nsContentPolicyType aContentPolicyType,
                   nsIURI* aBaseURI)
  : mLoadingPrincipal(aLoadingContext ?
                        aLoadingContext->NodePrincipal() : aLoadingPrincipal)
  , mTriggeringPrincipal(aTriggeringPrincipal ?
                           aTriggeringPrincipal : mLoadingPrincipal.get())
  , mLoadingContext(do_GetWeakReference(aLoadingContext))
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
  , mBaseURI(aBaseURI)
  , mInnerWindowID(aLoadingContext ?
                     aLoadingContext->OwnerDoc()->InnerWindowID() : 0)
{
  MOZ_ASSERT(mLoadingPrincipal);
  MOZ_ASSERT(mTriggeringPrincipal);

  // if consumers pass both, aLoadingContext and aLoadingPrincipal
  // then the loadingPrincipal must be the same as the node's principal
  MOZ_ASSERT(!aLoadingContext || !aLoadingPrincipal ||
             aLoadingContext->NodePrincipal() == aLoadingPrincipal);

  // if the load is sandboxed, we can not also inherit the principal
  if (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED) {
    mSecurityFlags ^= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }
}

LoadInfo::LoadInfo(nsIPrincipal* aLoadingPrincipal,
                   nsIPrincipal* aTriggeringPrincipal,
                   nsSecurityFlags aSecurityFlags,
                   nsContentPolicyType aContentPolicyType,
                   uint32_t aInnerWindowID)
  : mLoadingPrincipal(aLoadingPrincipal)
  , mTriggeringPrincipal(aTriggeringPrincipal)
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
  , mInnerWindowID(aInnerWindowID)
{
  MOZ_ASSERT(mLoadingPrincipal);
  MOZ_ASSERT(mTriggeringPrincipal);
}

LoadInfo::~LoadInfo()
{
}

NS_IMPL_ISUPPORTS(LoadInfo, nsILoadInfo)

NS_IMETHODIMP
LoadInfo::GetLoadingPrincipal(nsIPrincipal** aLoadingPrincipal)
{
  NS_ADDREF(*aLoadingPrincipal = mLoadingPrincipal);
  return NS_OK;
}

nsIPrincipal*
LoadInfo::LoadingPrincipal()
{
  return mLoadingPrincipal;
}

NS_IMETHODIMP
LoadInfo::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal)
{
  NS_ADDREF(*aTriggeringPrincipal = mTriggeringPrincipal);
  return NS_OK;
}

nsIPrincipal*
LoadInfo::TriggeringPrincipal()
{
  return mTriggeringPrincipal;
}

NS_IMETHODIMP
LoadInfo::GetLoadingDocument(nsIDOMDocument** aResult)
{
  nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext);
  if (node) {
    nsCOMPtr<nsIDOMDocument> context = do_QueryInterface(node->OwnerDoc());
    context.forget(aResult);
  }
  return NS_OK;
}

nsINode*
LoadInfo::LoadingNode()
{
  nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext);
  return node;
}

NS_IMETHODIMP
LoadInfo::GetSecurityFlags(nsSecurityFlags* aResult)
{
  *aResult = mSecurityFlags;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetForceInheritPrincipal(bool* aInheritPrincipal)
{
  *aInheritPrincipal =
    (mSecurityFlags & nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetLoadingSandboxed(bool* aLoadingSandboxed)
{
  *aLoadingSandboxed = (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetContentPolicyType(nsContentPolicyType* aResult)
{
  *aResult = mContentPolicyType;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetBaseURI(nsIURI** aBaseURI)
{
  *aBaseURI = mBaseURI;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

nsIURI*
LoadInfo::BaseURI()
{
  return mBaseURI;
}

NS_IMETHODIMP
LoadInfo::GetInnerWindowID(uint32_t* aResult)
{
  *aResult = mInnerWindowID;
  return NS_OK;
}

} // namespace mozilla
