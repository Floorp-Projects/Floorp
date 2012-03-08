/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kan-Ru Chen <kchen@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

/* static */ nsRefPtr<PowerManagerService> PowerManagerService::sSingleton;

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

  nsCOMPtr<nsIDOMMozWakeLock> wl =
    do_QueryInterface(NS_ISUPPORTS_CAST(nsIDOMMozWakeLock*, wakelock));
  wl.forget(aWakeLock);

  return NS_OK;
}

} // power
} // dom
} // mozilla
