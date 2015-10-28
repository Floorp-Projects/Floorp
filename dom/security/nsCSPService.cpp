/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIObserver.h"
#include "nsIContent.h"
#include "nsCSPService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsError.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "mozilla/Preferences.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsPrincipal.h"

using namespace mozilla;

/* Keeps track of whether or not CSP is enabled */
bool CSPService::sCSPEnabled = true;

static PRLogModuleInfo* gCspPRLog;

CSPService::CSPService()
{
  Preferences::AddBoolVarCache(&sCSPEnabled, "security.csp.enable");

  if (!gCspPRLog)
    gCspPRLog = PR_NewLogModule("CSP");
}

CSPService::~CSPService()
{
  mAppStatusCache.Clear();
}

NS_IMPL_ISUPPORTS(CSPService, nsIContentPolicy, nsIChannelEventSink)

// Helper function to identify protocols not subject to CSP.
bool
subjectToCSP(nsIURI* aURI) {
  // The three protocols: data:, blob: and filesystem: share the same
  // protocol flag (URI_IS_LOCAL_RESOURCE) with other protocols, like
  // chrome:, resource:, moz-icon:, but those three protocols get
  // special attention in CSP and are subject to CSP, hence we have
  // to make sure those protocols are subject to CSP, see:
  // http://www.w3.org/TR/CSP2/#source-list-guid-matching
  bool match = false;
  nsresult rv = aURI->SchemeIs("data", &match);
  if (NS_SUCCEEDED(rv) && match) {
    return true;
  }
  rv = aURI->SchemeIs("blob", &match);
  if (NS_SUCCEEDED(rv) && match) {
    return true;
  }
  rv = aURI->SchemeIs("filesystem", &match);
  if (NS_SUCCEEDED(rv) && match) {
    return true;
  }
  // finally we have to whitelist "about:" which does not fall in
  // any of the two categories underneath but is not subject to CSP.
  rv = aURI->SchemeIs("about", &match);
  if (NS_SUCCEEDED(rv) && match) {
    return false;
  }

  // Other protocols are not subject to CSP and can be whitelisted:
  // * URI_IS_LOCAL_RESOURCE
  //   e.g. chrome:, data:, blob:, resource:, moz-icon:
  // * URI_INHERITS_SECURITY_CONTEXT
  //   e.g. javascript:
  //
  // Please note that it should be possible for websites to
  // whitelist their own protocol handlers with respect to CSP,
  // hence we use protocol flags to accomplish that.
  rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE, &match);
  if (NS_SUCCEEDED(rv) && match) {
    return false;
  }
  rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT, &match);
  if (NS_SUCCEEDED(rv) && match) {
    return false;
  }
  // all other protocols are subject To CSP.
  return true;
}

