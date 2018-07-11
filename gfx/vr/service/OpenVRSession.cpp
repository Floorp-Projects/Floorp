#include "OpenVRSession.h"
#include "gfxPrefs.h"

#if defined(XP_WIN)
#include <d3d11.h>
#include "mozilla/gfx/DeviceManagerDx.h"
#endif // defined(XP_WIN)

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"

#if !defined(M_PI)
#define M_PI 3.14159265358979323846264338327950288
#endif

#define BTN_MASK_FROM_ID(_id) \
  ::vr::ButtonMaskFromId(vr::EVRButtonId::_id)

static const uint32_t kNumOpenVRHaptcs = 1;

using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

namespace {

dom::GamepadHand
GetControllerHandFromControllerRole(::vr::ETrackedControllerRole aRole)
{
  dom::GamepadHand hand;

  switch(aRole) {
    case ::vr::ETrackedControllerRole::TrackedControllerRole_Invalid:
    case ::vr::ETrackedControllerRole::TrackedControllerRole_OptOut:
      hand = dom::GamepadHand::_empty;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
      hand = dom::GamepadHand::Left;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
      hand = dom::GamepadHand::Right;
      break;
    default:
      hand = dom::GamepadHand::_empty;
      MOZ_ASSERT(false);
      break;
  }

  return hand;
}


void
UpdateButton(VRControllerState& aState, const ::vr::VRControllerState_t& aControllerState, uint32_t aButtonIndex, uint64_t aButtonMask)
{
  uint64_t mask = (1ULL << aButtonIndex);
  if ((aControllerState.ulButtonPressed & aButtonMask) == 0) {
    // not pressed
    aState.buttonPressed &= ~mask;
    aState.triggerValue[aButtonIndex] = 0.0f;
  } else {
    // pressed
    aState.buttonPressed |= mask;
    aState.triggerValue[aButtonIndex] = 1.0f;
  }
  if ((aControllerState.ulButtonTouched & aButtonMask) == 0) {
    // not touched
    aState.buttonTouched &= ~mask;
  } else {
    // touched
    aState.buttonTouched |= mask;
  }
}

void
UpdateTrigger(VRControllerState& aState, uint32_t aButtonIndex, float aValue, float aThreshold)
{
  // For OpenVR, the threshold value of ButtonPressed and ButtonTouched is 0.55.
  // We prefer to let developers to set their own threshold for the adjustment.
  // Therefore, we don't check ButtonPressed and ButtonTouched with ButtonMask here.
  // we just check the button value is larger than the threshold value or not.
  uint64_t mask = (1ULL << aButtonIndex);
  aState.triggerValue[aButtonIndex] = aValue;
  if (aValue > aThreshold) {
    aState.buttonPressed |= mask;
    aState.buttonTouched |= mask;
  } else {
    aState.buttonPressed &= ~mask;
    aState.buttonTouched &= ~mask;
  }
}

}; // anonymous namespace

OpenVRSession::OpenVRSession()
  : VRSession()
  , mVRSystem(nullptr)
  , mVRChaperone(nullptr)
  , mVRCompositor(nullptr)
  , mControllerDeviceIndex{0}
  , mShouldQuit(false)
  , mIsWindowsMR(false)
{
}

OpenVRSession::~OpenVRSession()
{
  Shutdown();
}

