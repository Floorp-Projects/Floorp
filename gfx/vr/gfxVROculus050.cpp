/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/Preferences.h"

#include "gfxVROculus050.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace ovr050;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;

namespace {

#ifdef OVR_CAPI_050_LIMITED_MOZILLA
static pfn_ovr_Initialize ovr_Initialize = nullptr;
static pfn_ovr_Shutdown ovr_Shutdown = nullptr;
static pfn_ovrHmd_Detect ovrHmd_Detect = nullptr;
static pfn_ovrHmd_Create ovrHmd_Create = nullptr;
static pfn_ovrHmd_Destroy ovrHmd_Destroy = nullptr;
static pfn_ovrHmd_CreateDebug ovrHmd_CreateDebug = nullptr;
static pfn_ovrHmd_GetLastError ovrHmd_GetLastError = nullptr;
static pfn_ovrHmd_AttachToWindow ovrHmd_AttachToWindow = nullptr;
static pfn_ovrHmd_GetEnabledCaps ovrHmd_GetEnabledCaps = nullptr;
static pfn_ovrHmd_SetEnabledCaps ovrHmd_SetEnabledCaps = nullptr;
static pfn_ovrHmd_ConfigureTracking ovrHmd_ConfigureTracking = nullptr;
static pfn_ovrHmd_RecenterPose ovrHmd_RecenterPose = nullptr;
static pfn_ovrHmd_GetTrackingState ovrHmd_GetTrackingState = nullptr;
static pfn_ovrHmd_GetFovTextureSize ovrHmd_GetFovTextureSize = nullptr;
static pfn_ovrHmd_GetRenderDesc ovrHmd_GetRenderDesc = nullptr;
static pfn_ovrHmd_CreateDistortionMesh ovrHmd_CreateDistortionMesh = nullptr;
static pfn_ovrHmd_DestroyDistortionMesh ovrHmd_DestroyDistortionMesh = nullptr;
static pfn_ovrHmd_GetRenderScaleAndOffset ovrHmd_GetRenderScaleAndOffset = nullptr;
static pfn_ovrHmd_GetFrameTiming ovrHmd_GetFrameTiming = nullptr;
static pfn_ovrHmd_BeginFrameTiming ovrHmd_BeginFrameTiming = nullptr;
static pfn_ovrHmd_EndFrameTiming ovrHmd_EndFrameTiming = nullptr;
static pfn_ovrHmd_ResetFrameTiming ovrHmd_ResetFrameTiming = nullptr;
static pfn_ovrHmd_GetEyePoses ovrHmd_GetEyePoses = nullptr;
static pfn_ovrHmd_GetHmdPosePerEye ovrHmd_GetHmdPosePerEye = nullptr;
static pfn_ovrHmd_GetEyeTimewarpMatrices ovrHmd_GetEyeTimewarpMatrices = nullptr;
static pfn_ovrMatrix4f_Projection ovrMatrix4f_Projection = nullptr;
static pfn_ovrMatrix4f_OrthoSubProjection ovrMatrix4f_OrthoSubProjection = nullptr;
static pfn_ovr_GetTimeInSeconds ovr_GetTimeInSeconds = nullptr;

#ifdef HAVE_64BIT_BUILD
#define BUILD_BITS 64
#else
#define BUILD_BITS 32
#endif

#define LIBOVR_PRODUCT_VERSION 0
#define LIBOVR_MAJOR_VERSION   5
#define LIBOVR_MINOR_VERSION   0

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
    libName.AppendPrintf("LibOVRRT%d_%d_%d.dll", BUILD_BITS, LIBOVR_PRODUCT_VERSION, LIBOVR_MAJOR_VERSION);
#elif defined(__APPLE__)
    searchPath.Truncate();
    searchPath.AppendPrintf("/Library/Frameworks/LibOVRRT_%d.framework/Versions/%d", LIBOVR_PRODUCT_VERSION, LIBOVR_MAJOR_VERSION);
    libSearchPaths.AppendElement(searchPath);

