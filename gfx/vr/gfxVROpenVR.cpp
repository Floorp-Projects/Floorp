/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/Preferences.h"

#include "mozilla/gfx/Quaternion.h"

#ifdef XP_WIN
#include "CompositorD3D11.h"
#include "TextureD3D11.h"
#endif // XP_WIN

#include "gfxVROpenVR.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"
#include "openvr/openvr.h"

#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;
using namespace mozilla::dom;

namespace {
extern "C" {
typedef uint32_t (VR_CALLTYPE * pfn_VR_InitInternal)(::vr::HmdError *peError, ::vr::EVRApplicationType eApplicationType);
typedef void (VR_CALLTYPE * pfn_VR_ShutdownInternal)();
typedef bool (VR_CALLTYPE * pfn_VR_IsHmdPresent)();
typedef bool (VR_CALLTYPE * pfn_VR_IsRuntimeInstalled)();
typedef const char * (VR_CALLTYPE * pfn_VR_GetStringForHmdError)(::vr::HmdError error);
typedef void * (VR_CALLTYPE * pfn_VR_GetGenericInterface)(const char *pchInterfaceVersion, ::vr::HmdError *peError);
} // extern "C"
} // namespace

static pfn_VR_InitInternal vr_InitInternal = nullptr;
static pfn_VR_ShutdownInternal vr_ShutdownInternal = nullptr;
static pfn_VR_IsHmdPresent vr_IsHmdPresent = nullptr;
static pfn_VR_IsRuntimeInstalled vr_IsRuntimeInstalled = nullptr;
static pfn_VR_GetStringForHmdError vr_GetStringForHmdError = nullptr;
static pfn_VR_GetGenericInterface vr_GetGenericInterface = nullptr;

// EButton_System, EButton_DPad_xx, and EButton_A
// can not be triggered in Steam Vive in OpenVR SDK 1.0.3.
const uint64_t gOpenVRButtonMask[] = {
  // vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_System),
  vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_ApplicationMenu),
  vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_Grip),
  // vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_DPad_Left),
  // vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_DPad_Up),
  // vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_DPad_Right),
  // vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_DPad_Down),
  // vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_A),
  vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_SteamVR_Touchpad),
  vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_SteamVR_Trigger)
};

const uint32_t gNumOpenVRButtonMask = sizeof(gOpenVRButtonMask) /
                                      sizeof(uint64_t);

enum class VRControllerAxisType : uint16_t {
  TrackpadXAxis,
  TrackpadYAxis,
  Trigger,
  NumVRControllerAxisType
};

#define VRControllerAxis(aButtonId) (aButtonId - vr::EVRButtonId::k_EButton_Axis0)

const uint32_t gOpenVRAxes[] = {
  VRControllerAxis(vr::EVRButtonId::k_EButton_Axis0),
  VRControllerAxis(vr::EVRButtonId::k_EButton_Axis0),
  VRControllerAxis(vr::EVRButtonId::k_EButton_Axis1)
};

const uint32_t gNumOpenVRAxis = sizeof(gOpenVRAxes) /
                                sizeof(uint32_t);


bool
LoadOpenVRRuntime()
{
  static PRLibrary *openvrLib = nullptr;

  nsAdoptingCString openvrPath = Preferences::GetCString("gfx.vr.openvr-runtime");
  if (!openvrPath)
    return false;

  openvrLib = PR_LoadLibrary(openvrPath.BeginReading());
  if (!openvrLib)
    return false;

#define REQUIRE_FUNCTION(_x) do {                                       \
    *(void **)&vr_##_x = (void *) PR_FindSymbol(openvrLib, "VR_" #_x);  \
    if (!vr_##_x) { printf_stderr("VR_" #_x " symbol missing\n"); return false; } \
  } while (0)

  REQUIRE_FUNCTION(InitInternal);
  REQUIRE_FUNCTION(ShutdownInternal);
  REQUIRE_FUNCTION(IsHmdPresent);
  REQUIRE_FUNCTION(IsRuntimeInstalled);
  REQUIRE_FUNCTION(GetStringForHmdError);
  REQUIRE_FUNCTION(GetGenericInterface);

#undef REQUIRE_FUNCTION

  return true;
}