bool
OpenVRSession::Initialize(mozilla::gfx::VRSystemState& aSystemState)
{
  if (mVRSystem != nullptr) {
    // Already initialized
    return true;
  }
  if (!::vr::VR_IsRuntimeInstalled()) {
    return false;
  }
  if (!::vr::VR_IsHmdPresent()) {
    return false;
  }

  ::vr::HmdError err;

  ::vr::VR_Init(&err, ::vr::EVRApplicationType::VRApplication_Scene);
  if (err) {
    return false;
  }

  mVRSystem = (::vr::IVRSystem *)::vr::VR_GetGenericInterface(::vr::IVRSystem_Version, &err);
  if (err || !mVRSystem) {
    Shutdown();
    return false;
  }
  mVRChaperone = (::vr::IVRChaperone *)::vr::VR_GetGenericInterface(::vr::IVRChaperone_Version, &err);
  if (err || !mVRChaperone) {
    Shutdown();
    return false;
  }
  mVRCompositor = (::vr::IVRCompositor*)::vr::VR_GetGenericInterface(::vr::IVRCompositor_Version, &err);
  if (err || !mVRCompositor) {
    Shutdown();
    return false;
  }

#if defined(XP_WIN)
  if (!CreateD3DObjects()) {
    Shutdown();
    return false;
  }

#endif

  // Configure coordinate system
  mVRCompositor->SetTrackingSpace(::vr::TrackingUniverseSeated);

  if (!InitState(aSystemState)) {
    Shutdown();
    return false;
  }

  // Succeeded
  return true;
}

#if defined(XP_WIN)
bool
OpenVRSession::CreateD3DObjects()
{
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetVRDevice();
  if (!device) {
    return false;
  }
  if (!CreateD3DContext(device)) {
    return false;
  }
  return true;
}
#endif

void
OpenVRSession::Shutdown()
{
  if (mVRSystem || mVRCompositor || mVRSystem) {
    ::vr::VR_Shutdown();
    mVRCompositor = nullptr;
    mVRChaperone = nullptr;
    mVRSystem = nullptr;
  }
}

bool
OpenVRSession::InitState(VRSystemState& aSystemState)
{
  VRDisplayState& state = aSystemState.displayState;
  strncpy(state.mDisplayName, "OpenVR HMD", kVRDisplayNameMaxLen);
  state.mIsConnected = mVRSystem->IsTrackedDeviceConnected(::vr::k_unTrackedDeviceIndex_Hmd);
  state.mIsMounted = false;
  state.mCapabilityFlags = (VRDisplayCapabilityFlags)((int)VRDisplayCapabilityFlags::Cap_None |
    (int)VRDisplayCapabilityFlags::Cap_Orientation |
    (int)VRDisplayCapabilityFlags::Cap_Position |
    (int)VRDisplayCapabilityFlags::Cap_External |
    (int)VRDisplayCapabilityFlags::Cap_Present |
    (int)VRDisplayCapabilityFlags::Cap_StageParameters);

  ::vr::ETrackedPropertyError err;
  bool bHasProximitySensor = mVRSystem->GetBoolTrackedDeviceProperty(::vr::k_unTrackedDeviceIndex_Hmd, ::vr::Prop_ContainsProximitySensor_Bool, &err);
  if (err == ::vr::TrackedProp_Success && bHasProximitySensor) {
    state.mCapabilityFlags = (VRDisplayCapabilityFlags)((int)state.mCapabilityFlags | (int)VRDisplayCapabilityFlags::Cap_MountDetection);
  }

  uint32_t w, h;
  mVRSystem->GetRecommendedRenderTargetSize(&w, &h);
  state.mEyeResolution.width = w;
  state.mEyeResolution.height = h;

  // default to an identity quaternion
  aSystemState.sensorState.pose.orientation[3] = 1.0f;

  UpdateStageParameters(state);
  UpdateEyeParameters(aSystemState);

  VRHMDSensorState& sensorState = aSystemState.sensorState;
  sensorState.flags = (VRDisplayCapabilityFlags)(
    (int)VRDisplayCapabilityFlags::Cap_Orientation |
    (int)VRDisplayCapabilityFlags::Cap_Position);
  sensorState.pose.orientation[3] = 1.0f; // Default to an identity quaternion

  return true;
}

