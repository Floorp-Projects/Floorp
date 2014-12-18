/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "gfxVR.h"
#include "mozilla/Preferences.h"

#include "ovr_capi_dynamic.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h" // for gfxWindowsPlatform::GetDPIScale
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

namespace {

#ifdef OVR_CAPI_LIMITED_MOZILLA
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

#if defined(XP_WIN)
# ifdef HAVE_64BIT_BUILD
#  define OVR_LIB_NAME "libovr64.dll"
# else
#  define OVR_LIB_NAME "libovr.dll"
# endif
#elif defined(XP_MACOSX)
# define OVR_LIB_NAME "libovr.dylib"
#else
# define OVR_LIB_NAME 0
#endif

static bool
InitializeOculusCAPI()
{
  static PRLibrary *ovrlib = nullptr;

  if (!ovrlib) {
    const char *libName = OVR_LIB_NAME;

    // If the pref is present, we override libName
    nsAdoptingCString prefLibName = Preferences::GetCString("dom.vr.ovr_lib_path");
    if (prefLibName && prefLibName.get()) {
      libName = prefLibName.get();
    }

    // If the env var is present, we override libName
    if (PR_GetEnv("OVR_LIB_NAME")) {
      libName = PR_GetEnv("OVR_LIB_NAME");
    }

    if (!libName) {
      printf_stderr("Don't know how to find Oculus VR library; missing dom.vr.ovr_lib_path or OVR_LIB_NAME\n");
      return false;
    }

    ovrlib = PR_LoadLibrary(libName);

    if (!ovrlib) {
      // Not found? Try harder. Needed mainly on OSX/etc. where
      // the binary location is not in the search path.
      const char *xulName = "libxul.so";
#if defined(XP_MACOSX)
      xulName = "XUL";
#endif

      char *xulpath = PR_GetLibraryFilePathname(xulName, (PRFuncPtr) &InitializeOculusCAPI);
      if (xulpath) {
        char *xuldir = strrchr(xulpath, '/');
        if (xuldir) {
          *xuldir = 0;
          xuldir = xulpath;

          char *ovrpath = PR_GetLibraryName(xuldir, libName);
          ovrlib = PR_LoadLibrary(ovrpath);
          PR_Free(ovrpath);
        }
        PR_Free(xulpath);
      }
    }

    if (!ovrlib) {
      printf_stderr("Failed to load Oculus VR library, tried '%s'\n", libName);
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

} // anonymous namespace

using namespace mozilla::gfx;

// Dummy nsIScreen implementation, for when we just need to specify a size
class FakeScreen : public nsIScreen
{
public:
  explicit FakeScreen(const IntRect& aScreenRect)
    : mScreenRect(aScreenRect)
  { }

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetRect(int32_t *l, int32_t *t, int32_t *w, int32_t *h) {
    *l = mScreenRect.x;
    *t = mScreenRect.y;
    *w = mScreenRect.width;
    *h = mScreenRect.height;
    return NS_OK;
  }
  NS_IMETHOD GetAvailRect(int32_t *l, int32_t *t, int32_t *w, int32_t *h) {
    return GetRect(l, t, w, h);
  }
  NS_IMETHOD GetRectDisplayPix(int32_t *l, int32_t *t, int32_t *w, int32_t *h) {
    return GetRect(l, t, w, h);
  }
  NS_IMETHOD GetAvailRectDisplayPix(int32_t *l, int32_t *t, int32_t *w, int32_t *h) {
    return GetAvailRect(l, t, w, h);
  }

  NS_IMETHOD GetId(uint32_t* aId) { *aId = (uint32_t)-1; return NS_OK; }
  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) { *aPixelDepth = 24; return NS_OK; }
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth) { *aColorDepth = 24; return NS_OK; }

  NS_IMETHOD LockMinimumBrightness(uint32_t aBrightness) { return NS_ERROR_NOT_AVAILABLE; }
  NS_IMETHOD UnlockMinimumBrightness(uint32_t aBrightness) { return NS_ERROR_NOT_AVAILABLE; }
  NS_IMETHOD GetRotation(uint32_t* aRotation) {
    *aRotation = nsIScreen::ROTATION_0_DEG;
    return NS_OK;
  }
  NS_IMETHOD SetRotation(uint32_t aRotation) { return NS_ERROR_NOT_AVAILABLE; }
  NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor) {
    *aContentsScaleFactor = 1.0;
    return NS_OK;
  }

protected:
  virtual ~FakeScreen() {}

