/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#error "Oculus 1.3 runtime support only available for Windows"
#endif

#include <math.h>


#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/DeviceManagerD3D11.h"
#include "ipc/VRLayerParent.h"

#include "mozilla/gfx/Quaternion.h"

#include <d3d11.h>
#include "../layers/d3d11/CompositorD3D11.h"
#include "mozilla/layers/TextureD3D11.h"

#include "gfxVROculus.h"

/** XXX The DX11 objects and quad blitting could be encapsulated
 *    into a separate object if either Oculus starts supporting
 *     non-Windows platforms or the blit is needed by other HMD\
 *     drivers.
 *     Alternately, we could remove the extra blit for
 *     Oculus as well with some more refactoring.
 */

// See CompositorD3D11Shaders.h
struct ShaderBytes { const void* mData; size_t mLength; };
extern ShaderBytes sRGBShader;
extern ShaderBytes sLayerQuadVS;
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;

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
ToFovPort(const VRFieldOfView& aFOV)
{
  ovrFovPort fovPort;
  fovPort.LeftTan = tan(aFOV.leftDegrees * M_PI / 180.0);
  fovPort.RightTan = tan(aFOV.rightDegrees * M_PI / 180.0);
  fovPort.UpTan = tan(aFOV.upDegrees * M_PI / 180.0);
  fovPort.DownTan = tan(aFOV.downDegrees * M_PI / 180.0);
  return fovPort;
}

VRFieldOfView
FromFovPort(const ovrFovPort& aFOV)
{
  VRFieldOfView fovInfo;
  fovInfo.leftDegrees = atan(aFOV.LeftTan) * 180.0 / M_PI;
  fovInfo.rightDegrees = atan(aFOV.RightTan) * 180.0 / M_PI;
  fovInfo.upDegrees = atan(aFOV.UpTan) * 180.0 / M_PI;
  fovInfo.downDegrees = atan(aFOV.DownTan) * 180.0 / M_PI;
  return fovInfo;
}

} // namespace

VRDisplayOculus::VRDisplayOculus(ovrSession aSession)
  : VRDisplayHost(VRDisplayType::Oculus)
  , mSession(aSession)
  , mTextureSet(nullptr)
  , mQuadVS(nullptr)
  , mQuadPS(nullptr)
  , mVSConstantBuffer(nullptr)
  , mPSConstantBuffer(nullptr)
  , mVertexBuffer(nullptr)
  , mInputLayout(nullptr)
  , mLinearSamplerState(nullptr)
  , mIsPresenting(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayOculus, VRDisplayHost);

  mDisplayInfo.mDisplayName.AssignLiteral("Oculus VR HMD");
  mDisplayInfo.mIsConnected = true;

  mDesc = ovr_GetHmdDesc(aSession);

  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None;
  if (mDesc.AvailableTrackingCaps & ovrTrackingCap_Orientation) {
    mDisplayInfo.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_Orientation;
  }
  if (mDesc.AvailableTrackingCaps & ovrTrackingCap_Position) {
    mDisplayInfo.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_Position;
  }
  mDisplayInfo.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_External;
  mDisplayInfo.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_Present;

  mFOVPort[VRDisplayInfo::Eye_Left] = mDesc.DefaultEyeFov[ovrEye_Left];
  mFOVPort[VRDisplayInfo::Eye_Right] = mDesc.DefaultEyeFov[ovrEye_Right];

  mDisplayInfo.mEyeFOV[VRDisplayInfo::Eye_Left] = FromFovPort(mFOVPort[VRDisplayInfo::Eye_Left]);
  mDisplayInfo.mEyeFOV[VRDisplayInfo::Eye_Right] = FromFovPort(mFOVPort[VRDisplayInfo::Eye_Right]);

  uint32_t w = mDesc.Resolution.w;
  uint32_t h = mDesc.Resolution.h;

  float pixelsPerDisplayPixel = 1.0;
  ovrSizei texSize[2];

  // get eye parameters and create the mesh
  for (uint32_t eye = 0; eye < VRDisplayInfo::NumEyes; eye++) {

    ovrEyeRenderDesc renderDesc = ovr_GetRenderDesc(mSession, (ovrEyeType)eye, mFOVPort[eye]);

    // As of Oculus 0.6.0, the HmdToEyeOffset values are correct and don't need to be negated.
    mDisplayInfo.mEyeTranslation[eye] = Point3D(renderDesc.HmdToEyeOffset.x, renderDesc.HmdToEyeOffset.y, renderDesc.HmdToEyeOffset.z);

    texSize[eye] = ovr_GetFovTextureSize(mSession, (ovrEyeType)eye, mFOVPort[eye], pixelsPerDisplayPixel);
  }

  // take the max of both for eye resolution
  mDisplayInfo.mEyeResolution.width = std::max(texSize[VRDisplayInfo::Eye_Left].w, texSize[VRDisplayInfo::Eye_Right].w);
  mDisplayInfo.mEyeResolution.height = std::max(texSize[VRDisplayInfo::Eye_Left].h, texSize[VRDisplayInfo::Eye_Right].h);
}