void
OpenVRSession::UpdateStageParameters(VRDisplayState& aState)
{
  float sizeX = 0.0f;
  float sizeZ = 0.0f;
  if (mVRChaperone->GetPlayAreaSize(&sizeX, &sizeZ)) {
    ::vr::HmdMatrix34_t t = mVRSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
    aState.mStageSize.width = sizeX;
    aState.mStageSize.height = sizeZ;

    aState.mSittingToStandingTransform[0] = t.m[0][0];
    aState.mSittingToStandingTransform[1] = t.m[1][0];
    aState.mSittingToStandingTransform[2] = t.m[2][0];
    aState.mSittingToStandingTransform[3] = 0.0f;

    aState.mSittingToStandingTransform[4] = t.m[0][1];
    aState.mSittingToStandingTransform[5] = t.m[1][1];
    aState.mSittingToStandingTransform[6] = t.m[2][1];
    aState.mSittingToStandingTransform[7] = 0.0f;

    aState.mSittingToStandingTransform[8] = t.m[0][2];
    aState.mSittingToStandingTransform[9] = t.m[1][2];
    aState.mSittingToStandingTransform[10] = t.m[2][2];
    aState.mSittingToStandingTransform[11] = 0.0f;

    aState.mSittingToStandingTransform[12] = t.m[0][3];
    aState.mSittingToStandingTransform[13] = t.m[1][3];
    aState.mSittingToStandingTransform[14] = t.m[2][3];
    aState.mSittingToStandingTransform[15] = 1.0f;
  } else {
    // If we fail, fall back to reasonable defaults.
    // 1m x 1m space, 0.75m high in seated position
    aState.mStageSize.width = 1.0f;
    aState.mStageSize.height = 1.0f;

    aState.mSittingToStandingTransform[0] = 1.0f;
    aState.mSittingToStandingTransform[1] = 0.0f;
    aState.mSittingToStandingTransform[2] = 0.0f;
    aState.mSittingToStandingTransform[3] = 0.0f;

    aState.mSittingToStandingTransform[4] = 0.0f;
    aState.mSittingToStandingTransform[5] = 1.0f;
    aState.mSittingToStandingTransform[6] = 0.0f;
    aState.mSittingToStandingTransform[7] = 0.0f;

    aState.mSittingToStandingTransform[8] = 0.0f;
    aState.mSittingToStandingTransform[9] = 0.0f;
    aState.mSittingToStandingTransform[10] = 1.0f;
    aState.mSittingToStandingTransform[11] = 0.0f;

    aState.mSittingToStandingTransform[12] = 0.0f;
    aState.mSittingToStandingTransform[13] = 0.75f;
    aState.mSittingToStandingTransform[14] = 0.0f;
    aState.mSittingToStandingTransform[15] = 1.0f;
  }
}

void
OpenVRSession::UpdateEyeParameters(VRSystemState& aState)
{
  // This must be called every frame in order to
  // account for continuous adjustments to ipd.
  gfx::Matrix4x4 headToEyeTransforms[2];

  for (uint32_t eye = 0; eye < 2; ++eye) {
    ::vr::HmdMatrix34_t eyeToHead = mVRSystem->GetEyeToHeadTransform(static_cast<::vr::Hmd_Eye>(eye));
    aState.displayState.mEyeTranslation[eye].x = eyeToHead.m[0][3];
    aState.displayState.mEyeTranslation[eye].y = eyeToHead.m[1][3];
    aState.displayState.mEyeTranslation[eye].z = eyeToHead.m[2][3];

    float left, right, up, down;
    mVRSystem->GetProjectionRaw(static_cast<::vr::Hmd_Eye>(eye), &left, &right, &up, &down);
    aState.displayState.mEyeFOV[eye].upDegrees = atan(-up) * 180.0 / M_PI;
    aState.displayState.mEyeFOV[eye].rightDegrees = atan(right) * 180.0 / M_PI;
    aState.displayState.mEyeFOV[eye].downDegrees = atan(down) * 180.0 / M_PI;
    aState.displayState.mEyeFOV[eye].leftDegrees = atan(-left) * 180.0 / M_PI;

    Matrix4x4 pose;
    // NOTE! eyeToHead.m is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.
    memcpy(&pose._11, &eyeToHead.m, sizeof(eyeToHead.m));
    pose.Transpose();
    pose.Invert();
    headToEyeTransforms[eye] = pose;
  }
  aState.sensorState.CalcViewMatrices(headToEyeTransforms);
}

