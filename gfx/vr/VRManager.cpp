/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "VRManager.h"
#include "VRManagerParent.h"
#include "gfxVR.h"
#include "gfxVROpenVR.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/Unused.h"

#include "gfxPrefs.h"
#include "gfxVR.h"
#if defined(XP_WIN)
#include "gfxVROculus.h"
#endif
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
#include "gfxVROSVR.h"
#endif
#include "ipc/VRLayerParent.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::gl;

namespace mozilla {
namespace gfx {

static StaticRefPtr<VRManager> sVRManagerSingleton;

/*static*/ void
VRManager::ManagerInit()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sVRManagerSingleton == nullptr) {
    sVRManagerSingleton = new VRManager();
    ClearOnShutdown(&sVRManagerSingleton);
  }
}

VRManager::VRManager()
  : mInitialized(false)
{
  MOZ_COUNT_CTOR(VRManager);
  MOZ_ASSERT(sVRManagerSingleton == nullptr);

  RefPtr<VRDisplayManager> mgr;
  RefPtr<VRControllerManager> controllerMgr;

  /**
   * We must add the VRDisplayManager's to mManagers in a careful order to
   * ensure that we don't detect the same VRDisplay from multiple API's.
   *
   * Oculus comes first, as it will only enumerate Oculus HMD's and is the
   * native interface for Oculus HMD's.
   *
   * OpenvR comes second, as it is the native interface for HTC Vive
   * which is the most common HMD at this time.
   *
   * OSVR will be used if Oculus SDK and OpenVR don't detect any HMDS,
   * to support everyone else.
   */

#if defined(XP_WIN)
  // The Oculus runtime is supported only on Windows
  mgr = VRDisplayManagerOculus::Create();
  if (mgr) {
    mManagers.AppendElement(mgr);
  }
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
  // OpenVR is cross platform compatible
  mgr = VRDisplayManagerOpenVR::Create();
  if (mgr) {
    mManagers.AppendElement(mgr);
    controllerMgr = VRControllerManagerOpenVR::Create();
    if (controllerMgr) {
      mControllerManagers.AppendElement(controllerMgr);
    }
  }

  // OSVR is cross platform compatible
  mgr = VRDisplayManagerOSVR::Create();
  if (mgr) {
      mManagers.AppendElement(mgr);
  }
#endif
  // Enable gamepad extensions while VR is enabled.
  // Preference only can be set at the Parent process.
  if (XRE_IsParentProcess() && gfxPrefs::VREnabled()) {
    Preferences::SetBool("dom.gamepad.extensions.enabled", true);
  }
}

VRManager::~VRManager()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitialized);
  MOZ_COUNT_DTOR(VRManager);
}

void
VRManager::Destroy()
{
  mVRDisplays.Clear();
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Destroy();
  }

  mVRControllers.Clear();
  for (uint32_t i = 0; i < mControllerManagers.Length(); ++i) {
    mControllerManagers[i]->Destroy();
  }
  mInitialized = false;
}

void
VRManager::Init()
{
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Init();
  }

  for (uint32_t i = 0; i < mControllerManagers.Length(); ++i) {
    mControllerManagers[i]->Init();
  }
  mInitialized = true;
}

/* static */VRManager*
VRManager::Get()
{
  MOZ_ASSERT(sVRManagerSingleton != nullptr);

  return sVRManagerSingleton;
}

void
VRManager::AddVRManagerParent(VRManagerParent* aVRManagerParent)
{
  if (mVRManagerParents.IsEmpty()) {
    Init();
  }
  mVRManagerParents.PutEntry(aVRManagerParent);
}

void
VRManager::RemoveVRManagerParent(VRManagerParent* aVRManagerParent)
{
  mVRManagerParents.RemoveEntry(aVRManagerParent);
  if (mVRManagerParents.IsEmpty()) {
    Destroy();
  }
}

void
VRManager::NotifyVsync(const TimeStamp& aVsyncTimestamp)
{
  const double kVRDisplayRefreshMaxDuration = 5000; // milliseconds

  bool bHaveEventListener = false;

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    VRManagerParent *vmp = iter.Get()->GetKey();
    if (mVRDisplays.Count()) {
      Unused << vmp->SendNotifyVSync();
    }
    bHaveEventListener |= vmp->HaveEventListener();
  }

  for (auto iter = mVRDisplays.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRDisplayHost* display = iter.UserData();
    display->NotifyVSync();
  }

  if (bHaveEventListener) {
    // If content has set an EventHandler to be notified of VR display events
    // we must continually refresh the VR display enumeration to check
    // for events that we must fire such as Window.onvrdisplayconnect
    // Note that enumeration itself may activate display hardware, such
    // as Oculus, so we only do this when we know we are displaying content
    // that is looking for VR displays.
    if (mLastRefreshTime.IsNull()) {
      // This is the first vsync, must refresh VR displays
      RefreshVRDisplays();
      RefreshVRControllers();
      mLastRefreshTime = TimeStamp::Now();
    } else {
      // We don't have to do this every frame, so check if we
      // have refreshed recently.
      TimeDuration duration = TimeStamp::Now() - mLastRefreshTime;
      if (duration.ToMilliseconds() > kVRDisplayRefreshMaxDuration) {
        RefreshVRDisplays();
        RefreshVRControllers();
        mLastRefreshTime = TimeStamp::Now();
      }
    }
  }
}

