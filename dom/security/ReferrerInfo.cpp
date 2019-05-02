/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsICookieService.h"
#include "nsIHttpChannel.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIURIFixup.h"
#include "nsIURL.h"
#include "nsIURIMutator.h"

#include "nsAlgorithm.h"
#include "ReferrerInfo.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/HttpBaseChannel.h"

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

#define DEFAULT_RP 3
#define DEFAULT_TRACKER_RP 3
#define DEFAULT_PRIVATE_RP 2
#define DEFAULT_TRACKER_PRIVATE_RP 2

#define MAX_REFERRER_SENDING_POLICY 2
#define MAX_CROSS_ORIGIN_SENDING_POLICY 2
#define MAX_TRIMMING_POLICY 2

#define MIN_REFERRER_SENDING_POLICY 0
#define MIN_CROSS_ORIGIN_SENDING_POLICY 0
#define MIN_TRIMMING_POLICY 0

static uint32_t sDefaultRp = DEFAULT_RP;
static uint32_t sDefaultTrackerRp = DEFAULT_TRACKER_RP;
static uint32_t defaultPrivateRp = DEFAULT_PRIVATE_RP;
static uint32_t defaultTrackerPrivateRp = DEFAULT_TRACKER_PRIVATE_RP;

static bool sUserSpoofReferrerSource = false;
static bool sUserHideOnionReferrerSource = false;
static uint32_t sUserReferrerSendingPolicy = 0;
static uint32_t sUserXOriginSendingPolicy = 0;
static uint32_t sUserTrimmingPolicy = 0;
static uint32_t sUserXOriginTrimmingPolicy = 0;

static void CachePreferrenceValue() {
  static bool sPrefCached = false;
  if (sPrefCached) {
    return;
  }

  Preferences::AddBoolVarCache(&sUserSpoofReferrerSource,
                               "network.http.referer.spoofSource");
  Preferences::AddBoolVarCache(&sUserHideOnionReferrerSource,
                               "network.http.referer.hideOnionSource");
  Preferences::AddUintVarCache(&sUserReferrerSendingPolicy,
                               "network.http.sendRefererHeader");
  sUserReferrerSendingPolicy =
      clamped<uint32_t>(sUserReferrerSendingPolicy, MIN_REFERRER_SENDING_POLICY,
                        MAX_REFERRER_SENDING_POLICY);

  Preferences::AddUintVarCache(&sUserXOriginSendingPolicy,
                               "network.http.referer.XOriginPolicy");
  sUserXOriginSendingPolicy = clamped<uint32_t>(
      sUserXOriginSendingPolicy, MIN_CROSS_ORIGIN_SENDING_POLICY,
      MAX_CROSS_ORIGIN_SENDING_POLICY);

  Preferences::AddUintVarCache(&sUserTrimmingPolicy,
                               "network.http.referer.trimmingPolicy");
  sUserTrimmingPolicy = clamped<uint32_t>(
      sUserTrimmingPolicy, MIN_TRIMMING_POLICY, MAX_TRIMMING_POLICY);

  Preferences::AddUintVarCache(&sUserXOriginTrimmingPolicy,
                               "network.http.referer.XOriginTrimmingPolicy");
  sUserXOriginTrimmingPolicy = clamped<uint32_t>(
      sUserXOriginTrimmingPolicy, MIN_TRIMMING_POLICY, MAX_TRIMMING_POLICY);

  Preferences::AddUintVarCache(
      &sDefaultRp, "network.http.referer.defaultPolicy", DEFAULT_RP);
  Preferences::AddUintVarCache(&sDefaultTrackerRp,
                               "network.http.referer.defaultPolicy.trackers",
                               DEFAULT_TRACKER_RP);
  Preferences::AddUintVarCache(&defaultPrivateRp,
                               "network.http.referer.defaultPolicy.pbmode",
                               DEFAULT_PRIVATE_RP);
  Preferences::AddUintVarCache(
      &defaultTrackerPrivateRp,
      "network.http.referer.defaultPolicy.trackers.pbmode",
      DEFAULT_TRACKER_PRIVATE_RP);

  sPrefCached = true;
}

