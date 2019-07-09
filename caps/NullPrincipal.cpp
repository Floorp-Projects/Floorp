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
#include "nsIClassInfoImpl.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "nsIScriptSecurityManager.h"
#include "ContentPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "pratom.h"

#include "json/json.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(NullPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_NULLPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(NullPrincipal, nsIPrincipal, nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(NullPrincipal, nsIPrincipal, nsISerializable)

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithInheritedAttributes(
    nsIPrincipal* aInheritFrom) {
  MOZ_ASSERT(aInheritFrom);
  return CreateWithInheritedAttributes(
      Cast(aInheritFrom)->OriginAttributesRef(), false);
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithInheritedAttributes(
    nsIDocShell* aDocShell, bool aIsFirstParty) {
  MOZ_ASSERT(aDocShell);

  OriginAttributes attrs = nsDocShell::Cast(aDocShell)->GetOriginAttributes();
  return CreateWithInheritedAttributes(attrs, aIsFirstParty);
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithInheritedAttributes(
    const OriginAttributes& aOriginAttributes, bool aIsFirstParty) {
  RefPtr<NullPrincipal> nullPrin = new NullPrincipal();
  nsresult rv = nullPrin->Init(aOriginAttributes, aIsFirstParty);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return nullPrin.forget();
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::Create(
    const OriginAttributes& aOriginAttributes, nsIURI* aURI) {
  RefPtr<NullPrincipal> nullPrin = new NullPrincipal();
  nsresult rv = nullPrin->Init(aOriginAttributes, aURI);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  return nullPrin.forget();
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithoutOriginAttributes() {
  return NullPrincipal::Create(OriginAttributes(), nullptr);
}

nsresult NullPrincipal::Init(const OriginAttributes& aOriginAttributes,
                             nsIURI* aURI) {
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

nsresult NullPrincipal::Init(const OriginAttributes& aOriginAttributes,
                             bool aIsFirstParty) {
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

nsresult NullPrincipal::GetScriptLocation(nsACString& aStr) {
  return mURI->GetSpec(aStr);
}

/**
 * nsIPrincipal implementation
 */

uint32_t NullPrincipal::GetHashValue() { return (NS_PTR_TO_INT32(this) >> 2); }

NS_IMETHODIMP
NullPrincipal::GetURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> uri = mURI;
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::GetDomain(nsIURI** aDomain) {
  nsCOMPtr<nsIURI> uri = mURI;
  uri.forget(aDomain);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::SetDomain(nsIURI* aDomain) {
  // I think the right thing to do here is to just throw...  Silently failing
  // seems counterproductive.
  return NS_ERROR_NOT_AVAILABLE;
}

bool NullPrincipal::MayLoadInternal(nsIURI* aURI) {
  // Also allow the load if we are the principal of the URI being checked.
  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          aURI, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    return SubsumesInternal(blobPrincipal,
                            BasePrincipal::ConsiderDocumentDomain);
  }

  return false;
}

NS_IMETHODIMP
NullPrincipal::GetBaseDomain(nsACString& aBaseDomain) {
  // For a null principal, we use our unique uuid as the base domain.
  return mURI->GetPathQueryRef(aBaseDomain);
}

NS_IMETHODIMP
NullPrincipal::GetAddonId(nsAString& aAddonId) {
  aAddonId.Truncate();
  return NS_OK;
};

/**
 * nsISerializable implementation
 */
NS_IMETHODIMP
NullPrincipal::Read(nsIObjectInputStream* aStream) {
  // Note - NullPrincipal use NS_GENERIC_FACTORY_CONSTRUCTOR_INIT, which means
  // that the Init() method has already been invoked by the time we deserialize.
  // This is in contrast to ContentPrincipal, which uses
  // NS_GENERIC_FACTORY_CONSTRUCTOR, in which case ::Read needs to invoke
  // Init().

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
NullPrincipal::Write(nsIObjectOutputStream* aStream) {
  // Read is used still for legacy principals
  MOZ_RELEASE_ASSERT(false, "Old style serialization is removed");
  return NS_OK;
}

nsresult NullPrincipal::PopulateJSONObject(Json::Value& aObject) {
  nsAutoCString principalURI;
  nsresult rv = mURI->GetSpec(principalURI);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(principalURI.Length() ==
                 NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME ":").Length() +
                     NSID_LENGTH - 1,
             "Length of the URI should be: (scheme, uuid, - nullptr)");
  aObject[std::to_string(eSpec)] = principalURI.get();

  nsAutoCString suffix;
  OriginAttributesRef().CreateSuffix(suffix);
  if (suffix.Length() > 0) {
    aObject[std::to_string(eSuffix)] = suffix.get();
  }

  return NS_OK;
}

already_AddRefed<BasePrincipal> NullPrincipal::FromProperties(
    nsTArray<NullPrincipal::KeyVal>& aFields) {
  MOZ_ASSERT(aFields.Length() == eMax + 1, "Must have all the keys");
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  OriginAttributes attrs;

  // The odd structure here is to make the code to not compile
  // if all the switch enum cases haven't been codified
  for (const auto& field : aFields) {
    switch (field.key) {
      case NullPrincipal::eSpec:
        if (!field.valueWasSerialized) {
          MOZ_ASSERT(false,
                     "Null principals require a spec URI in serialized JSON");
          return nullptr;
        }
        rv = NS_NewURI(getter_AddRefs(uri), field.value);
        NS_ENSURE_SUCCESS(rv, nullptr);
        break;
      case NullPrincipal::eSuffix:
        bool ok = attrs.PopulateFromSuffix(field.value);
        if (!ok) {
          return nullptr;
        }
        break;
    }
  }

  if (!uri) {
    MOZ_ASSERT(false, "No URI deserialized");
    return nullptr;
  }

  RefPtr<NullPrincipal> nullPrincipal = new NullPrincipal();
  rv = nullPrincipal->Init(attrs, uri);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return nullPrincipal.forget();
}
