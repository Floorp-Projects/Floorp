/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/Hal.h"
#include "mozilla/HalWakeLock.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "jsprf.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "PowerManagerService.h"
#include "WakeLock.h"

// For _exit().
#ifdef XP_WIN
#include <process.h>
#else
#include <unistd.h>
#endif

namespace mozilla {
namespace dom {
namespace power {

using namespace hal;

NS_IMPL_ISUPPORTS(PowerManagerService, nsIPowerManagerService)

/* static */ StaticRefPtr<PowerManagerService> PowerManagerService::sSingleton;

/* static */ already_AddRefed<PowerManagerService>
PowerManagerService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new PowerManagerService();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }

  RefPtr<PowerManagerService> service = sSingleton.get();
  return service.forget();
}

void
PowerManagerService::Init()
{
  RegisterWakeLockObserver(this);
}

PowerManagerService::~PowerManagerService()
{
  UnregisterWakeLockObserver(this);
}

void
PowerManagerService::ComputeWakeLockState(const WakeLockInformation& aWakeLockInfo,
                                          nsAString &aState)
{
  WakeLockState state = hal::ComputeWakeLockState(aWakeLockInfo.numLocks(),
                                                  aWakeLockInfo.numHidden());
  switch (state) {
  case WAKE_LOCK_STATE_UNLOCKED:
    aState.AssignLiteral("unlocked");
    break;
  case WAKE_LOCK_STATE_HIDDEN:
    aState.AssignLiteral("locked-background");
    break;
  case WAKE_LOCK_STATE_VISIBLE:
    aState.AssignLiteral("locked-foreground");
    break;
  }
}

void
PowerManagerService::Notify(const WakeLockInformation& aWakeLockInfo)
{
  nsAutoString state;
  ComputeWakeLockState(aWakeLockInfo, state);

  /**
   * Copy the listeners list before we walk through the callbacks
   * because the callbacks may install new listeners. We expect no
   * more than one listener per window, so it shouldn't be too long.
   */
  AutoTArray<nsCOMPtr<nsIDOMMozWakeLockListener>, 2> listeners(mWakeLockListeners);

  for (uint32_t i = 0; i < listeners.Length(); ++i) {
    listeners[i]->Callback(aWakeLockInfo.topic(), state);
  }
}

NS_IMETHODIMP
PowerManagerService::AddWakeLockListener(nsIDOMMozWakeLockListener *aListener)
{
  if (mWakeLockListeners.Contains(aListener))
    return NS_OK;

  mWakeLockListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PowerManagerService::RemoveWakeLockListener(nsIDOMMozWakeLockListener *aListener)
{
  mWakeLockListeners.RemoveElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PowerManagerService::GetWakeLockState(const nsAString &aTopic, nsAString &aState)
{
  WakeLockInformation info;
  GetWakeLockInfo(aTopic, &info);

  ComputeWakeLockState(info, aState);

  return NS_OK;
}

already_AddRefed<WakeLock>
PowerManagerService::NewWakeLock(const nsAString& aTopic,
                                 nsPIDOMWindowInner* aWindow,
                                 mozilla::ErrorResult& aRv)
{
  RefPtr<WakeLock> wakelock = new WakeLock();
  aRv = wakelock->Init(aTopic, aWindow);
  if (aRv.Failed()) {
    return nullptr;
  }

  return wakelock.forget();
}

NS_IMETHODIMP
PowerManagerService::NewWakeLock(const nsAString &aTopic,
                                 mozIDOMWindow *aWindow,
                                 nsISupports **aWakeLock)
{
  mozilla::ErrorResult rv;
  RefPtr<WakeLock> wakelock =
    NewWakeLock(aTopic, nsPIDOMWindowInner::From(aWindow), rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  nsCOMPtr<nsIDOMEventListener> eventListener = wakelock.get();
  eventListener.forget(aWakeLock);
  return NS_OK;
}

already_AddRefed<WakeLock>
PowerManagerService::NewWakeLockOnBehalfOfProcess(const nsAString& aTopic,
                                                  ContentParent* aContentParent)
{
  RefPtr<WakeLock> wakelock = new WakeLock();
  nsresult rv = wakelock->Init(aTopic, aContentParent);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return wakelock.forget();
}

} // namespace power
} // namespace dom
} // namespace mozilla