/* static */
bool ReferrerInfo::HideOnionReferrerSource() {
  CachePreferrenceValue();
  return sUserHideOnionReferrerSource;
}

/* static */
uint32_t ReferrerInfo::GetDefaultReferrerPolicy(nsIHttpChannel* aChannel,
                                                nsIURI* aURI,
                                                bool privateBrowsing) {
  CachePreferrenceValue();
  bool thirdPartyTrackerIsolated = false;
  if (StaticPrefs::network_cookie_cookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      aChannel && aURI) {
    uint32_t rejectedReason = 0;
    thirdPartyTrackerIsolated =
        !AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
            aChannel, aURI, &rejectedReason);
    // Here we intentionally do not notify about the rejection reason, if any
    // in order to avoid this check to have any visible side-effects (e.g. a
    // web console report.)
  }

  uint32_t defaultToUse;
  if (thirdPartyTrackerIsolated) {
    if (privateBrowsing) {
      defaultToUse = defaultTrackerPrivateRp;
    } else {
      defaultToUse = sDefaultTrackerRp;
    }
  } else {
    if (privateBrowsing) {
      defaultToUse = defaultPrivateRp;
    } else {
      defaultToUse = sDefaultRp;
    }
  }

  switch (defaultToUse) {
    case DefaultReferrerPolicy::eDefaultPolicyNoReferrer:
      return nsIHttpChannel::REFERRER_POLICY_NO_REFERRER;
    case DefaultReferrerPolicy::eDefaultPolicySameOrgin:
      return nsIHttpChannel::REFERRER_POLICY_SAME_ORIGIN;
    case DefaultReferrerPolicy::eDefaultPolicyStrictWhenXorigin:
      return nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN;
  }

  return nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE;
}

/* static */
bool ReferrerInfo::IsReferrerSchemeAllowed(nsIURI* aReferrer) {
  NS_ENSURE_TRUE(aReferrer, false);

  nsAutoCString scheme;
  nsresult rv = aReferrer->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return scheme.EqualsIgnoreCase("https") || scheme.EqualsIgnoreCase("http") ||
         scheme.EqualsIgnoreCase("ftp");
}

