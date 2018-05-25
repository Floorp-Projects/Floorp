/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#error "Oculus 1.3 runtime support only available for Windows"
#endif

#include <math.h>


#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedLibrary.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/CompositorThread.h"
#include "ipc/VRLayerParent.h"

#include "mozilla/gfx/Quaternion.h"

#include <d3d11.h>
#include "CompositorD3D11.h"
#include "TextureD3D11.h"

#include "gfxVROculus.h"
#include "VRManagerParent.h"
#include "VRThread.h"

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/Telemetry.h"

/** XXX The DX11 objects and quad blitting could be encapsulated
 *    into a separate object if either Oculus starts supporting
 *     non-Windows platforms or the blit is needed by other HMD\
 *     drivers.
 *     Alternately, we could remove the extra blit for
 *     Oculus as well with some more refactoring.
 */

// See CompositorD3D11Shaders.h
namespace mozilla {
namespace layers {
struct ShaderBytes { const void* mData; size_t mLength; };
extern ShaderBytes sRGBShader;
extern ShaderBytes sLayerQuadVS;
} // namespace layers
} // namespace mozilla
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;
using namespace mozilla::dom;

namespace {

static pfn_ovr_Initialize ovr_Initialize = nullptr;
static pfn_ovr_Shutdown ovr_Shutdown = nullptr;
static pfn_ovr_GetLastErrorInfo ovr_GetLastErrorInfo = nullptr;
static pfn_ovr_GetVersionString ovr_GetVersionString = nullptr;
static pfn_ovr_TraceMessage ovr_TraceMessage = nullptr;
static pfn_ovr_IdentifyClient ovr_IdentifyClient = nullptr;
static pfn_ovr_GetHmdDesc ovr_GetHmdDesc = nullptr;
static pfn_ovr_GetTrackerCount ovr_GetTrackerCount = nullptr;
static pfn_ovr_GetTrackerDesc ovr_GetTrackerDesc = nullptr;
static pfn_ovr_Create ovr_Create = nullptr;
static pfn_ovr_Destroy ovr_Destroy = nullptr;
static pfn_ovr_GetSessionStatus ovr_GetSessionStatus = nullptr;
static pfn_ovr_IsExtensionSupported ovr_IsExtensionSupported = nullptr;
static pfn_ovr_EnableExtension ovr_EnableExtension = nullptr;
static pfn_ovr_SetTrackingOriginType ovr_SetTrackingOriginType = nullptr;
static pfn_ovr_GetTrackingOriginType ovr_GetTrackingOriginType = nullptr;
static pfn_ovr_RecenterTrackingOrigin ovr_RecenterTrackingOrigin = nullptr;
static pfn_ovr_SpecifyTrackingOrigin ovr_SpecifyTrackingOrigin = nullptr;
static pfn_ovr_ClearShouldRecenterFlag ovr_ClearShouldRecenterFlag = nullptr;
static pfn_ovr_GetTrackingState ovr_GetTrackingState = nullptr;
static pfn_ovr_GetDevicePoses ovr_GetDevicePoses = nullptr;
static pfn_ovr_GetTrackerPose ovr_GetTrackerPose = nullptr;
static pfn_ovr_GetInputState ovr_GetInputState = nullptr;
static pfn_ovr_GetConnectedControllerTypes ovr_GetConnectedControllerTypes = nullptr;
static pfn_ovr_GetTouchHapticsDesc ovr_GetTouchHapticsDesc = nullptr;
static pfn_ovr_SetControllerVibration ovr_SetControllerVibration = nullptr;
static pfn_ovr_SubmitControllerVibration ovr_SubmitControllerVibration = nullptr;
static pfn_ovr_GetControllerVibrationState ovr_GetControllerVibrationState = nullptr;
static pfn_ovr_TestBoundary ovr_TestBoundary = nullptr;
static pfn_ovr_TestBoundaryPoint ovr_TestBoundaryPoint = nullptr;
static pfn_ovr_SetBoundaryLookAndFeel ovr_SetBoundaryLookAndFeel = nullptr;
static pfn_ovr_ResetBoundaryLookAndFeel ovr_ResetBoundaryLookAndFeel = nullptr;
static pfn_ovr_GetBoundaryGeometry ovr_GetBoundaryGeometry = nullptr;
static pfn_ovr_GetBoundaryDimensions ovr_GetBoundaryDimensions = nullptr;
static pfn_ovr_GetBoundaryVisible ovr_GetBoundaryVisible = nullptr;
static pfn_ovr_RequestBoundaryVisible ovr_RequestBoundaryVisible = nullptr;
static pfn_ovr_GetTextureSwapChainLength ovr_GetTextureSwapChainLength = nullptr;
static pfn_ovr_GetTextureSwapChainCurrentIndex ovr_GetTextureSwapChainCurrentIndex = nullptr;
static pfn_ovr_GetTextureSwapChainDesc ovr_GetTextureSwapChainDesc = nullptr;
static pfn_ovr_CommitTextureSwapChain ovr_CommitTextureSwapChain = nullptr;
static pfn_ovr_DestroyTextureSwapChain ovr_DestroyTextureSwapChain = nullptr;
static pfn_ovr_DestroyMirrorTexture ovr_DestroyMirrorTexture = nullptr;
static pfn_ovr_GetFovTextureSize ovr_GetFovTextureSize = nullptr;
static pfn_ovr_GetRenderDesc2 ovr_GetRenderDesc2 = nullptr;
static pfn_ovr_WaitToBeginFrame ovr_WaitToBeginFrame = nullptr;
static pfn_ovr_BeginFrame ovr_BeginFrame = nullptr;
static pfn_ovr_EndFrame ovr_EndFrame = nullptr;
static pfn_ovr_SubmitFrame ovr_SubmitFrame = nullptr;
static pfn_ovr_GetPerfStats ovr_GetPerfStats = nullptr;
static pfn_ovr_ResetPerfStats ovr_ResetPerfStats = nullptr;
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
static pfn_ovr_GetExternalCameras ovr_GetExternalCameras = nullptr;
static pfn_ovr_SetExternalCameraProperties ovr_SetExternalCameraProperties = nullptr;

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
#define OVR_MAJOR_VERSION   1
#define OVR_MINOR_VERSION   19

enum class OculusLeftControllerButtonType : uint16_t {
  LThumb,
  IndexTrigger,
  HandTrigger,
  Button_X,
  Button_Y,
  LThumbRest,
  NumButtonType
};

enum class OculusRightControllerButtonType : uint16_t {
  RThumb,
  IndexTrigger,
  HandTrigger,
  Button_A,
  Button_B,
  RThumbRest,
  NumButtonType
};

static const uint32_t kNumOculusButton = static_cast<uint32_t>
                                         (OculusLeftControllerButtonType::
                                         NumButtonType);
static const uint32_t kNumOculusHaptcs = 1;

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

VROculusSession::VROculusSession()
  : mOvrLib(nullptr)
  , mSession(nullptr)
  , mInitFlags((ovrInitFlags)0)
  , mTextureSet(nullptr)
  , mRequestPresentation(false)
  , mRequestTracking(false)
  , mDrawBlack(false)
  , mIsConnected(false)
  , mIsMounted(false)
{
}

ovrSession
VROculusSession::Get()
{
  MOZ_ASSERT(mSession);
  return mSession;
}

bool
VROculusSession::IsTrackingReady() const
{
  // We should return true only if the HMD is connected and we
  // are ready for tracking
  MOZ_ASSERT(!mIsConnected || mSession);
  return mIsConnected;
}

bool
VROculusSession::IsPresentationReady() const
{
  return !mRenderTargets.IsEmpty();
}

bool
VROculusSession::IsMounted() const
{
  return mIsMounted;
}

void
VROculusSession::StopTracking()
{
  if (mRequestTracking) {
    mRequestTracking = false;
    Refresh();
  }
}

void
VROculusSession::StartTracking()
{
  if (!mRequestTracking) {
    mRequestTracking = true;
    Refresh();
  }
}

void
VROculusSession::StartPresentation(const IntSize& aSize)
{
  if (!mRequestPresentation) {
    mRequestPresentation = true;
    mTelemetry.Clear();
    mTelemetry.mPresentationStart = TimeStamp::Now();

    ovrPerfStats perfStats;
    if (ovr_GetPerfStats(mSession, &perfStats) == ovrSuccess) {
      if (perfStats.FrameStatsCount) {
        mTelemetry.mLastDroppedFrameCount = perfStats.FrameStats[0].AppDroppedFrameCount;
      }
    }
  }

  // Update the size, even when we are already presenting.
  mPresentationSize = aSize;
  Refresh();
}

void
VROculusSession::StopPresentation()
{
  if (mRequestPresentation) {
    mLastPresentationEnd = TimeStamp::Now();
    mRequestPresentation = false;

    const TimeDuration duration = mLastPresentationEnd - mTelemetry.mPresentationStart;
    Telemetry::Accumulate(Telemetry::WEBVR_USERS_VIEW_IN, 1);
    Telemetry::Accumulate(Telemetry::WEBVR_TIME_SPENT_VIEWING_IN_OCULUS,
                          duration.ToMilliseconds());

    if (mTelemetry.IsLastDroppedFrameValid() && duration.ToSeconds()) {
      ovrPerfStats perfStats;
      if (ovr_GetPerfStats(mSession, &perfStats) == ovrSuccess) {
        if (perfStats.FrameStatsCount) {
          const uint32_t droppedFramesPerSec = (perfStats.FrameStats[0].AppDroppedFrameCount -
                                                mTelemetry.mLastDroppedFrameCount) / duration.ToSeconds();
          Telemetry::Accumulate(Telemetry::WEBVR_DROPPED_FRAMES_IN_OCULUS, droppedFramesPerSec);
        }
      }
    }
    Refresh();
  }
}

VROculusSession::~VROculusSession()
{
  mSubmitThread = nullptr;
  Uninitialize();
}

void
VROculusSession::Uninitialize()
{
  StopRendering();
  StopSession();
  StopLib();
  UnloadOvrLib();
}

void
VROculusSession::StopRendering()
{
  if (!mRenderTargets.IsEmpty()) {
    mRenderTargets.Clear();
  }
  if (mTextureSet && mSession) {
    ovr_DestroyTextureSwapChain(mSession, mTextureSet);
  }
  mTextureSet = nullptr;
  mDevice = nullptr;
}

void
VROculusSession::StopSession()
{
  if (mSession) {
    ovr_Destroy(mSession);
    mIsConnected = false;
    mIsMounted = false;
    mSession = nullptr;
  }
}

void
VROculusSession::StopLib()
{
  if (mInitFlags) {
    ovr_Shutdown();
    mInitFlags = (ovrInitFlags)0;
  }
}

void
VROculusSession::Refresh(bool aForceRefresh)
{
  // We are waiting for drawing the black layer command for
  // Compositor thread. Ignore Refresh() calls from other threads.
  if (mDrawBlack && !aForceRefresh) {
    return;
  }

  if (!mRequestTracking) {
    Uninitialize();
    return;
  }

  ovrInitFlags flags = (ovrInitFlags)(ovrInit_RequestVersion | ovrInit_MixedRendering);
  bool bInvisible = true;
  if (!gfxPrefs::VROculusInvisibleEnabled()) {
    bInvisible = false;
  }
  if (mRequestPresentation) {
    bInvisible = false;
  } else if (!mLastPresentationEnd.IsNull()) {
    TimeDuration duration = TimeStamp::Now() - mLastPresentationEnd;
    TimeDuration timeout = TimeDuration::FromMilliseconds(gfxPrefs::VROculusPresentTimeout());
    if (timeout > TimeDuration(0) && duration < timeout) {
      // Do not immediately re-initialize with an invisible session after
      // the end of a VR presentation.  Waiting for the configured duraction
      // ensures that the user will not drop to Oculus Home during VR link
      // traversal.
      bInvisible = false;

      // While we are waiting for either the timeout or a new presentation,
      // fill the HMD with black / no layers.
      if (mSession && mTextureSet) {
        if (!aForceRefresh) {
          // VROculusSession didn't start submitting frames yet.
          // Or, the VR thread has been shut down already.
          if (!mSubmitThread || !mSubmitThread->IsActive()) {
            return;
          }
          // ovr_SubmitFrame is running at VR Submit thread,
          // so we post this task to VR Submit thread and let it paint
          // a black frame.
          mDrawBlack = true;
          mSubmitThread->PostTask(NewRunnableMethod<bool>(
            "gfx::VROculusSession::Refresh",
            this,
            &VROculusSession::Refresh, true));
          return;
        }
        ovrLayerEyeFov layer;
        memset(&layer, 0, sizeof(layer));
        layer.Header.Type = ovrLayerType_Disabled;
        ovrLayerHeader *layers = &layer.Header;
        ovr_SubmitFrame(mSession, 0, nullptr, &layers, 1);
        mDrawBlack = false;
      }
    }
  }
  if (bInvisible) {
    flags = (ovrInitFlags)(flags | ovrInit_Invisible);
  }

  if (mInitFlags != flags) {
    Uninitialize();
  }

  if(!Initialize(flags)) {
    // If we fail to initialize, ensure the Oculus libraries
    // are unloaded, as we can't poll for ovrSessionStatus::ShouldQuit
    // without an active ovrSession.
    Uninitialize();
  }

  if (mSession) {
    ovrSessionStatus status;
    if (OVR_SUCCESS(ovr_GetSessionStatus(mSession, &status))) {
      mIsConnected = status.HmdPresent;
      mIsMounted = status.HmdMounted;
      if (status.ShouldQuit) {
        mLastShouldQuit = TimeStamp::Now();
        Uninitialize();
      }
    } else {
      mIsConnected = false;
      mIsMounted = false;
    }
  }
}

bool
VROculusSession::IsQuitTimeoutActive()
{
  // If Oculus asked us to quit our session, do not try to initialize again
  // immediately.
  if (!mLastShouldQuit.IsNull()) {
    TimeDuration duration = TimeStamp::Now() - mLastShouldQuit;
    TimeDuration timeout = TimeDuration::FromMilliseconds(gfxPrefs::VROculusQuitTimeout());
    if (timeout > TimeDuration(0) && duration < timeout) {
      return true;
    }
  }
  return false;
}

bool
VROculusSession::Initialize(ovrInitFlags aFlags)
{
  if (IsQuitTimeoutActive()) {
    return false;
  }

  if (!LoadOvrLib()) {
    return false;
  }
  if (!StartLib(aFlags)) {
    return false;
  }
  if (!StartSession()) {
    return false;
  }
  if (!StartRendering()) {
    return false;
  }
  return true;
}

bool
VROculusSession::StartRendering()
{
  if (!mRequestPresentation) {
    // Nothing to do if we aren't presenting
    return true;
  }
  if (!mDevice) {
    mDevice = gfx::DeviceManagerDx::Get()->GetVRDevice();
    if (!mDevice) {
      NS_WARNING("Failed to get a D3D11Device for Oculus");
      return false;
    }
  }

  if (!mTextureSet) {
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
    desc.Width = mPresentationSize.width;
    desc.Height = mPresentationSize.height;
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.StaticImage = false;
    desc.MiscFlags = ovrTextureMisc_DX_Typeless;
    desc.BindFlags = ovrTextureBind_DX_RenderTarget;

    ovrResult orv = ovr_CreateTextureSwapChainDX(mSession, mDevice, &desc, &mTextureSet);
    if (orv != ovrSuccess) {
      NS_WARNING("ovr_CreateTextureSwapChainDX failed");
      return false;
    }
  }

  if (mTextureSet && mRenderTargets.IsEmpty()) {
    int textureCount = 0;
    ovrResult orv = ovr_GetTextureSwapChainLength(mSession, mTextureSet, &textureCount);
    if (orv != ovrSuccess) {
      NS_WARNING("ovr_GetTextureSwapChainLength failed");
      return false;
    }
    mRenderTargets.SetLength(textureCount);
    for (int i = 0; i < textureCount; ++i) {
      RefPtr<CompositingRenderTargetD3D11> rt;
      ID3D11Texture2D* texture = nullptr;
      orv = ovr_GetTextureSwapChainBufferDX(mSession, mTextureSet, i, IID_PPV_ARGS(&texture));
      MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainBufferDX failed.");
      rt = new CompositingRenderTargetD3D11(texture, IntPoint(0, 0), DXGI_FORMAT_B8G8R8A8_UNORM);
      rt->SetSize(mPresentationSize);
      mRenderTargets[i] = rt;
      texture->Release();
    }
  }
  return true;
}

bool
VROculusSession::StartLib(ovrInitFlags aFlags)
{
  if (mInitFlags == 0) {
    ovrInitParams params;
    memset(&params, 0, sizeof(params));
    params.Flags = aFlags;
    params.RequestedMinorVersion = OVR_MINOR_VERSION;
    params.LogCallback = nullptr;
    params.ConnectionTimeoutMS = 0;

    ovrResult orv = ovr_Initialize(&params);

    if (orv == ovrSuccess) {
      mInitFlags = aFlags;
    }
    else {
      return false;
    }
  }
  MOZ_ASSERT(mInitFlags == aFlags);
  return true;
}

bool
VROculusSession::StartSession()
{
  // ovr_Create can be slow when no HMD is present and we wish
  // to keep the same oculus session when possible, so we detect
  // presence of an HMD with ovr_GetHmdDesc before calling ovr_Create
  ovrHmdDesc desc = ovr_GetHmdDesc(NULL);
  if (desc.Type == ovrHmd_None) {
    // No HMD connected, destroy any existing session
    if (mSession) {
      ovr_Destroy(mSession);
      mSession = nullptr;
    }
    return false;
  }
  if (mSession != nullptr) {
    // HMD Detected and we already have a session, let's keep using it.
    return true;
  }

  // HMD Detected and we don't have a session yet,
  // try to create a new session
  ovrSession session;
  ovrGraphicsLuid luid;
  ovrResult orv = ovr_Create(&session, &luid);
  if (orv == ovrSuccess) {
    orv = ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);
    if (orv != ovrSuccess) {
      NS_WARNING("ovr_SetTrackingOriginType failed.\n");
    }
    mSession = session;
    return true;
  }

