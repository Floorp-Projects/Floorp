/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIObserver.h"
#include "nsIContent.h"
#include "nsCSPService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIChannelPolicy.h"
#include "nsIChannelEventSink.h"
#include "nsIPropertyBag2.h"
#include "nsIWritablePropertyBag2.h"
#include "nsError.h"
#include "nsChannelProperties.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "mozilla/Preferences.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"

using namespace mozilla;

/* Keeps track of whether or not CSP is enabled */
bool CSPService::sCSPEnabled = true;

#ifdef PR_LOGGING
static PRLogModuleInfo* gCspPRLog;
#endif

CSPService::CSPService()
{
  Preferences::AddBoolVarCache(&sCSPEnabled, "security.csp.enable");

#ifdef PR_LOGGING
  if (!gCspPRLog)
    gCspPRLog = PR_NewLogModule("CSP");
#endif
}

CSPService::~CSPService()
{
  mAppStatusCache.Clear();
}

NS_IMPL_ISUPPORTS(CSPService, nsIContentPolicy, nsIChannelEventSink)

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
  if (!aContentLocation)
    return NS_ERROR_FAILURE;

#ifdef PR_LOGGING
  {
    nsAutoCString location;
    aContentLocation->GetSpec(location);
    PR_LOG(gCspPRLog, PR_LOG_DEBUG,
           ("CSPService::ShouldLoad called for %s", location.get()));
  }
