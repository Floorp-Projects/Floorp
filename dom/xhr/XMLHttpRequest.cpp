/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequest.h"
#include "XMLHttpRequestMainThread.h"
#include "XMLHttpRequestWorker.h"
#include "mozilla/Logging.h"
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
    nsCOMPtr<nsIScriptObjectPrincipal> principal =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (!global || !principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
    if (window) {
      Document* document = window->GetExtantDoc();
      if (NS_WARN_IF(!document)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      cookieJarSettings = document->CookieJarSettings();
    } else {
      // We are here because this is a sandbox.
      cookieJarSettings =
          net::CookieJarSettings::Create(principal->GetPrincipal());
    }

    RefPtr<XMLHttpRequestMainThread> req = new XMLHttpRequestMainThread(global);
    req->Construct(principal->GetPrincipal(), cookieJarSettings, false);
    req->InitParameters(aParams.mMozAnon, aParams.mMozSystem);
    return req.forget();
  }

  return XMLHttpRequestWorker::Construct(aGlobal, aParams, aRv);
}

}  // namespace mozilla::dom
