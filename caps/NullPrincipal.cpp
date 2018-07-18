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
#include "NullPrincipal.h"
#include "NullPrincipalURI.h"
#include "nsMemory.h"
#include "nsIURIWithPrincipal.h"
#include "nsIClassInfoImpl.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "nsIScriptSecurityManager.h"
#include "ContentPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "pratom.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(NullPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_NULLPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(NullPrincipal,
                           nsIPrincipal,
                           nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(NullPrincipal,
                            nsIPrincipal,
                            nsISerializable)

/* static */ already_AddRefed<NullPrincipal>
NullPrincipal::CreateWithInheritedAttributes(nsIPrincipal* aInheritFrom)
{
  RefPtr<NullPrincipal> nullPrin = new NullPrincipal();
  nsresult rv = nullPrin->Init(Cast(aInheritFrom)->OriginAttributesRef());
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return nullPrin.forget();
}

/* static */ already_AddRefed<NullPrincipal>
NullPrincipal::CreateWithInheritedAttributes(nsIDocShell* aDocShell, bool aIsFirstParty)
{
  OriginAttributes attrs = nsDocShell::Cast(aDocShell)->GetOriginAttributes();

  RefPtr<NullPrincipal> nullPrin = new NullPrincipal();
  nsresult rv = nullPrin->Init(attrs, aIsFirstParty);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return nullPrin.forget();
}

/* static */ already_AddRefed<NullPrincipal>
NullPrincipal::Create(const OriginAttributes& aOriginAttributes, nsIURI* aURI)
{
  RefPtr<NullPrincipal> nullPrin = new NullPrincipal();
  nsresult rv = nullPrin->Init(aOriginAttributes, aURI);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  return nullPrin.forget();
}

/* static */ already_AddRefed<NullPrincipal>
NullPrincipal::CreateWithoutOriginAttributes()
{
  return NullPrincipal::Create(OriginAttributes(), nullptr);
}

nsresult
NullPrincipal::Init(const OriginAttributes& aOriginAttributes, nsIURI* aURI)
{
  if (aURI) {
    nsAutoCString scheme;
    nsresult rv = aURI->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(scheme.EqualsLiteral(NS_NULLPRINCIPAL_SCHEME),
                   NS_ERROR_NOT_AVAILABLE);

    mURI = aURI;
  } else {
    mURI = NullPrincipalURI::Create();
    NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_AVAILABLE);
  }

  nsAutoCString originNoSuffix;
  DebugOnly<nsresult> rv = mURI->GetSpec(originNoSuffix);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  FinishInit(originNoSuffix, aOriginAttributes);

  return NS_OK;
}

nsresult
NullPrincipal::Init(const OriginAttributes& aOriginAttributes, bool aIsFirstParty)
{
  mURI = NullPrincipalURI::Create();
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_AVAILABLE);

  nsAutoCString originNoSuffix;
  DebugOnly<nsresult> rv = mURI->GetSpec(originNoSuffix);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsAutoCString path;
  rv = mURI->GetPathQueryRef(path);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  OriginAttributes attrs(aOriginAttributes);
  if (aIsFirstParty) {
    // remove the '{}' characters from both ends.
    path.Mid(path, 1, path.Length() - 2);
    path.AppendLiteral(".mozilla");
    attrs.SetFirstPartyDomain(true, path);
  }

  FinishInit(originNoSuffix, attrs);

  return NS_OK;
}

nsresult
NullPrincipal::GetScriptLocation(nsACString &aStr)
{
  return mURI->GetSpec(aStr);
}

/**
 * nsIPrincipal implementation
 */

NS_IMETHODIMP
NullPrincipal::GetHashValue(uint32_t *aResult)
{
  *aResult = (NS_PTR_TO_INT32(this) >> 2);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // Never destroy an existing CSP on the principal.
  // This method should only be called in rare cases.

  MOZ_ASSERT(!mCSP, "do not destroy an existing CSP");
  if (mCSP) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mCSP = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::GetURI(nsIURI** aURI)
{
  nsCOMPtr<nsIURI> uri = mURI;
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::GetDomain(nsIURI** aDomain)
{
  nsCOMPtr<nsIURI> uri = mURI;
  uri.forget(aDomain);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::SetDomain(nsIURI* aDomain)
{
  // I think the right thing to do here is to just throw...  Silently failing
  // seems counterproductive.
  return NS_ERROR_NOT_AVAILABLE;
}

bool
NullPrincipal::MayLoadInternal(nsIURI* aURI)
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
NullPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  // For a null principal, we use our unique uuid as the base domain.
  return mURI->GetPathQueryRef(aBaseDomain);
}

NS_IMETHODIMP
NullPrincipal::GetAddonId(nsAString& aAddonId)
{
  aAddonId.Truncate();
  return NS_OK;
};

/**
 * nsISerializable implementation
 */
NS_IMETHODIMP
NullPrincipal::Read(nsIObjectInputStream* aStream)
{
  // Note - NullPrincipal use NS_GENERIC_FACTORY_CONSTRUCTOR_INIT, which means
  // that the Init() method has already been invoked by the time we deserialize.
  // This is in contrast to ContentPrincipal, which uses NS_GENERIC_FACTORY_CONSTRUCTOR,
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
NullPrincipal::Write(nsIObjectOutputStream* aStream)
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
