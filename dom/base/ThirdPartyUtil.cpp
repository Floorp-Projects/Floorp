/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThirdPartyUtil.h"
#include "nsDocShell.h"
#include "nsGlobalWindowOuter.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsILoadContext.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"
#include "nsGlobalWindowOuter.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(ThirdPartyUtil, mozIThirdPartyUtil)

//
// MOZ_LOG=thirdPartyUtil:5
//
static mozilla::LazyLogModule gThirdPartyLog("thirdPartyUtil");
#undef LOG
#define LOG(args) MOZ_LOG(gThirdPartyLog, mozilla::LogLevel::Debug, args)

static mozilla::StaticRefPtr<ThirdPartyUtil> gService;

// static
void ThirdPartyUtil::Startup() {
  nsCOMPtr<mozIThirdPartyUtil> tpu;
  if (NS_WARN_IF(!(tpu = do_GetService(THIRDPARTYUTIL_CONTRACTID)))) {
    NS_WARNING("Failed to get third party util!");
  }
}

nsresult ThirdPartyUtil::Init() {
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_AVAILABLE);

  MOZ_ASSERT(!gService);
  gService = this;
  mozilla::ClearOnShutdown(&gService);

  mTLDService = nsEffectiveTLDService::GetInstance();
  return mTLDService ? NS_OK : NS_ERROR_FAILURE;
}

ThirdPartyUtil::~ThirdPartyUtil() { gService = nullptr; }

// static
ThirdPartyUtil* ThirdPartyUtil::GetInstance() {
  if (gService) {
    return gService;
  }
  nsCOMPtr<mozIThirdPartyUtil> tpuService =
      mozilla::services::GetThirdPartyUtil();
  if (!tpuService) {
    return nullptr;
  }
  MOZ_ASSERT(
      gService,
      "gService must have been initialized in nsEffectiveTLDService::Init");
  return gService;
}

// Determine if aFirstDomain is a different base domain to aSecondURI; or, if
// the concept of base domain does not apply, determine if the two hosts are not
// string-identical.
nsresult ThirdPartyUtil::IsThirdPartyInternal(const nsCString& aFirstDomain,
                                              nsIURI* aSecondURI,
                                              bool* aResult) {
  if (!aSecondURI) {
    return NS_ERROR_INVALID_ARG;
  }

  // BlobURLs are always first-party.
  if (aSecondURI->SchemeIs("blob")) {
    *aResult = false;
    return NS_OK;
  }

  // Get the base domain for aSecondURI.
  nsAutoCString secondDomain;
  nsresult rv = GetBaseDomain(aSecondURI, secondDomain);
  LOG(("ThirdPartyUtil::IsThirdPartyInternal %s =? %s", aFirstDomain.get(),
       secondDomain.get()));
  if (NS_FAILED(rv)) return rv;

  *aResult = IsThirdPartyInternal(aFirstDomain, secondDomain);
  return NS_OK;
}

nsCString ThirdPartyUtil::GetBaseDomainFromWindow(nsPIDOMWindowOuter* aWindow) {
  mozilla::dom::Document* doc = aWindow ? aWindow->GetExtantDoc() : nullptr;

  if (!doc) {
    return EmptyCString();
  }

  return doc->GetBaseDomain();
}

NS_IMETHODIMP
ThirdPartyUtil::GetPrincipalFromWindow(mozIDOMWindowProxy* aWin,
                                       nsIPrincipal** result) {
  nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin = do_QueryInterface(aWin);
  if (!scriptObjPrin) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIPrincipal> prin = scriptObjPrin->GetPrincipal();
  if (!prin) {
    return NS_ERROR_INVALID_ARG;
  }

  prin.forget(result);
  return NS_OK;
}

// Get the URI associated with a window.
NS_IMETHODIMP
ThirdPartyUtil::GetURIFromWindow(mozIDOMWindowProxy* aWin, nsIURI** result) {
  nsCOMPtr<nsIPrincipal> prin;
  nsresult rv = GetPrincipalFromWindow(aWin, getter_AddRefs(prin));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (prin->GetIsNullPrincipal()) {
    LOG(("ThirdPartyUtil::GetURIFromWindow can't use null principal\n"));
    return NS_ERROR_INVALID_ARG;
  }
  auto* basePrin = BasePrincipal::Cast(prin);
  return basePrin->GetURI(result);
}