VRDisplayOpenVR::VRDisplayOpenVR(::vr::IVRSystem *aVRSystem,
                                 ::vr::IVRChaperone *aVRChaperone,
                                 ::vr::IVRCompositor *aVRCompositor)
  : VRDisplayHost(VRDeviceType::OpenVR)
  , mVRSystem(aVRSystem)
  , mVRChaperone(aVRChaperone)
  , mVRCompositor(aVRCompositor)
  , mIsPresenting(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayOpenVR, VRDisplayHost);

  mDisplayInfo.mDisplayName.AssignLiteral("OpenVR HMD");
  mDisplayInfo.mIsConnected = true;
  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                                  VRDisplayCapabilityFlags::Cap_Orientation |
                                  VRDisplayCapabilityFlags::Cap_Position |
                                  VRDisplayCapabilityFlags::Cap_External |
                                  VRDisplayCapabilityFlags::Cap_Present |
                                  VRDisplayCapabilityFlags::Cap_StageParameters;

  mVRCompositor->SetTrackingSpace(::vr::TrackingUniverseSeated);

  uint32_t w, h;
  mVRSystem->GetRecommendedRenderTargetSize(&w, &h);
  mDisplayInfo.mEyeResolution.width = w;
  mDisplayInfo.mEyeResolution.height = h;

  // SteamVR gives the application a single FOV to use; it's not configurable as with Oculus
  for (uint32_t eye = 0; eye < 2; ++eye) {
    // get l/r/t/b clip plane coordinates
    float l, r, t, b;
    mVRSystem->GetProjectionRaw(static_cast<::vr::Hmd_Eye>(eye), &l, &r, &t, &b);
    mDisplayInfo.mEyeFOV[eye].SetFromTanRadians(-t, r, b, -l);

    ::vr::HmdMatrix34_t eyeToHead = mVRSystem->GetEyeToHeadTransform(static_cast<::vr::Hmd_Eye>(eye));

    mDisplayInfo.mEyeTranslation[eye].x = eyeToHead.m[0][3];
    mDisplayInfo.mEyeTranslation[eye].y = eyeToHead.m[1][3];
    mDisplayInfo.mEyeTranslation[eye].z = eyeToHead.m[2][3];
  }

  UpdateStageParameters();
}

VRDisplayOpenVR::~VRDisplayOpenVR()
{
  Destroy();
  MOZ_COUNT_DTOR_INHERITED(VRDisplayOpenVR, VRDisplayHost);
}

void
VRDisplayOpenVR::Destroy()
{
  StopPresentation();
  vr_ShutdownInternal();
}

void
VRDisplayOpenVR::UpdateStageParameters()
{
  float sizeX = 0.0f;
  float sizeZ = 0.0f;
  if (mVRChaperone->GetPlayAreaSize(&sizeX, &sizeZ)) {
    ::vr::HmdMatrix34_t t = mVRSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
    mDisplayInfo.mStageSize.width = sizeX;
    mDisplayInfo.mStageSize.height = sizeZ;

    mDisplayInfo.mSittingToStandingTransform._11 = t.m[0][0];
    mDisplayInfo.mSittingToStandingTransform._12 = t.m[1][0];
    mDisplayInfo.mSittingToStandingTransform._13 = t.m[2][0];
    mDisplayInfo.mSittingToStandingTransform._14 = 0.0f;

    mDisplayInfo.mSittingToStandingTransform._21 = t.m[0][1];
    mDisplayInfo.mSittingToStandingTransform._22 = t.m[1][1];
    mDisplayInfo.mSittingToStandingTransform._23 = t.m[2][1];
    mDisplayInfo.mSittingToStandingTransform._24 = 0.0f;

    mDisplayInfo.mSittingToStandingTransform._31 = t.m[0][2];
    mDisplayInfo.mSittingToStandingTransform._32 = t.m[1][2];
    mDisplayInfo.mSittingToStandingTransform._33 = t.m[2][2];
    mDisplayInfo.mSittingToStandingTransform._34 = 0.0f;

    mDisplayInfo.mSittingToStandingTransform._41 = t.m[0][3];
    mDisplayInfo.mSittingToStandingTransform._42 = t.m[1][3];
    mDisplayInfo.mSittingToStandingTransform._43 = t.m[2][3];
    mDisplayInfo.mSittingToStandingTransform._44 = 1.0f;
  } else {
    // If we fail, fall back to reasonable defaults.
    // 1m x 1m space, 0.75m high in seated position

    mDisplayInfo.mStageSize.width = 1.0f;
    mDisplayInfo.mStageSize.height = 1.0f;

    mDisplayInfo.mSittingToStandingTransform._11 = 1.0f;
    mDisplayInfo.mSittingToStandingTransform._12 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._13 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._14 = 0.0f;

    mDisplayInfo.mSittingToStandingTransform._21 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._22 = 1.0f;
    mDisplayInfo.mSittingToStandingTransform._23 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._24 = 0.0f;

    mDisplayInfo.mSittingToStandingTransform._31 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._32 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._33 = 1.0f;
    mDisplayInfo.mSittingToStandingTransform._34 = 0.0f;

    mDisplayInfo.mSittingToStandingTransform._41 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._42 = 0.75f;
    mDisplayInfo.mSittingToStandingTransform._43 = 0.0f;
    mDisplayInfo.mSittingToStandingTransform._44 = 1.0f;
  }
}

