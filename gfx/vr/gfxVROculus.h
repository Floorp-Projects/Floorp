/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OCULUS_H
#define GFX_VR_OCULUS_H

#include "nsTArray.h"
#include "nsISupportsImpl.h" // For NS_INLINE_DECL_REFCOUNTING
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

enum class OculusControllerAxisType : uint16_t {
  ThumbstickXAxis,
  ThumbstickYAxis,
  NumVRControllerAxisType
};

class VROculusSession
{
  NS_INLINE_DECL_REFCOUNTING(VROculusSession);
public:
  VROculusSession();
  void Refresh();
  bool IsTrackingReady() const;
  bool IsRenderReady() const;
  ovrSession Get();
  void StartPresentation(const IntSize& aSize);
  void StopPresentation();
  void StopTracking();
  bool IsQuitTimeoutActive();
  already_AddRefed<layers::CompositingRenderTargetD3D11> GetNextRenderTarget();
  ovrTextureSwapChain GetSwapChain();

private:
  PRLibrary* mOvrLib;
  ovrSession mSession;
  ovrInitFlags mInitFlags;
  ovrTextureSwapChain mTextureSet;
  nsTArray<RefPtr<layers::CompositingRenderTargetD3D11>> mRenderTargets;
  bool mPresenting;
  IntSize mPresentationSize;
  RefPtr<ID3D11Device> mDevice;
  // The timestamp of the last time Oculus set ShouldQuit to true.
  TimeStamp mLastShouldQuit;
  // The timestamp of the last ending presentation
  TimeStamp mLastPresentationEnd;

  ~VROculusSession();
  void Uninitialize(bool aUnloadLib);
  bool Initialize(ovrInitFlags aFlags);
  bool LoadOvrLib();
  void UnloadOvrLib();
  bool StartSession();
  void StopSession();
  bool StartLib(ovrInitFlags aFlags);
  void StopLib();
  bool StartRendering();
  void StopRendering();
};

class VRDisplayOculus : public VRDisplayHost
{
public:
  virtual void NotifyVSync() override;
  void ZeroSensor() override;

protected:
  virtual VRHMDSensorState GetSensorState() override;
  virtual void StartPresentation() override;
  virtual void StopPresentation() override;
  virtual bool SubmitFrame(mozilla::layers::TextureSourceD3D11* aSource,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;
  void UpdateStageParameters();

public:
  explicit VRDisplayOculus(VROculusSession* aSession);
  void Destroy();

protected:
  virtual ~VRDisplayOculus();

  VRHMDSensorState GetSensorState(double absTime);

  ovrHmdDesc mDesc;
  RefPtr<VROculusSession> mSession;
  ovrFovPort mFOVPort[2];

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

  float mEyeHeight;

  bool UpdateConstantBuffers();

  struct Vertex
  {
    float position[2];
  };
};

class VRControllerOculus : public VRControllerHost
{
public:
  explicit VRControllerOculus(dom::GamepadHand aHand, uint32_t aDisplayID);
  float GetAxisMove(uint32_t aAxis);
  void SetAxisMove(uint32_t aAxis, float aValue);
  float GetIndexTrigger();
  void SetIndexTrigger(float aValue);
  float GetHandTrigger();
  void SetHandTrigger(float aValue);
  void VibrateHaptic(ovrSession aSession,
                     uint32_t aHapticIndex,
                     double aIntensity,
                     double aDuration,
                     uint32_t aPromiseID);
  void StopVibrateHaptic();

protected:
  virtual ~VRControllerOculus();

private:
  void UpdateVibrateHaptic(ovrSession aSession,
                           uint32_t aHapticIndex,
                           double aIntensity,
                           double aDuration,
                           uint64_t aVibrateIndex,
                           uint32_t aPromiseID);
  void VibrateHapticComplete(ovrSession aSession, uint32_t aPromiseID, bool aStop);

  float mAxisMove[static_cast<uint32_t>(
                  OculusControllerAxisType::NumVRControllerAxisType)];
  float mIndexTrigger;
  float mHandTrigger;
  nsCOMPtr<nsIThread> mVibrateThread;
  Atomic<bool> mIsVibrateStopped;
};

} // namespace impl

class VRSystemManagerOculus : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerOculus> Create();
  virtual void Destroy() override;
  virtual void Shutdown() override;
  virtual bool GetHMDs(nsTArray<RefPtr<VRDisplayHost> >& aHMDResult) override;
  virtual bool GetIsPresenting() override;
  virtual void HandleInput() override;
  virtual void GetControllers(nsTArray<RefPtr<VRControllerHost>>&
                              aControllerResult) override;
  virtual void ScanForControllers() override;
  virtual void RemoveControllers() override;
  virtual void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                             double aIntensity, double aDuration, uint32_t aPromiseID) override;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) override;

protected:
  VRSystemManagerOculus();

private:
  void HandleButtonPress(uint32_t aControllerIdx,
                         uint32_t aButton,
                         uint64_t aButtonMask,
                         uint64_t aButtonPressed,
                         uint64_t aButtonTouched);
  void HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                      float aValue);
  void HandlePoseTracking(uint32_t aControllerIdx,
                          const dom::GamepadPoseState& aPose,
                          VRControllerHost* aController);
  void HandleIndexTriggerPress(uint32_t aControllerIdx, uint32_t aButton, float aValue);
  void HandleHandTriggerPress(uint32_t aControllerIdx, uint32_t aButton, float aValue);
  void HandleTouchEvent(uint32_t aControllerIdx, uint32_t aButton,
                        uint64_t aTouchMask, uint64_t aTouched);
  RefPtr<impl::VRDisplayOculus> mDisplay;
  nsTArray<RefPtr<impl::VRControllerOculus>> mOculusController;
  RefPtr<impl::VROculusSession> mSession;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_OCULUS_H */
