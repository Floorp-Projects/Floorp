/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OCULUS_H
#define GFX_VR_OCULUS_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"
#include "VRDisplayHost.h"
#include "ovr_capi_dynamic.h"

struct ID3D11Device;

namespace mozilla {
namespace layers {
class CompositingRenderTargetD3D11;
struct VertexShaderConstants;
struct PixelShaderConstants;
}
namespace gfx {
namespace impl {

class VRDisplayOculus : public VRDisplayHost
{
public:
  virtual void NotifyVSync() override;
  virtual VRHMDSensorState GetSensorState() override;
  virtual VRHMDSensorState GetImmediateSensorState() override;
  void ZeroSensor() override;

protected:
  virtual void StartPresentation() override;
  virtual void StopPresentation() override;
  virtual void SubmitFrame(mozilla::layers::TextureSourceD3D11* aSource,
                           const IntSize& aSize,
                           const VRHMDSensorState& aSensorState,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;

public:
  explicit VRDisplayOculus(ovrSession aSession);

protected:
  virtual ~VRDisplayOculus();
  void Destroy();

  bool RequireSession();
  const ovrHmdDesc& GetHmdDesc();

  already_AddRefed<layers::CompositingRenderTargetD3D11> GetNextRenderTarget();

  VRHMDSensorState GetSensorState(double timeOffset);

  ovrHmdDesc mDesc;
  ovrSession mSession;
  ovrFovPort mFOVPort[2];
  ovrTextureSwapChain mTextureSet;
  nsTArray<RefPtr<layers::CompositingRenderTargetD3D11>> mRenderTargets;

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mContext;
  ID3D11VertexShader* mQuadVS;
  ID3D11PixelShader* mQuadPS;
  RefPtr<ID3D11SamplerState> mLinearSamplerState;
  layers::VertexShaderConstants mVSConstants;
  layers::PixelShaderConstants mPSConstants;
  RefPtr<ID3D11Buffer> mVSConstantBuffer;
  RefPtr<ID3D11Buffer> mPSConstantBuffer;
  RefPtr<ID3D11Buffer> mVertexBuffer;
  RefPtr<ID3D11InputLayout> mInputLayout;

  bool mIsPresenting;
  
  bool UpdateConstantBuffers();

  struct Vertex
  {
    float position[2];
  };
};

} // namespace impl

class VRDisplayManagerOculus : public VRDisplayManager
{
public:
  static already_AddRefed<VRDisplayManagerOculus> Create();
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost> >& aHMDResult) override;
protected:
  VRDisplayManagerOculus()
    : mOculusInitialized(false)
  { }

  RefPtr<impl::VRDisplayOculus> mHMDInfo;
  bool mOculusInitialized;
  RefPtr<nsIThread> mOculusThread;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_OCULUS_H */
