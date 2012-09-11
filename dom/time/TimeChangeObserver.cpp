/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeChangeObserver.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsPIDOMWindow.h"
#include "nsDOMEvent.h"
#include "nsContentUtils.h"

using namespace mozilla::hal;
using namespace mozilla;

StaticAutoPtr<nsSystemTimeChangeObserver> sObserver;

nsSystemTimeChangeObserver* nsSystemTimeChangeObserver::GetInstance()
{
  if (!sObserver) {
    sObserver = new nsSystemTimeChangeObserver();
    ClearOnShutdown(&sObserver);
  }
  return sObserver;
}

nsSystemTimeChangeObserver::~nsSystemTimeChangeObserver()
{
  mWindowListeners.Clear();
  UnregisterSystemTimeChangeObserver(this);
}

void
nsSystemTimeChangeObserver::Notify(const SystemTimeChange& aReason)
{
  //Copy mWindowListeners and iterate over windowListeners instead because
  //mWindowListeners may be modified while we loop.
  nsTArray<nsWeakPtr> windowListeners;
  for (uint32 i = 0; i < mWindowListeners.Length(); i++) {
    windowListeners.AppendElement(mWindowListeners.SafeElementAt(i));
  }

  for (int32 i = windowListeners.Length() - 1; i >= 0; i--) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryReferent(windowListeners[i]);
    if (!window) {
      mWindowListeners.RemoveElement(windowListeners[i]);
      return;
    }

    nsCOMPtr<nsIDOMDocument> domdoc;
    window->GetDocument(getter_AddRefs(domdoc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
    if (!domdoc) {
      return;
    }

    nsContentUtils::DispatchTrustedEvent(doc, window,
      NS_LITERAL_STRING("moztimechange"), /* bubbles = */ true,
      /* canceable = */ false);
  }
}

nsresult
nsSystemTimeChangeObserver::AddWindowListener(nsIDOMWindow* aWindow)
{
  return GetInstance()->AddWindowListenerImpl(aWindow);
}

nsresult
nsSystemTimeChangeObserver::AddWindowListenerImpl(nsIDOMWindow* aWindow)
{
  if (!aWindow) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (mWindowListeners.IndexOf(NS_GetWeakReference(aWindow)) !=
      nsTArray<nsIDOMWindow*>::NoIndex) {
    return NS_OK;
  }

  if (mWindowListeners.Length() == 0) {
    RegisterSystemTimeChangeObserver(sObserver);
  }

  mWindowListeners.AppendElement(NS_GetWeakReference(aWindow));
  return NS_OK;
}

nsresult
nsSystemTimeChangeObserver::RemoveWindowListener(nsIDOMWindow* aWindow)
{
  if (!sObserver) {
    return NS_OK;
  }

  return GetInstance()->RemoveWindowListenerImpl(aWindow);
}

nsresult
nsSystemTimeChangeObserver::RemoveWindowListenerImpl(nsIDOMWindow* aWindow)
{
  mWindowListeners.RemoveElement(NS_GetWeakReference(aWindow));

  if (mWindowListeners.Length() == 0) {
    UnregisterSystemTimeChangeObserver(sObserver);
  }

  return NS_OK;
}
