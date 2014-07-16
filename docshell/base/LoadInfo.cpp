/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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

LoadInfo::LoadInfo(nsIPrincipal* aPrincipal,
                   nsINode* aLoadingContext,
                   nsSecurityFlags aSecurityFlags,
                   nsContentPolicyType aContentPolicyType)
  : mPrincipal(aPrincipal)
  , mLoadingContext(do_GetWeakReference(aLoadingContext))
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
{
  MOZ_ASSERT(aPrincipal);
  // if the load is sandboxed, we can not also inherit the principal
  if (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED) {
    mSecurityFlags ^= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }
}

LoadInfo::~LoadInfo()
{
}

NS_IMPL_ISUPPORTS(LoadInfo, nsILoadInfo)

NS_IMETHODIMP
LoadInfo::GetLoadingPrincipal(nsIPrincipal** aPrincipal)
{
  NS_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

nsIPrincipal*
LoadInfo::LoadingPrincipal()
{
  return mPrincipal;
}

NS_IMETHODIMP
LoadInfo::GetLoadingDocument(nsIDOMDocument** outLoadingDocument)
{
  nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext);
  if (node) {
    nsCOMPtr<nsIDOMDocument> context = do_QueryInterface(node->OwnerDoc());
    context.forget(outLoadingDocument);
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
LoadInfo::GetSecurityFlags(nsSecurityFlags* outSecurityFlags)
{
  *outSecurityFlags = mSecurityFlags;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetForceInheritPrincipal(bool* aInheritPrincipal)
{
  *aInheritPrincipal = (mSecurityFlags & nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetLoadingSandboxed(bool* aLoadingSandboxed)
{
  *aLoadingSandboxed = (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetContentPolicyType(nsContentPolicyType* outContentPolicyType)
{
  *outContentPolicyType = mContentPolicyType;
  return NS_OK;
}

} // namespace mozilla
