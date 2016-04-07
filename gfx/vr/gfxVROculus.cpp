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
#include "mozilla/TimeStamp.h"

#include "mozilla/gfx/Quaternion.h"

#ifdef XP_WIN
#include "../layers/d3d11/CompositorD3D11.h"
#endif

#include "gfxVROculus.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;

namespace {

#ifdef OVR_CAPI_LIMITED_MOZILLA
static pfn_ovr_Initialize ovr_Initialize = nullptr;
static pfn_ovr_Shutdown ovr_Shutdown = nullptr;
static pfn_ovr_GetLastErrorInfo ovr_GetLastErrorInfo = nullptr;
static pfn_ovr_GetVersionString ovr_GetVersionString = nullptr;
static pfn_ovr_TraceMessage ovr_TraceMessage = nullptr;
static pfn_ovr_GetHmdDesc ovr_GetHmdDesc = nullptr;
static pfn_ovr_GetTrackerCount ovr_GetTrackerCount = nullptr;
static pfn_ovr_GetTrackerDesc ovr_GetTrackerDesc = nullptr;
static pfn_ovr_Create ovr_Create = nullptr;
static pfn_ovr_Destroy ovr_Destroy = nullptr;
static pfn_ovr_GetSessionStatus ovr_GetSessionStatus = nullptr;
static pfn_ovr_SetTrackingOriginType ovr_SetTrackingOriginType = nullptr;
static pfn_ovr_GetTrackingOriginType ovr_GetTrackingOriginType = nullptr;
static pfn_ovr_RecenterTrackingOrigin ovr_RecenterTrackingOrigin = nullptr;
static pfn_ovr_ClearShouldRecenterFlag ovr_ClearShouldRecenterFlag = nullptr;
static pfn_ovr_GetTrackingState ovr_GetTrackingState = nullptr;
static pfn_ovr_GetTrackerPose ovr_GetTrackerPose = nullptr;
static pfn_ovr_GetInputState ovr_GetInputState = nullptr;
static pfn_ovr_GetConnectedControllerTypes ovr_GetConnectedControllerTypes = nullptr;
static pfn_ovr_SetControllerVibration ovr_SetControllerVibration = nullptr;
static pfn_ovr_GetTextureSwapChainLength ovr_GetTextureSwapChainLength = nullptr;
static pfn_ovr_GetTextureSwapChainCurrentIndex ovr_GetTextureSwapChainCurrentIndex = nullptr;
static pfn_ovr_GetTextureSwapChainDesc ovr_GetTextureSwapChainDesc = nullptr;
static pfn_ovr_CommitTextureSwapChain ovr_CommitTextureSwapChain = nullptr;
static pfn_ovr_DestroyTextureSwapChain ovr_DestroyTextureSwapChain = nullptr;
static pfn_ovr_DestroyMirrorTexture ovr_DestroyMirrorTexture = nullptr;
static pfn_ovr_GetFovTextureSize ovr_GetFovTextureSize = nullptr;
static pfn_ovr_GetRenderDesc ovr_GetRenderDesc = nullptr;
static pfn_ovr_SubmitFrame ovr_SubmitFrame = nullptr;
static pfn_ovr_GetPredictedDisplayTime ovr_GetPredictedDisplayTime = nullptr;
static pfn_ovr_GetTimeInSeconds ovr_GetTimeInSeconds = nullptr;
static pfn_ovr_GetBool ovr_GetBool = nullptr;
static pfn_ovr_SetBool ovr_SetBool = nullptr;
static pfn_ovr_GetInt ovr_GetInt = nullptr;
static pfn_ovr_SetInt ovr_SetInt = nullptr;
static pfn_ovr_GetFloat ovr_GetFloat = nullptr;
static pfn_ovr_SetFloat ovr_SetFloat = nullptr;
static pfn_ovr_GetFloatArray ovr_GetFloatArray = nullptr;
static pfn_ovr_SetFloatArray ovr_SetFloatArray = nullptr;
static pfn_ovr_GetString ovr_GetString = nullptr;
static pfn_ovr_SetString ovr_SetString = nullptr;

#ifdef XP_WIN
static pfn_ovr_CreateTextureSwapChainDX ovr_CreateTextureSwapChainDX = nullptr;
static pfn_ovr_GetTextureSwapChainBufferDX ovr_GetTextureSwapChainBufferDX = nullptr;
static pfn_ovr_CreateMirrorTextureDX ovr_CreateMirrorTextureDX = nullptr;
static pfn_ovr_GetMirrorTextureBufferDX ovr_GetMirrorTextureBufferDX = nullptr;
#endif

static pfn_ovr_CreateTextureSwapChainGL ovr_CreateTextureSwapChainGL = nullptr;
static pfn_ovr_GetTextureSwapChainBufferGL ovr_GetTextureSwapChainBufferGL = nullptr;
static pfn_ovr_CreateMirrorTextureGL ovr_CreateMirrorTextureGL = nullptr;
static pfn_ovr_GetMirrorTextureBufferGL ovr_GetMirrorTextureBufferGL = nullptr;

#ifdef HAVE_64BIT_BUILD
#define BUILD_BITS 64
#else
#define BUILD_BITS 32
#endif

#define OVR_PRODUCT_VERSION 1
#define OVR_MAJOR_VERSION   3
#define OVR_MINOR_VERSION   1

static bool
InitializeOculusCAPI()
{
  static PRLibrary *ovrlib = nullptr;

  if (!ovrlib) {
    nsTArray<nsCString> libSearchPaths;
    nsCString libName;
    nsCString searchPath;

#if defined(_WIN32)
    static const char dirSep = '\\';
#else
    static const char dirSep = '/';
#endif

#if defined(_WIN32)
    static const int pathLen = 260;
    searchPath.SetCapacity(pathLen);
    int realLen = ::GetSystemDirectoryA(searchPath.BeginWriting(), pathLen);
    if (realLen != 0 && realLen < pathLen) {
      searchPath.SetLength(realLen);
      libSearchPaths.AppendElement(searchPath);
    }
    libName.AppendPrintf("LibOVRRT%d_%d.dll", BUILD_BITS, OVR_PRODUCT_VERSION);
#elif defined(__APPLE__)
    searchPath.Truncate();
    searchPath.AppendPrintf("/Library/Frameworks/LibOVRRT_%d.framework/Versions/%d", OVR_PRODUCT_VERSION, OVR_MAJOR_VERSION);
    libSearchPaths.AppendElement(searchPath);

    if (PR_GetEnv("HOME")) {
      searchPath.Truncate();
      searchPath.AppendPrintf("%s/Library/Frameworks/LibOVRRT_%d.framework/Versions/%d", PR_GetEnv("HOME"), OVR_PRODUCT_VERSION, OVR_MAJOR_VERSION);
      libSearchPaths.AppendElement(searchPath);
    }
    // The following will match the va_list overload of AppendPrintf if the product version is 0
    // That's bad times.
    //libName.AppendPrintf("LibOVRRT_%d", OVR_PRODUCT_VERSION);
    libName.Append("LibOVRRT_");
    libName.AppendInt(OVR_PRODUCT_VERSION);
#else
    libSearchPaths.AppendElement(nsCString("/usr/local/lib"));
    libSearchPaths.AppendElement(nsCString("/usr/lib"));
    libName.AppendPrintf("libOVRRT%d_%d.so.%d", BUILD_BITS, OVR_PRODUCT_VERSION, OVR_MAJOR_VERSION);
#endif
    
    // If the pref is present, we override libName
    nsAdoptingCString prefLibPath = mozilla::Preferences::GetCString("dom.vr.ovr_lib_path");
    if (prefLibPath && prefLibPath.get()) {
      libSearchPaths.InsertElementsAt(0, 1, prefLibPath);
    }

    nsAdoptingCString prefLibName = mozilla::Preferences::GetCString("dom.vr.ovr_lib_name");
    if (prefLibName && prefLibName.get()) {
      libName.Assign(prefLibName);
    }

    // search the path/module dir
    libSearchPaths.InsertElementsAt(0, 1, nsCString());

    // If the env var is present, we override libName
    if (PR_GetEnv("OVR_LIB_PATH")) {
      searchPath = PR_GetEnv("OVR_LIB_PATH");
      libSearchPaths.InsertElementsAt(0, 1, searchPath);
    }

    if (PR_GetEnv("OVR_LIB_NAME")) {
      libName = PR_GetEnv("OVR_LIB_NAME");
    }

    for (uint32_t i = 0; i < libSearchPaths.Length(); ++i) {
      nsCString& libPath = libSearchPaths[i];
      nsCString fullName;
      if (libPath.Length() == 0) {
        fullName.Assign(libName);
      } else {
        fullName.AppendPrintf("%s%c%s", libPath.BeginReading(), dirSep, libName.BeginReading());
      }

      ovrlib = PR_LoadLibrary(fullName.BeginReading());
      if (ovrlib)
        break;
    }

    if (!ovrlib) {
      return false;
    }
  }

  // was it already initialized?
  if (ovr_Initialize)
    return true;

#define REQUIRE_FUNCTION(_x) do { \
    *(void **)&_x = (void *) PR_FindSymbol(ovrlib, #_x);                \
    if (!_x) { printf_stderr(#_x " symbol missing\n"); goto fail; }       \
  } while (0)

  REQUIRE_FUNCTION(ovr_Initialize);
  REQUIRE_FUNCTION(ovr_Shutdown);
  REQUIRE_FUNCTION(ovr_GetLastErrorInfo);
  REQUIRE_FUNCTION(ovr_GetVersionString);
  REQUIRE_FUNCTION(ovr_TraceMessage);
  REQUIRE_FUNCTION(ovr_GetHmdDesc);
  REQUIRE_FUNCTION(ovr_GetTrackerCount);
  REQUIRE_FUNCTION(ovr_GetTrackerDesc);
  REQUIRE_FUNCTION(ovr_Create);
  REQUIRE_FUNCTION(ovr_Destroy);
  REQUIRE_FUNCTION(ovr_GetSessionStatus);
  REQUIRE_FUNCTION(ovr_SetTrackingOriginType);
  REQUIRE_FUNCTION(ovr_GetTrackingOriginType);
  REQUIRE_FUNCTION(ovr_RecenterTrackingOrigin);
  REQUIRE_FUNCTION(ovr_ClearShouldRecenterFlag);
  REQUIRE_FUNCTION(ovr_GetTrackingState);
  REQUIRE_FUNCTION(ovr_GetTrackerPose);
  REQUIRE_FUNCTION(ovr_GetInputState);
  REQUIRE_FUNCTION(ovr_GetConnectedControllerTypes);
  REQUIRE_FUNCTION(ovr_SetControllerVibration);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainLength);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainCurrentIndex);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainDesc);
  REQUIRE_FUNCTION(ovr_CommitTextureSwapChain);
  REQUIRE_FUNCTION(ovr_DestroyTextureSwapChain);
  REQUIRE_FUNCTION(ovr_DestroyMirrorTexture);
  REQUIRE_FUNCTION(ovr_GetFovTextureSize);
  REQUIRE_FUNCTION(ovr_GetRenderDesc);
  REQUIRE_FUNCTION(ovr_SubmitFrame);
  REQUIRE_FUNCTION(ovr_GetPredictedDisplayTime);
  REQUIRE_FUNCTION(ovr_GetTimeInSeconds);
  REQUIRE_FUNCTION(ovr_GetBool);
  REQUIRE_FUNCTION(ovr_SetBool);
  REQUIRE_FUNCTION(ovr_GetInt);
  REQUIRE_FUNCTION(ovr_SetInt);
  REQUIRE_FUNCTION(ovr_GetFloat);
  REQUIRE_FUNCTION(ovr_SetFloat);
  REQUIRE_FUNCTION(ovr_GetFloatArray);
  REQUIRE_FUNCTION(ovr_SetFloatArray);
  REQUIRE_FUNCTION(ovr_GetString);
  REQUIRE_FUNCTION(ovr_SetString);

#ifdef XP_WIN

