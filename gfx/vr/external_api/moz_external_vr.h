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

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#ifdef MOZILLA_INTERNAL_API
#  include "mozilla/TypedEnumBits.h"
#  include "mozilla/gfx/2D.h"
#endif  // MOZILLA_INTERNAL_API

#if defined(__ANDROID__)
#  include <pthread.h>
#endif  // defined(__ANDROID__)

namespace mozilla {
#ifdef MOZILLA_INTERNAL_API
namespace dom {
enum class GamepadHand : uint8_t;
enum class GamepadCapabilityFlags : uint16_t;
}  // namespace dom
#endif  //  MOZILLA_INTERNAL_API
namespace gfx {

static const int32_t kVRExternalVersion = 7;

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
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 5) - 1
};

#endif  // ifndef MOZILLA_INTERNAL_API

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
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 10) - 1
};

#ifdef MOZILLA_INTERNAL_API
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VRDisplayCapabilityFlags)
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
  VRFieldOfView eyeFOV[VRDisplayState::NumEyes];
  Point3D_POD eyeTranslation[VRDisplayState::NumEyes];
  IntSize_POD eyeResolution;
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
};

struct VRControllerState {
  char controllerName[kVRControllerNameMaxLen];
#ifdef MOZILLA_INTERNAL_API
  dom::GamepadHand hand;
#else
  ControllerHand hand;
#endif
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
  VRPose pose;
  bool isPositionValid;
  bool isOrientationValid;
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
  bool presentationActive;
  bool navigationTransitionActive;
  VRLayerState layerState[kVRLayerMaxCount];
  VRHapticState hapticState[kVRHapticsMaxCount];
};

struct VRSystemState {
  bool enumerationCompleted;
  VRDisplayState displayState;
  VRHMDSensorState sensorState;
  VRControllerState controllerState[kVRControllerMaxCount];
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
};

// As we are memcpy'ing VRExternalShmem and its members around, it must be a POD
// type
static_assert(std::is_pod<VRExternalShmem>::value,
              "VRExternalShmem must be a POD type.");

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_EXTERNAL_API_H */
