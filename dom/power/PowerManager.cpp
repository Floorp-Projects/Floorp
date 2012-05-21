/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "PowerManager.h"
#include "WakeLock.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

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

bool
PowerManager::CheckPermission()
{
  if (nsContentUtils::IsCallerChrome()) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(win, false);
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(win->GetExtantDocument());
  NS_ENSURE_TRUE(doc, false);

  nsCOMPtr<nsIURI> uri;
  doc->NodePrincipal()->GetURI(getter_AddRefs(uri));

  if (!nsContentUtils::URIIsChromeOrInPref(uri, "dom.power.whitelist")) {
    return false;
  }

  return true;
}

NS_IMETHODIMP
PowerManager::Reboot()
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  pmService->Reboot();

  return NS_OK;
}

NS_IMETHODIMP
PowerManager::PowerOff()
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_STATE(pmService);

  pmService->PowerOff();

  return NS_OK;
}

NS_IMETHODIMP
PowerManager::AddWakeLockListener(nsIDOMMozWakeLockListener *aListener)
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  // already added? bail out.
  if (mListeners.Contains(aListener))
    return NS_OK;

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::RemoveWakeLockListener(nsIDOMMozWakeLockListener *aListener)
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  mListeners.RemoveElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetWakeLockState(const nsAString &aTopic, nsAString &aState)
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

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
  for (PRUint32 i = 0; i < listeners.Length(); ++i) {
    listeners[i]->Callback(aTopic, aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetScreenEnabled(bool *aEnabled)
{
  if (!CheckPermission()) {
    *aEnabled = true;
    return NS_OK;
  }

  *aEnabled = hal::GetScreenEnabled();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::SetScreenEnabled(bool aEnabled)
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  // TODO bug 707589: When the screen's state changes, all visible windows
  // should fire a visibility change event.
  hal::SetScreenEnabled(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetScreenBrightness(double *aBrightness)
{
  if (!CheckPermission()) {
    *aBrightness = 1;
    return NS_OK;
  }

  *aBrightness = hal::GetScreenBrightness();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::SetScreenBrightness(double aBrightness)
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  NS_ENSURE_TRUE(0 <= aBrightness && aBrightness <= 1, NS_ERROR_INVALID_ARG);
  hal::SetScreenBrightness(aBrightness);
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::GetCpuSleepAllowed(bool *aAllowed)
{
  if (!CheckPermission()) {
    *aAllowed = true;
    return NS_OK;
  }

  *aAllowed = hal::GetCpuSleepAllowed();
  return NS_OK;
}

NS_IMETHODIMP
PowerManager::SetCpuSleepAllowed(bool aAllowed)
{
  NS_ENSURE_TRUE(CheckPermission(), NS_ERROR_DOM_SECURITY_ERR);

  hal::SetCpuSleepAllowed(aAllowed);
  return NS_OK;
}

} // power
} // dom
} // mozilla