NS_IMETHODIMP
ThirdPartyUtil::GetContentBlockingAllowListPrincipalFromWindow(
    mozIDOMWindowProxy* aWin, nsIURI* aURIBeingLoaded, nsIPrincipal** result) {
  nsPIDOMWindowOuter* outerWindow = nsPIDOMWindowOuter::From(aWin);
  nsPIDOMWindowInner* innerWindow = outerWindow->GetCurrentInnerWindow();
  Document* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;
  if (!doc) {
    return GetPrincipalFromWindow(aWin, result);
  }

  nsCOMPtr<nsIPrincipal> principal =
      doc->GetContentBlockingAllowListPrincipal();
  if (aURIBeingLoaded && principal && principal->GetIsNullPrincipal()) {
    // If we have an initial principal during navigation, recompute it to get
    // the real content blocking allow list principal.
    nsIDocShell* docShell = doc->GetDocShell();
    OriginAttributes attrs =
        docShell ? nsDocShell::Cast(docShell)->GetOriginAttributes()
                 : OriginAttributes();
    ContentBlockingAllowList::RecomputePrincipal(aURIBeingLoaded, attrs,
                                                 getter_AddRefs(principal));
  }

  if (!principal || !principal->GetIsContentPrincipal()) {
    // This is for compatibility with GetURIFromWindow.  Null principals are
    // explicitly special cased there.  GetURI returns nullptr for
    // SystemPrincipal and ExpandedPrincipal.
    LOG(
        ("ThirdPartyUtil::GetContentBlockingAllowListPrincipalFromWindow can't "
         "use null principal\n"));
    return NS_ERROR_INVALID_ARG;
  }

  principal.forget(result);
  return NS_OK;
}

// Determine if aFirstURI is third party with respect to aSecondURI. See docs
// for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyURI(nsIURI* aFirstURI, nsIURI* aSecondURI,
                                bool* aResult) {
  NS_ENSURE_ARG(aFirstURI);
  NS_ENSURE_ARG(aSecondURI);
  NS_ASSERTION(aResult, "null outparam pointer");

  nsAutoCString firstHost;
  nsresult rv = GetBaseDomain(aFirstURI, firstHost);
  if (NS_FAILED(rv)) return rv;

  return IsThirdPartyInternal(firstHost, aSecondURI, aResult);
}

