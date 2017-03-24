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

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"

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

#define BTN_MASK_FROM_ID(_id) \
  vr::ButtonMaskFromId(vr::EVRButtonId::_id)

static const uint32_t kNumOpenVRHaptcs = 1;


bool
LoadOpenVRRuntime()
{
  static PRLibrary *openvrLib = nullptr;
  std::string openvrPath = gfxPrefs::VROpenVRRuntime();

  if (!openvrPath.c_str())
    return false;

  openvrLib = PR_LoadLibrary(openvrPath.c_str());
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
  mDisplayInfo.mIsMounted = false;
  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                                  VRDisplayCapabilityFlags::Cap_Orientation |
                                  VRDisplayCapabilityFlags::Cap_Position |
                                  VRDisplayCapabilityFlags::Cap_External |
                                  VRDisplayCapabilityFlags::Cap_Present |
                                  VRDisplayCapabilityFlags::Cap_StageParameters;

  ::vr::ETrackedPropertyError err;
  bool bHasProximitySensor = mVRSystem->GetBoolTrackedDeviceProperty(::vr::k_unTrackedDeviceIndex_Hmd, ::vr::Prop_ContainsProximitySensor_Bool, &err);
  if (err == ::vr::TrackedProp_Success && bHasProximitySensor) {
    mDisplayInfo.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_MountDetection;
  }

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

void
VRDisplayOpenVR::PollEvents()
{
  ::vr::VREvent_t event;
  while (mVRSystem->PollNextEvent(&event, sizeof(event))) {
    if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
      switch (event.eventType) {
      case ::vr::VREvent_TrackedDeviceUserInteractionStarted:
        mDisplayInfo.mIsMounted = true;
        break;
      case ::vr::VREvent_TrackedDeviceUserInteractionEnded:
        mDisplayInfo.mIsMounted = false;
        break;
      default:
        // ignore
        break;
      }
    }
  }
}

VRHMDSensorState
VRDisplayOpenVR::GetSensorState(double timeOffset)
{
  PollEvents();

  ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  // Note: We *must* call WaitGetPoses in order for any rendering to happen at all
  mVRCompositor->WaitGetPoses(poses, ::vr::k_unMaxTrackedDeviceCount, nullptr, 0);

  VRHMDSensorState result;
  result.Clear();

  ::vr::Compositor_FrameTiming timing;
  timing.m_nSize = sizeof(::vr::Compositor_FrameTiming);
  if (mVRCompositor->GetFrameTiming(&timing)) {
    result.timestamp = timing.m_flSystemTimeInSeconds;
  } else {
    // This should not happen, but log it just in case
    NS_WARNING("OpenVR - IVRCompositor::GetFrameTiming failed");
  }

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

  // Make sure we respond to OpenVR events even when not presenting
  PollEvents();
}

VRControllerOpenVR::VRControllerOpenVR(dom::GamepadHand aHand, uint32_t aNumButtons,
                                       uint32_t aNumAxes)
  : VRControllerHost(VRDeviceType::OpenVR)
  , mTrigger(0)
  , mVibrateThread(nullptr)
  , mIsVibrateStopped(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerOpenVR, VRControllerHost);
  mControllerInfo.mControllerName.AssignLiteral("OpenVR Gamepad");
  mControllerInfo.mMappingType = GamepadMappingType::_empty;
  mControllerInfo.mHand = aHand;
  mControllerInfo.mNumButtons = aNumButtons;
  mControllerInfo.mNumAxes = aNumAxes;
  mControllerInfo.mNumHaptics = kNumOpenVRHaptcs;
}

