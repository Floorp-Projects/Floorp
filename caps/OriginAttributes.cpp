/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsURLHelper.h"

namespace mozilla {

using dom::URLParams;

bool OriginAttributes::sFirstPartyIsolation = false;
bool OriginAttributes::sRestrictedOpenerAccess = false;
bool OriginAttributes::sBlockPostMessageForFPI = false;

void OriginAttributes::InitPrefs() {
  MOZ_ASSERT(NS_IsMainThread());
  static bool sInited = false;
  if (!sInited) {
    sInited = true;
    Preferences::AddBoolVarCache(&sFirstPartyIsolation,
                                 "privacy.firstparty.isolate");
    Preferences::AddBoolVarCache(
        &sRestrictedOpenerAccess,
        "privacy.firstparty.isolate.restrict_opener_access");
    Preferences::AddBoolVarCache(
        &sBlockPostMessageForFPI,
        "privacy.firstparty.isolate.block_post_message");
  }
}

void OriginAttributes::SetFirstPartyDomain(const bool aIsTopLevelDocument,
                                           nsIURI* aURI, bool aForced) {
  bool isFirstPartyEnabled = IsFirstPartyEnabled();

  // If the prefs are off or this is not a top level load, bail out.
  if ((!isFirstPartyEnabled || !aIsTopLevelDocument) && !aForced) {
    return;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  MOZ_ASSERT(tldService);
  if (!tldService) {
    return;
  }

  nsAutoCString baseDomain;
  nsresult rv = tldService->GetBaseDomain(aURI, 0, baseDomain);
  if (NS_SUCCEEDED(rv)) {
    mFirstPartyDomain = NS_ConvertUTF8toUTF16(baseDomain);
    return;
  }

  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS) {
    // If the host is an IPv4/IPv6 address, we still accept it as a
    // valid firstPartyDomain.
    nsAutoCString ipAddr;
    rv = aURI->GetHost(ipAddr);
    NS_ENSURE_SUCCESS_VOID(rv);

    if (net_IsValidIPv6Addr(ipAddr)) {
      // According to RFC2732, the host of an IPv6 address should be an
      // IPv6reference. The GetHost() of nsIURI will only return the IPv6
      // address. So, we need to convert it back to IPv6reference here.
      mFirstPartyDomain.Truncate();
      mFirstPartyDomain.AssignLiteral("[");
      mFirstPartyDomain.Append(NS_ConvertUTF8toUTF16(ipAddr));
      mFirstPartyDomain.AppendLiteral("]");
    } else {
      mFirstPartyDomain = NS_ConvertUTF8toUTF16(ipAddr);
    }

    return;
  }

  // Saving isInsufficientDomainLevels before rv is overwritten.
  bool isInsufficientDomainLevels = (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  nsAutoCString scheme;
  rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (scheme.EqualsLiteral("about")) {
    mFirstPartyDomain.AssignLiteral(ABOUT_URI_FIRST_PARTY_DOMAIN);
    return;
  }

  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          aURI, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    mFirstPartyDomain = blobPrincipal->OriginAttributesRef().mFirstPartyDomain;
    return;
  }

  if (isInsufficientDomainLevels) {
    nsAutoCString publicSuffix;
    rv = tldService->GetPublicSuffix(aURI, publicSuffix);
    if (NS_SUCCEEDED(rv)) {
      mFirstPartyDomain = NS_ConvertUTF8toUTF16(publicSuffix);
    }
    return;
  }
}

void OriginAttributes::SetFirstPartyDomain(const bool aIsTopLevelDocument,
                                           const nsACString& aDomain) {
  bool isFirstPartyEnabled = IsFirstPartyEnabled();

  // If the pref is off or this is not a top level load, bail out.
  if (!isFirstPartyEnabled || !aIsTopLevelDocument) {
    return;
  }

  mFirstPartyDomain = NS_ConvertUTF8toUTF16(aDomain);
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
    params.Set(NS_LITERAL_STRING("inBrowser"), NS_LITERAL_STRING("1"));
  }

  if (mUserContextId != nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID) {
    value.Truncate();
    value.AppendInt(mUserContextId);
    params.Set(NS_LITERAL_STRING("userContextId"), value);
  }

  if (mPrivateBrowsingId) {
    value.Truncate();
    value.AppendInt(mPrivateBrowsingId);
    params.Set(NS_LITERAL_STRING("privateBrowsingId"), value);
  }

  if (!mFirstPartyDomain.IsEmpty()) {
    nsAutoString sanitizedFirstPartyDomain(mFirstPartyDomain);
    sanitizedFirstPartyDomain.ReplaceChar(
        dom::quota::QuotaManager::kReplaceChars, '+');

    params.Set(NS_LITERAL_STRING("firstPartyDomain"),
               sanitizedFirstPartyDomain);
  }

  aStr.Truncate();

  params.Serialize(value);
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

void OriginAttributes::CreateAnonymizedSuffix(nsACString& aStr) const {
  OriginAttributes attrs = *this;

  if (!attrs.mFirstPartyDomain.IsEmpty()) {
    attrs.mFirstPartyDomain.AssignLiteral("_anonymizedFirstPartyDomain_");
  }

  attrs.CreateSuffix(aStr);
}

namespace {

class MOZ_STACK_CLASS PopulateFromSuffixIterator final
    : public URLParams::ForEachIterator {
 public:
  explicit PopulateFromSuffixIterator(OriginAttributes* aOriginAttributes)
      : mOriginAttributes(aOriginAttributes) {
    MOZ_ASSERT(aOriginAttributes);
    // If mPrivateBrowsingId is passed in as >0 and is not present in the
    // suffix, then it will remain >0 when it should be 0 according to the
    // suffix. Set to 0 before iterating to fix this.
    mOriginAttributes->mPrivateBrowsingId = 0;
  }

  bool URLParamsIterator(const nsAString& aName,
                         const nsAString& aValue) override {
    if (aName.EqualsLiteral("inBrowser")) {
      if (!aValue.EqualsLiteral("1")) {
        return false;
      }

      mOriginAttributes->mInIsolatedMozBrowser = true;
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
      mOriginAttributes->mUserContextId = static_cast<uint32_t>(val);

      return true;
    }

    if (aName.EqualsLiteral("privateBrowsingId")) {
      nsresult rv;
      int64_t val = aValue.ToInteger64(&rv);
      NS_ENSURE_SUCCESS(rv, false);
      NS_ENSURE_TRUE(val >= 0 && val <= UINT32_MAX, false);
      mOriginAttributes->mPrivateBrowsingId = static_cast<uint32_t>(val);

      return true;
    }

    if (aName.EqualsLiteral("firstPartyDomain")) {
      MOZ_RELEASE_ASSERT(mOriginAttributes->mFirstPartyDomain.IsEmpty());
      mOriginAttributes->mFirstPartyDomain.Assign(aValue);
      return true;
    }

    // No other attributes are supported.
    return false;
  }

 private:
  OriginAttributes* mOriginAttributes;
};

}  // namespace

bool OriginAttributes::PopulateFromSuffix(const nsACString& aStr) {
  if (aStr.IsEmpty()) {
    return true;
  }

  if (aStr[0] != '^') {
    return false;
  }

  PopulateFromSuffixIterator iterator(this);
  return URLParams::Parse(Substring(aStr, 1, aStr.Length() - 1), iterator);
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

}  // namespace mozilla
