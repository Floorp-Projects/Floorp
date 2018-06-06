/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "mozilla/Preferences.h"

#include "mozilla/gfx/Quaternion.h"

#ifdef XP_WIN
#include "CompositorD3D11.h"
#include "TextureD3D11.h"
static const char* kShmemName = "moz.gecko.vr_ext.0.0.1";
#elif defined(XP_MACOSX)
#include "mozilla/gfx/MacIOSurface.h"
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
static const char* kShmemName = "/moz.gecko.vr_ext.0.0.1";
#elif defined(MOZ_WIDGET_ANDROID)
#include <string.h>
#include <pthread.h>
#include "GeckoVRManager.h"
#endif // defined(MOZ_WIDGET_ANDROID)

#include "gfxVRExternal.h"
#include "VRManagerParent.h"
#include "VRManager.h"
#include "VRThread.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/Telemetry.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;
using namespace mozilla::dom;

static const uint32_t kNumExternalHaptcs = 1;

VRDisplayExternal::VRDisplayExternal(const VRDisplayState& aDisplayState)
  : VRDisplayHost(VRDeviceType::External)
  , mIsPresenting(false)
  , mLastSensorState{}
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayExternal, VRDisplayHost);
  mDisplayInfo.mDisplayState = aDisplayState;

  // default to an identity quaternion
  mLastSensorState.orientation[3] = 1.0f;
}

VRDisplayExternal::~VRDisplayExternal()
{
  Destroy();
  MOZ_COUNT_DTOR_INHERITED(VRDisplayExternal, VRDisplayHost);
}

void
VRDisplayExternal::Destroy()
{
  StopPresentation();

  // TODO - Implement
}

void
VRDisplayExternal::ZeroSensor()
{
}

void
VRDisplayExternal::Refresh()
{
  VRManager *vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();

  manager->PullState(&mDisplayInfo.mDisplayState);
}

VRHMDSensorState
VRDisplayExternal::GetSensorState()
{
  VRManager *vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();

  manager->PullState(&mDisplayInfo.mDisplayState, &mLastSensorState);

//  result.CalcViewMatrices(headToEyeTransforms);
  mLastSensorState.inputFrameID = mDisplayInfo.mFrameId;
  return mLastSensorState;
}

void
VRDisplayExternal::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }
  mIsPresenting = true;
  mTelemetry.Clear();
  mTelemetry.mPresentationStart = TimeStamp::Now();

  // TODO - Implement this

  // mTelemetry.mLastDroppedFrameCount = stats.m_nNumReprojectedFrames;
}

void
VRDisplayExternal::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }

  // TODO - Implement this

/*
  mIsPresenting = false;
  const TimeDuration duration = TimeStamp::Now() - mTelemetry.mPresentationStart;
  Telemetry::Accumulate(Telemetry::WEBVR_USERS_VIEW_IN, 2);
  Telemetry::Accumulate(Telemetry::WEBVR_TIME_SPENT_VIEWING_IN_OPENVR,
                        duration.ToMilliseconds());

  ::vr::Compositor_CumulativeStats stats;
  mVRCompositor->GetCumulativeStats(&stats, sizeof(::vr::Compositor_CumulativeStats));
  const uint32_t droppedFramesPerSec = (stats.m_nNumReprojectedFrames -
                                        mTelemetry.mLastDroppedFrameCount) / duration.ToSeconds();
  Telemetry::Accumulate(Telemetry::WEBVR_DROPPED_FRAMES_IN_OPENVR, droppedFramesPerSec);
*/
}

#if defined(XP_WIN)

bool
VRDisplayExternal::SubmitFrame(ID3D11Texture2D* aSource,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  // FINDME!  Implement this
  return false;
}

#elif defined(XP_MACOSX)

bool
VRDisplayExternal::SubmitFrame(MacIOSurface* aMacIOSurface,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  const void* ioSurface = aMacIOSurface->GetIOSurfacePtr();
  bool result = false;
  if (ioSurface == nullptr) {
    NS_WARNING("VRDisplayExternal::SubmitFrame() could not get an IOSurface");
  } else {
    // FINDME!  Implement this
  }
  return result;
}

#elif defined(MOZ_WIDGET_ANDROID)

bool
VRDisplayExternal::SubmitFrame(const layers::SurfaceTextureDescriptor& aSurface,
                               const gfx::Rect& aLeftEyeRect,
                               const gfx::Rect& aRightEyeRect) {

  return false;
}

#endif