  IntRect mScreenRect;
};

NS_IMPL_ISUPPORTS(FakeScreen, nsIScreen)

class HMDInfoOculus : public VRHMDInfo {
  friend class VRHMDManagerOculusImpl;
public:
  explicit HMDInfoOculus(ovrHmd aHMD);

  bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
              double zNear, double zFar) MOZ_OVERRIDE;

  bool StartSensorTracking() MOZ_OVERRIDE;
  VRHMDSensorState GetSensorState(double timeOffset) MOZ_OVERRIDE;
  void StopSensorTracking() MOZ_OVERRIDE;
  void ZeroSensor() MOZ_OVERRIDE;

  void FillDistortionConstants(uint32_t whichEye,
                               const IntSize& textureSize, const IntRect& eyeViewport,
                               const Size& destViewport, const Rect& destRect,
                               VRDistortionConstants& values) MOZ_OVERRIDE;

  void Destroy();

protected:
  virtual ~HMDInfoOculus() {
      Destroy();
      MOZ_COUNT_DTOR_INHERITED(HMDInfoOculus, VRHMDInfo);
  }

  ovrHmd mHMD;
  ovrFovPort mFOVPort[2];
  uint32_t mStartCount;
};

static ovrFovPort
ToFovPort(const VRFieldOfView& aFOV)
{
  ovrFovPort fovPort;
  fovPort.LeftTan = tan(aFOV.leftDegrees * M_PI / 180.0);
  fovPort.RightTan = tan(aFOV.rightDegrees * M_PI / 180.0);
  fovPort.UpTan = tan(aFOV.upDegrees * M_PI / 180.0);
  fovPort.DownTan = tan(aFOV.downDegrees * M_PI / 180.0);
  return fovPort;
}

static VRFieldOfView
FromFovPort(const ovrFovPort& aFOV)
{
  VRFieldOfView fovInfo;
  fovInfo.leftDegrees = atan(aFOV.LeftTan) * 180.0 / M_PI;
  fovInfo.rightDegrees = atan(aFOV.RightTan) * 180.0 / M_PI;
  fovInfo.upDegrees = atan(aFOV.UpTan) * 180.0 / M_PI;
  fovInfo.downDegrees = atan(aFOV.DownTan) * 180.0 / M_PI;
  return fovInfo;
}

HMDInfoOculus::HMDInfoOculus(ovrHmd aHMD)
  : VRHMDInfo(VRHMDType::Oculus)
  , mHMD(aHMD)
  , mStartCount(0)
{
  MOZ_COUNT_CTOR_INHERITED(HMDInfoOculus, VRHMDInfo);

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
    screenmgr->ScreenForRect(mHMD->WindowsPos.x, mHMD->WindowsPos.y,
                             mHMD->Resolution.w, mHMD->Resolution.h,
                             getter_AddRefs(mScreen));
  }
}

void
HMDInfoOculus::Destroy()
{
  if (mHMD) {
    ovrHmd_Destroy(mHMD);
    mHMD = nullptr;
  }
}

bool
HMDInfoOculus::SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
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
    mEyeTranslation[eye] = Point3D(-renderDesc.ViewAdjust.x, -renderDesc.ViewAdjust.y, -renderDesc.ViewAdjust.z);

    // note that we are using a right-handed coordinate system here, to match CSS
    ovrMatrix4f projMatrix = ovrMatrix4f_Projection(mFOVPort[eye], zNear, zFar, true);

    // XXX this is gross, we really need better methods on Matrix4x4
    memcpy(&mEyeProjectionMatrix[eye], projMatrix.M, sizeof(ovrMatrix4f));
    mEyeProjectionMatrix[eye].Transpose();

    texSize[eye] = ovrHmd_GetFovTextureSize(mHMD, (ovrEyeType) eye, mFOVPort[eye], pixelsPerDisplayPixel);

    ovrDistortionMesh mesh;
    bool ok = ovrHmd_CreateDistortionMesh(mHMD, (ovrEyeType) eye, mFOVPort[eye], caps, &mesh);
    if (!ok)
      return false;

    mDistortionMesh[eye].mVertices.SetLength(mesh.VertexCount);
    mDistortionMesh[eye].mIndices.SetLength(mesh.IndexCount);

    ovrDistortionVertex *srcv = mesh.pVertexData;
    VRDistortionVertex *destv = mDistortionMesh[eye].mVertices.Elements();
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
  //* need to call this during rendering each frame I think? */
  //ovrHmd_GetRenderScaleAndOffset(fovPort, texSize, renderViewport, uvScaleOffsetOut);
}

