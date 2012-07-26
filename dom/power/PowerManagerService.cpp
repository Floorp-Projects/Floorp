/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalWakeLock.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIDOMWindow.h"
#include "PowerManagerService.h"
#include "WakeLock.h"

namespace mozilla {
namespace dom {
namespace power {

NS_IMPL_ISUPPORTS1(PowerManagerService, nsIPowerManagerService)

/* static */ StaticRefPtr<PowerManagerService> PowerManagerService::sSingleton;

/* static */ already_AddRefed<nsIPowerManagerService>
PowerManagerService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new PowerManagerService();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }

  nsCOMPtr<nsIPowerManagerService> service(do_QueryInterface(sSingleton));
  return service.forget();
}

void
PowerManagerService::Init()
{
  hal::RegisterWakeLockObserver(this);
}

PowerManagerService::~PowerManagerService()
{
  hal::UnregisterWakeLockObserver(this);
}

void
PowerManagerService::ComputeWakeLockState(const hal::WakeLockInformation& aWakeLockInfo,
                                          nsAString &aState)
{
  hal::WakeLockState state = hal::ComputeWakeLockState(aWakeLockInfo.numLocks(),
                                                       aWakeLockInfo.numHidden());
  switch (state) {
  case hal::WAKE_LOCK_STATE_UNLOCKED:
    aState.AssignLiteral("unlocked");
    break;
  case hal::WAKE_LOCK_STATE_HIDDEN:
    aState.AssignLiteral("locked-background");
    break;
  case hal::WAKE_LOCK_STATE_VISIBLE:
    aState.AssignLiteral("locked-foreground");
    break;
  }
}

void
PowerManagerService::Notify(const hal::WakeLockInformation& aWakeLockInfo)
{
  nsAutoString state;
  ComputeWakeLockState(aWakeLockInfo, state);

  /**
   * Copy the listeners list before we walk through the callbacks
   * because the callbacks may install new listeners. We expect no
   * more than one listener per window, so it shouldn't be too long.
   */
  nsAutoTArray<nsCOMPtr<nsIDOMMozWakeLockListener>, 2> listeners(mWakeLockListeners);

  for (PRUint32 i = 0; i < listeners.Length(); ++i) {
    listeners[i]->Callback(aWakeLockInfo.topic(), state);
  }
}

NS_IMETHODIMP
PowerManagerService::Reboot()
{
  hal::Reboot();
  return NS_OK;
}

NS_IMETHODIMP
PowerManagerService::PowerOff()
{
  hal::PowerOff();
  return NS_OK;
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
  hal::WakeLockInformation info;
  hal::GetWakeLockInfo(aTopic, &info);

  ComputeWakeLockState(info, aState);

  return NS_OK;
}

NS_IMETHODIMP
PowerManagerService::NewWakeLock(const nsAString &aTopic,
                                 nsIDOMWindow *aWindow,
                                 nsIDOMMozWakeLock **aWakeLock)
{
  nsRefPtr<WakeLock> wakelock = new WakeLock();
  nsresult rv = wakelock->Init(aTopic, aWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMMozWakeLock> wl(wakelock);
  wl.forget(aWakeLock);

  return NS_OK;
}

} // power
} // dom
} // mozilla
