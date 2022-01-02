/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "nsIClassInfoImpl.h"
#include "nsIEffectiveTLDService.h"
#include "nsIHttpChannel.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIOService.h"
#include "nsIURL.h"

#include "nsWhitespaceTokenizer.h"
#include "nsAlgorithm.h"
#include "nsContentUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "ReferrerInfo.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/Telemetry.h"
#include "nsIWebProgressListener.h"

static mozilla::LazyLogModule gReferrerInfoLog("ReferrerInfo");
#define LOG(msg) MOZ_LOG(gReferrerInfoLog, mozilla::LogLevel::Debug, msg)
#define LOG_ENABLED() MOZ_LOG_TEST(gReferrerInfoLog, mozilla::LogLevel::Debug)

using namespace mozilla::net;

namespace mozilla {
namespace dom {

// Implementation of ClassInfo is required to serialize/deserialize
NS_IMPL_CLASSINFO(ReferrerInfo, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  REFERRERINFO_CID)

NS_IMPL_ISUPPORTS_CI(ReferrerInfo, nsIReferrerInfo, nsISerializable)

#define MAX_REFERRER_SENDING_POLICY 2
#define MAX_CROSS_ORIGIN_SENDING_POLICY 2
#define MAX_TRIMMING_POLICY 2

#define MIN_REFERRER_SENDING_POLICY 0
#define MIN_CROSS_ORIGIN_SENDING_POLICY 0
#define MIN_TRIMMING_POLICY 0

struct LegacyReferrerPolicyTokenMap {
  const char* mToken;
  ReferrerPolicy mPolicy;
};

/*
 * Parse ReferrerPolicy from token.
 * The supported tokens are defined in ReferrerPolicy.webidl.
 * The legacy tokens are "never", "default", "always" and
 * "origin-when-crossorigin". The legacy tokens are only supported in meta
 * referrer content
 *
 * @param aContent content string to be transformed into
 *                 ReferrerPolicyEnum, e.g. "origin".
 */
ReferrerPolicy ReferrerPolicyFromToken(const nsAString& aContent,
                                       bool allowedLegacyToken) {
  nsString lowerContent(aContent);
  ToLowerCase(lowerContent);

  if (allowedLegacyToken) {
    static const LegacyReferrerPolicyTokenMap sLegacyReferrerPolicyToken[] = {
        {"never", ReferrerPolicy::No_referrer},
        {"default", ReferrerPolicy::No_referrer_when_downgrade},
        {"always", ReferrerPolicy::Unsafe_url},
        {"origin-when-crossorigin", ReferrerPolicy::Origin_when_cross_origin},
    };

    uint8_t numStr = (sizeof(sLegacyReferrerPolicyToken) /
                      sizeof(sLegacyReferrerPolicyToken[0]));
    for (uint8_t i = 0; i < numStr; i++) {
      if (lowerContent.EqualsASCII(sLegacyReferrerPolicyToken[i].mToken)) {
        return sLegacyReferrerPolicyToken[i].mPolicy;
      }
    }
  }

  // Supported tokes - ReferrerPolicyValues, are generated from
  // ReferrerPolicy.webidl
  for (uint8_t i = 0; ReferrerPolicyValues::strings[i].value; i++) {
    if (lowerContent.EqualsASCII(ReferrerPolicyValues::strings[i].value)) {
      return static_cast<enum ReferrerPolicy>(i);
    }
  }

  // Return no referrer policy (empty string) if none of the previous match
  return ReferrerPolicy::_empty;
}

// static
ReferrerPolicy ReferrerInfo::ReferrerPolicyFromMetaString(
    const nsAString& aContent) {
  // This is implemented as described in
  // https://html.spec.whatwg.org/multipage/semantics.html#meta-referrer
  // Meta referrer accepts both supported tokens in ReferrerPolicy.webidl and
  // legacy tokens.
  return ReferrerPolicyFromToken(aContent, true);
}

// static
ReferrerPolicy ReferrerInfo::ReferrerPolicyAttributeFromString(
    const nsAString& aContent) {
  // This is implemented as described in
  // https://html.spec.whatwg.org/multipage/infrastructure.html#referrer-policy-attribute
  // referrerpolicy attribute only accepts supported tokens in
  // ReferrerPolicy.webidl
  return ReferrerPolicyFromToken(aContent, false);
}

// static
ReferrerPolicy ReferrerInfo::ReferrerPolicyFromHeaderString(
    const nsAString& aContent) {
  // Multiple headers could be concatenated into one comma-separated
  // list of policies. Need to tokenize the multiple headers.
  ReferrerPolicyEnum referrerPolicy = ReferrerPolicy::_empty;
  for (const auto& token : nsCharSeparatedTokenizer(aContent, ',').ToRange()) {
    if (token.IsEmpty()) {
      continue;
    }

    // Referrer-Policy header only accepts supported tokens in
    // ReferrerPolicy.webidl
    ReferrerPolicyEnum policy = ReferrerPolicyFromToken(token, false);
    // If there are multiple policies available, the last valid policy should be
    // used.
    // https://w3c.github.io/webappsec-referrer-policy/#unknown-policy-values
    if (policy != ReferrerPolicy::_empty) {
      referrerPolicy = policy;
    }
  }
  return referrerPolicy;
}

// static
const char* ReferrerInfo::ReferrerPolicyToString(ReferrerPolicyEnum aPolicy) {
  uint8_t index = static_cast<uint8_t>(aPolicy);
  uint8_t referrerPolicyCount = ArrayLength(ReferrerPolicyValues::strings);
  MOZ_ASSERT(index < referrerPolicyCount);
  if (index >= referrerPolicyCount) {
    return "";
  }

  return ReferrerPolicyValues::strings[index].value;
}

/* static */
uint32_t ReferrerInfo::GetUserReferrerSendingPolicy() {
  return clamped<uint32_t>(
      StaticPrefs::network_http_sendRefererHeader_DoNotUseDirectly(),
      MIN_REFERRER_SENDING_POLICY, MAX_REFERRER_SENDING_POLICY);
}

/* static */
uint32_t ReferrerInfo::GetUserXOriginSendingPolicy() {
  return clamped<uint32_t>(
      StaticPrefs::network_http_referer_XOriginPolicy_DoNotUseDirectly(),
      MIN_CROSS_ORIGIN_SENDING_POLICY, MAX_CROSS_ORIGIN_SENDING_POLICY);
}

/* static */
uint32_t ReferrerInfo::GetUserTrimmingPolicy() {
  return clamped<uint32_t>(
      StaticPrefs::network_http_referer_trimmingPolicy_DoNotUseDirectly(),
      MIN_TRIMMING_POLICY, MAX_TRIMMING_POLICY);
}

/* static */
uint32_t ReferrerInfo::GetUserXOriginTrimmingPolicy() {
  return clamped<uint32_t>(
      StaticPrefs::
          network_http_referer_XOriginTrimmingPolicy_DoNotUseDirectly(),
      MIN_TRIMMING_POLICY, MAX_TRIMMING_POLICY);
}

/* static */
ReferrerPolicy ReferrerInfo::GetDefaultReferrerPolicy(nsIHttpChannel* aChannel,
                                                      nsIURI* aURI,
                                                      bool privateBrowsing) {
  bool thirdPartyTrackerIsolated = false;
  if (aChannel && aURI) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    nsCOMPtr<nsICookieJarSettings> cjs;
    Unused << loadInfo->GetCookieJarSettings(getter_AddRefs(cjs));
    if (!cjs) {
      cjs = privateBrowsing
                ? net::CookieJarSettings::Create(CookieJarSettings::ePrivate)
                : net::CookieJarSettings::Create(CookieJarSettings::eRegular);
    }

    // We only check if the channel is isolated if it's in the parent process
    // with the rejection of third party contexts is enabled. We don't need to
    // check this in content processes since the tracking state of the channel
    // is unknown here and the referrer policy would be updated when the channel
    // starts connecting in the parent process.
    if (XRE_IsParentProcess() && cjs->GetRejectThirdPartyContexts()) {
      uint32_t rejectedReason = 0;
      thirdPartyTrackerIsolated =
          !ContentBlocking::ShouldAllowAccessFor(aChannel, aURI,
                                                 &rejectedReason) &&
          rejectedReason !=
              nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
      // Here we intentionally do not notify about the rejection reason, if any
      // in order to avoid this check to have any visible side-effects (e.g. a
      // web console report.)
    }
  }