VRControllerOpenVR::~VRControllerOpenVR()
{
  if (mVibrateThread) {
    mVibrateThread->Shutdown();
    mVibrateThread = nullptr;
  }

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

void
VRControllerOpenVR::SetTrigger(float aValue)
{
  mTrigger = aValue;
}

float
VRControllerOpenVR::GetTrigger()
{
  return mTrigger;
}

void
VRControllerOpenVR::UpdateVibrateHaptic(vr::IVRSystem* aVRSystem,
                                        uint32_t aHapticIndex,
                                        double aIntensity,
                                        double aDuration,
                                        uint64_t aVibrateIndex,
                                        uint32_t aPromiseID)
{
  // UpdateVibrateHaptic() only can be called by mVibrateThread
  MOZ_ASSERT(mVibrateThread == NS_GetCurrentThread());

  // It has been interrupted by loss focus.
  if (mIsVibrateStopped) {
    VibrateHapticComplete(aPromiseID);
    return;
  }
  // Avoid the previous vibrate event to override the new one.
  if (mVibrateIndex != aVibrateIndex) {
    VibrateHapticComplete(aPromiseID);
    return;
  }

  double duration = (aIntensity == 0) ? 0 : aDuration;
  // We expect OpenVR to vibrate for 5 ms, but we found it only response the
  // commend ~ 3.9 ms. For duration time longer than 3.9 ms, we separate them
  // to a loop of 3.9 ms for make users feel that is a continuous events.
  uint32_t microSec = (duration < 3.9 ? duration : 3.9) * 1000 * aIntensity;
  aVRSystem->TriggerHapticPulse(GetTrackedIndex(),
                                aHapticIndex, microSec);

  // In OpenVR spec, it mentions TriggerHapticPulse() may not trigger another haptic pulse
  // on this controller and axis combination for 5ms.
  const double kVibrateRate = 5.0;
  if (duration >= kVibrateRate) {
    MOZ_ASSERT(mVibrateThread);

    RefPtr<Runnable> runnable =
      NewRunnableMethod<vr::IVRSystem*, uint32_t, double, double, uint64_t, uint32_t>
        (this, &VRControllerOpenVR::UpdateVibrateHaptic, aVRSystem,
         aHapticIndex, aIntensity, duration - kVibrateRate, aVibrateIndex, aPromiseID);
    NS_DelayedDispatchToCurrentThread(runnable.forget(), kVibrateRate);
  } else {
    // The pulse has completed
    VibrateHapticComplete(aPromiseID);
  }
}

void
VRControllerOpenVR::VibrateHapticComplete(uint32_t aPromiseID)
{
  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);

  CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<uint32_t>
    (vm, &VRManager::NotifyVibrateHapticCompleted, aPromiseID));
}

void
VRControllerOpenVR::VibrateHaptic(vr::IVRSystem* aVRSystem,
                                  uint32_t aHapticIndex,
                                  double aIntensity,
                                  double aDuration,
                                  uint32_t aPromiseID)
{
  // Spinning up the haptics thread at the first haptics call.
  if (!mVibrateThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mVibrateThread));
    MOZ_ASSERT(mVibrateThread);

    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false, "Failed to create async thread.");
    }
  }
  ++mVibrateIndex;
  mIsVibrateStopped = false;

  RefPtr<Runnable> runnable =
      NewRunnableMethod<vr::IVRSystem*, uint32_t, double, double, uint64_t, uint32_t>
        (this, &VRControllerOpenVR::UpdateVibrateHaptic, aVRSystem,
         aHapticIndex, aIntensity, aDuration, mVibrateIndex, aPromiseID);
  mVibrateThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

void
VRControllerOpenVR::StopVibrateHaptic()
{
  mIsVibrateStopped = true;
}

VRSystemManagerOpenVR::VRSystemManagerOpenVR()
  : mVRSystem(nullptr)
  , mOpenVRInstalled(false)
{
}

/*static*/ already_AddRefed<VRSystemManagerOpenVR>
VRSystemManagerOpenVR::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROpenVREnabled()) {
    return nullptr;
  }

  if (!LoadOpenVRRuntime()) {
    return nullptr;
  }

  RefPtr<VRSystemManagerOpenVR> manager = new VRSystemManagerOpenVR();
  return manager.forget();
}

bool
VRSystemManagerOpenVR::Init()
{
  if (mOpenVRInstalled)
    return true;

  if (!vr_IsRuntimeInstalled())
    return false;

  mOpenVRInstalled = true;
  return true;
}

void
VRSystemManagerOpenVR::Destroy()
{
  if (mOpenVRInstalled) {
    if (mOpenVRHMD) {
      mOpenVRHMD = nullptr;
    }
    RemoveControllers();
    mVRSystem = nullptr;
    mOpenVRInstalled = false;
  }
}

void
VRSystemManagerOpenVR::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
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

    mVRSystem = system;
    mOpenVRHMD = new VRDisplayOpenVR(system, chaperone, compositor);
  }

  if (mOpenVRHMD) {
    aHMDResult.AppendElement(mOpenVRHMD);
  }
}

bool
VRSystemManagerOpenVR::GetIsPresenting()
{
  if (mOpenVRHMD) {
    VRDisplayInfo displayInfo(mOpenVRHMD->GetDisplayInfo());
    return displayInfo.GetIsPresenting();
  }

  return false;
}