    if (PR_GetEnv("HOME")) {
      searchPath.Truncate();
      searchPath.AppendPrintf("%s/Library/Frameworks/LibOVRRT_%d.framework/Versions/%d", PR_GetEnv("HOME"), LIBOVR_PRODUCT_VERSION, LIBOVR_MAJOR_VERSION);
      libSearchPaths.AppendElement(searchPath);
    }
    // The following will match the va_list overload of AppendPrintf if the product version is 0
    // That's bad times.
    //libName.AppendPrintf("LibOVRRT_%d", LIBOVR_PRODUCT_VERSION);
    libName.Append("LibOVRRT_");
    libName.AppendInt(LIBOVR_PRODUCT_VERSION);
#else
    libSearchPaths.AppendElement(nsCString("/usr/local/lib"));
    libSearchPaths.AppendElement(nsCString("/usr/lib"));
    libName.AppendPrintf("libOVRRT%d_%d.so.%d", BUILD_BITS, LIBOVR_PRODUCT_VERSION, LIBOVR_MAJOR_VERSION);
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
      printf_stderr("Failed to load Oculus VR library!\n");
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
  REQUIRE_FUNCTION(ovrHmd_Detect);
  REQUIRE_FUNCTION(ovrHmd_Create);
  REQUIRE_FUNCTION(ovrHmd_Destroy);
  REQUIRE_FUNCTION(ovrHmd_CreateDebug);
  REQUIRE_FUNCTION(ovrHmd_GetLastError);
  REQUIRE_FUNCTION(ovrHmd_AttachToWindow);
  REQUIRE_FUNCTION(ovrHmd_GetEnabledCaps);
  REQUIRE_FUNCTION(ovrHmd_SetEnabledCaps);
  REQUIRE_FUNCTION(ovrHmd_ConfigureTracking);
  REQUIRE_FUNCTION(ovrHmd_RecenterPose);
  REQUIRE_FUNCTION(ovrHmd_GetTrackingState);

  REQUIRE_FUNCTION(ovrHmd_GetFovTextureSize);
  REQUIRE_FUNCTION(ovrHmd_GetRenderDesc);
  REQUIRE_FUNCTION(ovrHmd_CreateDistortionMesh);
  REQUIRE_FUNCTION(ovrHmd_DestroyDistortionMesh);
  REQUIRE_FUNCTION(ovrHmd_GetRenderScaleAndOffset);
  REQUIRE_FUNCTION(ovrHmd_GetFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_BeginFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_EndFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_ResetFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_GetEyePoses);
  REQUIRE_FUNCTION(ovrHmd_GetHmdPosePerEye);
  REQUIRE_FUNCTION(ovrHmd_GetEyeTimewarpMatrices);
  REQUIRE_FUNCTION(ovrMatrix4f_Projection);
  REQUIRE_FUNCTION(ovrMatrix4f_OrthoSubProjection);
  REQUIRE_FUNCTION(ovr_GetTimeInSeconds);

#undef REQUIRE_FUNCTION

  return true;

 fail:
  ovr_Initialize = nullptr;
  return false;
}

#else
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

} // anonymous namespace

HMDInfoOculus050::HMDInfoOculus050(ovrHmd aHMD)
  : VRHMDInfo(VRHMDType::Oculus050)
  , mHMD(aHMD)
  , mStartCount(0)
{
  MOZ_ASSERT(sizeof(HMDInfoOculus050::DistortionVertex) == sizeof(VRDistortionVertex),
             "HMDInfoOculus050::DistortionVertex must match the size of VRDistortionVertex");

  MOZ_COUNT_CTOR_INHERITED(HMDInfoOculus050, VRHMDInfo);

  mDeviceName.AssignLiteral("Oculus VR HMD (0.5.0)");

  mSupportedSensorBits = 0;
  if (mHMD->TrackingCaps & ovrTrackingCap_Orientation)
    mSupportedSensorBits |= State_Orientation;
  if (mHMD->TrackingCaps & ovrTrackingCap_Position)
    mSupportedSensorBits |= State_Position;

  mRecommendedEyeFOV[Eye_Left] = FromFovPort(mHMD->DefaultEyeFov[ovrEye_Left]);
  mRecommendedEyeFOV[Eye_Right] = FromFovPort(mHMD->DefaultEyeFov[ovrEye_Right]);

  mMaximumEyeFOV[Eye_Left] = FromFovPort(mHMD->MaxEyeFov[ovrEye_Left]);
  mMaximumEyeFOV[Eye_Right] = FromFovPort(mHMD->MaxEyeFov[ovrEye_Right]);

  SetFOV(mRecommendedEyeFOV[Eye_Left], mRecommendedEyeFOV[Eye_Right], 0.01, 10000.0);

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
#if 1
    if (getenv("FAKE_OCULUS_SCREEN")) {
      const char *env = getenv("FAKE_OCULUS_SCREEN");
      nsresult err;
      int32_t xcoord = nsCString(env).ToInteger(&err);
      if (err != NS_OK) xcoord = 0;
      mScreen = VRHMDManager::MakeFakeScreen(xcoord, 0, 1920, 1080);
    } else
#endif
    {
      screenmgr->ScreenForRect(mHMD->WindowsPos.x, mHMD->WindowsPos.y,
                               mHMD->Resolution.w, mHMD->Resolution.h,
                               getter_AddRefs(mScreen));
    }
  }
}