  REQUIRE_FUNCTION(ovr_CreateTextureSwapChainDX);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainBufferDX);
  REQUIRE_FUNCTION(ovr_CreateMirrorTextureDX);
  REQUIRE_FUNCTION(ovr_GetMirrorTextureBufferDX);

#endif

  REQUIRE_FUNCTION(ovr_CreateTextureSwapChainGL);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainBufferGL);
  REQUIRE_FUNCTION(ovr_CreateMirrorTextureGL);
  REQUIRE_FUNCTION(ovr_GetMirrorTextureBufferGL);

#undef REQUIRE_FUNCTION

  return true;

 fail:
  ovr_Initialize = nullptr;
  return false;
}

#else
#include <OVR_Version.h>
// we're statically linked; it's available
static bool InitializeOculusCAPI()
{
  return true;
}

#endif

ovrFovPort
ToFovPort(const gfx::VRFieldOfView& aFOV)
{
  ovrFovPort fovPort;
  fovPort.LeftTan = tan(aFOV.leftDegrees * M_PI / 180.0);
  fovPort.RightTan = tan(aFOV.rightDegrees * M_PI / 180.0);
  fovPort.UpTan = tan(aFOV.upDegrees * M_PI / 180.0);
  fovPort.DownTan = tan(aFOV.downDegrees * M_PI / 180.0);
  return fovPort;
}