  uint32_t defaultToUse;
  if (thirdPartyTrackerIsolated) {
    if (privateBrowsing) {
      defaultToUse =
          StaticPrefs::network_http_referer_defaultPolicy_trackers_pbmode();
    } else {
      defaultToUse = StaticPrefs::network_http_referer_defaultPolicy_trackers();
    }
  } else {
    if (privateBrowsing) {
      defaultToUse = StaticPrefs::network_http_referer_defaultPolicy_pbmode();
    } else {
      defaultToUse = StaticPrefs::network_http_referer_defaultPolicy();
    }
  }

  switch (defaultToUse) {
    case DefaultReferrerPolicy::eDefaultPolicyNoReferrer:
      return ReferrerPolicy::No_referrer;
    case DefaultReferrerPolicy::eDefaultPolicySameOrgin:
      return ReferrerPolicy::Same_origin;
    case DefaultReferrerPolicy::eDefaultPolicyStrictWhenXorigin:
      return ReferrerPolicy::Strict_origin_when_cross_origin;
  }

  return ReferrerPolicy::No_referrer_when_downgrade;
}

/* static */
bool ReferrerInfo::IsReferrerSchemeAllowed(nsIURI* aReferrer) {
  NS_ENSURE_TRUE(aReferrer, false);

  nsAutoCString scheme;
  nsresult rv = aReferrer->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return scheme.EqualsIgnoreCase("https") || scheme.EqualsIgnoreCase("http");
}

/* static */
bool ReferrerInfo::ShouldResponseInheritReferrerInfo(nsIChannel* aChannel) {
  if (!aChannel) {
    return false;
  }

  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(channelURI));
  NS_ENSURE_SUCCESS(rv, false);

  bool isAbout = channelURI->SchemeIs("about");
  if (!isAbout) {
    return false;
  }

  nsAutoCString aboutSpec;
  rv = channelURI->GetSpec(aboutSpec);
  NS_ENSURE_SUCCESS(rv, false);

  return aboutSpec.EqualsLiteral("about:srcdoc");
}

/* static */
nsresult ReferrerInfo::HandleSecureToInsecureReferral(
    nsIURI* aOriginalURI, nsIURI* aURI, ReferrerPolicyEnum aPolicy,
    bool& aAllowed) {
  NS_ENSURE_ARG(aOriginalURI);
  NS_ENSURE_ARG(aURI);

  aAllowed = false;

  bool referrerIsHttpsScheme = aOriginalURI->SchemeIs("https");
  if (!referrerIsHttpsScheme) {
    aAllowed = true;
    return NS_OK;
  }

  // It's ok to send referrer for https-to-http scenarios if the referrer
  // policy is "unsafe-url", "origin", or "origin-when-cross-origin".
  // in other referrer policies, https->http is not allowed...
  bool uriIsHttpsScheme = aURI->SchemeIs("https");
  if (aPolicy != ReferrerPolicy::Unsafe_url &&
      aPolicy != ReferrerPolicy::Origin_when_cross_origin &&
      aPolicy != ReferrerPolicy::Origin && !uriIsHttpsScheme) {
    return NS_OK;
  }

  aAllowed = true;
  return NS_OK;
}

