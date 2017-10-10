/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_H
#define GFX_VR_H

#include "nsTArray.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Atomics.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace layers {
class PTextureParent;
}
namespace dom {
enum class GamepadMappingType : uint8_t;
enum class GamepadHand : uint8_t;
struct GamepadPoseState;
}
namespace gfx {
class VRLayerParent;
class VRDisplayHost;
class VRControllerHost;
class VRManagerPromise;

enum class VRDeviceType : uint16_t {
  Oculus,
  OpenVR,
  OSVR,
  GVR,
  Puppet,
  NumVRDeviceTypes
};

enum class VRDisplayCapabilityFlags : uint16_t {
  Cap_None = 0,
  /**
   * Cap_Position is set if the VRDisplay is capable of tracking its position.
   */
  Cap_Position = 1 << 1,
  /**
    * Cap_Orientation is set if the VRDisplay is capable of tracking its orientation.
    */
  Cap_Orientation = 1 << 2,
  /**
   * Cap_Present is set if the VRDisplay is capable of presenting content to an
   * HMD or similar device.  Can be used to indicate "magic window" devices that
   * are capable of 6DoF tracking but for which requestPresent is not meaningful.
   * If false then calls to requestPresent should always fail, and
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
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 9) - 1
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VRDisplayCapabilityFlags)

struct VRFieldOfView {
  VRFieldOfView() = default;
  VRFieldOfView(double up, double right, double down, double left)
    : upDegrees(up), rightDegrees(right), downDegrees(down), leftDegrees(left)
  {}

  void SetFromTanRadians(double up, double right, double down, double left)
  {
    upDegrees = atan(up) * 180.0 / M_PI;
    rightDegrees = atan(right) * 180.0 / M_PI;
    downDegrees = atan(down) * 180.0 / M_PI;
    leftDegrees = atan(left) * 180.0 / M_PI;
  }

  bool operator==(const VRFieldOfView& other) const {
    return other.upDegrees == upDegrees &&
           other.downDegrees == downDegrees &&
           other.rightDegrees == rightDegrees &&
           other.leftDegrees == leftDegrees;
  }

  bool operator!=(const VRFieldOfView& other) const {
    return !(*this == other);
  }

  bool IsZero() const {
    return upDegrees == 0.0 ||
      rightDegrees == 0.0 ||
      downDegrees == 0.0 ||
      leftDegrees == 0.0;
  }

  Matrix4x4 ConstructProjectionMatrix(float zNear, float zFar, bool rightHanded) const;

  double upDegrees;
  double rightDegrees;
  double downDegrees;
  double leftDegrees;
};

struct VRHMDSensorState {
  int64_t inputFrameID;
  double timestamp;
  VRDisplayCapabilityFlags flags;

  // These members will only change with inputFrameID:
  float orientation[4];
  float position[3];
  float leftViewMatrix[16];
  float rightViewMatrix[16];
  float angularVelocity[3];
  float angularAcceleration[3];
  float linearVelocity[3];
  float linearAcceleration[3];

  void Clear() {
    memset(this, 0, sizeof(VRHMDSensorState));
  }

  bool operator==(const VRHMDSensorState& other) const {
    return inputFrameID == other.inputFrameID &&
           timestamp == other.timestamp;
  }

  bool operator!=(const VRHMDSensorState& other) const {
    return !(*this == other);
  }
  void CalcViewMatrices(const gfx::Matrix4x4* aHeadToEyeTransforms);
};

// The maximum number of frames of latency that we would expect before we
// should give up applying pose prediction.
// If latency is greater than one second, then the experience is not likely
// to be corrected by pose prediction.  Setting this value too
// high may result in unnecessary memory allocation.
// As the current fastest refresh rate is 90hz, 100 is selected as a
// conservative value.
static const int kVRMaxLatencyFrames = 100;

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

struct VRDisplayInfo
{
  VRDeviceType GetType() const { return mType; }
  uint32_t GetDisplayID() const { return mDisplayID; }
  const nsCString& GetDisplayName() const { return mDisplayName; }
  VRDisplayCapabilityFlags GetCapabilities() const { return mCapabilityFlags; }

  const IntSize& SuggestedEyeResolution() const { return mEyeResolution; }
  const Point3D& GetEyeTranslation(uint32_t whichEye) const { return mEyeTranslation[whichEye]; }
  const VRFieldOfView& GetEyeFOV(uint32_t whichEye) const { return mEyeFOV[whichEye]; }
  bool GetIsConnected() const { return mIsConnected; }
  bool GetIsMounted() const { return mIsMounted; }
  uint32_t GetPresentingGroups() const { return mPresentingGroups; }
  uint32_t GetGroupMask() const { return mGroupMask; }
  const Size& GetStageSize() const { return mStageSize; }
  const Matrix4x4& GetSittingToStandingTransform() const { return mSittingToStandingTransform; }
  uint64_t GetFrameId() const { return mFrameId; }

  enum Eye {
    Eye_Left,
    Eye_Right,
    NumEyes
  };

  uint32_t mDisplayID;
  VRDeviceType mType;
  nsCString mDisplayName;
  VRDisplayCapabilityFlags mCapabilityFlags;
  VRFieldOfView mEyeFOV[VRDisplayInfo::NumEyes];
  Point3D mEyeTranslation[VRDisplayInfo::NumEyes];
  IntSize mEyeResolution;
  bool mIsConnected;
  bool mIsMounted;
  uint32_t mPresentingGroups;
  uint32_t mGroupMask;
  Size mStageSize;
  Matrix4x4 mSittingToStandingTransform;
  uint64_t mFrameId;
  uint32_t mPresentingGeneration;
  VRHMDSensorState mLastSensorState[kVRMaxLatencyFrames];

  bool operator==(const VRDisplayInfo& other) const {
    for (size_t i = 0; i < kVRMaxLatencyFrames; i++) {
      if (mLastSensorState[i] != other.mLastSensorState[i]) {
        return false;
      }
    }
    return mType == other.mType &&
           mDisplayID == other.mDisplayID &&
           mDisplayName == other.mDisplayName &&
           mCapabilityFlags == other.mCapabilityFlags &&
           mEyeResolution == other.mEyeResolution &&
           mIsConnected == other.mIsConnected &&
           mIsMounted == other.mIsMounted &&
           mPresentingGroups == other.mPresentingGroups &&
           mGroupMask == other.mGroupMask &&
           mEyeFOV[0] == other.mEyeFOV[0] &&
           mEyeFOV[1] == other.mEyeFOV[1] &&
           mEyeTranslation[0] == other.mEyeTranslation[0] &&
           mEyeTranslation[1] == other.mEyeTranslation[1] &&
           mStageSize == other.mStageSize &&
           mSittingToStandingTransform == other.mSittingToStandingTransform &&
           mFrameId == other.mFrameId &&
           mPresentingGeneration == other.mPresentingGeneration;
  }

  bool operator!=(const VRDisplayInfo& other) const {
    return !(*this == other);
  }

  const VRHMDSensorState& GetSensorState() const
  {
    return mLastSensorState[mFrameId % kVRMaxLatencyFrames];
  }
};

struct VRSubmitFrameResultInfo
{
  VRSubmitFrameResultInfo()
   : mFrameNum(0),
     mWidth(0),
     mHeight(0)
  {}

  nsCString mBase64Image;
  SurfaceFormat mFormat;
  uint64_t mFrameNum;
  uint32_t mWidth;
  uint32_t mHeight;
};

struct VRControllerInfo
{
  VRDeviceType GetType() const { return mType; }
  uint32_t GetControllerID() const { return mControllerID; }
  const nsCString& GetControllerName() const { return mControllerName; }
  dom::GamepadMappingType GetMappingType() const { return mMappingType; }
  uint32_t GetDisplayID() const { return mDisplayID; }
  dom::GamepadHand GetHand() const { return mHand; }
  uint32_t GetNumButtons() const { return mNumButtons; }
  uint32_t GetNumAxes() const { return mNumAxes; }
  uint32_t GetNumHaptics() const { return mNumHaptics; }

  uint32_t mControllerID;
  VRDeviceType mType;
  nsCString mControllerName;
  dom::GamepadMappingType mMappingType;
  uint32_t mDisplayID;
  dom::GamepadHand mHand;
  uint32_t mNumButtons;
  uint32_t mNumAxes;
  uint32_t mNumHaptics;

  bool operator==(const VRControllerInfo& other) const {
    return mType == other.mType &&
           mControllerID == other.mControllerID &&
           mControllerName == other.mControllerName &&
           mMappingType == other.mMappingType &&
           mDisplayID == other.mDisplayID &&
           mHand == other.mHand &&
           mNumButtons == other.mNumButtons &&
           mNumAxes == other.mNumAxes &&
           mNumHaptics == other.mNumHaptics;
  }

  bool operator!=(const VRControllerInfo& other) const {
    return !(*this == other);
  }
};

struct VRTelemetry
{
  VRTelemetry()
   : mLastDroppedFrameCount(-1)
  {}

  void Clear() {
    mPresentationStart = TimeStamp();
    mLastDroppedFrameCount = -1;
  }

  bool IsLastDroppedFrameValid() {
    return (mLastDroppedFrameCount != -1);
  }

  TimeStamp mPresentationStart;
  int32_t mLastDroppedFrameCount;
};

class VRSystemManager {
public:
  static uint32_t AllocateDisplayID();
  static uint32_t AllocateControllerID();

protected:
  static Atomic<uint32_t> sDisplayBase;
  static Atomic<uint32_t> sControllerBase;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRSystemManager)

  virtual void Destroy() = 0;
  virtual void Shutdown() = 0;
  virtual void Enumerate() = 0;
  virtual void NotifyVSync();
  virtual bool ShouldInhibitEnumeration();
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) = 0;
  virtual bool GetIsPresenting() = 0;
  virtual void HandleInput() = 0;
  virtual void GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult) = 0;
  virtual void ScanForControllers() = 0;
  virtual void RemoveControllers() = 0;
  virtual void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                             double aIntensity, double aDuration, const VRManagerPromise& aPromise) = 0;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) = 0;
  void NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed, bool aTouched,
                      double aValue);
  void NewAxisMove(uint32_t aIndex, uint32_t aAxis, double aValue);
  void NewPoseState(uint32_t aIndex, const dom::GamepadPoseState& aPose);
  void NewHandChangeEvent(uint32_t aIndex, const dom::GamepadHand aHand);
  void AddGamepad(const VRControllerInfo& controllerInfo);
  void RemoveGamepad(uint32_t aIndex);

protected:
  VRSystemManager() : mControllerCount(0) { }
  virtual ~VRSystemManager() = default;

  uint32_t mControllerCount;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_H */