gfx::VRFieldOfView
FromFovPort(const ovrFovPort& aFOV)
{
  gfx::VRFieldOfView fovInfo;
  fovInfo.leftDegrees = atan(aFOV.LeftTan) * 180.0 / M_PI;
  fovInfo.rightDegrees = atan(aFOV.RightTan) * 180.0 / M_PI;
  fovInfo.upDegrees = atan(aFOV.UpTan) * 180.0 / M_PI;
  fovInfo.downDegrees = atan(aFOV.DownTan) * 180.0 / M_PI;
  return fovInfo;
}

} // namespace

HMDInfoOculus::HMDInfoOculus(ovrSession aSession)
  : VRHMDInfo(VRHMDType::Oculus, false)
  , mSession(aSession)
  , mInputFrameID(0)
{
  MOZ_ASSERT(sizeof(HMDInfoOculus::DistortionVertex) == sizeof(VRDistortionVertex),
             "HMDInfoOculus::DistortionVertex must match the size of VRDistortionVertex");

  MOZ_COUNT_CTOR_INHERITED(HMDInfoOculus, VRHMDInfo);

  mDeviceInfo.mDeviceName.AssignLiteral("Oculus VR HMD");

  mDesc = ovr_GetHmdDesc(aSession);

  mDeviceInfo.mSupportedSensorBits = VRStateValidFlags::State_None;
  if (mDesc.AvailableTrackingCaps & ovrTrackingCap_Orientation) {
    mDeviceInfo.mSupportedSensorBits |= VRStateValidFlags::State_Orientation;
  }
  if (mDesc.AvailableTrackingCaps & ovrTrackingCap_Position) {
    mDeviceInfo.mSupportedSensorBits |= VRStateValidFlags::State_Position;
  }

  mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Left] = FromFovPort(mDesc.DefaultEyeFov[ovrEye_Left]);
  mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Right] = FromFovPort(mDesc.DefaultEyeFov[ovrEye_Right]);

  mDeviceInfo.mMaximumEyeFOV[VRDeviceInfo::Eye_Left] = FromFovPort(mDesc.MaxEyeFov[ovrEye_Left]);
  mDeviceInfo.mMaximumEyeFOV[VRDeviceInfo::Eye_Right] = FromFovPort(mDesc.MaxEyeFov[ovrEye_Right]);

  uint32_t w = mDesc.Resolution.w;
  uint32_t h = mDesc.Resolution.h;
  mDeviceInfo.mScreenRect.x = 0;
  mDeviceInfo.mScreenRect.y = 0;
  mDeviceInfo.mScreenRect.width = std::max(w, h);
  mDeviceInfo.mScreenRect.height = std::min(w, h);
  mDeviceInfo.mIsFakeScreen = true;

  SetFOV(mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Left], mDeviceInfo.mRecommendedEyeFOV[VRDeviceInfo::Eye_Right], 0.01, 10000.0);

  for (int i = 0; i < kMaxLatencyFrames; i++) {
    mLastSensorState[i].Clear();
  }
}