void
OpenVRSession::UpdateHeadsetPose(VRSystemState& aState)
{
  const uint32_t posesSize = ::vr::k_unTrackedDeviceIndex_Hmd + 1;
  ::vr::TrackedDevicePose_t poses[posesSize];
  // Note: We *must* call WaitGetPoses in order for any rendering to happen at all.
  mVRCompositor->WaitGetPoses(nullptr, 0, poses, posesSize);

  ::vr::Compositor_FrameTiming timing;
  timing.m_nSize = sizeof(::vr::Compositor_FrameTiming);
  if (mVRCompositor->GetFrameTiming(&timing)) {
    aState.sensorState.timestamp = timing.m_flSystemTimeInSeconds;
  } else {
    // This should not happen, but log it just in case
    fprintf(stderr, "OpenVR - IVRCompositor::GetFrameTiming failed");
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
    memcpy(&m._11, &pose.mDeviceToAbsoluteTracking, sizeof(pose.mDeviceToAbsoluteTracking));
    m.Transpose();

    gfx::Quaternion rot;
    rot.SetFromRotationMatrix(m);
    rot.Invert();

    aState.sensorState.flags = (VRDisplayCapabilityFlags)((int)aState.sensorState.flags | (int)VRDisplayCapabilityFlags::Cap_Orientation);
    aState.sensorState.pose.orientation[0] = rot.x;
    aState.sensorState.pose.orientation[1] = rot.y;
    aState.sensorState.pose.orientation[2] = rot.z;
    aState.sensorState.pose.orientation[3] = rot.w;
    aState.sensorState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
    aState.sensorState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
    aState.sensorState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];

    aState.sensorState.flags =(VRDisplayCapabilityFlags)((int)aState.sensorState.flags | (int)VRDisplayCapabilityFlags::Cap_Position);
    aState.sensorState.pose.position[0] = m._41;
    aState.sensorState.pose.position[1] = m._42;
    aState.sensorState.pose.position[2] = m._43;
    aState.sensorState.pose.linearVelocity[0] = pose.vVelocity.v[0];
    aState.sensorState.pose.linearVelocity[1] = pose.vVelocity.v[1];
    aState.sensorState.pose.linearVelocity[2] = pose.vVelocity.v[2];
  }
}