void
VRManager::NotifyVRVsync(const uint32_t& aDisplayID)
{
  for (uint32_t i = 0; i < mControllerManagers.Length(); ++i) {
    mControllerManagers[i]->HandleInput();
  }
  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendNotifyVRVSync(aDisplayID);
  }
}

void
VRManager::RefreshVRDisplays(bool aMustDispatch)
{
  nsTArray<RefPtr<gfx::VRDisplayHost> > displays;

  /** We don't wish to enumerate the same display from multiple managers,
   * so stop as soon as we get a display.
   * It is still possible to get multiple displays from a single manager,
   * but do not wish to mix-and-match for risk of reporting a duplicate.
   *
   * XXX - Perhaps there will be a better way to detect duplicate displays
   *       in the future.
   */
  for (uint32_t i = 0; i < mManagers.Length() && displays.Length() == 0; ++i) {
    mManagers[i]->GetHMDs(displays);
  }

  bool displayInfoChanged = false;

  if (displays.Length() != mVRDisplays.Count()) {
    // Catch cases where a VR display has been removed
    displayInfoChanged = true;
  }

  for (const auto& display: displays) {
    if (!GetDisplay(display->GetDisplayInfo().GetDisplayID())) {
      // This is a new display
      displayInfoChanged = true;
      break;
    }

    if (display->CheckClearDisplayInfoDirty()) {
      // This display's info has changed
      displayInfoChanged = true;
      break;
    }
  }

  if (displayInfoChanged) {
    mVRDisplays.Clear();
    for (const auto& display: displays) {
      mVRDisplays.Put(display->GetDisplayInfo().GetDisplayID(), display);
    }
  }

  if (displayInfoChanged || aMustDispatch) {
    DispatchVRDisplayInfoUpdate();
  }
}

void
VRManager::DispatchVRDisplayInfoUpdate()
{
  nsTArray<VRDisplayInfo> update;
  GetVRDisplayInfo(update);

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendUpdateDisplayInfo(update);
  }
}


/**
 * Get any VR displays that have already been enumerated without
 * activating any new devices.
 */
void
VRManager::GetVRDisplayInfo(nsTArray<VRDisplayInfo>& aDisplayInfo)
{
  aDisplayInfo.Clear();
  for (auto iter = mVRDisplays.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRDisplayHost* display = iter.UserData();
    aDisplayInfo.AppendElement(VRDisplayInfo(display->GetDisplayInfo()));
  }
}

RefPtr<gfx::VRDisplayHost>
VRManager::GetDisplay(const uint32_t& aDisplayID)
{
  RefPtr<gfx::VRDisplayHost> display;
  if (mVRDisplays.Get(aDisplayID, getter_AddRefs(display))) {
    return display;
  }
  return nullptr;
}

void
VRManager::SubmitFrame(VRLayerParent* aLayer, layers::PTextureParent* aTexture,
                       const gfx::Rect& aLeftEyeRect,
                       const gfx::Rect& aRightEyeRect)
{
  TextureHost* th = TextureHost::AsTextureHost(aTexture);
  mLastFrame = th;
  RefPtr<VRDisplayHost> display = GetDisplay(aLayer->GetDisplayID());
  if (display) {
    display->SubmitFrame(aLayer, 0, aTexture, aLeftEyeRect, aRightEyeRect);
  }
}

RefPtr<gfx::VRControllerHost>
VRManager::GetController(const uint32_t& aControllerID)
{
  RefPtr<gfx::VRControllerHost> controller;
  if (mVRControllers.Get(aControllerID, getter_AddRefs(controller))) {
    return controller;
  }
  return nullptr;
}

void
VRManager::GetVRControllerInfo(nsTArray<VRControllerInfo>& aControllerInfo)
{
  aControllerInfo.Clear();
  for (auto iter = mVRControllers.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRControllerHost* controller = iter.UserData();
    aControllerInfo.AppendElement(VRControllerInfo(controller->GetControllerInfo()));
  }
}

void
VRManager::RefreshVRControllers()
{
  nsTArray<RefPtr<gfx::VRControllerHost>> controllers;

  ScanForControllers();

  for (uint32_t i = 0; i < mControllerManagers.Length()
      && controllers.Length() == 0; ++i) {
    mControllerManagers[i]->GetControllers(controllers);
  }

  bool controllerInfoChanged = false;

  if (controllers.Length() != mVRControllers.Count()) {
    // Catch cases where VR controllers has been removed
    controllerInfoChanged = true;
  }

  for (const auto& controller : controllers) {
    if (!GetController(controller->GetControllerInfo().GetControllerID())) {
      // This is a new controller
      controllerInfoChanged = true;
      break;
    }
  }

  if (controllerInfoChanged) {
    mVRControllers.Clear();
    for (const auto& controller : controllers) {
      mVRControllers.Put(controller->GetControllerInfo().GetControllerID(),
                         controller);
    }
  }
}

void
VRManager::ScanForControllers()
{
  for (uint32_t i = 0; i < mControllerManagers.Length(); ++i) {
    mControllerManagers[i]->ScanForDevices();
  }
}

void
VRManager::RemoveControllers()
{
  for (uint32_t i = 0; i < mControllerManagers.Length(); ++i) {
    mControllerManagers[i]->RemoveDevices();
  }
  mVRControllers.Clear();
}

template<class T>
void
VRManager::NotifyGamepadChange(const T& aInfo)
{
  dom::GamepadChangeEvent e(aInfo);

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendGamepadUpdate(e);
  }
}

} // namespace gfx
} // namespace mozilla