void
HMDInfoOculus050::Destroy()
{
  if (mHMD) {
    ovrHmd_Destroy(mHMD);
    mHMD = nullptr;
  }
}

bool
HMDInfoOculus050::SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
                      double zNear, double zFar)
{
  float pixelsPerDisplayPixel = 1.0;
  ovrSizei texSize[2];

  uint32_t caps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette; // XXX TODO add TimeWarp

  // get eye parameters and create the mesh
  for (uint32_t eye = 0; eye < NumEyes; eye++) {
    mEyeFOV[eye] = eye == 0 ? aFOVLeft : aFOVRight;
    mFOVPort[eye] = ToFovPort(mEyeFOV[eye]);

    ovrEyeRenderDesc renderDesc = ovrHmd_GetRenderDesc(mHMD, (ovrEyeType) eye, mFOVPort[eye]);

    // these values are negated so that content can add the adjustment to its camera position,
    // instead of subtracting
    mEyeTranslation[eye] = Point3D(-renderDesc.HmdToEyeViewOffset.x, -renderDesc.HmdToEyeViewOffset.y, -renderDesc.HmdToEyeViewOffset.z);

    // note that we are using a right-handed coordinate system here, to match CSS
    mEyeProjectionMatrix[eye] = mEyeFOV[eye].ConstructProjectionMatrix(zNear, zFar, true);

    texSize[eye] = ovrHmd_GetFovTextureSize(mHMD, (ovrEyeType) eye, mFOVPort[eye], pixelsPerDisplayPixel);

    ovrDistortionMesh mesh;
    bool ok = ovrHmd_CreateDistortionMesh(mHMD, (ovrEyeType) eye, mFOVPort[eye], caps, &mesh);
    if (!ok)
      return false;

    mDistortionMesh[eye].mVertices.SetLength(mesh.VertexCount);
    mDistortionMesh[eye].mIndices.SetLength(mesh.IndexCount);

    ovrDistortionVertex *srcv = mesh.pVertexData;
    HMDInfoOculus050::DistortionVertex *destv = reinterpret_cast<HMDInfoOculus050::DistortionVertex*>(mDistortionMesh[eye].mVertices.Elements());
    memset(destv, 0, mesh.VertexCount * sizeof(VRDistortionVertex));
    for (uint32_t i = 0; i < mesh.VertexCount; ++i) {
      destv[i].pos[0] = srcv[i].ScreenPosNDC.x;
      destv[i].pos[1] = srcv[i].ScreenPosNDC.y;

      destv[i].texR[0] = srcv[i].TanEyeAnglesR.x;
      destv[i].texR[1] = srcv[i].TanEyeAnglesR.y;
      destv[i].texG[0] = srcv[i].TanEyeAnglesG.x;
      destv[i].texG[1] = srcv[i].TanEyeAnglesG.y;
      destv[i].texB[0] = srcv[i].TanEyeAnglesB.x;
      destv[i].texB[1] = srcv[i].TanEyeAnglesB.y;

      destv[i].genericAttribs[0] = srcv[i].VignetteFactor;
      destv[i].genericAttribs[1] = srcv[i].TimeWarpFactor;
    }

    memcpy(mDistortionMesh[eye].mIndices.Elements(), mesh.pIndexData, mesh.IndexCount * sizeof(uint16_t));
    ovrHmd_DestroyDistortionMesh(&mesh);
  }

  // take the max of both for eye resolution
  mEyeResolution.width = std::max(texSize[Eye_Left].w, texSize[Eye_Right].w);
  mEyeResolution.height = std::max(texSize[Eye_Left].h, texSize[Eye_Right].h);

  mConfiguration.hmdType = mType;
  mConfiguration.value = 0;
  mConfiguration.fov[0] = aFOVLeft;
  mConfiguration.fov[1] = aFOVRight;

  return true;
}

