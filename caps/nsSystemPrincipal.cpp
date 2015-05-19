/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#include "nscore.h"
#include "nsSystemPrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIClassInfoImpl.h"
#include "nsIScriptSecurityManager.h"
#include "pratom.h"

NS_IMPL_CLASSINFO(nsSystemPrincipal, nullptr,
                  nsIClassInfo::SINGLETON | nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_SYSTEMPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsSystemPrincipal,
                           nsIPrincipal,
                           nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(nsSystemPrincipal,
                            nsIPrincipal,
                            nsISerializable)

#define SYSTEM_PRINCIPAL_SPEC "[System Principal]"

void
nsSystemPrincipal::GetScriptLocation(nsACString &aStr)
{
    aStr.AssignLiteral(SYSTEM_PRINCIPAL_SPEC);
}

///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::CheckMayLoad(nsIURI* uri, bool aReport, bool aAllowIfInheritsPrincipal)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetHashValue(uint32_t *result)
{
    *result = NS_PTR_TO_INT32(this);
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetURI(nsIURI** aURI)
{
    *aURI = nullptr;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetOrigin(nsACString& aOrigin)
{
    aOrigin.AssignLiteral(SYSTEM_PRINCIPAL_SPEC);
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  *aCsp = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // CSP on a null principal makes no sense
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetDomain(nsIURI** aDomain)
{
    *aDomain = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SetDomain(nsIURI* aDomain)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  // No base domain for chrome.
  return NS_OK;
}

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::Read(nsIObjectInputStream* aStream)
{
    // no-op: CID is sufficient to identify the mSystemPrincipal singleton
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::Write(nsIObjectOutputStream* aStream)
{
    // no-op: CID is sufficient to identify the mSystemPrincipal singleton
    return NS_OK;
}
