/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_H
#define GFX_VR_H

#include "moz_external_vr.h"
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

// The maximum number of frames of latency that we would expect before we
// should give up applying pose prediction.
// If latency is greater than one second, then the experience is not likely
// to be corrected by pose prediction.  Setting this value too
// high may result in unnecessary memory allocation.
// As the current fastest refresh rate is 90hz, 100 is selected as a
// conservative value.
static const int kVRMaxLatencyFrames = 100;

enum class VRDeviceType : uint16_t {
  Oculus,
  OpenVR,
  OSVR,
  GVR,
  Puppet,
  External,
  NumVRDeviceTypes
};

struct VRDisplayInfo
{
  uint32_t mDisplayID;
  VRDeviceType mType;
  uint32_t mPresentingGroups;
  uint32_t mGroupMask;
  uint64_t mFrameId;
  VRDisplayState mDisplayState;

  VRHMDSensorState mLastSensorState[kVRMaxLatencyFrames];
  const VRHMDSensorState& GetSensorState() const
  {
    return mLastSensorState[mFrameId % kVRMaxLatencyFrames];
  }

  VRDeviceType GetType() const { return mType; }
  uint32_t GetDisplayID() const { return mDisplayID; }
  const char* GetDisplayName() const { return mDisplayState.mDisplayName; }
  VRDisplayCapabilityFlags GetCapabilities() const { return mDisplayState.mCapabilityFlags; }

  const IntSize SuggestedEyeResolution() const;
  const Point3D GetEyeTranslation(uint32_t whichEye) const;
  const VRFieldOfView& GetEyeFOV(uint32_t whichEye) const { return mDisplayState.mEyeFOV[whichEye]; }
  bool GetIsConnected() const { return mDisplayState.mIsConnected; }
  bool GetIsMounted() const { return mDisplayState.mIsMounted; }
  uint32_t GetPresentingGroups() const { return mPresentingGroups; }
  uint32_t GetGroupMask() const { return mGroupMask; }
  const Size GetStageSize() const;
  const Matrix4x4 GetSittingToStandingTransform() const;
  uint64_t GetFrameId() const { return mFrameId; }

  bool operator==(const VRDisplayInfo& other) const {
    for (size_t i = 0; i < kVRMaxLatencyFrames; i++) {
      if (mLastSensorState[i] != other.mLastSensorState[i]) {
        return false;
      }
    }
    // Note that mDisplayState is asserted to be a POD type, so memcmp is safe
    return mType == other.mType &&
           mDisplayID == other.mDisplayID &&
           memcmp(&mDisplayState, &other.mDisplayState, sizeof(VRDisplayState)) == 0 &&
           mPresentingGroups == other.mPresentingGroups &&
           mGroupMask == other.mGroupMask &&
           mFrameId == other.mFrameId;
  }

  bool operator!=(const VRDisplayInfo& other) const {
    return !(*this == other);
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
  const char* GetControllerName() const { return mControllerState.mControllerName; }
  dom::GamepadMappingType GetMappingType() const { return mMappingType; }
  uint32_t GetDisplayID() const { return mDisplayID; }
  dom::GamepadHand GetHand() const { return mControllerState.mHand; }
  uint32_t GetNumButtons() const { return mControllerState.mNumButtons; }
  uint32_t GetNumAxes() const { return mControllerState.mNumAxes; }
  uint32_t GetNumHaptics() const { return mControllerState.mNumHaptics; }

  uint32_t mControllerID;
  VRDeviceType mType;
  dom::GamepadMappingType mMappingType;
  uint32_t mDisplayID;
  VRControllerState mControllerState;
  bool operator==(const VRControllerInfo& other) const {
    return mType == other.mType &&
           mControllerID == other.mControllerID &&
           strncmp(mControllerState.mControllerName, other.mControllerState.mControllerName, kVRControllerNameMaxLen) == 0 &&
           mMappingType == other.mMappingType &&
           mDisplayID == other.mDisplayID &&
           mControllerState.mHand == other.mControllerState.mHand &&
           mControllerState.mNumButtons == other.mControllerState.mNumButtons &&
           mControllerState.mNumAxes == other.mControllerState.mNumAxes &&
           mControllerState.mNumHaptics == other.mControllerState.mNumHaptics;
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