void
HMDInfoOculus::Destroy()
{
  if (mSession) {
    ovr_Destroy(mSession);
    mSession = nullptr;
  }
}

bool
HMDInfoOculus::SetFOV(const gfx::VRFieldOfView& aFOVLeft, const gfx::VRFieldOfView& aFOVRight,
                      double zNear, double zFar)
{
  float pixelsPerDisplayPixel = 1.0;
  ovrSizei texSize[2];

  // get eye parameters and create the mesh
  for (uint32_t eye = 0; eye < VRDeviceInfo::NumEyes; eye++) {
    mDeviceInfo.mEyeFOV[eye] = eye == 0 ? aFOVLeft : aFOVRight;
    mFOVPort[eye] = ToFovPort(mDeviceInfo.mEyeFOV[eye]);

    ovrEyeRenderDesc renderDesc = ovr_GetRenderDesc(mSession, (ovrEyeType)eye, mFOVPort[eye]);

    // As of Oculus 0.6.0, the HmdToEyeOffset values are correct and don't need to be negated.
    mDeviceInfo.mEyeTranslation[eye] = Point3D(renderDesc.HmdToEyeOffset.x, renderDesc.HmdToEyeOffset.y, renderDesc.HmdToEyeOffset.z);

    // note that we are using a right-handed coordinate system here, to match CSS
    mDeviceInfo.mEyeProjectionMatrix[eye] = mDeviceInfo.mEyeFOV[eye].ConstructProjectionMatrix(zNear, zFar, true);

    texSize[eye] = ovr_GetFovTextureSize(mSession, (ovrEyeType)eye, mFOVPort[eye], pixelsPerDisplayPixel);
  }

  // take the max of both for eye resolution
  mDeviceInfo.mEyeResolution.width = std::max(texSize[VRDeviceInfo::Eye_Left].w, texSize[VRDeviceInfo::Eye_Right].w);
  mDeviceInfo.mEyeResolution.height = std::max(texSize[VRDeviceInfo::Eye_Left].h, texSize[VRDeviceInfo::Eye_Right].h);

  mConfiguration.hmdType = mDeviceInfo.mType;
  mConfiguration.value = 0;
  mConfiguration.fov[0] = aFOVLeft;
  mConfiguration.fov[1] = aFOVRight;

  return true;
}

