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
NS_IMPL_QUERY_INTERFACE2_CI(nsSystemPrincipal,
                            nsIPrincipal,
                            nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER2(nsSystemPrincipal,
                             nsIPrincipal,
                             nsISerializable)

NS_IMETHODIMP_(nsrefcnt) 
nsSystemPrincipal::AddRef()
{
  NS_PRECONDITION(int32_t(refcount) >= 0, "illegal refcnt");
  nsrefcnt count = PR_ATOMIC_INCREMENT(&refcount);
  NS_LOG_ADDREF(this, count, "nsSystemPrincipal", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(nsrefcnt)
nsSystemPrincipal::Release()
{
  NS_PRECONDITION(0 != refcount, "dup release");
  nsrefcnt count = PR_ATOMIC_DECREMENT(&refcount);
  NS_LOG_RELEASE(this, count, "nsSystemPrincipal");
  if (count == 0) {
    delete this;
  }

  return count;
}

static const char SYSTEM_PRINCIPAL_SPEC[] = "[System Principal]";

void
nsSystemPrincipal::GetScriptLocation(nsACString &aStr)
{
    aStr.Assign(SYSTEM_PRINCIPAL_SPEC);
}

#ifdef DEBUG
void nsSystemPrincipal::dumpImpl()
{
  fprintf(stderr, "nsSystemPrincipal (%p)\n", this);
}
#endif 


///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::Equals(nsIPrincipal *other, bool *result)
{
    *result = (other == this);
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::EqualsIgnoringDomain(nsIPrincipal *other, bool *result)
{
    return Equals(other, result);
}

NS_IMETHODIMP
nsSystemPrincipal::Subsumes(nsIPrincipal *other, bool *result)
{
    *result = true;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SubsumesIgnoringDomain(nsIPrincipal *other, bool *result)
{
    *result = true;
    return NS_OK;
}

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
nsSystemPrincipal::GetOrigin(char** aOrigin)
{
    *aOrigin = ToNewCString(NS_LITERAL_CSTRING(SYSTEM_PRINCIPAL_SPEC));
    return *aOrigin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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
nsSystemPrincipal::GetSecurityPolicy(void** aSecurityPolicy)
{
    *aSecurityPolicy = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SetSecurityPolicy(void* aSecurityPolicy)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetJarPrefix(nsACString& aJarPrefix)
{
  aJarPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetAppStatus(uint16_t* aAppStatus)
{
  *aAppStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetAppId(uint32_t* aAppId)
{
  *aAppId = nsIScriptSecurityManager::NO_APP_ID;
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = false;
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetUnknownAppId(bool* aUnknownAppId)
{
  *aUnknownAppId = false;
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetIsNullPrincipal(bool* aIsNullPrincipal)
{
  *aIsNullPrincipal = false;
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

/////////////////////////////////////////////
// Constructor, Destructor, initialization //
/////////////////////////////////////////////

nsSystemPrincipal::nsSystemPrincipal()
{
}

nsSystemPrincipal::~nsSystemPrincipal()
{
}