nsresult ReferrerInfo::HandleUserXOriginSendingPolicy(nsIURI* aURI,
                                                      nsIURI* aReferrer,
                                                      bool& aAllowed) const {
  NS_ENSURE_ARG(aURI);
  aAllowed = false;

  nsAutoCString uriHost;
  nsAutoCString referrerHost;

  nsresult rv = aURI->GetAsciiHost(uriHost);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aReferrer->GetAsciiHost(referrerHost);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Send an empty referrer if xorigin and leaving a .onion domain.
  if (StaticPrefs::network_http_referer_hideOnionSource() &&
      !uriHost.Equals(referrerHost) &&
      StringEndsWith(referrerHost, ".onion"_ns)) {
    return NS_OK;
  }

  switch (GetUserXOriginSendingPolicy()) {
    // Check policy for sending referrer only when hosts match
    case XOriginSendingPolicy::ePolicySendWhenSameHost: {
      if (!uriHost.Equals(referrerHost)) {
        return NS_OK;
      }
      break;
    }

    case XOriginSendingPolicy::ePolicySendWhenSameDomain: {
      nsCOMPtr<nsIEffectiveTLDService> eTLDService =
          do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
      if (!eTLDService) {
        // check policy for sending only when effective top level domain
        // matches. this falls back on using host if eTLDService does not work
        if (!uriHost.Equals(referrerHost)) {
          return NS_OK;
        }
        break;
      }

      nsAutoCString uriDomain;
      nsAutoCString referrerDomain;
      uint32_t extraDomains = 0;

      rv = eTLDService->GetBaseDomain(aURI, extraDomains, uriDomain);
      if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
          rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        // uri is either an IP address, an alias such as 'localhost', an eTLD
        // such as 'co.uk', or the empty string. Uses the normalized host in
        // such cases.
        rv = aURI->GetAsciiHost(uriDomain);
      }

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = eTLDService->GetBaseDomain(aReferrer, extraDomains, referrerDomain);
      if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
          rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        // referrer is either an IP address, an alias such as 'localhost', an
        // eTLD such as 'co.uk', or the empty string. Uses the normalized host
        // in such cases.
        rv = aReferrer->GetAsciiHost(referrerDomain);
      }

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!uriDomain.Equals(referrerDomain)) {
        return NS_OK;
      }
      break;
    }

    default:
      break;
  }

  aAllowed = true;
  return NS_OK;
}

/* static */
bool ReferrerInfo::ShouldSetNullOriginHeader(net::HttpBaseChannel* aChannel,
                                             nsIURI* aOriginURI) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aOriginURI);

  if (StaticPrefs::network_http_referer_hideOnionSource()) {
    nsAutoCString host;
    if (NS_SUCCEEDED(aOriginURI->GetAsciiHost(host)) &&
        StringEndsWith(host, ".onion"_ns)) {
      return ReferrerInfo::IsCrossOriginRequest(aChannel);
    }
  }

  // When we're dealing with CORS (mode is "cors"), we shouldn't take the
  // Referrer-Policy into account
  uint32_t corsMode = CORS_NONE;
  NS_ENSURE_SUCCESS(aChannel->GetCorsMode(&corsMode), false);
  if (corsMode == CORS_USE_CREDENTIALS) {
    return false;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  NS_ENSURE_SUCCESS(aChannel->GetReferrerInfo(getter_AddRefs(referrerInfo)),
                    false);
  if (!referrerInfo) {
    return false;
  }
  enum ReferrerPolicy policy = referrerInfo->ReferrerPolicy();
  if (policy == ReferrerPolicy::No_referrer) {
    return true;
  }

  bool allowed = false;
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(aChannel->GetURI(getter_AddRefs(uri)), false);

  if (NS_SUCCEEDED(ReferrerInfo::HandleSecureToInsecureReferral(
          aOriginURI, uri, policy, allowed)) &&
      !allowed) {
    return true;
  }

  if (policy == ReferrerPolicy::Same_origin) {
    return ReferrerInfo::IsCrossOriginRequest(aChannel);
  }

  return false;
}

nsresult ReferrerInfo::HandleUserReferrerSendingPolicy(nsIHttpChannel* aChannel,
                                                       bool& aAllowed) const {
  aAllowed = false;
  uint32_t referrerSendingPolicy;
  uint32_t loadFlags;
  nsresult rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (loadFlags & nsIHttpChannel::LOAD_INITIAL_DOCUMENT_URI) {
    referrerSendingPolicy = ReferrerSendingPolicy::ePolicySendWhenUserTrigger;
  } else {
    referrerSendingPolicy = ReferrerSendingPolicy::ePolicySendInlineContent;
  }
  if (GetUserReferrerSendingPolicy() < referrerSendingPolicy) {
    return NS_OK;
  }

  aAllowed = true;
  return NS_OK;
}

