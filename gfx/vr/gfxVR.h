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
class Compositor;
class CompositingRenderTarget;
}

namespace gfx {

enum class VRHMDType : uint16_t {
  Oculus,
  OSVR,
  NumHMDTypes
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
   * Cap_All used for validity checking during IPC serialization
   */
  Cap_All = (1 << 5) - 1
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VRDisplayCapabilityFlags)

struct VRFieldOfView {
  VRFieldOfView() {}
  VRFieldOfView(double up, double right, double down, double left)
    : upDegrees(up), rightDegrees(right), downDegrees(down), leftDegrees(left)
  {}

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

  Matrix4x4 ConstructProjectionMatrix(float zNear, float zFar, bool rightHanded);

  double upDegrees;
  double rightDegrees;
  double downDegrees;
  double leftDegrees;
};

struct VRDistortionVertex {
  float values[12];
};

struct VRDistortionMesh {
  nsTArray<VRDistortionVertex> mVertices;
  nsTArray<uint16_t> mIndices;
};

// 12 floats per vertex. Position, tex coordinates
// for each channel, and 4 generic attributes
struct VRDistortionConstants {
  float eyeToSourceScaleAndOffset[4];
  float destinationScaleAndOffset[4];
};

struct VRDisplayInfo
{
  VRHMDType GetType() const { return mType; }
  uint32_t GetDeviceID() const { return mDeviceID; }
  const nsCString& GetDeviceName() const { return mDeviceName; }
  VRDisplayCapabilityFlags GetCapabilities() const { return mCapabilityFlags; }
  const VRFieldOfView& GetRecommendedEyeFOV(uint32_t whichEye) const { return mRecommendedEyeFOV[whichEye]; }
  const VRFieldOfView& GetMaximumEyeFOV(uint32_t whichEye) const { return mMaximumEyeFOV[whichEye]; }

  const IntSize& SuggestedEyeResolution() const { return mEyeResolution; }
  const Point3D& GetEyeTranslation(uint32_t whichEye) const { return mEyeTranslation[whichEye]; }
  const Matrix4x4& GetEyeProjectionMatrix(uint32_t whichEye) const { return mEyeProjectionMatrix[whichEye]; }
  const VRFieldOfView& GetEyeFOV(uint32_t whichEye) const { return mEyeFOV[whichEye]; }

  enum Eye {
    Eye_Left,
    Eye_Right,
    NumEyes
  };

  uint32_t mDeviceID;
  VRHMDType mType;
  nsCString mDeviceName;
  VRDisplayCapabilityFlags mCapabilityFlags;
  VRFieldOfView mMaximumEyeFOV[VRDisplayInfo::NumEyes];
  VRFieldOfView mRecommendedEyeFOV[VRDisplayInfo::NumEyes];
  VRFieldOfView mEyeFOV[VRDisplayInfo::NumEyes];
  Point3D mEyeTranslation[VRDisplayInfo::NumEyes];
  Matrix4x4 mEyeProjectionMatrix[VRDisplayInfo::NumEyes];
  /* Suggested resolution for rendering a single eye.
   * Assumption is that left/right rendering will be 2x of this size.
   * XXX fix this for vertical displays
   */
  IntSize mEyeResolution;
  IntRect mScreenRect;

  bool mIsFakeScreen;



  bool operator==(const VRDisplayInfo& other) const {
    return mType == other.mType &&
           mDeviceID == other.mDeviceID &&
           mDeviceName == other.mDeviceName &&
           mCapabilityFlags == other.mCapabilityFlags &&
           mEyeResolution == other.mEyeResolution &&
           mScreenRect == other.mScreenRect &&
           mIsFakeScreen == other.mIsFakeScreen &&
           mMaximumEyeFOV[0] == other.mMaximumEyeFOV[0] &&
           mMaximumEyeFOV[1] == other.mMaximumEyeFOV[1] &&
           mRecommendedEyeFOV[0] == other.mRecommendedEyeFOV[0] &&
           mRecommendedEyeFOV[1] == other.mRecommendedEyeFOV[1] &&
           mEyeFOV[0] == other.mEyeFOV[0] &&
           mEyeFOV[1] == other.mEyeFOV[1] &&
           mEyeTranslation[0] == other.mEyeTranslation[0] &&
           mEyeTranslation[1] == other.mEyeTranslation[1] &&
           mEyeProjectionMatrix[0] == other.mEyeProjectionMatrix[0] &&
           mEyeProjectionMatrix[1] == other.mEyeProjectionMatrix[1];
  }

  bool operator!=(const VRDisplayInfo& other) const {
    return !(*this == other);
  }
};



struct VRHMDSensorState {
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

struct VRSensorUpdate {
  VRSensorUpdate() { }; // Required for ipdl binding
  VRSensorUpdate(uint32_t aDeviceID, const VRHMDSensorState& aSensorState)
   : mDeviceID(aDeviceID)
   , mSensorState(aSensorState) { };

