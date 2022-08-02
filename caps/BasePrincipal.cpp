/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"

#include "nsDocShell.h"

#include "ExpandedPrincipal.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsIOService.h"
#include "nsIURIWithSpecialOrigin.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsAboutProtocolUtils.h"
#include "ThirdPartyUtil.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/Components.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/StorageUtils.h"
#include "nsIURL.h"
#include "nsEffectiveTLDService.h"
#include "nsIURIMutator.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "nsIURIMutator.h"
#include "nsMixedContentBlocker.h"
#include "prnetdb.h"
#include "nsIURIFixup.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/StorageAccess.h"
#include "nsPIDOMWindow.h"
#include "nsIURIMutator.h"
#include "mozilla/PermissionManager.h"

#include "json/json.h"
#include "nsSerializationHelper.h"

namespace mozilla {

BasePrincipal::BasePrincipal(PrincipalKind aKind,
                             const nsACString& aOriginNoSuffix,
                             const OriginAttributes& aOriginAttributes)
    : mOriginNoSuffix(NS_Atomize(aOriginNoSuffix)),
      mOriginSuffix(aOriginAttributes.CreateSuffixAtom()),
      mOriginAttributes(aOriginAttributes),
      mKind(aKind),
      mHasExplicitDomain(false) {}

BasePrincipal::BasePrincipal(BasePrincipal* aOther,
                             const OriginAttributes& aOriginAttributes)
    : mOriginNoSuffix(aOther->mOriginNoSuffix),
      mOriginSuffix(aOriginAttributes.CreateSuffixAtom()),
      mOriginAttributes(aOriginAttributes),
      mKind(aOther->mKind),
      mHasExplicitDomain(aOther->mHasExplicitDomain) {}

BasePrincipal::~BasePrincipal() = default;

NS_IMETHODIMP
BasePrincipal::GetOrigin(nsACString& aOrigin) {
  nsresult rv = GetOriginNoSuffix(aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  rv = GetOriginSuffix(suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  aOrigin.Append(suffix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAsciiOrigin(nsACString& aOrigin) {
  aOrigin.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return nsContentUtils::GetASCIIOrigin(prinURI, aOrigin);
}

NS_IMETHODIMP
BasePrincipal::GetHostPort(nsACString& aRes) {
  aRes.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetHostPort(aRes);
}

NS_IMETHODIMP
BasePrincipal::GetHost(nsACString& aRes) {
  aRes.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetHost(aRes);
}

NS_IMETHODIMP
BasePrincipal::GetOriginNoSuffix(nsACString& aOrigin) {
  mOriginNoSuffix->ToUTF8String(aOrigin);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetSiteOrigin(nsACString& aSiteOrigin) {
  nsresult rv = GetSiteOriginNoSuffix(aSiteOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  rv = GetOriginSuffix(suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  aSiteOrigin.Append(suffix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetSiteOriginNoSuffix(nsACString& aSiteOrigin) {
  return GetOriginNoSuffix(aSiteOrigin);
}

// Returns the inner Json::value of the serialized principal
// Example input and return values:
// Null principal:
// {"0":{"0":"moz-nullprincipal:{56cac540-864d-47e7-8e25-1614eab5155e}"}} ->
// {"0":"moz-nullprincipal:{56cac540-864d-47e7-8e25-1614eab5155e}"}
//
// Content principal:
// {"1":{"0":"https://mozilla.com"}} -> {"0":"https://mozilla.com"}
//
// Expanded principal:
// {"2":{"0":"<base64principal1>,<base64principal2>"}} ->
// {"0":"<base64principal1>,<base64principal2>"}
//
// System principal:
// {"3":{}} -> {}
// The aKey passed in also returns the corresponding PrincipalKind enum
//
// Warning: The Json::Value* pointer is into the aRoot object
static const Json::Value* GetPrincipalObject(const Json::Value& aRoot,
                                             int& aOutPrincipalKind) {
  const Json::Value::Members members = aRoot.getMemberNames();
  // We only support one top level key in the object
  if (members.size() != 1) {
    return nullptr;
  }
  // members[0] here is the "0", "1", "2", "3" principalKind
  // that is the top level of the serialized JSON principal
  const std::string stringPrincipalKind = members[0];

  // Next we take the string value from the JSON
  // and convert it into the int for the BasePrincipal::PrincipalKind enum

  // Verify that the key is within the valid range
  int principalKind = std::stoi(stringPrincipalKind);
  MOZ_ASSERT(BasePrincipal::eNullPrincipal == 0,
             "We need to rely on 0 being a bounds check for the first "
             "principal kind.");
  if (principalKind < 0 || principalKind > BasePrincipal::eKindMax) {
    return nullptr;
  }
  MOZ_ASSERT(principalKind == BasePrincipal::eNullPrincipal ||
             principalKind == BasePrincipal::eContentPrincipal ||
             principalKind == BasePrincipal::eExpandedPrincipal ||
             principalKind == BasePrincipal::eSystemPrincipal);
  aOutPrincipalKind = principalKind;

  if (!aRoot[stringPrincipalKind].isObject()) {
    return nullptr;
  }

  // Return the inner value of the principal object
  return &aRoot[stringPrincipalKind];
}

// Accepts the JSON inner object without the wrapping principalKind
// (See GetPrincipalObject for the inner object response examples)
// Creates an array of KeyVal objects that are all defined on the principal
// Each principal type (null, content, expanded) has a KeyVal that stores the
// fields of the JSON
//
// This simplifies deserializing elsewhere as we do the checking for presence
// and string values here for the complete set of serializable keys that the
// corresponding principal supports.
//
// The KeyVal object has the following fields:
// - valueWasSerialized: is true if the deserialized JSON contained a string
// value
// - value: The string that was serialized for this key
// - key: an SerializableKeys enum value specific to the principal.
//        For example content principal is an enum of: eURI, eDomain,
//        eSuffix, eCSP
//
//
//  Given an inner content principal:
//  {"0": "https://mozilla.com", "2": "^privateBrowsingId=1"}
//    |                |          |         |
//    -----------------------------         |
//         |           |                    |
//        Key          ----------------------
//                                |
//                              Value
//
// They Key "0" corresponds to ContentPrincipal::eURI
// They Key "1" corresponds to ContentPrincipal::eSuffix
template <typename T>
static nsTArray<typename T::KeyVal> GetJSONKeys(const Json::Value* aInput) {
  int size = T::eMax + 1;
  nsTArray<typename T::KeyVal> fields;
  for (int i = 0; i != size; i++) {
    typename T::KeyVal* field = fields.AppendElement();
    // field->valueWasSerialized returns if the field was found in the
    // deserialized code. This simplifies the consumers from having to check
    // length.
    field->valueWasSerialized = false;
    field->key = static_cast<typename T::SerializableKeys>(i);
    const std::string key = std::to_string(field->key);
    if (aInput->isMember(key)) {
      const Json::Value& val = (*aInput)[key];
      if (val.isString()) {
        field->value.Append(nsDependentCString(val.asCString()));
        field->valueWasSerialized = true;
      }
    }
  }
  return fields;
}

// Takes a JSON string and parses it turning it into a principal of the
// corresponding type
//
// Given a content principal:
//
//                               inner JSON object
//                                      |
//       ---------------------------------------------------------
//       |                                                       |
// {"1": {"0": "https://mozilla.com", "2": "^privateBrowsingId=1"}}
//   |     |             |             |            |
//   |     -----------------------------            |
//   |              |    |                          |
// PrincipalKind    |    |                          |
//                  |    ----------------------------
//           SerializableKeys           |
//                                    Value
//
// The string is first deserialized with jsoncpp to get the Json::Value of the
// object. The inner JSON object is parsed with GetPrincipalObject which returns
// a KeyVal array of the inner object's fields. PrincipalKind is returned by
// GetPrincipalObject which is then used to decide which principal
// implementation of FromProperties to call. The corresponding FromProperties
// call takes the KeyVal fields and turns it into a principal.
already_AddRefed<BasePrincipal> BasePrincipal::FromJSON(
    const nsACString& aJSON) {
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> const reader(builder.newCharReader());
  bool parseSuccess =
      reader->parse(aJSON.BeginReading(), aJSON.EndReading(), &root, nullptr);
  if (!parseSuccess) {
    MOZ_ASSERT(false,
               "Unable to parse string as JSON to deserialize as a principal");
    return nullptr;
  }

  int principalKind = -1;
  const Json::Value* value = GetPrincipalObject(root, principalKind);
  if (!value) {
#ifdef DEBUG
    fprintf(stderr, "Unexpected JSON principal %s\n",
            root.toStyledString().c_str());
#endif
    MOZ_ASSERT(false, "Unexpected JSON to deserialize as a principal");

    return nullptr;
  }
  MOZ_ASSERT(principalKind != -1,
             "PrincipalKind should always be >=0 by this point");

  if (principalKind == eSystemPrincipal) {
    RefPtr<BasePrincipal> principal =
        BasePrincipal::Cast(nsContentUtils::GetSystemPrincipal());
    return principal.forget();
  }

  if (principalKind == eNullPrincipal) {
    nsTArray<NullPrincipal::KeyVal> res = GetJSONKeys<NullPrincipal>(value);
    return NullPrincipal::FromProperties(res);
  }

  if (principalKind == eContentPrincipal) {
    nsTArray<ContentPrincipal::KeyVal> res =
        GetJSONKeys<ContentPrincipal>(value);
    return ContentPrincipal::FromProperties(res);
  }

  if (principalKind == eExpandedPrincipal) {
    nsTArray<ExpandedPrincipal::KeyVal> res =
        GetJSONKeys<ExpandedPrincipal>(value);
    return ExpandedPrincipal::FromProperties(res);
  }

  MOZ_RELEASE_ASSERT(false, "Unexpected enum to deserialize as a principal");
}

nsresult BasePrincipal::PopulateJSONObject(Json::Value& aObject) {
  return NS_OK;
}

// Returns a JSON representation of the principal.
// Calling BasePrincipal::FromJSON will deserialize the JSON into
// the corresponding principal type.
nsresult BasePrincipal::ToJSON(nsACString& aResult) {
  MOZ_ASSERT(aResult.IsEmpty(), "ToJSON only supports an empty result input");
  aResult.Truncate();

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  Json::Value innerJSONObject = Json::objectValue;

  nsresult rv = PopulateJSONObject(innerJSONObject);
  NS_ENSURE_SUCCESS(rv, rv);

  Json::Value root = Json::objectValue;
  std::string key = std::to_string(Kind());
  root[key] = innerJSONObject;
  std::string result = Json::writeString(builder, root);
  aResult.Append(result);
  if (aResult.Length() == 0) {
    MOZ_ASSERT(false, "JSON writer failed to output a principal serialization");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

bool BasePrincipal::FastSubsumesIgnoringFPD(
    nsIPrincipal* aOther, DocumentDomainConsideration aConsideration) {
  MOZ_ASSERT(aOther);

  if (Kind() == eContentPrincipal &&
      !dom::ChromeUtils::IsOriginAttributesEqualIgnoringFPD(
          mOriginAttributes, Cast(aOther)->mOriginAttributes)) {
    return false;
  }

  return SubsumesInternal(aOther, aConsideration);
}

bool BasePrincipal::Subsumes(nsIPrincipal* aOther,
                             DocumentDomainConsideration aConsideration) {
  MOZ_ASSERT(aOther);
  MOZ_ASSERT_IF(Kind() == eContentPrincipal, mOriginSuffix);

  // Expanded principals handle origin attributes for each of their
  // sub-principals individually, null principals do only simple checks for
  // pointer equality, and system principals are immune to origin attributes
  // checks, so only do this check for content principals.
  if (Kind() == eContentPrincipal &&
      mOriginSuffix != Cast(aOther)->mOriginSuffix) {
    return false;
  }

  return SubsumesInternal(aOther, aConsideration);
}

NS_IMETHODIMP
BasePrincipal::Equals(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastEquals(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsForPermission(nsIPrincipal* aOther, bool aExactHost,
                                   bool* aResult) {
  *aResult = false;
  NS_ENSURE_ARG_POINTER(aOther);
  NS_ENSURE_ARG_POINTER(aResult);

  auto* other = Cast(aOther);
  if (Kind() != other->Kind()) {
    // Principals of different kinds can't be equal.
    return NS_OK;
  }

  if (Kind() == eSystemPrincipal) {
    *aResult = this == other;
    return NS_OK;
  }

  if (Kind() == eNullPrincipal) {
    // We don't store permissions for NullPrincipals.
    return NS_OK;
  }

  MOZ_ASSERT(Kind() == eExpandedPrincipal || Kind() == eContentPrincipal);

  // Certain origin attributes should not be used to isolate permissions.
  // Create a stripped copy of both OA sets to compare.
  mozilla::OriginAttributes ourAttrs = mOriginAttributes;
  PermissionManager::MaybeStripOriginAttributes(false, ourAttrs);
  mozilla::OriginAttributes theirAttrs = aOther->OriginAttributesRef();
  PermissionManager::MaybeStripOriginAttributes(false, theirAttrs);

  if (ourAttrs != theirAttrs) {
    return NS_OK;
  }

  if (mOriginNoSuffix == other->mOriginNoSuffix) {
    *aResult = true;
    return NS_OK;
  }

  // If we are matching with an exact host, we're done now - the permissions
  // don't match otherwise, we need to start comparing subdomains!
  if (aExactHost) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> ourURI;
  nsresult rv = GetURI(getter_AddRefs(ourURI));
  NS_ENSURE_SUCCESS(rv, rv);
  // Some principal types may indicate success, but still return nullptr for
  // URI.
  NS_ENSURE_TRUE(ourURI, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> otherURI;
  rv = other->GetURI(getter_AddRefs(otherURI));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(otherURI, NS_ERROR_FAILURE);

  // Compare schemes
  nsAutoCString otherScheme;
  rv = otherURI->GetScheme(otherScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString ourScheme;
  rv = ourURI->GetScheme(ourScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (otherScheme != ourScheme) {
    return NS_OK;
  }

  // Compare ports
  int32_t otherPort;
  rv = otherURI->GetPort(&otherPort);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t ourPort;
  rv = ourURI->GetPort(&ourPort);
  NS_ENSURE_SUCCESS(rv, rv);

  if (otherPort != ourPort) {
    return NS_OK;
  }

  // Check if the host or any subdomain of their host matches.
  nsAutoCString otherHost;
  rv = otherURI->GetHost(otherHost);
  if (NS_FAILED(rv) || otherHost.IsEmpty()) {
    return NS_OK;
  }

  nsAutoCString ourHost;
  rv = ourURI->GetHost(ourHost);
  if (NS_FAILED(rv) || ourHost.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    NS_ERROR("Should have a tld service!");
    return NS_ERROR_FAILURE;
  }

  // This loop will not loop forever, as GetNextSubDomain will eventually fail
  // with NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS.
  while (otherHost != ourHost) {
    rv = tldService->GetNextSubDomain(otherHost, otherHost);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        return NS_OK;
      }
      return rv;
    }
  }

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsConsideringDomain(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastEqualsConsideringDomain(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsURI(nsIURI* aOtherURI, bool* aResult) {
  *aResult = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->EqualsExceptRef(aOtherURI, aResult);
}

NS_IMETHODIMP
BasePrincipal::Subsumes(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastSubsumes(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomain(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastSubsumesConsideringDomain(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomainIgnoringFPD(nsIPrincipal* aOther,
                                                    bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastSubsumesConsideringDomainIgnoringFPD(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::CheckMayLoad(nsIURI* aURI, bool aAllowIfInheritsPrincipal) {
  return CheckMayLoadHelper(aURI, aAllowIfInheritsPrincipal, false, 0);
}

NS_IMETHODIMP
BasePrincipal::CheckMayLoadWithReporting(nsIURI* aURI,
                                         bool aAllowIfInheritsPrincipal,
                                         uint64_t aInnerWindowID) {
  return CheckMayLoadHelper(aURI, aAllowIfInheritsPrincipal, true,
                            aInnerWindowID);
}

nsresult BasePrincipal::CheckMayLoadHelper(nsIURI* aURI,
                                           bool aAllowIfInheritsPrincipal,
                                           bool aReport,
                                           uint64_t aInnerWindowID) {
  NS_ENSURE_ARG_POINTER(aURI);
  MOZ_ASSERT(
      aReport || aInnerWindowID == 0,
      "Why do we have an inner window id if we're not supposed to report?");

  // Check the internal method first, which allows us to quickly approve loads
  // for the System Principal.
  if (MayLoadInternal(aURI)) {
    return NS_OK;
  }

  nsresult rv;
  if (aAllowIfInheritsPrincipal) {
    // If the caller specified to allow loads of URIs that inherit
    // our principal, allow the load if this URI inherits its principal.
    bool doesInheritSecurityContext;
    rv = NS_URIChainHasFlags(aURI,
                             nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                             &doesInheritSecurityContext);
    if (NS_SUCCEEDED(rv) && doesInheritSecurityContext) {
      return NS_OK;
    }
  }

  // Web Accessible Resources in MV2 Extensions are marked with
  // URI_FETCHABLE_BY_ANYONE
  bool fetchableByAnyone;
  rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_FETCHABLE_BY_ANYONE,
                           &fetchableByAnyone);
  if (NS_SUCCEEDED(rv) && fetchableByAnyone) {
    return NS_OK;
  }

  // Get the principal uri for the last flag check or error.
  nsCOMPtr<nsIURI> prinURI;
  rv = GetURI(getter_AddRefs(prinURI));
  if (!(NS_SUCCEEDED(rv) && prinURI)) {
    return NS_ERROR_DOM_BAD_URI;
  }

  // If MV3 Extension uris are web accessible by this principal it is allowed to
  // load.
  bool maybeWebAccessible = false;
  NS_URIChainHasFlags(aURI, nsIProtocolHandler::WEBEXT_URI_WEB_ACCESSIBLE,
                      &maybeWebAccessible);
  NS_ENSURE_SUCCESS(rv, rv);
  if (maybeWebAccessible) {
    bool isWebAccessible = false;
    rv = ExtensionPolicyService::GetSingleton().SourceMayLoadExtensionURI(
        prinURI, aURI, &isWebAccessible);
    if (NS_SUCCEEDED(rv) && isWebAccessible) {
      return NS_OK;
    }
  }

  if (aReport) {
    nsScriptSecurityManager::ReportError(
        "CheckSameOriginError", prinURI, aURI,
        mOriginAttributes.mPrivateBrowsingId > 0, aInnerWindowID);
  }

  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
BasePrincipal::IsThirdPartyURI(nsIURI* aURI, bool* aRes) {
  if (IsSystemPrincipal() || (AddonPolicy() && AddonAllowsLoad(aURI))) {
    *aRes = false;
    return NS_OK;
  }

  *aRes = true;
  // If we do not have a URI its always 3rd party.
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
  return thirdPartyUtil->IsThirdPartyURI(prinURI, aURI, aRes);
}

NS_IMETHODIMP
BasePrincipal::IsThirdPartyPrincipal(nsIPrincipal* aPrin, bool* aRes) {
  *aRes = true;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return aPrin->IsThirdPartyURI(prinURI, aRes);
}
NS_IMETHODIMP
BasePrincipal::IsThirdPartyChannel(nsIChannel* aChan, bool* aRes) {
  if (IsSystemPrincipal()) {
    // Nothing is 3rd party to the system principal.
    *aRes = false;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> prinURI;
  GetURI(getter_AddRefs(prinURI));
  ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
  return thirdPartyUtil->IsThirdPartyChannel(aChan, prinURI, aRes);
}

NS_IMETHODIMP
BasePrincipal::IsSameOrigin(nsIURI* aURI, bool* aRes) {
  *aRes = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    // Note that expanded and system principals return here, because they have
    // no URI.
    return NS_OK;
  }
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm) {
    return NS_OK;
  }
  bool reportError = false;
  bool isPrivateWindow = false;  // Only used for error reporting.
  *aRes = NS_SUCCEEDED(
      ssm->CheckSameOriginURI(prinURI, aURI, reportError, isPrivateWindow));
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::IsL10nAllowed(nsIURI* aURI, bool* aRes) {
  *aRes = false;

  if (nsContentUtils::IsErrorPage(aURI)) {
    *aRes = true;
    return NS_OK;
  }

  // The system principal is always allowed.
  if (IsSystemPrincipal()) {
    *aRes = true;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, NS_OK);

  bool hasFlags;

  // Allow access to uris that cannot be loaded by web content.
  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_DANGEROUS_TO_LOAD,
                           &hasFlags);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  if (hasFlags) {
    *aRes = true;
    return NS_OK;
  }

  // UI resources also get access.
  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                           &hasFlags);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  if (hasFlags) {
    *aRes = true;
    return NS_OK;
  }

  auto policy = AddonPolicy();
  *aRes = (policy && policy->IsPrivileged());
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::AllowsRelaxStrictFileOriginPolicy(nsIURI* aURI, bool* aRes) {
  *aRes = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  *aRes = NS_RelaxStrictFileOriginPolicy(aURI, prinURI);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetPrefLightCacheKey(nsIURI* aURI, bool aWithCredentials,
                                    const OriginAttributes& aOriginAttributes,
                                    nsACString& _retval) {
  _retval.Truncate();
  constexpr auto space = " "_ns;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString scheme, host, port;
  if (uri) {
    uri->GetScheme(scheme);
    uri->GetHost(host);
    port.AppendInt(NS_GetRealPort(uri));
  }

  if (aWithCredentials) {
    _retval.AssignLiteral("cred");
  } else {
    _retval.AssignLiteral("nocred");
  }

  nsAutoCString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString originAttributesSuffix;
  aOriginAttributes.CreateSuffix(originAttributesSuffix);

  _retval.Append(space + scheme + space + host + space + port + space + spec +
                 space + originAttributesSuffix);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::HasFirstpartyStorageAccess(mozIDOMWindow* aCheckWindow,
                                          uint32_t* aRejectedReason,
                                          bool* aOutAllowed) {
  *aRejectedReason = 0;
  *aOutAllowed = false;

  nsPIDOMWindowInner* win = nsPIDOMWindowInner::From(aCheckWindow);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aOutAllowed = ShouldAllowAccessFor(win, uri, aRejectedReason);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsNullPrincipal(bool* aResult) {
  *aResult = Kind() == eNullPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsContentPrincipal(bool* aResult) {
  *aResult = Kind() == eContentPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsExpandedPrincipal(bool* aResult) {
  *aResult = Kind() == eExpandedPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAsciiSpec(nsACString& aSpec) {
  aSpec.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetAsciiSpec(aSpec);
}

NS_IMETHODIMP
BasePrincipal::GetSpec(nsACString& aSpec) {
  aSpec.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetSpec(aSpec);
}

NS_IMETHODIMP
BasePrincipal::GetAsciiHost(nsACString& aHost) {
  aHost.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetAsciiHost(aHost);
}

NS_IMETHODIMP
BasePrincipal::GetExposablePrePath(nsACString& aPrepath) {
  aPrepath.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> exposableURI = net::nsIOService::CreateExposableURI(prinURI);
  return exposableURI->GetDisplayPrePath(aPrepath);
}

NS_IMETHODIMP
BasePrincipal::GetExposableSpec(nsACString& aSpec) {
  aSpec.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  nsCOMPtr<nsIURI> clone;
  rv = NS_MutateURI(prinURI)
           .SetQuery(""_ns)
           .SetRef(""_ns)
           .SetUserPass(""_ns)
           .Finalize(clone);
  NS_ENSURE_SUCCESS(rv, rv);
  return clone->GetAsciiSpec(aSpec);
}

NS_IMETHODIMP
BasePrincipal::GetPrePath(nsACString& aPath) {
  aPath.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetPrePath(aPath);
}

NS_IMETHODIMP
BasePrincipal::GetFilePath(nsACString& aPath) {
  aPath.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetFilePath(aPath);
}

NS_IMETHODIMP
BasePrincipal::GetIsSystemPrincipal(bool* aResult) {
  *aResult = IsSystemPrincipal();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsAddonOrExpandedAddonPrincipal(bool* aResult) {
  *aResult = AddonPolicy() || ContentScriptAddonPolicy();
  return NS_OK;
}

NS_IMETHODIMP BasePrincipal::GetIsOnion(bool* aIsOnion) {
  *aIsOnion = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }

  nsAutoCString host;
  rv = prinURI->GetHost(host);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }
  *aIsOnion = StringEndsWith(host, ".onion"_ns);
  return NS_OK;
}

NS_IMETHODIMP BasePrincipal::GetIsIpAddress(bool* aIsIpAddress) {
  *aIsIpAddress = false;

  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }

  nsAutoCString host;
  rv = prinURI->GetHost(host);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  PRNetAddr prAddr;
  memset(&prAddr, 0, sizeof(prAddr));

  if (PR_StringToNetAddr(host.get(), &prAddr) == PR_SUCCESS) {
    *aIsIpAddress = true;
  }

  return NS_OK;
}

NS_IMETHODIMP BasePrincipal::GetIsLocalIpAddress(bool* aIsIpAddress) {
  *aIsIpAddress = false;

  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }

  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  if (NS_FAILED(rv) || !ioService) {
    return NS_OK;
  }
  rv = ioService->HostnameIsLocalIPAddress(prinURI, aIsIpAddress);
  if (NS_FAILED(rv)) {
    *aIsIpAddress = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetScheme(nsACString& aScheme) {
  aScheme.Truncate();

  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }

  return prinURI->GetScheme(aScheme);
}

NS_IMETHODIMP
BasePrincipal::SchemeIs(const char* aScheme, bool* aResult) {
  *aResult = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_WARN_IF(NS_FAILED(rv)) || !prinURI) {
    return NS_OK;
  }
  *aResult = prinURI->SchemeIs(aScheme);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::IsURIInPrefList(const char* aPref, bool* aResult) {
  *aResult = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  *aResult = nsContentUtils::IsURIInPrefList(prinURI, aPref);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::IsURIInList(const nsACString& aList, bool* aResult) {
  *aResult = false;
  nsCOMPtr<nsIURI> prinURI;

  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }

  *aResult = nsContentUtils::IsURIInList(prinURI, nsCString(aList));
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsOriginPotentiallyTrustworthy(bool* aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  *aResult = false;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return NS_OK;
  }

  *aResult = nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(uri);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAboutModuleFlags(uint32_t* flags) {
  *flags = 0;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (!prinURI->SchemeIs("about")) {
    return NS_OK;
  }

  nsCOMPtr<nsIAboutModule> aboutModule;
  rv = NS_GetAboutModule(prinURI, getter_AddRefs(aboutModule));
  if (NS_FAILED(rv) || !aboutModule) {
    return rv;
  }
  return aboutModule->GetURIFlags(prinURI, flags);
}

NS_IMETHODIMP
BasePrincipal::GetOriginAttributes(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal) {
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginSuffix(nsACString& aOriginAttributes) {
  MOZ_ASSERT(mOriginSuffix);
  mOriginSuffix->ToUTF8String(aOriginAttributes);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetUserContextId(uint32_t* aUserContextId) {
  *aUserContextId = UserContextId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId) {
  *aPrivateBrowsingId = PrivateBrowsingId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsInIsolatedMozBrowserElement(
    bool* aIsInIsolatedMozBrowserElement) {
  *aIsInIsolatedMozBrowserElement = IsInIsolatedMozBrowserElement();
  return NS_OK;
}

nsresult BasePrincipal::GetAddonPolicy(
    extensions::WebExtensionPolicy** aResult) {
  RefPtr<extensions::WebExtensionPolicy> policy(AddonPolicy());
  policy.forget(aResult);
  return NS_OK;
}

extensions::WebExtensionPolicy* BasePrincipal::AddonPolicy() {
  if (Is<ContentPrincipal>()) {
    return As<ContentPrincipal>()->AddonPolicy();
  }
  return nullptr;
}

bool BasePrincipal::AddonHasPermission(const nsAtom* aPerm) {
  if (auto policy = AddonPolicy()) {
    return policy->HasPermission(aPerm);
  }
  return false;
}

nsIPrincipal* BasePrincipal::PrincipalToInherit(nsIURI* aRequestedURI) {
  if (Is<ExpandedPrincipal>()) {
    return As<ExpandedPrincipal>()->PrincipalToInherit(aRequestedURI);
  }
  return this;
}

bool BasePrincipal::IsLoopbackHost() {
  nsAutoCString host;
  nsresult rv = GetHost(host);
  NS_ENSURE_SUCCESS(rv, false);
  return nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackHost(host);
}

already_AddRefed<BasePrincipal> BasePrincipal::CreateContentPrincipal(
    nsIURI* aURI, const OriginAttributes& aAttrs) {
  MOZ_ASSERT(aURI);

  nsAutoCString originNoSuffix;
  nsresult rv =
      ContentPrincipal::GenerateOriginNoSuffixFromURI(aURI, originNoSuffix);
  if (NS_FAILED(rv)) {
    // If the generation of the origin fails, we still want to have a valid
    // principal. Better to return a null principal here.
    return NullPrincipal::Create(aAttrs);
  }

  return CreateContentPrincipal(aURI, aAttrs, originNoSuffix);
}

already_AddRefed<BasePrincipal> BasePrincipal::CreateContentPrincipal(
    nsIURI* aURI, const OriginAttributes& aAttrs,
    const nsACString& aOriginNoSuffix) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(!aOriginNoSuffix.IsEmpty());

  // If the URI is supposed to inherit the security context of whoever loads it,
  // we shouldn't make a content principal for it.
  bool inheritsPrincipal;
  nsresult rv = NS_URIChainHasFlags(
      aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
      &inheritsPrincipal);
  if (NS_FAILED(rv) || inheritsPrincipal) {
    return NullPrincipal::Create(aAttrs);
  }

  // Check whether the URI knows what its principal is supposed to be.
#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  nsCOMPtr<nsIURIWithSpecialOrigin> uriWithSpecialOrigin =
      do_QueryInterface(aURI);
  if (uriWithSpecialOrigin) {
    nsCOMPtr<nsIURI> origin;
    rv = uriWithSpecialOrigin->GetOrigin(getter_AddRefs(origin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    MOZ_ASSERT(origin);
    OriginAttributes attrs;
    RefPtr<BasePrincipal> principal = CreateContentPrincipal(origin, attrs);
    return principal.forget();
  }
#endif

  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          aURI, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    RefPtr<BasePrincipal> principal = Cast(blobPrincipal);
    return principal.forget();
  }

  // Mint a content principal.
  RefPtr<ContentPrincipal> principal =
      new ContentPrincipal(aURI, aAttrs, aOriginNoSuffix);
  return principal.forget();
}

already_AddRefed<BasePrincipal> BasePrincipal::CreateContentPrincipal(
    const nsACString& aOrigin) {
  MOZ_ASSERT(!StringBeginsWith(aOrigin, "["_ns),
             "CreateContentPrincipal does not support System and Expanded "
             "principals");

  MOZ_ASSERT(
      !StringBeginsWith(aOrigin, nsLiteralCString(NS_NULLPRINCIPAL_SCHEME ":")),
      "CreateContentPrincipal does not support NullPrincipal");

  nsAutoCString originNoSuffix;
  OriginAttributes attrs;
  if (!attrs.PopulateFromOrigin(aOrigin, originNoSuffix)) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return BasePrincipal::CreateContentPrincipal(uri, attrs);
}

already_AddRefed<BasePrincipal> BasePrincipal::CloneForcingOriginAttributes(
    const OriginAttributes& aOriginAttributes) {
  if (NS_WARN_IF(!IsContentPrincipal())) {
    return nullptr;
  }

  nsAutoCString originNoSuffix;
  nsresult rv = GetOriginNoSuffix(originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(GetURI(getter_AddRefs(uri)));

  RefPtr<ContentPrincipal> copy =
      new ContentPrincipal(uri, aOriginAttributes, originNoSuffix);
  return copy.forget();
}

extensions::WebExtensionPolicy* BasePrincipal::ContentScriptAddonPolicy() {
  if (!Is<ExpandedPrincipal>()) {
    return nullptr;
  }

  auto expanded = As<ExpandedPrincipal>();
  for (auto& prin : expanded->AllowList()) {
    if (auto policy = BasePrincipal::Cast(prin)->AddonPolicy()) {
      return policy;
    }
  }

  return nullptr;
}

bool BasePrincipal::AddonAllowsLoad(nsIURI* aURI,
                                    bool aExplicit /* = false */) {
  if (Is<ExpandedPrincipal>()) {
    return As<ExpandedPrincipal>()->AddonAllowsLoad(aURI, aExplicit);
  }
  if (auto policy = AddonPolicy()) {
    return policy->CanAccessURI(aURI, aExplicit);
  }
  return false;
}

NS_IMETHODIMP
BasePrincipal::GetLocalStorageQuotaKey(nsACString& aKey) {
  aKey.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  // The special handling of the file scheme should be consistent with
  // GetStorageOriginKey.

  nsAutoCString baseDomain;
  rv = uri->GetAsciiHost(baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (baseDomain.IsEmpty() && uri->SchemeIs("file")) {
    nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->GetDirectory(baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsIEffectiveTLDService> eTLDService(
        do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString eTLDplusOne;
    rv = eTLDService->GetBaseDomain(uri, 0, eTLDplusOne);
    if (NS_SUCCEEDED(rv)) {
      baseDomain = eTLDplusOne;
    } else if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
               rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
      rv = NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  OriginAttributesRef().CreateSuffix(aKey);

  nsAutoCString subdomainsDBKey;
  rv = dom::StorageUtils::CreateReversedDomain(baseDomain, subdomainsDBKey);
  NS_ENSURE_SUCCESS(rv, rv);

  aKey.Append(':');
  aKey.Append(subdomainsDBKey);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetNextSubDomainPrincipal(
    nsIPrincipal** aNextSubDomainPrincipal) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return NS_OK;
  }

  nsAutoCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv) || host.IsEmpty()) {
    return NS_OK;
  }

  nsCString subDomain;
  rv = nsEffectiveTLDService::GetInstance()->GetNextSubDomain(host, subDomain);

  if (NS_FAILED(rv) || subDomain.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> subDomainURI;
  rv = NS_MutateURI(uri).SetHost(subDomain).Finalize(subDomainURI);
  if (NS_FAILED(rv) || !subDomainURI) {
    return NS_OK;
  }
  // Copy the attributes over
  mozilla::OriginAttributes attrs = OriginAttributesRef();

  if (!StaticPrefs::permissions_isolateBy_userContext()) {
    // Disable userContext for permissions.
    attrs.StripAttributes(mozilla::OriginAttributes::STRIP_USER_CONTEXT_ID);
  }
  RefPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(subDomainURI, attrs);

  if (!principal) {
    return NS_OK;
  }
  principal.forget(aNextSubDomainPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetStorageOriginKey(nsACString& aOriginKey) {
  aOriginKey.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  // The special handling of the file scheme should be consistent with
  // GetLocalStorageQuotaKey.

  nsAutoCString domainOrigin;
  rv = uri->GetAsciiHost(domainOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (domainOrigin.IsEmpty()) {
    // For the file:/// protocol use the exact directory as domain.
    if (uri->SchemeIs("file")) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = url->GetDirectory(domainOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Append reversed domain
  nsAutoCString reverseDomain;
  rv = dom::StorageUtils::CreateReversedDomain(domainOrigin, reverseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  aOriginKey.Append(reverseDomain);

  // Append scheme
  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  aOriginKey.Append(':');
  aOriginKey.Append(scheme);

  // Append port if any
  int32_t port = NS_GetRealPort(uri);
  if (port != -1) {
    aOriginKey.Append(nsPrintfCString(":%d", port));
  }

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsScriptAllowedByPolicy(bool* aIsScriptAllowedByPolicy) {
  *aIsScriptAllowedByPolicy = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm) {
    return NS_ERROR_UNEXPECTED;
  }
  return ssm->PolicyAllowsScript(prinURI, aIsScriptAllowedByPolicy);
}

bool SiteIdentifier::Equals(const SiteIdentifier& aOther) const {
  MOZ_ASSERT(IsInitialized());
  MOZ_ASSERT(aOther.IsInitialized());
  return mPrincipal->FastEquals(aOther.mPrincipal);
}

NS_IMETHODIMP
BasePrincipal::CreateReferrerInfo(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                                  nsIReferrerInfo** _retval) {
  nsCOMPtr<nsIURI> prinURI;
  RefPtr<dom::ReferrerInfo> info;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    info = new dom::ReferrerInfo(nullptr);
    info.forget(_retval);
    return NS_OK;
  }
  info = new dom::ReferrerInfo(prinURI, aReferrerPolicy);
  info.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetPrecursorPrincipal(nsIPrincipal** aPrecursor) {
  *aPrecursor = nullptr;
  return NS_OK;
}

NS_IMPL_ADDREF(BasePrincipal::Deserializer)
NS_IMPL_RELEASE(BasePrincipal::Deserializer)

NS_INTERFACE_MAP_BEGIN(BasePrincipal::Deserializer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
  if (mPrincipal) {
    return mPrincipal->QueryInterface(aIID, aInstancePtr);
  } else
NS_INTERFACE_MAP_END

NS_IMETHODIMP
BasePrincipal::Deserializer::Write(nsIObjectOutputStream* aStream) {
  // Read is used still for legacy principals
  MOZ_RELEASE_ASSERT(false, "Old style serialization is removed");
  return NS_OK;
}

}  // namespace mozilla