VRControllerExternal::VRControllerExternal(dom::GamepadHand aHand, uint32_t aDisplayID,
                                       uint32_t aNumButtons, uint32_t aNumTriggers,
                                       uint32_t aNumAxes, const nsCString& aId)
  : VRControllerHost(VRDeviceType::External, aHand, aDisplayID)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerExternal, VRControllerHost);

  VRControllerState& state = mControllerInfo.mControllerState;
  strncpy(state.mControllerName, aId.BeginReading(), kVRControllerNameMaxLen);
  state.mNumButtons = aNumButtons;
  state.mNumAxes = aNumAxes;
  state.mNumTriggers = aNumTriggers;
  state.mNumHaptics = kNumExternalHaptcs;
}

VRControllerExternal::~VRControllerExternal()
{
  MOZ_COUNT_DTOR_INHERITED(VRControllerExternal, VRControllerHost);
}

VRSystemManagerExternal::VRSystemManagerExternal()
 : mExternalShmem(nullptr)
{
#if defined(XP_MACOSX)
  mShmemFD = 0;
#elif defined(XP_WIN)
  mShmemFile = NULL;
#elif defined(MOZ_WIDGET_ANDROID)
  mDoShutdown = false;
  mExternalStructFailed = false;
#endif
}

VRSystemManagerExternal::~VRSystemManagerExternal()
{
  CloseShmem();
}

void
VRSystemManagerExternal::OpenShmem()
{
  if (mExternalShmem) {
    return;
#if defined(MOZ_WIDGET_ANDROID)
  } else if (mExternalStructFailed) {
    return;
#endif // defined(MOZ_WIDGET_ANDROID)
  }

#if defined(XP_MACOSX)
  if (mShmemFD == 0) {
    mShmemFD = shm_open(kShmemName, O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
  }
  if (mShmemFD <= 0) {
    mShmemFD = 0;
    return;
  }

  struct stat sb;
  fstat(mShmemFD, &sb);
  off_t length = sb.st_size;
  if (length < (off_t)sizeof(VRExternalShmem)) {
    // TODO - Implement logging
    CloseShmem();
    return;
  }

  mExternalShmem = (VRExternalShmem*)mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, mShmemFD, 0);
  if (mExternalShmem == MAP_FAILED) {
    // TODO - Implement logging
    mExternalShmem = NULL;
    CloseShmem();
    return;
  }

#elif defined(XP_WIN)
  if (mShmemFile == NULL) {
    mShmemFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kShmemName);
    if (mShmemFile == NULL) {
      // TODO - Implement logging
      CloseShmem();
      return;
    }
  }
  LARGE_INTEGER length;
  length.QuadPart = sizeof(VRExternalShmem);
  mExternalShmem = (VRExternalShmem*)MapViewOfFile(mShmemFile, // handle to map object
    FILE_MAP_ALL_ACCESS,  // read/write permission
    0,
    0,
    length.QuadPart);

  if (mExternalShmem == NULL) {
    // TODO - Implement logging
    CloseShmem();
    return;
  }
#elif defined(MOZ_WIDGET_ANDROID)
  mExternalShmem = (VRExternalShmem*)mozilla::GeckoVRManager::GetExternalContext();
  if (!mExternalShmem) {
    return;
  }
  if (mExternalShmem->version != kVRExternalVersion) {
    mExternalShmem = nullptr;
    mExternalStructFailed = true;
    return;
  }
  if (mExternalShmem->size != sizeof(VRExternalShmem)) {
    mExternalShmem = nullptr;
    mExternalStructFailed = true;
    return;
  }
#endif
  CheckForShutdown();
}

void
VRSystemManagerExternal::CheckForShutdown()
{
#if defined(MOZ_WIDGET_ANDROID)
  if (mDoShutdown) {
    Shutdown();
  }
#else
  if (mExternalShmem) {
    if (mExternalShmem->generationA == -1 && mExternalShmem->generationB == -1) {
      Shutdown();
    }
  }
#endif // defined(MOZ_WIDGET_ANDROID)
}

void
VRSystemManagerExternal::CloseShmem()
{
#if defined(XP_MACOSX)
  if (mExternalShmem) {
    munmap((void *)mExternalShmem, sizeof(VRExternalShmem));
    mExternalShmem = NULL;
  }
  if (mShmemFD) {
    close(mShmemFD);
  }
  mShmemFD = 0;
#elif defined(XP_WIN)
  if (mExternalShmem) {
    UnmapViewOfFile((void *)mExternalShmem);
    mExternalShmem = NULL;
  }
  if (mShmemFile) {
    CloseHandle(mShmemFile);
    mShmemFile = NULL;
  }
#elif defined(MOZ_WIDGET_ANDROID)
  mExternalShmem = NULL;
#endif
}

