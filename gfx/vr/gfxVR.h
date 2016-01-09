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
  Cardboard,
  Oculus050,
  NumHMDTypes
};

enum class VRStateValidFlags : uint16_t {
  State_None = 0,
  State_Position = 1 << 1,
  State_Orientation = 1 << 2,
  // State_All used for validity checking during IPC serialization
  State_All = (1 << 3) - 1
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(VRStateValidFlags)

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

struct VRDeviceInfo
{
  VRHMDType GetType() const { return mType; }
  uint32_t GetDeviceID() const { return mDeviceID; }
  const nsCString& GetDeviceName() const { return mDeviceName; }
  VRStateValidFlags GetSupportedSensorStateBits() const { return mSupportedSensorBits; }
  const VRFieldOfView& GetRecommendedEyeFOV(uint32_t whichEye) const { return mRecommendedEyeFOV[whichEye]; }
  const VRFieldOfView& GetMaximumEyeFOV(uint32_t whichEye) const { return mMaximumEyeFOV[whichEye]; }

  const IntSize& SuggestedEyeResolution() const { return mEyeResolution; }
  const Point3D& GetEyeTranslation(uint32_t whichEye) const { return mEyeTranslation[whichEye]; }
  const Matrix4x4& GetEyeProjectionMatrix(uint32_t whichEye) const { return mEyeProjectionMatrix[whichEye]; }
  const VRFieldOfView& GetEyeFOV(uint32_t whichEye) const { return mEyeFOV[whichEye]; }
  bool GetUseMainThreadOrientation() const { return mUseMainThreadOrientation; }

  enum Eye {
    Eye_Left,
    Eye_Right,
    NumEyes
  };

  uint32_t mDeviceID;
  VRHMDType mType;
  nsCString mDeviceName;
  VRStateValidFlags mSupportedSensorBits;
  VRFieldOfView mMaximumEyeFOV[VRDeviceInfo::NumEyes];
  VRFieldOfView mRecommendedEyeFOV[VRDeviceInfo::NumEyes];
  VRFieldOfView mEyeFOV[VRDeviceInfo::NumEyes];
  Point3D mEyeTranslation[VRDeviceInfo::NumEyes];
  Matrix4x4 mEyeProjectionMatrix[VRDeviceInfo::NumEyes];
  /* Suggested resolution for rendering a single eye.
   * Assumption is that left/right rendering will be 2x of this size.
   * XXX fix this for vertical displays
   */
  IntSize mEyeResolution;
  IntRect mScreenRect;

  bool mIsFakeScreen;
  bool mUseMainThreadOrientation;



  bool operator==(const VRDeviceInfo& other) const {
    return mType == other.mType &&
           mDeviceID == other.mDeviceID &&
           mDeviceName == other.mDeviceName &&
           mSupportedSensorBits == other.mSupportedSensorBits &&
           mEyeResolution == other.mEyeResolution &&
           mScreenRect == other.mScreenRect &&
           mIsFakeScreen == other.mIsFakeScreen &&
           mUseMainThreadOrientation == other.mUseMainThreadOrientation &&
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

  bool operator!=(const VRDeviceInfo& other) const {
    return !(*this == other);
  }
};



struct VRHMDSensorState {
  double timestamp;
  int32_t inputFrameID;
  VRStateValidFlags flags;
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

struct VRDeviceUpdate {
  VRDeviceUpdate() { }; // Required for ipdl binding
  VRDeviceUpdate(const VRDeviceInfo& aDeviceInfo,
                 const VRHMDSensorState& aSensorState)
   : mDeviceInfo(aDeviceInfo)
   , mSensorState(aSensorState) { };

  VRDeviceInfo mDeviceInfo;
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
    int32_t currentRenderTarget;

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
  const VRDeviceInfo& GetDeviceInfo() const { return mDeviceInfo; }

  /* set the FOV for this HMD unit; this triggers a computation of all the remaining bits.  Returns false if it fails */
  virtual bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
                      double zNear, double zFar) = 0;

  virtual bool KeepSensorTracking() = 0;
  virtual void NotifyVsync(const TimeStamp& aVsyncTimestamp) = 0;
  virtual VRHMDSensorState GetSensorState(double timeOffset = 0.0) = 0;

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
  explicit VRHMDInfo(VRHMDType aType, bool aUseMainThreadOrientation);
  virtual ~VRHMDInfo();

  VRHMDConfiguration mConfiguration;
  VRDeviceInfo mDeviceInfo;
  VRDistortionMesh mDistortionMesh[VRDeviceInfo::NumEyes];
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
