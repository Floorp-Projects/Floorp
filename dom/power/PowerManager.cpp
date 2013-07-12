/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "PowerManager.h"
#include "WakeLock.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIDocument.h"
#include "nsIPermissionManager.h"
#include "nsIPowerManagerService.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsError.h"

DOMCI_DATA(MozPowerManager, mozilla::dom::power::PowerManager)

namespace mozilla {
namespace dom {
namespace power {

NS_INTERFACE_MAP_BEGIN(PowerManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozPowerManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozPowerManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozWakeLockListener)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozPowerManager)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(PowerManager)
NS_IMPL_RELEASE(PowerManager)

nsresult
PowerManager::Init(nsIDOMWindow *aWindow)
{
  mWindow = do_GetWeakReference(aWindow);

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  // Add ourself to the global notification list.
  pmService->AddWakeLockListener(this);
  return NS_OK;
}

nsresult
PowerManager::Shutdown()
{
  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  // Remove ourself from the global notification list.
  pmService->RemoveWakeLockListener(this);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::Reboot()
{
  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  pmService->Reboot();

  return NS_OK;
}

NS_IMETHODIMP
PowerManager::FactoryReset()
{
  hal::FactoryReset();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::PowerOff()
{
  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  pmService->PowerOff();

  return NS_OK;
}

NS_IMETHODIMP
PowerManager::AddWakeLockListener(nsIDOMMozWakeLockListener *aListener)
{
  // already added? bail out.
  if (mListeners.Contains(aListener))
    return NS_OK;

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::RemoveWakeLockListener(nsIDOMMozWakeLockListener *aListener)
{
  mListeners.RemoveElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetWakeLockState(const nsAString &aTopic, nsAString &aState)
{
  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  return pmService->GetWakeLockState(aTopic, aState);
}

NS_IMETHODIMP
PowerManager::Callback(const nsAString &aTopic, const nsAString &aState)
{
  /**
   * We maintain a local listener list instead of using the global
   * list so that when the window is destroyed we don't have to
   * cleanup the mess.
   * Copy the listeners list before we walk through the callbacks
   * because the callbacks may install new listeners. We expect no
   * more than one listener per window, so it shouldn't be too long.
   */
  nsAutoTArray<nsCOMPtr<nsIDOMMozWakeLockListener>, 2> listeners(mListeners);
  for (uint32_t i = 0; i < listeners.Length(); ++i) {
    listeners[i]->Callback(aTopic, aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetScreenEnabled(bool *aEnabled)
{
  *aEnabled = hal::GetScreenEnabled();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::SetScreenEnabled(bool aEnabled)
{
  hal::SetScreenEnabled(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetScreenBrightness(double *aBrightness)
{
  *aBrightness = hal::GetScreenBrightness();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::SetScreenBrightness(double aBrightness)
{
  NS_ENSURE_TRUE(0 <= aBrightness && aBrightness <= 1, NS_ERROR_INVALID_ARG);
  hal::SetScreenBrightness(aBrightness);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetCpuSleepAllowed(bool *aAllowed)
{
  *aAllowed = hal::GetCpuSleepAllowed();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::SetCpuSleepAllowed(bool aAllowed)
{
  hal::SetCpuSleepAllowed(aAllowed);
  return NS_OK;
}

already_AddRefed<PowerManager>
PowerManager::CheckPermissionAndCreateInstance(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, nullptr);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, "power", &permission);

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return nullptr;
  }

  nsRefPtr<PowerManager> powerManager = new PowerManager();
  powerManager->Init(aWindow);

  return powerManager.forget();
}

} // power
} // dom
} // mozilla
