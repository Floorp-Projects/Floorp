/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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

enum class VRDeviceType : uint16_t {
  Oculus,
  OpenVR,
  OSVR,
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
  VRFieldOfView() {}
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
  bool GetIsPresenting() const { return mIsPresenting; }
  const Size& GetStageSize() const { return mStageSize; }
  const Matrix4x4& GetSittingToStandingTransform() const { return mSittingToStandingTransform; }

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
  bool mIsPresenting;
  Size mStageSize;
  Matrix4x4 mSittingToStandingTransform;

  bool operator==(const VRDisplayInfo& other) const {
    return mType == other.mType &&
           mDisplayID == other.mDisplayID &&
           mDisplayName == other.mDisplayName &&
           mCapabilityFlags == other.mCapabilityFlags &&
           mEyeResolution == other.mEyeResolution &&
           mIsConnected == other.mIsConnected &&
           mIsMounted == other.mIsMounted &&
           mIsPresenting == other.mIsPresenting &&
           mEyeFOV[0] == other.mEyeFOV[0] &&
           mEyeFOV[1] == other.mEyeFOV[1] &&
           mEyeTranslation[0] == other.mEyeTranslation[0] &&
           mEyeTranslation[1] == other.mEyeTranslation[1] &&
           mStageSize == other.mStageSize &&
           mSittingToStandingTransform == other.mSittingToStandingTransform;
  }

  bool operator!=(const VRDisplayInfo& other) const {
    return !(*this == other);
  }
};

struct VRHMDSensorState {
  VRHMDSensorState()
  {
    Clear();
  }
  double timestamp;
  int32_t inputFrameID;
  VRDisplayCapabilityFlags flags;
  float orientation[4];
  float position[3];
  float angularVelocity[3];
  float angularAcceleration[3];
  float linearVelocity[3];
  float linearAcceleration[3];

  void Clear() {
    memset(this, 0, sizeof(VRHMDSensorState));
  }
};

struct VRControllerInfo
{
  VRDeviceType GetType() const { return mType; }
  uint32_t GetControllerID() const { return mControllerID; }
  const nsCString& GetControllerName() const { return mControllerName; }
  dom::GamepadMappingType GetMappingType() const { return mMappingType; }
  dom::GamepadHand GetHand() const { return mHand; }
  uint32_t GetNumButtons() const { return mNumButtons; }
  uint32_t GetNumAxes() const { return mNumAxes; }
  uint32_t GetNumHaptics() const { return mNumHaptics; }

  uint32_t mControllerID;
  VRDeviceType mType;
  nsCString mControllerName;
  dom::GamepadMappingType mMappingType;
  dom::GamepadHand mHand;
  uint32_t mNumButtons;
  uint32_t mNumAxes;
  uint32_t mNumHaptics;

  bool operator==(const VRControllerInfo& other) const {
    return mType == other.mType &&
           mControllerID == other.mControllerID &&
           mControllerName == other.mControllerName &&
           mMappingType == other.mMappingType &&
           mHand == other.mHand &&
           mNumButtons == other.mNumButtons &&
           mNumAxes == other.mNumAxes &&
           mNumHaptics == other.mNumHaptics;
  }

  bool operator!=(const VRControllerInfo& other) const {
    return !(*this == other);
  }
};

class VRSystemManager {
public:
  static uint32_t AllocateDisplayID();

protected:
  static Atomic<uint32_t> sDisplayBase;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRSystemManager)

  virtual void Destroy() = 0;
  virtual void Shutdown() = 0;
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) = 0;
  virtual bool GetIsPresenting() = 0;
  virtual void HandleInput() = 0;
  virtual void GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult) = 0;
  virtual void ScanForControllers() = 0;
  virtual void RemoveControllers() = 0;
  virtual void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                             double aIntensity, double aDuration, uint32_t aPromiseID) = 0;
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
  virtual ~VRSystemManager() { }

  uint32_t mControllerCount;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_H */