  // Failed to create a session for the HMD
  return false;
}

bool
VROculusSession::LoadOvrLib()
{
  if (mOvrLib) {
    // Already loaded, early exit
    return true;
  }
#if defined(_WIN32)
  nsTArray<nsString> libSearchPaths;
  nsString libName;
  nsString searchPath;

  static const char dirSep = '\\';
  static const int pathLen = 260;
  searchPath.SetCapacity(pathLen);
  int realLen = ::GetSystemDirectoryW(char16ptr_t(searchPath.BeginWriting()),
                                      pathLen);
  if (realLen != 0 && realLen < pathLen) {
    searchPath.SetLength(realLen);
    libSearchPaths.AppendElement(searchPath);
  }
  libName.AppendPrintf("LibOVRRT%d_%d.dll", BUILD_BITS, OVR_PRODUCT_VERSION);

  // search the path/module dir
  libSearchPaths.InsertElementsAt(0, 1, EmptyString());

  // If the env var is present, we override libName
  if (_wgetenv(L"OVR_LIB_PATH")) {
    searchPath = _wgetenv(L"OVR_LIB_PATH");
    libSearchPaths.InsertElementsAt(0, 1, searchPath);
  }

  if (_wgetenv(L"OVR_LIB_NAME")) {
    libName = _wgetenv(L"OVR_LIB_NAME");
  }

  for (uint32_t i = 0; i < libSearchPaths.Length(); ++i) {
    nsString& libPath = libSearchPaths[i];
    nsString fullName;
    if (libPath.Length() == 0) {
      fullName.Assign(libName);
    } else {
      fullName.AppendPrintf("%s%c%s", libPath.get(), dirSep, libName.get());
    }

    mOvrLib = LoadLibraryWithFlags(fullName.get());
    if (mOvrLib) {
      break;
    }
  }
#else
#error "Unsupported platform!"
#endif

  if (!mOvrLib) {
    return false;
  }

#define REQUIRE_FUNCTION(_x) do { \
    *(void **)&_x = (void *) PR_FindSymbol(mOvrLib, #_x);                \
    if (!_x) { printf_stderr(#_x " symbol missing\n"); goto fail; }       \
  } while (0)

  REQUIRE_FUNCTION(ovr_Initialize);
  REQUIRE_FUNCTION(ovr_Shutdown);
  REQUIRE_FUNCTION(ovr_GetLastErrorInfo);
  REQUIRE_FUNCTION(ovr_GetVersionString);
  REQUIRE_FUNCTION(ovr_TraceMessage);
  REQUIRE_FUNCTION(ovr_IdentifyClient);
  REQUIRE_FUNCTION(ovr_GetHmdDesc);
  REQUIRE_FUNCTION(ovr_GetTrackerCount);
  REQUIRE_FUNCTION(ovr_GetTrackerDesc);
  REQUIRE_FUNCTION(ovr_Create);
  REQUIRE_FUNCTION(ovr_Destroy);
  REQUIRE_FUNCTION(ovr_GetSessionStatus);
  REQUIRE_FUNCTION(ovr_IsExtensionSupported);
  REQUIRE_FUNCTION(ovr_EnableExtension);
  REQUIRE_FUNCTION(ovr_SetTrackingOriginType);
  REQUIRE_FUNCTION(ovr_GetTrackingOriginType);
  REQUIRE_FUNCTION(ovr_RecenterTrackingOrigin);
  REQUIRE_FUNCTION(ovr_SpecifyTrackingOrigin);
  REQUIRE_FUNCTION(ovr_ClearShouldRecenterFlag);
  REQUIRE_FUNCTION(ovr_GetTrackingState);
  REQUIRE_FUNCTION(ovr_GetDevicePoses);
  REQUIRE_FUNCTION(ovr_GetTrackerPose);
  REQUIRE_FUNCTION(ovr_GetInputState);
  REQUIRE_FUNCTION(ovr_GetConnectedControllerTypes);
  REQUIRE_FUNCTION(ovr_GetTouchHapticsDesc);
  REQUIRE_FUNCTION(ovr_SetControllerVibration);
  REQUIRE_FUNCTION(ovr_SubmitControllerVibration);
  REQUIRE_FUNCTION(ovr_GetControllerVibrationState);
  REQUIRE_FUNCTION(ovr_TestBoundary);
  REQUIRE_FUNCTION(ovr_TestBoundaryPoint);
  REQUIRE_FUNCTION(ovr_SetBoundaryLookAndFeel);
  REQUIRE_FUNCTION(ovr_ResetBoundaryLookAndFeel);
  REQUIRE_FUNCTION(ovr_GetBoundaryGeometry);
  REQUIRE_FUNCTION(ovr_GetBoundaryDimensions);
  REQUIRE_FUNCTION(ovr_GetBoundaryVisible);
  REQUIRE_FUNCTION(ovr_RequestBoundaryVisible);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainLength);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainCurrentIndex);
  REQUIRE_FUNCTION(ovr_GetTextureSwapChainDesc);
  REQUIRE_FUNCTION(ovr_CommitTextureSwapChain);
  REQUIRE_FUNCTION(ovr_DestroyTextureSwapChain);
  REQUIRE_FUNCTION(ovr_DestroyMirrorTexture);
  REQUIRE_FUNCTION(ovr_GetFovTextureSize);
  REQUIRE_FUNCTION(ovr_GetRenderDesc2);
  REQUIRE_FUNCTION(ovr_WaitToBeginFrame);
  REQUIRE_FUNCTION(ovr_BeginFrame);
  REQUIRE_FUNCTION(ovr_EndFrame);
  REQUIRE_FUNCTION(ovr_SubmitFrame);
  REQUIRE_FUNCTION(ovr_GetPerfStats);
  REQUIRE_FUNCTION(ovr_ResetPerfStats);
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
  REQUIRE_FUNCTION(ovr_GetExternalCameras);
  REQUIRE_FUNCTION(ovr_SetExternalCameraProperties);

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
  PR_UnloadLibrary(mOvrLib);
  mOvrLib = nullptr;
  return false;
}