nsresult ReferrerInfo::HandleSecureToInsecureReferral(nsIURI* aURI,
                                                      bool& aAllowed) const {
  NS_ENSURE_ARG(aURI);

  aAllowed = false;
  bool referrerIsHttpsScheme;
  nsresult rv = mOriginalReferrer->SchemeIs("https", &referrerIsHttpsScheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!referrerIsHttpsScheme) {
    aAllowed = true;
    return NS_OK;
  }

  bool uriIsHttpsScheme;
  rv = aURI->SchemeIs("https", &uriIsHttpsScheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // It's ok to send referrer for https-to-http scenarios if the referrer
  // policy is "unsafe-url", "origin", or "origin-when-cross-origin".
  // in other referrer policies, https->http is not allowed...

  if (mPolicy != nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL &&
      mPolicy != nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN &&
      mPolicy != nsIHttpChannel::REFERRER_POLICY_ORIGIN && !uriIsHttpsScheme) {
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
  if (sUserHideOnionReferrerSource && !uriHost.Equals(referrerHost) &&
      StringEndsWith(referrerHost, NS_LITERAL_CSTRING(".onion"))) {
    return NS_OK;
  }

  switch (sUserXOriginSendingPolicy) {
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
  if (sUserReferrerSendingPolicy < referrerSendingPolicy) {
    return NS_OK;
  }

  aAllowed = true;
  return NS_OK;
}

bool ReferrerInfo::IsCrossOriginRequest(nsIHttpChannel* aChannel) const {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  nsCOMPtr<nsIURI> triggeringURI;
  loadInfo->TriggeringPrincipal()->GetURI(getter_AddRefs(triggeringURI));

  if (!triggeringURI) {
    LOG(("no triggering URI via loadInfo, assuming load is cross-origin"));
    return true;
  }

  if (LOG_ENABLED()) {
    nsAutoCString triggeringURISpec;
    triggeringURI->GetAsciiSpec(triggeringURISpec);
    LOG(("triggeringURI=%s\n", triggeringURISpec.get()));
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  bool isPrivateWin = loadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;

  rv = ssm->CheckSameOriginURI(triggeringURI, uri, false, isPrivateWin);
  return (NS_FAILED(rv));
}

ReferrerInfo::TrimmingPolicy ReferrerInfo::ComputeTrimmingPolicy(
    nsIHttpChannel* aChannel) const {
  uint32_t trimmingPolicy = sUserTrimmingPolicy;

  switch (mPolicy) {
    case nsIHttpChannel::REFERRER_POLICY_ORIGIN:
    case nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN:
      trimmingPolicy = TrimmingPolicy::ePolicySchemeHostPort;
      break;

    case nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN:
    case nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN:
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
    case nsIHttpChannel::REFERRER_POLICY_SAME_ORIGIN:
    case nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE:
    case nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL:
      if (trimmingPolicy != TrimmingPolicy::ePolicySchemeHostPort) {
        // Ignore set trimmingPolicy if it is already the strictest
        // policy. Apply the user cross-origin trimming policy if it's more
        // restrictive than the general one.
        if (sUserXOriginTrimmingPolicy != TrimmingPolicy::ePolicyFullURI &&
            IsCrossOriginRequest(aChannel)) {
          trimmingPolicy = std::max(trimmingPolicy, sUserXOriginTrimmingPolicy);
        }
      }
      break;

    case nsIHttpChannel::REFERRER_POLICY_NO_REFERRER:
    case nsIHttpChannel::REFERRER_POLICY_UNSET:
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected value");
      break;
  }

  return static_cast<TrimmingPolicy>(trimmingPolicy);
}

nsresult ReferrerInfo::TrimReferrerWithPolicy(nsCOMPtr<nsIURI>& aReferrer,
                                              TrimmingPolicy aTrimmingPolicy,
                                              nsACString& aResult) const {
  if (aTrimmingPolicy == TrimmingPolicy::ePolicyFullURI) {
    // use the full URI
    return aReferrer->GetAsciiSpec(aResult);
  }

  // All output strings start with: scheme+host+port
  // We want the IDN-normalized PrePath.  That's not something currently
  // available and there doesn't yet seem to be justification for adding it to
  // the interfaces, so just build it up ourselves from scheme+AsciiHostPort
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

  switch (aTrimmingPolicy) {
    case TrimmingPolicy::ePolicySchemeHostPortPath: {
      nsCOMPtr<nsIURL> url(do_QueryInterface(aReferrer));
      if (url) {
        nsAutoCString path;
        rv = url->GetFilePath(path);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        aResult.Append(path);
        rv = NS_MutateURI(url)
                 .SetQuery(EmptyCString())
                 .SetRef(EmptyCString())
                 .Finalize(aReferrer);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        break;
      }
      // No URL, so fall through to truncating the path and any query/ref off
      // as well.
    }
      MOZ_FALLTHROUGH;
    default:  // (User Pref limited to [0,2])
    case TrimmingPolicy::ePolicySchemeHostPort:
      aResult.AppendLiteral("/");
      // This nukes any query/ref present as well in the case of nsStandardURL
      rv = NS_MutateURI(aReferrer)
               .SetPathQueryRef(EmptyCString())
               .Finalize(aReferrer);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
  }

  return NS_OK;
}
ReferrerInfo::ReferrerInfo()
    : mOriginalReferrer(nullptr),
      mPolicy(mozilla::net::RP_Unset),
      mSendReferrer(true),
      mInitialized(false),
      mOverridePolicyByDefault(false),
      mComputedReferrer(Maybe<nsCString>()) {}

ReferrerInfo::ReferrerInfo(nsIURI* aOriginalReferrer, uint32_t aPolicy,
                           bool aSendReferrer,
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

already_AddRefed<nsIReferrerInfo> ReferrerInfo::Clone() const {
  RefPtr<ReferrerInfo> copy(new ReferrerInfo(*this));
  return copy.forget();
}

already_AddRefed<nsIReferrerInfo> ReferrerInfo::CloneWithNewPolicy(
    uint32_t aPolicy) const {
  RefPtr<ReferrerInfo> copy(new ReferrerInfo(*this));
  copy->mPolicy = aPolicy;
  return copy.forget();
}

NS_IMETHODIMP
ReferrerInfo::GetOriginalReferrer(nsIURI** aOriginalReferrer) {
  *aOriginalReferrer = mOriginalReferrer;
  NS_IF_ADDREF(*aOriginalReferrer);
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetReferrerPolicy(uint32_t* aReferrerPolicy) {
  *aReferrerPolicy = mPolicy;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetSendReferrer(bool* aSendReferrer) {
  *aSendReferrer = mSendReferrer;
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

NS_IMETHODIMP
ReferrerInfo::Init(uint32_t aReferrerPolicy, bool aSendReferrer,
                   nsIURI* aOriginalReferrer) {
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  };

  mInitialized = true;
  mPolicy = aReferrerPolicy;
  mSendReferrer = aSendReferrer;
  mOriginalReferrer = aOriginalReferrer;
  return NS_OK;
}

nsresult ReferrerInfo::ComputeReferrer(nsIHttpChannel* aChannel) {
  CachePreferrenceValue();
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
  mComputedReferrer.emplace(EmptyCString());

  if (!mSendReferrer || !mOriginalReferrer ||
      mPolicy == nsIHttpChannel::REFERRER_POLICY_NO_REFERRER) {
    return NS_OK;
  }

  if (mPolicy == nsIHttpChannel::REFERRER_POLICY_UNSET) {
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

  if (mPolicy == nsIHttpChannel::REFERRER_POLICY_NO_REFERRER) {
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

  // Enforce Referrer whitelist, only http, https, ftp scheme are allowed
  if (!IsReferrerSchemeAllowed(mOriginalReferrer)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isSecureToInsecureAllowed = false;
  rv = HandleSecureToInsecureReferral(uri, isSecureToInsecureAllowed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isSecureToInsecureAllowed) {
    return NS_OK;
  }

  // Don't send referrer when the request is cross-origin and policy is
  // "same-origin".
  if (mPolicy == nsIHttpChannel::REFERRER_POLICY_SAME_ORIGIN &&
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
  if (sUserSpoofReferrerSource) {
    nsCOMPtr<nsIURI> userSpoofReferrer;
    rv = NS_GetURIWithoutRef(uri, getter_AddRefs(userSpoofReferrer));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    referrer = userSpoofReferrer;
  }

  // strip away any userpass; we don't want to be giving out passwords ;-)
  // This is required by Referrer Policy stripping algorithm.
  nsCOMPtr<nsIURIFixup> urifixup = services::GetURIFixup();
  if (NS_WARN_IF(!urifixup)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> exposableURI;
  rv = urifixup->CreateExposableURI(referrer, getter_AddRefs(exposableURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  referrer = exposableURI;

  TrimmingPolicy trimmingPolicy = ComputeTrimmingPolicy(aChannel);

  nsAutoCString referrerSpec;
  rv = TrimReferrerWithPolicy(referrer, trimmingPolicy, referrerSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // finally, remember the referrer URI.
  mComputedReferrer.reset();
  mComputedReferrer.emplace(referrerSpec);

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

  rv = aStream->Read32(&mPolicy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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

  rv = aStream->Write32(mPolicy);
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

}  // namespace dom
}  // namespace mozilla