  uint32_t mDeviceID;
  VRHMDSensorState mSensorState;
};

struct VRDisplayUpdate {
  VRDisplayUpdate() { }; // Required for ipdl binding
  VRDisplayUpdate(const VRDisplayInfo& aDeviceInfo,
                 const VRHMDSensorState& aSensorState)
   : mDeviceInfo(aDeviceInfo)
   , mSensorState(aSensorState) { };

  VRDisplayInfo mDeviceInfo;
  VRHMDSensorState mSensorState;
};

/* A pure data struct that can be used to see if
 * the configuration of one HMDInfo matches another; for rendering purposes,
 * really asking "can the rendering details of this one be used for the other"
 */
struct VRHMDConfiguration {
  VRHMDConfiguration() : hmdType(VRHMDType::NumHMDTypes) {}

  bool operator==(const VRHMDConfiguration& other) const {
    return hmdType == other.hmdType &&
      value == other.value &&
      fov[0] == other.fov[0] &&
      fov[1] == other.fov[1];
  }

  bool operator!=(const VRHMDConfiguration& other) const {
    return hmdType != other.hmdType ||
      value != other.value ||
      fov[0] != other.fov[0] ||
      fov[1] != other.fov[1];
  }

  bool IsValid() const {
    return hmdType != VRHMDType::NumHMDTypes;
  }

  VRHMDType hmdType;
  uint32_t value;
  VRFieldOfView fov[2];
};

class VRHMDRenderingSupport {
public:
  struct RenderTargetSet {
    RenderTargetSet();
    
    NS_INLINE_DECL_REFCOUNTING(RenderTargetSet)

    RefPtr<layers::Compositor> compositor;
    IntSize size;
    nsTArray<RefPtr<layers::CompositingRenderTarget>> renderTargets;

    virtual already_AddRefed<layers::CompositingRenderTarget> GetNextRenderTarget() = 0;
  protected:
    virtual ~RenderTargetSet();
  };

  virtual already_AddRefed<RenderTargetSet> CreateRenderTargetSet(layers::Compositor *aCompositor, const IntSize& aSize) = 0;
  virtual void DestroyRenderTargetSet(RenderTargetSet *aRTSet) = 0;
  virtual void SubmitFrame(RenderTargetSet *aRTSet, int32_t aInputFrameID) = 0;
protected:
  VRHMDRenderingSupport() { }
};

class VRHMDInfo {

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRHMDInfo)

  const VRHMDConfiguration& GetConfiguration() const { return mConfiguration; }
  const VRDisplayInfo& GetDeviceInfo() const { return mDeviceInfo; }

  /* set the FOV for this HMD unit; this triggers a computation of all the remaining bits.  Returns false if it fails */
  virtual bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
                      double zNear, double zFar) = 0;

  virtual bool KeepSensorTracking() = 0;
  virtual void NotifyVsync(const TimeStamp& aVsyncTimestamp) = 0;
  virtual VRHMDSensorState GetSensorState() = 0;
  virtual VRHMDSensorState GetImmediateSensorState() = 0;

  virtual void ZeroSensor() = 0;

  // if rendering is offloaded
  virtual VRHMDRenderingSupport *GetRenderingSupport() { return nullptr; }

  // distortion mesh stuff; we should implement renderingsupport for this
  virtual void FillDistortionConstants(uint32_t whichEye,
                                       const IntSize& textureSize, // the full size of the texture
                                       const IntRect& eyeViewport, // the viewport within the texture for the current eye
                                       const Size& destViewport,   // the size of the destination viewport
                                       const Rect& destRect,       // the rectangle within the dest viewport that this should be rendered
                                       VRDistortionConstants& values) = 0;

  const VRDistortionMesh& GetDistortionMesh(uint32_t whichEye) const { return mDistortionMesh[whichEye]; }
protected:
  explicit VRHMDInfo(VRHMDType aType);
  virtual ~VRHMDInfo();

  VRHMDConfiguration mConfiguration;
  VRDisplayInfo mDeviceInfo;
  VRDistortionMesh mDistortionMesh[VRDisplayInfo::NumEyes];
};

class VRHMDManager {
public:
  static uint32_t AllocateDeviceID();

protected:
  static Atomic<uint32_t> sDeviceBase;


public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRHMDManager)

  virtual bool Init() = 0;
  virtual void Destroy() = 0;
  virtual void GetHMDs(nsTArray<RefPtr<VRHMDInfo>>& aHMDResult) = 0;

protected:
  VRHMDManager() { }
  virtual ~VRHMDManager() { }
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_H */