already_AddRefed<CompositingRenderTargetD3D11>
VROculusSession::GetNextRenderTarget()
{
  int currentRenderTarget = 0;
  DebugOnly<ovrResult> orv = ovr_GetTextureSwapChainCurrentIndex(mSession, mTextureSet, &currentRenderTarget);
  MOZ_ASSERT(orv == ovrSuccess, "ovr_GetTextureSwapChainCurrentIndex failed.");

  mRenderTargets[currentRenderTarget]->ClearOnBind();
  RefPtr<CompositingRenderTargetD3D11> rt = mRenderTargets[currentRenderTarget];
  return rt.forget();
}

ovrTextureSwapChain
VROculusSession::GetSwapChain()
{
  MOZ_ASSERT(mTextureSet);
  return mTextureSet;
}

void
VROculusSession::UnloadOvrLib()
{
  if (mOvrLib) {
    PR_UnloadLibrary(mOvrLib);
    mOvrLib = nullptr;
  }
}

VRDisplayOculus::VRDisplayOculus(VROculusSession* aSession)
  : VRDisplayHost(VRDeviceType::Oculus)
  , mSession(aSession)
  , mQuadVS(nullptr)
  , mQuadPS(nullptr)
  , mLinearSamplerState(nullptr)
  , mVSConstantBuffer(nullptr)
  , mPSConstantBuffer(nullptr)
  , mVertexBuffer(nullptr)
  , mInputLayout(nullptr)
  , mEyeHeight(OVR_DEFAULT_EYE_HEIGHT)
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayOculus, VRDisplayHost);
  VRDisplayState& state = mDisplayInfo.mDisplayState;
  strncpy(state.mDisplayName, "Oculus VR HMD", kVRDisplayNameMaxLen);
  state.mIsConnected = true;
  state.mIsMounted = false;

  mDesc = ovr_GetHmdDesc(aSession->Get());

  state.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None;
  if (mDesc.AvailableTrackingCaps & ovrTrackingCap_Orientation) {
    state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_Orientation;
    state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_AngularAcceleration;
  }
  if (mDesc.AvailableTrackingCaps & ovrTrackingCap_Position) {
    state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_Position;
    state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_LinearAcceleration;
    state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_StageParameters;
  }
  state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_External;
  state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_MountDetection;
  state.mCapabilityFlags |= VRDisplayCapabilityFlags::Cap_Present;

  mFOVPort[VRDisplayState::Eye_Left] = mDesc.DefaultEyeFov[ovrEye_Left];
  mFOVPort[VRDisplayState::Eye_Right] = mDesc.DefaultEyeFov[ovrEye_Right];

  state.mEyeFOV[VRDisplayState::Eye_Left] = FromFovPort(mFOVPort[VRDisplayState::Eye_Left]);
  state.mEyeFOV[VRDisplayState::Eye_Right] = FromFovPort(mFOVPort[VRDisplayState::Eye_Right]);

  float pixelsPerDisplayPixel = 1.0;
  ovrSizei texSize[2];

  // get eye texture sizes
  for (uint32_t eye = 0; eye < VRDisplayState::NumEyes; eye++) {
    texSize[eye] = ovr_GetFovTextureSize(mSession->Get(), (ovrEyeType)eye, mFOVPort[eye], pixelsPerDisplayPixel);
  }

  // take the max of both for eye resolution
  state.mEyeResolution.width = std::max(texSize[VRDisplayState::Eye_Left].w, texSize[VRDisplayState::Eye_Right].w);
  state.mEyeResolution.height = std::max(texSize[VRDisplayState::Eye_Left].h, texSize[VRDisplayState::Eye_Right].h);

  UpdateEyeParameters();
  UpdateStageParameters();
}

