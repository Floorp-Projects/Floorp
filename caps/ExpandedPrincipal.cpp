/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExpandedPrincipal.h"
#include "nsIClassInfoImpl.h"
#include "nsIObjectInputStream.h"
#include "nsReadableUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozilla/JSONWriter.h"

#include "js/JSON.h"
#include "ExpandedPrincipalJSONHandler.h"
#include "SubsumedPrincipalJSONHandler.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(ExpandedPrincipal, nullptr, 0, NS_EXPANDEDPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(ExpandedPrincipal, nsIPrincipal,
                           nsIExpandedPrincipal)
NS_IMPL_CI_INTERFACE_GETTER(ExpandedPrincipal, nsIPrincipal,
                            nsIExpandedPrincipal)

ExpandedPrincipal::ExpandedPrincipal(
    nsTArray<nsCOMPtr<nsIPrincipal>>&& aPrincipals,
    const nsACString& aOriginNoSuffix, const OriginAttributes& aAttrs)
    : BasePrincipal(eExpandedPrincipal, aOriginNoSuffix, aAttrs),
      mPrincipals(std::move(aPrincipals)) {}

ExpandedPrincipal::~ExpandedPrincipal() = default;

already_AddRefed<ExpandedPrincipal> ExpandedPrincipal::Create(
    const nsTArray<nsCOMPtr<nsIPrincipal>>& aAllowList,
    const OriginAttributes& aAttrs) {
  nsTArray<nsCOMPtr<nsIPrincipal>> principals;
  for (size_t i = 0; i < aAllowList.Length(); ++i) {
    principals.AppendElement(aAllowList[i]);
  }

  nsAutoCString origin;
  origin.AssignLiteral("[Expanded Principal [");
  StringJoinAppend(
      origin, ", "_ns, principals,
      [](nsACString& dest, const nsCOMPtr<nsIPrincipal>& principal) {
        nsAutoCString subOrigin;
        DebugOnly<nsresult> rv = principal->GetOrigin(subOrigin);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        dest.Append(subOrigin);
      });
  origin.AppendLiteral("]]");

  RefPtr<ExpandedPrincipal> ep =
      new ExpandedPrincipal(std::move(principals), origin, aAttrs);
  return ep.forget();
}

NS_IMETHODIMP
ExpandedPrincipal::GetDomain(nsIURI** aDomain) {
  *aDomain = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExpandedPrincipal::SetDomain(nsIURI* aDomain) { return NS_OK; }

bool ExpandedPrincipal::SubsumesInternal(
    nsIPrincipal* aOther,
    BasePrincipal::DocumentDomainConsideration aConsideration) {
  // If aOther is an ExpandedPrincipal too, we break it down into its component
  // nsIPrincipals, and check subsumes on each one.
  if (Cast(aOther)->Is<ExpandedPrincipal>()) {
    auto* expanded = Cast(aOther)->As<ExpandedPrincipal>();

    for (auto& other : expanded->AllowList()) {
      // Use SubsumesInternal rather than Subsumes here, since OriginAttribute
      // checks are only done between non-expanded sub-principals, and we don't
      // need to incur the extra virtual call overhead.
      if (!SubsumesInternal(other, aConsideration)) {
        return false;
      }
    }
    return true;
  }

  // We're dealing with a regular principal. One of our principals must subsume
  // it.
  for (uint32_t i = 0; i < mPrincipals.Length(); ++i) {
    if (Cast(mPrincipals[i])->Subsumes(aOther, aConsideration)) {
      return true;
    }
  }

  return false;
}

bool ExpandedPrincipal::MayLoadInternal(nsIURI* uri) {
  for (uint32_t i = 0; i < mPrincipals.Length(); ++i) {
    if (BasePrincipal::Cast(mPrincipals[i])->MayLoadInternal(uri)) {
      return true;
    }
  }

  return false;
}

uint32_t ExpandedPrincipal::GetHashValue() {
  MOZ_CRASH("extended principal should never be used as key in a hash map");
}

NS_IMETHODIMP
ExpandedPrincipal::GetURI(nsIURI** aURI) {
  *aURI = nullptr;
  return NS_OK;
}

const nsTArray<nsCOMPtr<nsIPrincipal>>& ExpandedPrincipal::AllowList() {
  return mPrincipals;
}

NS_IMETHODIMP
ExpandedPrincipal::GetBaseDomain(nsACString& aBaseDomain) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ExpandedPrincipal::GetAddonId(nsAString& aAddonId) {
  aAddonId.Truncate();
  return NS_OK;
};

bool ExpandedPrincipal::AddonHasPermission(const nsAtom* aPerm) {
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (BasePrincipal::Cast(mPrincipals[i])->AddonHasPermission(aPerm)) {
      return true;
    }
  }
  return false;
}