void
VRSystemManagerOpenVR::HandleInput()
{
  // mVRSystem is available after VRDisplay is created
  // at GetHMDs().
  if (!mVRSystem) {
    return;
  }

  RefPtr<impl::VRControllerOpenVR> controller;
  vr::VRControllerState_t state;
  vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
  mVRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, 0.0f,
                                             poses, vr::k_unMaxTrackedDeviceCount);
  // Process OpenVR controller state
  for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
    uint32_t axisIdx = 0;
    uint32_t buttonIdx = 0;
    controller = mOpenVRController[i];
    const uint32_t trackedIndex = controller->GetTrackedIndex();

    MOZ_ASSERT(mVRSystem->GetTrackedDeviceClass(trackedIndex)
               == vr::TrackedDeviceClass_Controller);

    if (mVRSystem->GetControllerState(trackedIndex, &state)) {
      for (uint32_t j = 0; j < vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t axisType = mVRSystem->GetInt32TrackedDeviceProperty(
                                   trackedIndex,
                                   static_cast<vr::TrackedDeviceProperty>(
                                   vr::Prop_Axis0Type_Int32 + j));
        switch (axisType) {
          case vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
            HandleAxisMove(i, axisIdx,
                           state.rAxis[j].x);
            ++axisIdx;
            HandleAxisMove(i, axisIdx,
                           state.rAxis[j].y);
            ++axisIdx;
            HandleButtonPress(i, buttonIdx,
                              vr::ButtonMaskFromId(
                               static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + j)),
                              state.ulButtonPressed);
            ++buttonIdx;
            break;
          case vr::EVRControllerAxisType::k_eControllerAxis_Trigger:
            HandleTriggerPress(i, buttonIdx,
                               vr::ButtonMaskFromId(
                                static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + j)),
                               state.rAxis[j].x, state.ulButtonPressed);
            ++buttonIdx;
            break;
        }
      }
      MOZ_ASSERT(axisIdx ==
                 controller->GetControllerInfo().GetNumAxes());

      const uint64_t supportedButtons = mVRSystem->GetUint64TrackedDeviceProperty(
                                         trackedIndex, vr::Prop_SupportedButtons_Uint64);
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_A)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_A),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_Grip)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_Grip),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_ApplicationMenu),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Left),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Up),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Right),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Down),
                          state.ulButtonPressed);
        ++buttonIdx;
      }
      MOZ_ASSERT(buttonIdx ==
                 controller->GetControllerInfo().GetNumButtons());
      controller->SetButtonPressed(state.ulButtonPressed);

      // Start to process pose
      const ::vr::TrackedDevicePose_t& pose = poses[trackedIndex];

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
        HandlePoseTracking(i, poseState, controller);
      }
    }
  }
}