// If the optional aURI is provided, determine whether aWindow is foreign with
// respect to aURI. If the optional aURI is not provided, determine whether the
// given "window hierarchy" is third party. See docs for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyWindow(mozIDOMWindowProxy* aWindow, nsIURI* aURI,
                                   bool* aResult) {
  NS_ENSURE_ARG(aWindow);
  NS_ASSERTION(aResult, "null outparam pointer");

  bool result;

  // Ignore about:blank URIs here since they have no domain and attempting to
  // compare against them will fail.
  if (aURI && !NS_IsAboutBlank(aURI)) {
    nsCOMPtr<nsIPrincipal> prin;
    nsresult rv = GetPrincipalFromWindow(aWindow, getter_AddRefs(prin));
    NS_ENSURE_SUCCESS(rv, rv);
    // Determine whether aURI is foreign with respect to the current principal.
    rv = prin->IsThirdPartyURI(aURI, &result);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (result) {
      *aResult = true;
      return NS_OK;
    }
  }

  nsPIDOMWindowOuter* current = nsPIDOMWindowOuter::From(aWindow);
  do {
    // We use GetInProcessScriptableParent rather than GetParent because we
    // consider <iframe mozbrowser> to be a top-level frame.
    nsPIDOMWindowOuter* parent = current->GetInProcessScriptableParent();
    // We don't use SameCOMIdentity here since we know that nsPIDOMWindowOuter
    // is only implemented by nsGlobalWindowOuter, so different objects of that
    // type will not have different nsISupports COM identities, and checking the
    // actual COM identity using SameCOMIdentity is expensive due to the virtual
    // calls involved.
    if (parent == current) {
      auto* const browsingContext = current->GetBrowsingContext();
      MOZ_ASSERT(browsingContext);

      // We're either at the topmost content window (i.e. no third party), or,
      // with fission, we may be an out-of-process content subframe (i.e. third
      // party), since GetInProcessScriptableParent above explicitly does not
      // go beyond process boundaries. In either case, we already know the
      // result.
      *aResult = browsingContext->IsContentSubframe();
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> currentPrincipal;
    nsresult rv =
        GetPrincipalFromWindow(current, getter_AddRefs(currentPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIPrincipal> parentPrincipal;
    rv = GetPrincipalFromWindow(parent, getter_AddRefs(parentPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = currentPrincipal->IsThirdPartyPrincipal(parentPrincipal, &result);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (result) {
      *aResult = true;
      return NS_OK;
    }

    current = parent;
  } while (1);

  MOZ_ASSERT_UNREACHABLE("should've returned");
  return NS_ERROR_UNEXPECTED;
}

nsresult ThirdPartyUtil::IsThirdPartyGlobal(
    mozilla::dom::WindowGlobalParent* aWindowGlobal, bool* aResult) {
  NS_ENSURE_ARG(aWindowGlobal);
  NS_ASSERTION(aResult, "null outparam pointer");

  auto* currentWGP = aWindowGlobal;
  do {
    MOZ_ASSERT(currentWGP->BrowsingContext());
    if (currentWGP->BrowsingContext()->IsTop()) {
      *aResult = false;
      return NS_OK;
    }
    nsCOMPtr<nsIPrincipal> currentPrincipal = currentWGP->DocumentPrincipal();
    RefPtr<WindowGlobalParent> parent =
        currentWGP->BrowsingContext()->GetEmbedderWindowGlobal();
    if (!parent) {
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrincipal> parentPrincipal = parent->DocumentPrincipal();
    nsresult rv =
        currentPrincipal->IsThirdPartyPrincipal(parentPrincipal, aResult);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (*aResult) {
      return NS_OK;
    }

    currentWGP = parent;
  } while (true);
}

// Determine if the URI associated with aChannel or any URI of the window
// hierarchy associated with the channel is foreign with respect to aSecondURI.
// See docs for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyChannel(nsIChannel* aChannel, nsIURI* aURI,
                                    bool* aResult) {
  LOG(("ThirdPartyUtil::IsThirdPartyChannel [channel=%p]", aChannel));
  NS_ENSURE_ARG(aChannel);
  NS_ASSERTION(aResult, "null outparam pointer");

  nsresult rv;
  bool doForce = false;
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(aChannel);
  if (httpChannelInternal) {
    uint32_t flags = 0;
    // Avoid checking the return value here since some channel implementations
    // may return NS_ERROR_NOT_IMPLEMENTED.
    mozilla::Unused << httpChannelInternal->GetThirdPartyFlags(&flags);

    doForce = (flags & nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);

    // If aURI was not supplied, and we're forcing, then we're by definition
    // not foreign. If aURI was supplied, we still want to check whether it's
    // foreign with respect to the channel URI. (The forcing only applies to
    // whatever window hierarchy exists above the channel.)
    if (doForce && !aURI) {
      *aResult = false;
      return NS_OK;
    }
  }

  bool parentIsThird = false;

  // Obtain the URI from the channel, and its base domain.
  nsCOMPtr<nsIURI> channelURI;
  rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  if (NS_FAILED(rv)) return rv;

  nsAutoCString channelDomain;
  rv = GetBaseDomain(channelURI, channelDomain);
  if (NS_FAILED(rv)) return rv;

  if (!doForce) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    parentIsThird = loadInfo->GetIsInThirdPartyContext();
    BasePrincipal* loadingPrincipal =
        BasePrincipal::Cast(loadInfo->GetLoadingPrincipal());
    if (!parentIsThird &&
        loadInfo->GetExternalContentPolicyType() !=
            nsIContentPolicy::TYPE_DOCUMENT &&
        (!loadingPrincipal->AddonPolicy() ||
         !loadingPrincipal->AddonAllowsLoad(channelURI))) {
      // Check if the channel itself is third-party to its own requestor.
      // Unforunately, we have to go through the loading principal.

      rv = loadingPrincipal->IsThirdPartyURI(channelURI, &parentIsThird);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  // If we're not comparing to a URI, we have our answer. Otherwise, if
  // parentIsThird, we're not forcing and we know that we're a third-party
  // request.
  if (!aURI || parentIsThird) {
    *aResult = parentIsThird;
    return NS_OK;
  }

  // Determine whether aURI is foreign with respect to channelURI.
  return IsThirdPartyInternal(channelDomain, aURI, aResult);
}

NS_IMETHODIMP
ThirdPartyUtil::GetTopWindowForChannel(nsIChannel* aChannel,
                                       nsIURI* aURIBeingLoaded,
                                       mozIDOMWindowProxy** aWin) {
  NS_ENSURE_ARG(aWin);

  // Find the associated window and its parent window.
  nsCOMPtr<nsILoadContext> ctx;
  NS_QueryNotificationCallbacks(aChannel, ctx);
  if (!ctx) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIDOMWindowProxy> window;
  ctx->GetAssociatedWindow(getter_AddRefs(window));
  if (!window) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsPIDOMWindowOuter> top =
      nsGlobalWindowOuter::Cast(window)
          ->GetTopExcludingExtensionAccessibleContentFrames(aURIBeingLoaded);
  top.forget(aWin);
  return NS_OK;
}

// Get the base domain for aHostURI; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". Only properly-formed URI's are tolerated, though a trailing
// dot may be present. If aHostURI is an IP address, an alias such as
// 'localhost', an eTLD such as 'co.uk', or the empty string, aBaseDomain will
// be the exact host. The result of this function should only be used in exact
// string comparisons, since substring comparisons will not be valid for the
// special cases elided above.
NS_IMETHODIMP
ThirdPartyUtil::GetBaseDomain(nsIURI* aHostURI, nsACString& aBaseDomain) {
  if (!aHostURI) {
    return NS_ERROR_INVALID_ARG;
  }

  // Get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = mTLDService->GetBaseDomain(aHostURI, 0, aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHostURI is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. Uses the normalized host in such
    // cases.
    rv = aHostURI->GetAsciiHost(aBaseDomain);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // aHostURI (and thus aBaseDomain) may be the string '.'. If so, fail.
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // Reject any URIs without a host that aren't file:// URIs. This makes it the
  // only way we can get a base domain consisting of the empty string, which
  // means we can safely perform foreign tests on such URIs where "not foreign"
  // means "the involved URIs are all file://".
  if (aBaseDomain.IsEmpty() && !aHostURI->SchemeIs("file")) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMETHODIMP
ThirdPartyUtil::GetBaseDomainFromSchemeHost(const nsACString& aScheme,
                                            const nsACString& aAsciiHost,
                                            nsACString& aBaseDomain) {
  MOZ_DIAGNOSTIC_ASSERT(IsAscii(aAsciiHost));

  // Get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = mTLDService->GetBaseDomainFromHost(aAsciiHost, 0, aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aMozURL is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. Uses the normalized host in such
    // cases.
    aBaseDomain = aAsciiHost;
    rv = NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // aMozURL (and thus aBaseDomain) may be the string '.'. If so, fail.
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // Reject any URLs without a host that aren't file:// URLs. This makes it the
  // only way we can get a base domain consisting of the empty string, which
  // means we can safely perform foreign tests on such URLs where "not foreign"
  // means "the involved URLs are all file://".
  if (aBaseDomain.IsEmpty() && !aScheme.EqualsLiteral("file")) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMETHODIMP_(ThirdPartyAnalysisResult)
ThirdPartyUtil::AnalyzeChannel(nsIChannel* aChannel, bool aNotify, nsIURI* aURI,
                               RequireThirdPartyCheck aRequireThirdPartyCheck,
                               uint32_t* aRejectedReason) {
  MOZ_ASSERT_IF(aNotify, aRejectedReason);

  ThirdPartyAnalysisResult result;

  nsCOMPtr<nsIURI> uri;
  if (!aURI && aChannel) {
    aChannel->GetURI(getter_AddRefs(uri));
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel ? aChannel->LoadInfo() : nullptr;

  bool isForeign = true;
  if (aChannel &&
      (!aRequireThirdPartyCheck || aRequireThirdPartyCheck(loadInfo))) {
    IsThirdPartyChannel(aChannel, aURI ? aURI : uri.get(), &isForeign);
  }
  if (isForeign) {
    result += ThirdPartyAnalysis::IsForeign;
  }

  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aChannel);
  if (classifiedChannel) {
    if (classifiedChannel->IsThirdPartyTrackingResource()) {
      result += ThirdPartyAnalysis::IsThirdPartyTrackingResource;
    }
    if (classifiedChannel->IsThirdPartySocialTrackingResource()) {
      result += ThirdPartyAnalysis::IsThirdPartySocialTrackingResource;
    }

    // Check first-party storage access even for non-tracking resources, since
    // we will need the result when computing the access rights for the reject
    // foreign cookie behavior mode.

    // If the caller has requested third-party checks, we will only perform the
    // storage access check once we know we're in the third-party context.
    bool performStorageChecks =
        aRequireThirdPartyCheck ? result.contains(ThirdPartyAnalysis::IsForeign)
                                : true;
    if (performStorageChecks &&
        ContentBlocking::ShouldAllowAccessFor(aChannel, aURI ? aURI : uri.get(),
                                              aRejectedReason)) {
      result += ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted;
    }

    if (aNotify && !result.contains(
                       ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted)) {
      ContentBlockingNotifier::OnDecision(
          aChannel, ContentBlockingNotifier::BlockingDecision::eBlock,
          *aRejectedReason);
    }
  }

  return result;
}