void
OpenVRSession::EnumerateControllers(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  bool controllerPresent[kVRControllerMaxCount] = { false };

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

    uint32_t stateIndex = 0;
    uint32_t firstEmptyIndex = kVRControllerMaxCount;

    // Find the existing controller
    for (stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
      if (mControllerDeviceIndex[stateIndex] == 0 && firstEmptyIndex == kVRControllerMaxCount) {
        firstEmptyIndex = stateIndex;
      }
      if (mControllerDeviceIndex[stateIndex] == trackedDevice) {
        break;
      }
    }
    if (stateIndex == kVRControllerMaxCount) {
      // This is a new controller, let's add it
      if (firstEmptyIndex == kVRControllerMaxCount) {
        NS_WARNING("OpenVR - Too many controllers, need to increase kVRControllerMaxCount.");
        continue;
      }
      stateIndex = firstEmptyIndex;
      mControllerDeviceIndex[stateIndex] = trackedDevice;
      VRControllerState& controllerState = aState.controllerState[stateIndex];
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
            if (j <= 2) {
              ++numButtons;
            } else {
          #ifdef DEBUG
              // SteamVR Knuckles is the only special case for using 2D axis values on triggers.
              ::vr::ETrackedPropertyError err;
              uint32_t requiredBufferLen;
              char charBuf[128];
              requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(trackedDevice,
                                  ::vr::Prop_RenderModelName_String, charBuf, 128, &err);
              MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
              nsCString deviceId(charBuf);
              MOZ_ASSERT(deviceId.Find("knuckles") != kNotFound);
          #endif // #ifdef DEBUG
              numButtons += 2;
            }
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

      nsCString deviceId;
      GetControllerDeviceId(deviceType, trackedDevice, deviceId);

      strncpy(controllerState.controllerName, deviceId.BeginReading(), kVRControllerNameMaxLen);
      controllerState.numButtons = numButtons;
      controllerState.numAxes = numAxes;
      controllerState.numHaptics = kNumOpenVRHaptcs;

      // If the Windows MR controller doesn't has the amount
      // of buttons or axes as our expectation, switching off
      // the workaround for Windows MR.
      if (mIsWindowsMR && (numAxes < 4 || numButtons < 5)) {
        mIsWindowsMR = false;
        NS_WARNING("OpenVR - Switching off Windows MR mode.");
      }
    }
    controllerPresent[stateIndex] = true;
  }
  // Clear out entries for disconnected controllers
  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
    if (!controllerPresent[stateIndex] && mControllerDeviceIndex[stateIndex] != 0) {
      mControllerDeviceIndex[stateIndex] = 0;
      memset(&aState.controllerState[stateIndex], 0, sizeof(VRControllerState));
    }
  }
}

void
OpenVRSession::UpdateControllerButtons(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  // Compared to Edge, we have a wrong implementation for the vertical axis value.
  // In order to not affect the current VR content, we add a workaround for yAxis.
  const float yAxisInvert = (mIsWindowsMR) ? -1.0f : 1.0f;
  const float triggerThreshold = gfxPrefs::VRControllerTriggerThreshold();

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice = mControllerDeviceIndex[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::ETrackedControllerRole role = mVRSystem->
                                          GetControllerRoleForTrackedDeviceIndex(
                                          trackedDevice);
    dom::GamepadHand hand = GetControllerHandFromControllerRole(role);
    controllerState.hand = hand;

    ::vr::VRControllerState_t vrControllerState;
    if (mVRSystem->GetControllerState(trackedDevice, &vrControllerState, sizeof(vrControllerState))) {
      uint32_t axisIdx = 0;
      uint32_t buttonIdx = 0;
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t axisType = mVRSystem->GetInt32TrackedDeviceProperty(
                                   trackedDevice,
                                   static_cast<::vr::TrackedDeviceProperty>(
                                   ::vr::Prop_Axis0Type_Int32 + j));
        switch (axisType) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
          {
            if (mIsWindowsMR) {
              // Adjust the input mapping for Windows MR which has
              // different order.
              axisIdx = (axisIdx == 0) ? 2 : 0;
              buttonIdx = (buttonIdx == 0) ? 4 : 0;
            }

            controllerState.axisValue[axisIdx] = vrControllerState.rAxis[j].x;
            ++axisIdx;
            controllerState.axisValue[axisIdx] = vrControllerState.rAxis[j].y * yAxisInvert;
            ++axisIdx;
            uint64_t buttonMask = ::vr::ButtonMaskFromId(
                                 static_cast<::vr::EVRButtonId>(::vr::k_EButton_Axis0 + j));

            UpdateButton(controllerState, vrControllerState, buttonIdx, buttonMask);
            ++buttonIdx;

            if (mIsWindowsMR) {
              axisIdx = (axisIdx == 4) ? 2 : 4;
              buttonIdx = (buttonIdx == 5) ? 1 : 2;
            }
            break;
          }
          case vr::EVRControllerAxisType::k_eControllerAxis_Trigger:
          {
            if (j <= 2) {
              UpdateTrigger(controllerState, buttonIdx, vrControllerState.rAxis[j].x, triggerThreshold);
              ++buttonIdx;
            } else {
              // For SteamVR Knuckles.
              UpdateTrigger(controllerState, buttonIdx, vrControllerState.rAxis[j].x, triggerThreshold);
              ++buttonIdx;
              UpdateTrigger(controllerState, buttonIdx, vrControllerState.rAxis[j].y, triggerThreshold);
              ++buttonIdx;
            }
            break;
          }
        }
      }
      MOZ_ASSERT(axisIdx ==
                 controller->GetControllerInfo().GetNumAxes());

      const uint64_t supportedButtons = mVRSystem->GetUint64TrackedDeviceProperty(
                                         trackedDevice, ::vr::Prop_SupportedButtons_Uint64);
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_A)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_A));
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_Grip)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_Grip));
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_ApplicationMenu));
        ++buttonIdx;
      }
      if (mIsWindowsMR) {
        // button 4 in Windows MR has already been assigned
        // to k_eControllerAxis_TrackPad.
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_DPad_Left));
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_DPad_Up));
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_DPad_Right));
        ++buttonIdx;
      }
      if (supportedButtons &
          BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx, BTN_MASK_FROM_ID(k_EButton_DPad_Down));
        ++buttonIdx;
      }
    }
  }
}

