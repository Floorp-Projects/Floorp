/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigatorBinding.h"
#include "PaymentProviderUtils.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "nsIDocShell.h"

using namespace mozilla::dom;

/* static */ bool
PaymentProviderUtils::EnabledForScope(JSContext* aCx,
                                      JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win =
    do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(aGlobal));
  NS_ENSURE_TRUE(win, false);

  nsIDocShell *docShell = win->GetDocShell();
  NS_ENSURE_TRUE(docShell, false);

  nsString paymentRequestId;
  docShell->GetPaymentRequestId(paymentRequestId);

  return !paymentRequestId.IsEmpty();
}