void
VRDisplayOpenVR::ZeroSensor()
{
  mVRSystem->ResetSeatedZeroPose();
  UpdateStageParameters();
}

VRHMDSensorState
VRDisplayOpenVR::GetSensorState()
{
  return GetSensorState(0.0f);
}

VRHMDSensorState
VRDisplayOpenVR::GetImmediateSensorState()
{
  return GetSensorState(0.0f);
}

VRHMDSensorState
VRDisplayOpenVR::GetSensorState(double timeOffset)
{
  {
    ::vr::VREvent_t event;
    while (mVRSystem->PollNextEvent(&event, sizeof(event))) {
      // ignore
    }
  }

  ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  // Note: We *must* call WaitGetPoses in order for any rendering to happen at all
  mVRCompositor->WaitGetPoses(poses, ::vr::k_unMaxTrackedDeviceCount, nullptr, 0);

  VRHMDSensorState result;
  result.Clear();
  result.timestamp = PR_Now();

  if (poses[::vr::k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected &&
      poses[::vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid &&
      poses[::vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult == ::vr::TrackingResult_Running_OK)
  {
    const ::vr::TrackedDevicePose_t& pose = poses[::vr::k_unTrackedDeviceIndex_Hmd];

    gfx::Matrix4x4 m;
    // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.  We do this so we can
    // pull out a Quaternion.
    memcpy(&m._11, &pose.mDeviceToAbsoluteTracking, sizeof(float) * 12);
    m.Transpose();

    gfx::Quaternion rot;
    rot.SetFromRotationMatrix(m);
    rot.Invert();

    result.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
    result.orientation[0] = rot.x;
    result.orientation[1] = rot.y;
    result.orientation[2] = rot.z;
    result.orientation[3] = rot.w;
    result.angularVelocity[0] = pose.vAngularVelocity.v[0];
    result.angularVelocity[1] = pose.vAngularVelocity.v[1];
    result.angularVelocity[2] = pose.vAngularVelocity.v[2];

    result.flags |= VRDisplayCapabilityFlags::Cap_Position;
    result.position[0] = m._41;
    result.position[1] = m._42;
    result.position[2] = m._43;
    result.linearVelocity[0] = pose.vVelocity.v[0];
    result.linearVelocity[1] = pose.vVelocity.v[1];
    result.linearVelocity[2] = pose.vVelocity.v[2];
  }

  return result;
}

void
VRDisplayOpenVR::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }
  mIsPresenting = true;
}

void
VRDisplayOpenVR::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }

  mVRCompositor->ClearLastSubmittedFrame();

  mIsPresenting = false;
}


#if defined(XP_WIN)

