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
#include "nsIClassInfoImpl.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "nsEscape.h"
#include "ContentPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "pratom.h"
#include "nsIObjectInputStream.h"

#include "js/JSON.h"
#include "NullPrincipalJSONHandler.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(NullPrincipal, nullptr, 0, NS_NULLPRINCIPAL_CID)
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
  nsCOMPtr<nsIURI> uri = CreateURI(aInheritFrom);
  return Create(Cast(aInheritFrom)->OriginAttributesRef(), uri);
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::Create(
    const OriginAttributes& aOriginAttributes, nsIURI* aNullPrincipalURI) {
  nsCOMPtr<nsIURI> uri = aNullPrincipalURI;
  if (!uri) {
    uri = NullPrincipal::CreateURI(nullptr);
  }

  MOZ_RELEASE_ASSERT(uri->SchemeIs(NS_NULLPRINCIPAL_SCHEME));

  nsAutoCString originNoSuffix;
  DebugOnly<nsresult> rv = uri->GetSpec(originNoSuffix);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  RefPtr<NullPrincipal> nullPrin =
      new NullPrincipal(uri, originNoSuffix, aOriginAttributes);
  return nullPrin.forget();
}

/* static */
already_AddRefed<NullPrincipal> NullPrincipal::CreateWithoutOriginAttributes() {
  return NullPrincipal::Create(OriginAttributes(), nullptr);
}

void NullPrincipal::EscapePrecursorQuery(nsACString& aPrecursorQuery) {
  // origins should not contain existing escape sequences, so set `esc_Forced`
  // to force any `%` in the input to be escaped in addition to non-ascii,
  // control characters and DEL.
  nsCString modified;
  if (NS_EscapeURLSpan(aPrecursorQuery, esc_Query | esc_Forced, modified)) {
    aPrecursorQuery.Assign(std::move(modified));
  }
}

void NullPrincipal::UnescapePrecursorQuery(nsACString& aPrecursorQuery) {
  nsCString modified;
  if (NS_UnescapeURL(aPrecursorQuery.BeginReading(), aPrecursorQuery.Length(),
                     /* aFlags */ 0, modified)) {
    aPrecursorQuery.Assign(std::move(modified));
  }
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
      case eNullPrincipal: {
        // If the precursor null principal has a precursor, inherit it.
        if (nsCOMPtr<nsIURI> nullPrecursorURI = aPrecursor->GetURI()) {
          MOZ_ALWAYS_SUCCEEDS(nullPrecursorURI->GetQuery(precursorOrigin));
        }
        break;
      }
      case eContentPrincipal: {
        MOZ_ALWAYS_SUCCEEDS(aPrecursor->GetOriginNoSuffix(precursorOrigin));
#ifdef DEBUG
        nsAutoCString original(precursorOrigin);
#endif
        EscapePrecursorQuery(precursorOrigin);
#ifdef DEBUG
        nsAutoCString unescaped(precursorOrigin);
        UnescapePrecursorQuery(unescaped);
        MOZ_ASSERT(unescaped == original,
                   "cannot recover original precursor origin after escape");
#endif
        break;
      }

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

nsresult NullPrincipal::WriteJSONInnerProperties(JSONWriter& aWriter) {
  nsAutoCString principalURI;
  nsresult rv = mURI->GetSpec(principalURI);
  NS_ENSURE_SUCCESS(rv, rv);
  WriteJSONProperty<eSpec>(aWriter, principalURI);

  nsAutoCString suffix;
  OriginAttributesRef().CreateSuffix(suffix);
  if (suffix.Length() > 0) {
    WriteJSONProperty<eSuffix>(aWriter, suffix);
  }

  return NS_OK;
}

NS_IMETHODIMP
NullPrincipal::GetPrecursorPrincipal(nsIPrincipal** aPrincipal) {
  *aPrincipal = nullptr;

  nsAutoCString query;
  if (NS_FAILED(mURI->GetQuery(query)) || query.IsEmpty()) {
    return NS_OK;
  }
  UnescapePrecursorQuery(query);

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

bool NullPrincipalJSONHandler::startObject() {
  switch (mState) {
    case State::Init:
      mState = State::StartObject;
      break;
    default:
      NS_WARNING("Unexpected object value");
      mState = State::Error;
      return false;
  }

  return true;
}

bool NullPrincipalJSONHandler::propertyName(const JS::Latin1Char* name,
                                            size_t length) {
  switch (mState) {
    case State::StartObject:
    case State::AfterPropertyValue: {
      if (length != 1) {
        NS_WARNING(
            nsPrintfCString("Unexpected property name length: %zu", length)
                .get());
        mState = State::Error;
        return false;
      }

      char key = char(name[0]);
      switch (key) {
        case NullPrincipal::SpecKey:
          mState = State::SpecKey;
          break;
        case NullPrincipal::SuffixKey:
          mState = State::SuffixKey;
          break;
        default:
          NS_WARNING(
              nsPrintfCString("Unexpected property name: '%c'", key).get());
          mState = State::Error;
          return false;
      }
      break;
    }
    default:
      NS_WARNING("Unexpected property name");
      mState = State::Error;
      return false;
  }

  return true;
}

bool NullPrincipalJSONHandler::endObject() {
  switch (mState) {
    case State::AfterPropertyValue:
      MOZ_ASSERT(mUri);

      mPrincipal = NullPrincipal::Create(mAttrs, mUri);
      MOZ_ASSERT(mPrincipal);

      mState = State::EndObject;
      break;
    default:
      NS_WARNING("Unexpected end of object");
      mState = State::Error;
      return false;
  }

  return true;
}

bool NullPrincipalJSONHandler::stringValue(const JS::Latin1Char* str,
                                           size_t length) {
  switch (mState) {
    case State::SpecKey: {
      nsDependentCSubstring spec(reinterpret_cast<const char*>(str), length);
      nsresult rv = NS_NewURI(getter_AddRefs(mUri), spec);
      if (NS_FAILED(rv)) {
        mState = State::Error;
        return false;
      }

      mState = State::AfterPropertyValue;
      break;
    }
    case State::SuffixKey: {
      nsDependentCSubstring attrs(reinterpret_cast<const char*>(str), length);
      if (!mAttrs.PopulateFromSuffix(attrs)) {
        mState = State::Error;
        return false;
      }

      mState = State::AfterPropertyValue;
      break;
    }
    default:
      NS_WARNING("Unexpected string value");
      mState = State::Error;
      return false;
  }

  return true;
}
