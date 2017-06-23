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
#include "mozilla/dom/GamepadPoseState.h"

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

  virtual void ZeroSensor() = 0;
  virtual void StartPresentation() = 0;
  virtual void StopPresentation() = 0;
  virtual void NotifyVSync();

  void StartFrame();
  void SubmitFrame(VRLayerParent* aLayer,
                   mozilla::layers::PTextureParent* aTexture,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);

  bool CheckClearDisplayInfoDirty();
  void SetGroupMask(uint32_t aGroupMask);
  bool GetIsConnected();

protected:
  explicit VRDisplayHost(VRDeviceType aType);
  virtual ~VRDisplayHost();

#if defined(XP_WIN)
  // Subclasses should override this SubmitFrame function.
  // Returns true if the SubmitFrame call will block as necessary
  // to control timing of the next frame and throttle the render loop
  // for the needed framerate.
  virtual bool SubmitFrame(mozilla::layers::TextureSourceD3D11* aSource,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#endif

  VRDisplayInfo mDisplayInfo;

  nsTArray<RefPtr<VRLayerParent>> mLayers;
  // Weak reference to mLayers entries are cleared in
  // VRLayerParent destructor

protected:
  virtual VRHMDSensorState GetSensorState() = 0;

private:
  VRDisplayInfo mLastUpdateDisplayInfo;
  TimeStamp mLastFrameStart;
};

class VRControllerHost {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRControllerHost)

  const VRControllerInfo& GetControllerInfo() const;
  void SetButtonPressed(uint64_t aBit);
  uint64_t GetButtonPressed();
  void SetButtonTouched(uint64_t aBit);
  uint64_t GetButtonTouched();
  void SetPose(const dom::GamepadPoseState& aPose);
  const dom::GamepadPoseState& GetPose();
  dom::GamepadHand GetHand();
  void SetVibrateIndex(uint64_t aIndex);
  uint64_t GetVibrateIndex();

protected:
  explicit VRControllerHost(VRDeviceType aType);
  virtual ~VRControllerHost();

  VRControllerInfo mControllerInfo;
  // The current button pressed bit of button mask.
  uint64_t mButtonPressed;
  // The current button touched bit of button mask.
  uint64_t mButtonTouched;
  uint64_t mVibrateIndex;
  dom::GamepadPoseState mPose;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_DISPLAY_HOST_H */