VRDisplayOculus::~VRDisplayOculus() {
  StopPresentation();
  Destroy();
  MOZ_COUNT_DTOR_INHERITED(VRDisplayOculus, VRDisplayHost);
}

void
VRDisplayOculus::Destroy()
{
  if (mSession) {
    ovr_Destroy(mSession);
    mSession = nullptr;
  }
}

void
VRDisplayOculus::ZeroSensor()
{
  ovr_RecenterTrackingOrigin(mSession);
}

VRHMDSensorState
VRDisplayOculus::GetSensorState()
{
  mInputFrameID++;

  VRHMDSensorState result;
  double frameDelta = 0.0f;
  if (gfxPrefs::VRPosePredictionEnabled()) {
    // XXX We might need to call ovr_GetPredictedDisplayTime even if we don't use the result.
    // If we don't call it, the Oculus driver will spew out many warnings...
    double predictedFrameTime = ovr_GetPredictedDisplayTime(mSession, mInputFrameID);
    frameDelta = predictedFrameTime - ovr_GetTimeInSeconds();
  }
  result = GetSensorState(frameDelta);
  result.inputFrameID = mInputFrameID;
  mLastSensorState[result.inputFrameID % kMaxLatencyFrames] = result;
  return result;
}

VRHMDSensorState
VRDisplayOculus::GetImmediateSensorState()
{
  return GetSensorState(0.0);
}

