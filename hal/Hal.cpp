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
#include "mozilla/dom/TabChild.h"
#include "mozilla/Util.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Observer.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsIWebNavigation.h"
#include "nsITabChild.h"
#include "nsIDocShell.h"

using namespace mozilla::dom;
using namespace mozilla::services;

#define PROXY_IF_SANDBOXED(_call)                 \
  do {                                            \
    if (InSandbox()) {                            \
      hal_sandbox::_call;                         \
    } else {                                      \
      hal_impl::_call;                            \
    }                                             \
  } while (0)

#define RETURN_PROXY_IF_SANDBOXED(_call)          \
  do {                                            \
    if (InSandbox()) {                            \
      return hal_sandbox::_call;                  \
    } else {                                      \
      return hal_impl::_call;                     \
    }                                             \
  } while (0)

namespace mozilla {
namespace hal {

PRLogModuleInfo *sHalLog = PR_LOG_DEFINE("hal");

namespace {

void
AssertMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

bool
InSandbox()
{
  return GeckoProcessType_Content == XRE_GetProcessType();
}

bool
WindowIsActive(nsIDOMWindow *window)
{
  NS_ENSURE_TRUE(window, false);

  nsCOMPtr<nsIDOMDocument> doc;
  window->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, false);

  bool hidden = true;
  doc->GetMozHidden(&hidden);
  return !hidden;
}

nsAutoPtr<WindowIdentifier::IDArrayType> gLastIDToVibrate;

// This observer makes sure we delete gLastIDToVibrate, so we don't
// leak.
class ShutdownObserver : public nsIObserver
{
public:
  ShutdownObserver() {}
  virtual ~ShutdownObserver() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports *subject, const char *aTopic,
                     const PRUnichar *aData)
  {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
    gLastIDToVibrate = nsnull;
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(ShutdownObserver, nsIObserver);

void InitLastIDToVibrate()
{
  gLastIDToVibrate = new WindowIdentifier::IDArrayType();

  nsCOMPtr<nsIObserverService> observerService = GetObserverService();
  if (!observerService) {
    NS_WARNING("Could not get observer service!");
    return;
  }

  ShutdownObserver *obs = new ShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

} // anonymous namespace

WindowIdentifier::WindowIdentifier()
  : mWindow(nsnull)
  , mIsEmpty(true)
{
}

WindowIdentifier::WindowIdentifier(nsIDOMWindow *window)
  : mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(nsCOMPtr<nsIDOMWindow> &window)
  : mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(const nsTArray<uint64> &id, nsIDOMWindow *window)
  : mID(id)
  , mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(const WindowIdentifier &other)
  : mID(other.mID)
  , mWindow(other.mWindow)
  , mIsEmpty(other.mIsEmpty)
{
}

const InfallibleTArray<uint64>&
WindowIdentifier::AsArray() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mID;
}

bool
WindowIdentifier::HasTraveledThroughIPC() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mID.Length() >= 2;
}

void
WindowIdentifier::AppendProcessID()
{
  MOZ_ASSERT(!mIsEmpty);
  mID.AppendElement(ContentChild::GetSingleton()->GetID());
}

uint64
WindowIdentifier::GetWindowID() const
{
  MOZ_ASSERT(!mIsEmpty);
  nsCOMPtr<nsPIDOMWindow> pidomWindow = do_QueryInterface(mWindow);
  if (!pidomWindow) {
    return uint64(-1);
  }
  return pidomWindow->WindowID();
}

nsIDOMWindow*
WindowIdentifier::GetWindow() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mWindow;
}

void
Vibrate(const nsTArray<uint32>& pattern, const WindowIdentifier &id)
{
  AssertMainThread();

  // Only active windows may start vibrations.  If |id| hasn't gone
  // through the IPC layer -- that is, if our caller is the outside
  // world, not hal_proxy -- check whether the window is active.  If
  // |id| has gone through IPC, don't check the window's visibility;
  // only the window corresponding to the bottommost process has its
  // visibility state set correctly.
  if (!id.HasTraveledThroughIPC() && !WindowIsActive(id.GetWindow())) {
    HAL_LOG(("Vibrate: Window is inactive, dropping vibrate."));
    return;
  }

  if (InSandbox()) {
    hal_sandbox::Vibrate(pattern, id);
  }
  else {
    if (!gLastIDToVibrate)
      InitLastIDToVibrate();
    *gLastIDToVibrate = id.AsArray();

    HAL_LOG(("Vibrate: Forwarding to hal_impl."));

    // hal_impl doesn't need |id|. Send it an empty id, which will
    // assert if it's used.
    hal_impl::Vibrate(pattern, WindowIdentifier());
  }
}

