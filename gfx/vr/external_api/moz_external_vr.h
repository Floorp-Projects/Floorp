/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_EXTERNAL_API_H
#define GFX_VR_EXTERNAL_API_H

#define GFX_VR_EIGHTCC(c1, c2, c3, c4, c5, c6, c7, c8)                  \
  ((uint64_t)(c1) << 56 | (uint64_t)(c2) << 48 | (uint64_t)(c3) << 40 | \
   (uint64_t)(c4) << 32 | (uint64_t)(c5) << 24 | (uint64_t)(c6) << 16 | \
   (uint64_t)(c7) << 8 | (uint64_t)(c8))

#ifdef MOZILLA_INTERNAL_API
#  define __STDC_WANT_LIB_EXT1__ 1
// __STDC_WANT_LIB_EXT1__ required for memcpy_s
#  include <stdlib.h>
#  include <string.h>
#  include "mozilla/TypedEnumBits.h"
#  include "mozilla/gfx/2D.h"
#  include <stddef.h>
#  include <stdint.h>
#  include <type_traits>
#endif  // MOZILLA_INTERNAL_API

#if defined(__ANDROID__)
#  include <pthread.h>
#endif  // defined(__ANDROID__)

#include <cstdint>
#include <type_traits>

namespace mozilla {
#ifdef MOZILLA_INTERNAL_API
namespace dom {
enum class GamepadHand : uint8_t;
enum class GamepadCapabilityFlags : uint16_t;
}  // namespace dom
#endif  //  MOZILLA_INTERNAL_API
namespace gfx {

// If there is any change of "SHMEM_VERSION" or "kVRExternalVersion",
// we need to change both of them at the same time.

// TODO: we might need to use different names for the mutexes
// and mapped files if we have both release and nightlies
// running at the same time? Or...what if we have multiple
// release builds running on same machine? (Bug 1563232)
#define SHMEM_VERSION "0.0.11"
static const int32_t kVRExternalVersion = 18;

// We assign VR presentations to groups with a bitmask.
// Currently, we will only display either content or chrome.
// Later, we will have more groups to support VR home spaces and
// multitasking environments.
// These values are not exposed to regular content and only affect
// chrome-only API's.  They may be changed at any time.
static const uint32_t kVRGroupNone = 0;
static const uint32_t kVRGroupContent = 1 << 0;
static const uint32_t kVRGroupChrome = 1 << 1;
static const uint32_t kVRGroupAll = 0xffffffff;

static const int kVRDisplayNameMaxLen = 256;
static const int kVRControllerNameMaxLen = 256;
static const int kVRControllerMaxCount = 16;
static const int kVRControllerMaxButtons = 64;
static const int kVRControllerMaxAxis = 16;
static const int kVRLayerMaxCount = 8;
static const int kVRHapticsMaxCount = 32;

#if defined(__ANDROID__)
typedef uint64_t VRLayerTextureHandle;
#elif defined(XP_MACOSX)
typedef uint32_t VRLayerTextureHandle;
#else
typedef void* VRLayerTextureHandle;
#endif

struct Point3D_POD {
  float x;
  float y;
  float z;
};

struct IntSize_POD {
  int32_t width;
  int32_t height;
};

struct FloatSize_POD {
  float width;
  float height;
};

#ifndef MOZILLA_INTERNAL_API

enum class ControllerHand : uint8_t { _empty, Left, Right, EndGuard_ };

enum class ControllerCapabilityFlags : uint16_t {
  Cap_None = 0,
  /**
   * Cap_Position is set if the Gamepad is capable of tracking its position.
   */
  Cap_Position = 1 << 1,
  /**
   * Cap_Orientation is set if the Gamepad is capable of tracking its
   * orientation.
   */
  Cap_Orientation = 1 << 2,
  /**
   * Cap_AngularAcceleration is set if the Gamepad is capable of tracking its
   * angular acceleration.
   */
  Cap_AngularAcceleration = 1 << 3,
  /**
   * Cap_LinearAcceleration is set if the Gamepad is capable of tracking its
   * linear acceleration.
   */
  Cap_LinearAcceleration = 1 << 4,
  /**
   * Cap_TargetRaySpacePosition is set if the Gamepad has a grip space position.
   */
  Cap_GripSpacePosition = 1 << 5,
  /**
   * Cap_PositionEmulated is set if the XRInputSoruce is capable of setting a
   * emulated position (e.g. neck model) even if still doesn't support 6DOF
   * tracking.
   */
  Cap_PositionEmulated = 1 << 6,
  /**
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 7) - 1
};

#endif  // ifndef MOZILLA_INTERNAL_API

enum class VRControllerType : uint8_t {
  _empty,
  HTCVive,
  HTCViveCosmos,
  HTCViveFocus,
  HTCViveFocusPlus,
  MSMR,
  ValveIndex,
  OculusGo,
  OculusTouch,
  OculusTouch2,
  PicoGaze,
  PicoG2,
  PicoNeo2,
  _end
};

enum class TargetRayMode : uint8_t { Gaze, TrackedPointer, Screen };

enum class GamepadMappingType : uint8_t { _empty, Standard, XRStandard };

enum class VRDisplayBlendMode : uint8_t { Opaque, Additive, AlphaBlend };

enum class VRDisplayCapabilityFlags : uint16_t {
  Cap_None = 0,
  /**
   * Cap_Position is set if the VRDisplay is capable of tracking its position.
   */
  Cap_Position = 1 << 1,
  /**
   * Cap_Orientation is set if the VRDisplay is capable of tracking its
   * orientation.
   */
  Cap_Orientation = 1 << 2,
  /**
   * Cap_Present is set if the VRDisplay is capable of presenting content to an
   * HMD or similar device.  Can be used to indicate "magic window" devices that
   * are capable of 6DoF tracking but for which requestPresent is not
   * meaningful. If false then calls to requestPresent should always fail, and
   * getEyeParameters should return null.
   */
  Cap_Present = 1 << 3,
  /**
   * Cap_External is set if the VRDisplay is separate from the device's
   * primary display. If presenting VR content will obscure
   * other content on the device, this should be un-set. When
   * un-set, the application should not attempt to mirror VR content
   * or update non-VR UI because that content will not be visible.
   */
  Cap_External = 1 << 4,
  /**
   * Cap_AngularAcceleration is set if the VRDisplay is capable of tracking its
   * angular acceleration.
   */
  Cap_AngularAcceleration = 1 << 5,
  /**
   * Cap_LinearAcceleration is set if the VRDisplay is capable of tracking its
   * linear acceleration.
   */
  Cap_LinearAcceleration = 1 << 6,
  /**
   * Cap_StageParameters is set if the VRDisplay is capable of room scale VR
   * and can report the StageParameters to describe the space.
   */
  Cap_StageParameters = 1 << 7,
  /**
   * Cap_MountDetection is set if the VRDisplay is capable of sensing when the
   * user is wearing the device.
   */
  Cap_MountDetection = 1 << 8,
  /**
   * Cap_PositionEmulated is set if the VRDisplay is capable of setting a
   * emulated position (e.g. neck model) even if still doesn't support 6DOF
   * tracking.
   */
  Cap_PositionEmulated = 1 << 9,
  /**
   * Cap_Inline is set if the device can be used for WebXR inline sessions
   * where the content is displayed within an element on the page.
   */
  Cap_Inline = 1 << 10,
  /**
   * Cap_ImmersiveVR is set if the device can give exclusive access to the
   * XR device display and that content is not intended to be integrated
   * with the user's environment
   */
  Cap_ImmersiveVR = 1 << 11,
  /**
   * Cap_ImmersiveAR is set if the device can give exclusive access to the
   * XR device display and that content is intended to be integrated with
   * the user's environment.
   */
  Cap_ImmersiveAR = 1 << 12,
  /**
   * Cap_UseDepthValues is set if the device will use the depth values of the
   * submitted frames if provided.  How the depth values are used is determined
   * by the VR runtime.  Often the depth is used for occlusion of system UI
   * or to enable more effective asynchronous reprojection of frames.
   */
  Cap_UseDepthValues = 1 << 13,
  /**
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 14) - 1
};

#ifdef MOZILLA_INTERNAL_API
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VRDisplayCapabilityFlags)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VRDisplayBlendMode)
#endif  // MOZILLA_INTERNAL_API

struct VRPose {
  float orientation[4];
  float position[3];
  float angularVelocity[3];
  float angularAcceleration[3];
  float linearVelocity[3];
  float linearAcceleration[3];
};

struct VRHMDSensorState {
  uint64_t inputFrameID;
  double timestamp;
  VRDisplayCapabilityFlags flags;

  // These members will only change with inputFrameID:
  VRPose pose;
  float leftViewMatrix[16];
  float rightViewMatrix[16];

#ifdef MOZILLA_INTERNAL_API

  void Clear() { memset(this, 0, sizeof(VRHMDSensorState)); }

  bool operator==(const VRHMDSensorState& other) const {
    return inputFrameID == other.inputFrameID && timestamp == other.timestamp;
  }

  bool operator!=(const VRHMDSensorState& other) const {
    return !(*this == other);
  }

  void CalcViewMatrices(const gfx::Matrix4x4* aHeadToEyeTransforms);

#endif  // MOZILLA_INTERNAL_API
};

struct VRFieldOfView {
  double upDegrees;
  double rightDegrees;
  double downDegrees;
  double leftDegrees;

#ifdef MOZILLA_INTERNAL_API

  VRFieldOfView() = default;
  VRFieldOfView(double up, double right, double down, double left)
      : upDegrees(up),
        rightDegrees(right),
        downDegrees(down),
        leftDegrees(left) {}

  void SetFromTanRadians(double up, double right, double down, double left) {
    upDegrees = atan(up) * 180.0 / M_PI;
    rightDegrees = atan(right) * 180.0 / M_PI;
    downDegrees = atan(down) * 180.0 / M_PI;
    leftDegrees = atan(left) * 180.0 / M_PI;
  }

  bool operator==(const VRFieldOfView& other) const {
    return other.upDegrees == upDegrees && other.downDegrees == downDegrees &&
           other.rightDegrees == rightDegrees &&
           other.leftDegrees == leftDegrees;
  }

  bool operator!=(const VRFieldOfView& other) const {
    return !(*this == other);
  }

  bool IsZero() const {
    return upDegrees == 0.0 || rightDegrees == 0.0 || downDegrees == 0.0 ||
           leftDegrees == 0.0;
  }

  Matrix4x4 ConstructProjectionMatrix(float zNear, float zFar,
                                      bool rightHanded) const;

#endif  // MOZILLA_INTERNAL_API
};

struct VRDisplayState {
  enum Eye { Eye_Left, Eye_Right, NumEyes };

  // When true, indicates that the VR service has shut down
  bool shutdown;
  // Minimum number of milliseconds to wait before attempting
  // to start the VR service again
  uint32_t minRestartInterval;
  char displayName[kVRDisplayNameMaxLen];
  // eight byte character code identifier
  // LSB first, so "ABCDEFGH" -> ('H'<<56) + ('G'<<48) + ('F'<<40) +
  //                             ('E'<<32) + ('D'<<24) + ('C'<<16) +
  //                             ('B'<<8) + 'A').
  uint64_t eightCC;
  VRDisplayCapabilityFlags capabilityFlags;
  VRDisplayBlendMode blendMode;
  VRFieldOfView eyeFOV[VRDisplayState::NumEyes];
  Point3D_POD eyeTranslation[VRDisplayState::NumEyes];
  IntSize_POD eyeResolution;
  float nativeFramebufferScaleFactor;
  bool suppressFrames;
  bool isConnected;
  bool isMounted;
  FloatSize_POD stageSize;
  // We can't use a Matrix4x4 here unless we ensure it's a POD type
  float sittingToStandingTransform[16];
  uint64_t lastSubmittedFrameId;
  bool lastSubmittedFrameSuccessful;
  uint32_t presentingGeneration;
  // Telemetry
  bool reportsDroppedFrames;
  uint64_t droppedFrameCount;

#ifdef MOZILLA_INTERNAL_API
  void Clear() { memset(this, 0, sizeof(VRDisplayState)); }
#endif
};

struct VRControllerState {
  char controllerName[kVRControllerNameMaxLen];
#ifdef MOZILLA_INTERNAL_API
  dom::GamepadHand hand;
#else
  ControllerHand hand;
#endif
  // For WebXR->WebVR mapping conversion, once we remove WebVR,
  // we can remove this item.
  VRControllerType type;
  // https://immersive-web.github.io/webxr/#enumdef-xrtargetraymode
  TargetRayMode targetRayMode;

  // https://immersive-web.github.io/webxr-gamepads-module/#enumdef-gamepadmappingtype
  GamepadMappingType mappingType;

  // Start frame ID of the most recent primary select
  // action, or 0 if the select action has never occurred.
  uint64_t selectActionStartFrameId;
  // End frame Id of the most recent primary select
  // action, or 0 if action never occurred.
  // If selectActionStopFrameId is less than
  // selectActionStartFrameId, then the select
  // action has not ended yet.
  uint64_t selectActionStopFrameId;

  // start frame Id of the most recent primary squeeze
  // action, or 0 if the squeeze action has never occurred.
  uint64_t squeezeActionStartFrameId;
  // End frame Id of the most recent primary squeeze
  // action, or 0 if action never occurred.
  // If squeezeActionStopFrameId is less than
  // squeezeActionStartFrameId, then the squeeze
  // action has not ended yet.
  uint64_t squeezeActionStopFrameId;

  uint32_t numButtons;
  uint32_t numAxes;
  uint32_t numHaptics;
  // The current button pressed bit of button mask.
  uint64_t buttonPressed;
  // The current button touched bit of button mask.
  uint64_t buttonTouched;
  float triggerValue[kVRControllerMaxButtons];
  float axisValue[kVRControllerMaxAxis];

#ifdef MOZILLA_INTERNAL_API
  dom::GamepadCapabilityFlags flags;
#else
  ControllerCapabilityFlags flags;
#endif

  // When Cap_Position is set in flags, pose corresponds
  // to the controllers' pose in grip space:
  // https://immersive-web.github.io/webxr/#dom-xrinputsource-gripspace
  VRPose pose;

  // When Cap_TargetRaySpacePosition is set in flags, targetRayPose corresponds
  // to the controllers' pose in target ray space:
  // https://immersive-web.github.io/webxr/#dom-xrinputsource-targetrayspace
  VRPose targetRayPose;

  bool isPositionValid;
  bool isOrientationValid;

#ifdef MOZILLA_INTERNAL_API
  void Clear() { memset(this, 0, sizeof(VRControllerState)); }
#endif
};

struct VRLayerEyeRect {
  float x;
  float y;
  float width;
  float height;
};

enum class VRLayerType : uint16_t {
  LayerType_None = 0,
  LayerType_2D_Content = 1,
  LayerType_Stereo_Immersive = 2
};

enum class VRLayerTextureType : uint16_t {
  LayerTextureType_None = 0,
  LayerTextureType_D3D10SurfaceDescriptor = 1,
  LayerTextureType_MacIOSurface = 2,
  LayerTextureType_GeckoSurfaceTexture = 3
};

struct VRLayer_2D_Content {
  VRLayerTextureHandle textureHandle;
  VRLayerTextureType textureType;
  uint64_t frameId;
};

struct VRLayer_Stereo_Immersive {
  VRLayerTextureHandle textureHandle;
  VRLayerTextureType textureType;
  uint64_t frameId;
  uint64_t inputFrameId;
  VRLayerEyeRect leftEyeRect;
  VRLayerEyeRect rightEyeRect;
  IntSize_POD textureSize;
};

struct VRLayerState {
  VRLayerType type;
  union {
    VRLayer_2D_Content layer_2d_content;
    VRLayer_Stereo_Immersive layer_stereo_immersive;
  };
};

struct VRHapticState {
  // Reference frame for timing.
  // When 0, this does not represent an active haptic pulse.
  uint64_t inputFrameID;
  // Index within VRSystemState.controllerState identifying the controller
  // to emit the haptic pulse
  uint32_t controllerIndex;
  // 0-based index indicating which haptic actuator within the controller
  uint32_t hapticIndex;
  // Start time of the haptic feedback pulse, relative to the start of
  // inputFrameID, in seconds
  float pulseStart;
  // Duration of the haptic feedback pulse, in seconds
  float pulseDuration;
  // Intensity of the haptic feedback pulse, from 0.0f to 1.0f
  float pulseIntensity;
};

struct VRBrowserState {
#if defined(__ANDROID__)
  bool shutdown;
#endif  // defined(__ANDROID__)
  /**
   * In order to support WebXR's navigator.xr.IsSessionSupported call without
   * displaying any permission dialogue, it is necessary to have a safe way to
   * detect the capability of running a VR or AR session without activating XR
   * runtimes or powering on hardware.
   *
   * API's such as OpenVR make no guarantee that hardware and software won't be
   * left activated after enumerating devices, so each backend in gfx/vr/service
   * must allow for more granular detection of capabilities.
   *
   * When detectRuntimesOnly is true, the initialization exits early after
   * reporting the presence of XR runtime software.
   *
   * The result of the runtime detection is reported with the Cap_ImmersiveVR
   * and Cap_ImmersiveAR bits in VRDisplayState.flags.
   */
  bool detectRuntimesOnly;
  bool presentationActive;
  bool navigationTransitionActive;
  VRLayerState layerState[kVRLayerMaxCount];
  VRHapticState hapticState[kVRHapticsMaxCount];

#ifdef MOZILLA_INTERNAL_API
  void Clear() { memset(this, 0, sizeof(VRBrowserState)); }
#endif
};

struct VRSystemState {
  bool enumerationCompleted;
  VRDisplayState displayState;
  VRHMDSensorState sensorState;
  VRControllerState controllerState[kVRControllerMaxCount];
};

enum class VRFxEventType : uint8_t {
  NONE = 0,
  IME,
  SHUTDOWN,
  FULLSCREEN,
  TOTAL
};

enum class VRFxEventState : uint8_t {
  NONE = 0,
  BLUR,
  FOCUS,
  FULLSCREEN_ENTER,
  FULLSCREEN_EXIT,
  TOTAL
};

// Data shared via shmem for running Firefox in a VR windowed environment
struct VRWindowState {
  // State from Firefox
  uint64_t hwndFx;
  uint32_t widthFx;
  uint32_t heightFx;
  VRLayerTextureHandle textureFx;
  uint32_t windowID;
  VRFxEventType eventType;
  VRFxEventState eventState;