void
HMDInfoOculus::FillDistortionConstants(uint32_t whichEye,
                                       const IntSize& textureSize,
                                       const IntRect& eyeViewport,
                                       const Size& destViewport,
                                       const Rect& destRect,
                                       VRDistortionConstants& values)
{
}

bool
HMDInfoOculus::KeepSensorTracking()
{
  // Oculus PC SDK 0.8 and newer enable tracking by default
  return true;
}

void
HMDInfoOculus::NotifyVsync(const mozilla::TimeStamp& aVsyncTimestamp)
{
  ++mInputFrameID;
}

void
HMDInfoOculus::ZeroSensor()
{
  ovr_RecenterTrackingOrigin(mSession);
}

VRHMDSensorState
HMDInfoOculus::GetSensorState()
{
  VRHMDSensorState result;
  double frameTiming = 0.0f;
  if (gfxPrefs::VRPosePredictionEnabled()) {
    frameTiming = ovr_GetPredictedDisplayTime(mSession, mInputFrameID);
  }
  result = GetSensorState(frameTiming);
  result.inputFrameID = mInputFrameID;
  mLastSensorState[mInputFrameID % kMaxLatencyFrames] = result;
  return result;
}

VRHMDSensorState
HMDInfoOculus::GetImmediateSensorState()
{
  return GetSensorState(0.0);
}

