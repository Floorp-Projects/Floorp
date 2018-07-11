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

int VRDisplayExternal::sPushIndex = 0;

VRDisplayExternal::VRDisplayExternal(const VRDisplayState& aDisplayState)
  : VRDisplayHost(VRDeviceType::External)
  , mIsPresenting(false)
  , mLastSensorState{}
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayExternal, VRDisplayHost);
  mDisplayInfo.mDisplayState = aDisplayState;

  // default to an identity quaternion
  mLastSensorState.pose.orientation[3] = 1.0f;
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

  manager->PullState(&mDisplayInfo.mDisplayState, &mLastSensorState, mDisplayInfo.mControllerState);
}

VRHMDSensorState
VRDisplayExternal::GetSensorState()
{
  return mLastSensorState;
}

void
VRDisplayExternal::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }
  sPushIndex = 0;
  mIsPresenting = true;
  mTelemetry.Clear();
  mTelemetry.mPresentationStart = TimeStamp::Now();

  // Indicate that we are ready to start immersive mode
  VRBrowserState state;
  memset(&state, 0, sizeof(VRBrowserState));
  state.layerState[0].type = VRLayerType::LayerType_Stereo_Immersive;
  VRManager *vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();
  manager->PushState(&state);

  // TODO - Implement telemetry:

  // mTelemetry.mLastDroppedFrameCount = stats.m_nNumReprojectedFrames;
}

void
VRDisplayExternal::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }
  mIsPresenting = false;
  sPushIndex = 0;

  // Indicate that we have stopped immersive mode
  VRBrowserState state;
  memset(&state, 0, sizeof(VRBrowserState));
  VRManager *vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();
  manager->PushState(&state, true);

  // TODO - Implement telemetry:

/*
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

bool
VRDisplayExternal::PopulateLayerTexture(const layers::SurfaceDescriptor& aTexture,
                                        VRLayerTextureType* aTextureType,
                                        VRLayerTextureHandle* aTextureHandle)
{
  switch (aTexture.type()) {
#if defined(XP_WIN)
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      const SurfaceDescriptorD3D10& surf = aTexture.get_SurfaceDescriptorD3D10();
      *aTextureType = VRLayerTextureType::LayerTextureType_D3D10SurfaceDescriptor;
      *aTextureHandle = (void *)surf.handle();
      return true;
    }
#elif defined(XP_MACOSX)
    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      const auto& desc = aTexture.get_SurfaceDescriptorMacIOSurface();
      RefPtr<MacIOSurface> surf = MacIOSurface::LookupSurface(desc.surfaceId(),
                                                              desc.scaleFactor(),
                                                              !desc.isOpaque());
      if (!surf) {
        NS_WARNING("VRDisplayHost::SubmitFrame failed to get a MacIOSurface");
        return false;
      }
      *aTextureType = VRLayerTextureType::LayerTextureType_MacIOSurface;
      *aTextureHandle = (void *)surf->GetIOSurfacePtr();
      return true;
    }
#elif defined(MOZ_WIDGET_ANDROID)
    case SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const SurfaceTextureDescriptor& desc = aTexture.get_SurfaceTextureDescriptor();
      java::GeckoSurfaceTexture::LocalRef surfaceTexture = java::GeckoSurfaceTexture::Lookup(desc.handle());
      if (!surfaceTexture) {
        NS_WARNING("VRDisplayHost::SubmitFrame failed to get a SurfaceTexture");
        return false;
      }
      *aTextureType = VRLayerTextureType::LayerTextureType_GeckoSurfaceTexture;
      *aTextureHandle = desc.handle();
      return true;
    }
#endif
    default: {
      MOZ_ASSERT(false);
      return false;
    }
  }
}

bool
VRDisplayExternal::SubmitFrame(const layers::SurfaceDescriptor& aTexture,
                               uint64_t aFrameId,
                               const gfx::Rect& aLeftEyeRect,
                               const gfx::Rect& aRightEyeRect)
{
  VRBrowserState state;
  memset(&state, 0, sizeof(VRBrowserState));
  state.layerState[0].type = VRLayerType::LayerType_Stereo_Immersive;
  VRLayer_Stereo_Immersive& layer = state.layerState[0].layer_stereo_immersive;
  if (!PopulateLayerTexture(aTexture, &layer.mTextureType, &layer.mTextureHandle)) {
    return false;
  }
  layer.mFrameId = aFrameId;
  layer.mInputFrameId = mDisplayInfo.mLastSensorState[mDisplayInfo.mFrameId % kVRMaxLatencyFrames].inputFrameID;

  layer.mLeftEyeRect.x = aLeftEyeRect.x;
  layer.mLeftEyeRect.y = aLeftEyeRect.y;
  layer.mLeftEyeRect.width = aLeftEyeRect.width;
  layer.mLeftEyeRect.height = aLeftEyeRect.height;
  layer.mRightEyeRect.x = aRightEyeRect.x;
  layer.mRightEyeRect.y = aRightEyeRect.y;
  layer.mRightEyeRect.width = aRightEyeRect.width;
  layer.mRightEyeRect.height = aRightEyeRect.height;

  VRManager *vm = VRManager::Get();
  VRSystemManagerExternal* manager = vm->GetExternalManager();
  manager->PushState(&state, true);
  sPushIndex++;

  VRDisplayState displayState;
  memset(&displayState, 0, sizeof(VRDisplayState));
  if (manager->PullState(&displayState, &mLastSensorState, mDisplayInfo.mControllerState)) {
    if (manager->PullState(&displayState, &mLastSensorState)) {
      if (!displayState.mIsConnected) {
        // Service has shut down or hardware has been disconnected
        return false;
      }
    }
#ifdef XP_WIN
    Sleep(0);
#else
    sleep(0);
#endif
  }

  return displayState.mLastSubmittedFrameSuccessful;
}

VRSystemManagerExternal::VRSystemManagerExternal(VRExternalShmem* aAPIShmem /* = nullptr*/)
 : mExternalShmem(aAPIShmem)
