/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_VRSESSION_H
#define GFX_VR_SERVICE_VRSESSION_H

#include "VRSession.h"

#include "moz_external_vr.h"

#if defined(XP_WIN)
#include <d3d11_1.h>
#elif defined(XP_MACOSX)
class MacIOSurface;
#endif

namespace mozilla {
namespace gfx {

class VRSession
{
public:
  VRSession();
  virtual ~VRSession();

  virtual bool Initialize(mozilla::gfx::VRSystemState& aSystemState) = 0;
  virtual void Shutdown() = 0;
  virtual void ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) = 0;
  virtual void StartFrame(mozilla::gfx::VRSystemState& aSystemState) = 0;
  virtual bool ShouldQuit() const = 0;
  virtual bool StartPresentation() = 0;
  virtual void StopPresentation() = 0;
  virtual bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer) = 0;

#if defined(XP_WIN)
protected:
  bool CreateD3DContext(RefPtr<ID3D11Device> aDevice);
  RefPtr<ID3D11Device1> mDevice;
  RefPtr<ID3D11DeviceContext1> mContext;
  ID3D11Device1* GetD3DDevice();
  ID3D11DeviceContext1* GetD3DDeviceContext();
  ID3DDeviceContextState* GetD3DDeviceContextState();
  RefPtr<ID3DDeviceContextState> mDeviceContextState;
#endif
};

} // namespace mozilla
} // namespace gfx

#endif // GFX_VR_SERVICE_VRSESSION_H
