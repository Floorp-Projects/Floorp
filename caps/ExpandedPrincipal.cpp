/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExpandedPrincipal.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/Base64.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(ExpandedPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_EXPANDEDPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(ExpandedPrincipal, nsIPrincipal,
                           nsIExpandedPrincipal, nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(ExpandedPrincipal, nsIPrincipal,
                            nsIExpandedPrincipal, nsISerializable)

struct OriginComparator {
  bool LessThan(nsIPrincipal* a, nsIPrincipal* b) const {
    nsAutoCString originA;
    DebugOnly<nsresult> rv = a->GetOrigin(originA);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    nsAutoCString originB;
    rv = b->GetOrigin(originB);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return originA < originB;
  }

  bool Equals(nsIPrincipal* a, nsIPrincipal* b) const {
    nsAutoCString originA;
    DebugOnly<nsresult> rv = a->GetOrigin(originA);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    nsAutoCString originB;
    rv = b->GetOrigin(originB);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return a == b;
  }
};

ExpandedPrincipal::ExpandedPrincipal(
    nsTArray<nsCOMPtr<nsIPrincipal>>& aAllowList)
    : BasePrincipal(eExpandedPrincipal) {
  // We force the principals to be sorted by origin so that ExpandedPrincipal
  // origins can have a canonical form.
  OriginComparator c;
  for (size_t i = 0; i < aAllowList.Length(); ++i) {
    mPrincipals.InsertElementSorted(aAllowList[i], c);
  }
}

ExpandedPrincipal::ExpandedPrincipal() : BasePrincipal(eExpandedPrincipal) {}

ExpandedPrincipal::~ExpandedPrincipal() {}

already_AddRefed<ExpandedPrincipal> ExpandedPrincipal::Create(
    nsTArray<nsCOMPtr<nsIPrincipal>>& aAllowList,
    const OriginAttributes& aAttrs) {
  RefPtr<ExpandedPrincipal> ep = new ExpandedPrincipal(aAllowList);

  nsAutoCString origin;
  origin.AssignLiteral("[Expanded Principal [");
  for (size_t i = 0; i < ep->mPrincipals.Length(); ++i) {
    if (i != 0) {
      origin.AppendLiteral(", ");
    }

    nsAutoCString subOrigin;
    DebugOnly<nsresult> rv = ep->mPrincipals.ElementAt(i)->GetOrigin(subOrigin);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    origin.Append(subOrigin);
  }
  origin.AppendLiteral("]]");

  ep->FinishInit(origin, aAttrs);
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

void ExpandedPrincipal::SetCsp(nsIContentSecurityPolicy* aCSP) { mCSP = aCSP; }

NS_IMETHODIMP
ExpandedPrincipal::GetCsp(nsIContentSecurityPolicy** aCsp) {
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
ExpandedPrincipal::Read(nsIObjectInputStream* aStream) {
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

  if (!mPrincipals.SetCapacity(count, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  OriginComparator c;
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

    // Play it safe and InsertElementSorted, in case the sort order
    // changed for some bizarre reason.
    mPrincipals.InsertElementSorted(std::move(principal), c);
  }

  return NS_OK;
}

NS_IMETHODIMP
ExpandedPrincipal::Write(nsIObjectOutputStream* aStream) {
  // Read is used still for legacy principals
  MOZ_RELEASE_ASSERT(false, "Old style serialization is removed");
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

nsresult ExpandedPrincipal::PopulateJSONObject(Json::Value& aObject) {
  nsAutoCString principalList;
  // First item through we have a blank separator and append the next result
  nsAutoCString sep;
  for (auto& principal : mPrincipals) {
    nsAutoCString JSON;
    BasePrincipal::Cast(principal)->ToJSON(JSON);
    // This is blank for the first run through so the last in the list doesn't
    // add a separator
    principalList.Append(sep);
    sep = ',';
    // Values currently only copes with strings so encode into base64 to allow a
    // CSV safely.
    nsresult rv;
    rv = Base64EncodeAppend(JSON, principalList);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  aObject[std::to_string(eSpecs)] = principalList.get();

  nsAutoCString suffix;
  OriginAttributesRef().CreateSuffix(suffix);
  if (suffix.Length() > 0) {
    aObject[std::to_string(eSuffix)] = suffix.get();
  }

  return NS_OK;
}

already_AddRefed<BasePrincipal> ExpandedPrincipal::FromProperties(
    nsTArray<ExpandedPrincipal::KeyVal>& aFields) {
  MOZ_ASSERT(aFields.Length() == eMax + 1, "Must have all the keys");
  nsTArray<nsCOMPtr<nsIPrincipal>> allowList;
  OriginAttributes attrs;
  // The odd structure here is to make the code to not compile
  // if all the switch enum cases haven't been codified
  for (const auto& field : aFields) {
    switch (field.key) {
      case ExpandedPrincipal::eSpecs:
        if (!field.valueWasSerialized) {
          MOZ_ASSERT(false,
                     "Expanded principals require specs in serialized JSON");
          return nullptr;
        }
        for (const nsACString& each : field.value.Split(',')) {
          nsAutoCString result;
          nsresult rv;
          rv = Base64Decode(each, result);
          MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to decode");

          NS_ENSURE_SUCCESS(rv, nullptr);
          nsCOMPtr<nsIPrincipal> principal = BasePrincipal::FromJSON(result);
          allowList.AppendElement(principal);
        }
        break;
      case ExpandedPrincipal::eSuffix:
        if (field.valueWasSerialized) {
          bool ok = attrs.PopulateFromSuffix(field.value);
          if (!ok) {
            return nullptr;
          }
        }
        break;
    }
  }

  if (allowList.Length() == 0) {
    return nullptr;
  }

  RefPtr<ExpandedPrincipal> expandedPrincipal =
      ExpandedPrincipal::Create(allowList, attrs);

  return expandedPrincipal.forget();
}

NS_IMETHODIMP
ExpandedPrincipal::IsThirdPartyURI(nsIURI* aURI, bool* aRes) {
  // ExpandedPrincipal for extension content scripts consist of two principals,
  // the document's principal and the extension's principal.
  // To make sure that the third-party check behaves like the web page on which
  // the content script is running, ignore the extension's principal.

  for (const auto& principal : mPrincipals) {
    if (!Cast(principal)->AddonPolicy()) {
      return Cast(principal)->IsThirdPartyURI(aURI, aRes);
    }
  }

  if (mPrincipals.IsEmpty()) {
    *aRes = true;
    return NS_OK;
  }

  return Cast(mPrincipals[0])->IsThirdPartyURI(aURI, aRes);
}