VRDisplayOculus::~VRDisplayOculus() {
  Destroy();
  MOZ_COUNT_DTOR_INHERITED(VRDisplayOculus, VRDisplayHost);
}

void
VRDisplayOculus::Destroy()
{
  StopPresentation();
  mSession = nullptr;
}

void
VRDisplayOculus::UpdateEyeParameters(gfx::Matrix4x4* aHeadToEyeTransforms /* = nullptr */)
{
  // Note this must be called every frame, as the IPD adjustment can be changed
  // by the user during a VR session.
  for (uint32_t eye = 0; eye < VRDisplayState::NumEyes; eye++) {
    // As of Oculus 1.17 SDK, we must use the ovr_GetRenderDesc2 function to return the updated
    // version of ovrEyeRenderDesc.  This is normally done by the Oculus static lib shim, but we
    // need to do this explicitly as we are loading the Oculus runtime dll directly.
    ovrEyeRenderDesc renderDesc = ovr_GetRenderDesc2(mSession->Get(), (ovrEyeType)eye, mFOVPort[eye]);
    VRDisplayState& state = mDisplayInfo.mDisplayState;
    state.mEyeTranslation[eye].x = renderDesc.HmdToEyePose.Position.x;
    state.mEyeTranslation[eye].y = renderDesc.HmdToEyePose.Position.y;
    state.mEyeTranslation[eye].z = renderDesc.HmdToEyePose.Position.z;
    if (aHeadToEyeTransforms) {
      Matrix4x4 pose;
      pose.SetRotationFromQuaternion(gfx::Quaternion(renderDesc.HmdToEyePose.Orientation.x, renderDesc.HmdToEyePose.Orientation.y, renderDesc.HmdToEyePose.Orientation.z, renderDesc.HmdToEyePose.Orientation.w));
      pose.PreTranslate(renderDesc.HmdToEyePose.Position.x, renderDesc.HmdToEyePose.Position.y, renderDesc.HmdToEyePose.Position.z);
      pose.Invert();
      aHeadToEyeTransforms[eye] = pose;
    }
  }
}

void
VRDisplayOculus::UpdateStageParameters()
{
  if (!mSession->IsTrackingReady()) {
    return;
  }
  VRDisplayState& state = mDisplayInfo.mDisplayState;
  ovrVector3f playArea;
  ovrResult res = ovr_GetBoundaryDimensions(mSession->Get(), ovrBoundary_PlayArea, &playArea);
  if (res == ovrSuccess) {
    state.mStageSize.width = playArea.x;
    state.mStageSize.height = playArea.z;
  } else {
    // If we fail, fall back to reasonable defaults.
    // 1m x 1m space
    state.mStageSize.width = 1.0f;
    state.mStageSize.height = 1.0f;
  }

  mEyeHeight = ovr_GetFloat(mSession->Get(), OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);

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
  state.mSittingToStandingTransform[13] = mEyeHeight;
  state.mSittingToStandingTransform[14] = 0.0f;
  state.mSittingToStandingTransform[15] = 1.0f;
}

void
VRDisplayOculus::ZeroSensor()
{
  if (!mSession->IsTrackingReady()) {
    return;
  }
  ovr_RecenterTrackingOrigin(mSession->Get());
  UpdateStageParameters();
}

VRHMDSensorState
VRDisplayOculus::GetSensorState()
{
  VRHMDSensorState result{};
  if (mSession->IsTrackingReady()) {
    gfx::Matrix4x4 headToEyeTransforms[2];
    UpdateEyeParameters(headToEyeTransforms);
    double predictedFrameTime = 0.0f;
    if (gfxPrefs::VRPosePredictionEnabled()) {
      // XXX We might need to call ovr_GetPredictedDisplayTime even if we don't use the result.
      // If we don't call it, the Oculus driver will spew out many warnings...
      predictedFrameTime = ovr_GetPredictedDisplayTime(mSession->Get(), 0);
    }
    result = GetSensorState(predictedFrameTime);
    result.position[1] -= mEyeHeight;
    result.CalcViewMatrices(headToEyeTransforms);
  }
  result.inputFrameID = mDisplayInfo.mFrameId;

  return result;
}