bool ExpandedPrincipal::AddonAllowsLoad(nsIURI* aURI,
                                        bool aExplicit /* = false */) {
  for (const auto& principal : mPrincipals) {
    if (Cast(principal)->AddonAllowsLoad(aURI, aExplicit)) {
      return true;
    }
  }
  return false;
}

void ExpandedPrincipal::SetCsp(nsIContentSecurityPolicy* aCSP) {
  AssertIsOnMainThread();
  mCSP = new nsMainThreadPtrHolder<nsIContentSecurityPolicy>(
      "ExpandedPrincipal::mCSP", aCSP);
}

NS_IMETHODIMP
ExpandedPrincipal::GetCsp(nsIContentSecurityPolicy** aCsp) {
  AssertIsOnMainThread();
  NS_IF_ADDREF(*aCsp = mCSP);
  return NS_OK;
}

nsIPrincipal* ExpandedPrincipal::PrincipalToInherit(nsIURI* aRequestedURI) {
  if (aRequestedURI) {
    // If a given sub-principal subsumes the given URI, use that principal for
    // inheritance. In general, this only happens with certain CORS modes, loads
    // with forced principal inheritance, and creation of XML documents from
    // XMLHttpRequests or fetch requests. For URIs that normally inherit a
    // principal (such as data: URIs), we fall back to the last principal in the
    // allowlist.
    for (const auto& principal : mPrincipals) {
      if (Cast(principal)->MayLoadInternal(aRequestedURI)) {
        return principal;
      }
    }
  }
  return mPrincipals.LastElement();
}

nsresult ExpandedPrincipal::GetScriptLocation(nsACString& aStr) {
  aStr.AssignLiteral("[Expanded Principal [");
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (i != 0) {
      aStr.AppendLiteral(", ");
    }

    nsAutoCString spec;
    nsresult rv =
        nsJSPrincipals::get(mPrincipals.ElementAt(i))->GetScriptLocation(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    aStr.Append(spec);
  }
  aStr.AppendLiteral("]]");
  return NS_OK;
}

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

// We've had way too many issues with unversioned serializations, so
// explicitly version this one.
static const uint32_t kSerializationVersion = 1;

