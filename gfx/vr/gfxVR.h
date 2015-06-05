/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_H
#define GFX_VR_H

#include "nsTArray.h"
#include "nsIScreen.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/nsRefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Atomics.h"

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

struct VRFieldOfView {
  static VRFieldOfView FromCSSPerspectiveInfo(double aPerspectiveDistance,
                                              const Point& aPerspectiveOrigin,
                                              const Point& aTransformOrigin,
                                              const Rect& aContentRectangle)
  {
    /**/
    return VRFieldOfView();
  }

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

// 12 floats per vertex. Position, tex coordinates
// for each channel, and 4 generic attributes
struct VRDistortionConstants {
  float eyeToSourceScaleAndOffset[4];
  float destinationScaleAndOffset[4];
};

struct VRDistortionVertex {
  float values[12];
};

struct VRDistortionMesh {
  nsTArray<VRDistortionVertex> mVertices;
  nsTArray<uint16_t> mIndices;
};

struct VRHMDSensorState {
  double timestamp;
  uint32_t flags;
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

    nsRefPtr<layers::Compositor> compositor;
    IntSize size;
    nsTArray<nsRefPtr<layers::CompositingRenderTarget>> renderTargets;
    int32_t currentRenderTarget;

    virtual already_AddRefed<layers::CompositingRenderTarget> GetNextRenderTarget() = 0;
  protected:
    virtual ~RenderTargetSet();
  };

  virtual already_AddRefed<RenderTargetSet> CreateRenderTargetSet(layers::Compositor *aCompositor, const IntSize& aSize) = 0;
  virtual void DestroyRenderTargetSet(RenderTargetSet *aRTSet) = 0;
  virtual void SubmitFrame(RenderTargetSet *aRTSet) = 0;
protected:
  VRHMDRenderingSupport() { }
};

class VRHMDInfo {
public:
  enum Eye {
    Eye_Left,
    Eye_Right,
    NumEyes
  };

  enum StateValidFlags {
    State_Position = 1 << 1,
    State_Orientation = 1 << 2
  };

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRHMDInfo)

  VRHMDType GetType() const { return mType; }
  uint32_t GetDeviceIndex() const { return mDeviceIndex; }
  const nsCString& GetDeviceName() const { return mDeviceName; }

  virtual const VRFieldOfView& GetRecommendedEyeFOV(uint32_t whichEye) { return mRecommendedEyeFOV[whichEye]; }
  virtual const VRFieldOfView& GetMaximumEyeFOV(uint32_t whichEye) { return mMaximumEyeFOV[whichEye]; }

  const VRHMDConfiguration& GetConfiguration() const { return mConfiguration; }

  /* set the FOV for this HMD unit; this triggers a computation of all the remaining bits.  Returns false if it fails */
  virtual bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
                      double zNear, double zFar) = 0;
  const VRFieldOfView& GetEyeFOV(uint32_t whichEye)  { return mEyeFOV[whichEye]; }

  /* Suggested resolution for rendering a single eye.
   * Assumption is that left/right rendering will be 2x of this size.
   * XXX fix this for vertical displays
   */
  const IntSize& SuggestedEyeResolution() const { return mEyeResolution; }
  const Point3D& GetEyeTranslation(uint32_t whichEye) const { return mEyeTranslation[whichEye]; }
  const Matrix4x4& GetEyeProjectionMatrix(uint32_t whichEye) const { return mEyeProjectionMatrix[whichEye]; }

  virtual uint32_t GetSupportedSensorStateBits() { return mSupportedSensorBits; }
  virtual bool StartSensorTracking() = 0;
  virtual VRHMDSensorState GetSensorState(double timeOffset = 0.0) = 0;
  virtual void StopSensorTracking() = 0;

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

  virtual const VRDistortionMesh& GetDistortionMesh(uint32_t whichEye) const { return mDistortionMesh[whichEye]; }

  // The nsIScreen that represents this device
  virtual nsIScreen* GetScreen() { return mScreen; }

protected:
  explicit VRHMDInfo(VRHMDType aType);
  virtual ~VRHMDInfo() { MOZ_COUNT_DTOR(VRHMDInfo); }

  VRHMDType mType;
  VRHMDConfiguration mConfiguration;
  uint32_t mDeviceIndex;
  nsCString mDeviceName;

  VRFieldOfView mEyeFOV[NumEyes];
  IntSize mEyeResolution;
  Point3D mEyeTranslation[NumEyes];
  Matrix4x4 mEyeProjectionMatrix[NumEyes];
  VRDistortionMesh mDistortionMesh[NumEyes];
  uint32_t mSupportedSensorBits;

  VRFieldOfView mRecommendedEyeFOV[NumEyes];
  VRFieldOfView mMaximumEyeFOV[NumEyes];

  nsCOMPtr<nsIScreen> mScreen;
};

class VRHMDManager {
public:
  static void ManagerInit();
  static void ManagerDestroy();
  static void GetAllHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult);
  static uint32_t AllocateDeviceIndex();
  static already_AddRefed<nsIScreen> MakeFakeScreen(int32_t x, int32_t y, uint32_t width, uint32_t height);

protected:
  typedef nsTArray<nsRefPtr<VRHMDManager>> VRHMDManagerArray;
  static VRHMDManagerArray *sManagers;
  static Atomic<uint32_t> sDeviceBase;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRHMDManager)

  virtual bool PlatformInit() = 0;
  virtual bool Init() = 0;
  virtual void Destroy() = 0;
  virtual void GetHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult) = 0;

protected:
  VRHMDManager() { }
  virtual ~VRHMDManager() { }
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_H */
