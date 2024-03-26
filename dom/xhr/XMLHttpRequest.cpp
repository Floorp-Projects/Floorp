/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequest.h"
#include "XMLHttpRequestMainThread.h"
#include "XMLHttpRequestWorker.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/net/CookieJarSettings.h"

mozilla::LazyLogModule gXMLHttpRequestLog("XMLHttpRequest");

namespace mozilla::dom {

/* static */
already_AddRefed<XMLHttpRequest> XMLHttpRequest::Constructor(
    const GlobalObject& aGlobal, const MozXMLHttpRequestParameters& aParams,
    ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIGlobalObject> global =
        do_QueryInterface(aGlobal.GetAsSupports());
    nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (!global || !scriptPrincipal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
    nsCOMPtr<nsIPrincipal> principal = scriptPrincipal->GetPrincipal();
    if (window) {
      Document* document = window->GetExtantDoc();
      if (NS_WARN_IF(!document)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      cookieJarSettings = document->CookieJarSettings();
    } else {
      // We are here because this is a sandbox.
      cookieJarSettings = net::CookieJarSettings::Create(principal);
    }

    RefPtr<XMLHttpRequestMainThread> req = new XMLHttpRequestMainThread(global);
    req->Construct(principal, cookieJarSettings, false);

    bool isAnon = false;
    if (aParams.mMozAnon.WasPassed()) {
      isAnon = aParams.mMozAnon.Value();
    } else {
      isAnon =
          StaticPrefs::network_fetch_systemDefaultsToOmittingCredentials() &&
          (aParams.mMozSystem || principal->IsSystemPrincipal());
    }
    req->InitParameters(isAnon, aParams.mMozSystem);
    return req.forget();
  }

  return XMLHttpRequestWorker::Construct(aGlobal, aParams, aRv);
}

}  // namespace mozilla::dom
