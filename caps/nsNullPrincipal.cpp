/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the principal that has no rights and can't be accessed by
 * anything other than itself and chrome; null principals are not
 * same-origin with anything but themselves.
 */

#include "mozilla/ArrayUtils.h"

#include "nsNullPrincipal.h"
#include "nsNullPrincipalURI.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsIClassInfoImpl.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "nsIScriptSecurityManager.h"
#include "nsPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "pratom.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(nsNullPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_NULLPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsNullPrincipal,
                           nsIPrincipal,
                           nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(nsNullPrincipal,
                            nsIPrincipal,
                            nsISerializable)

/* static */ already_AddRefed<nsNullPrincipal>
nsNullPrincipal::CreateWithInheritedAttributes(nsIPrincipal* aInheritFrom)
{
  nsRefPtr<nsNullPrincipal> nullPrin = new nsNullPrincipal();
  nsresult rv = nullPrin->Init(Cast(aInheritFrom)->OriginAttributesRef());
  return NS_SUCCEEDED(rv) ? nullPrin.forget() : nullptr;
}

/* static */ already_AddRefed<nsNullPrincipal>
nsNullPrincipal::Create(const OriginAttributes& aOriginAttributes)
{
  nsRefPtr<nsNullPrincipal> nullPrin = new nsNullPrincipal();
  nsresult rv = nullPrin->Init(aOriginAttributes);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return nullPrin.forget();
}

nsresult
nsNullPrincipal::Init(const OriginAttributes& aOriginAttributes)
{
  mOriginAttributes = aOriginAttributes;
  MOZ_ASSERT(AppId() != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mURI = nsNullPrincipalURI::Create();
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_AVAILABLE);

  return NS_OK;
}

void
nsNullPrincipal::GetScriptLocation(nsACString &aStr)
{
  mURI->GetSpec(aStr);
}

/**
 * nsIPrincipal implementation
 */

NS_IMETHODIMP
nsNullPrincipal::GetHashValue(uint32_t *aResult)
{
  *aResult = (NS_PTR_TO_INT32(this) >> 2);
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::GetURI(nsIURI** aURI)
{
  return NS_EnsureSafeToReturn(mURI, aURI);
}

NS_IMETHODIMP
nsNullPrincipal::GetDomain(nsIURI** aDomain)
{
  return NS_EnsureSafeToReturn(mURI, aDomain);
}

NS_IMETHODIMP
nsNullPrincipal::SetDomain(nsIURI* aDomain)
{
  // I think the right thing to do here is to just throw...  Silently failing
  // seems counterproductive.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsNullPrincipal::GetOrigin(nsACString& aOrigin)
{
  return mURI->GetSpec(aOrigin);
}

NS_IMETHODIMP
nsNullPrincipal::CheckMayLoad(nsIURI* aURI, bool aReport, bool aAllowIfInheritsPrincipal)
 {
  if (aAllowIfInheritsPrincipal) {
    if (nsPrincipal::IsPrincipalInherited(aURI)) {
      return NS_OK;
    }
  }

  // Also allow the load if we are the principal of the URI being checked.
  nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(aURI);
  if (uriPrinc) {
    nsCOMPtr<nsIPrincipal> principal;
    uriPrinc->GetPrincipal(getter_AddRefs(principal));

    if (principal == this) {
      return NS_OK;
    }
  }

  if (aReport) {
    nsScriptSecurityManager::ReportError(
      nullptr, NS_LITERAL_STRING("CheckSameOriginError"), mURI, aURI);
  }

  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
nsNullPrincipal::GetIsNullPrincipal(bool* aIsNullPrincipal)
{
  *aIsNullPrincipal = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  // For a null principal, we use our unique uuid as the base domain.
  return mURI->GetPath(aBaseDomain);
}

/**
 * nsISerializable implementation
 */
NS_IMETHODIMP
nsNullPrincipal::Read(nsIObjectInputStream* aStream)
{
  // Note - nsNullPrincipal use NS_GENERIC_FACTORY_CONSTRUCTOR_INIT, which means
  // that the Init() method has already been invoked by the time we deserialize.
  // This is in contrast to nsPrincipal, which uses NS_GENERIC_FACTORY_CONSTRUCTOR,
  // in which case ::Read needs to invoke Init().
  return mOriginAttributes.Deserialize(aStream);
}

NS_IMETHODIMP
nsNullPrincipal::Write(nsIObjectOutputStream* aStream)
{
  OriginAttributesRef().Serialize(aStream);
  return NS_OK;
}

