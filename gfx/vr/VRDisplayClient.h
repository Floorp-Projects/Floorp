/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_DISPLAY_CLIENT_H
#define GFX_VR_DISPLAY_CLIENT_H

#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/VRDisplayBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {
enum class XRReferenceSpaceType : uint8_t;
class XRSession;
}  // namespace dom
namespace gfx {
class VRDisplayPresentation;
class VRManagerChild;

class VRDisplayClient {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRDisplayClient)

  explicit VRDisplayClient(const VRDisplayInfo& aDisplayInfo);

  MOZ_CAN_RUN_SCRIPT void UpdateDisplayInfo(const VRDisplayInfo& aDisplayInfo);
  void UpdateSubmitFrameResult(const VRSubmitFrameResultInfo& aResult);

  const VRDisplayInfo& GetDisplayInfo() const { return mDisplayInfo; }
  virtual const VRHMDSensorState& GetSensorState() const;
  void GetSubmitFrameResult(VRSubmitFrameResultInfo& aResult);

  already_AddRefed<VRDisplayPresentation> BeginPresentation(
      const nsTArray<dom::VRLayer>& aLayers, uint32_t aGroup);
  void PresentationCreated();
  void PresentationDestroyed();

  void SessionStarted(dom::XRSession* aSession);
  void SessionEnded(dom::XRSession* aSession);

  bool GetIsConnected() const;

  void NotifyDisconnected();
  void SetGroupMask(uint32_t aGroupMask);

  bool IsPresentationGenerationCurrent() const;
  void MakePresentationGenerationCurrent();

  void StartVRNavigation();
  void StopVRNavigation(const TimeDuration& aTimeout);

  bool IsPresenting();
  bool IsReferenceSpaceTypeSupported(dom::XRReferenceSpaceType aType) const;

 protected:
  virtual ~VRDisplayClient();

  MOZ_CAN_RUN_SCRIPT void FireEvents();
  void FireGamepadEvents();
  MOZ_CAN_RUN_SCRIPT void StartFrame();

  VRDisplayInfo mDisplayInfo;

  bool bLastEventWasMounted;
  bool bLastEventWasPresenting;

  int mPresentationCount;
  uint64_t mLastEventFrameId;
  uint32_t mLastPresentingGeneration;

  // Difference between mDisplayInfo.mControllerState and
  // mLastEventControllerState determines what gamepad events to fire when
  // updated.
  VRControllerState mLastEventControllerState[kVRControllerMaxCount];

  /**
   * mSessions is cleared in VRDisplayClient::SessionEnded.
   * SessionEnded is guaranteed to be called by every XRSession
   * when it is shutdown explicitly with the WebXR XRSession.end
   * call, when all JS references on the XRSession are released, or
   * when the window is closed.
   */
  nsTArray<RefPtr<dom::XRSession>> mSessions;

 private:
  void GamepadMappingForWebVR(VRControllerState& aControllerState);

  VRSubmitFrameResultInfo mSubmitFrameResult;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_DISPLAY_CLIENT_H */
