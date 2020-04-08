/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_VRSESSION_H
#define GFX_VR_SERVICE_VRSESSION_H

#include "moz_external_vr.h"

#if defined(XP_WIN)
#  include <d3d11_1.h>
#elif defined(XP_MACOSX)
class MacIOSurface;
#endif

namespace mozilla {
namespace gfx {

class VRSession {
 public:
  VRSession();

  // Since this class doesn't use smartpointers for its refcounted
  // members (so that it can compile outside of mozilla-central),
  // prevent copying the addresses without increasing the refcount.
  VRSession(const VRSession&) = delete;
  VRSession& operator=(const VRSession&) = delete;

#ifdef XP_WIN
  virtual ~VRSession();
#else
  virtual ~VRSession() = default;
#endif

  static void UpdateTrigger(VRControllerState& aState, uint32_t aButtonIndex,
                            float aValue, float aThreshold);
  /**
   * In order to support WebXR's navigator.xr.IsSessionSupported call without
   * displaying any permission dialogue, it is necessary to have a safe way to
   * detect the capability of running a VR or AR session without activating XR
   * runtimes or powering on hardware.
   *
   * API's such as OpenVR make no guarantee that hardware and software won't be
   * left activated after enumerating devices, so each backend in gfx/vr/service
   * must allow for more granular detection of capabilities.
   *
   * By passing true to bDetectRuntimesOnly, the initialization exits early
   * after reporting the presence of XR runtime software. The Initialize method
   * will only enumerate hardware and possibly return true when
   * aDetectRuntimesOnly is false.
   */
  virtual bool Initialize(mozilla::gfx::VRSystemState& aSystemState,
                          bool aDetectRuntimesOnly) = 0;
  virtual void Shutdown() = 0;
  virtual void ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) = 0;
  virtual void StartFrame(mozilla::gfx::VRSystemState& aSystemState) = 0;
  virtual bool StartPresentation() = 0;
  virtual void StopPresentation() = 0;
  virtual void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                             float aIntensity, float aDuration) = 0;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) = 0;
  virtual void StopAllHaptics() = 0;
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer);
  bool ShouldQuit() const;

 protected:
  bool mShouldQuit;
#if defined(XP_WIN)
  virtual bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                           ID3D11Texture2D* aTexture) = 0;
  bool CreateD3DContext(ID3D11Device* aDevice);

  ID3D11Device1* GetD3DDevice();
  ID3D11DeviceContext1* GetD3DDeviceContext();
  ID3DDeviceContextState* GetD3DDeviceContextState();

  ID3D11Device1* mDevice;
  ID3D11DeviceContext1* mContext;
  ID3DDeviceContextState* mDeviceContextState;

#elif defined(XP_MACOSX)
  virtual bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                           const VRLayerTextureHandle& aTexture) = 0;
#endif
  void SetControllerSelectionAndSqueezeFrameId(VRControllerState& controllerState, uint64_t aFrameId);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_VRSESSION_H
