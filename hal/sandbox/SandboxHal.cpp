/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "Hal.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/battery/Types.h"
#include "mozilla/Observer.h"
#include "mozilla/unused.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace mozilla {
namespace hal_sandbox {

static PHalChild* sHal;
static PHalChild*
Hal()
{
  if (!sHal) {
    sHal = ContentChild::GetSingleton()->SendPHalConstructor();
  }
  return sHal;
}

void
Vibrate(const nsTArray<uint32>& pattern, const WindowIdentifier &id)
{
  HAL_LOG(("Vibrate: Sending to parent process."));

  AutoInfallibleTArray<uint32, 8> p(pattern);

  WindowIdentifier newID(id);
  newID.AppendProcessID();
  Hal()->SendVibrate(p, newID.AsArray(), GetTabChildFrom(newID.GetWindow()));
}

void
CancelVibrate(const WindowIdentifier &id)
{
  HAL_LOG(("CancelVibrate: Sending to parent process."));

  WindowIdentifier newID(id);
  newID.AppendProcessID();
  Hal()->SendCancelVibrate(newID.AsArray(), GetTabChildFrom(newID.GetWindow()));
}

void
EnableBatteryNotifications()
{
  Hal()->SendEnableBatteryNotifications();
}

void
DisableBatteryNotifications()
{
  Hal()->SendDisableBatteryNotifications();
}

void
GetCurrentBatteryInformation(BatteryInformation* aBatteryInfo)
{
  Hal()->SendGetCurrentBatteryInformation(aBatteryInfo);
}

bool
GetScreenEnabled()
{
  bool enabled = false;
  Hal()->SendGetScreenEnabled(&enabled);
  return enabled;
}

void
SetScreenEnabled(bool enabled)
{
  Hal()->SendSetScreenEnabled(enabled);
}

double
GetScreenBrightness()
{
  double brightness = 0;
  Hal()->SendGetScreenBrightness(&brightness);
  return brightness;
}

void
SetScreenBrightness(double brightness)
{
  Hal()->SendSetScreenBrightness(brightness);
}

class HalParent : public PHalParent
                , public BatteryObserver {
public:
  NS_OVERRIDE virtual bool
  RecvVibrate(const InfallibleTArray<unsigned int>& pattern,
              const InfallibleTArray<uint64> &id,
              PBrowserParent *browserParent)
  {
    // Check whether browserParent is active.  We should have already
    // checked that the corresponding window is active, but this check
    // isn't redundant.  A window may be inactive in an active
    // browser.  And a window is not notified synchronously when it's
    // deactivated, so the window may think it's active when the tab
    // is actually inactive.
    TabParent *tabParent = static_cast<TabParent*>(browserParent);
    if (!tabParent->Active()) {
      HAL_LOG(("RecvVibrate: Tab is not active. Cancelling."));
      return true;
    }

    // Forward to hal::, not hal_impl::, because we might be a
    // subprocess of another sandboxed process.  The hal:: entry point
    // will do the right thing.
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(tabParent->GetBrowserDOMWindow());
    WindowIdentifier newID(id, window);
    hal::Vibrate(pattern, newID);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvCancelVibrate(const InfallibleTArray<uint64> &id,
                    PBrowserParent *browserParent)
  {
    TabParent *tabParent = static_cast<TabParent*>(browserParent);
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(tabParent->GetBrowserDOMWindow());
    WindowIdentifier newID(id, window);
    hal::CancelVibrate(newID);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvEnableBatteryNotifications() {
    hal::RegisterBatteryObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvDisableBatteryNotifications() {
    hal::UnregisterBatteryObserver(this);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetCurrentBatteryInformation(BatteryInformation* aBatteryInfo) {
    hal::GetCurrentBatteryInformation(aBatteryInfo);
    return true;
  }

  void Notify(const BatteryInformation& aBatteryInfo) {
    unused << SendNotifyBatteryChange(aBatteryInfo);
  }

  NS_OVERRIDE virtual bool
  RecvGetScreenEnabled(bool *enabled)
  {
    *enabled = hal::GetScreenEnabled();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvSetScreenEnabled(const bool &enabled)
  {
    hal::SetScreenEnabled(enabled);
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvGetScreenBrightness(double *brightness)
  {
    *brightness = hal::GetScreenBrightness();
    return true;
  }

  NS_OVERRIDE virtual bool
  RecvSetScreenBrightness(const double &brightness)
  {
    hal::SetScreenBrightness(brightness);
    return true;
  }
};

class HalChild : public PHalChild {
public:
  NS_OVERRIDE virtual bool
  RecvNotifyBatteryChange(const BatteryInformation& aBatteryInfo) {
    hal::NotifyBatteryChange(aBatteryInfo);
    return true;
  }
};

PHalChild* CreateHalChild() {
  return new HalChild();
}

PHalParent* CreateHalParent() {
  return new HalParent();
}

} // namespace hal_sandbox
} // namespace mozilla