VRHMDSensorState
HMDInfoOculus::GetSensorState(double timeOffset)
{
  VRHMDSensorState result;
  result.Clear();

  ovrTrackingState state = ovr_GetTrackingState(mSession, timeOffset, true);
  ovrPoseStatef& pose(state.HeadPose);

  result.timestamp = pose.TimeInSeconds;

  if (state.StatusFlags & ovrStatus_OrientationTracked) {
    result.flags |= VRStateValidFlags::State_Orientation;

    result.orientation[0] = pose.ThePose.Orientation.x;
    result.orientation[1] = pose.ThePose.Orientation.y;
    result.orientation[2] = pose.ThePose.Orientation.z;
    result.orientation[3] = pose.ThePose.Orientation.w;
    
    result.angularVelocity[0] = pose.AngularVelocity.x;
    result.angularVelocity[1] = pose.AngularVelocity.y;
    result.angularVelocity[2] = pose.AngularVelocity.z;

    result.angularAcceleration[0] = pose.AngularAcceleration.x;
    result.angularAcceleration[1] = pose.AngularAcceleration.y;
    result.angularAcceleration[2] = pose.AngularAcceleration.z;
  }

  if (state.StatusFlags & ovrStatus_PositionTracked) {
    result.flags |= VRStateValidFlags::State_Position;

    result.position[0] = pose.ThePose.Position.x;
    result.position[1] = pose.ThePose.Position.y;
    result.position[2] = pose.ThePose.Position.z;
    
    result.linearVelocity[0] = pose.LinearVelocity.x;
    result.linearVelocity[1] = pose.LinearVelocity.y;
    result.linearVelocity[2] = pose.LinearVelocity.z;

    result.linearAcceleration[0] = pose.LinearAcceleration.x;
    result.linearAcceleration[1] = pose.LinearAcceleration.y;
    result.linearAcceleration[2] = pose.LinearAcceleration.z;
  }
  
  return result;
}

struct RenderTargetSetOculus : public VRHMDRenderingSupport::RenderTargetSet
{
  RenderTargetSetOculus(ovrSession aSession,
                        const IntSize& aSize,
                        HMDInfoOculus *aHMD,
                        ovrTextureSwapChain aTS)
    : hmd(aHMD)
    , textureSet(aTS)
    , session(aSession)
  {
    size = aSize;
  }
  
  already_AddRefed<layers::CompositingRenderTarget> GetNextRenderTarget() override {
    int currentRenderTarget = 0;
    DebugOnly<ovrResult> orv = ovr_GetTextureSwapChainCurrentIndex(session, textureSet, &currentRenderTarget);
    MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainCurrentIndex failed.");

    renderTargets[currentRenderTarget]->ClearOnBind();
    RefPtr<layers::CompositingRenderTarget> rt = renderTargets[currentRenderTarget];
    return rt.forget();
  }

  void Destroy() {
    ovr_DestroyTextureSwapChain(session, textureSet);
    hmd = nullptr;
    textureSet = nullptr;
  }
  
  ~RenderTargetSetOculus() {
    Destroy();
  }

  RefPtr<HMDInfoOculus> hmd;
  ovrTextureSwapChain textureSet;
  ovrSession session;
};

#ifdef XP_WIN
class BasicTextureSourceD3D11 : public layers::TextureSourceD3D11
{
public:
  BasicTextureSourceD3D11(ID3D11Texture2D *aTexture, const IntSize& aSize) {
    mTexture = aTexture;
    mSize = aSize;
  }
};

struct RenderTargetSetD3D11 : public RenderTargetSetOculus
{
  RenderTargetSetD3D11(ovrSession aSession,
                       layers::CompositorD3D11 *aCompositor,
                       const IntSize& aSize,
                       HMDInfoOculus *aHMD,
                       ovrTextureSwapChain aTS)
    : RenderTargetSetOculus(aSession, aSize, aHMD, aTS)
  {
    compositor = aCompositor;
    
    int textureCount = 0;
    DebugOnly<ovrResult> orv = ovr_GetTextureSwapChainLength(session, aTS, &textureCount);
    MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainLength failed.");

    renderTargets.SetLength(textureCount);
    
    for (int i = 0; i < textureCount; ++i) {
      
      RefPtr<layers::CompositingRenderTargetD3D11> rt;

      ID3D11Texture2D* texture = nullptr;
      orv = ovr_GetTextureSwapChainBufferDX(session, aTS, i, IID_PPV_ARGS(&texture));
      MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainBufferDX failed.");
      rt = new layers::CompositingRenderTargetD3D11(texture, IntPoint(0, 0), DXGI_FORMAT_B8G8R8A8_UNORM);
      rt->SetSize(size);
      renderTargets[i] = rt;
      texture->Release();
    }
  }
};
#endif