#endif

  // default decision, CSP can revise it if there's a policy to enforce
  *aDecision = nsIContentPolicy::ACCEPT;

  // No need to continue processing if CSP is disabled
  if (!sCSPEnabled)
    return NS_OK;

  // shortcut for about: chrome: and resource: and javascript: uris since
  // they're not subject to CSP content policy checks.
  bool schemeMatch = false;
  NS_ENSURE_SUCCESS(aContentLocation->SchemeIs("about", &schemeMatch), NS_OK);
  if (schemeMatch)
    return NS_OK;
  NS_ENSURE_SUCCESS(aContentLocation->SchemeIs("chrome", &schemeMatch), NS_OK);
  if (schemeMatch)
    return NS_OK;
  NS_ENSURE_SUCCESS(aContentLocation->SchemeIs("resource", &schemeMatch), NS_OK);
  if (schemeMatch)
    return NS_OK;
  NS_ENSURE_SUCCESS(aContentLocation->SchemeIs("javascript", &schemeMatch), NS_OK);
  if (schemeMatch)
    return NS_OK;


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
  nsAutoCString contentOrigin;
  aContentLocation->GetPrePath(contentOrigin);
  if (aRequestPrincipal && !mAppStatusCache.Get(contentOrigin, &status)) {
    aRequestPrincipal->GetAppStatus(&status);
    mAppStatusCache.Put(contentOrigin, status);
  }

  if (status == nsIPrincipal::APP_STATUS_CERTIFIED) {
    // The CSP for certified apps is :
    // "default-src *; script-src 'self'; object-src 'none'; style-src 'self'"
    // That means we can optimize for this case by:
    // - loading only same origin scripts and stylesheets.
    // - never loading objects.
    // - accepting everything else.

    switch (aContentType) {
      case nsIContentPolicy::TYPE_SCRIPT:
      case nsIContentPolicy::TYPE_STYLESHEET:
        {
          nsAutoCString sourceOrigin;
          aRequestOrigin->GetPrePath(sourceOrigin);
          if (!sourceOrigin.Equals(contentOrigin)) {
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

  // find the principal of the document that initiated this request and see
  // if it has a CSP policy object
  nsCOMPtr<nsINode> node(do_QueryInterface(aRequestContext));
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (node) {
    principal = node->NodePrincipal();
    principal->GetCsp(getter_AddRefs(csp));

    if (csp) {
#ifdef PR_LOGGING
      {
        uint32_t numPolicies = 0;
        nsresult rv = csp->GetPolicyCount(&numPolicies);
        if (NS_SUCCEEDED(rv)) {
          for (uint32_t i=0; i<numPolicies; i++) {
            nsAutoString policy;
            csp->GetPolicy(i, policy);
            PR_LOG(gCspPRLog, PR_LOG_DEBUG,
                   ("Document has CSP[%d]: %s", i,
                   NS_ConvertUTF16toUTF8(policy).get()));
          }
        }
      }
#endif
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
#ifdef PR_LOGGING
  else {
    nsAutoCString uriSpec;
    aContentLocation->GetSpec(uriSpec);
    PR_LOG(gCspPRLog, PR_LOG_DEBUG,
           ("COULD NOT get nsINode for location: %s", uriSpec.get()));
  }
#endif

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

  // get the Content Security Policy and load type from the property bag
  nsCOMPtr<nsISupports> policyContainer;
  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(oldChannel));
  if (!props)
    return NS_OK;

  props->GetPropertyAsInterface(NS_CHANNEL_PROP_CHANNEL_POLICY,
                                NS_GET_IID(nsISupports),
                                getter_AddRefs(policyContainer));

  // see if we have a valid nsIChannelPolicy containing CSP and load type
  nsCOMPtr<nsIChannelPolicy> channelPolicy(do_QueryInterface(policyContainer));
  if (!channelPolicy)
    return NS_OK;

  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  channelPolicy->GetContentSecurityPolicy(getter_AddRefs(supports));
  csp = do_QueryInterface(supports);
  uint32_t loadType;
  channelPolicy->GetLoadType(&loadType);

  // if no CSP in the channelPolicy, nothing for us to add to the channel
  if (!csp)
    return NS_OK;

  /* Since redirecting channels don't call into nsIContentPolicy, we call our
   * Content Policy implementation directly when redirects occur. When channels
   * are created using NS_NewChannel(), callers can optionally pass in a
   * nsIChannelPolicy containing a CSP object and load type, which is placed in
   * the new channel's property bag. This container is propagated forward when
   * channels redirect.
   */

  // Does the CSP permit this host for this type of load?
  // If not, cancel the load now.
  nsCOMPtr<nsIURI> newUri;
  newChannel->GetURI(getter_AddRefs(newUri));
  nsCOMPtr<nsIURI> originalUri;
  oldChannel->GetOriginalURI(getter_AddRefs(originalUri));
  int16_t aDecision = nsIContentPolicy::ACCEPT;
  csp->ShouldLoad(loadType,        // load type per nsIContentPolicy (uint32_t)
                  newUri,          // nsIURI
                  nullptr,          // nsIURI
                  nullptr,          // nsISupports
                  EmptyCString(),  // ACString - MIME guess
                  originalUri,     // nsISupports - extra
                  &aDecision);

#ifdef PR_LOGGING
  if (newUri) {
    nsAutoCString newUriSpec("None");
    newUri->GetSpec(newUriSpec);
    PR_LOG(gCspPRLog, PR_LOG_DEBUG,
           ("CSPService::AsyncOnChannelRedirect called for %s",
            newUriSpec.get()));
  }
  if (aDecision == 1)
    PR_LOG(gCspPRLog, PR_LOG_DEBUG,
           ("CSPService::AsyncOnChannelRedirect ALLOWING request."));
  else
    PR_LOG(gCspPRLog, PR_LOG_DEBUG,
           ("CSPService::AsyncOnChannelRedirect CANCELLING request."));
#endif

  // if ShouldLoad doesn't accept the load, cancel the request
  if (aDecision != 1) {
    autoCallback.DontCallback();
    return NS_BINDING_FAILED;
  }

  // the redirect is permitted, so propagate the Content Security Policy
  // and load type to the redirecting channel
  nsresult rv;
  nsCOMPtr<nsIWritablePropertyBag2> props2 = do_QueryInterface(newChannel);
  if (props2) {
    rv = props2->SetPropertyAsInterface(NS_CHANNEL_PROP_CHANNEL_POLICY,
                                        channelPolicy);
    if (NS_SUCCEEDED(rv)) {
      return NS_OK;
    }
  }

  // The redirecting channel isn't a writable property bag, we won't be able
  // to enforce the load policy if it redirects again, so we stop it now.
  nsAutoCString newUriSpec;
  rv = newUri->GetSpec(newUriSpec);
  NS_ConvertUTF8toUTF16 unicodeSpec(newUriSpec);
  const char16_t *formatParams[] = { unicodeSpec.get() };
  if (NS_SUCCEEDED(rv)) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Redirect Error"), nullptr,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "InvalidRedirectChannelWarning",
                                    formatParams, 1);
  }

  return NS_BINDING_FAILED;
}
