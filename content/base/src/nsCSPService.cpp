/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brandon Sterne <bsterne@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prlog.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIObserver.h"
#include "nsIDocument.h"
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
            csp->ShouldLoad(aContentType,
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

NS_IMETHODIMP
CSPService::ShouldProcess(PRUint32         aContentType,
                          nsIURI           *aContentLocation,
                          nsIURI           *aRequestOrigin,
                          nsISupports      *aRequestContext,
                          const nsACString &aMimeTypeGuess,
                          nsISupports      *aExtra,
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
  PRInt16 aDecision = nsIContentPolicy::ACCEPT;
  csp->ShouldLoad(loadType,        // load type per nsIContentPolicy (PRUint32)
                  newUri,          // nsIURI
                  nsnull,          // nsIURI
                  nsnull,          // nsISupports
                  EmptyCString(),  // ACString - MIME guess
                  nsnull,          // nsISupports - extra
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
  nsCOMPtr<nsIWritablePropertyBag2> props2 = do_QueryInterface(newChannel, &rv);
  if (props2)
    props2->SetPropertyAsInterface(NS_CHANNEL_PROP_CHANNEL_POLICY,
                                   channelPolicy);

  return NS_OK;
}
