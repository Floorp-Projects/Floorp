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
#include "VRManager.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#include "mozilla/layers/CompositorThread.h"
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

#define BTN_MASK_FROM_ID(_id) \
  ::vr::ButtonMaskFromId(vr::EVRButtonId::_id)

static const uint32_t kNumOpenVRHaptcs = 1;

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
  ::vr::VR_Shutdown();
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
  tex.eType = ::vr::ETextureType::TextureType_DirectX;
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
  mDisplayInfo.mIsConnected = ::vr::VR_IsHmdPresent();

  // Make sure we respond to OpenVR events even when not presenting
  PollEvents();
}

VRControllerOpenVR::VRControllerOpenVR(dom::GamepadHand aHand, uint32_t aNumButtons,
                                       uint32_t aNumAxes, ::vr::ETrackedDeviceClass aDeviceType)
  : VRControllerHost(VRDeviceType::OpenVR)
  , mTrigger(0)
  , mAxisMove(aNumAxes)
  , mVibrateThread(nullptr)
  , mIsVibrateStopped(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerOpenVR, VRControllerHost);

  switch (aDeviceType) {
    case ::vr::TrackedDeviceClass_Controller:
      mControllerInfo.mControllerName.AssignLiteral("OpenVR Gamepad");
      break;
    case ::vr::TrackedDeviceClass_GenericTracker:
      mControllerInfo.mControllerName.AssignLiteral("OpenVR Tracker");
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }

  mAxisMove.SetLengthAndRetainStorage(aNumAxes);
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

float
VRControllerOpenVR::GetAxisMove(uint32_t aAxis)
{
  return mAxisMove[aAxis];
}