VRHMDSensorState
VRDisplayOculus::GetSensorState(double absTime)
{
  VRHMDSensorState result{};

  ovrTrackingState state = ovr_GetTrackingState(mSession->Get(), absTime, true);
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

    result.flags |= VRDisplayCapabilityFlags::Cap_AngularAcceleration;

    result.angularAcceleration[0] = pose.AngularAcceleration.x;
    result.angularAcceleration[1] = pose.AngularAcceleration.y;
    result.angularAcceleration[2] = pose.AngularAcceleration.z;
  } else {
    // default to an identity quaternion
    result.orientation[3] = 1.0f;
  }

  if (state.StatusFlags & ovrStatus_PositionTracked) {
    result.flags |= VRDisplayCapabilityFlags::Cap_Position;

    result.position[0] = pose.ThePose.Position.x;
    result.position[1] = pose.ThePose.Position.y;
    result.position[2] = pose.ThePose.Position.z;

    result.linearVelocity[0] = pose.LinearVelocity.x;
    result.linearVelocity[1] = pose.LinearVelocity.y;
    result.linearVelocity[2] = pose.LinearVelocity.z;

    result.flags |= VRDisplayCapabilityFlags::Cap_LinearAcceleration;

    result.linearAcceleration[0] = pose.LinearAcceleration.x;
    result.linearAcceleration[1] = pose.LinearAcceleration.y;
    result.linearAcceleration[2] = pose.LinearAcceleration.z;
  }
  result.flags |= VRDisplayCapabilityFlags::Cap_External;
  result.flags |= VRDisplayCapabilityFlags::Cap_MountDetection;
  result.flags |= VRDisplayCapabilityFlags::Cap_Present;

  return result;
}

void
VRDisplayOculus::StartPresentation()
{
  if (!CreateD3DObjects()) {
    return;
  }
  mSession->StartPresentation(IntSize(mDisplayInfo.mDisplayState.mEyeResolution.width * 2, mDisplayInfo.mDisplayState.mEyeResolution.height));
  if (!mSession->IsPresentationReady()) {
    return;
  }

  if (!mQuadVS) {
    if (FAILED(mDevice->CreateVertexShader(sLayerQuadVS.mData, sLayerQuadVS.mLength, nullptr, &mQuadVS))) {
      NS_WARNING("Failed to create vertex shader for Oculus");
      return;
    }
  }

  if (!mQuadPS) {
    if (FAILED(mDevice->CreatePixelShader(sRGBShader.mData, sRGBShader.mLength, nullptr, &mQuadPS))) {
      NS_WARNING("Failed to create pixel shader for Oculus");
      return;
    }
  }

  CD3D11_BUFFER_DESC cBufferDesc(sizeof(layers::VertexShaderConstants),
    D3D11_BIND_CONSTANT_BUFFER,
    D3D11_USAGE_DYNAMIC,
    D3D11_CPU_ACCESS_WRITE);

  if (!mVSConstantBuffer) {
    if (FAILED(mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mVSConstantBuffer)))) {
      NS_WARNING("Failed to vertex shader constant buffer for Oculus");
      return;
    }
  }

  if (!mPSConstantBuffer) {
    cBufferDesc.ByteWidth = sizeof(layers::PixelShaderConstants);
    if (FAILED(mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mPSConstantBuffer)))) {
      NS_WARNING("Failed to pixel shader constant buffer for Oculus");
      return;
    }
  }

  if (!mLinearSamplerState) {
    CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
    if (FAILED(mDevice->CreateSamplerState(&samplerDesc, getter_AddRefs(mLinearSamplerState)))) {
      NS_WARNING("Failed to create sampler state for Oculus");
      return;
    }
  }

  if (!mInputLayout) {
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
  }

  if (!mVertexBuffer) {
    Vertex vertices[] = { { { 0.0, 0.0 } },{ { 1.0, 0.0 } },{ { 0.0, 1.0 } },{ { 1.0, 1.0 } } };
    CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = (void*)vertices;

    if (FAILED(mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mVertexBuffer)))) {
      NS_WARNING("Failed to create vertex buffer for Oculus");
      return;
    }
  }

  memset(&mVSConstants, 0, sizeof(mVSConstants));
  memset(&mPSConstants, 0, sizeof(mPSConstants));
}

void
VRDisplayOculus::StopPresentation()
{
  if (mSession) {
    mSession->StopPresentation();
  }
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

bool
VRDisplayOculus::SubmitFrame(ID3D11Texture2D* aSource,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
  if (!CreateD3DObjects()) {
    return false;
  }

  AutoRestoreRenderState restoreState(this);
  if (!restoreState.IsSuccess()) {
    return false;
  }

  if (!mSession->IsPresentationReady()) {
    return false;
  }
  /**
    * XXX - We should resolve fail the promise returned by
    *       VRDisplay.requestPresent() when the DX11 resources fail allocation
    *       in VRDisplayOculus::StartPresentation().
    *       Bailing out here prevents the crash but content should be aware
    *       that frames are not being presented.
    *       See Bug 1299309.
    **/

  RefPtr<CompositingRenderTargetD3D11> surface = mSession->GetNextRenderTarget();

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

  RefPtr<ID3D11ShaderResourceView> srView;
  HRESULT hr = mDevice->CreateShaderResourceView(aSource, nullptr, getter_AddRefs(srView));
  if (FAILED(hr)) {
    gfxWarning() << "Could not create shader resource view for Oculus: " << hexa(hr);
    return false;
  }
  ID3D11ShaderResourceView* viewPtr = srView.get();
  mContext->PSSetShaderResources(0 /* 0 == TexSlot::RGB */, 1, &viewPtr);
  // XXX Use Constant from TexSlot in CompositorD3D11.cpp?

  ID3D11SamplerState *sampler = mLinearSamplerState;
  mContext->PSSetSamplers(0, 1, &sampler);

  if (!UpdateConstantBuffers()) {
    NS_WARNING("Failed to update constant buffers for Oculus");
    return false;
  }

  mContext->Draw(4, 0);

  ovrResult orv = ovr_CommitTextureSwapChain(mSession->Get(), mSession->GetSwapChain());
  if (orv != ovrSuccess) {
    NS_WARNING("ovr_CommitTextureSwapChain failed.\n");
    return false;
  }

  ovrLayerEyeFov layer;
  memset(&layer, 0, sizeof(layer));
  layer.Header.Type = ovrLayerType_EyeFov;
  layer.Header.Flags = 0;
  layer.ColorTexture[0] = mSession->GetSwapChain();
  layer.ColorTexture[1] = nullptr;
  layer.Fov[0] = mFOVPort[0];
  layer.Fov[1] = mFOVPort[1];
  layer.Viewport[0].Pos.x = aSize.width * aLeftEyeRect.X();
  layer.Viewport[0].Pos.y = aSize.height * aLeftEyeRect.Y();
  layer.Viewport[0].Size.w = aSize.width * aLeftEyeRect.Width();
  layer.Viewport[0].Size.h = aSize.height * aLeftEyeRect.Height();
  layer.Viewport[1].Pos.x = aSize.width * aRightEyeRect.X();
  layer.Viewport[1].Pos.y = aSize.height * aRightEyeRect.Y();
  layer.Viewport[1].Size.w = aSize.width * aRightEyeRect.Width();
  layer.Viewport[1].Size.h = aSize.height * aRightEyeRect.Height();

  const VRHMDSensorState& sensorState = mDisplayInfo.GetSensorState();
  gfx::Matrix4x4 matView[2];
  memcpy(matView[0].components, sensorState.leftViewMatrix, sizeof(sensorState.leftViewMatrix));
  memcpy(matView[1].components, sensorState.rightViewMatrix, sizeof(sensorState.rightViewMatrix));

  for (uint32_t i = 0; i < 2; ++i) {
    Point3D eyeTranslation;
    Quaternion eyeRotation;
    Point3D eyeScale;
    if (!matView[i].Decompose(eyeTranslation, eyeRotation, eyeScale)) {
      NS_WARNING("Failed to decompose eye pose matrix for Oculus");
    }
    layer.RenderPose[i].Orientation.x = eyeRotation.x;
    layer.RenderPose[i].Orientation.y = eyeRotation.y;
    layer.RenderPose[i].Orientation.z = eyeRotation.z;
    layer.RenderPose[i].Orientation.w = eyeRotation.w;
    layer.RenderPose[i].Position.x = eyeTranslation.x;
    layer.RenderPose[i].Position.y = eyeTranslation.y;
    layer.RenderPose[i].Position.z = eyeTranslation.z;
  }

  ovrLayerHeader *layers = &layer.Header;
  orv = ovr_SubmitFrame(mSession->Get(), mDisplayInfo.mFrameId, nullptr, &layers, 1);
  // ovr_SubmitFrame will fail during the Oculus health and safety warning.
  // and will start succeeding once the warning has been dismissed by the user.

  if (!OVR_UNQUALIFIED_SUCCESS(orv)) {
    /**
     * We wish to throttle the framerate for any case that the rendered
     * result is not visible.  In some cases, such as during the Oculus
     * "health and safety warning", orv will be > 0 (OVR_SUCCESS but not
     * OVR_UNQUALIFIED_SUCCESS) and ovr_SubmitFrame will not block.
     * In this case, returning true would have resulted in an unthrottled
     * render loop hiting excessive frame rates and consuming resources.
     */
    return false;
  }

  mSession->mSubmitThread = mSubmitThread;
  return true;
}

void
VRDisplayOculus::Refresh()
{
  mDisplayInfo.mDisplayState.mIsConnected = mSession->IsTrackingReady();
  mDisplayInfo.mDisplayState.mIsMounted = mSession->IsMounted();
}

VRControllerOculus::VRControllerOculus(dom::GamepadHand aHand, uint32_t aDisplayID)
  : VRControllerHost(VRDeviceType::Oculus, aHand, aDisplayID)
  , mIndexTrigger(0.0f)
  , mHandTrigger(0.0f)
  , mVibrateThread(nullptr)
  , mIsVibrateStopped(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerOculus, VRControllerHost);

  VRControllerState& state = mControllerInfo.mControllerState;

  char* touchID = "";
  switch (aHand) {
    case dom::GamepadHand::Left:
      touchID = "Oculus Touch (Left)";
      break;
    case dom::GamepadHand::Right:
      touchID = "Oculus Touch (Right)";
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }

  strncpy(state.mControllerName, touchID, kVRControllerNameMaxLen);

  MOZ_ASSERT(kNumOculusButton ==
             static_cast<uint32_t>(OculusLeftControllerButtonType::NumButtonType)
             && kNumOculusButton ==
             static_cast<uint32_t>(OculusRightControllerButtonType::NumButtonType));

  state.mNumButtons = kNumOculusButton;
  state.mNumAxes = static_cast<uint32_t>(
                   OculusControllerAxisType::NumVRControllerAxisType);
  state.mNumHaptics = kNumOculusHaptcs;
}

