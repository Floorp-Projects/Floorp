/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoWindowStateHelper.h"

#include "mozilla/dom/Event.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::dom;

/****************************************************************
 ****************** nsAutoWindowStateHelper *********************
 ****************************************************************/

nsAutoWindowStateHelper::nsAutoWindowStateHelper(nsIDOMWindow *aWindow)
  : mWindow(aWindow),
    mDefaultEnabled(DispatchEventToChrome("DOMWillOpenModalDialog"))
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));

  if (window) {
    window->EnterModalState();
  }
}

nsAutoWindowStateHelper::~nsAutoWindowStateHelper()
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mWindow));

  if (window) {
    window->LeaveModalState();
  }

  if (mDefaultEnabled) {
    DispatchEventToChrome("DOMModalDialogClosed");
  }
}

bool
nsAutoWindowStateHelper::DispatchEventToChrome(const char *aEventName)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mWindow);
  if (!window || (window->IsInnerWindow() && !window->IsCurrentInnerWindow())) {
    return true;
  }

  // The functions of nsContentUtils do not provide the required behavior,
  // so the following is inlined.
  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    return true;
  }

  ErrorResult rv;
  nsRefPtr<Event> event = doc->CreateEvent(NS_LITERAL_STRING("Events"), rv);
  if (rv.Failed()) {
    return false;
  }
  NS_ENSURE_TRUE(NS_SUCCEEDED(event->InitEvent(NS_ConvertASCIItoUTF16(aEventName), true, true)), false);
  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<EventTarget> target = do_QueryInterface(window);
  bool defaultActionEnabled;
  target->DispatchEvent(event, &defaultActionEnabled);
  return defaultActionEnabled;
}
