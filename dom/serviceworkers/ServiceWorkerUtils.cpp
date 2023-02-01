/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUtils.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"
#include "nsIURL.h"
#include "nsPrintfCString.h"

namespace mozilla::dom {

static bool IsServiceWorkersTestingEnabledInWindow(JSObject* const aGlobal) {
  if (const nsCOMPtr<nsPIDOMWindowInner> innerWindow =
          Navigator::GetWindowFromGlobal(aGlobal)) {
    if (auto* bc = innerWindow->GetBrowsingContext()) {
      return bc->Top()->ServiceWorkersTestingEnabled();
    }
  }
  return false;
}

static bool IsInPrivateBrowsing(JSContext* const aCx) {
  if (const nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx)) {
    if (const nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull()) {
      return principal->GetPrivateBrowsingId() > 0;
    }
  }
  return false;
}

bool ServiceWorkersEnabled(JSContext* aCx, JSObject* aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::dom_serviceWorkers_enabled()) {
    return false;
  }

  // xpc::CurrentNativeGlobal below requires rooting
  JS::Rooted<JSObject*> global(aCx, aGlobal);

  if (IsInPrivateBrowsing(aCx)) {
    return false;
  }

  // Allow a webextension principal to register a service worker script with
  // a moz-extension url only if 'extensions.service_worker_register.allowed'
  // is true.
  if (!StaticPrefs::extensions_serviceWorkerRegister_allowed()) {
    nsIPrincipal* principal = nsContentUtils::SubjectPrincipal(aCx);
    if (principal && BasePrincipal::Cast(principal)->AddonPolicy()) {
      return false;
    }
  }

  if (IsSecureContextOrObjectIsFromSecureContext(aCx, global)) {
    return true;
  }

  return StaticPrefs::dom_serviceWorkers_testing_enabled() ||
         IsServiceWorkersTestingEnabledInWindow(global);
}

bool ServiceWorkerVisible(JSContext* aCx, JSObject* aGlobal) {
  if (NS_IsMainThread()) {
    // We want to expose ServiceWorker interface only when
    // navigator.serviceWorker is available. Currently it may not be available
    // with some reasons:
    // 1. navigator.serviceWorker is not supported in workers. (bug 1131324)
    return ServiceWorkersEnabled(aCx, aGlobal);
  }

  // We are already in ServiceWorker and interfaces need to be exposed for e.g.
  // globalThis.registration.serviceWorker. Note that navigator.serviceWorker
  // is still not supported. (bug 1131324)
  return IS_INSTANCE_OF(ServiceWorkerGlobalScope, aGlobal);
}

bool ServiceWorkerRegistrationDataIsValid(
    const ServiceWorkerRegistrationData& aData) {
  return !aData.scope().IsEmpty() && !aData.currentWorkerURL().IsEmpty() &&
         !aData.cacheName().IsEmpty();
}

namespace {

void CheckForSlashEscapedCharsInPath(nsIURI* aURI, const char* aURLDescription,
                                     ErrorResult& aRv) {
  MOZ_ASSERT(aURI);

  // A URL that can't be downcast to a standard URL is an invalid URL and should
  // be treated as such and fail with SecurityError.
  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (NS_WARN_IF(!url)) {
    // This really should not happen, since the caller checks that we
    // have an http: or https: URL!
    aRv.ThrowInvalidStateError("http: or https: URL without a concept of path");
    return;
  }

  nsAutoCString path;
  nsresult rv = url->GetFilePath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Again, should not happen.
    aRv.ThrowInvalidStateError("http: or https: URL without a concept of path");
    return;
  }

  ToLowerCase(path);
  if (path.Find("%2f") != kNotFound || path.Find("%5c") != kNotFound) {
    nsPrintfCString err("%s contains %%2f or %%5c", aURLDescription);
    aRv.ThrowTypeError(err);
  }
}

}  // anonymous namespace

void ServiceWorkerScopeAndScriptAreValid(const ClientInfo& aClientInfo,
                                         nsIURI* aScopeURI, nsIURI* aScriptURI,
                                         ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aScopeURI);
  MOZ_DIAGNOSTIC_ASSERT(aScriptURI);

  auto principalOrErr = aClientInfo.GetPrincipal();
  if (NS_WARN_IF(principalOrErr.isErr())) {
    aRv.ThrowInvalidStateError("Can't make security decisions about Client");
    return;
  }

  auto hasHTTPScheme = [](nsIURI* aURI) -> bool {
    return aURI->SchemeIs("http") || aURI->SchemeIs("https");
  };
  auto hasMozExtScheme = [](nsIURI* aURI) -> bool {
    return aURI->SchemeIs("moz-extension");
  };

  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();

  auto isExtension = !!BasePrincipal::Cast(principal)->AddonPolicy();
  auto hasValidURISchemes = !isExtension ? hasHTTPScheme : hasMozExtScheme;

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 3.
  if (!hasValidURISchemes(aScriptURI)) {
    auto message = !isExtension
                       ? "Script URL's scheme is not 'http' or 'https'"_ns
                       : "Script URL's scheme is not 'moz-extension'"_ns;
    aRv.ThrowTypeError(message);
    return;
  }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 4.
  CheckForSlashEscapedCharsInPath(aScriptURI, "script URL", aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 8.
  if (!hasValidURISchemes(aScopeURI)) {
    auto message = !isExtension
                       ? "Scope URL's scheme is not 'http' or 'https'"_ns
                       : "Scope URL's scheme is not 'moz-extension'"_ns;
    aRv.ThrowTypeError(message);
    return;
  }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 9.
  CheckForSlashEscapedCharsInPath(aScopeURI, "scope URL", aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // The refs should really be empty coming in here, but if someone
  // injects bad data into IPC, who knows.  So let's revalidate that.
  nsAutoCString ref;
  Unused << aScopeURI->GetRef(ref);
  if (NS_WARN_IF(!ref.IsEmpty())) {
    aRv.ThrowSecurityError("Non-empty fragment on scope URL");
    return;
  }

  Unused << aScriptURI->GetRef(ref);
  if (NS_WARN_IF(!ref.IsEmpty())) {
    aRv.ThrowSecurityError("Non-empty fragment on script URL");
    return;
  }

  // Unfortunately we don't seem to have an obvious window id here; in
  // particular ClientInfo does not have one.
  nsresult rv = principal->CheckMayLoadWithReporting(
      aScopeURI, false /* allowIfInheritsPrincipal */, 0 /* innerWindowID */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowSecurityError("Scope URL is not same-origin with Client");
    return;
  }

  rv = principal->CheckMayLoadWithReporting(
      aScriptURI, false /* allowIfInheritsPrincipal */, 0 /* innerWindowID */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowSecurityError("Script URL is not same-origin with Client");
    return;
  }
}

}  // namespace mozilla::dom