float
VRControllerOculus::GetAxisMove(uint32_t aAxis)
{
  return mAxisMove[aAxis];
}

void
VRControllerOculus::SetAxisMove(uint32_t aAxis, float aValue)
{
  mAxisMove[aAxis] = aValue;
}

float
VRControllerOculus::GetIndexTrigger()
{
  return mIndexTrigger;
}

void
VRControllerOculus::SetIndexTrigger(float aValue)
{
  mIndexTrigger = aValue;
}

float
VRControllerOculus::GetHandTrigger()
{
  return mHandTrigger;
}

void
VRControllerOculus::SetHandTrigger(float aValue)
{
  mHandTrigger = aValue;
}

VRControllerOculus::~VRControllerOculus()
{
  ShutdownVibrateHapticThread();
  MOZ_COUNT_DTOR_INHERITED(VRControllerOculus, VRControllerHost);
}

void
VRControllerOculus::UpdateVibrateHaptic(ovrSession aSession,
                                        uint32_t aHapticIndex,
                                        double aIntensity,
                                        double aDuration,
                                        uint64_t aVibrateIndex,
                                        const VRManagerPromise& aPromise)
{
  // UpdateVibrateHaptic() only can be called by mVibrateThread
  MOZ_ASSERT(mVibrateThread->GetThread() == NS_GetCurrentThread());

  // It has been interrupted by loss focus.
  if (mIsVibrateStopped) {
    VibrateHapticComplete(aSession, aPromise, true);
    return;
  }
  // Avoid the previous vibrate event to override the new one.
  if (mVibrateIndex != aVibrateIndex) {
    VibrateHapticComplete(aSession, aPromise, false);
    return;
  }

  const double duration = (aIntensity == 0) ? 0 : aDuration;
  // Vibration amplitude in the [0.0, 1.0] range.
  const float amplitude = aIntensity > 1.0 ? 1.0 : aIntensity;
  // Vibration is enabled by specifying the frequency.
  // Specifying 0.0f will disable the vibration, 0.5f will vibrate at 160Hz,
  // and 1.0f will vibrate at 320Hz.
  const float frequency = (duration > 0) ? 1.0f : 0.0f;
  ovrControllerType hand;

  switch (GetHand()) {
    case GamepadHand::Left:
      hand = ovrControllerType::ovrControllerType_LTouch;
      break;
    case GamepadHand::Right:
      hand = ovrControllerType::ovrControllerType_RTouch;
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }

  // Oculus Touch only can get the response from ovr_SetControllerVibration()
  // at the presenting mode.
  ovrResult result = ovr_SetControllerVibration(aSession, hand, frequency,
                                                (frequency == 0.0f) ? 0.0f : amplitude);
  if (result != ovrSuccess) {
    printf_stderr("%s hand ovr_SetControllerVibration skipped.\n",
                  GamepadHandValues::strings[uint32_t(GetHand())].value);
  }

  // In Oculus dev doc, it mentions vibration lasts for a maximum of 2.5 seconds
  // at ovr_SetControllerVibration(), but we found 2.450 sec is more close to the
  // real looping use case.
  const double kVibrateRate = 2450.0;
  const double remainingTime = (duration > kVibrateRate)
                                ? (duration - kVibrateRate) : duration;

  if (remainingTime) {
    MOZ_ASSERT(mVibrateThread);

    RefPtr<Runnable> runnable =
      NewRunnableMethod<ovrSession, uint32_t, double, double, uint64_t,
        StoreCopyPassByConstLRef<VRManagerPromise>>(
          "VRControllerOculus::UpdateVibrateHaptic",
          this, &VRControllerOculus::UpdateVibrateHaptic, aSession,
          aHapticIndex, aIntensity, (duration > kVibrateRate) ? remainingTime : 0, aVibrateIndex, aPromise);
    mVibrateThread->PostDelayedTask(runnable.forget(), (duration > kVibrateRate) ? kVibrateRate : remainingTime);
  } else {
    VibrateHapticComplete(aSession, aPromise, true);
  }
}

void
VRControllerOculus::VibrateHapticComplete(ovrSession aSession, const VRManagerPromise& aPromise,
                                          bool aStop)
{
  if (aStop) {
    ovrControllerType hand;

    switch (GetHand()) {
      case GamepadHand::Left:
        hand = ovrControllerType::ovrControllerType_LTouch;
        break;
      case GamepadHand::Right:
        hand = ovrControllerType::ovrControllerType_RTouch;
        break;
      default:
        MOZ_ASSERT(false);
        break;
    }

    ovrResult result = ovr_SetControllerVibration(aSession, hand, 0.0f, 0.0f);
    if (result != ovrSuccess) {
      printf_stderr("%s Haptics skipped.\n",
                    GamepadHandValues::strings[uint32_t(GetHand())].value);
    }
  }

  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);

  VRListenerThreadHolder::Loop()->PostTask(
    NewRunnableMethod<StoreCopyPassByConstLRef<VRManagerPromise>>(
      "VRManager::NotifyVibrateHapticCompleted",
      vm, &VRManager::NotifyVibrateHapticCompleted, aPromise));
}

void
VRControllerOculus::VibrateHaptic(ovrSession aSession,
                                  uint32_t aHapticIndex,
                                  double aIntensity,
                                  double aDuration,
                                  const VRManagerPromise& aPromise)
{
  // Spinning up the haptics thread at the first haptics call.
  if (!mVibrateThread) {
    mVibrateThread = new VRThread(NS_LITERAL_CSTRING("Oculus_Vibration"));
  }
  mVibrateThread->Start();
  ++mVibrateIndex;
  mIsVibrateStopped = false;

  RefPtr<Runnable> runnable =
    NewRunnableMethod<ovrSession, uint32_t, double, double, uint64_t,
      StoreCopyPassByConstLRef<VRManagerPromise>>(
        "VRControllerOculus::UpdateVibrateHaptic",
        this, &VRControllerOculus::UpdateVibrateHaptic, aSession,
        aHapticIndex, aIntensity, aDuration, mVibrateIndex, aPromise);
  mVibrateThread->PostTask(runnable.forget());
}

void
VRControllerOculus::StopVibrateHaptic()
{
  mIsVibrateStopped = true;
}

void
VRControllerOculus::ShutdownVibrateHapticThread()
{
  StopVibrateHaptic();
  if (mVibrateThread) {
    mVibrateThread->Shutdown();
    mVibrateThread = nullptr;
  }
}