#if !defined(MOZ_WIDGET_ANDROID)
 , mSameProcess(aAPIShmem != nullptr)
#endif
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
  int32_t version = -1;
  int32_t size = 0;
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) == 0) {
    version = mExternalShmem->version;
    size = mExternalShmem->size;
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
  } else {
    return;
  }
  if (version != kVRExternalVersion) {
    mExternalShmem = nullptr;
    mExternalStructFailed = true;
    return;
  }
  if (size != sizeof(VRExternalShmem)) {
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
#if !defined(MOZ_WIDGET_ANDROID)
  if (mSameProcess) {
    return;
  }
#endif
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
VRSystemManagerExternal::Create(VRExternalShmem* aAPIShmem /* = nullptr*/)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled()) {
    return nullptr;
  }

  if (!gfxPrefs::VRExternalEnabled() && aAPIShmem == nullptr) {
    return nullptr;
  }

  RefPtr<VRSystemManagerExternal> manager = new VRSystemManagerExternal(aAPIShmem);
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
      memset(&displayState, 0, sizeof(VRDisplayState));
      // We must block until enumeration has completed in order
      // to signal that the WebVR promise should be resolved at the
      // right time.
      while (!PullState(&displayState)) {
#ifdef XP_WIN
        Sleep(0);
#else
        sleep(0);
#endif
      }
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
  // Controller updates are handled in VRDisplayClient for VRSystemManagerExternal
  aControllerResult.Clear();
}

void
VRSystemManagerExternal::ScanForControllers()
{
  // Controller updates are handled in VRDisplayClient for VRSystemManagerExternal
  if (mDisplay) {
    mDisplay->Refresh();
  }
  return;
}

void
VRSystemManagerExternal::HandleInput()
{
  // Controller updates are handled in VRDisplayClient for VRSystemManagerExternal
  if (mDisplay) {
    mDisplay->Refresh();
  }
  return;
}

void
VRSystemManagerExternal::RemoveControllers()
{
  // Controller updates are handled in VRDisplayClient for VRSystemManagerExternal
}

bool
VRSystemManagerExternal::PullState(VRDisplayState* aDisplayState,
                                   VRHMDSensorState* aSensorState /* = nullptr */,
                                   VRControllerState* aControllerState /* = nullptr */) {
  bool success = false;
  MOZ_ASSERT(mExternalShmem);
  if (mExternalShmem) {
#if defined(MOZ_WIDGET_ANDROID)
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) == 0) {
      memcpy(aDisplayState, (void*)&(mExternalShmem->state.displayState), sizeof(VRDisplayState));
      if (aSensorState) {
        memcpy(aSensorState, (void*)&(mExternalShmem->state.sensorState), sizeof(VRHMDSensorState));
      }
      if (aControllerState) {
        memcpy(aControllerState, (void*)&(mExternalShmem->state.controllerState), sizeof(VRControllerState) * kVRControllerMaxCount);
      }
      success = mExternalShmem->state.enumerationCompleted;
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
      mDoShutdown = aDisplayState->shutdown;
    }
#else
    VRExternalShmem tmp;
    memcpy(&tmp, (void *)mExternalShmem, sizeof(VRExternalShmem));
    if (tmp.generationA == tmp.generationB && tmp.generationA != 0 && tmp.generationA != -1 && tmp.state.enumerationCompleted) {
      memcpy(aDisplayState, &tmp.state.displayState, sizeof(VRDisplayState));
      if (aSensorState) {
        memcpy(aSensorState, &tmp.state.sensorState, sizeof(VRHMDSensorState));
      }
      if (aControllerState) {
        memcpy(aControllerState, (void*)&(mExternalShmem->state.controllerState), sizeof(VRControllerState) * kVRControllerMaxCount);
      }
      success = true;
    }
#endif // defined(MOZ_WIDGET_ANDROID)
  }

  return success;
}

void
VRSystemManagerExternal::PushState(VRBrowserState* aBrowserState, bool aNotifyCond)
{
  MOZ_ASSERT(aBrowserState);
  MOZ_ASSERT(mExternalShmem);
  if (mExternalShmem) {
#if defined(MOZ_WIDGET_ANDROID)
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->browserMutex)) == 0) {
      memcpy((void *)&(mExternalShmem->browserState), aBrowserState, sizeof(VRBrowserState));
      if (aNotifyCond) {
        pthread_cond_signal((pthread_cond_t*)&(mExternalShmem->browserCond));
      }
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->browserMutex));
    }
#else
    mExternalShmem->browserGenerationA++;
    memcpy((void *)&(mExternalShmem->browserState), (void *)aBrowserState, sizeof(VRBrowserState));
    mExternalShmem->browserGenerationB++;
#endif // defined(MOZ_WIDGET_ANDROID)
  }
}
