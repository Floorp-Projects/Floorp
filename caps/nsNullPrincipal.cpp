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

#include "nsDocShell.h"
#include "nsNullPrincipal.h"
#include "nsNullPrincipalURI.h"
#include "nsMemory.h"
#include "nsIURIWithPrincipal.h"
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
  RefPtr<nsNullPrincipal> nullPrin = new nsNullPrincipal();
  nsresult rv = nullPrin->Init(Cast(aInheritFrom)->OriginAttributesRef());
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return nullPrin.forget();
}

/* static */ already_AddRefed<nsNullPrincipal>
nsNullPrincipal::CreateWithInheritedAttributes(nsIDocShell* aDocShell)
{
  RefPtr<nsNullPrincipal> nullPrin = new nsNullPrincipal();
  nsresult rv =
    nullPrin->Init(nsDocShell::Cast(aDocShell)->GetOriginAttributes());
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return nullPrin.forget();
}

/* static */ already_AddRefed<nsNullPrincipal>
nsNullPrincipal::Create(const OriginAttributes& aOriginAttributes, nsIURI* aURI)
{
  RefPtr<nsNullPrincipal> nullPrin = new nsNullPrincipal();
  nsresult rv = nullPrin->Init(aOriginAttributes, aURI);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  return nullPrin.forget();
}

nsresult
nsNullPrincipal::Init(const OriginAttributes& aOriginAttributes, nsIURI* aURI)
{
  mOriginAttributes = aOriginAttributes;

  if (aURI) {
    nsAutoCString scheme;
    nsresult rv = aURI->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(scheme.EqualsLiteral(NS_NULLPRINCIPAL_SCHEME),
                   NS_ERROR_NOT_AVAILABLE);

    mURI = aURI;
  } else {
    mURI = nsNullPrincipalURI::Create();
    NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_AVAILABLE);
  }

  FinishInit();

  return NS_OK;
}

nsresult
nsNullPrincipal::GetScriptLocation(nsACString &aStr)
{
  return mURI->GetSpec(aStr);
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

nsresult
nsNullPrincipal::GetOriginInternal(nsACString& aOrigin)
{
  return mURI->GetSpec(aOrigin);
}

bool
nsNullPrincipal::MayLoadInternal(nsIURI* aURI)
{
  // Also allow the load if we are the principal of the URI being checked.
  nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(aURI);
  if (uriPrinc) {
    nsCOMPtr<nsIPrincipal> principal;
    uriPrinc->GetPrincipal(getter_AddRefs(principal));

    if (principal == this) {
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
nsNullPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  // For a null principal, we use our unique uuid as the base domain.
  return mURI->GetPath(aBaseDomain);
}

NS_IMETHODIMP
nsNullPrincipal::GetAddonId(nsAString& aAddonId)
{
  aAddonId.Truncate();
  return NS_OK;
};

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

  nsAutoCString spec;
  nsresult rv = aStream->ReadCString(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  rv = aStream->ReadCString(suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  bool ok = attrs.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  return Init(attrs, uri);
}

NS_IMETHODIMP
nsNullPrincipal::Write(nsIObjectOutputStream* aStream)
{
  NS_ENSURE_STATE(mURI);

  nsAutoCString spec;
  nsresult rv = mURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteStringZ(spec.get());
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  OriginAttributesRef().CreateSuffix(suffix);

  rv = aStream->WriteStringZ(suffix.get());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
