/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeChangeObserver.h"
#include "mozilla/Hal.h"
#include "mozilla/Observer.h"
#include "mozilla/HalTypes.h"
#include "nsWeakPtr.h"
#include "nsTObserverArray.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsIDocument.h"

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::services;

class nsSystemTimeChangeObserver : public SystemClockChangeObserver,
                                   public SystemTimezoneChangeObserver
{
  typedef nsTObserverArray<nsWeakPtr> ListenerArray;
public:
  static nsSystemTimeChangeObserver* GetInstance();
  virtual ~nsSystemTimeChangeObserver();

  // Implementing hal::SystemClockChangeObserver::Notify()
  void Notify(const int64_t& aClockDeltaMS);

  // Implementing hal::SystemTimezoneChangeObserver::Notify()
  void Notify(
    const mozilla::hal::SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo);

  nsresult AddWindowListenerImpl(nsPIDOMWindowInner* aWindow);
  nsresult RemoveWindowListenerImpl(nsPIDOMWindowInner* aWindow);

private:
  nsSystemTimeChangeObserver() { };
  ListenerArray mWindowListeners;
  void FireMozTimeChangeEvent();
};

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
  UnregisterSystemClockChangeObserver(this);
  UnregisterSystemTimezoneChangeObserver(this);
}

void
nsSystemTimeChangeObserver::FireMozTimeChangeEvent()
{
  ListenerArray::ForwardIterator iter(mWindowListeners);
  while (iter.HasMore()) {
    nsWeakPtr weakWindow = iter.GetNext();
    nsCOMPtr<nsPIDOMWindowInner> innerWindow = do_QueryReferent(weakWindow);
    nsCOMPtr<nsPIDOMWindowOuter> outerWindow;
    nsCOMPtr<nsIDocument> document;
    if (!innerWindow ||
        !(document = innerWindow->GetExtantDoc()) ||
        !(outerWindow = innerWindow->GetOuterWindow())) {
      mWindowListeners.RemoveElement(weakWindow);
      continue;
    }

    nsContentUtils::DispatchTrustedEvent(document, outerWindow,
      NS_LITERAL_STRING("moztimechange"), /* bubbles = */ true,
      /* canceable = */ false);
  }
}

void
nsSystemTimeChangeObserver::Notify(const int64_t& aClockDeltaMS)
{
  // Notify observers that the system clock has been adjusted.
  nsCOMPtr<nsIObserverService> observerService = GetObserverService();
  if (observerService) {
    nsString dataStr;
    dataStr.AppendFloat(static_cast<double>(aClockDeltaMS));
    observerService->NotifyObservers(
      nullptr, "system-clock-change", dataStr.get());
  }

  FireMozTimeChangeEvent();
}

void
nsSystemTimeChangeObserver::Notify(
  const SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo)
{
  FireMozTimeChangeEvent();
}

nsresult
mozilla::time::AddWindowListener(nsPIDOMWindowInner* aWindow)
{
  return nsSystemTimeChangeObserver::GetInstance()->AddWindowListenerImpl(aWindow);
}

nsresult
nsSystemTimeChangeObserver::AddWindowListenerImpl(nsPIDOMWindowInner* aWindow)
{
  if (!aWindow) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsWeakPtr windowWeakRef = do_GetWeakReference(aWindow);
  NS_ASSERTION(windowWeakRef, "nsIDOMWindow implementations shuld support weak ref");

  if (mWindowListeners.IndexOf(windowWeakRef) !=
      ListenerArray::array_type::NoIndex) {
    return NS_OK;
  }

  if (mWindowListeners.IsEmpty()) {
    RegisterSystemClockChangeObserver(sObserver);
    RegisterSystemTimezoneChangeObserver(sObserver);
  }

  mWindowListeners.AppendElement(windowWeakRef);
  return NS_OK;
}

nsresult
mozilla::time::RemoveWindowListener(nsPIDOMWindowInner* aWindow)
{
  if (!sObserver) {
    return NS_OK;
  }

  return nsSystemTimeChangeObserver::GetInstance()->RemoveWindowListenerImpl(aWindow);
}

nsresult
nsSystemTimeChangeObserver::RemoveWindowListenerImpl(nsPIDOMWindowInner* aWindow)
{
  if (!aWindow) {
    return NS_OK;
  }

  nsWeakPtr windowWeakRef = do_GetWeakReference(aWindow);
  mWindowListeners.RemoveElement(windowWeakRef);

  if (mWindowListeners.IsEmpty()) {
    UnregisterSystemClockChangeObserver(sObserver);
    UnregisterSystemTimezoneChangeObserver(sObserver);
  }

  return NS_OK;
}