VRHMDSensorState
VRDisplayOculus::GetSensorState(double timeOffset)
{
  VRHMDSensorState result;
  result.Clear();

  ovrTrackingState state = ovr_GetTrackingState(mSession, timeOffset, true);
  ovrPoseStatef& pose(state.HeadPose);

  result.timestamp = pose.TimeInSeconds;

  if (state.StatusFlags & ovrStatus_OrientationTracked) {
    result.flags |= VRDisplayCapabilityFlags::Cap_Orientation;

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
    result.flags |= VRDisplayCapabilityFlags::Cap_Position;

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
  result.flags |= VRDisplayCapabilityFlags::Cap_External;
  result.flags |= VRDisplayCapabilityFlags::Cap_Present;

  return result;
}

void
VRDisplayOculus::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }
  mIsPresenting = true;

  /**
   * The presentation format is determined by content, which describes the
   * left and right eye rectangles in the VRLayer.  The default, if no
   * coordinates are passed is to place the left and right eye textures
   * side-by-side within the buffer.
   *
   * XXX - An optimization would be to dynamically resize this buffer
   *       to accomodate sites that are choosing to render in a lower
   *       resolution or are using space outside of the left and right
   *       eye textures for other purposes.  (Bug 1291443)
   */
  ovrTextureSwapChainDesc desc;
  memset(&desc, 0, sizeof(desc));
  desc.Type = ovrTexture_2D;
  desc.ArraySize = 1;
  desc.Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB;
  desc.Width = mDisplayInfo.mEyeResolution.width * 2;
  desc.Height = mDisplayInfo.mEyeResolution.height;
  desc.MipLevels = 1;
  desc.SampleCount = 1;
  desc.StaticImage = false;
  desc.MiscFlags = ovrTextureMisc_DX_Typeless;
  desc.BindFlags = ovrTextureBind_DX_RenderTarget;

  if (!mDevice) {
    mDevice = gfx::DeviceManagerD3D11::Get()->GetCompositorDevice();
    if (!mDevice) {
      NS_WARNING("Failed to get a D3D11Device for Oculus");
      return;
    }
  }

  mDevice->GetImmediateContext(getter_AddRefs(mContext));
  if (!mContext) {
    NS_WARNING("Failed to get immediate context for Oculus");
    return;
  }

  if (FAILED(mDevice->CreateVertexShader(sLayerQuadVS.mData, sLayerQuadVS.mLength, nullptr, &mQuadVS))) {
    NS_WARNING("Failed to create vertex shader for Oculus");
    return;
  }

  if (FAILED(mDevice->CreatePixelShader(sRGBShader.mData, sRGBShader.mLength, nullptr, &mQuadPS))) {
    NS_WARNING("Failed to create pixel shader for Oculus");
    return;
  }

  CD3D11_BUFFER_DESC cBufferDesc(sizeof(layers::VertexShaderConstants),
    D3D11_BIND_CONSTANT_BUFFER,
    D3D11_USAGE_DYNAMIC,
    D3D11_CPU_ACCESS_WRITE);

  if (FAILED(mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mVSConstantBuffer)))) {
    NS_WARNING("Failed to vertex shader constant buffer for Oculus");
    return;
  }

  cBufferDesc.ByteWidth = sizeof(layers::PixelShaderConstants);
  if (FAILED(mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mPSConstantBuffer)))) {
    NS_WARNING("Failed to pixel shader constant buffer for Oculus");
    return;
  }

  CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
  if (FAILED(mDevice->CreateSamplerState(&samplerDesc, getter_AddRefs(mLinearSamplerState)))) {
    NS_WARNING("Failed to create sampler state for Oculus");
    return;
  }

  D3D11_INPUT_ELEMENT_DESC layout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  if (FAILED(mDevice->CreateInputLayout(layout,
                                            sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC),
                                            sLayerQuadVS.mData,
                                            sLayerQuadVS.mLength,
                                            getter_AddRefs(mInputLayout)))) {
    NS_WARNING("Failed to create input layout for Oculus");
    return;
  }

  ovrResult orv = ovr_CreateTextureSwapChainDX(mSession, mDevice, &desc, &mTextureSet);
  if (orv != ovrSuccess) {
    NS_WARNING("ovr_CreateTextureSwapChainDX failed");
    return;
  }

  int textureCount = 0;
  orv = ovr_GetTextureSwapChainLength(mSession, mTextureSet, &textureCount);
  if (orv != ovrSuccess) {
    NS_WARNING("ovr_GetTextureSwapChainLength failed");
    return;
  }

  Vertex vertices[] = { { { 0.0, 0.0 } },{ { 1.0, 0.0 } },{ { 0.0, 1.0 } },{ { 1.0, 1.0 } } };
  CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = (void*)vertices;

  if (FAILED(mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mVertexBuffer)))) {
    NS_WARNING("Failed to create vertex buffer for Oculus");
    return;
  }

  mRenderTargets.SetLength(textureCount);

  memset(&mVSConstants, 0, sizeof(mVSConstants));
  memset(&mPSConstants, 0, sizeof(mPSConstants));

  for (int i = 0; i < textureCount; ++i) {
    RefPtr<CompositingRenderTargetD3D11> rt;
    ID3D11Texture2D* texture = nullptr;
    orv = ovr_GetTextureSwapChainBufferDX(mSession, mTextureSet, i, IID_PPV_ARGS(&texture));
    MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainBufferDX failed.");
    rt = new CompositingRenderTargetD3D11(texture, IntPoint(0, 0), DXGI_FORMAT_B8G8R8A8_UNORM);
    rt->SetSize(IntSize(mDisplayInfo.mEyeResolution.width * 2, mDisplayInfo.mEyeResolution.height));
    mRenderTargets[i] = rt;
    texture->Release();
  }
}

void
VRDisplayOculus::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }
  mIsPresenting = false;

  ovr_SubmitFrame(mSession, 0, nullptr, nullptr, 0);

  if (mTextureSet) {
    ovr_DestroyTextureSwapChain(mSession, mTextureSet);
    mTextureSet = nullptr;
  }
}