/* nsIContentPolicy implementation */
NS_IMETHODIMP
CSPService::ShouldLoad(uint32_t aContentType,
                       nsIURI *aContentLocation,
                       nsIURI *aRequestOrigin,
                       nsISupports *aRequestContext,
                       const nsACString &aMimeTypeGuess,
                       nsISupports *aExtra,
                       nsIPrincipal *aRequestPrincipal,
                       int16_t *aDecision)
{
  MOZ_ASSERT(aContentType ==
             nsContentUtils::InternalContentPolicyTypeToExternalOrCSPInternal(aContentType),
             "We should only see external content policy types or CSP special types (preloads or workers) here.");

  if (!aContentLocation) {
    return NS_ERROR_FAILURE;
  }

  if (MOZ_LOG_TEST(gCspPRLog, LogLevel::Debug)) {
    nsAutoCString location;
    aContentLocation->GetSpec(location);
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
           ("CSPService::ShouldLoad called for %s", location.get()));
  }

  // default decision, CSP can revise it if there's a policy to enforce
  *aDecision = nsIContentPolicy::ACCEPT;

  // No need to continue processing if CSP is disabled or if the protocol
  // is *not* subject to CSP.
  // Please note, the correct way to opt-out of CSP using a custom
  // protocolHandler is to set one of the nsIProtocolHandler flags
  // that are whitelistet in subjectToCSP()
  if (!sCSPEnabled || !subjectToCSP(aContentLocation)) {
    return NS_OK;
  }

  // These content types are not subject to CSP content policy checks:
  // TYPE_CSP_REPORT -- csp can't block csp reports
  // TYPE_REFRESH    -- never passed to ShouldLoad (see nsIContentPolicy.idl)
  // TYPE_DOCUMENT   -- used for frame-ancestors
  if (aContentType == nsIContentPolicy::TYPE_CSP_REPORT ||
    aContentType == nsIContentPolicy::TYPE_REFRESH ||
    aContentType == nsIContentPolicy::TYPE_DOCUMENT) {
    return NS_OK;
  }

  // ----- THIS IS A TEMPORARY FAST PATH FOR CERTIFIED APPS. -----
  // ----- PLEASE REMOVE ONCE bug 925004 LANDS.              -----

  // Cache the app status for this origin.
  uint16_t status = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  nsAutoCString sourceOrigin;
  if (aRequestPrincipal && aRequestOrigin) {
    aRequestOrigin->GetPrePath(sourceOrigin);
    if (!mAppStatusCache.Get(sourceOrigin, &status)) {
      aRequestPrincipal->GetAppStatus(&status);
      mAppStatusCache.Put(sourceOrigin, status);
    }
  }

  if (status == nsIPrincipal::APP_STATUS_CERTIFIED) {
    // The CSP for certified apps is :
    // "default-src * data: blob:; script-src 'self'; object-src 'none'; style-src 'self' app://theme.gaiamobile.org:*"
    // That means we can optimize for this case by:
    // - loading same origin scripts and stylesheets, and stylesheets from the
    //   theme url space.
    // - never loading objects.
    // - accepting everything else.

    switch (aContentType) {
      case nsIContentPolicy::TYPE_SCRIPT:
      case nsIContentPolicy::TYPE_STYLESHEET:
        {
          // Whitelist the theme resources.
          auto themeOrigin = Preferences::GetCString("b2g.theme.origin");
          nsAutoCString contentOrigin;
          aContentLocation->GetPrePath(contentOrigin);

          if (!(sourceOrigin.Equals(contentOrigin) ||
                (themeOrigin && themeOrigin.Equals(contentOrigin)))) {
            *aDecision = nsIContentPolicy::REJECT_SERVER;
          }
        }
        break;

      case nsIContentPolicy::TYPE_OBJECT:
        *aDecision = nsIContentPolicy::REJECT_SERVER;
        break;

      default:
        *aDecision = nsIContentPolicy::ACCEPT;
    }

    // Only cache and return if we are successful. If not, we want the error
    // to be reported, and thus fallback to the slow path.
    if (*aDecision == nsIContentPolicy::ACCEPT) {
      return NS_OK;
    }
  }

  // ----- END OF TEMPORARY FAST PATH FOR CERTIFIED APPS. -----

  // query the principal of the document; if no document is passed, then
  // fall back to using the requestPrincipal (e.g. service workers do not
  // pass a document).
  nsCOMPtr<nsINode> node(do_QueryInterface(aRequestContext));
  nsCOMPtr<nsIPrincipal> principal = node ? node->NodePrincipal()
                                          : aRequestPrincipal;
  if (principal) {
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    principal->GetCsp(getter_AddRefs(csp));

    if (csp) {
      if (MOZ_LOG_TEST(gCspPRLog, LogLevel::Debug)) {
        uint32_t numPolicies = 0;
        nsresult rv = csp->GetPolicyCount(&numPolicies);
        if (NS_SUCCEEDED(rv)) {
          for (uint32_t i=0; i<numPolicies; i++) {
            nsAutoString policy;
            csp->GetPolicy(i, policy);
            MOZ_LOG(gCspPRLog, LogLevel::Debug,
                   ("Document has CSP[%d]: %s", i,
                   NS_ConvertUTF16toUTF8(policy).get()));
          }
        }
      }
      // obtain the enforcement decision
      // (don't pass aExtra, we use that slot for redirects)
      csp->ShouldLoad(aContentType,
                      aContentLocation,
                      aRequestOrigin,
                      aRequestContext,
                      aMimeTypeGuess,
                      nullptr,
                      aDecision);
    }
  }
  else if (MOZ_LOG_TEST(gCspPRLog, LogLevel::Debug)) {
    nsAutoCString uriSpec;
    aContentLocation->GetSpec(uriSpec);
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
           ("COULD NOT get nsIPrincipal for location: %s", uriSpec.get()));
  }

  return NS_OK;
}

