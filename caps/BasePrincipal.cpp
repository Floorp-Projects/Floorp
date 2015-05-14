/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"

#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsScriptSecurityManager.h"

namespace mozilla {

void
BasePrincipal::OriginAttributes::Serialize(nsIObjectOutputStream* aStream) const
{
  aStream->Write32(mAppId);
  aStream->WriteBoolean(mIsInBrowserElement);
}

nsresult
BasePrincipal::OriginAttributes::Deserialize(nsIObjectInputStream* aStream)
{
  nsresult rv = aStream->Read32(&mAppId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&mIsInBrowserElement);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool
BasePrincipal::Subsumes(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration)
{
  MOZ_RELEASE_ASSERT(aOther, "The caller is performing a nonsensical security check!");
  return SubsumesInternal(aOther, aConsideration);
}

NS_IMETHODIMP
BasePrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{

  *aResult = Subsumes(aOther, DontConsiderDocumentDomain) &&
             Cast(aOther)->Subsumes(this, DontConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  *aResult = Subsumes(aOther, ConsiderDocumentDomain) &&
             Cast(aOther)->Subsumes(this, ConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  *aResult = Subsumes(aOther, DontConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  *aResult = Subsumes(aOther, ConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  NS_IF_ADDREF(*aCsp = mCSP);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // If CSP was already set, it should not be destroyed!  Instead, it should
  // get set anew when a new principal is created.
  if (mCSP)
    return NS_ERROR_ALREADY_INITIALIZED;

  mCSP = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsNullPrincipal(bool* aIsNullPrincipal)
{
  *aIsNullPrincipal = false;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetJarPrefix(nsACString& aJarPrefix)
{
  MOZ_ASSERT(AppId() != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mozilla::GetJarPrefix(AppId(), IsInBrowserElement(), aJarPrefix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAppStatus(uint16_t* aAppStatus)
{
  if (AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    NS_WARNING("Asking for app status on a principal with an unknown app id");
    *aAppStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    return NS_OK;
  }

  *aAppStatus = nsScriptSecurityManager::AppStatusForPrincipal(this);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAppId(uint32_t* aAppId)
{
  if (AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    MOZ_ASSERT(false);
    *aAppId = nsIScriptSecurityManager::NO_APP_ID;
    return NS_OK;
  }

  *aAppId = AppId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = IsInBrowserElement();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetUnknownAppId(bool* aUnknownAppId)
{
  *aUnknownAppId = AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID;
  return NS_OK;
}

} // namespace mozilla
