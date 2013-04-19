/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoWindowStateHelper.h"

#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsString.h"
#include "nsGUIEvent.h"

/****************************************************************
 ****************** nsAutoWindowStateHelper *********************
 ****************************************************************/

nsAutoWindowStateHelper::nsAutoWindowStateHelper(nsIDOMWindow *aWindow)
  : mWindow(aWindow),
    mDefaultEnabled(DispatchEventToChrome("DOMWillOpenModalDialog"))
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));

  if (window) {
    mCallerWindow = window->EnterModalState();
  }
}

nsAutoWindowStateHelper::~nsAutoWindowStateHelper()
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mWindow));

  if (window) {
    window->LeaveModalState(mCallerWindow);
  }

  if (mDefaultEnabled) {
    DispatchEventToChrome("DOMModalDialogClosed");
  }
}

bool
nsAutoWindowStateHelper::DispatchEventToChrome(const char *aEventName)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mWindow);
  if (!window) {
    return true;
  }

  // The functions of nsContentUtils do not provide the required behavior,
  // so the following is inlined.
  nsIDOMDocument* doc = window->GetExtantDocument();
  if (!doc) {
    return true;
  }

  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_TRUE(NS_SUCCEEDED(event->InitEvent(NS_ConvertASCIItoUTF16(aEventName), true, true)), false);
  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(window));
  bool defaultActionEnabled;
  target->DispatchEvent(event, &defaultActionEnabled);
  return defaultActionEnabled;
}
