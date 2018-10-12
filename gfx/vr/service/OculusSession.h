/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_OCULUSSESSION_H
#define GFX_VR_SERVICE_OCULUSSESSION_H

#include "VRSession.h"

#include "mozilla/gfx/2D.h"
#include "moz_external_vr.h"
#include "nsTArray.h"
#include "../ovr_capi_dynamic.h"
#include "prlink.h"
#include "ShaderDefinitionsD3D11.h" // for VertexShaderConstants and PixelShaderConstants

struct ID3D11Device;

namespace mozilla {
namespace layers {
struct VertexShaderConstants;
struct PixelShaderConstants;
}
namespace gfx {

class OculusSession : public VRSession
{
public:
  OculusSession();
  virtual ~OculusSession();

  bool Initialize(mozilla::gfx::VRSystemState& aSystemState) override;
  void Shutdown() override;
  void ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) override;
  void StartFrame(mozilla::gfx::VRSystemState& aSystemState) override;
  bool StartPresentation() override;
  void StopPresentation() override;
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                   ID3D11Texture2D* aTexture) override;
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                    float aIntensity, float aDuration) override;
  void StopVibrateHaptic(uint32_t aControllerIdx) override;
  void StopAllHaptics() override;

private:
  bool LoadOvrLib();
  void UnloadOvrLib();
  bool StartLib(ovrInitFlags aFlags);
  void StopLib();
  bool StartSession();
  void StopSession();
  bool StartRendering();
  void StopRendering();
  bool CreateD3DObjects();
  bool CreateShaders();
  void DestroyShaders();
  void CoverTransitions();
  void UpdateVisibility();
  bool ChangeVisibility(bool bVisible);
  bool InitState(mozilla::gfx::VRSystemState& aSystemState);
  void UpdateStageParameters(mozilla::gfx::VRDisplayState& aState);
  void UpdateEyeParameters(mozilla::gfx::VRSystemState& aState);
  void UpdateHeadsetPose(mozilla::gfx::VRSystemState& aState);
  void UpdateControllers(VRSystemState& aState);
  void UpdateControllerInputs(VRSystemState& aState,
                              const ovrInputState& aInputState);
  void UpdateHaptics();
  void EnumerateControllers(VRSystemState& aState,
                            const ovrInputState& aInputState);
  void UpdateControllerPose(VRSystemState& aState,
                            const ovrInputState& aInputState);
  void UpdateTelemetry(VRSystemState& aSystemState);
  bool IsPresentationReady() const;
  bool UpdateConstantBuffers();

  PRLibrary* mOvrLib;
  ovrSession mSession;
  ovrInitFlags mInitFlags;
  ovrTextureSwapChain mTextureSet;
  nsTArray<RefPtr<ID3D11RenderTargetView>> mRTView;
  nsTArray<RefPtr<ID3D11Texture2D>> mTexture;
  nsTArray<RefPtr<ID3D11ShaderResourceView>> mSRV;

  ID3D11VertexShader* mQuadVS;
  ID3D11PixelShader* mQuadPS;
  RefPtr<ID3D11SamplerState> mLinearSamplerState;
  layers::VertexShaderConstants mVSConstants;
  layers::PixelShaderConstants mPSConstants;
  RefPtr<ID3D11Buffer> mVSConstantBuffer;
  RefPtr<ID3D11Buffer> mPSConstantBuffer;
  RefPtr<ID3D11Buffer> mVertexBuffer;
  RefPtr<ID3D11InputLayout> mInputLayout;

  IntSize mPresentationSize;
  ovrFovPort mFOVPort[2];

  // Most recent HMD eye poses, from start of frame
  ovrPosef mFrameStartPose[2];

  float mRemainingVibrateTime[2];
  float mHapticPulseIntensity[2];
  TimeStamp mLastHapticUpdate;

  // The timestamp of the last ending presentation
  TimeStamp mLastPresentationEnd;
  bool mIsPresenting;
};

} // namespace mozilla
} // namespace gfx

#endif // GFX_VR_SERVICE_OCULUSSESSION_H
