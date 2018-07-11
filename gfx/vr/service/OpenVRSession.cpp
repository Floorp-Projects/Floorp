#include "OpenVRSession.h"

#if defined(XP_WIN)
#include <d3d11.h>
#include "mozilla/gfx/DeviceManagerDx.h"
#endif // defined(XP_WIN)

#if defined(MOZILLA_INTERNAL_API)
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#endif

#if !defined(M_PI)
#define M_PI 3.14159265358979323846264338327950288
#endif

#define BTN_MASK_FROM_ID(_id) \
  ::vr::ButtonMaskFromId(vr::EVRButtonId::_id)

using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

OpenVRSession::OpenVRSession()
  : VRSession()
  , mVRSystem(nullptr)
  , mVRChaperone(nullptr)
  , mVRCompositor(nullptr)
  , mShouldQuit(false)
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
  if (!::vr::VR_IsHmdPresent()) {
    fprintf(stderr, "No HMD detected, VR_IsHmdPresent returned false.\n");
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
  aSystemState.sensorState.orientation[3] = 1.0f;

  UpdateStageParameters(state);
  UpdateEyeParameters(state);

  VRHMDSensorState& sensorState = aSystemState.sensorState;
  sensorState.flags = (VRDisplayCapabilityFlags)(
    (int)VRDisplayCapabilityFlags::Cap_Orientation |
    (int)VRDisplayCapabilityFlags::Cap_Position);
  sensorState.orientation[3] = 1.0f; // Default to an identity quaternion

  return true;
}

void
OpenVRSession::UpdateStageParameters(VRDisplayState& state)
{
  float sizeX = 0.0f;
  float sizeZ = 0.0f;
  if (mVRChaperone->GetPlayAreaSize(&sizeX, &sizeZ)) {
    ::vr::HmdMatrix34_t t = mVRSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
    state.mStageSize.width = sizeX;
    state.mStageSize.height = sizeZ;

    state.mSittingToStandingTransform[0] = t.m[0][0];
    state.mSittingToStandingTransform[1] = t.m[1][0];
    state.mSittingToStandingTransform[2] = t.m[2][0];
    state.mSittingToStandingTransform[3] = 0.0f;

    state.mSittingToStandingTransform[4] = t.m[0][1];
    state.mSittingToStandingTransform[5] = t.m[1][1];
    state.mSittingToStandingTransform[6] = t.m[2][1];
    state.mSittingToStandingTransform[7] = 0.0f;

    state.mSittingToStandingTransform[8] = t.m[0][2];
    state.mSittingToStandingTransform[9] = t.m[1][2];
    state.mSittingToStandingTransform[10] = t.m[2][2];
    state.mSittingToStandingTransform[11] = 0.0f;

    state.mSittingToStandingTransform[12] = t.m[0][3];
    state.mSittingToStandingTransform[13] = t.m[1][3];
    state.mSittingToStandingTransform[14] = t.m[2][3];
    state.mSittingToStandingTransform[15] = 1.0f;
  } else {
    // If we fail, fall back to reasonable defaults.
    // 1m x 1m space, 0.75m high in seated position

    state.mStageSize.width = 1.0f;
    state.mStageSize.height = 1.0f;

    state.mSittingToStandingTransform[0] = 1.0f;
    state.mSittingToStandingTransform[1] = 0.0f;
    state.mSittingToStandingTransform[2] = 0.0f;
    state.mSittingToStandingTransform[3] = 0.0f;

    state.mSittingToStandingTransform[4] = 0.0f;
    state.mSittingToStandingTransform[5] = 1.0f;
    state.mSittingToStandingTransform[6] = 0.0f;
    state.mSittingToStandingTransform[7] = 0.0f;

    state.mSittingToStandingTransform[8] = 0.0f;
    state.mSittingToStandingTransform[9] = 0.0f;
    state.mSittingToStandingTransform[10] = 1.0f;
    state.mSittingToStandingTransform[11] = 0.0f;

    state.mSittingToStandingTransform[12] = 0.0f;
    state.mSittingToStandingTransform[13] = 0.75f;
    state.mSittingToStandingTransform[14] = 0.0f;
    state.mSittingToStandingTransform[15] = 1.0f;
  }
}

