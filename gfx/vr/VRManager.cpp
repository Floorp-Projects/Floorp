/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "VRManager.h"
#include "VRManagerParent.h"
#include "gfxVR.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/unused.h"

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

#if defined(XP_WIN)
  // The Oculus runtime is supported only on Windows
  mgr = VRDisplayManagerOculus::Create();
  if (mgr) {
    mManagers.AppendElement(mgr);
  }
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
  // OSVR is cross platform compatible
  mgr = VRDisplayManagerOSVR::Create();
  if (mgr){
      mManagers.AppendElement(mgr);
  }
#endif
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
  mInitialized = false;
}

void
VRManager::Init()
{
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Init();
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
    Unused << vmp->SendNotifyVSync();
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
    } else {
      // We don't have to do this every frame, so check if we
      // have refreshed recently.
      TimeDuration duration = TimeStamp::Now() - mLastRefreshTime;
      if (duration.ToMilliseconds() > kVRDisplayRefreshMaxDuration) {
        RefreshVRDisplays();
      }
    }
  }
}

void
VRManager::NotifyVRVsync(const uint32_t& aDisplayID)
{
  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendNotifyVRVSync(aDisplayID);
  }
}

void
VRManager::RefreshVRDisplays(bool aMustDispatch)
{
  nsTArray<RefPtr<gfx::VRDisplayHost> > displays;

  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
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

  mLastRefreshTime = TimeStamp::Now();
}

void
VRManager::DispatchVRDisplayInfoUpdate()
{
  nsTArray<VRDisplayInfo> update;
  for (auto iter = mVRDisplays.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRDisplayHost* display = iter.UserData();
    update.AppendElement(VRDisplayInfo(display->GetDisplayInfo()));
  }

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendUpdateDisplayInfo(update);
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
VRManager::SubmitFrame(VRLayerParent* aLayer, const int32_t& aInputFrameID,
                     layers::PTextureParent* aTexture, const gfx::Rect& aLeftEyeRect,
                     const gfx::Rect& aRightEyeRect)
{
  TextureHost* th = TextureHost::AsTextureHost(aTexture);
  mLastFrame = th;
  RefPtr<VRDisplayHost> display = GetDisplay(aLayer->GetDisplayID());
  if (display) {
    display->SubmitFrame(aLayer, aInputFrameID, aTexture, aLeftEyeRect, aRightEyeRect);
  }
}

} // namespace gfx
} // namespace mozilla