void
HMDInfoOculus::FillDistortionConstants(uint32_t whichEye,
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

  values.eyeToSourceScaleAndOffset[0] = scaleOut[0].x;
  values.eyeToSourceScaleAndOffset[1] = scaleOut[0].y;
  values.eyeToSourceScaleAndOffset[2] = scaleOut[1].x;
  values.eyeToSourceScaleAndOffset[3] = scaleOut[1].y;

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
HMDInfoOculus::StartSensorTracking()
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
HMDInfoOculus::StopSensorTracking()
{
  if (--mStartCount == 0) {
    ovrHmd_ConfigureTracking(mHMD, 0, 0);
  }
}

void
HMDInfoOculus::ZeroSensor()
{
  ovrHmd_RecenterPose(mHMD);
}

VRHMDSensorState
HMDInfoOculus::GetSensorState(double timeOffset)
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

namespace mozilla {
namespace gfx {

class VRHMDManagerOculusImpl {
public:
  VRHMDManagerOculusImpl() : mOculusInitialized(false), mOculusPlatformInitialized(false)
  { }

  bool PlatformInit();
  bool Init();
  void Destroy();
  void GetOculusHMDs(nsTArray<nsRefPtr<VRHMDInfo> >& aHMDResult);
protected:
  nsTArray<nsRefPtr<HMDInfoOculus>> mOculusHMDs;
  bool mOculusInitialized;
  bool mOculusPlatformInitialized;
};

} // namespace gfx
} // namespace mozilla

VRHMDManagerOculusImpl *VRHMDManagerOculus::mImpl = nullptr;

// These just forward to the Impl class, to have a non-static container for various
// objects.

bool
VRHMDManagerOculus::PlatformInit()
{
  if (!mImpl) {
    mImpl = new VRHMDManagerOculusImpl;
  }
  return mImpl->PlatformInit();
}

bool
VRHMDManagerOculus::Init()
{
  if (!mImpl) {
    mImpl = new VRHMDManagerOculusImpl;
  }
  return mImpl->Init();
}

void
VRHMDManagerOculus::GetOculusHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult)
{
  if (!mImpl) {
    mImpl = new VRHMDManagerOculusImpl;
  }
  mImpl->GetOculusHMDs(aHMDResult);
}

void
VRHMDManagerOculus::Destroy()
{
  if (!mImpl)
    return;
  mImpl->Destroy();
  delete mImpl;
  mImpl = nullptr;
}

bool
VRHMDManagerOculusImpl::PlatformInit()
{
  if (mOculusPlatformInitialized)
    return true;

  if (!gfxPrefs::VREnabled())
    return false;

  if (!InitializeOculusCAPI())
    return false;

  bool ok = ovr_Initialize();

  if (!ok)
    return false;

  mOculusPlatformInitialized = true;
  return true;
}

bool
VRHMDManagerOculusImpl::Init()
{
  if (mOculusInitialized)
    return true;

  if (!PlatformInit())
    return false;

  int count = ovrHmd_Detect();

  for (int i = 0; i < count; ++i) {
    ovrHmd hmd = ovrHmd_Create(i);
    nsRefPtr<HMDInfoOculus> oc = new HMDInfoOculus(hmd);
    mOculusHMDs.AppendElement(oc);
  }

  // VRAddTestDevices == 1: add test device only if no real devices present
  // VRAddTestDevices == 2: add test device always
  if ((count == 0 && gfxPrefs::VRAddTestDevices() == 1) ||
      (gfxPrefs::VRAddTestDevices() == 2))
  {
    ovrHmd hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
    nsRefPtr<HMDInfoOculus> oc = new HMDInfoOculus(hmd);
    mOculusHMDs.AppendElement(oc);
  }

  mOculusInitialized = true;
  return true;
}

void
VRHMDManagerOculusImpl::Destroy()
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
VRHMDManagerOculusImpl::GetOculusHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult)
{
  Init();
  for (size_t i = 0; i < mOculusHMDs.Length(); ++i) {
    aHMDResult.AppendElement(mOculusHMDs[i]);
  }
}
