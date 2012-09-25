/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "nsGlobalWindow.h"
#include "nsIDOMFile.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsContentUtils.h"
#include "nsBlobProtocolHandler.h"

namespace mozilla {
namespace dom {

void
URL::CreateObjectURL(nsISupports* aGlobal, nsIDOMBlob* aBlob,
                     const objectURLOptions& aOptions,
                     nsAString& aResult,
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

  nsresult rv = aBlob->GetInternalUrl(doc->NodePrincipal(), aResult);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  doc->RegisterFileDataUri(NS_LossyConvertUTF16toASCII(aResult));
}

void
URL::RevokeObjectURL(nsISupports* aGlobal, const nsAString& aURL)
{
  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aGlobal);
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
    nsBlobProtocolHandler::GetFileDataEntryPrincipal(asciiurl);
  bool subsumes;
  if (principal && winPrincipal &&
      NS_SUCCEEDED(winPrincipal->Subsumes(principal, &subsumes)) &&
      subsumes) {
    if (window->GetExtantDoc()) {
      window->GetExtantDoc()->UnregisterFileDataUri(asciiurl);
    }
    nsBlobProtocolHandler::RemoveFileDataEntry(asciiurl);
  }
}

}
}