void
VRDisplayOpenVR::SubmitFrame(TextureSourceD3D11* aSource,
  const IntSize& aSize,
  const VRHMDSensorState& aSensorState,
  const gfx::Rect& aLeftEyeRect,
  const gfx::Rect& aRightEyeRect)
{
  if (!mIsPresenting) {
    return;
  }

  ::vr::Texture_t tex;
  tex.handle = (void *)aSource->GetD3D11Texture();
  tex.eType = ::vr::EGraphicsAPIConvention::API_DirectX;
  tex.eColorSpace = ::vr::EColorSpace::ColorSpace_Auto;

  ::vr::VRTextureBounds_t bounds;
  bounds.uMin = aLeftEyeRect.x;
  bounds.vMin = 1.0 - aLeftEyeRect.y;
  bounds.uMax = aLeftEyeRect.x + aLeftEyeRect.width;
  bounds.vMax = 1.0 - aLeftEyeRect.y - aLeftEyeRect.height;

  ::vr::EVRCompositorError err;
  err = mVRCompositor->Submit(::vr::EVREye::Eye_Left, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  bounds.uMin = aRightEyeRect.x;
  bounds.vMin = 1.0 - aRightEyeRect.y;
  bounds.uMax = aRightEyeRect.x + aRightEyeRect.width;
  bounds.vMax = 1.0 - aRightEyeRect.y - aRightEyeRect.height;

  err = mVRCompositor->Submit(::vr::EVREye::Eye_Right, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  mVRCompositor->PostPresentHandoff();

  // Trigger the next VSync immediately
  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
}

#endif

void
VRDisplayOpenVR::NotifyVSync()
{
  // We update mIsConneced once per frame.
  mDisplayInfo.mIsConnected = vr_IsHmdPresent();
}

VRDisplayManagerOpenVR::VRDisplayManagerOpenVR()
  : mOpenVRInstalled(false)
{
}

/*static*/ already_AddRefed<VRDisplayManagerOpenVR>
VRDisplayManagerOpenVR::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROpenVREnabled()) {
    return nullptr;
  }

  if (!LoadOpenVRRuntime()) {
    return nullptr;
  }

  RefPtr<VRDisplayManagerOpenVR> manager = new VRDisplayManagerOpenVR();
  return manager.forget();
}

bool
VRDisplayManagerOpenVR::Init()
{
  if (mOpenVRInstalled)
    return true;

  if (!vr_IsRuntimeInstalled())
    return false;

  mOpenVRInstalled = true;
  return true;
}

void
VRDisplayManagerOpenVR::Destroy()
{
  if (mOpenVRInstalled) {
    if (mOpenVRHMD) {
      mOpenVRHMD = nullptr;
    }
    mOpenVRInstalled = false;
  }
}

void
VRDisplayManagerOpenVR::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (!mOpenVRInstalled) {
    return;
  }

  if (!vr_IsHmdPresent()) {
    if (mOpenVRHMD) {
      mOpenVRHMD = nullptr;
    }
  } else if (mOpenVRHMD == nullptr) {
    ::vr::HmdError err;

    vr_InitInternal(&err, ::vr::EVRApplicationType::VRApplication_Scene);
    if (err) {
      return;
    }

    ::vr::IVRSystem *system = (::vr::IVRSystem *)vr_GetGenericInterface(::vr::IVRSystem_Version, &err);
    if (err || !system) {
      vr_ShutdownInternal();
      return;
    }
    ::vr::IVRChaperone *chaperone = (::vr::IVRChaperone *)vr_GetGenericInterface(::vr::IVRChaperone_Version, &err);
    if (err || !chaperone) {
      vr_ShutdownInternal();
      return;
    }
    ::vr::IVRCompositor *compositor = (::vr::IVRCompositor*)vr_GetGenericInterface(::vr::IVRCompositor_Version, &err);
    if (err || !compositor) {
      vr_ShutdownInternal();
      return;
    }

    mOpenVRHMD = new VRDisplayOpenVR(system, chaperone, compositor);
  }

  if (mOpenVRHMD) {
    aHMDResult.AppendElement(mOpenVRHMD);
  }
}

VRControllerOpenVR::VRControllerOpenVR()
  : VRControllerHost(VRDeviceType::OpenVR)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerOpenVR, VRControllerHost);
  mControllerInfo.mControllerName.AssignLiteral("OpenVR HMD");
#ifdef MOZ_GAMEPAD
  mControllerInfo.mMappingType = static_cast<uint32_t>(GamepadMappingType::_empty);
