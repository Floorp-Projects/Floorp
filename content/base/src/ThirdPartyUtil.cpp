/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThirdPartyUtil.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIHttpChannelInternal.h"
#include "nsIDOMWindow.h"
#include "nsILoadContext.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsThreadUtils.h"

NS_IMPL_ISUPPORTS(ThirdPartyUtil, mozIThirdPartyUtil)

nsresult
ThirdPartyUtil::Init()
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_AVAILABLE);

  nsresult rv;
  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
  return rv;
}

// Determine if aFirstDomain is a different base domain to aSecondURI; or, if
// the concept of base domain does not apply, determine if the two hosts are not
// string-identical.
nsresult
ThirdPartyUtil::IsThirdPartyInternal(const nsCString& aFirstDomain,
                                     nsIURI* aSecondURI,
                                     bool* aResult)
{
  NS_ENSURE_ARG(aSecondURI);

  // Get the base domain for aSecondURI.
  nsCString secondDomain;
  nsresult rv = GetBaseDomain(aSecondURI, secondDomain);
  if (NS_FAILED(rv))
    return rv;

  // Check strict equality.
  *aResult = aFirstDomain != secondDomain;
  return NS_OK;
}

// Get the URI associated with a window.
NS_IMETHODIMP
ThirdPartyUtil::GetURIFromWindow(nsIDOMWindow* aWin, nsIURI** result)
{
  nsresult rv;
  nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin = do_QueryInterface(aWin);
  if (!scriptObjPrin) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIPrincipal* prin = scriptObjPrin->GetPrincipal();
  if (!prin) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURI> uri;
  rv = prin->GetURI(result);
  return rv;
}

// Determine if aFirstURI is third party with respect to aSecondURI. See docs
// for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyURI(nsIURI* aFirstURI,
                                nsIURI* aSecondURI,
                                bool* aResult)
{
  NS_ENSURE_ARG(aFirstURI);
  NS_ENSURE_ARG(aSecondURI);
  NS_ASSERTION(aResult, "null outparam pointer");

  nsCString firstHost;
  nsresult rv = GetBaseDomain(aFirstURI, firstHost);
  if (NS_FAILED(rv))
    return rv;

  return IsThirdPartyInternal(firstHost, aSecondURI, aResult);
}