void
OpenVRSession::UpdateControllerPoses(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  // Compared to Edge, we have a wrong implementation for the vertical axis value.
  // In order to not affect the current VR content, we add a workaround for yAxis.
  const float yAxisInvert = (mIsWindowsMR) ? -1.0f : 1.0f;
  const float triggerThreshold = gfxPrefs::VRControllerTriggerThreshold();

  ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  mVRSystem->GetDeviceToAbsoluteTrackingPose(::vr::TrackingUniverseSeated, 0.0f,
                                             poses, ::vr::k_unMaxTrackedDeviceCount);

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice = mControllerDeviceIndex[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::TrackedDevicePose_t& pose = poses[trackedDevice];

    if (pose.bDeviceIsConnected) {
      controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation |
                               dom::GamepadCapabilityFlags::Cap_Position);
    } else {
      controllerState.flags =  dom::GamepadCapabilityFlags::Cap_None;
    }
    if (pose.bPoseIsValid &&
        pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
      gfx::Matrix4x4 m;

      // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
      // because of its arrangement, we can copy the 12 elements in and
      // then transpose them to the right place.  We do this so we can
      // pull out a Quaternion.
      memcpy(&m.components, &pose.mDeviceToAbsoluteTracking, sizeof(pose.mDeviceToAbsoluteTracking));
      m.Transpose();

      gfx::Quaternion rot;
      rot.SetFromRotationMatrix(m);
      rot.Invert();

      controllerState.pose.orientation[0] = rot.x;
      controllerState.pose.orientation[1] = rot.y;
      controllerState.pose.orientation[2] = rot.z;
      controllerState.pose.orientation[3] = rot.w;
      controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
      controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
      controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
      controllerState.pose.angularAcceleration[0] = 0.0f;
      controllerState.pose.angularAcceleration[1] = 0.0f;
      controllerState.pose.angularAcceleration[2] = 0.0f;
      controllerState.isOrientationValid = true;

      controllerState.pose.position[0] = m._41;
      controllerState.pose.position[1] = m._42;
      controllerState.pose.position[2] = m._43;
      controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
      controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
      controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
      controllerState.pose.linearAcceleration[0] = 0.0f;
      controllerState.pose.linearAcceleration[1] = 0.0f;
      controllerState.pose.linearAcceleration[2] = 0.0f;
      controllerState.isPositionValid = true;
    } else {
      controllerState.isOrientationValid = false;
      controllerState.isPositionValid = false;
    }
  }
}