already_AddRefed<VRHMDRenderingSupport::RenderTargetSet>
HMDInfoOculus::CreateRenderTargetSet(layers::Compositor *aCompositor, const IntSize& aSize)
{
#ifdef XP_WIN
  if (aCompositor->GetBackendType() == layers::LayersBackend::LAYERS_D3D11)
  {
    layers::CompositorD3D11 *comp11 = static_cast<layers::CompositorD3D11*>(aCompositor);

    ovrTextureSwapChainDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.Type = ovrTexture_2D;
    desc.ArraySize = 1;
    desc.Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB;
    desc.Width = aSize.width;
    desc.Height = aSize.height;
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.StaticImage = false;
    desc.MiscFlags = ovrTextureMisc_DX_Typeless;
    desc.BindFlags = ovrTextureBind_DX_RenderTarget;

    ovrTextureSwapChain ts = nullptr;
    
    ovrResult orv = ovr_CreateTextureSwapChainDX(mSession, comp11->GetDevice(), &desc, &ts);
    if (orv != ovrSuccess) {
      return nullptr;
    }

    RefPtr<RenderTargetSetD3D11> rts = new RenderTargetSetD3D11(mSession, comp11, aSize, this, ts);
    return rts.forget();
  }
#endif

  if (aCompositor->GetBackendType() == layers::LayersBackend::LAYERS_OPENGL) {
  }

  return nullptr;
}

void
HMDInfoOculus::DestroyRenderTargetSet(RenderTargetSet *aRTSet)
{
  RenderTargetSetOculus *rts = static_cast<RenderTargetSetOculus*>(aRTSet);
  rts->Destroy();
}

void
HMDInfoOculus::SubmitFrame(RenderTargetSet *aRTSet, int32_t aInputFrameID)
{
  RenderTargetSetOculus *rts = static_cast<RenderTargetSetOculus*>(aRTSet);
  MOZ_ASSERT(rts->hmd != nullptr);
  MOZ_ASSERT(rts->textureSet != nullptr);
  MOZ_ASSERT(aInputFrameID >= 0);
  if (aInputFrameID < 0) {
    // Sanity check to prevent invalid memory access on builds with assertions
    // disabled.
    aInputFrameID = 0;
  }
  ovrResult orv = ovr_CommitTextureSwapChain(mSession, rts->textureSet);
  if (orv != ovrSuccess) {
    printf_stderr("ovr_CommitTextureSwapChain failed.\n");
  }

  VRHMDSensorState sensorState = mLastSensorState[aInputFrameID % kMaxLatencyFrames];
  // It is possible to get a cache miss on mLastSensorState if latency is
  // longer than kMaxLatencyFrames.  An optimization would be to find a frame
  // that is closer than the one selected with the modulus.
  // If we hit this; however, latency is already so high that the site is
  // un-viewable and a more accurate pose prediction is not likely to
  // compensate.
  ovrLayerEyeFov layer;
  memset(&layer, 0, sizeof(layer));
  layer.Header.Type = ovrLayerType_EyeFov;
  layer.Header.Flags = 0;
  layer.ColorTexture[0] = rts->textureSet;
  layer.ColorTexture[1] = nullptr;
  layer.Fov[0] = mFOVPort[0];
  layer.Fov[1] = mFOVPort[1];
  layer.Viewport[0].Pos.x = 0;
  layer.Viewport[0].Pos.y = 0;
  layer.Viewport[0].Size.w = rts->size.width / 2;
  layer.Viewport[0].Size.h = rts->size.height;
  layer.Viewport[1].Pos.x = rts->size.width / 2;
  layer.Viewport[1].Pos.y = 0;
  layer.Viewport[1].Size.w = rts->size.width / 2;
  layer.Viewport[1].Size.h = rts->size.height;

  const Point3D& l = rts->hmd->mDeviceInfo.mEyeTranslation[0];
  const Point3D& r = rts->hmd->mDeviceInfo.mEyeTranslation[1];
  const ovrVector3f hmdToEyeViewOffset[2] = { { l.x, l.y, l.z },
                                              { r.x, r.y, r.z } };

  for (uint32_t i = 0; i < 2; ++i) {
    gfx::Quaternion o(sensorState.orientation[0],
                      sensorState.orientation[1],
                      sensorState.orientation[2],
                      sensorState.orientation[3]);
    Point3D vo(hmdToEyeViewOffset[i].x, hmdToEyeViewOffset[i].y, hmdToEyeViewOffset[i].z);
    Point3D p = o.RotatePoint(vo);

    layer.RenderPose[i].Orientation.x = o.x;
    layer.RenderPose[i].Orientation.y = o.y;
    layer.RenderPose[i].Orientation.z = o.z;
    layer.RenderPose[i].Orientation.w = o.w;
    layer.RenderPose[i].Position.x = p.x + sensorState.position[0];
    layer.RenderPose[i].Position.y = p.y + sensorState.position[1];
    layer.RenderPose[i].Position.z = p.z + sensorState.position[2];
  }

  ovrLayerHeader *layers = &layer.Header;
  orv = ovr_SubmitFrame(mSession, aInputFrameID, nullptr, &layers, 1);
  //printf_stderr("Submitted frame %d, result: %d\n", rts->textureSet->CurrentIndex, orv);
  if (orv != ovrSuccess) {
    printf_stderr("ovr_SubmitFrame failed.\n");
  }
}

