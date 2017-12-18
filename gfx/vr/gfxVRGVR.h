/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_GVR_H
#define GFX_VR_GVR_H

#include "gfxVR.h"

#include <memory>

#include "mozilla/EnumeratedArray.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"

#include "VRDisplayHost.h"

#pragma GCC system_header
#pragma GCC visibility push(default)
#include "vr/gvr/capi/include/gvr.h"
#include "vr/gvr/capi/include/gvr_controller.h"
#pragma GCC visibility pop


namespace mozilla {
namespace gl {
class GLContextEGL;
} // namespace gl
namespace layers {
class EGLImageDescriptor;
} // namespace layers
namespace gfx {
namespace impl {

class VRControllerGVR : public VRControllerHost
{
public:
  explicit VRControllerGVR(dom::GamepadHand aHand, uint32_t aDisplayID);
  virtual ~VRControllerGVR();
  void Update(gvr_controller_state* aState, VRSystemManager* aManager);
};

class VRDisplayGVR : public VRDisplayHost
{
public:
  VRDisplayGVR();

  // BEGIN VRDisplayHost interface
  void ZeroSensor() override;
  void StartPresentation() override;
  void StopPresentation() override;
  bool SubmitFrame(const mozilla::layers::EGLImageDescriptor* aDescriptor,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect) override;
protected:
  virtual VRHMDSensorState GetSensorState() override;
  // END VRDisplayHost interface

public:
  void SetPaused(const bool aPaused);
  void SetPresentingContext(void* aGVRPresentingContext);
  void EnableControllers(const bool aEnable, VRSystemManager* aManager);
  void UpdateControllers(VRSystemManager* aManager);
  void GetControllers(nsTArray<RefPtr<VRControllerHost> >& aControllerResult);

protected:
  virtual ~VRDisplayGVR();
  void UpdateHeadToEye(gvr_context* aContext);
  void UpdateViewport();
  void RecreateSwapChain();

  bool mIsPresenting;
  bool mControllerAdded;

  gfx::Matrix4x4 mHeadToEyes[2];
  gvr_context* mPresentingContext;
  gvr_controller_context* mControllerContext;
  gvr_controller_state* mControllerState;
  gvr_buffer_viewport_list* mViewportList;
  gvr_buffer_viewport* mLeftViewport;
  gvr_buffer_viewport* mRightViewport;
  gvr_mat4f mHeadMatrix;
  gvr_swap_chain* mSwapChain;
  gvr_sizei mFrameBufferSize;

  RefPtr<VRControllerGVR> mController;
};


} // namespace impl

class VRSystemManagerGVR : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerGVR> Create();

  void Destroy() override;
  void Shutdown() override;
  void Enumerate() override;
  bool ShouldInhibitEnumeration() override;
  void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) override;
  bool GetIsPresenting() override;
  void HandleInput() override;
  void GetControllers(nsTArray<RefPtr<VRControllerHost>>&
                      aControllerResult) override;
  void ScanForControllers() override;
  void RemoveControllers() override;
  void VibrateHaptic(uint32_t aControllerIdx,
                     uint32_t aHapticIndex,
                     double aIntensity,
                     double aDuration,
                     const VRManagerPromise& aPromise) override;
  void StopVibrateHaptic(uint32_t aControllerIdx) override;

protected:
  VRSystemManagerGVR();
  virtual ~VRSystemManagerGVR();

private:
  RefPtr<impl::VRDisplayGVR> mGVRHMD;
};

} // namespace gfx
} // namespace mozilla


#endif /* GFX_VR_GVR_H */