  // State from VRHost
  uint32_t dxgiAdapterHost;
  uint32_t widthHost;
  uint32_t heightHost;

  // Name of synchronization primitive to signal change to this struct
  char signalName[32];
};

enum class VRTelemetryId : uint8_t {
  NONE = 0,
  INSTALLED_FROM = 1,
  ENTRY_METHOD = 2,
  FIRST_RUN = 3,
  TOTAL = 4,
};

enum class VRTelemetryInstallFrom : uint8_t {
  User = 0,
  FxR = 1,
  HTC = 2,
  Valve = 3,
  TOTAL = 4,
};

enum class VRTelemetryEntryMethod : uint8_t {
  SystemBtn = 0,
  Library = 1,
  Gaze = 2,
  TOTAL = 3,
};

struct VRTelemetryState {
  uint32_t uid;

  bool installedFrom : 1;
  bool entryMethod : 1;
  bool firstRun : 1;

  uint8_t installedFromValue : 3;
  uint8_t entryMethodValue : 3;
  bool firstRunValue : 1;
};

struct VRExternalShmem {
  int32_t version;
  int32_t size;
#if defined(__ANDROID__)
  pthread_mutex_t systemMutex;
  pthread_mutex_t geckoMutex;
  pthread_mutex_t servoMutex;
  pthread_cond_t systemCond;
  pthread_cond_t geckoCond;
  pthread_cond_t servoCond;
#else
  int64_t generationA;
#endif  // defined(__ANDROID__)
  VRSystemState state;
#if !defined(__ANDROID__)
  int64_t generationB;
  int64_t geckoGenerationA;
  int64_t servoGenerationA;
#endif  // !defined(__ANDROID__)
  VRBrowserState geckoState;
  VRBrowserState servoState;
#if !defined(__ANDROID__)
  int64_t geckoGenerationB;
  int64_t servoGenerationB;
#endif  // !defined(__ANDROID__)
#if defined(XP_WIN)
  VRWindowState windowState;
  VRTelemetryState telemetryState;
#endif
#ifdef MOZILLA_INTERNAL_API
  void Clear() volatile {
/**
 * When possible we do a memset_s, which is explicitly safe for
 * the volatile, POD struct.  A memset may be optimized out by
 * the compiler and will fail to compile due to volatile keyword
 * propagation.
 *
 * A loop-based fallback is provided in case the toolchain does
 * not include STDC_LIB_EXT1 for memset_s.
 */
#  ifdef __STDC_LIB_EXT1__
    memset_s((void*)this, sizeof(VRExternalShmem), 0, sizeof(VRExternalShmem));
#  else
    size_t remaining = sizeof(VRExternalShmem);
    volatile unsigned char* d = (volatile unsigned char*)this;
    while (remaining--) {
      *d++ = 0;
    }
#  endif
  }
#endif
};

// As we are memcpy'ing VRExternalShmem and its members around, it must be a POD
// type
static_assert(std::is_pod<VRExternalShmem>::value,
              "VRExternalShmem must be a POD type.");

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_EXTERNAL_API_H */