/*static*/ already_AddRefed<VRDisplayManagerOculus>
VRDisplayManagerOculus::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROculusEnabled())
  {
    return nullptr;
  }

  if (!InitializeOculusCAPI()) {
    return nullptr;
  }

  RefPtr<VRDisplayManagerOculus> manager = new VRDisplayManagerOculus();
  return manager.forget();
}

bool
VRDisplayManagerOculus::Init()
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
VRDisplayManagerOculus::Destroy()
{
  if (mOculusInitialized) {
    MOZ_ASSERT(NS_GetCurrentThread() == mOculusThread);
    mOculusThread = nullptr;

    mHMDInfo = nullptr;

    ovr_Shutdown();
    mOculusInitialized = false;
  }
}

void
VRDisplayManagerOculus::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
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
      mHMDInfo = new VRDisplayOculus(session);
    }
  }

  if (mHMDInfo) {
    aHMDResult.AppendElement(mHMDInfo);
  }
}

already_AddRefed<CompositingRenderTargetD3D11>
VRDisplayOculus::GetNextRenderTarget()
{
  int currentRenderTarget = 0;
  DebugOnly<ovrResult> orv = ovr_GetTextureSwapChainCurrentIndex(mSession, mTextureSet, &currentRenderTarget);
  MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainCurrentIndex failed.");

  mRenderTargets[currentRenderTarget]->ClearOnBind();
  RefPtr<CompositingRenderTargetD3D11> rt = mRenderTargets[currentRenderTarget];
  return rt.forget();
}

bool
VRDisplayOculus::UpdateConstantBuffers()
{
  HRESULT hr;
  D3D11_MAPPED_SUBRESOURCE resource;
  resource.pData = nullptr;

  hr = mContext->Map(mVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    return false;
  }
  *(VertexShaderConstants*)resource.pData = mVSConstants;
  mContext->Unmap(mVSConstantBuffer, 0);
  resource.pData = nullptr;

  hr = mContext->Map(mPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    return false;
  }
  *(PixelShaderConstants*)resource.pData = mPSConstants;
  mContext->Unmap(mPSConstantBuffer, 0);

  ID3D11Buffer *buffer = mVSConstantBuffer;
  mContext->VSSetConstantBuffers(0, 1, &buffer);
  buffer = mPSConstantBuffer;
  mContext->PSSetConstantBuffers(0, 1, &buffer);
  return true;
}