/* static */
bool ReferrerInfo::IsCrossOriginRequest(nsIHttpChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  if (!loadInfo->TriggeringPrincipal()->GetIsContentPrincipal()) {
    LOG(("no triggering URI via loadInfo, assuming load is cross-origin"));
    return true;
  }

  if (LOG_ENABLED()) {
    nsAutoCString triggeringURISpec;
    loadInfo->TriggeringPrincipal()->GetAsciiSpec(triggeringURISpec);
    LOG(("triggeringURI=%s\n", triggeringURISpec.get()));
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  bool isPrivateWin = loadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
  bool isSameOrigin = false;
  rv = loadInfo->TriggeringPrincipal()->IsSameOrigin(uri, isPrivateWin,
                                                     &isSameOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }
  return !isSameOrigin;
}

/* static */
bool ReferrerInfo::IsCrossSiteRequest(nsIHttpChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  if (!loadInfo->TriggeringPrincipal()->GetIsContentPrincipal()) {
    LOG(("no triggering URI via loadInfo, assuming load is cross-site"));
    return true;
  }

  if (LOG_ENABLED()) {
    nsAutoCString triggeringURISpec;
    loadInfo->TriggeringPrincipal()->GetAsciiSpec(triggeringURISpec);
    LOG(("triggeringURI=%s\n", triggeringURISpec.get()));
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  bool isCrossSite = true;
  rv = loadInfo->TriggeringPrincipal()->IsThirdPartyURI(uri, &isCrossSite);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  return isCrossSite;
}

ReferrerInfo::TrimmingPolicy ReferrerInfo::ComputeTrimmingPolicy(
    nsIHttpChannel* aChannel) const {
  uint32_t trimmingPolicy = GetUserTrimmingPolicy();

  switch (mPolicy) {
    case ReferrerPolicy::Origin:
    case ReferrerPolicy::Strict_origin:
      trimmingPolicy = TrimmingPolicy::ePolicySchemeHostPort;
      break;

    case ReferrerPolicy::Origin_when_cross_origin:
    case ReferrerPolicy::Strict_origin_when_cross_origin:
      if (trimmingPolicy != TrimmingPolicy::ePolicySchemeHostPort &&
          IsCrossOriginRequest(aChannel)) {
        // Ignore set trimmingPolicy if it is already the strictest
        // policy.
        trimmingPolicy = TrimmingPolicy::ePolicySchemeHostPort;
      }
      break;

    // This function is called when a nonempty referrer value is allowed to
    // send. For the next 3 policies: same-origin, no-referrer-when-downgrade,
    // unsafe-url, without trimming we should have a full uri. And the trimming
    // policy only depends on user prefs.
    case ReferrerPolicy::Same_origin:
    case ReferrerPolicy::No_referrer_when_downgrade:
    case ReferrerPolicy::Unsafe_url:
      if (trimmingPolicy != TrimmingPolicy::ePolicySchemeHostPort) {
        // Ignore set trimmingPolicy if it is already the strictest
        // policy. Apply the user cross-origin trimming policy if it's more
        // restrictive than the general one.
        if (GetUserXOriginTrimmingPolicy() != TrimmingPolicy::ePolicyFullURI &&
            IsCrossOriginRequest(aChannel)) {
          trimmingPolicy =
              std::max(trimmingPolicy, GetUserXOriginTrimmingPolicy());
        }
      }
      break;

    case ReferrerPolicy::No_referrer:
    case ReferrerPolicy::_empty:
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected value");
      break;
  }

  return static_cast<TrimmingPolicy>(trimmingPolicy);
}

nsresult ReferrerInfo::LimitReferrerLength(
    nsIHttpChannel* aChannel, nsIURI* aReferrer, TrimmingPolicy aTrimmingPolicy,
    nsACString& aInAndOutTrimmedReferrer) const {
  if (!StaticPrefs::network_http_referer_referrerLengthLimit()) {
    return NS_OK;
  }

  if (aInAndOutTrimmedReferrer.Length() <=
      StaticPrefs::network_http_referer_referrerLengthLimit()) {
    return NS_OK;
  }

  nsAutoString referrerLengthLimit;
  referrerLengthLimit.AppendInt(
      StaticPrefs::network_http_referer_referrerLengthLimit());
  if (aTrimmingPolicy == ePolicyFullURI ||
      aTrimmingPolicy == ePolicySchemeHostPortPath) {
    // If referrer header is over max Length, down to origin
    nsresult rv = GetOriginFromReferrerURI(aReferrer, aInAndOutTrimmedReferrer);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Step 6 within https://w3c.github.io/webappsec-referrer-policy/#strip-url
    // states that the trailing "/" does not need to get stripped. However,
    // GetOriginFromReferrerURI() also removes any trailing "/" hence we have to
    // add it back here.
    aInAndOutTrimmedReferrer.AppendLiteral("/");
    if (aInAndOutTrimmedReferrer.Length() <=
        StaticPrefs::network_http_referer_referrerLengthLimit()) {
      AutoTArray<nsString, 2> params = {
          referrerLengthLimit, NS_ConvertUTF8toUTF16(aInAndOutTrimmedReferrer)};
      LogMessageToConsole(aChannel, "ReferrerLengthOverLimitation", params);
      return NS_OK;
    }
  }

  // If we end up here either the trimmingPolicy is equal to
  // 'ePolicySchemeHostPort' or the 'origin' of any other policy is still over
  // the length limit. If so, truncate the referrer entirely.
  AutoTArray<nsString, 2> params = {
      referrerLengthLimit, NS_ConvertUTF8toUTF16(aInAndOutTrimmedReferrer)};
  LogMessageToConsole(aChannel, "ReferrerOriginLengthOverLimitation", params);
  aInAndOutTrimmedReferrer.Truncate();

  return NS_OK;
}

nsresult ReferrerInfo::GetOriginFromReferrerURI(nsIURI* aReferrer,
                                                nsACString& aResult) const {
  MOZ_ASSERT(aReferrer);
  aResult.Truncate();
  // We want the IDN-normalized PrePath. That's not something currently
  // available and there doesn't yet seem to be justification for adding it to
  // the interfaces, so just build it up from scheme+AsciiHostPort
  nsAutoCString scheme, asciiHostPort;
  nsresult rv = aReferrer->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aResult = scheme;
  aResult.AppendLiteral("://");
  // Note we explicitly cleared UserPass above, so do not need to build it.
  rv = aReferrer->GetAsciiHostPort(asciiHostPort);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aResult.Append(asciiHostPort);
  return NS_OK;
}

nsresult ReferrerInfo::TrimReferrerWithPolicy(nsIURI* aReferrer,
                                              TrimmingPolicy aTrimmingPolicy,
                                              nsACString& aResult) const {
  MOZ_ASSERT(aReferrer);

  if (aTrimmingPolicy == TrimmingPolicy::ePolicyFullURI) {
    return aReferrer->GetAsciiSpec(aResult);
  }

  nsresult rv = GetOriginFromReferrerURI(aReferrer, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aTrimmingPolicy == TrimmingPolicy::ePolicySchemeHostPortPath) {
    nsCOMPtr<nsIURL> url(do_QueryInterface(aReferrer));
    if (url) {
      nsAutoCString path;
      rv = url->GetFilePath(path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      aResult.Append(path);
      return NS_OK;
    }
  }

  // Step 6 within https://w3c.github.io/webappsec-referrer-policy/#strip-url
  // states that the trailing "/" does not need to get stripped. However,
  // GetOriginFromReferrerURI() also removes any trailing "/" hence we have to
  // add it back here.
  aResult.AppendLiteral("/");
  return NS_OK;
}

bool ReferrerInfo::ShouldIgnoreLessRestrictedPolicies(
    nsIHttpChannel* aChannel, const ReferrerPolicyEnum aPolicy) const {
  MOZ_ASSERT(aChannel);

  // We only care about the less restricted policies.
  if (aPolicy != ReferrerPolicy::Unsafe_url &&
      aPolicy != ReferrerPolicy::No_referrer_when_downgrade &&
      aPolicy != ReferrerPolicy::Origin_when_cross_origin) {
    return false;
  }

  bool isCrossSite = IsCrossSiteRequest(aChannel);
  bool isPrivate = NS_UsePrivateBrowsing(aChannel);
  bool isEnabled =
      isPrivate
          ? StaticPrefs::
                network_http_referer_disallowCrossSiteRelaxingDefault_pbmode()
          : StaticPrefs::
                network_http_referer_disallowCrossSiteRelaxingDefault();

  if (!isEnabled) {
    // Log the warning message to console to inform that we will ignore
    // less restricted policies for cross-site requests in the future.
    if (isCrossSite) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, false);

      AutoTArray<nsString, 1> params = {
          NS_ConvertUTF8toUTF16(uri->GetSpecOrDefault())};
      LogMessageToConsole(aChannel, "ReferrerPolicyDisallowRelaxingWarning",
                          params);
    }
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  // Check if the channel is triggered by the system or the extension.
  auto* triggerBasePrincipal =
      BasePrincipal::Cast(loadInfo->TriggeringPrincipal());
  if (triggerBasePrincipal->IsSystemPrincipal() ||
      triggerBasePrincipal->AddonPolicy()) {
    return false;
  }

  if (isCrossSite) {
    // Log the console message to say that the less restricted policy was
    // ignored.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, true);

    uint32_t idx = static_cast<uint32_t>(aPolicy);

    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(
            nsDependentCString(ReferrerPolicyValues::strings[idx].value)),
        NS_ConvertUTF8toUTF16(uri->GetSpecOrDefault())};
    LogMessageToConsole(aChannel, "ReferrerPolicyDisallowRelaxingMessage",
                        params);
  }

  return isCrossSite;
}

void ReferrerInfo::LogMessageToConsole(
    nsIHttpChannel* aChannel, const char* aMsg,
    const nsTArray<nsString>& aParams) const {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uint64_t windowID = 0;

  rv = aChannel->GetTopLevelContentWindowId(&windowID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (!windowID) {
    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (loadGroup) {
      windowID = nsContentUtils::GetInnerWindowID(loadGroup);
    }
  }

  nsAutoString localizedMsg;
  rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eSECURITY_PROPERTIES, aMsg, aParams, localizedMsg);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = nsContentUtils::ReportToConsoleByWindowID(
      localizedMsg, nsIScriptError::infoFlag, "Security"_ns, windowID, uri);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

ReferrerPolicy ReferrerPolicyIDLToReferrerPolicy(
    nsIReferrerInfo::ReferrerPolicyIDL aReferrerPolicy) {
  switch (aReferrerPolicy) {
    case nsIReferrerInfo::EMPTY:
      return ReferrerPolicy::_empty;
      break;
    case nsIReferrerInfo::NO_REFERRER:
      return ReferrerPolicy::No_referrer;
      break;
    case nsIReferrerInfo::NO_REFERRER_WHEN_DOWNGRADE:
      return ReferrerPolicy::No_referrer_when_downgrade;
      break;
    case nsIReferrerInfo::ORIGIN:
      return ReferrerPolicy::Origin;
      break;
    case nsIReferrerInfo::ORIGIN_WHEN_CROSS_ORIGIN:
      return ReferrerPolicy::Origin_when_cross_origin;
      break;
    case nsIReferrerInfo::UNSAFE_URL:
      return ReferrerPolicy::Unsafe_url;
      break;
    case nsIReferrerInfo::SAME_ORIGIN:
      return ReferrerPolicy::Same_origin;
      break;
    case nsIReferrerInfo::STRICT_ORIGIN:
      return ReferrerPolicy::Strict_origin;
      break;
    case nsIReferrerInfo::STRICT_ORIGIN_WHEN_CROSS_ORIGIN:
      return ReferrerPolicy::Strict_origin_when_cross_origin;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid ReferrerPolicy value");
      break;
  }

  return ReferrerPolicy::_empty;
}

nsIReferrerInfo::ReferrerPolicyIDL ReferrerPolicyToReferrerPolicyIDL(
    ReferrerPolicy aReferrerPolicy) {
  switch (aReferrerPolicy) {
    case ReferrerPolicy::_empty:
      return nsIReferrerInfo::EMPTY;
      break;
    case ReferrerPolicy::No_referrer:
      return nsIReferrerInfo::NO_REFERRER;
      break;
    case ReferrerPolicy::No_referrer_when_downgrade:
      return nsIReferrerInfo::NO_REFERRER_WHEN_DOWNGRADE;
      break;
    case ReferrerPolicy::Origin:
      return nsIReferrerInfo::ORIGIN;
      break;
    case ReferrerPolicy::Origin_when_cross_origin:
      return nsIReferrerInfo::ORIGIN_WHEN_CROSS_ORIGIN;
      break;
    case ReferrerPolicy::Unsafe_url:
      return nsIReferrerInfo::UNSAFE_URL;
      break;
    case ReferrerPolicy::Same_origin:
      return nsIReferrerInfo::SAME_ORIGIN;
      break;
    case ReferrerPolicy::Strict_origin:
      return nsIReferrerInfo::STRICT_ORIGIN;
      break;
    case ReferrerPolicy::Strict_origin_when_cross_origin:
      return nsIReferrerInfo::STRICT_ORIGIN_WHEN_CROSS_ORIGIN;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid ReferrerPolicy value");
      break;
  }

  return nsIReferrerInfo::EMPTY;
}

ReferrerInfo::ReferrerInfo()
    : mOriginalReferrer(nullptr),
      mPolicy(ReferrerPolicy::_empty),
      mSendReferrer(true),
      mInitialized(false),
      mOverridePolicyByDefault(false) {}

ReferrerInfo::ReferrerInfo(const Document& aDoc) : ReferrerInfo() {
  InitWithDocument(&aDoc);
}

ReferrerInfo::ReferrerInfo(const Element& aElement) : ReferrerInfo() {
  InitWithElement(&aElement);
}

ReferrerInfo::ReferrerInfo(nsIURI* aOriginalReferrer,
                           ReferrerPolicyEnum aPolicy, bool aSendReferrer,
                           const Maybe<nsCString>& aComputedReferrer)
    : mOriginalReferrer(aOriginalReferrer),
      mPolicy(aPolicy),
      mSendReferrer(aSendReferrer),
      mInitialized(true),
      mOverridePolicyByDefault(false),
      mComputedReferrer(aComputedReferrer) {}

ReferrerInfo::ReferrerInfo(const ReferrerInfo& rhs)
    : mOriginalReferrer(rhs.mOriginalReferrer),
      mPolicy(rhs.mPolicy),
      mSendReferrer(rhs.mSendReferrer),
      mInitialized(rhs.mInitialized),
      mOverridePolicyByDefault(rhs.mOverridePolicyByDefault),
      mComputedReferrer(rhs.mComputedReferrer) {}

already_AddRefed<ReferrerInfo> ReferrerInfo::Clone() const {
  RefPtr<ReferrerInfo> copy(new ReferrerInfo(*this));
  return copy.forget();
}

already_AddRefed<ReferrerInfo> ReferrerInfo::CloneWithNewPolicy(
    ReferrerPolicyEnum aPolicy) const {
  RefPtr<ReferrerInfo> copy(new ReferrerInfo(*this));
  copy->mPolicy = aPolicy;
  return copy.forget();
}

already_AddRefed<ReferrerInfo> ReferrerInfo::CloneWithNewSendReferrer(
    bool aSendReferrer) const {
  RefPtr<ReferrerInfo> copy(new ReferrerInfo(*this));
  copy->mSendReferrer = aSendReferrer;
  return copy.forget();
}

already_AddRefed<ReferrerInfo> ReferrerInfo::CloneWithNewOriginalReferrer(
    nsIURI* aOriginalReferrer) const {
  RefPtr<ReferrerInfo> copy(new ReferrerInfo(*this));
  copy->mOriginalReferrer = aOriginalReferrer;
  return copy.forget();
}

NS_IMETHODIMP
ReferrerInfo::GetOriginalReferrer(nsIURI** aOriginalReferrer) {
  *aOriginalReferrer = mOriginalReferrer;
  NS_IF_ADDREF(*aOriginalReferrer);
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetReferrerPolicy(
    JSContext* aCx, nsIReferrerInfo::ReferrerPolicyIDL* aReferrerPolicy) {
  *aReferrerPolicy = ReferrerPolicyToReferrerPolicyIDL(mPolicy);
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetReferrerPolicyString(nsACString& aResult) {
  aResult.AssignASCII(ReferrerPolicyToString(mPolicy));
  return NS_OK;
}

ReferrerPolicy ReferrerInfo::ReferrerPolicy() { return mPolicy; }

NS_IMETHODIMP
ReferrerInfo::GetSendReferrer(bool* aSendReferrer) {
  *aSendReferrer = mSendReferrer;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::Equals(nsIReferrerInfo* aOther, bool* aResult) {
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);
  MOZ_ASSERT(mInitialized);
  if (aOther == this) {
    *aResult = true;
    return NS_OK;
  }

  *aResult = false;
  ReferrerInfo* other = static_cast<ReferrerInfo*>(aOther);
  MOZ_ASSERT(other->mInitialized);

  if (mPolicy != other->mPolicy || mSendReferrer != other->mSendReferrer ||
      mOverridePolicyByDefault != other->mOverridePolicyByDefault ||
      mComputedReferrer != other->mComputedReferrer) {
    return NS_OK;
  }

  if (!mOriginalReferrer != !other->mOriginalReferrer) {
    // One or the other has mOriginalReferrer, but not both... not equal
    return NS_OK;
  }

  bool originalReferrerEquals;
  if (mOriginalReferrer &&
      (NS_FAILED(mOriginalReferrer->Equals(other->mOriginalReferrer,
                                           &originalReferrerEquals)) ||
       !originalReferrerEquals)) {
    return NS_OK;
  }

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetComputedReferrerSpec(nsAString& aComputedReferrerSpec) {
  aComputedReferrerSpec.Assign(
      mComputedReferrer.isSome()
          ? NS_ConvertUTF8toUTF16(mComputedReferrer.value())
          : EmptyString());
  return NS_OK;
}

already_AddRefed<nsIURI> ReferrerInfo::GetComputedReferrer() {
  if (!mComputedReferrer.isSome() || mComputedReferrer.value().IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> result;
  nsresult rv = NS_NewURI(getter_AddRefs(result), mComputedReferrer.value());
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return result.forget();
}

HashNumber ReferrerInfo::Hash() const {
  MOZ_ASSERT(mInitialized);
  nsAutoCString originalReferrerSpec;
  if (mOriginalReferrer) {
    Unused << mOriginalReferrer->GetSpec(originalReferrerSpec);
  }

  return mozilla::AddToHash(
      static_cast<uint32_t>(mPolicy), mSendReferrer, mOverridePolicyByDefault,
      mozilla::HashString(originalReferrerSpec),
      mozilla::HashString(mComputedReferrer.isSome() ? mComputedReferrer.value()
                                                     : ""_ns));
}

NS_IMETHODIMP
ReferrerInfo::Init(nsIReferrerInfo::ReferrerPolicyIDL aReferrerPolicy,
                   bool aSendReferrer, nsIURI* aOriginalReferrer) {
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  };

  mPolicy = ReferrerPolicyIDLToReferrerPolicy(aReferrerPolicy);
  mSendReferrer = aSendReferrer;
  mOriginalReferrer = aOriginalReferrer;
  mInitialized = true;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::InitWithDocument(const Document* aDocument) {
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  };

  mPolicy = aDocument->GetReferrerPolicy();
  mSendReferrer = true;
  mOriginalReferrer = aDocument->GetDocumentURIAsReferrer();
  mInitialized = true;
  return NS_OK;
}

/**
 * Check whether the given node has referrerpolicy attribute and parse
 * referrer policy from the attribute.
 * Currently, referrerpolicy attribute is supported in a, area, img, iframe,
 * script, or link element.
 */
static ReferrerPolicy ReferrerPolicyFromAttribute(const Element& aElement) {
  if (!aElement.IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area,
                                    nsGkAtoms::script, nsGkAtoms::iframe,
                                    nsGkAtoms::link, nsGkAtoms::img)) {
    return ReferrerPolicy::_empty;
  }
  return aElement.GetReferrerPolicyAsEnum();
}

static bool HasRelNoReferrer(const Element& aElement) {
  // rel=noreferrer is only support in <a> and <area>
  if (!aElement.IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area)) {
    return false;
  }

  nsAutoString rel;
  aElement.GetAttr(nsGkAtoms::rel, rel);
  nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tok(rel);

  while (tok.hasMoreTokens()) {
    const nsAString& token = tok.nextToken();
    if (token.LowerCaseEqualsLiteral("noreferrer")) {
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
ReferrerInfo::InitWithElement(const Element* aElement) {
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  };

  // Referrer policy from referrerpolicy attribute will have a higher priority
  // than referrer policy from <meta> tag and Referrer-Policy header.
  mPolicy = ReferrerPolicyFromAttribute(*aElement);
  if (mPolicy == ReferrerPolicy::_empty) {
    // Fallback to use document's referrer poicy if we don't have referrer
    // policy from attribute.
    mPolicy = aElement->OwnerDoc()->GetReferrerPolicy();
  }

  mSendReferrer = !HasRelNoReferrer(*aElement);
  mOriginalReferrer = aElement->OwnerDoc()->GetDocumentURIAsReferrer();

  mInitialized = true;
  return NS_OK;
}

/* static */
already_AddRefed<nsIReferrerInfo>
ReferrerInfo::CreateFromOtherAndPolicyOverride(
    nsIReferrerInfo* aOther, ReferrerPolicyEnum aPolicyOverride) {
  MOZ_ASSERT(aOther);
  ReferrerPolicyEnum policy = aPolicyOverride != ReferrerPolicy::_empty
                                  ? aPolicyOverride
                                  : aOther->ReferrerPolicy();

  nsCOMPtr<nsIURI> referrer = aOther->GetComputedReferrer();
  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      new ReferrerInfo(referrer, policy, aOther->GetSendReferrer());
  return referrerInfo.forget();
}

/* static */
already_AddRefed<nsIReferrerInfo>
ReferrerInfo::CreateFromDocumentAndPolicyOverride(
    Document* aDoc, ReferrerPolicyEnum aPolicyOverride) {
  MOZ_ASSERT(aDoc);
  ReferrerPolicyEnum policy = aPolicyOverride != ReferrerPolicy::_empty
                                  ? aPolicyOverride
                                  : aDoc->GetReferrerPolicy();
  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      new ReferrerInfo(aDoc->GetDocumentURIAsReferrer(), policy);
  return referrerInfo.forget();
}

/* static */
already_AddRefed<nsIReferrerInfo> ReferrerInfo::CreateForFetch(
    nsIPrincipal* aPrincipal, Document* aDoc) {
  MOZ_ASSERT(aPrincipal);

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (!aPrincipal || aPrincipal->IsSystemPrincipal()) {
    referrerInfo = new ReferrerInfo(nullptr);
    return referrerInfo.forget();
  }

  if (!aDoc) {
    aPrincipal->CreateReferrerInfo(ReferrerPolicy::_empty,
                                   getter_AddRefs(referrerInfo));
    return referrerInfo.forget();
  }

  // If it weren't for history.push/replaceState, we could just use the
  // principal's URI here.  But since we want changes to the URI effected
  // by push/replaceState to be reflected in the XHR referrer, we have to
  // be more clever.
  //
  // If the document's original URI (before any push/replaceStates) matches
  // our principal, then we use the document's current URI (after
  // push/replaceStates).  Otherwise (if the document is, say, a data:
  // URI), we just use the principal's URI.
  nsCOMPtr<nsIURI> docCurURI = aDoc->GetDocumentURI();
  nsCOMPtr<nsIURI> docOrigURI = aDoc->GetOriginalURI();

  if (docCurURI && docOrigURI) {
    bool equal = false;
    aPrincipal->EqualsURI(docOrigURI, &equal);
    if (equal) {
      referrerInfo = new ReferrerInfo(docCurURI, aDoc->GetReferrerPolicy());
      return referrerInfo.forget();
    }
  }
  aPrincipal->CreateReferrerInfo(aDoc->GetReferrerPolicy(),
                                 getter_AddRefs(referrerInfo));
  return referrerInfo.forget();
}

/* static */
already_AddRefed<nsIReferrerInfo> ReferrerInfo::CreateForExternalCSSResources(
    mozilla::StyleSheet* aExternalSheet, ReferrerPolicyEnum aPolicy) {
  MOZ_ASSERT(aExternalSheet && !aExternalSheet->IsInline());
  nsCOMPtr<nsIReferrerInfo> referrerInfo;

  // Step 2
  // https://w3c.github.io/webappsec-referrer-policy/#integration-with-css
  // Use empty policy at the beginning and update it later from Referrer-Policy
  // header.
  referrerInfo = new ReferrerInfo(aExternalSheet->GetSheetURI(), aPolicy);
  return referrerInfo.forget();
}

/* static */
already_AddRefed<nsIReferrerInfo> ReferrerInfo::CreateForInternalCSSResources(
    Document* aDocument) {
  MOZ_ASSERT(aDocument);
  nsCOMPtr<nsIReferrerInfo> referrerInfo;

  referrerInfo = new ReferrerInfo(aDocument->GetDocumentURI(),
                                  aDocument->GetReferrerPolicy());
  return referrerInfo.forget();
}

// Bug 1415044 to investigate which referrer and policy we should use
/* static */
already_AddRefed<nsIReferrerInfo> ReferrerInfo::CreateForSVGResources(
    Document* aDocument) {
  MOZ_ASSERT(aDocument);
  nsCOMPtr<nsIReferrerInfo> referrerInfo;

  referrerInfo = new ReferrerInfo(aDocument->GetDocumentURI(),
                                  aDocument->GetReferrerPolicy());
  return referrerInfo.forget();
}

nsresult ReferrerInfo::ComputeReferrer(nsIHttpChannel* aChannel) {
  NS_ENSURE_ARG(aChannel);
  MOZ_ASSERT(NS_IsMainThread());

  // If the referrerInfo is passed around when redirect, just use the last
  // computedReferrer to recompute
  nsCOMPtr<nsIURI> referrer;
  nsresult rv = NS_OK;
  mOverridePolicyByDefault = false;

  if (mComputedReferrer.isSome()) {
    if (mComputedReferrer.value().IsEmpty()) {
      return NS_OK;
    }

    rv = NS_NewURI(getter_AddRefs(referrer), mComputedReferrer.value());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mComputedReferrer.reset();
  // Emplace mComputedReferrer with an empty string, which means we have
  // computed the referrer and the result referrer value is empty (not send
  // referrer). So any early return later than this line will use that empty
  // referrer.
  mComputedReferrer.emplace(""_ns);

  if (!mSendReferrer || !mOriginalReferrer ||
      mPolicy == ReferrerPolicy::No_referrer) {
    return NS_OK;
  }

  if (mPolicy == ReferrerPolicy::_empty ||
      ShouldIgnoreLessRestrictedPolicies(aChannel, mPolicy)) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    OriginAttributes attrs = loadInfo->GetOriginAttributes();
    bool isPrivate = attrs.mPrivateBrowsingId > 0;

    nsCOMPtr<nsIURI> uri;
    rv = aChannel->GetURI(getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPolicy = GetDefaultReferrerPolicy(aChannel, uri, isPrivate);
    mOverridePolicyByDefault = true;
  }

  if (mPolicy == ReferrerPolicy::No_referrer) {
    return NS_OK;
  }

  bool isUserReferrerSendingAllowed = false;
  rv = HandleUserReferrerSendingPolicy(aChannel, isUserReferrerSendingAllowed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isUserReferrerSendingAllowed) {
    return NS_OK;
  }

  // Enforce Referrer allowlist, only http, https scheme are allowed
  if (!IsReferrerSchemeAllowed(mOriginalReferrer)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isSecureToInsecureAllowed = false;
  rv = HandleSecureToInsecureReferral(mOriginalReferrer, uri, mPolicy,
                                      isSecureToInsecureAllowed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isSecureToInsecureAllowed) {
    return NS_OK;
  }

  // Don't send referrer when the request is cross-origin and policy is
  // "same-origin".
  if (mPolicy == ReferrerPolicy::Same_origin &&
      IsCrossOriginRequest(aChannel)) {
    return NS_OK;
  }

  // Strip away any fragment per RFC 2616 section 14.36
  // and Referrer Policy section 6.3.5.
  if (!referrer) {
    rv = NS_GetURIWithoutRef(mOriginalReferrer, getter_AddRefs(referrer));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool isUserXOriginAllowed = false;
  rv = HandleUserXOriginSendingPolicy(uri, referrer, isUserXOriginAllowed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isUserXOriginAllowed) {
    return NS_OK;
  }

  // Handle user pref network.http.referer.spoofSource, send spoofed referrer if
  // desired
  if (StaticPrefs::network_http_referer_spoofSource()) {
    nsCOMPtr<nsIURI> userSpoofReferrer;
    rv = NS_GetURIWithoutRef(uri, getter_AddRefs(userSpoofReferrer));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    referrer = userSpoofReferrer;
  }

  // strip away any userpass; we don't want to be giving out passwords ;-)
  // This is required by Referrer Policy stripping algorithm.
  nsCOMPtr<nsIURI> exposableURI = nsIOService::CreateExposableURI(referrer);
  referrer = exposableURI;

  TrimmingPolicy trimmingPolicy = ComputeTrimmingPolicy(aChannel);

  nsAutoCString trimmedReferrer;
  // We first trim the referrer according to the policy by calling
  // 'TrimReferrerWithPolicy' and right after we have to call
  // 'LimitReferrerLength' (using the same arguments) because the trimmed
  // referrer might exceed the allowed max referrer length.
  rv = TrimReferrerWithPolicy(referrer, trimmingPolicy, trimmedReferrer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = LimitReferrerLength(aChannel, referrer, trimmingPolicy, trimmedReferrer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // finally, remember the referrer spec.
  mComputedReferrer.reset();
  mComputedReferrer.emplace(trimmedReferrer);

  return NS_OK;
}

/* ===== nsISerializable implementation ====== */

NS_IMETHODIMP
ReferrerInfo::Read(nsIObjectInputStream* aStream) {
  bool nonNull;
  nsresult rv = aStream->ReadBoolean(&nonNull);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (nonNull) {
    nsAutoCString spec;
    nsresult rv = aStream->ReadCString(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = NS_NewURI(getter_AddRefs(mOriginalReferrer), spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    mOriginalReferrer = nullptr;
  }

  // ReferrerPolicy.webidl has different order with ReferrerPolicyIDL. We store
  // to disk using the order of ReferrerPolicyIDL, so we convert to
  // ReferrerPolicyIDL to make it be compatible to the old format.
  uint32_t policy;
  rv = aStream->Read32(&policy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mPolicy = ReferrerPolicyIDLToReferrerPolicy(
      static_cast<nsIReferrerInfo::ReferrerPolicyIDL>(policy));

  rv = aStream->ReadBoolean(&mSendReferrer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isComputed;
  rv = aStream->ReadBoolean(&isComputed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isComputed) {
    nsAutoCString computedReferrer;
    rv = aStream->ReadCString(computedReferrer);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    mComputedReferrer.emplace(computedReferrer);
  }

  rv = aStream->ReadBoolean(&mInitialized);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->ReadBoolean(&mOverridePolicyByDefault);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::Write(nsIObjectOutputStream* aStream) {
  bool nonNull = (mOriginalReferrer != nullptr);
  nsresult rv = aStream->WriteBoolean(nonNull);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (nonNull) {
    nsAutoCString spec;
    nsresult rv = mOriginalReferrer->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aStream->WriteStringZ(spec.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aStream->Write32(ReferrerPolicyToReferrerPolicyIDL(mPolicy));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteBoolean(mSendReferrer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isComputed = mComputedReferrer.isSome();
  rv = aStream->WriteBoolean(isComputed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isComputed) {
    rv = aStream->WriteStringZ(mComputedReferrer.value().get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aStream->WriteBoolean(mInitialized);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteBoolean(mOverridePolicyByDefault);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

void ReferrerInfo::RecordTelemetry(nsIHttpChannel* aChannel) {
#ifdef DEBUG
  MOZ_ASSERT(!mTelemetryRecorded);
  mTelemetryRecorded = true;
#endif  // DEBUG

  // The telemetry probe has 18 buckets. The first 9 buckets are for same-site
  // requests and the rest 9 buckets are for cross-site requests.
  uint32_t telemetryOffset =
      IsCrossSiteRequest(aChannel)
          ? static_cast<uint32_t>(ReferrerPolicy::EndGuard_)
          : 0;

  Telemetry::Accumulate(Telemetry::REFERRER_POLICY_COUNT,
                        static_cast<uint32_t>(mPolicy) + telemetryOffset);
}

}  // namespace dom
}  // namespace mozilla