void
OpenVRSession::GetControllerDeviceId(::vr::ETrackedDeviceClass aDeviceType,
                                     ::vr::TrackedDeviceIndex_t aDeviceIndex,
                                     nsCString& aId)
{
  switch (aDeviceType) {
    case ::vr::TrackedDeviceClass_Controller:
    {
      ::vr::ETrackedPropertyError err;
      uint32_t requiredBufferLen;
      bool isFound = false;
      char charBuf[128];
      requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(aDeviceIndex,
                          ::vr::Prop_RenderModelName_String, charBuf, 128, &err);
      if (requiredBufferLen > 128) {
        MOZ_CRASH("Larger than the buffer size.");
      }
      MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
      nsCString deviceId(charBuf);
      if (deviceId.Find("knuckles") != kNotFound) {
        aId.AssignLiteral("OpenVR Knuckles");
        isFound = true;
      }
      requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(aDeviceIndex,
        ::vr::Prop_SerialNumber_String, charBuf, 128, &err);
      if (requiredBufferLen > 128) {
        MOZ_CRASH("Larger than the buffer size.");
      }
      MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
      deviceId.Assign(charBuf);
      if (deviceId.Find("MRSOURCE") != kNotFound) {
        aId.AssignLiteral("Spatial Controller (Spatial Interaction Source) ");
        mIsWindowsMR = true;
        isFound = true;
      }
      if (!isFound) {
        aId.AssignLiteral("OpenVR Gamepad");
      }
      break;
    }
    case ::vr::TrackedDeviceClass_GenericTracker:
    {
      aId.AssignLiteral("OpenVR Tracker");
      break;
    }
    default:
      MOZ_ASSERT(false);
      break;
  }
}

void
OpenVRSession::StartFrame(mozilla::gfx::VRSystemState& aSystemState)
{
  UpdateHeadsetPose(aSystemState);
  UpdateEyeParameters(aSystemState);
  EnumerateControllers(aSystemState);
  UpdateControllerButtons(aSystemState);
  UpdateControllerPoses(aSystemState);
  aSystemState.sensorState.inputFrameID++;
}

bool
OpenVRSession::ShouldQuit() const
{
  return mShouldQuit;
}

void
OpenVRSession::ProcessEvents(mozilla::gfx::VRSystemState& aSystemState)
{
  bool isHmdPresent = ::vr::VR_IsHmdPresent();
  if (!isHmdPresent) {
    mShouldQuit = true;
  }

  ::vr::VREvent_t event;
  while (mVRSystem && mVRSystem->PollNextEvent(&event, sizeof(event))) {
    switch (event.eventType) {
      case ::vr::VREvent_TrackedDeviceUserInteractionStarted:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsMounted = true;
        }
        break;
      case ::vr::VREvent_TrackedDeviceUserInteractionEnded:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsMounted = false;
        }
        break;
      case ::vr::EVREventType::VREvent_TrackedDeviceActivated:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsConnected = true;
        }
        break;
      case ::vr::EVREventType::VREvent_TrackedDeviceDeactivated:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsConnected = false;
        }
        break;
      case ::vr::EVREventType::VREvent_DriverRequestedQuit:
      case ::vr::EVREventType::VREvent_Quit:
      case ::vr::EVREventType::VREvent_ProcessQuit:
      case ::vr::EVREventType::VREvent_QuitAcknowledged:
      case ::vr::EVREventType::VREvent_QuitAborted_UserPrompt:
        mShouldQuit = true;
        break;
      default:
        // ignore
        break;
    }
  }
}