void
CancelVibrate(const WindowIdentifier &id)
{
  AssertMainThread();

  // Although only active windows may start vibrations, a window may
  // cancel its own vibration even if it's no longer active.
  //
  // After a window is marked as inactive, it sends a CancelVibrate
  // request.  We want this request to cancel a playing vibration
  // started by that window, so we certainly don't want to reject the
  // cancellation request because the window is now inactive.
  //
  // But it could be the case that, after this window became inactive,
  // some other window came along and started a vibration.  We don't
  // want this window's cancellation request to cancel that window's
  // actively-playing vibration!
  //
  // To solve this problem, we keep track of the id of the last window
  // to start a vibration, and only accepts cancellation requests from
  // the same window.  All other cancellation requests are ignored.

  if (InSandbox()) {
    hal_sandbox::CancelVibrate(id);
  }
  else if (*gLastIDToVibrate == id.AsArray()) {
    // Don't forward our ID to hal_impl. It doesn't need it, and we
    // don't want it to be tempted to read it.  The empty identifier
    // will assert if it's used.
    HAL_LOG(("CancelVibrate: Forwarding to hal_impl."));
    hal_impl::CancelVibrate(WindowIdentifier());
  }
}

class BatteryObserversManager
{
public:
  void AddObserver(BatteryObserver* aObserver) {
    if (!mObservers) {
      mObservers = new ObserverList<BatteryInformation>();
    }

    mObservers->AddObserver(aObserver);

    if (mObservers->Length() == 1) {
      PROXY_IF_SANDBOXED(EnableBatteryNotifications());
    }
  }

  void RemoveObserver(BatteryObserver* aObserver) {
    mObservers->RemoveObserver(aObserver);

    if (mObservers->Length() == 0) {
      PROXY_IF_SANDBOXED(DisableBatteryNotifications());

      delete mObservers;
      mObservers = 0;

      delete mBatteryInfo;
      mBatteryInfo = 0;
    }
  }

  void CacheBatteryInformation(const BatteryInformation& aBatteryInfo) {
    if (mBatteryInfo) {
      delete mBatteryInfo;
    }
    mBatteryInfo = new BatteryInformation(aBatteryInfo);
  }

  bool HasCachedBatteryInformation() const {
    return mBatteryInfo;
  }

  void GetCachedBatteryInformation(BatteryInformation* aBatteryInfo) const {
    *aBatteryInfo = *mBatteryInfo;
  }

  void Broadcast(const BatteryInformation& aBatteryInfo) {
    MOZ_ASSERT(mObservers);
    mObservers->Broadcast(aBatteryInfo);
  }

private:
  ObserverList<BatteryInformation>* mObservers;
  BatteryInformation*               mBatteryInfo;
};

static BatteryObserversManager sBatteryObservers;

void
RegisterBatteryObserver(BatteryObserver* aBatteryObserver)
{
  AssertMainThread();
  sBatteryObservers.AddObserver(aBatteryObserver);
}

void
UnregisterBatteryObserver(BatteryObserver* aBatteryObserver)
{
  AssertMainThread();
  sBatteryObservers.RemoveObserver(aBatteryObserver);
}

// EnableBatteryNotifications isn't defined on purpose.
// DisableBatteryNotifications isn't defined on purpose.

void
GetCurrentBatteryInformation(BatteryInformation* aBatteryInfo)
{
  AssertMainThread();

  if (sBatteryObservers.HasCachedBatteryInformation()) {
    sBatteryObservers.GetCachedBatteryInformation(aBatteryInfo);
  } else {
    PROXY_IF_SANDBOXED(GetCurrentBatteryInformation(aBatteryInfo));
    sBatteryObservers.CacheBatteryInformation(*aBatteryInfo);
  }
}

void NotifyBatteryChange(const BatteryInformation& aBatteryInfo)
{
  AssertMainThread();

  sBatteryObservers.CacheBatteryInformation(aBatteryInfo);
  sBatteryObservers.Broadcast(aBatteryInfo);
}

bool GetScreenEnabled()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetScreenEnabled());
}

void SetScreenEnabled(bool enabled)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenEnabled(enabled));
}

double GetScreenBrightness()
{
  AssertMainThread();
  RETURN_PROXY_IF_SANDBOXED(GetScreenBrightness());
}

void SetScreenBrightness(double brightness)
{
  AssertMainThread();
  PROXY_IF_SANDBOXED(SetScreenBrightness(clamped(brightness, 0.0, 1.0)));
}

} // namespace hal
} // namespace mozilla
