/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_OSVRSESSION_H
#define GFX_VR_SERVICE_OSVRSESSION_H

#include "VRSession.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/TimeStamp.h"
#include "moz_external_vr.h"

#include <osvr/ClientKit/ClientKitC.h>
#include <osvr/ClientKit/DisplayC.h>

#if defined(XP_WIN)
#include <d3d11_1.h>
#elif defined(XP_MACOSX)
class MacIOSurface;
#endif

namespace mozilla {
namespace gfx {

class OSVRSession : public VRSession
{
public:
  OSVRSession();
  virtual ~OSVRSession();

  bool Initialize(mozilla::gfx::VRSystemState& aSystemState) override;
  void Shutdown() override;
  void ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) override;
  void StartFrame(mozilla::gfx::VRSystemState& aSystemState) override;
  bool StartPresentation() override;
  void StopPresentation() override;
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                    float aIntensity, float aDuration) override;
  void StopVibrateHaptic(uint32_t aControllerIdx) override;
  void StopAllHaptics() override;

protected:
#if defined(XP_WIN)
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                   ID3D11Texture2D* aTexture) override;
#elif defined(XP_MACOSX)
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                   MacIOSurface* aTexture) override;
#endif

private:
  bool InitState(mozilla::gfx::VRSystemState& aSystemState);
  void UpdateHeadsetPose(mozilla::gfx::VRSystemState& aState);
  bool mRuntimeLoaded;
  bool mOSVRInitialized;
  bool mClientContextInitialized;
  bool mDisplayConfigInitialized;
  bool mInterfaceInitialized;
  OSVR_ClientContext m_ctx;
  OSVR_ClientInterface m_iface;
  OSVR_DisplayConfig m_display;
  gfx::Matrix4x4 mHeadToEye[2];

  // check if all components are initialized
  // and if not, it will try to initialize them
  void CheckOSVRStatus();
  void InitializeClientContext();
  void InitializeDisplay();
  void InitializeInterface();
};

} // namespace mozilla
} // namespace gfx

#endif // GFX_VR_SERVICE_OSVRSESSION_H