void
VRDisplayOculus::SubmitFrame(TextureSourceD3D11* aSource,
  const IntSize& aSize,
  const VRHMDSensorState& aSensorState,
  const gfx::Rect& aLeftEyeRect,
  const gfx::Rect& aRightEyeRect)
{
  if (!mIsPresenting) {
    return;
  }
  MOZ_ASSERT(mDevice);
  MOZ_ASSERT(mContext);

  RefPtr<CompositingRenderTargetD3D11> surface = GetNextRenderTarget();

  surface->BindRenderTarget(mContext);

  Matrix viewMatrix = Matrix::Translation(-1.0, 1.0);
  viewMatrix.PreScale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  viewMatrix.PreScale(1.0f, -1.0f);
  Matrix4x4 projection = Matrix4x4::From2D(viewMatrix);
  projection._33 = 0.0f;

  Matrix transform2d;
  gfx::Matrix4x4 transform = gfx::Matrix4x4::From2D(transform2d);

  D3D11_VIEWPORT viewport;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  viewport.Width = aSize.width;
  viewport.Height = aSize.height;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  D3D11_RECT scissor;
  scissor.left = 0;
  scissor.right = aSize.width;
  scissor.top = 0;
  scissor.bottom = aSize.height;

  memcpy(&mVSConstants.layerTransform, &transform._11, sizeof(mVSConstants.layerTransform));
  memcpy(&mVSConstants.projection, &projection._11, sizeof(mVSConstants.projection));
  mVSConstants.renderTargetOffset[0] = 0.0f;
  mVSConstants.renderTargetOffset[1] = 0.0f;
  mVSConstants.layerQuad = Rect(0.0f, 0.0f, aSize.width, aSize.height);
  mVSConstants.textureCoords = Rect(0.0f, 1.0f, 1.0f, -1.0f);

  mPSConstants.layerOpacity[0] = 1.0f;

  ID3D11Buffer* vbuffer = mVertexBuffer;
  UINT vsize = sizeof(Vertex);
  UINT voffset = 0;
  mContext->IASetVertexBuffers(0, 1, &vbuffer, &vsize, &voffset);
  mContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
  mContext->IASetInputLayout(mInputLayout);
  mContext->RSSetViewports(1, &viewport);
  mContext->RSSetScissorRects(1, &scissor);
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->VSSetShader(mQuadVS, nullptr, 0);
  mContext->PSSetShader(mQuadPS, nullptr, 0);
  ID3D11ShaderResourceView* srView = aSource->GetShaderResourceView();
  mContext->PSSetShaderResources(0 /* 0 == TexSlot::RGB */, 1, &srView);
  // XXX Use Constant from TexSlot in CompositorD3D11.cpp?

  ID3D11SamplerState *sampler = mLinearSamplerState;
  mContext->PSSetSamplers(0, 1, &sampler);

  if (!UpdateConstantBuffers()) {
    NS_WARNING("Failed to update constant buffers for Oculus");
    return;
  }

  mContext->Draw(4, 0);

  ovrResult orv = ovr_CommitTextureSwapChain(mSession, mTextureSet);
  if (orv != ovrSuccess) {
    NS_WARNING("ovr_CommitTextureSwapChain failed.\n");
    return;
  }

  ovrLayerEyeFov layer;
  memset(&layer, 0, sizeof(layer));
  layer.Header.Type = ovrLayerType_EyeFov;
  layer.Header.Flags = 0;
  layer.ColorTexture[0] = mTextureSet;
  layer.ColorTexture[1] = nullptr;
  layer.Fov[0] = mFOVPort[0];
  layer.Fov[1] = mFOVPort[1];
  layer.Viewport[0].Pos.x = aSize.width * aLeftEyeRect.x;
  layer.Viewport[0].Pos.y = aSize.height * aLeftEyeRect.y;
  layer.Viewport[0].Size.w = aSize.width * aLeftEyeRect.width;
  layer.Viewport[0].Size.h = aSize.height * aLeftEyeRect.height;
  layer.Viewport[1].Pos.x = aSize.width * aRightEyeRect.x;
  layer.Viewport[1].Pos.y = aSize.height * aRightEyeRect.y;
  layer.Viewport[1].Size.w = aSize.width * aRightEyeRect.width;
  layer.Viewport[1].Size.h = aSize.height * aRightEyeRect.height;

  const Point3D& l = mDisplayInfo.mEyeTranslation[0];
  const Point3D& r = mDisplayInfo.mEyeTranslation[1];
  const ovrVector3f hmdToEyeViewOffset[2] = { { l.x, l.y, l.z },
                                              { r.x, r.y, r.z } };

  for (uint32_t i = 0; i < 2; ++i) {
    Quaternion o(aSensorState.orientation[0],
      aSensorState.orientation[1],
      aSensorState.orientation[2],
      aSensorState.orientation[3]);
    Point3D vo(hmdToEyeViewOffset[i].x, hmdToEyeViewOffset[i].y, hmdToEyeViewOffset[i].z);
    Point3D p = o.RotatePoint(vo);
    layer.RenderPose[i].Orientation.x = o.x;
    layer.RenderPose[i].Orientation.y = o.y;
    layer.RenderPose[i].Orientation.z = o.z;
    layer.RenderPose[i].Orientation.w = o.w;
    layer.RenderPose[i].Position.x = p.x + aSensorState.position[0];
    layer.RenderPose[i].Position.y = p.y + aSensorState.position[1];
    layer.RenderPose[i].Position.z = p.z + aSensorState.position[2];
  }

  ovrLayerHeader *layers = &layer.Header;
  orv = ovr_SubmitFrame(mSession, aSensorState.inputFrameID, nullptr, &layers, 1);

  if (orv != ovrSuccess) {
    printf_stderr("ovr_SubmitFrame failed.\n");
  }

  // Trigger the next VSync immediately
  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
}

void
VRDisplayOculus::NotifyVSync()
{
  ovrSessionStatus sessionStatus;
  ovrResult ovr = ovr_GetSessionStatus(mSession, &sessionStatus);
  mDisplayInfo.mIsConnected = (ovr == ovrSuccess && sessionStatus.HmdPresent);
}