/*static*/ already_AddRefed<VRSystemManagerOculus>
VRSystemManagerOculus::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROculusEnabled())
  {
    return nullptr;
  }

  RefPtr<VRSystemManagerOculus> manager = new VRSystemManagerOculus();
  return manager.forget();
}

VRSystemManagerOculus::VRSystemManagerOculus()
  : mSession(nullptr)
{
}

void
VRSystemManagerOculus::Destroy()
{
  Shutdown();
  mSession = nullptr;
}

void
VRSystemManagerOculus::Shutdown()
{
  if (mSession) {
    mSession->StopTracking();
  }
  RemoveControllers();
  if (mDisplay) {
    mDisplay->Destroy();
  }
  mDisplay = nullptr;
}

void
VRSystemManagerOculus::NotifyVSync()
{
  VRSystemManager::NotifyVSync();
  if (!mSession) {
    return;
  }
  mSession->Refresh();
  if (mDisplay) {
    mDisplay->Refresh();
  }
  // Detect disconnection
  if (!mSession->IsTrackingReady()) {
    // No HMD connected
    mDisplay = nullptr;
    // Prevent enumeration from continuously running
    // after the headset is disconnected.
    mSession->StopTracking();
  }
}

bool
VRSystemManagerOculus::ShouldInhibitEnumeration()
{
  if (VRSystemManager::ShouldInhibitEnumeration()) {
    return true;
  }
  if (mDisplay) {
    // When we find an Oculus VR device, don't
    // allow any further enumeration as it
    // may get picked up redundantly by other
    // API's such as OpenVR.
    return true;
  }
  if (mSession && mSession->IsQuitTimeoutActive()) {
    // When we are responding to ShouldQuit, we return true here
    // to prevent further enumeration by other VRSystemManager's such as
    // VRSystemManagerOpenVR which would also enumerate the connected Oculus
    // HMD, resulting in interference with the Oculus runtime software updates.
    return true;
  }
  return false;
}

void
VRSystemManagerOculus::Enumerate()
{
  if (!mSession) {
    mSession = new VROculusSession();
  }
  mSession->StartTracking();
  if (mDisplay == nullptr && mSession->IsTrackingReady()) {
    // HMD Detected
    mDisplay = new VRDisplayOculus(mSession);
  }
  if (mDisplay == nullptr) {
    // Prevent enumeration from continuously running
    // when a headset was not found.
    mSession->StopTracking();
  }
}

void
VRSystemManagerOculus::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (mDisplay) {
    aHMDResult.AppendElement(mDisplay);
  }
}

bool
VRSystemManagerOculus::GetIsPresenting()
{
  if (mDisplay) {
    VRDisplayInfo displayInfo(mDisplay->GetDisplayInfo());
    return displayInfo.GetPresentingGroups() != 0;
  }

  return false;
}

void
VRSystemManagerOculus::HandleInput()
{
  // The session is available after VRDisplay is created
  // at GetHMDs().
  if (!mSession || !mSession->IsTrackingReady()) {
    return;
  }

  RefPtr<impl::VRControllerOculus> controller;
  ovrInputState inputState;
  uint32_t axis = 0;
  const bool hasInputState = ovr_GetInputState(mSession->Get(), ovrControllerType_Touch,
                                               &inputState) == ovrSuccess;

  if (!hasInputState) {
    return;
  }

  for (uint32_t i = 0; i < mOculusController.Length(); ++i) {
    controller = mOculusController[i];
    const GamepadHand hand = controller->GetHand();
    const uint32_t handIdx = static_cast<uint32_t>(hand) - 1;
    uint32_t buttonIdx = 0;
    ovrHandType oculusHandType;

    switch (hand) {
      case dom::GamepadHand::Left:
        oculusHandType = ovrHand_Left;
        HandleButtonPress(i, buttonIdx, ovrButton_LThumb, inputState.Buttons,
                          inputState.Touches);
        ++buttonIdx;
        HandleIndexTriggerPress(i, buttonIdx, inputState.IndexTrigger[handIdx], ovrTouch_LIndexTrigger, inputState.Touches);
        ++buttonIdx;
        HandleHandTriggerPress(i, buttonIdx, inputState.HandTrigger[handIdx]);
        ++buttonIdx;
        HandleButtonPress(i, buttonIdx, ovrButton_X, inputState.Buttons,
                          inputState.Touches);
        ++buttonIdx;
        HandleButtonPress(i, buttonIdx, ovrButton_Y, inputState.Buttons,
                          inputState.Touches);
        ++buttonIdx;
        HandleTouchEvent(i, buttonIdx, ovrTouch_LThumbRest, inputState.Touches);
        ++buttonIdx;
        break;
      case dom::GamepadHand::Right:
        oculusHandType = ovrHand_Right;
        HandleButtonPress(i, buttonIdx, ovrButton_RThumb, inputState.Buttons,
                          inputState.Touches);
        ++buttonIdx;
        HandleIndexTriggerPress(i, buttonIdx, inputState.IndexTrigger[handIdx], ovrTouch_RIndexTrigger, inputState.Touches);
        ++buttonIdx;
        HandleHandTriggerPress(i, buttonIdx, inputState.HandTrigger[handIdx]);
        ++buttonIdx;
        HandleButtonPress(i, buttonIdx, ovrButton_A, inputState.Buttons,
                          inputState.Touches);
        ++buttonIdx;
        HandleButtonPress(i, buttonIdx, ovrButton_B, inputState.Buttons,
                          inputState.Touches);
        ++buttonIdx;
        HandleTouchEvent(i, buttonIdx, ovrTouch_RThumbRest, inputState.Touches);
        ++buttonIdx;
        break;
      default:
        MOZ_ASSERT(false);
        break;
    }
    controller->SetButtonPressed(inputState.Buttons);
    controller->SetButtonTouched(inputState.Touches);

    axis = static_cast<uint32_t>(OculusControllerAxisType::ThumbstickXAxis);
    HandleAxisMove(i, axis, inputState.Thumbstick[oculusHandType].x);

    axis = static_cast<uint32_t>(OculusControllerAxisType::ThumbstickYAxis);
    HandleAxisMove(i, axis, -inputState.Thumbstick[oculusHandType].y);

    // Process pose state.
    GamepadPoseState poseState;
    GetControllerPoseState(handIdx, poseState);
    HandlePoseTracking(i, poseState, controller);
  }
}