bool
OpenVRSession::SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer)
{
#if defined(XP_WIN)

  if (aLayer.mTextureType == VRLayerTextureType::LayerTextureType_D3D10SurfaceDescriptor) {
      RefPtr<ID3D11Texture2D> dxTexture;
      HRESULT hr = mDevice->OpenSharedResource((HANDLE)aLayer.mTextureHandle,
        __uuidof(ID3D11Texture2D),
        (void**)(ID3D11Texture2D**)getter_AddRefs(dxTexture));
      if (FAILED(hr) || !dxTexture) {
        NS_WARNING("Failed to open shared texture");
        return false;
      }

      // Similar to LockD3DTexture in TextureD3D11.cpp
      RefPtr<IDXGIKeyedMutex> mutex;
      dxTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
      if (mutex) {
        HRESULT hr = mutex->AcquireSync(0, 1000);
        if (hr == WAIT_TIMEOUT) {
          gfxDevCrash(LogReason::D3DLockTimeout) << "D3D lock mutex timeout";
        }
        else if (hr == WAIT_ABANDONED) {
          gfxCriticalNote << "GFX: D3D11 lock mutex abandoned";
        }
        if (FAILED(hr)) {
          NS_WARNING("Failed to lock the texture");
          return false;
        }
      }
      bool success = SubmitFrame((void *)dxTexture,
                     ::vr::ETextureType::TextureType_DirectX,
                     aLayer.mLeftEyeRect, aLayer.mRightEyeRect);
      if (mutex) {
        HRESULT hr = mutex->ReleaseSync(0);
        if (FAILED(hr)) {
          NS_WARNING("Failed to unlock the texture");
        }
      }
      if (!success) {
        return false;
      }
      return true;
  }

#elif defined(XP_MACOSX)

  if (aLayer.mTextureType == VRLayerTextureType::LayerTextureType_MacIOSurface) {
    return SubmitFrame(aLayer.mTextureHandle,
                       ::vr::ETextureType::TextureType_IOSurface,
                       aLayer.mLeftEyeRect, aLayer.mRightEyeRect);
  }

#endif

  return false;
}

bool
OpenVRSession::SubmitFrame(void* aTextureHandle,
                           ::vr::ETextureType aTextureType,
                           const VRLayerEyeRect& aLeftEyeRect,
                           const VRLayerEyeRect& aRightEyeRect)
{
  ::vr::Texture_t tex;
  tex.handle = aTextureHandle;
  tex.eType = aTextureType;
  tex.eColorSpace = ::vr::EColorSpace::ColorSpace_Auto;

  ::vr::VRTextureBounds_t bounds;
  bounds.uMin = aLeftEyeRect.x;
  bounds.vMin = 1.0 - aLeftEyeRect.y;
  bounds.uMax = aLeftEyeRect.x + aLeftEyeRect.width;
  bounds.vMax = 1.0 - (aLeftEyeRect.y + aLeftEyeRect.height);

  ::vr::EVRCompositorError err;
  err = mVRCompositor->Submit(::vr::EVREye::Eye_Left, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  bounds.uMin = aRightEyeRect.x;
  bounds.vMin = 1.0 - aRightEyeRect.y;
  bounds.uMax = aRightEyeRect.x + aRightEyeRect.width;
  bounds.vMax = 1.0 - (aRightEyeRect.y + aRightEyeRect.height);

  err = mVRCompositor->Submit(::vr::EVREye::Eye_Right, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  mVRCompositor->PostPresentHandoff();
  return true;
}

void
OpenVRSession::StopPresentation()
{
  mVRCompositor->ClearLastSubmittedFrame();

  ::vr::Compositor_CumulativeStats stats;
  mVRCompositor->GetCumulativeStats(&stats, sizeof(::vr::Compositor_CumulativeStats));
  // TODO - Need to send telemetry back to browser.
  // Bug 1473398 will refactor this original gfxVROpenVR code:
  //   const uint32_t droppedFramesPerSec = (stats.m_nNumReprojectedFrames -
  //                                      mTelemetry.mLastDroppedFrameCount) / duration.ToSeconds();
  // Telemetry::Accumulate(Telemetry::WEBVR_DROPPED_FRAMES_IN_OPENVR, droppedFramesPerSec);
}

bool
OpenVRSession::StartPresentation()
{
  return true;
}

} // namespace mozilla
} // namespace gfx