#else
  mControllerInfo.mMappingType = 0;
#endif
  mControllerInfo.mNumButtons = gNumOpenVRButtonMask;
  mControllerInfo.mNumAxes = gNumOpenVRAxis;
}

VRControllerOpenVR::~VRControllerOpenVR()
{
  MOZ_COUNT_DTOR_INHERITED(VRControllerOpenVR, VRControllerHost);
}

void
VRControllerOpenVR::SetTrackedIndex(uint32_t aTrackedIndex)
{
  mTrackedIndex = aTrackedIndex;
}

uint32_t
VRControllerOpenVR::GetTrackedIndex()
{
  return mTrackedIndex;
}

VRControllerManagerOpenVR::VRControllerManagerOpenVR()
  : mOpenVRInstalled(false), mVRSystem(nullptr)
{
}

VRControllerManagerOpenVR::~VRControllerManagerOpenVR()
{
  Destroy();
}

/*static*/ already_AddRefed<VRControllerManagerOpenVR>
VRControllerManagerOpenVR::Create()
{
  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROpenVREnabled()) {
    return nullptr;
  }

  RefPtr<VRControllerManagerOpenVR> manager = new VRControllerManagerOpenVR();
  return manager.forget();
}

bool
VRControllerManagerOpenVR::Init()
{
  if (mOpenVRInstalled)
    return true;

  if (!vr_IsRuntimeInstalled())
    return false;

  // Loading the OpenVR Runtime
  vr::EVRInitError err = vr::VRInitError_None;

  vr_InitInternal(&err, vr::VRApplication_Scene);
  if (err != vr::VRInitError_None) {
    return false;
  }

  mVRSystem = (vr::IVRSystem *)vr_GetGenericInterface(vr::IVRSystem_Version, &err);
  if ((err != vr::VRInitError_None) || !mVRSystem) {
    vr_ShutdownInternal();
    return false;
  }

  mOpenVRInstalled = true;
  return true;
}

void
VRControllerManagerOpenVR::Destroy()
{
  mOpenVRController.Clear();
  mOpenVRInstalled = false;
}

void
VRControllerManagerOpenVR::HandleInput()
{
  RefPtr<impl::VRControllerOpenVR> controller;
  vr::VRControllerState_t state;
  uint32_t axis = 0;

  if (!mOpenVRInstalled) {
    return;
  }

  MOZ_ASSERT(mVRSystem);

  vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
  mVRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, 0.0f,
                                             poses, vr::k_unMaxTrackedDeviceCount);
  // Process OpenVR controller state
  for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
    controller = mOpenVRController[i];

    MOZ_ASSERT(mVRSystem->GetTrackedDeviceClass(controller->GetTrackedIndex())
               == vr::TrackedDeviceClass_Controller);

    if (mVRSystem->GetControllerState(controller->GetTrackedIndex(), &state)) {
      HandleButtonPress(controller->GetIndex(), state.ulButtonPressed);

      axis = static_cast<uint32_t>(VRControllerAxisType::TrackpadXAxis);
      HandleAxisMove(controller->GetIndex(), axis,
                     state.rAxis[gOpenVRAxes[axis]].x);

      axis = static_cast<uint32_t>(VRControllerAxisType::TrackpadYAxis);
      HandleAxisMove(controller->GetIndex(), axis,
                     state.rAxis[gOpenVRAxes[axis]].y);

      axis = static_cast<uint32_t>(VRControllerAxisType::Trigger);
      HandleAxisMove(controller->GetIndex(), axis,
                     state.rAxis[gOpenVRAxes[axis]].x);
    }

    // Start to process pose
    const ::vr::TrackedDevicePose_t& pose = poses[controller->GetTrackedIndex()];

    if (pose.bDeviceIsConnected && pose.bPoseIsValid &&
      pose.eTrackingResult == vr::TrackingResult_Running_OK) {
      gfx::Matrix4x4 m;

      // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
      // because of its arrangement, we can copy the 12 elements in and
      // then transpose them to the right place.  We do this so we can
      // pull out a Quaternion.
      memcpy(&m.components, &pose.mDeviceToAbsoluteTracking, sizeof(float) * 12);
      m.Transpose();

      gfx::Quaternion rot;
      rot.SetFromRotationMatrix(m);
      rot.Invert();

      GamepadPoseState poseState;
      poseState.flags |= GamepadCapabilityFlags::Cap_Orientation;
      poseState.orientation[0] = rot.x;
      poseState.orientation[1] = rot.y;
      poseState.orientation[2] = rot.z;
      poseState.orientation[3] = rot.w;
      poseState.angularVelocity[0] = pose.vAngularVelocity.v[0];
      poseState.angularVelocity[1] = pose.vAngularVelocity.v[1];
      poseState.angularVelocity[2] = pose.vAngularVelocity.v[2];

      poseState.flags |= GamepadCapabilityFlags::Cap_Position;
      poseState.position[0] = m._41;
      poseState.position[1] = m._42;
      poseState.position[2] = m._43;
      poseState.linearVelocity[0] = pose.vVelocity.v[0];
      poseState.linearVelocity[1] = pose.vVelocity.v[1];
      poseState.linearVelocity[2] = pose.vVelocity.v[2];
      HandlePoseTracking(controller->GetIndex(), poseState, controller);
    }
  }
}