NS_IMETHODIMP
CSPService::ShouldProcess(uint32_t         aContentType,
                          nsIURI           *aContentLocation,
                          nsIURI           *aRequestOrigin,
                          nsISupports      *aRequestContext,
                          const nsACString &aMimeTypeGuess,
                          nsISupports      *aExtra,
                          nsIPrincipal     *aRequestPrincipal,
                          int16_t          *aDecision)
{
  MOZ_ASSERT(aContentType ==
             nsContentUtils::InternalContentPolicyTypeToExternalOrCSPInternal(aContentType),
             "We should only see external content policy types or preloads here.");

  if (!aContentLocation)
    return NS_ERROR_FAILURE;

  *aDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

/* nsIChannelEventSink implementation */
NS_IMETHODIMP
CSPService::AsyncOnChannelRedirect(nsIChannel *oldChannel,
                                   nsIChannel *newChannel,
                                   uint32_t flags,
                                   nsIAsyncVerifyRedirectCallback *callback)
{
  nsAsyncRedirectAutoCallback autoCallback(callback);

  nsCOMPtr<nsIURI> newUri;
  nsresult rv = newChannel->GetURI(getter_AddRefs(newUri));
  NS_ENSURE_SUCCESS(rv, rv);

  // No need to continue processing if CSP is disabled or if the protocol
  // is *not* subject to CSP.
  // Please note, the correct way to opt-out of CSP using a custom
  // protocolHandler is to set one of the nsIProtocolHandler flags
  // that are whitelistet in subjectToCSP()
  if (!sCSPEnabled || !subjectToCSP(newUri)) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = oldChannel->GetLoadInfo(getter_AddRefs(loadInfo));

  // if no loadInfo on the channel, nothing for us to do
  if (!loadInfo) {
    return NS_OK;
  }

  // Get the LoadingPrincipal and CSP from the loadInfo
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = loadInfo->LoadingPrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);

  // if there is no CSP, nothing for us to do
  if (!csp) {
    return NS_OK;
  }

  /* Since redirecting channels don't call into nsIContentPolicy, we call our
   * Content Policy implementation directly when redirects occur using the
   * information set in the LoadInfo when channels are created.
   *
   * We check if the CSP permits this host for this type of load, if not,
   * we cancel the load now.
   */
  nsCOMPtr<nsIURI> originalUri;
  rv = oldChannel->GetOriginalURI(getter_AddRefs(originalUri));
  NS_ENSURE_SUCCESS(rv, rv);
  /* On redirect, if the content policy is a preload type, rejecting the preload
   * results in the load silently failing, so we convert preloads to the actual
   * type. See Bug 1219453.
   */
  nsContentPolicyType policyType =
    nsContentUtils::InternalContentPolicyTypeToExternalOrWorker(
        loadInfo->InternalContentPolicyType());

  int16_t aDecision = nsIContentPolicy::ACCEPT;
  csp->ShouldLoad(policyType,     // load type per nsIContentPolicy (uint32_t)
                  newUri,         // nsIURI
                  nullptr,        // nsIURI
                  nullptr,        // nsISupports
                  EmptyCString(), // ACString - MIME guess
                  originalUri,    // aMimeTypeGuess
                  &aDecision);

  if (newUri && MOZ_LOG_TEST(gCspPRLog, LogLevel::Debug)) {
    nsAutoCString newUriSpec("None");
    newUri->GetSpec(newUriSpec);
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
           ("CSPService::AsyncOnChannelRedirect called for %s",
            newUriSpec.get()));
  }
  if (aDecision == 1) {
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
           ("CSPService::AsyncOnChannelRedirect ALLOWING request."));
  }
  else {
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
           ("CSPService::AsyncOnChannelRedirect CANCELLING request."));
  }

  // if ShouldLoad doesn't accept the load, cancel the request
  if (!NS_CP_ACCEPTED(aDecision)) {
    autoCallback.DontCallback();
    return NS_BINDING_FAILED;
  }
  return NS_OK;
}