void
VRSystemManagerOpenVR::HandleButtonPress(uint32_t aControllerIdx,
                                         uint32_t aButton,
                                         uint64_t aButtonMask,
                                         uint64_t aButtonPressed)
{
  RefPtr<impl::VRControllerOpenVR> controller(mOpenVRController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const uint64_t diff = (controller->GetButtonPressed() ^ aButtonPressed);

  if (!diff) {
    return;
  }

  if (diff & aButtonMask) {
    // diff & aButtonPressed would be true while a new button press
    // event, otherwise it is an old press event and needs to notify
    // the button has been released.
    NewButtonEvent(aControllerIdx, aButton, aButtonMask & aButtonPressed,
                   (aButtonMask & aButtonPressed) ? 1.0L : 0.0L);
  }
}

void
VRSystemManagerOpenVR::HandleTriggerPress(uint32_t aControllerIdx,
                                          uint32_t aButton,
                                          uint64_t aButtonMask,
                                          float aValue,
                                          uint64_t aButtonPressed)
{
  RefPtr<impl::VRControllerOpenVR> controller(mOpenVRController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const uint64_t diff = (controller->GetButtonPressed() ^ aButtonPressed);
  const float oldValue = controller->GetTrigger();

  // Avoid sending duplicated events in IPC channels.
  if ((oldValue != aValue) ||
      (diff & aButtonMask)) {
    NewButtonEvent(aControllerIdx, aButton, aButtonMask & aButtonPressed, aValue);
    controller->SetTrigger(aValue);
  }
}

void
VRSystemManagerOpenVR::HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                                      float aValue)
{
  if (aValue != 0.0f) {
    NewAxisMove(aControllerIdx, aAxis, aValue);
  }
}

void
VRSystemManagerOpenVR::HandlePoseTracking(uint32_t aControllerIdx,
                                          const GamepadPoseState& aPose,
                                          VRControllerHost* aController)
{
  MOZ_ASSERT(aController);
  if (aPose != aController->GetPose()) {
    aController->SetPose(aPose);
    NewPoseState(aControllerIdx, aPose);
  }
}

void
VRSystemManagerOpenVR::VibrateHaptic(uint32_t aControllerIdx,
                                     uint32_t aHapticIndex,
                                     double aIntensity,
                                     double aDuration,
                                     uint32_t aPromiseID)
{
  // mVRSystem is available after VRDisplay is created
  // at GetHMDs().
  if (!mVRSystem) {
    return;
  }

  RefPtr<impl::VRControllerOpenVR> controller = mOpenVRController[aControllerIdx];
  MOZ_ASSERT(controller);

  controller->VibrateHaptic(mVRSystem, aHapticIndex, aIntensity, aDuration, aPromiseID);
}

void
VRSystemManagerOpenVR::StopVibrateHaptic(uint32_t aControllerIdx)
{
  // mVRSystem is available after VRDisplay is created
  // at GetHMDs().
  if (!mVRSystem) {
    return;
  }

  RefPtr<impl::VRControllerOpenVR> controller = mOpenVRController[aControllerIdx];
  MOZ_ASSERT(controller);

  controller->StopVibrateHaptic();
}

void
VRSystemManagerOpenVR::GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult)
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
VRSystemManagerOpenVR::ScanForControllers()
{
  // mVRSystem is available after VRDisplay is created
  // at GetHMDs().
  if (!mVRSystem) {
    return;
  }

  vr::TrackedDeviceIndex_t trackedIndexArray[vr::k_unMaxTrackedDeviceCount];
  uint32_t newControllerCount = 0;
  // Basically, we would have HMDs in the tracked devices,
  // but we are just interested in the controllers.
  for (vr::TrackedDeviceIndex_t trackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1;
       trackedDevice < vr::k_unMaxTrackedDeviceCount; ++trackedDevice) {

    if (!mVRSystem->IsTrackedDeviceConnected(trackedDevice)) {
      continue;
    }
    if (mVRSystem->GetTrackedDeviceClass(trackedDevice)
        != vr::TrackedDeviceClass_Controller) {
      continue;
    }

    trackedIndexArray[newControllerCount] = trackedDevice;
    ++newControllerCount;
  }

  if (newControllerCount != mControllerCount) {
    // The controller count is changed, removing the existing gamepads first.
    for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
      RemoveGamepad(i);
    }
    mControllerCount = 0;
    mOpenVRController.Clear();

    // Re-adding controllers to VRControllerManager.
    for (vr::TrackedDeviceIndex_t i = 0; i < newControllerCount; ++i) {
      const vr::TrackedDeviceIndex_t trackedDevice = trackedIndexArray[i];
      const vr::ETrackedControllerRole role = mVRSystem->
                                               GetControllerRoleForTrackedDeviceIndex(
                                               trackedDevice);
      GamepadHand hand;
      uint32_t numButtons = 0;
      uint32_t numAxes = 0;

      switch(role) {
        case vr::ETrackedControllerRole::TrackedControllerRole_Invalid:
          hand = GamepadHand::_empty;
          break;
        case vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
          hand = GamepadHand::Left;
          break;
        case vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
          hand = GamepadHand::Right;
          break;
      }

      // Scan the axes that the controllers support
      for (uint32_t j = 0; j < vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t supportAxis = mVRSystem->GetInt32TrackedDeviceProperty(trackedDevice,
                                      static_cast<vr::TrackedDeviceProperty>(
                                      vr::Prop_Axis0Type_Int32 + j));
        switch (supportAxis) {
          case vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
            numAxes += 2; // It has x and y axes.
            ++numButtons;
            break;
          case vr::k_eControllerAxis_Trigger:
            ++numButtons;
            break;
        }
      }

      // Scan the buttons that the controllers support
      const uint64_t supportButtons = mVRSystem->GetUint64TrackedDeviceProperty(
                                       trackedDevice, vr::Prop_SupportedButtons_Uint64);
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_A)) {
        ++numButtons;
      }
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_Grip)) {
        ++numButtons;
      }
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        ++numButtons;
      }
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        ++numButtons;
      }
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        ++numButtons;
      }
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        ++numButtons;
      }
      if (supportButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        ++numButtons;
      }

      RefPtr<VRControllerOpenVR> openVRController =
        new VRControllerOpenVR(hand, numButtons, numAxes);
      openVRController->SetTrackedIndex(trackedDevice);
      mOpenVRController.AppendElement(openVRController);

      // Not already present, add it.
      AddGamepad(openVRController->GetControllerInfo());
      ++mControllerCount;
    }
  }
}

void
VRSystemManagerOpenVR::RemoveControllers()
{
  mOpenVRController.Clear();
  mControllerCount = 0;
}