void
HMDInfoOculus050::FillDistortionConstants(uint32_t whichEye,
                                       const IntSize& textureSize,
                                       const IntRect& eyeViewport,
                                       const Size& destViewport,
                                       const Rect& destRect,
                                       VRDistortionConstants& values)
{
  ovrSizei texSize = { textureSize.width, textureSize.height };
  ovrRecti eyePort = { { eyeViewport.x, eyeViewport.y }, { eyeViewport.width, eyeViewport.height } };
  ovrVector2f scaleOut[2];

  ovrHmd_GetRenderScaleAndOffset(mFOVPort[whichEye], texSize, eyePort, scaleOut);

  values.eyeToSourceScaleAndOffset[0] = scaleOut[1].x;
  values.eyeToSourceScaleAndOffset[1] = scaleOut[1].y;
  values.eyeToSourceScaleAndOffset[2] = scaleOut[0].x;
  values.eyeToSourceScaleAndOffset[3] = scaleOut[0].y;

  // These values are in clip space [-1..1] range, but we're providing
  // scaling in the 0..2 space for sanity.

  // this is the destRect in clip space
  float x0 = destRect.x / destViewport.width * 2.0 - 1.0;
  float x1 = (destRect.x + destRect.width) / destViewport.width * 2.0 - 1.0;

  float y0 = destRect.y / destViewport.height * 2.0 - 1.0;
  float y1 = (destRect.y + destRect.height) / destViewport.height * 2.0 - 1.0;

  // offset
  values.destinationScaleAndOffset[0] = (x0+x1) / 2.0;
  values.destinationScaleAndOffset[1] = (y0+y1) / 2.0;
  // scale
  values.destinationScaleAndOffset[2] = destRect.width / destViewport.width;
  values.destinationScaleAndOffset[3] = destRect.height / destViewport.height;
}

bool
HMDInfoOculus050::StartSensorTracking()
{
  if (mStartCount == 0) {
    bool ok = ovrHmd_ConfigureTracking(mHMD, ovrTrackingCap_Orientation | ovrTrackingCap_Position, 0);
    if (!ok)
      return false;
  }

  mStartCount++;
  return true;
}

void
HMDInfoOculus050::StopSensorTracking()
{
  if (--mStartCount == 0) {
    ovrHmd_ConfigureTracking(mHMD, 0, 0);
  }
}

void
HMDInfoOculus050::ZeroSensor()
{
  ovrHmd_RecenterPose(mHMD);
}

VRHMDSensorState
HMDInfoOculus050::GetSensorState(double timeOffset)
{
  VRHMDSensorState result;
  result.Clear();

  // XXX this is the wrong time base for timeOffset; we need to figure out how to synchronize
  // the Oculus time base and the browser one.
  ovrTrackingState state = ovrHmd_GetTrackingState(mHMD, ovr_GetTimeInSeconds() + timeOffset);
  ovrPoseStatef& pose(state.HeadPose);

  result.timestamp = pose.TimeInSeconds;

  if (state.StatusFlags & ovrStatus_OrientationTracked) {
    result.flags |= State_Orientation;

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
    result.flags |= State_Position;

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

bool
VRHMDManagerOculus050::PlatformInit()
{
  if (mOculusPlatformInitialized)
    return true;

  if (!gfxPrefs::VREnabled())
    return false;

  if (!InitializeOculusCAPI())
    return false;

  ovrInitParams params;
  params.Flags = ovrInit_RequestVersion;
  params.RequestedMinorVersion = LIBOVR_MINOR_VERSION;
  params.LogCallback = nullptr;
  params.ConnectionTimeoutMS = 0;

  bool ok = ovr_Initialize(&params);

  if (!ok)
    return false;

  mOculusPlatformInitialized = true;
  return true;
}

bool
VRHMDManagerOculus050::Init()
{
  if (mOculusInitialized)
    return true;

  if (!PlatformInit())
    return false;

  int count = ovrHmd_Detect();

  for (int i = 0; i < count; ++i) {
    ovrHmd hmd = ovrHmd_Create(i);
    if (hmd) {
      nsRefPtr<HMDInfoOculus050> oc = new HMDInfoOculus050(hmd);
      mOculusHMDs.AppendElement(oc);
    }
  }

  // VRAddTestDevices == 1: add test device only if no real devices present
  // VRAddTestDevices == 2: add test device always
  if ((count == 0 && gfxPrefs::VRAddTestDevices() == 1) ||
      (gfxPrefs::VRAddTestDevices() == 2))
  {
    ovrHmd hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
    if (hmd) {
      nsRefPtr<HMDInfoOculus050> oc = new HMDInfoOculus050(hmd);
      mOculusHMDs.AppendElement(oc);
    }
  }

  mOculusInitialized = true;
  return true;
}

void
VRHMDManagerOculus050::Destroy()
{
  if (!mOculusInitialized)
    return;

  for (size_t i = 0; i < mOculusHMDs.Length(); ++i) {
    mOculusHMDs[i]->Destroy();
  }

  mOculusHMDs.Clear();

  ovr_Shutdown();
  mOculusInitialized = false;
}

void
VRHMDManagerOculus050::GetHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult)
{
  Init();
  for (size_t i = 0; i < mOculusHMDs.Length(); ++i) {
    aHMDResult.AppendElement(mOculusHMDs[i]);
  }
}
