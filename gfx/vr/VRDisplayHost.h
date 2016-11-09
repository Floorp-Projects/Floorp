/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_DISPLAY_HOST_H
#define GFX_VR_DISPLAY_HOST_H

#include "gfxVR.h"
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
#if defined(XP_WIN)
class TextureSourceD3D11;
#endif
} // namespace layers
namespace gfx {
class VRLayerParent;

class VRDisplayHost {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRDisplayHost)

  const VRDisplayInfo& GetDisplayInfo() const { return mDisplayInfo; }

  void AddLayer(VRLayerParent* aLayer);
  void RemoveLayer(VRLayerParent* aLayer);

  virtual VRHMDSensorState GetSensorState() = 0;
  virtual VRHMDSensorState GetImmediateSensorState() = 0;
  virtual void ZeroSensor() = 0;
  virtual void StartPresentation() = 0;
  virtual void StopPresentation() = 0;
  virtual void NotifyVSync() { };

  void SubmitFrame(VRLayerParent* aLayer,
                   const int32_t& aInputFrameID,
                   mozilla::layers::PTextureParent* aTexture,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);

  bool CheckClearDisplayInfoDirty();

protected:
  explicit VRDisplayHost(VRDeviceType aType);
  virtual ~VRDisplayHost();

#if defined(XP_WIN)
  virtual void SubmitFrame(mozilla::layers::TextureSourceD3D11* aSource,
                           const IntSize& aSize,
                           const VRHMDSensorState& aSensorState,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#endif

  VRDisplayInfo mDisplayInfo;

  nsTArray<RefPtr<VRLayerParent>> mLayers;
  // Weak reference to mLayers entries are cleared in VRLayerParent destructor

  // The maximum number of frames of latency that we would expect before we
  // should give up applying pose prediction.
  // If latency is greater than one second, then the experience is not likely
  // to be corrected by pose prediction.  Setting this value too
  // high may result in unnecessary memory allocation.
  // As the current fastest refresh rate is 90hz, 100 is selected as a
  // conservative value.
  static const int kMaxLatencyFrames = 100;
  VRHMDSensorState mLastSensorState[kMaxLatencyFrames];
  int32_t mInputFrameID;

private:
  VRDisplayInfo mLastUpdateDisplayInfo;
};

class VRControllerHost {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRControllerHost)

  const VRControllerInfo& GetControllerInfo() const;
  void SetIndex(uint32_t aIndex);
  uint32_t GetIndex();
  void SetButtonPressed(uint64_t aBit);
  uint64_t GetButtonPressed();

protected:
  explicit VRControllerHost(VRDeviceType aType);
  virtual ~VRControllerHost();

  VRControllerInfo mControllerInfo;
  // The controller index in VRControllerManager.
  uint32_t mIndex;
  // The current button pressed bit of button mask.
  uint64_t mButtonPressed;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_DISPLAY_HOST_H */
