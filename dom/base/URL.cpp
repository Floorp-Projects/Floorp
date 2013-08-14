/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "nsGlobalWindow.h"
#include "nsIDOMFile.h"
#include "DOMMediaStream.h"
#include "mozilla/dom/MediaSource.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsHostObjectProtocolHandler.h"

namespace mozilla {
namespace dom {

void
URL::CreateObjectURL(const GlobalObject& aGlobal, nsIDOMBlob* aBlob,
                     const objectURLOptions& aOptions,
                     nsString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal.Get(), aBlob,
                          NS_LITERAL_CSTRING(BLOBURI_SCHEME), aOptions, aResult,
                          aError);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                     const mozilla::dom::objectURLOptions& aOptions,
                     nsString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal.Get(), &aStream,
                          NS_LITERAL_CSTRING(MEDIASTREAMURI_SCHEME), aOptions,
                          aResult, aError);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                     const objectURLOptions& aOptions,
                     nsString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal.Get(), &aSource,
                          NS_LITERAL_CSTRING(MEDIASOURCEURI_SCHEME), aOptions,
                          aResult, aError);
}

void
URL::CreateObjectURLInternal(nsISupports* aGlobal, nsISupports* aObject,
                             const nsACString& aScheme,
                             const mozilla::dom::objectURLOptions& aOptions,
                             nsString& aResult,
                             ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aGlobal);
  nsGlobalWindow* window = static_cast<nsGlobalWindow*>(w.get());
  NS_PRECONDITION(!window || window->IsInnerWindow(),
                  "Should be inner window");

  if (!window || !window->GetExtantDoc()) {
    aError.Throw(NS_ERROR_INVALID_POINTER);
    return;
  }

  nsIDocument* doc = window->GetExtantDoc();

  nsCString url;
  nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(aScheme, aObject,
    doc->NodePrincipal(), url);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  doc->RegisterHostObjectUri(url);
  CopyASCIItoUTF16(url, aResult);
}

void
URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL)
{
  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aGlobal.Get());
  nsGlobalWindow* window = static_cast<nsGlobalWindow*>(w.get());
  NS_PRECONDITION(!window || window->IsInnerWindow(),
                  "Should be inner window");
  if (!window)
    return;

  NS_LossyConvertUTF16toASCII asciiurl(aURL);

  nsIPrincipal* winPrincipal = window->GetPrincipal();
  if (!winPrincipal) {
    return;
  }

  nsIPrincipal* principal =
    nsHostObjectProtocolHandler::GetDataEntryPrincipal(asciiurl);
  bool subsumes;
  if (principal && winPrincipal &&
      NS_SUCCEEDED(winPrincipal->Subsumes(principal, &subsumes)) &&
      subsumes) {
    if (window->GetExtantDoc()) {
      window->GetExtantDoc()->UnregisterHostObjectUri(asciiurl);
    }
    nsHostObjectProtocolHandler::RemoveDataEntry(asciiurl);
  }
}

}
}
