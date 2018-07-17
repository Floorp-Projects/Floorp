/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#include "nscore.h"
#include "SystemPrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIClassInfoImpl.h"
#include "nsIScriptSecurityManager.h"
#include "pratom.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(SystemPrincipal, nullptr,
                  nsIClassInfo::SINGLETON | nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_SYSTEMPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(SystemPrincipal,
                           nsIPrincipal,
                           nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(SystemPrincipal,
                            nsIPrincipal,
                            nsISerializable)

#define SYSTEM_PRINCIPAL_SPEC "[System Principal]"

already_AddRefed<SystemPrincipal>
SystemPrincipal::Create()
{
  RefPtr<SystemPrincipal> sp = new SystemPrincipal();
  sp->FinishInit(NS_LITERAL_CSTRING(SYSTEM_PRINCIPAL_SPEC),
                 OriginAttributes());
  return sp.forget();
}

nsresult
SystemPrincipal::GetScriptLocation(nsACString &aStr)
{
    aStr.AssignLiteral(SYSTEM_PRINCIPAL_SPEC);
    return NS_OK;
}

///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////

NS_IMETHODIMP
SystemPrincipal::GetHashValue(uint32_t *result)
{
    *result = NS_PTR_TO_INT32(this);
    return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetURI(nsIURI** aURI)
{
    *aURI = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  *aCsp = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // Never destroy an existing CSP on the principal.
  // This method should only be called in rare cases.

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SystemPrincipal::EnsureCSP(nsIDocument* aDocument,
                           nsIContentSecurityPolicy** aCSP)
{
  // CSP on a system principal makes no sense
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SystemPrincipal::GetPreloadCsp(nsIContentSecurityPolicy** aPreloadCSP)
{
  *aPreloadCSP = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::EnsurePreloadCSP(nsIDocument* aDocument,
                                  nsIContentSecurityPolicy** aPreloadCSP)
{
  // CSP on a system principal makes no sense
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetDomain(nsIURI** aDomain)
{
    *aDomain = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::SetDomain(nsIURI* aDomain)
{
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  // No base domain for chrome.
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetAddonId(nsAString& aAddonId)
{
  aAddonId.Truncate();
  return NS_OK;
};

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
SystemPrincipal::Read(nsIObjectInputStream* aStream)
{
    // no-op: CID is sufficient to identify the mSystemPrincipal singleton
    return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::Write(nsIObjectOutputStream* aStream)
{
    // no-op: CID is sufficient to identify the mSystemPrincipal singleton
    return NS_OK;
}