void
VRControllerManagerOpenVR::HandleButtonPress(uint32_t aControllerIdx,
                                             uint64_t aButtonPressed)
{
  uint64_t buttonMask = 0;
  RefPtr<impl::VRControllerOpenVR> controller;
  controller = mOpenVRController[aControllerIdx];
  uint64_t diff = (controller->GetButtonPressed() ^ aButtonPressed);

  if (!diff) {
    return;
  }

  for (uint32_t i = 0; i < gNumOpenVRButtonMask; ++i) {
    buttonMask = gOpenVRButtonMask[i];

    if (diff & buttonMask) {
      // diff & aButtonPressed would be true while a new button press
      // event, otherwise it is an old press event and needs to notify
      // the button has been released.
      NewButtonEvent(aControllerIdx, i, diff & aButtonPressed);
    }
  }

  controller->SetButtonPressed(aButtonPressed);
}

void
VRControllerManagerOpenVR::HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                                          float aValue)
{
  if (aValue != 0.0f) {
    NewAxisMove(aControllerIdx, aAxis, aValue);
  }
}

void
VRControllerManagerOpenVR::HandlePoseTracking(uint32_t aControllerIdx,
                                              const GamepadPoseState& aPose,
                                              VRControllerHost* aController)
{
  if (aPose != aController->GetPose()) {
    aController->SetPose(aPose);
    NewPoseState(aControllerIdx, aPose);
  }
}

void
VRControllerManagerOpenVR::GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult)
{
  if (!mOpenVRInstalled) {
    return;
  }

  aControllerResult.Clear();
  for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
    aControllerResult.AppendElement(mOpenVRController[i]);
  }
}

void
VRControllerManagerOpenVR::ScanForDevices()
{
  // Remove the existing gamepads
  for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
    RemoveGamepad(mOpenVRController[i]->GetIndex());
  }
  mControllerCount = 0;
  mOpenVRController.Clear();

  if (!mVRSystem)
    return;

  // Basically, we would have HMDs in the tracked devices, but we are just interested in the controllers.
  for ( vr::TrackedDeviceIndex_t trackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1;
        trackedDevice < vr::k_unMaxTrackedDeviceCount; ++trackedDevice ) {
    if (!mVRSystem->IsTrackedDeviceConnected(trackedDevice)) {
      continue;
    }

    if (mVRSystem->GetTrackedDeviceClass(trackedDevice) != vr::TrackedDeviceClass_Controller) {
      continue;
    }

    RefPtr<VRControllerOpenVR> openVRController = new VRControllerOpenVR();
    openVRController->SetIndex(mControllerCount);
    openVRController->SetTrackedIndex(trackedDevice);
    mOpenVRController.AppendElement(openVRController);

// Only in MOZ_GAMEPAD platform, We add gamepads.
#ifdef MOZ_GAMEPAD
    // Not already present, add it.
    AddGamepad("OpenVR Gamepad", static_cast<uint32_t>(GamepadMappingType::_empty),
               gNumOpenVRButtonMask, gNumOpenVRAxis);
    ++mControllerCount;
#endif
  }
}