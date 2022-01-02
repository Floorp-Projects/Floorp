/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSVRSession.h"
#include "prenv.h"
#include "nsString.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/SharedLibrary.h"
#include "mozilla/gfx/Quaternion.h"

#if defined(XP_WIN)
#  include <d3d11.h>
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif  // defined(XP_WIN)

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;

namespace {
// need to typedef functions that will be used in the code below
extern "C" {
typedef OSVR_ClientContext (*pfn_osvrClientInit)(
    const char applicationIdentifier[], uint32_t flags);
typedef OSVR_ReturnCode (*pfn_osvrClientShutdown)(OSVR_ClientContext ctx);
typedef OSVR_ReturnCode (*pfn_osvrClientUpdate)(OSVR_ClientContext ctx);
typedef OSVR_ReturnCode (*pfn_osvrClientCheckStatus)(OSVR_ClientContext ctx);
typedef OSVR_ReturnCode (*pfn_osvrClientGetInterface)(
    OSVR_ClientContext ctx, const char path[], OSVR_ClientInterface* iface);
typedef OSVR_ReturnCode (*pfn_osvrClientFreeInterface)(
    OSVR_ClientContext ctx, OSVR_ClientInterface iface);
typedef OSVR_ReturnCode (*pfn_osvrGetOrientationState)(
    OSVR_ClientInterface iface, OSVR_TimeValue* timestamp,
    OSVR_OrientationState* state);
typedef OSVR_ReturnCode (*pfn_osvrGetPositionState)(OSVR_ClientInterface iface,
                                                    OSVR_TimeValue* timestamp,
                                                    OSVR_PositionState* state);
typedef OSVR_ReturnCode (*pfn_osvrClientGetDisplay)(OSVR_ClientContext ctx,
                                                    OSVR_DisplayConfig* disp);
typedef OSVR_ReturnCode (*pfn_osvrClientFreeDisplay)(OSVR_DisplayConfig disp);
typedef OSVR_ReturnCode (*pfn_osvrClientGetNumEyesForViewer)(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount* eyes);
typedef OSVR_ReturnCode (*pfn_osvrClientGetViewerEyePose)(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_Pose3* pose);
typedef OSVR_ReturnCode (*pfn_osvrClientGetDisplayDimensions)(
    OSVR_DisplayConfig disp, OSVR_DisplayInputCount displayInputIndex,
    OSVR_DisplayDimension* width, OSVR_DisplayDimension* height);
typedef OSVR_ReturnCode (
    *pfn_osvrClientGetViewerEyeSurfaceProjectionClippingPlanes)(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, double* left, double* right, double* bottom,
    double* top);
typedef OSVR_ReturnCode (*pfn_osvrClientGetRelativeViewportForViewerEyeSurface)(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, OSVR_ViewportDimension* left,
    OSVR_ViewportDimension* bottom, OSVR_ViewportDimension* width,
    OSVR_ViewportDimension* height);
typedef OSVR_ReturnCode (*pfn_osvrClientGetViewerEyeSurfaceProjectionMatrixf)(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, float near, float far,
    OSVR_MatrixConventions flags, float* matrix);
typedef OSVR_ReturnCode (*pfn_osvrClientCheckDisplayStartup)(
    OSVR_DisplayConfig disp);
typedef OSVR_ReturnCode (*pfn_osvrClientSetRoomRotationUsingHead)(
    OSVR_ClientContext ctx);
}

static pfn_osvrClientInit osvr_ClientInit = nullptr;
static pfn_osvrClientShutdown osvr_ClientShutdown = nullptr;
static pfn_osvrClientUpdate osvr_ClientUpdate = nullptr;
static pfn_osvrClientCheckStatus osvr_ClientCheckStatus = nullptr;
static pfn_osvrClientGetInterface osvr_ClientGetInterface = nullptr;
static pfn_osvrClientFreeInterface osvr_ClientFreeInterface = nullptr;
static pfn_osvrGetOrientationState osvr_GetOrientationState = nullptr;
static pfn_osvrGetPositionState osvr_GetPositionState = nullptr;
static pfn_osvrClientGetDisplay osvr_ClientGetDisplay = nullptr;
static pfn_osvrClientFreeDisplay osvr_ClientFreeDisplay = nullptr;
static pfn_osvrClientGetNumEyesForViewer osvr_ClientGetNumEyesForViewer =
    nullptr;
static pfn_osvrClientGetViewerEyePose osvr_ClientGetViewerEyePose = nullptr;
static pfn_osvrClientGetDisplayDimensions osvr_ClientGetDisplayDimensions =
    nullptr;
static pfn_osvrClientGetViewerEyeSurfaceProjectionClippingPlanes
    osvr_ClientGetViewerEyeSurfaceProjectionClippingPlanes = nullptr;
static pfn_osvrClientGetRelativeViewportForViewerEyeSurface
    osvr_ClientGetRelativeViewportForViewerEyeSurface = nullptr;
static pfn_osvrClientGetViewerEyeSurfaceProjectionMatrixf
    osvr_ClientGetViewerEyeSurfaceProjectionMatrixf = nullptr;
static pfn_osvrClientCheckDisplayStartup osvr_ClientCheckDisplayStartup =
    nullptr;
static pfn_osvrClientSetRoomRotationUsingHead
    osvr_ClientSetRoomRotationUsingHead = nullptr;

bool LoadOSVRRuntime() {
  static PRLibrary* osvrUtilLib = nullptr;
  static PRLibrary* osvrCommonLib = nullptr;
  static PRLibrary* osvrClientLib = nullptr;
  static PRLibrary* osvrClientKitLib = nullptr;
  // this looks up the path in the about:config setting, from greprefs.js or
  // modules\libpref\init\all.js we need all the libs to be valid
#ifdef XP_WIN
  constexpr static auto* pfnGetPathStringPref = mozilla::Preferences::GetString;
  nsAutoString osvrUtilPath, osvrCommonPath, osvrClientPath, osvrClientKitPath;
#else
  constexpr static auto* pfnGetPathStringPref =
      mozilla::Preferences::GetCString;
  nsAutoCString osvrUtilPath, osvrCommonPath, osvrClientPath, osvrClientKitPath;
#endif
  if (NS_FAILED(pfnGetPathStringPref("gfx.vr.osvr.utilLibPath", osvrUtilPath,
                                     PrefValueKind::User)) ||
      NS_FAILED(pfnGetPathStringPref("gfx.vr.osvr.commonLibPath",
                                     osvrCommonPath, PrefValueKind::User)) ||
      NS_FAILED(pfnGetPathStringPref("gfx.vr.osvr.clientLibPath",
                                     osvrClientPath, PrefValueKind::User)) ||
      NS_FAILED(pfnGetPathStringPref("gfx.vr.osvr.clientKitLibPath",
                                     osvrClientKitPath, PrefValueKind::User))) {
    return false;
  }

  osvrUtilLib = LoadLibraryWithFlags(osvrUtilPath.get());
  osvrCommonLib = LoadLibraryWithFlags(osvrCommonPath.get());
  osvrClientLib = LoadLibraryWithFlags(osvrClientPath.get());
  osvrClientKitLib = LoadLibraryWithFlags(osvrClientKitPath.get());

  if (!osvrUtilLib) {
    printf_stderr("[OSVR] Failed to load OSVR Util library!\n");
    return false;
  }
  if (!osvrCommonLib) {
    printf_stderr("[OSVR] Failed to load OSVR Common library!\n");
    return false;
  }
  if (!osvrClientLib) {
    printf_stderr("[OSVR] Failed to load OSVR Client library!\n");
    return false;
  }
  if (!osvrClientKitLib) {
    printf_stderr("[OSVR] Failed to load OSVR ClientKit library!\n");
    return false;
  }

// make sure all functions that we'll be using are available
#define REQUIRE_FUNCTION(_x)                                                  \
  do {                                                                        \
    *(void**)&osvr_##_x = (void*)PR_FindSymbol(osvrClientKitLib, "osvr" #_x); \
    if (!osvr_##_x) {                                                         \
      printf_stderr("osvr" #_x " symbol missing\n");                          \
      goto fail;                                                              \
    }                                                                         \
  } while (0)

  REQUIRE_FUNCTION(ClientInit);
  REQUIRE_FUNCTION(ClientShutdown);
  REQUIRE_FUNCTION(ClientUpdate);
  REQUIRE_FUNCTION(ClientCheckStatus);
  REQUIRE_FUNCTION(ClientGetInterface);
  REQUIRE_FUNCTION(ClientFreeInterface);
  REQUIRE_FUNCTION(GetOrientationState);
  REQUIRE_FUNCTION(GetPositionState);
  REQUIRE_FUNCTION(ClientGetDisplay);
  REQUIRE_FUNCTION(ClientFreeDisplay);
  REQUIRE_FUNCTION(ClientGetNumEyesForViewer);
  REQUIRE_FUNCTION(ClientGetViewerEyePose);
  REQUIRE_FUNCTION(ClientGetDisplayDimensions);
  REQUIRE_FUNCTION(ClientGetViewerEyeSurfaceProjectionClippingPlanes);
  REQUIRE_FUNCTION(ClientGetRelativeViewportForViewerEyeSurface);
  REQUIRE_FUNCTION(ClientGetViewerEyeSurfaceProjectionMatrixf);
  REQUIRE_FUNCTION(ClientCheckDisplayStartup);
  REQUIRE_FUNCTION(ClientSetRoomRotationUsingHead);

#undef REQUIRE_FUNCTION

  return true;

fail:
  return false;
}

}  // namespace

mozilla::gfx::VRFieldOfView SetFromTanRadians(double left, double right,
                                              double bottom, double top) {
  mozilla::gfx::VRFieldOfView fovInfo;
  fovInfo.leftDegrees = atan(left) * 180.0 / M_PI;
  fovInfo.rightDegrees = atan(right) * 180.0 / M_PI;
  fovInfo.upDegrees = atan(top) * 180.0 / M_PI;
  fovInfo.downDegrees = atan(bottom) * 180.0 / M_PI;
  return fovInfo;
}

OSVRSession::OSVRSession()
    : VRSession(),
      mRuntimeLoaded(false),
      mOSVRInitialized(false),
      mClientContextInitialized(false),
      mDisplayConfigInitialized(false),
      mInterfaceInitialized(false),
      m_ctx(nullptr),
      m_iface(nullptr),
      m_display(nullptr) {}
OSVRSession::~OSVRSession() { Shutdown(); }

bool OSVRSession::Initialize(mozilla::gfx::VRSystemState& aSystemState,
                             bool aDetectRuntimesOnly) {
  if (StaticPrefs::dom_vr_puppet_enabled()) {
    // Ensure that tests using the VR Puppet do not find real hardware
    return false;
  }
  if (!StaticPrefs::dom_vr_enabled() || !StaticPrefs::dom_vr_osvr_enabled()) {
    return false;
  }
  if (mOSVRInitialized) {
    return true;
  }
  if (!LoadOSVRRuntime()) {
    return false;
  }
  mRuntimeLoaded = true;

  if (aDetectRuntimesOnly) {
    aSystemState.displayState.capabilityFlags |=
        VRDisplayCapabilityFlags::Cap_ImmersiveVR;
    return false;
  }

  // initialize client context
  InitializeClientContext();
  // try to initialize interface
  InitializeInterface();
  // try to initialize display object
  InitializeDisplay();
  // verify all components are initialized
  CheckOSVRStatus();

  if (!mOSVRInitialized) {
    return false;
  }

  if (!InitState(aSystemState)) {
    return false;
  }

  return true;
}

void OSVRSession::CheckOSVRStatus() {
  if (mOSVRInitialized) {
    return;
  }

  // client context must be initialized first
  InitializeClientContext();

  // update client context
  osvr_ClientUpdate(m_ctx);

  // initialize interface and display if they are not initialized yet
  InitializeInterface();
  InitializeDisplay();

  // OSVR is fully initialized now
  if (mClientContextInitialized && mDisplayConfigInitialized &&
      mInterfaceInitialized) {
    mOSVRInitialized = true;
  }
}

void OSVRSession::InitializeClientContext() {
  // already initialized
  if (mClientContextInitialized) {
    return;
  }

  // first time creating
  if (!m_ctx) {
    // get client context
    m_ctx = osvr_ClientInit("com.osvr.webvr", 0);
    // update context
    osvr_ClientUpdate(m_ctx);
    // verify we are connected
    if (OSVR_RETURN_SUCCESS == osvr_ClientCheckStatus(m_ctx)) {
      mClientContextInitialized = true;
    }
  }
  // client context exists but not up and running yet
  else {
    // update context
    osvr_ClientUpdate(m_ctx);
    if (OSVR_RETURN_SUCCESS == osvr_ClientCheckStatus(m_ctx)) {
      mClientContextInitialized = true;
    }
  }
}

void OSVRSession::InitializeInterface() {
  // already initialized
  if (mInterfaceInitialized) {
    return;
  }
  // Client context must be initialized before getting interface
  if (mClientContextInitialized) {
    // m_iface will remain nullptr if no interface is returned
    if (OSVR_RETURN_SUCCESS ==
        osvr_ClientGetInterface(m_ctx, "/me/head", &m_iface)) {
      mInterfaceInitialized = true;
    }
  }
}

void OSVRSession::InitializeDisplay() {
  // display is fully configured
  if (mDisplayConfigInitialized) {
    return;
  }

  // Client context must be initialized before getting interface
  if (mClientContextInitialized) {
    // first time creating display object
    if (m_display == nullptr) {
      OSVR_ReturnCode ret = osvr_ClientGetDisplay(m_ctx, &m_display);

      if (ret == OSVR_RETURN_SUCCESS) {
        osvr_ClientUpdate(m_ctx);
        // display object may have been created but not fully startup
        if (OSVR_RETURN_SUCCESS == osvr_ClientCheckDisplayStartup(m_display)) {
          mDisplayConfigInitialized = true;
        }
      }

      // Typically once we get Display object, pose data is available after
      // clientUpdate but sometimes it takes ~ 200 ms to get
      // a succesfull connection, so we might have to run a few update cycles
    } else {
      if (OSVR_RETURN_SUCCESS == osvr_ClientCheckDisplayStartup(m_display)) {
        mDisplayConfigInitialized = true;
      }
    }
  }
}

bool OSVRSession::InitState(mozilla::gfx::VRSystemState& aSystemState) {
  VRDisplayState& state = aSystemState.displayState;
  strncpy(state.displayName, "OSVR HMD", kVRDisplayNameMaxLen);
  state.eightCC = GFX_VR_EIGHTCC('O', 'S', 'V', 'R', ' ', ' ', ' ', ' ');
  state.isConnected = true;
  state.isMounted = false;
  state.capabilityFlags =
      (VRDisplayCapabilityFlags)((int)VRDisplayCapabilityFlags::Cap_None |
                                 (int)
                                     VRDisplayCapabilityFlags::Cap_Orientation |
                                 (int)VRDisplayCapabilityFlags::Cap_Position |
                                 (int)VRDisplayCapabilityFlags::Cap_External |
                                 (int)VRDisplayCapabilityFlags::Cap_Present |
                                 (int)
                                     VRDisplayCapabilityFlags::Cap_ImmersiveVR);
  state.blendMode = VRDisplayBlendMode::Opaque;
  state.reportsDroppedFrames = false;

  // XXX OSVR display topology allows for more than one viewer
  // will assume only one viewer for now (most likely stay that way)

  OSVR_EyeCount numEyes;
  osvr_ClientGetNumEyesForViewer(m_display, 0, &numEyes);

  for (uint8_t eye = 0; eye < numEyes; eye++) {
    double left, right, bottom, top;
    // XXX for now there is only one surface per eye
    osvr_ClientGetViewerEyeSurfaceProjectionClippingPlanes(
        m_display, 0, eye, 0, &left, &right, &bottom, &top);
    state.eyeFOV[eye] = SetFromTanRadians(-left, right, -bottom, top);
  }

  // XXX Assuming there is only one display input for now
  // however, it's possible to have more than one (dSight with 2 HDMI inputs)
  OSVR_DisplayDimension width, height;
  osvr_ClientGetDisplayDimensions(m_display, 0, &width, &height);

  for (uint8_t eye = 0; eye < numEyes; eye++) {
    OSVR_ViewportDimension l, b, w, h;
    osvr_ClientGetRelativeViewportForViewerEyeSurface(m_display, 0, eye, 0, &l,
                                                      &b, &w, &h);
    state.eyeResolution.width = w;
    state.eyeResolution.height = h;
    OSVR_Pose3 eyePose;
    // Viewer eye pose may not be immediately available, update client context
    // until we get it
    OSVR_ReturnCode ret =
        osvr_ClientGetViewerEyePose(m_display, 0, eye, &eyePose);
    while (ret != OSVR_RETURN_SUCCESS) {
      osvr_ClientUpdate(m_ctx);
      ret = osvr_ClientGetViewerEyePose(m_display, 0, eye, &eyePose);
    }
    state.eyeTranslation[eye].x = eyePose.translation.data[0];
    state.eyeTranslation[eye].y = eyePose.translation.data[1];
    state.eyeTranslation[eye].z = eyePose.translation.data[2];

    Matrix4x4 pose;
    pose.SetRotationFromQuaternion(gfx::Quaternion(
        osvrQuatGetX(&eyePose.rotation), osvrQuatGetY(&eyePose.rotation),
        osvrQuatGetZ(&eyePose.rotation), osvrQuatGetW(&eyePose.rotation)));
    pose.PreTranslate(eyePose.translation.data[0], eyePose.translation.data[1],
                      eyePose.translation.data[2]);
    pose.Invert();
    mHeadToEye[eye] = pose;
  }

  // default to an identity quaternion
  VRHMDSensorState& sensorState = aSystemState.sensorState;
  sensorState.flags =
      (VRDisplayCapabilityFlags)((int)
                                     VRDisplayCapabilityFlags::Cap_Orientation |
                                 (int)VRDisplayCapabilityFlags::Cap_Position);
  sensorState.pose.orientation[3] = 1.0f;  // Default to an identity quaternion

  return true;
}

void OSVRSession::Shutdown() {
  if (!mRuntimeLoaded) {
    return;
  }
  mOSVRInitialized = false;
  // client context may not have been initialized
  if (m_ctx) {
    osvr_ClientFreeDisplay(m_display);
  }
  // osvr checks that m_ctx or m_iface are not null
  osvr_ClientFreeInterface(m_ctx, m_iface);
  osvr_ClientShutdown(m_ctx);
}

void OSVRSession::ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) {}