NS_IMETHODIMP
ExpandedPrincipal::Deserializer::Read(nsIObjectInputStream* aStream) {
  uint32_t version;
  nsresult rv = aStream->Read32(&version);
  if (version != kSerializationVersion) {
    MOZ_ASSERT(false,
               "We really need to add handling of the old(?) version here");
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t count;
  rv = aStream->Read32(&count);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsTArray<nsCOMPtr<nsIPrincipal>> principals;
  if (!principals.SetCapacity(count, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsISupports> read;
    rv = aStream->ReadObject(true, getter_AddRefs(read));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(read);
    if (!principal) {
      return NS_ERROR_UNEXPECTED;
    }

    principals.AppendElement(std::move(principal));
  }

  mPrincipal = ExpandedPrincipal::Create(principals, OriginAttributes());
  return NS_OK;
}

nsresult ExpandedPrincipal::GetSiteIdentifier(SiteIdentifier& aSite) {
  // Call GetSiteIdentifier on each of our principals and return a new
  // ExpandedPrincipal.

  nsTArray<nsCOMPtr<nsIPrincipal>> allowlist;
  for (const auto& principal : mPrincipals) {
    SiteIdentifier site;
    nsresult rv = Cast(principal)->GetSiteIdentifier(site);
    NS_ENSURE_SUCCESS(rv, rv);
    allowlist.AppendElement(site.GetPrincipal());
  }

  RefPtr<ExpandedPrincipal> expandedPrincipal =
      ExpandedPrincipal::Create(allowlist, OriginAttributesRef());
  MOZ_ASSERT(expandedPrincipal, "ExpandedPrincipal::Create returned nullptr?");

  aSite.Init(expandedPrincipal);
  return NS_OK;
}

nsresult ExpandedPrincipal::WriteJSONInnerProperties(JSONWriter& aWriter) {
  aWriter.StartArrayProperty(JSONEnumKeyString<eSpecs>(),
                             JSONWriter::CollectionStyle::SingleLineStyle);

  for (const auto& principal : mPrincipals) {
    aWriter.StartObjectElement(JSONWriter::CollectionStyle::SingleLineStyle);

    nsresult rv = BasePrincipal::Cast(principal)->WriteJSONProperties(aWriter);
    NS_ENSURE_SUCCESS(rv, rv);

    aWriter.EndObject();
  }

  aWriter.EndArray();

  nsAutoCString suffix;
  OriginAttributesRef().CreateSuffix(suffix);
  if (suffix.Length() > 0) {
    WriteJSONProperty<eSuffix>(aWriter, suffix);
  }

  return NS_OK;
}

bool ExpandedPrincipalJSONHandler::ProcessSubsumedResult(bool aResult) {
  if (!aResult) {
    NS_WARNING("Failed to parse subsumed principal");
    mState = State::Error;
    return false;
  }
  return true;
}

bool ExpandedPrincipalJSONHandler::startObject() {
  if (mSubsumedHandler.isSome()) {
    return ProcessSubsumedResult(mSubsumedHandler->startObject());
  }

  switch (mState) {
    case State::Init:
      mState = State::StartObject;
      break;
    case State::StartArray:
      mState = State::SubsumedPrincipal;
      [[fallthrough]];
    case State::SubsumedPrincipal:
      mSubsumedHandler.emplace();

      return ProcessSubsumedResult(mSubsumedHandler->startObject());
    default:
      NS_WARNING("Unexpected object value");
      mState = State::Error;
      return false;
  }

  return true;
}

bool ExpandedPrincipalJSONHandler::propertyName(const JS::Latin1Char* name,
                                                size_t length) {
  if (mSubsumedHandler.isSome()) {
    return ProcessSubsumedResult(mSubsumedHandler->propertyName(name, length));
  }

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
        case ExpandedPrincipal::SpecsKey:
          mState = State::SpecsKey;
          break;
        case ExpandedPrincipal::SuffixKey:
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

bool ExpandedPrincipalJSONHandler::endObject() {
  if (mSubsumedHandler.isSome()) {
    if (!ProcessSubsumedResult(mSubsumedHandler->endObject())) {
      return false;
    }
    if (mSubsumedHandler->HasAccepted()) {
      nsCOMPtr<nsIPrincipal> principal = mSubsumedHandler->mPrincipal.forget();
      mSubsumedHandler.reset();
      mAllowList.AppendElement(principal);
    }
    return true;
  }

  switch (mState) {
    case State::AfterPropertyValue:
      mPrincipal = ExpandedPrincipal::Create(mAllowList, mAttrs);
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

bool ExpandedPrincipalJSONHandler::startArray() {
  switch (mState) {
    case State::SpecsKey:
      mState = State::StartArray;
      break;
    default:
      NS_WARNING("Unexpected array value");
      mState = State::Error;
      return false;
  }

  return true;
}

bool ExpandedPrincipalJSONHandler::endArray() {
  switch (mState) {
    case State::SubsumedPrincipal: {
      mState = State::AfterPropertyValue;
      break;
    }
    default:
      NS_WARNING("Unexpected end of array");
      mState = State::Error;
      return false;
  }

  return true;
}

bool ExpandedPrincipalJSONHandler::stringValue(const JS::Latin1Char* str,
                                               size_t length) {
  if (mSubsumedHandler.isSome()) {
    return ProcessSubsumedResult(mSubsumedHandler->stringValue(str, length));
  }

  switch (mState) {
    case State::SpecsKey: {
      nsDependentCSubstring specs(reinterpret_cast<const char*>(str), length);

      for (const nsACString& each : specs.Split(',')) {
        nsAutoCString result;
        nsresult rv = Base64Decode(each, result);
        MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to decode");
        if (NS_FAILED(rv)) {
          mState = State::Error;
          return false;
        }

        nsCOMPtr<nsIPrincipal> principal = BasePrincipal::FromJSON(result);
        if (!principal) {
          mState = State::Error;
          return false;
        }
        mAllowList.AppendElement(principal);
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

NS_IMETHODIMP
ExpandedPrincipal::IsThirdPartyURI(nsIURI* aURI, bool* aRes) {
  // ExpandedPrincipal for extension content scripts consist of two principals,
  // the document's principal and the extension's principal.
  // To make sure that the third-party check behaves like the web page on which
  // the content script is running, ignore the extension's principal.

  for (const auto& principal : mPrincipals) {
    if (!Cast(principal)->AddonPolicyCore()) {
      return Cast(principal)->IsThirdPartyURI(aURI, aRes);
    }
  }

  if (mPrincipals.IsEmpty()) {
    *aRes = true;
    return NS_OK;
  }

  return Cast(mPrincipals[0])->IsThirdPartyURI(aURI, aRes);
}