void
VRControllerOpenVR::SetAxisMove(uint32_t aAxis, float aValue)
{
  mAxisMove[aAxis] = aValue;
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
VRControllerOpenVR::SetHand(dom::GamepadHand aHand)
{
  mControllerInfo.mHand = aHand;
}

void
VRControllerOpenVR::UpdateVibrateHaptic(::vr::IVRSystem* aVRSystem,
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

  const double duration = (aIntensity == 0) ? 0 : aDuration;
  // We expect OpenVR to vibrate for 5 ms, but we found it only response the
  // commend ~ 3.9 ms. For duration time longer than 3.9 ms, we separate them
  // to a loop of 3.9 ms for make users feel that is a continuous events.
  const uint32_t microSec = (duration < 3.9 ? duration : 3.9) * 1000 * aIntensity;
  aVRSystem->TriggerHapticPulse(GetTrackedIndex(),
                                aHapticIndex, microSec);

  // In OpenVR spec, it mentions TriggerHapticPulse() may not trigger another haptic pulse
  // on this controller and axis combination for 5ms.
  const double kVibrateRate = 5.0;
  if (duration >= kVibrateRate) {
    MOZ_ASSERT(mVibrateThread);

    RefPtr<Runnable> runnable =
      NewRunnableMethod<::vr::IVRSystem*, uint32_t, double, double, uint64_t, uint32_t>
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
VRControllerOpenVR::VibrateHaptic(::vr::IVRSystem* aVRSystem,
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
      NewRunnableMethod<::vr::IVRSystem*, uint32_t, double, double, uint64_t, uint32_t>
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
{
}

/*static*/ already_AddRefed<VRSystemManagerOpenVR>
VRSystemManagerOpenVR::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROpenVREnabled()) {
    return nullptr;
  }

  if (!::vr::VR_IsRuntimeInstalled()) {
    return nullptr;
  }

  RefPtr<VRSystemManagerOpenVR> manager = new VRSystemManagerOpenVR();
  return manager.forget();
}

void
VRSystemManagerOpenVR::Destroy()
{
  Shutdown();
}

void
VRSystemManagerOpenVR::Shutdown()
{
  if (mOpenVRHMD) {
    mOpenVRHMD = nullptr;
  }
  RemoveControllers();
  mVRSystem = nullptr;
}

void
VRSystemManagerOpenVR::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (!::vr::VR_IsHmdPresent()) {
    if (mOpenVRHMD) {
      mOpenVRHMD = nullptr;
    }
  } else if (mOpenVRHMD == nullptr) {
    ::vr::HmdError err;

    ::vr::VR_Init(&err, ::vr::EVRApplicationType::VRApplication_Scene);
    if (err) {
      return;
    }

    ::vr::IVRSystem *system = (::vr::IVRSystem *)::vr::VR_GetGenericInterface(::vr::IVRSystem_Version, &err);
    if (err || !system) {
      ::vr::VR_Shutdown();
      return;
    }
    ::vr::IVRChaperone *chaperone = (::vr::IVRChaperone *)::vr::VR_GetGenericInterface(::vr::IVRChaperone_Version, &err);
    if (err || !chaperone) {
      ::vr::VR_Shutdown();
      return;
    }
    ::vr::IVRCompositor *compositor = (::vr::IVRCompositor*)::vr::VR_GetGenericInterface(::vr::IVRCompositor_Version, &err);
    if (err || !compositor) {
      ::vr::VR_Shutdown();
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
  ::vr::VRControllerState_t state;
  ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  mVRSystem->GetDeviceToAbsoluteTrackingPose(::vr::TrackingUniverseSeated, 0.0f,
                                             poses, ::vr::k_unMaxTrackedDeviceCount);
  // Process OpenVR controller state
  for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
    uint32_t axisIdx = 0;
    uint32_t buttonIdx = 0;
    controller = mOpenVRController[i];
    const uint32_t trackedIndex = controller->GetTrackedIndex();

    MOZ_ASSERT(mVRSystem->GetTrackedDeviceClass(trackedIndex)
               == ::vr::TrackedDeviceClass_Controller ||
               mVRSystem->GetTrackedDeviceClass(trackedIndex)
               == ::vr::TrackedDeviceClass_GenericTracker);

    // Sometimes, OpenVR controllers are not located by HMD at the initial time.
    // That makes us have to update the hand info at runtime although switching controllers
    // to the other hand does not have new changes at the current OpenVR SDK. But, it makes sense
    // to detect hand changing at runtime.
    const ::vr::ETrackedControllerRole role = mVRSystem->
                                                GetControllerRoleForTrackedDeviceIndex(
                                                trackedIndex);
    const dom::GamepadHand hand = GetGamepadHandFromControllerRole(role);
    if (hand != controller->GetHand()) {
      controller->SetHand(hand);
      NewHandChangeEvent(i, hand);
    }

    if (mVRSystem->GetControllerState(trackedIndex, &state, sizeof(state))) {
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t axisType = mVRSystem->GetInt32TrackedDeviceProperty(
                                   trackedIndex,
                                   static_cast<::vr::TrackedDeviceProperty>(
                                   ::vr::Prop_Axis0Type_Int32 + j));
        switch (axisType) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
            HandleAxisMove(i, axisIdx,
                           state.rAxis[j].x);
            ++axisIdx;
            HandleAxisMove(i, axisIdx,
                           state.rAxis[j].y);
            ++axisIdx;
            HandleButtonPress(i, buttonIdx,
                              ::vr::ButtonMaskFromId(
                                 static_cast<::vr::EVRButtonId>(::vr::k_EButton_Axis0 + j)),
                                 state.ulButtonPressed, state.ulButtonTouched);
            ++buttonIdx;
            break;
          case vr::EVRControllerAxisType::k_eControllerAxis_Trigger:
            HandleTriggerPress(i, buttonIdx, state.rAxis[j].x);
            ++buttonIdx;
            break;
        }
      }
      MOZ_ASSERT(axisIdx ==
                 controller->GetControllerInfo().GetNumAxes());

      const uint64_t supportedButtons = mVRSystem->GetUint64TrackedDeviceProperty(
                                         trackedIndex, ::vr::Prop_SupportedButtons_Uint64);
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_A)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_A),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_Grip)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_Grip),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_ApplicationMenu),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Left),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Up),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Right),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        HandleButtonPress(i, buttonIdx,
                          BTN_MASK_FROM_ID(k_EButton_DPad_Down),
                          state.ulButtonPressed, state.ulButtonTouched);
        ++buttonIdx;
      }
      MOZ_ASSERT(buttonIdx ==
                 controller->GetControllerInfo().GetNumButtons());
      controller->SetButtonPressed(state.ulButtonPressed);
      controller->SetButtonTouched(state.ulButtonTouched);

      // Start to process pose
      const ::vr::TrackedDevicePose_t& pose = poses[trackedIndex];

      if (pose.bDeviceIsConnected && pose.bPoseIsValid &&
        pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
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
                                         uint64_t aButtonPressed,
                                         uint64_t aButtonTouched)
{
  RefPtr<impl::VRControllerOpenVR> controller(mOpenVRController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const uint64_t pressedDiff = (controller->GetButtonPressed() ^ aButtonPressed);
  const uint64_t touchedDiff = (controller->GetButtonTouched() ^ aButtonTouched);

  if (!pressedDiff && !touchedDiff) {
    return;
  }

  if (pressedDiff & aButtonMask ||
      touchedDiff & aButtonMask) {
    // diff & (aButtonPressed, aButtonTouched) would be true while a new button pressed or
    // touched event, otherwise it is an old event and needs to notify
    // the button has been released.
    NewButtonEvent(aControllerIdx, aButton, aButtonMask & aButtonPressed,
                   aButtonMask & aButtonTouched,
                   (aButtonMask & aButtonPressed) ? 1.0L : 0.0L);
  }
}

void
VRSystemManagerOpenVR::HandleTriggerPress(uint32_t aControllerIdx,
                                          uint32_t aButton,
                                          float aValue)
{
  RefPtr<impl::VRControllerOpenVR> controller(mOpenVRController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const float oldValue = controller->GetTrigger();
  // For OpenVR, the threshold value of ButtonPressed and ButtonTouched is 0.55.
  // We prefer to let developers to set their own threshold for the adjustment.
  // Therefore, we don't check ButtonPressed and ButtonTouched with ButtonMask here.
  // we just check the button value is larger than the threshold value or not.
  const float threshold = gfxPrefs::VRControllerTriggerThreshold();

  // Avoid sending duplicated events in IPC channels.
  if (oldValue != aValue) {
    NewButtonEvent(aControllerIdx, aButton, aValue > threshold,
                   aValue > threshold, aValue);
    controller->SetTrigger(aValue);
  }
}

void
VRSystemManagerOpenVR::HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                                      float aValue)
{
  RefPtr<impl::VRControllerOpenVR> controller(mOpenVRController[aControllerIdx]);
  MOZ_ASSERT(controller);

  if (controller->GetAxisMove(aAxis) != aValue) {
    NewAxisMove(aControllerIdx, aAxis, aValue);
    controller->SetAxisMove(aAxis, aValue);
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

dom::GamepadHand
VRSystemManagerOpenVR::GetGamepadHandFromControllerRole(
                                          ::vr::ETrackedControllerRole aRole)
{
  dom::GamepadHand hand;

  switch(aRole) {
    case ::vr::ETrackedControllerRole::TrackedControllerRole_Invalid:
      hand = dom::GamepadHand::_empty;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
      hand = dom::GamepadHand::Left;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
      hand = dom::GamepadHand::Right;
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }

  return hand;
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

  ::vr::TrackedDeviceIndex_t trackedIndexArray[::vr::k_unMaxTrackedDeviceCount];
  uint32_t newControllerCount = 0;
  // Basically, we would have HMDs in the tracked devices,
  // but we are just interested in the controllers.
  for (::vr::TrackedDeviceIndex_t trackedDevice = ::vr::k_unTrackedDeviceIndex_Hmd + 1;
       trackedDevice < ::vr::k_unMaxTrackedDeviceCount; ++trackedDevice) {

    if (!mVRSystem->IsTrackedDeviceConnected(trackedDevice)) {
      continue;
    }

    const ::vr::ETrackedDeviceClass deviceType = mVRSystem->
                                                 GetTrackedDeviceClass(trackedDevice);
    if (deviceType != ::vr::TrackedDeviceClass_Controller
        && deviceType != ::vr::TrackedDeviceClass_GenericTracker) {
      continue;
    }

    trackedIndexArray[newControllerCount] = trackedDevice;
    ++newControllerCount;
  }

  if (newControllerCount != mControllerCount) {
    RemoveControllers();

    // Re-adding controllers to VRControllerManager.
    for (::vr::TrackedDeviceIndex_t i = 0; i < newControllerCount; ++i) {
      const ::vr::TrackedDeviceIndex_t trackedDevice = trackedIndexArray[i];
      const ::vr::ETrackedDeviceClass deviceType = mVRSystem->
                                                   GetTrackedDeviceClass(trackedDevice);
      const ::vr::ETrackedControllerRole role = mVRSystem->
                                                GetControllerRoleForTrackedDeviceIndex(
                                                trackedDevice);
      const GamepadHand hand = GetGamepadHandFromControllerRole(role);
      uint32_t numButtons = 0;
      uint32_t numAxes = 0;

      // Scan the axes that the controllers support
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t supportAxis = mVRSystem->GetInt32TrackedDeviceProperty(trackedDevice,
                                      static_cast<vr::TrackedDeviceProperty>(
                                      ::vr::Prop_Axis0Type_Int32 + j));
        switch (supportAxis) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
            numAxes += 2; // It has x and y axes.
            ++numButtons;
            break;
          case ::vr::k_eControllerAxis_Trigger:
            ++numButtons;
            break;
        }
      }

      // Scan the buttons that the controllers support
      const uint64_t supportButtons = mVRSystem->GetUint64TrackedDeviceProperty(
                                       trackedDevice, ::vr::Prop_SupportedButtons_Uint64);
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
        new VRControllerOpenVR(hand, numButtons, numAxes, deviceType);
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
  // The controller count is changed, removing the existing gamepads first.
  for (uint32_t i = 0; i < mOpenVRController.Length(); ++i) {
    RemoveGamepad(i);
  }
  mOpenVRController.Clear();
  mControllerCount = 0;
}