void OSVRSession::StartFrame(mozilla::gfx::VRSystemState& aSystemState) {
  UpdateHeadsetPose(aSystemState);
}

void OSVRSession::UpdateHeadsetPose(mozilla::gfx::VRSystemState& aState) {
  // Update client context before anything
  // this usually goes into app's mainloop
  osvr_ClientUpdate(m_ctx);

  VRHMDSensorState result{};
  OSVR_TimeValue timestamp;

  OSVR_OrientationState orientation;

  OSVR_ReturnCode ret =
      osvr_GetOrientationState(m_iface, &timestamp, &orientation);

  aState.sensorState.timestamp = timestamp.seconds;

  if (ret == OSVR_RETURN_SUCCESS) {
    result.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
    result.pose.orientation[0] = orientation.data[1];
    result.pose.orientation[1] = orientation.data[2];
    result.pose.orientation[2] = orientation.data[3];
    result.pose.orientation[3] = orientation.data[0];
  } else {
    // default to an identity quaternion
    result.pose.orientation[3] = 1.0f;
  }

  OSVR_PositionState position;
  ret = osvr_GetPositionState(m_iface, &timestamp, &position);
  if (ret == OSVR_RETURN_SUCCESS) {
    result.flags |= VRDisplayCapabilityFlags::Cap_Position;
    result.pose.position[0] = position.data[0];
    result.pose.position[1] = position.data[1];
    result.pose.position[2] = position.data[2];
  }

  result.CalcViewMatrices(mHeadToEye);
}

bool OSVRSession::StartPresentation() {
  return false;
  // TODO Implement
}

void OSVRSession::StopPresentation() {
  // TODO Implement
}

#if defined(XP_WIN)
bool OSVRSession::SubmitFrame(
    const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
    ID3D11Texture2D* aTexture) {
  return false;
  // TODO Implement
}
#elif defined(XP_MACOSX)
bool OSVRSession::SubmitFrame(
    const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
    const VRLayerTextureHandle& aTexture) {
  return false;
  // TODO Implement
}
#endif

void OSVRSession::VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                                float aIntensity, float aDuration) {}

void OSVRSession::StopVibrateHaptic(uint32_t aControllerIdx) {}

void OSVRSession::StopAllHaptics() {}