void
VRSystemManagerOculus::GetControllerPoseState(uint32_t aHandIdx, GamepadPoseState& aPoseState,
                                              bool aForceUpdate)
{
    ovrTrackingState state = ovr_GetTrackingState(mSession->Get(), 0.0, false);

    // HandPoses is ordered by ovrControllerType_LTouch and ovrControllerType_RTouch,
    // therefore, we can't get its state by the index of mOculusController.
    ovrPoseStatef& pose(state.HandPoses[aHandIdx]);

    if (aForceUpdate || state.HandStatusFlags[aHandIdx] & ovrStatus_OrientationTracked) {
      aPoseState.flags |= GamepadCapabilityFlags::Cap_Orientation;
      aPoseState.orientation[0] = pose.ThePose.Orientation.x;
      aPoseState.orientation[1] = pose.ThePose.Orientation.y;
      aPoseState.orientation[2] = pose.ThePose.Orientation.z;
      aPoseState.orientation[3] = pose.ThePose.Orientation.w;
      aPoseState.angularVelocity[0] = pose.AngularVelocity.x;
      aPoseState.angularVelocity[1] = pose.AngularVelocity.y;
      aPoseState.angularVelocity[2] = pose.AngularVelocity.z;

      aPoseState.flags |= GamepadCapabilityFlags::Cap_AngularAcceleration;
      aPoseState.angularAcceleration[0] = pose.AngularAcceleration.x;
      aPoseState.angularAcceleration[1] = pose.AngularAcceleration.y;
      aPoseState.angularAcceleration[2] = pose.AngularAcceleration.z;
      aPoseState.isOrientationValid = true;
    }
    if (aForceUpdate || state.HandStatusFlags[aHandIdx] & ovrStatus_PositionTracked) {
      aPoseState.flags |= GamepadCapabilityFlags::Cap_Position;
      aPoseState.position[0] = pose.ThePose.Position.x;
      aPoseState.position[1] = pose.ThePose.Position.y;
      aPoseState.position[2] = pose.ThePose.Position.z;
      aPoseState.linearVelocity[0] = pose.LinearVelocity.x;
      aPoseState.linearVelocity[1] = pose.LinearVelocity.y;
      aPoseState.linearVelocity[2] = pose.LinearVelocity.z;

      aPoseState.flags |= GamepadCapabilityFlags::Cap_LinearAcceleration;
      aPoseState.linearAcceleration[0] = pose.LinearAcceleration.x;
      aPoseState.linearAcceleration[1] = pose.LinearAcceleration.y;
      aPoseState.linearAcceleration[2] = pose.LinearAcceleration.z;

      float eyeHeight = ovr_GetFloat(mSession->Get(), OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
      aPoseState.position[1] -= eyeHeight;
      aPoseState.isPositionValid = true;
    }
}

void
VRSystemManagerOculus::HandleButtonPress(uint32_t aControllerIdx,
                                         uint32_t aButton,
                                         uint64_t aButtonMask,
                                         uint64_t aButtonPressed,
                                         uint64_t aButtonTouched)
{
  RefPtr<impl::VRControllerOculus> controller(mOculusController[aControllerIdx]);
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
VRSystemManagerOculus::HandleIndexTriggerPress(uint32_t aControllerIdx,
                                               uint32_t aButton,
                                               float aValue,
                                               uint64_t aTouchMask,
                                               uint64_t aButtonTouched)
{
  RefPtr<impl::VRControllerOculus> controller(mOculusController[aControllerIdx]);
  MOZ_ASSERT(controller);

  const uint64_t touchedDiff = (controller->GetButtonTouched() ^ aButtonTouched);
  const float oldValue = controller->GetIndexTrigger();
  // We prefer to let developers to set their own threshold for the adjustment.
  // Therefore, we don't check ButtonPressed and ButtonTouched with TouchMask here.
  // we just check the button value is larger than the threshold value or not.
  const float threshold = gfxPrefs::VRControllerTriggerThreshold();

  // Avoid sending duplicated events in IPC channels.
  if (oldValue != aValue ||
      touchedDiff & aTouchMask) {
    NewButtonEvent(aControllerIdx, aButton, aValue > threshold,
                   aButtonTouched & aTouchMask, aValue);
    controller->SetIndexTrigger(aValue);
  }
}

void
VRSystemManagerOculus::HandleHandTriggerPress(uint32_t aControllerIdx,
                                              uint32_t aButton,
                                              float aValue)
{
  RefPtr<impl::VRControllerOculus> controller(mOculusController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const float oldValue = controller->GetHandTrigger();
  // We prefer to let developers to set their own threshold for the adjustment.
  // Therefore, we don't check ButtonPressed and ButtonTouched with TouchMask here.
  // we just check the button value is larger than the threshold value or not.
  const float threshold = gfxPrefs::VRControllerTriggerThreshold();

  // Avoid sending duplicated events in IPC channels.
  if (oldValue != aValue) {
    NewButtonEvent(aControllerIdx, aButton, aValue > threshold,
                   aValue > threshold, aValue);
    controller->SetHandTrigger(aValue);
  }
}

void
VRSystemManagerOculus::HandleTouchEvent(uint32_t aControllerIdx, uint32_t aButton,
                                        uint64_t aTouchMask, uint64_t aButtonTouched)
{
  RefPtr<impl::VRControllerOculus> controller(mOculusController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const uint64_t touchedDiff = (controller->GetButtonTouched() ^ aButtonTouched);

  if (touchedDiff & aTouchMask) {
    NewButtonEvent(aControllerIdx, aButton, false, aTouchMask & aButtonTouched, 0.0f);
  }
}

void
VRSystemManagerOculus::HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                                      float aValue)
{
  RefPtr<impl::VRControllerOculus> controller(mOculusController[aControllerIdx]);
  MOZ_ASSERT(controller);
  float value = aValue;

  if (abs(aValue) < 0.0000009f) {
    value = 0.0f; // Clear noise signal
  }

  if (controller->GetAxisMove(aAxis) != value) {
    NewAxisMove(aControllerIdx, aAxis, value);
    controller->SetAxisMove(aAxis, value);
  }
}

void
VRSystemManagerOculus::HandlePoseTracking(uint32_t aControllerIdx,
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
VRSystemManagerOculus::VibrateHaptic(uint32_t aControllerIdx,
                                     uint32_t aHapticIndex,
                                     double aIntensity,
                                     double aDuration,
                                     const VRManagerPromise& aPromise)
{
  // The session is available after VRDisplay is created
  // at GetHMDs().
  if (!mSession || !mSession->IsTrackingReady() ||
      (aControllerIdx >= mOculusController.Length())) {
    return;
  }

  RefPtr<impl::VRControllerOculus> controller = mOculusController[aControllerIdx];
  MOZ_ASSERT(controller);

  controller->VibrateHaptic(mSession->Get(), aHapticIndex, aIntensity, aDuration, aPromise);
}

void
VRSystemManagerOculus::StopVibrateHaptic(uint32_t aControllerIdx)
{
  // The session is available after VRDisplay is created
  // at GetHMDs().
  if (!mSession || !mSession->IsTrackingReady() ||
      (aControllerIdx >= mOculusController.Length())) {
    return;
  }

  RefPtr<impl::VRControllerOculus> controller = mOculusController[aControllerIdx];
  MOZ_ASSERT(controller);

  controller->StopVibrateHaptic();
}

void
VRSystemManagerOculus::GetControllers(nsTArray<RefPtr<VRControllerHost>>&
                                      aControllerResult)
{
  aControllerResult.Clear();
  for (uint32_t i = 0; i < mOculusController.Length(); ++i) {
    aControllerResult.AppendElement(mOculusController[i]);
  }
}

void
VRSystemManagerOculus::ScanForControllers()
{
  // mSession is available after VRDisplay is created
  // at GetHMDs().
  if (!mSession || !mSession->IsTrackingReady()) {
    return;
  }

  ovrInputState inputState;
  bool hasInputState = ovr_GetInputState(mSession->Get(), ovrControllerType_Touch,
                                         &inputState) == ovrSuccess;

  if (!hasInputState) {
    return;
  }

  ovrControllerType activeControllerArray[2];
  uint32_t newControllerCount = 0;

  if (inputState.ControllerType & ovrControllerType_LTouch) {
    activeControllerArray[newControllerCount] = ovrControllerType_LTouch;
    ++newControllerCount;
  }

  if (inputState.ControllerType & ovrControllerType_RTouch) {
    activeControllerArray[newControllerCount] = ovrControllerType_RTouch;
    ++newControllerCount;
  }

  if (newControllerCount != mControllerCount) {
    RemoveControllers();

    // Re-adding controllers to VRControllerManager.
    for (uint32_t i = 0; i < newControllerCount; ++i) {
      GamepadHand hand;

      switch (activeControllerArray[i]) {
        case ovrControllerType::ovrControllerType_LTouch:
          hand = GamepadHand::Left;
          break;
        case ovrControllerType::ovrControllerType_RTouch:
          hand = GamepadHand::Right;
          break;
      }
      RefPtr<VRControllerOculus> oculusController = new VRControllerOculus(hand,
                                                      mDisplay->GetDisplayInfo().GetDisplayID());
      mOculusController.AppendElement(oculusController);

      // Not already present, add it.
      AddGamepad(oculusController->GetControllerInfo());

      // Process pose state.
      // We wanna Oculus Touch has the right position when it shows up,
      // so we force to update the pose no matter if it has OrientationTracked
      // or PositionTracked.
      const uint32_t handIdx = static_cast<uint32_t>(hand) - 1;
      GamepadPoseState poseState;
      GetControllerPoseState(handIdx, poseState, true);
      HandlePoseTracking(i, poseState, oculusController);

      ++mControllerCount;
    }
  }
}

void
VRSystemManagerOculus::RemoveControllers()
{
  // controller count is changed, removing the existing gamepads first.
  for (uint32_t i = 0; i < mOculusController.Length(); ++i) {
    mOculusController[i]->ShutdownVibrateHapticThread();
    RemoveGamepad(i);
  }

  mOculusController.Clear();
  mControllerCount = 0;
}
