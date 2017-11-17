/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor

#if defined(XP_WIN)
#include <d3d11_1.h>
#elif defined(XP_MACOSX)
class MacIOSurface;
#endif
namespace mozilla {
namespace gfx {
class VRThread;
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
                   const layers::SurfaceDescriptor& aTexture,
                   uint64_t aFrameId,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);

  bool CheckClearDisplayInfoDirty();
  void SetGroupMask(uint32_t aGroupMask);
  bool GetIsConnected();

  class AutoRestoreRenderState {
  public:
    explicit AutoRestoreRenderState(VRDisplayHost* aDisplay);
    ~AutoRestoreRenderState();
    bool IsSuccess();
  private:
    RefPtr<VRDisplayHost> mDisplay;
#if defined(XP_WIN)
    RefPtr<ID3DDeviceContextState> mPrevDeviceContextState;
#endif
    bool mSuccess;
  };

protected:
  explicit VRDisplayHost(VRDeviceType aType);
  virtual ~VRDisplayHost();

#if defined(XP_WIN)
  // Subclasses should override this SubmitFrame function.
  // Returns true if the SubmitFrame call will block as necessary
  // to control timing of the next frame and throttle the render loop
  // for the needed framerate.
  virtual bool SubmitFrame(ID3D11Texture2D* aSource,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#elif defined(XP_MACOSX)
  virtual bool SubmitFrame(MacIOSurface* aMacIOSurface,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#elif defined(MOZ_ANDROID_GOOGLE_VR)
  virtual bool SubmitFrame(const mozilla::layers::EGLImageDescriptor* aDescriptor,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#endif

  VRDisplayInfo mDisplayInfo;

  nsTArray<VRLayerParent *> mLayers;
  // Weak reference to mLayers entries are cleared in
  // VRLayerParent destructor

protected:
  virtual VRHMDSensorState GetSensorState() = 0;

  RefPtr<VRThread> mSubmitThread;
private:
  void SubmitFrameInternal(const layers::SurfaceDescriptor& aTexture,
                           uint64_t aFrameId,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect);

  VRDisplayInfo mLastUpdateDisplayInfo;
  TimeStamp mLastFrameStart;
  bool mFrameStarted;

#if defined(XP_WIN)
protected:
  bool CreateD3DObjects();
  RefPtr<ID3D11Device1> mDevice;
  RefPtr<ID3D11DeviceContext1> mContext;
  ID3D11Device1* GetD3DDevice();
  ID3D11DeviceContext1* GetD3DDeviceContext();
  ID3DDeviceContextState* GetD3DDeviceContextState();

private:
  RefPtr<ID3DDeviceContextState> mDeviceContextState;
#endif
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
  explicit VRControllerHost(VRDeviceType aType, dom::GamepadHand aHand,
                            uint32_t aDisplayID);
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
