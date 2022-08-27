/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/OriginAttributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"

static const char kSourceChar = ':';
static const char kSanitizedChar = '+';

namespace mozilla {

static void MakeTopLevelInfo(const nsACString& aScheme, const nsACString& aHost,
                             int32_t aPort, bool aUseSite,
                             nsAString& aTopLevelInfo) {
  if (!aUseSite) {
    aTopLevelInfo.Assign(NS_ConvertUTF8toUTF16(aHost));
    return;
  }

  // Note: If you change the serialization of the partition-key, please update
  // StoragePrincipalHelper.cpp too.

  nsAutoCString site;
  site.AssignLiteral("(");
  site.Append(aScheme);
  site.Append(",");
  site.Append(aHost);
  if (aPort != -1) {
    site.Append(",");
    site.AppendInt(aPort);
  }
  site.AppendLiteral(")");

  aTopLevelInfo.Assign(NS_ConvertUTF8toUTF16(site));
}

static void MakeTopLevelInfo(const nsACString& aScheme, const nsACString& aHost,
                             bool aUseSite, nsAString& aTopLevelInfo) {
  MakeTopLevelInfo(aScheme, aHost, -1, aUseSite, aTopLevelInfo);
}

static void PopulateTopLevelInfoFromURI(const bool aIsTopLevelDocument,
                                        nsIURI* aURI, bool aIsFirstPartyEnabled,
                                        bool aForced, bool aUseSite,
                                        nsString OriginAttributes::*aTarget,
                                        OriginAttributes& aOriginAttributes) {
  nsresult rv;

  if (!aURI) {
    return;
  }

  // If the prefs are off or this is not a top level load, bail out.
  if ((!aIsFirstPartyEnabled || !aIsTopLevelDocument) && !aForced) {
    return;
  }

  nsAString& topLevelInfo = aOriginAttributes.*aTarget;

  nsAutoCString scheme;
  rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (scheme.EqualsLiteral("about")) {
    MakeTopLevelInfo(scheme, nsLiteralCString(ABOUT_URI_FIRST_PARTY_DOMAIN),
                     aUseSite, topLevelInfo);
    return;
  }

  // Add-on principals should never get any first-party domain
  // attributes in order to guarantee their storage integrity when switching
  // FPI on and off.
  if (scheme.EqualsLiteral("moz-extension")) {
    return;
  }

  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          aURI, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    topLevelInfo = blobPrincipal->OriginAttributesRef().*aTarget;
    return;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  MOZ_ASSERT(tldService);
  NS_ENSURE_TRUE_VOID(tldService);

  nsAutoCString baseDomain;
  rv = tldService->GetBaseDomain(aURI, 0, baseDomain);
  if (NS_SUCCEEDED(rv)) {
    MakeTopLevelInfo(scheme, baseDomain, aUseSite, topLevelInfo);
    return;
  }

  // Saving before rv is overwritten.
  bool isIpAddress = (rv == NS_ERROR_HOST_IS_IP_ADDRESS);
  bool isInsufficientDomainLevels = (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);

  int32_t port;
  rv = aURI->GetPort(&port);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoCString host;
  rv = aURI->GetHost(host);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (isIpAddress) {
    // If the host is an IPv4/IPv6 address, we still accept it as a
    // valid topLevelInfo.
    nsAutoCString ipAddr;

    if (net_IsValidIPv6Addr(host)) {
      // According to RFC2732, the host of an IPv6 address should be an
      // IPv6reference. The GetHost() of nsIURI will only return the IPv6
      // address. So, we need to convert it back to IPv6reference here.
      ipAddr.AssignLiteral("[");
      ipAddr.Append(host);
      ipAddr.AppendLiteral("]");
    } else {
      ipAddr = host;
    }

    MakeTopLevelInfo(scheme, ipAddr, port, aUseSite, topLevelInfo);
    return;
  }

  if (aUseSite) {
    MakeTopLevelInfo(scheme, host, port, aUseSite, topLevelInfo);
    return;
  }

  if (isInsufficientDomainLevels) {
    nsAutoCString publicSuffix;
    rv = tldService->GetPublicSuffix(aURI, publicSuffix);
    if (NS_SUCCEEDED(rv)) {
      MakeTopLevelInfo(scheme, publicSuffix, port, aUseSite, topLevelInfo);
      return;
    }
  }
}

void OriginAttributes::SetFirstPartyDomain(const bool aIsTopLevelDocument,
                                           nsIURI* aURI, bool aForced) {
  PopulateTopLevelInfoFromURI(
      aIsTopLevelDocument, aURI, IsFirstPartyEnabled(), aForced,
      StaticPrefs::privacy_firstparty_isolate_use_site(),
      &OriginAttributes::mFirstPartyDomain, *this);
}

void OriginAttributes::SetFirstPartyDomain(const bool aIsTopLevelDocument,
                                           const nsACString& aDomain) {
  SetFirstPartyDomain(aIsTopLevelDocument, NS_ConvertUTF8toUTF16(aDomain));
}

void OriginAttributes::SetFirstPartyDomain(const bool aIsTopLevelDocument,
                                           const nsAString& aDomain,
                                           bool aForced) {
  // If the pref is off or this is not a top level load, bail out.
  if ((!IsFirstPartyEnabled() || !aIsTopLevelDocument) && !aForced) {
    return;
  }

  mFirstPartyDomain = aDomain;
}

void OriginAttributes::SetPartitionKey(nsIURI* aURI) {
  PopulateTopLevelInfoFromURI(
      false /* aIsTopLevelDocument */, aURI, IsFirstPartyEnabled(),
      true /* aForced */, StaticPrefs::privacy_dynamic_firstparty_use_site(),
      &OriginAttributes::mPartitionKey, *this);
}

void OriginAttributes::SetPartitionKey(const nsACString& aDomain) {
  SetPartitionKey(NS_ConvertUTF8toUTF16(aDomain));
}

void OriginAttributes::SetPartitionKey(const nsAString& aDomain) {
  mPartitionKey = aDomain;
}

void OriginAttributes::CreateSuffix(nsACString& aStr) const {
  URLParams params;
  nsAutoString value;

  //
  // Important: While serializing any string-valued attributes, perform a
  // release-mode assertion to make sure that they don't contain characters that
  // will break the quota manager when it uses the serialization for file
  // naming.
  //

  if (mInIsolatedMozBrowser) {
    params.Set(u"inBrowser"_ns, u"1"_ns);
  }

  if (mUserContextId != nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID) {
    value.Truncate();
    value.AppendInt(mUserContextId);
    params.Set(u"userContextId"_ns, value);
  }

  if (mPrivateBrowsingId) {
    value.Truncate();
    value.AppendInt(mPrivateBrowsingId);
    params.Set(u"privateBrowsingId"_ns, value);
  }

  if (!mFirstPartyDomain.IsEmpty()) {
    nsAutoString sanitizedFirstPartyDomain(mFirstPartyDomain);
    sanitizedFirstPartyDomain.ReplaceChar(kSourceChar, kSanitizedChar);

    params.Set(u"firstPartyDomain"_ns, sanitizedFirstPartyDomain);
  }

  if (!mGeckoViewSessionContextId.IsEmpty()) {
    nsAutoString sanitizedGeckoViewUserContextId(mGeckoViewSessionContextId);
    sanitizedGeckoViewUserContextId.ReplaceChar(
        dom::quota::QuotaManager::kReplaceChars16, kSanitizedChar);

    params.Set(u"geckoViewUserContextId"_ns, sanitizedGeckoViewUserContextId);
  }

  if (!mPartitionKey.IsEmpty()) {
    nsAutoString sanitizedPartitionKey(mPartitionKey);
    sanitizedPartitionKey.ReplaceChar(kSourceChar, kSanitizedChar);

    params.Set(u"partitionKey"_ns, sanitizedPartitionKey);
  }

  aStr.Truncate();

  params.Serialize(value, true);
  if (!value.IsEmpty()) {
    aStr.AppendLiteral("^");
    aStr.Append(NS_ConvertUTF16toUTF8(value));
  }

// In debug builds, check the whole string for illegal characters too (just in
// case).
#ifdef DEBUG
  nsAutoCString str;
  str.Assign(aStr);
  MOZ_ASSERT(str.FindCharInSet(dom::quota::QuotaManager::kReplaceChars) ==
             kNotFound);
#endif
}

already_AddRefed<nsAtom> OriginAttributes::CreateSuffixAtom() const {
  nsAutoCString suffix;
  CreateSuffix(suffix);
  return NS_Atomize(suffix);
}

void OriginAttributes::CreateAnonymizedSuffix(nsACString& aStr) const {
  OriginAttributes attrs = *this;

  if (!attrs.mFirstPartyDomain.IsEmpty()) {
    attrs.mFirstPartyDomain.AssignLiteral("_anonymizedFirstPartyDomain_");
  }

  if (!attrs.mPartitionKey.IsEmpty()) {
    attrs.mPartitionKey.AssignLiteral("_anonymizedPartitionKey_");
  }

  attrs.CreateSuffix(aStr);
}

bool OriginAttributes::PopulateFromSuffix(const nsACString& aStr) {
  if (aStr.IsEmpty()) {
    return true;
  }

  if (aStr[0] != '^') {
    return false;
  }

  // If a non-default mPrivateBrowsingId is passed and is not present in the
  // suffix, then it will retain the id when it should be default according
  // to the suffix. Set to default before iterating to fix this.
  mPrivateBrowsingId = nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID;

  // Checking that we are in a pristine state

  MOZ_RELEASE_ASSERT(mUserContextId == 0);
  MOZ_RELEASE_ASSERT(mPrivateBrowsingId == 0);
  MOZ_RELEASE_ASSERT(mFirstPartyDomain.IsEmpty());
  MOZ_RELEASE_ASSERT(mGeckoViewSessionContextId.IsEmpty());
  MOZ_RELEASE_ASSERT(mPartitionKey.IsEmpty());

  return URLParams::Parse(
      Substring(aStr, 1, aStr.Length() - 1),
      [this](const nsAString& aName, const nsAString& aValue) {
        if (aName.EqualsLiteral("inBrowser")) {
          if (!aValue.EqualsLiteral("1")) {
            return false;
          }

          mInIsolatedMozBrowser = true;
          return true;
        }

        if (aName.EqualsLiteral("addonId") || aName.EqualsLiteral("appId")) {
          // No longer supported. Silently ignore so that legacy origin strings
          // don't cause failures.
          return true;
        }

        if (aName.EqualsLiteral("userContextId")) {
          nsresult rv;
          int64_t val = aValue.ToInteger64(&rv);
          NS_ENSURE_SUCCESS(rv, false);
          NS_ENSURE_TRUE(val <= UINT32_MAX, false);
          mUserContextId = static_cast<uint32_t>(val);

          return true;
        }

        if (aName.EqualsLiteral("privateBrowsingId")) {
          nsresult rv;
          int64_t val = aValue.ToInteger64(&rv);
          NS_ENSURE_SUCCESS(rv, false);
          NS_ENSURE_TRUE(val >= 0 && val <= UINT32_MAX, false);
          mPrivateBrowsingId = static_cast<uint32_t>(val);

          return true;
        }

        if (aName.EqualsLiteral("firstPartyDomain")) {
          nsAutoString firstPartyDomain(aValue);
          firstPartyDomain.ReplaceChar(kSanitizedChar, kSourceChar);
          mFirstPartyDomain.Assign(firstPartyDomain);
          return true;
        }

        if (aName.EqualsLiteral("geckoViewUserContextId")) {
          mGeckoViewSessionContextId.Assign(aValue);
          return true;
        }

        if (aName.EqualsLiteral("partitionKey")) {
          nsAutoString partitionKey(aValue);
          partitionKey.ReplaceChar(kSanitizedChar, kSourceChar);
          mPartitionKey.Assign(partitionKey);
          return true;
        }

        // No other attributes are supported.
        return false;
      });
}

bool OriginAttributes::PopulateFromOrigin(const nsACString& aOrigin,
                                          nsACString& aOriginNoSuffix) {
  // RFindChar is only available on nsCString.
  nsCString origin(aOrigin);
  int32_t pos = origin.RFindChar('^');

  if (pos == kNotFound) {
    aOriginNoSuffix = origin;
    return true;
  }

  aOriginNoSuffix = Substring(origin, 0, pos);
  return PopulateFromSuffix(Substring(origin, pos));
}

void OriginAttributes::SyncAttributesWithPrivateBrowsing(
    bool aInPrivateBrowsing) {
  mPrivateBrowsingId = aInPrivateBrowsing ? 1 : 0;
}

/* static */
bool OriginAttributes::IsPrivateBrowsing(const nsACString& aOrigin) {
  nsAutoCString dummy;
  OriginAttributes attrs;
  if (NS_WARN_IF(!attrs.PopulateFromOrigin(aOrigin, dummy))) {
    return false;
  }

  return !!attrs.mPrivateBrowsingId;
}

/* static */
bool OriginAttributes::ParsePartitionKey(const nsAString& aPartitionKey,
                                         nsAString& outScheme,
                                         nsAString& outBaseDomain,
                                         int32_t& outPort) {
  outScheme.Truncate();
  outBaseDomain.Truncate();
  outPort = -1;

  // Partition keys have the format "(<scheme>,<baseDomain>,[port])". The port
  // is optional. For example: "(https,example.com,8443)" or
  // "(http,example.org)".
  // When privacy.dynamic_firstparty.use_site = false, the partitionKey contains
  // only the host, e.g. "example.com".
  // See MakeTopLevelInfo for the partitionKey serialization code.

  if (aPartitionKey.IsEmpty()) {
    return true;
  }

  // PartitionKey contains only the host.
  if (!StaticPrefs::privacy_dynamic_firstparty_use_site()) {
    outBaseDomain = aPartitionKey;
    return true;
  }

  // Smallest possible partitionKey is "(x,x)". Scheme and base domain are
  // mandatory.
  if (NS_WARN_IF(aPartitionKey.Length() < 5)) {
    return false;
  }

  if (NS_WARN_IF(aPartitionKey.First() != '(' || aPartitionKey.Last() != ')')) {
    return false;
  }

  // Remove outer brackets so we can string split.
  nsAutoString str(Substring(aPartitionKey, 1, aPartitionKey.Length() - 2));

  uint32_t fieldIndex = 0;
  for (const nsAString& field : str.Split(',')) {
    if (NS_WARN_IF(field.IsEmpty())) {
      // There cannot be empty fields.
      return false;
    }

    if (fieldIndex == 0) {
      outScheme.Assign(field);
    } else if (fieldIndex == 1) {
      outBaseDomain.Assign(field);
    } else if (fieldIndex == 2) {
      // Parse the port which is represented in the partitionKey string as a
      // decimal (base 10) number.
      long port = strtol(NS_ConvertUTF16toUTF8(field).get(), nullptr, 10);
      // Invalid port.
      if (NS_WARN_IF(port == 0)) {
        return false;
      }
      outPort = static_cast<int32_t>(port);
    } else {
      NS_WARNING("Invalid partitionKey. Too many tokens");
      return false;
    }

    fieldIndex++;
  }

  // scheme and base domain are required.
  return fieldIndex > 1;
}

}  // namespace mozilla
