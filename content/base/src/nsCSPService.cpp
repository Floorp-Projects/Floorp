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
#include "nsNetError.h"
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
}

NS_IMPL_ISUPPORTS2(CSPService, nsIContentPolicy, nsIChannelEventSink)

/* nsIContentPolicy implementation */
NS_IMETHODIMP
CSPService::ShouldLoad(PRUint32 aContentType,
                       nsIURI *aContentLocation,
                       nsIURI *aRequestOrigin,
                       nsISupports *aRequestContext,
                       const nsACString &aMimeTypeGuess,
                       nsISupports *aExtra,
                       nsIPrincipal *aRequestPrincipal,
                       PRInt16 *aDecision)
{
    if (!aContentLocation)
        return NS_ERROR_FAILURE;

#ifdef PR_LOGGING
    {
        nsCAutoString location;
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
            nsAutoString policy;
            csp->GetPolicy(policy);
            PR_LOG(gCspPRLog, PR_LOG_DEBUG,
                    ("Document has CSP: %s",
                     NS_ConvertUTF16toUTF8(policy).get()));
#endif
            // obtain the enforcement decision
            // (don't pass aExtra, we use that slot for redirects)
            csp->ShouldLoad(aContentType,
                            aContentLocation,
                            aRequestOrigin,
                            aRequestContext,
                            aMimeTypeGuess,
                            nsnull,
                            aDecision);
        }
    }
#ifdef PR_LOGGING
    else {
        nsCAutoString uriSpec;
        aContentLocation->GetSpec(uriSpec);
        PR_LOG(gCspPRLog, PR_LOG_DEBUG,
            ("COULD NOT get nsINode for location: %s", uriSpec.get()));
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP
CSPService::ShouldProcess(PRUint32         aContentType,
                          nsIURI           *aContentLocation,
                          nsIURI           *aRequestOrigin,
                          nsISupports      *aRequestContext,
                          const nsACString &aMimeTypeGuess,
                          nsISupports      *aExtra,
                          nsIPrincipal     *aRequestPrincipal,
                          PRInt16          *aDecision)
{
    if (!aContentLocation)
        return NS_ERROR_FAILURE;

    // default decision is to accept the item
    *aDecision = nsIContentPolicy::ACCEPT;

    // No need to continue processing if CSP is disabled
    if (!sCSPEnabled)
        return NS_OK;

    // find the nsDocument that initiated this request and see if it has a
    // CSP policy object
    nsCOMPtr<nsINode> node(do_QueryInterface(aRequestContext));
    nsCOMPtr<nsIPrincipal> principal;
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    if (node) {
        principal = node->NodePrincipal();
        principal->GetCsp(getter_AddRefs(csp));

        if (csp) {
#ifdef PR_LOGGING
            nsAutoString policy;
            csp->GetPolicy(policy);
            PR_LOG(gCspPRLog, PR_LOG_DEBUG,
                  ("shouldProcess - document has policy: %s",
                    NS_ConvertUTF16toUTF8(policy).get()));
#endif
            // obtain the enforcement decision
            csp->ShouldProcess(aContentType,
                               aContentLocation,
                               aRequestOrigin,
                               aRequestContext,
                               aMimeTypeGuess,
                               aExtra,
                               aDecision);
        }
    }
#ifdef PR_LOGGING
    else {
        nsCAutoString uriSpec;
        aContentLocation->GetSpec(uriSpec);
        PR_LOG(gCspPRLog, PR_LOG_DEBUG,
            ("COULD NOT get nsINode for location: %s", uriSpec.get()));
    }
#endif
    return NS_OK;
}

/* nsIChannelEventSink implementation */
NS_IMETHODIMP
CSPService::AsyncOnChannelRedirect(nsIChannel *oldChannel,
                                   nsIChannel *newChannel,
                                   PRUint32 flags,
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

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  channelPolicy->GetContentSecurityPolicy(getter_AddRefs(csp));
  PRUint32 loadType;
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
  PRInt16 aDecision = nsIContentPolicy::ACCEPT;
  csp->ShouldLoad(loadType,        // load type per nsIContentPolicy (PRUint32)
                  newUri,          // nsIURI
                  nsnull,          // nsIURI
                  nsnull,          // nsISupports
                  EmptyCString(),  // ACString - MIME guess
                  originalUri,     // nsISupports - extra
                  &aDecision);

#ifdef PR_LOGGING
  if (newUri) {
    nsCAutoString newUriSpec("None");
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
  nsCAutoString newUriSpec;
  rv = newUri->GetSpec(newUriSpec);
  const PRUnichar *formatParams[] = { NS_ConvertUTF8toUTF16(newUriSpec).get() };
  if (NS_SUCCEEDED(rv)) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    "Redirect Error", nsnull,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "InvalidRedirectChannelWarning",
                                    formatParams, 1);
  }

  return NS_BINDING_FAILED;
}
