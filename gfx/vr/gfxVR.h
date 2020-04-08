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
#include <type_traits>

namespace mozilla {
namespace layers {
class PTextureParent;
}
namespace dom {
enum class GamepadMappingType : uint8_t;
enum class GamepadHand : uint8_t;
}  // namespace dom
namespace gfx {
enum class VRAPIMode : uint8_t {WebXR, WebVR, NumVRAPIModes};

class VRLayerParent;
class VRDisplayHost;
class VRManagerPromise;

// The maximum number of frames of latency that we would expect before we
// should give up applying pose prediction.
// If latency is greater than one second, then the experience is not likely
// to be corrected by pose prediction.  Setting this value too
// high may result in unnecessary memory allocation.
// As the current fastest refresh rate is 90hz, 100 is selected as a
// conservative value.
static const int kVRMaxLatencyFrames = 100;

struct VRDisplayInfo {
  uint32_t mDisplayID;
  uint32_t mPresentingGroups;
  uint32_t mGroupMask;
  uint64_t mFrameId;
  VRDisplayState mDisplayState;
  VRControllerState mControllerState[kVRControllerMaxCount];

  VRHMDSensorState mLastSensorState[kVRMaxLatencyFrames];
  void Clear() { memset(this, 0, sizeof(VRDisplayInfo)); }
  const VRHMDSensorState& GetSensorState() const {
    return mLastSensorState[mFrameId % kVRMaxLatencyFrames];
  }

  uint32_t GetDisplayID() const { return mDisplayID; }
  const char* GetDisplayName() const { return mDisplayState.displayName; }
  VRDisplayCapabilityFlags GetCapabilities() const {
    return mDisplayState.capabilityFlags;
  }

  const IntSize SuggestedEyeResolution() const;
  const Point3D GetEyeTranslation(uint32_t whichEye) const;
  const VRFieldOfView& GetEyeFOV(uint32_t whichEye) const {
    return mDisplayState.eyeFOV[whichEye];
  }
  bool GetIsConnected() const { return mDisplayState.isConnected; }
  bool GetIsMounted() const { return mDisplayState.isMounted; }
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
    // Note that mDisplayState and mControllerState are asserted to be POD
    // types, so memcmp is safe
    return mDisplayID == other.mDisplayID &&
           memcmp(&mDisplayState, &other.mDisplayState,
                  sizeof(VRDisplayState)) == 0 &&
           memcmp(mControllerState, other.mControllerState,
                  sizeof(VRControllerState) * kVRControllerMaxCount) == 0 &&
           mPresentingGroups == other.mPresentingGroups &&
           mGroupMask == other.mGroupMask && mFrameId == other.mFrameId;
  }

  bool operator!=(const VRDisplayInfo& other) const {
    return !(*this == other);
  }
};

static_assert(std::is_pod<VRDisplayInfo>::value,
              "VRDisplayInfo must be a POD type.");

struct VRSubmitFrameResultInfo {
  VRSubmitFrameResultInfo()
      : mFormat(SurfaceFormat::UNKNOWN), mFrameNum(0), mWidth(0), mHeight(0) {}

  nsCString mBase64Image;
  SurfaceFormat mFormat;
  uint64_t mFrameNum;
  uint32_t mWidth;
  uint32_t mHeight;
};

struct VRControllerInfo {
  uint32_t GetControllerID() const { return mControllerID; }
  const char* GetControllerName() const {
    return mControllerState.controllerName;
  }
  dom::GamepadMappingType GetMappingType() const { return mMappingType; }
  uint32_t GetDisplayID() const { return mDisplayID; }
  dom::GamepadHand GetHand() const { return mControllerState.hand; }
  uint32_t GetNumButtons() const { return mControllerState.numButtons; }
  uint32_t GetNumAxes() const { return mControllerState.numAxes; }
  uint32_t GetNumHaptics() const { return mControllerState.numHaptics; }

  uint32_t mControllerID;
  dom::GamepadMappingType mMappingType;
  uint32_t mDisplayID;
  VRControllerState mControllerState;
  bool operator==(const VRControllerInfo& other) const {
    // Note that mControllerState is asserted to be a POD type, so memcmp is
    // safe
    return mControllerID == other.mControllerID &&
           memcmp(&mControllerState, &other.mControllerState,
                  sizeof(VRControllerState)) == 0 &&
           mMappingType == other.mMappingType && mDisplayID == other.mDisplayID;
  }

  bool operator!=(const VRControllerInfo& other) const {
    return !(*this == other);
  }
};

struct VRTelemetry {
  VRTelemetry() : mLastDroppedFrameCount(-1) {}

  void Clear() {
    mPresentationStart = TimeStamp();
    mLastDroppedFrameCount = -1;
  }

  bool IsLastDroppedFrameValid() { return (mLastDroppedFrameCount != -1); }

  TimeStamp mPresentationStart;
  int32_t mLastDroppedFrameCount;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_H */