/*static*/ already_AddRefed<VRHMDManagerOculus>
VRHMDManagerOculus::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROculusEnabled())
  {
    return nullptr;
  }

  if (!InitializeOculusCAPI()) {
    return nullptr;
  }

  RefPtr<VRHMDManagerOculus> manager = new VRHMDManagerOculus();
  return manager.forget();
}

bool
VRHMDManagerOculus::Init()
{
  if (!mOculusInitialized) {
    nsIThread* thread = nullptr;
    NS_GetCurrentThread(&thread);
    mOculusThread = already_AddRefed<nsIThread>(thread);

    ovrInitParams params;
    memset(&params, 0, sizeof(params));
    params.Flags = ovrInit_RequestVersion;
    params.RequestedMinorVersion = OVR_MINOR_VERSION;
    params.LogCallback = nullptr;
    params.ConnectionTimeoutMS = 0;

    ovrResult orv = ovr_Initialize(&params);

    if (orv == ovrSuccess) {
      mOculusInitialized = true;
    }
  }

  return mOculusInitialized;
}

void
VRHMDManagerOculus::Destroy()
{
  if(mOculusInitialized) {
    MOZ_ASSERT(NS_GetCurrentThread() == mOculusThread);
    mOculusThread = nullptr;

    mHMDInfo = nullptr;

    ovr_Shutdown();
    mOculusInitialized = false;
  }
}

void
VRHMDManagerOculus::GetHMDs(nsTArray<RefPtr<VRHMDInfo>>& aHMDResult)
{
  if (!mOculusInitialized) {
    return;
  }

  // ovr_Create can be slow when no HMD is present and we wish
  // to keep the same oculus session when possible, so we detect
  // presence of an HMD with ovr_GetHmdDesc before calling ovr_Create
  ovrHmdDesc desc = ovr_GetHmdDesc(NULL);
  if (desc.Type == ovrHmd_None) {
    // No HMD connected.
    mHMDInfo = nullptr;
  } else if (mHMDInfo == nullptr) {
    // HMD Detected
    ovrSession session;
    ovrGraphicsLuid luid;
    ovrResult orv = ovr_Create(&session, &luid);
    if (orv == ovrSuccess) {
      mHMDInfo = new HMDInfoOculus(session);
    }
  }

  if (mHMDInfo) {
    aHMDResult.AppendElement(mHMDInfo);
  }
}