// Determine if any URI of the window hierarchy of aWindow is foreign with
// respect to aSecondURI. See docs for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyWindow(nsIDOMWindow* aWindow,
                                   nsIURI* aURI,
                                   bool* aResult)
{
  NS_ENSURE_ARG(aWindow);
  NS_ASSERTION(aResult, "null outparam pointer");

  bool result;

  // Get the URI of the window, and its base domain.
  nsresult rv;
  nsCOMPtr<nsIURI> currentURI;
  rv = GetURIFromWindow(aWindow, getter_AddRefs(currentURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString bottomDomain;
  rv = GetBaseDomain(currentURI, bottomDomain);
  if (NS_FAILED(rv))
    return rv;

  if (aURI) {
    // Determine whether aURI is foreign with respect to currentURI.
    rv = IsThirdPartyInternal(bottomDomain, aURI, &result);
    if (NS_FAILED(rv))
      return rv;

    if (result) {
      *aResult = true;
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMWindow> current = aWindow, parent;
  nsCOMPtr<nsIURI> parentURI;
  do {
    // We use GetScriptableParent rather than GetParent because we consider
    // <iframe mozbrowser/mozapp> to be a top-level frame.
    rv = current->GetScriptableParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);

    if (SameCOMIdentity(parent, current)) {
      // We're at the topmost content window. We already know the answer.
      *aResult = false;
      return NS_OK;
    }

    rv = GetURIFromWindow(parent, getter_AddRefs(parentURI));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = IsThirdPartyInternal(bottomDomain, parentURI, &result);
    if (NS_FAILED(rv))
      return rv;

    if (result) {
      *aResult = true;
      return NS_OK;
    }

    current = parent;
    currentURI = parentURI;
  } while (1);

  NS_NOTREACHED("should've returned");
  return NS_ERROR_UNEXPECTED;
}

// Determine if the URI associated with aChannel or any URI of the window
// hierarchy associated with the channel is foreign with respect to aSecondURI.
// See docs for mozIThirdPartyUtil.
NS_IMETHODIMP 
ThirdPartyUtil::IsThirdPartyChannel(nsIChannel* aChannel,
                                    nsIURI* aURI,
                                    bool* aResult)
{
  NS_ENSURE_ARG(aChannel);
  NS_ASSERTION(aResult, "null outparam pointer");

  nsresult rv;
  bool doForce = false;
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
    do_QueryInterface(aChannel);
  if (httpChannelInternal) {
    rv = httpChannelInternal->GetForceAllowThirdPartyCookie(&doForce);
    NS_ENSURE_SUCCESS(rv, rv);

    // If aURI was not supplied, and we're forcing, then we're by definition
    // not foreign. If aURI was supplied, we still want to check whether it's
    // foreign with respect to the channel URI. (The forcing only applies to
    // whatever window hierarchy exists above the channel.)
    if (doForce && !aURI) {
      *aResult = false;
      return NS_OK;
    }
  }

  // Obtain the URI from the channel, and its base domain.
  nsCOMPtr<nsIURI> channelURI;
  aChannel->GetURI(getter_AddRefs(channelURI));
  NS_ENSURE_TRUE(channelURI, NS_ERROR_INVALID_ARG);

  nsCString channelDomain;
  rv = GetBaseDomain(channelURI, channelDomain);
  if (NS_FAILED(rv))
    return rv;

  if (aURI) {
    // Determine whether aURI is foreign with respect to channelURI.
    bool result;
    rv = IsThirdPartyInternal(channelDomain, aURI, &result);
    if (NS_FAILED(rv))
     return rv;

    // If it's foreign, or we're forcing, we're done.
    if (result || doForce) {
      *aResult = result;
      return NS_OK;
    }
  }

  // Find the associated window and its parent window.
  nsCOMPtr<nsILoadContext> ctx;
  NS_QueryNotificationCallbacks(aChannel, ctx);
  if (!ctx) return NS_ERROR_INVALID_ARG;

  // If there is no window, the consumer kicking off the load didn't provide one
  // to the channel. This is limited to loads of certain types of resources. If
  // those loads require cookies, the forceAllowThirdPartyCookie property should
  // be set on the channel.
  nsCOMPtr<nsIDOMWindow> ourWin, parentWin;
  ctx->GetAssociatedWindow(getter_AddRefs(ourWin));
  if (!ourWin) return NS_ERROR_INVALID_ARG;

  // We use GetScriptableParent rather than GetParent because we consider
  // <iframe mozbrowser/mozapp> to be a top-level frame.
  ourWin->GetScriptableParent(getter_AddRefs(parentWin));
  NS_ENSURE_TRUE(parentWin, NS_ERROR_INVALID_ARG);

  // Check whether this is the document channel for this window (representing a
  // load of a new page). In that situation we want to avoid comparing
  // channelURI to ourWin, since what's in ourWin right now will be replaced as
  // the channel loads.  This covers the case of a freshly kicked-off load
  // (e.g. the user typing something in the location bar, or clicking on a
  // bookmark), where the window's URI hasn't yet been set, and will be bogus.
  // It also covers situations where a subframe is navigated to someting that
  // is same-origin with all its ancestors.  This is a bit of a nasty hack, but
  // we will hopefully flag these channels better later.
  nsLoadFlags flags;
  rv = aChannel->GetLoadFlags(&flags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (flags & nsIChannel::LOAD_DOCUMENT_URI) {
    if (SameCOMIdentity(ourWin, parentWin)) {
      // We only need to compare aURI to the channel URI -- the window's will be
      // bogus. We already know the answer.
      *aResult = false;
      return NS_OK;
    }

    // Make sure to still compare to ourWin's ancestors
    ourWin = parentWin;
  }

  // Check the window hierarchy. This covers most cases for an ordinary page
  // load from the location bar.
  return IsThirdPartyWindow(ourWin, channelURI, aResult);
}

NS_IMETHODIMP
ThirdPartyUtil::GetTopWindowForChannel(nsIChannel* aChannel, nsIDOMWindow** aWin)
{
  NS_ENSURE_ARG(aWin);

  nsresult rv;
  // Find the associated window and its parent window.
  nsCOMPtr<nsILoadContext> ctx;
  NS_QueryNotificationCallbacks(aChannel, ctx);
  if (!ctx) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMWindow> window;
  rv = ctx->GetAssociatedWindow(getter_AddRefs(window));
  if (!window) {
    return NS_ERROR_INVALID_ARG;
  }

  rv = window->GetTop(aWin);
  return rv;
}

// Get the base domain for aHostURI; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". Only properly-formed URI's are tolerated, though a trailing
// dot may be present. If aHostURI is an IP address, an alias such as
// 'localhost', an eTLD such as 'co.uk', or the empty string, aBaseDomain will
// be the exact host. The result of this function should only be used in exact
// string comparisons, since substring comparisons will not be valid for the
// special cases elided above.
NS_IMETHODIMP
ThirdPartyUtil::GetBaseDomain(nsIURI* aHostURI,
                              nsACString& aBaseDomain)
{
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
  if (aBaseDomain.IsEmpty()) {
    bool isFileURI = false;
    aHostURI->SchemeIs("file", &isFileURI);
    NS_ENSURE_TRUE(isFileURI, NS_ERROR_INVALID_ARG);
  }

  return NS_OK;
}
