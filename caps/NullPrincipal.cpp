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

#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsDocShell.h"
#include "NullPrincipal.h"
#include "DefaultURI.h"
#include "nsSimpleURI.h"
#include "nsMemory.h"
#include "nsIClassInfoImpl.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "ContentPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "pratom.h"
#include "nsIObjectInputStream.h"
#include "mozilla/GkRustUtils.h"

#include "json/json.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(NullPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_NULLPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(NullPrincipal, nsIPrincipal)
NS_IMPL_CI_INTERFACE_GETTER(NullPrincipal, nsIPrincipal)

NullPrincipal::NullPrincipal(nsIURI* aURI, const nsACString& aOriginNoSuffix,
                             const OriginAttributes& aOriginAttributes)
    : BasePrincipal(eNullPrincipal, aOriginNoSuffix, aOriginAttributes),
      mURI(aURI) {}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithInheritedAttributes(
    nsIPrincipal* aInheritFrom) {
  MOZ_ASSERT(aInheritFrom);
  return CreateInternal(Cast(aInheritFrom)->OriginAttributesRef(), false,
                        nullptr, aInheritFrom);
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
  return CreateInternal(aOriginAttributes, aIsFirstParty);
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::Create(
    const OriginAttributes& aOriginAttributes, nsIURI* aURI) {
  return CreateInternal(aOriginAttributes, false, aURI);
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithoutOriginAttributes() {
  return NullPrincipal::Create(OriginAttributes(), nullptr);
}

already_AddRefed<nsIURI> NullPrincipal::CreateURI(
    nsIPrincipal* aPrecursor, const nsID* aNullPrincipalID) {
  nsCOMPtr<nsIURIMutator> iMutator;
  if (StaticPrefs::network_url_useDefaultURI()) {
    iMutator = new mozilla::net::DefaultURI::Mutator();
  } else {
    iMutator = new mozilla::net::nsSimpleURI::Mutator();
  }

  nsID uuid = aNullPrincipalID ? *aNullPrincipalID : nsID::GenerateUUID();

  NS_MutateURI mutator(iMutator);
  mutator.SetSpec(NS_NULLPRINCIPAL_SCHEME ":"_ns +
                  nsDependentCString(nsIDToCString(uuid).get()));

  // If there's a precursor URI, encode it in the null principal URI's query.
  if (aPrecursor) {
    nsAutoCString precursorOrigin;
    switch (BasePrincipal::Cast(aPrecursor)->Kind()) {
      case eNullPrincipal:
        // If the precursor null principal has a precursor, inherit it.
        if (nsCOMPtr<nsIURI> nullPrecursorURI = aPrecursor->GetURI()) {
          MOZ_ALWAYS_SUCCEEDS(nullPrecursorURI->GetQuery(precursorOrigin));
        }
        break;
      case eContentPrincipal:
        MOZ_ALWAYS_SUCCEEDS(aPrecursor->GetOriginNoSuffix(precursorOrigin));
        break;

      // For now, we won't track expanded or system principal precursors. We may
      // want to track expanded principal precursors in the future, but it's
      // unlikely we'll want to track system principal precursors.
      case eExpandedPrincipal:
      case eSystemPrincipal:
        break;
    }
    if (!precursorOrigin.IsEmpty()) {
      mutator.SetQuery(precursorOrigin);
    }
  }

  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(mutator.Finalize(getter_AddRefs(uri)));
  return uri.forget();
}

already_AddRefed<NullPrincipal> NullPrincipal::CreateInternal(
    const OriginAttributes& aOriginAttributes, bool aIsFirstParty, nsIURI* aURI,
    nsIPrincipal* aPrecursor) {
  MOZ_ASSERT_IF(aPrecursor, !aURI);
  nsCOMPtr<nsIURI> uri = aURI;
  if (!uri) {
    uri = NullPrincipal::CreateURI(aPrecursor);
  }

  MOZ_RELEASE_ASSERT(uri->SchemeIs(NS_NULLPRINCIPAL_SCHEME));

  nsAutoCString originNoSuffix;
  DebugOnly<nsresult> rv = uri->GetSpec(originNoSuffix);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  OriginAttributes attrs(aOriginAttributes);
  if (aIsFirstParty) {
    // The FirstPartyDomain attribute will not include information about the
    // precursor origin.
    nsAutoCString path;
    rv = uri->GetFilePath(path);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // remove the '{}' characters from both ends.
    path.Mid(path, 1, path.Length() - 2);
    path.AppendLiteral(".mozilla");
    attrs.SetFirstPartyDomain(true, path);
  }

  RefPtr<NullPrincipal> nullPrin =
      new NullPrincipal(uri, originNoSuffix, attrs);
  return nullPrin.forget();
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
NullPrincipal::GetIsOriginPotentiallyTrustworthy(bool* aResult) {
  *aResult = false;
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
NullPrincipal::Deserializer::Read(nsIObjectInputStream* aStream) {
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

  mPrincipal = NullPrincipal::Create(attrs, uri);
  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult NullPrincipal::PopulateJSONObject(Json::Value& aObject) {
  nsAutoCString principalURI;
  nsresult rv = mURI->GetSpec(principalURI);
  NS_ENSURE_SUCCESS(rv, rv);
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

  return NullPrincipal::Create(attrs, uri);
}

NS_IMETHODIMP
NullPrincipal::GetPrecursorPrincipal(nsIPrincipal** aPrincipal) {
  *aPrincipal = nullptr;

  nsAutoCString query;
  if (NS_FAILED(mURI->GetQuery(query)) || query.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> precursorURI;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(precursorURI), query))) {
    MOZ_ASSERT_UNREACHABLE(
        "Failed to parse precursor from nullprincipal query");
    return NS_OK;
  }

  // If our precursor is another null principal, re-construct it. This can
  // happen if a null principal without a precursor causes another principal to
  // be created.
  if (precursorURI->SchemeIs(NS_NULLPRINCIPAL_SCHEME)) {
#ifdef DEBUG
    nsAutoCString precursorQuery;
    precursorURI->GetQuery(precursorQuery);
    MOZ_ASSERT(precursorQuery.IsEmpty(),
               "Null principal with nested precursors?");
#endif
    *aPrincipal =
        NullPrincipal::Create(OriginAttributesRef(), precursorURI).take();
    return NS_OK;
  }

  RefPtr<BasePrincipal> contentPrincipal =
      BasePrincipal::CreateContentPrincipal(precursorURI,
                                            OriginAttributesRef());
  // If `CreateContentPrincipal` failed, it will create a new NullPrincipal and
  // return that instead. We only want to return real content principals here.
  if (!contentPrincipal || !contentPrincipal->Is<ContentPrincipal>()) {
    return NS_OK;
  }
  contentPrincipal.forget(aPrincipal);
  return NS_OK;
}
