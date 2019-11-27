/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSecurityManager.h"
#include "nsCSPContext.h"
#include "nsContentSecurityUtils.h"
#include "mozilla/dom/FramingChecker.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/StaticPrefs_fission.h"

#include "nsIMultiPartChannel.h"
#include "nsIObserverService.h"
#include "nsIHttpProtocolHandler.h"

using namespace mozilla;

namespace {
StaticRefPtr<DOMSecurityManager> gDOMSecurityManager;
}  // namespace

NS_INTERFACE_MAP_BEGIN(DOMSecurityManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(DOMSecurityManager)
NS_IMPL_RELEASE(DOMSecurityManager)

/* static */
void DOMSecurityManager::Initialize() {
  MOZ_ASSERT(!gDOMSecurityManager);

  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return;
  }

  RefPtr<DOMSecurityManager> service = new DOMSecurityManager();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->AddObserver(service, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC, false);
  obs->AddObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  gDOMSecurityManager = service.forget();
}

/* static */
void DOMSecurityManager::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gDOMSecurityManager) {
    return;
  }

  RefPtr<DOMSecurityManager> service = gDOMSecurityManager.forget();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->RemoveObserver(service, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC);
  obs->RemoveObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
}

NS_IMETHODIMP
DOMSecurityManager::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
    return NS_OK;
  }

  MOZ_ASSERT(!strcmp(aTopic, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC));

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aSubject);
  if (NS_WARN_IF(!channel)) {
    return NS_OK;
  }

  // Bug 1574372: Download should be fully done in the parent process.
  // Unfortunately we currently can not determine whether a load will
  // result in a download in the parent process. Hence, if running in
  // fission-mode, then we will enforce the security checks for
  // frame-ancestors and x-frame-options here in the parent using some
  // additional carveouts for downloads but if we run in
  // non-fission-mode then we do those two security checks within
  // Document::StartDocumentLoad in the content process.
  bool fissionEnabled = StaticPrefs::fission_autostart();
  if (fissionEnabled) {
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    nsresult rv =
        ParseCSPAndEnforceFrameAncestorCheck(channel, getter_AddRefs(csp));
    if (NS_FAILED(rv)) {
      return rv;
    }
    // X-Frame-Options needs to be enforced after CSP frame-ancestors
    // checks because if frame-ancestors is present, then x-frame-options
    // will be discarded
    rv = EnforeXFrameOptionsCheck(channel, csp);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult DOMSecurityManager::ParseCSPAndEnforceFrameAncestorCheck(
    nsIChannel* aChannel, nsIContentSecurityPolicy** aOutCSP) {
  MOZ_ASSERT(aChannel);

  // CSP can only hang off an http channel, if this channel is not
  // an http channel then there is nothing to do here.
  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = nsContentSecurityUtils::GetHttpChannelFromPotentialMultiPart(
      aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!httpChannel) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();
  // frame-ancestor check only makes sense for subdocument and object loads,
  // if this is not a load of such type, there is nothing to do here.
  if (contentType != nsIContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != nsIContentPolicy::TYPE_OBJECT) {
    return NS_OK;
  }

  nsAutoCString tCspHeaderValue, tCspROHeaderValue;

  Unused << httpChannel->GetResponseHeader(
      NS_LITERAL_CSTRING("content-security-policy"), tCspHeaderValue);

  Unused << httpChannel->GetResponseHeader(
      NS_LITERAL_CSTRING("content-security-policy-report-only"),
      tCspROHeaderValue);

  // if there are no CSP values, then there is nothing to do here.
  if (tCspHeaderValue.IsEmpty() && tCspROHeaderValue.IsEmpty()) {
    return NS_OK;
  }

  NS_ConvertASCIItoUTF16 cspHeaderValue(tCspHeaderValue);
  NS_ConvertASCIItoUTF16 cspROHeaderValue(tCspROHeaderValue);

  RefPtr<nsCSPContext> csp = new nsCSPContext();
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(resultPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> selfURI;
  aChannel->GetURI(getter_AddRefs(selfURI));

  nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
  nsAutoString referrerSpec;
  if (referrerInfo) {
    referrerInfo->GetComputedReferrerSpec(referrerSpec);
  }
  uint64_t innerWindowID = loadInfo->GetInnerWindowID();

  rv = csp->SetRequestContextWithPrincipal(resultPrincipal, selfURI,
                                           referrerSpec, innerWindowID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // ----- if there's a full-strength CSP header, apply it.
  if (!cspHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspHeaderValue, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- if there's a report-only CSP header, apply it.
  if (!cspROHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspROHeaderValue, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- Enforce frame-ancestor policy on any applied policies
  bool safeAncestry = false;
  // PermitsAncestry sends violation reports when necessary
  rv = csp->PermitsAncestry(loadInfo, &safeAncestry);

  if (NS_FAILED(rv) || !safeAncestry) {
    // stop!  ERROR page!
    aChannel->Cancel(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION);
  }

  // return the CSP for x-frame-options check
  csp.forget(aOutCSP);

  return NS_OK;
}

nsresult DOMSecurityManager::EnforeXFrameOptionsCheck(
    nsIChannel* aChannel, nsIContentSecurityPolicy* aCsp) {
  MOZ_ASSERT(aChannel);

  if (!FramingChecker::CheckFrameOptions(aChannel, aCsp)) {
    // stop!  ERROR page!
    aChannel->Cancel(NS_ERROR_XFO_VIOLATION);
  }
  return NS_OK;
}