void
OpenVRSession::UpdateEyeParameters(VRDisplayState& state, gfx::Matrix4x4* headToEyeTransforms /* = nullptr */)
{
  for (uint32_t eye = 0; eye < 2; ++eye) {
    ::vr::HmdMatrix34_t eyeToHead = mVRSystem->GetEyeToHeadTransform(static_cast<::vr::Hmd_Eye>(eye));
    state.mEyeTranslation[eye].x = eyeToHead.m[0][3];
    state.mEyeTranslation[eye].y = eyeToHead.m[1][3];
    state.mEyeTranslation[eye].z = eyeToHead.m[2][3];

    float left, right, up, down;
    mVRSystem->GetProjectionRaw(static_cast<::vr::Hmd_Eye>(eye), &left, &right, &up, &down);
    state.mEyeFOV[eye].upDegrees = atan(-up) * 180.0 / M_PI;
    state.mEyeFOV[eye].rightDegrees = atan(right) * 180.0 / M_PI;
    state.mEyeFOV[eye].downDegrees = atan(down) * 180.0 / M_PI;
    state.mEyeFOV[eye].leftDegrees = atan(-left) * 180.0 / M_PI;

    if (headToEyeTransforms) {
      Matrix4x4 pose;
      // NOTE! eyeToHead.m is a 3x4 matrix, not 4x4.  But
      // because of its arrangement, we can copy the 12 elements in and
      // then transpose them to the right place.
      memcpy(&pose._11, &eyeToHead.m, sizeof(eyeToHead.m));
      pose.Transpose();
      pose.Invert();
      headToEyeTransforms[eye] = pose;
    }
  }
}

void
OpenVRSession::GetSensorState(VRSystemState& state)
{
  const uint32_t posesSize = ::vr::k_unTrackedDeviceIndex_Hmd + 1;
  ::vr::TrackedDevicePose_t poses[posesSize];
  // Note: We *must* call WaitGetPoses in order for any rendering to happen at all.
  mVRCompositor->WaitGetPoses(nullptr, 0, poses, posesSize);
  gfx::Matrix4x4 headToEyeTransforms[2];
  UpdateEyeParameters(state.displayState, headToEyeTransforms);

  ::vr::Compositor_FrameTiming timing;
  timing.m_nSize = sizeof(::vr::Compositor_FrameTiming);
  if (mVRCompositor->GetFrameTiming(&timing)) {
    state.sensorState.timestamp = timing.m_flSystemTimeInSeconds;
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

    state.sensorState.flags = (VRDisplayCapabilityFlags)((int)state.sensorState.flags | (int)VRDisplayCapabilityFlags::Cap_Orientation);
    state.sensorState.orientation[0] = rot.x;
    state.sensorState.orientation[1] = rot.y;
    state.sensorState.orientation[2] = rot.z;
    state.sensorState.orientation[3] = rot.w;
    state.sensorState.angularVelocity[0] = pose.vAngularVelocity.v[0];
    state.sensorState.angularVelocity[1] = pose.vAngularVelocity.v[1];
    state.sensorState.angularVelocity[2] = pose.vAngularVelocity.v[2];

    state.sensorState.flags =(VRDisplayCapabilityFlags)((int)state.sensorState.flags | (int)VRDisplayCapabilityFlags::Cap_Position);
    state.sensorState.position[0] = m._41;
    state.sensorState.position[1] = m._42;
    state.sensorState.position[2] = m._43;
    state.sensorState.linearVelocity[0] = pose.vVelocity.v[0];
    state.sensorState.linearVelocity[1] = pose.vVelocity.v[1];
    state.sensorState.linearVelocity[2] = pose.vVelocity.v[2];
  }

  state.sensorState.CalcViewMatrices(headToEyeTransforms);
  state.sensorState.inputFrameID++;
}

void
OpenVRSession::GetControllerState(VRSystemState &state)
{
  // TODO - Implement
}

void
OpenVRSession::StartFrame(mozilla::gfx::VRSystemState& aSystemState)
{
  GetSensorState(aSystemState);
  GetControllerState(aSystemState);
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