/*static*/ already_AddRefed<VRSystemManagerExternal>
VRSystemManagerExternal::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VRExternalEnabled()) {
    return nullptr;
  }

  RefPtr<VRSystemManagerExternal> manager = new VRSystemManagerExternal();
  return manager.forget();
}

void
VRSystemManagerExternal::Destroy()
{
  Shutdown();
}

void
VRSystemManagerExternal::Shutdown()
{
  if (mDisplay) {
    mDisplay = nullptr;
  }
  RemoveControllers();
  CloseShmem();
#if defined(MOZ_WIDGET_ANDROID)
  mDoShutdown = false;
#endif
}

void
VRSystemManagerExternal::NotifyVSync()
{
  VRSystemManager::NotifyVSync();

  CheckForShutdown();

  if (mDisplay) {
    mDisplay->Refresh();
  }
}

void
VRSystemManagerExternal::Enumerate()
{
  if (mDisplay == nullptr) {
    OpenShmem();
    if (mExternalShmem) {
      VRDisplayState displayState;
      PullState(&displayState);
      if (displayState.mIsConnected) {
        mDisplay = new VRDisplayExternal(displayState);
      }
    }
  }
}

bool
VRSystemManagerExternal::ShouldInhibitEnumeration()
{
  if (VRSystemManager::ShouldInhibitEnumeration()) {
    return true;
  }
  if (mDisplay) {
    // When we find an a VR device, don't
    // allow any further enumeration as it
    // may get picked up redundantly by other
    // API's.
    return true;
  }
  return false;
}

void
VRSystemManagerExternal::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (mDisplay) {
    aHMDResult.AppendElement(mDisplay);
  }
}

bool
VRSystemManagerExternal::GetIsPresenting()
{
  if (mDisplay) {
    VRDisplayInfo displayInfo(mDisplay->GetDisplayInfo());
    return displayInfo.GetPresentingGroups() != 0;
  }

  return false;
}

void
VRSystemManagerExternal::HandleInput()
{
  // TODO - Implement This!
}

void
VRSystemManagerExternal::VibrateHaptic(uint32_t aControllerIdx,
                                      uint32_t aHapticIndex,
                                      double aIntensity,
                                      double aDuration,
                                      const VRManagerPromise& aPromise)
{
  // TODO - Implement this
}

void
VRSystemManagerExternal::StopVibrateHaptic(uint32_t aControllerIdx)
{
  // TODO - Implement this
}

void
VRSystemManagerExternal::GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult)
{
  aControllerResult.Clear();
  for (uint32_t i = 0; i < mExternalController.Length(); ++i) {
    aControllerResult.AppendElement(mExternalController[i]);
  }
}

void
VRSystemManagerExternal::ScanForControllers()
{
  // TODO - Implement this
}

void
VRSystemManagerExternal::RemoveControllers()
{
  // The controller count is changed, removing the existing gamepads first.
  for (uint32_t i = 0; i < mExternalController.Length(); ++i) {
    RemoveGamepad(i);
  }
  mExternalController.Clear();
  mControllerCount = 0;
}

void
VRSystemManagerExternal::PullState(VRDisplayState* aDisplayState, VRHMDSensorState* aSensorState /* = nullptr */)
{
  MOZ_ASSERT(mExternalShmem);
  if (mExternalShmem) {
#if defined(MOZ_WIDGET_ANDROID)
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) == 0) {
        memcpy(aDisplayState, (void*)&(mExternalShmem->state.displayState), sizeof(VRDisplayState));
        if (aSensorState) {
          memcpy(aSensorState, (void*)&(mExternalShmem->state.sensorState), sizeof(VRHMDSensorState));
        }
        pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
        mDoShutdown = aDisplayState->shutdown;
    }
#else
    VRExternalShmem tmp;
    memcpy(&tmp, (void *)mExternalShmem, sizeof(VRExternalShmem));
    if (tmp.generationA == tmp.generationB && tmp.generationA != 0 && tmp.generationA != -1) {
      memcpy(aDisplayState, &tmp.state.displayState, sizeof(VRDisplayState));
      if (aSensorState) {
        memcpy(aSensorState, &tmp.state.sensorState, sizeof(VRHMDSensorState));
      }
    }
#endif // defined(MOZ_WIDGET_ANDROID)
  }
}
