/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_OPENVRSESSION_H
#define GFX_VR_SERVICE_OPENVRSESSION_H

#include "VRSession.h"

#include "openvr.h"
#include "mozilla/gfx/2D.h"
#include "moz_external_vr.h"

#if defined(XP_WIN)
#include <d3d11_1.h>
#elif defined(XP_MACOSX)
class MacIOSurface;
#endif

namespace mozilla {
namespace gfx {

class OpenVRSession : public VRSession
{
public:
  OpenVRSession();
  virtual ~OpenVRSession();

  bool Initialize(mozilla::gfx::VRSystemState& aSystemState) override;
  void Shutdown() override;
  void ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) override;
  void StartFrame(mozilla::gfx::VRSystemState& aSystemState) override;
  bool ShouldQuit() const override;
  bool StartPresentation() override;
  void StopPresentation() override;
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer) override;

private:
  // OpenVR State
  ::vr::IVRSystem* mVRSystem = nullptr;
  ::vr::IVRChaperone* mVRChaperone = nullptr;
  ::vr::IVRCompositor* mVRCompositor = nullptr;
  bool mShouldQuit;

  bool InitState(mozilla::gfx::VRSystemState& aSystemState);
  void UpdateStageParameters(mozilla::gfx::VRDisplayState& state);
  void UpdateEyeParameters(mozilla::gfx::VRDisplayState& state, gfx::Matrix4x4* headToEyeTransforms = nullptr);
  void GetSensorState(mozilla::gfx::VRSystemState& state);
  void GetControllerState(VRSystemState &state);

  bool SubmitFrame(void* aTextureHandle,
                   ::vr::ETextureType aTextureType,
                   const VRLayerEyeRect& aLeftEyeRect,
                   const VRLayerEyeRect& aRightEyeRect);
#if defined(XP_WIN)
  bool CreateD3DObjects();
#endif
};

} // namespace mozilla
} // namespace gfx

#endif // GFX_VR_SERVICE_OPENVRSESSION_H
